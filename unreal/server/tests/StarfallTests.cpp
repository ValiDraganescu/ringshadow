// Starfall (Starfall.h, WorldStarfall.cpp; docs/weather.md "Starfall"): the
// schedule by a fixed seed, the place rules, the rich deposit that pays half
// again in XP and is gone when emptied, the dev fall near a pilot, and the
// save round trip. Isolated scenes of a few seconds; run from unreal/server.

#include "test.h"

#include "NavGrid.h"
#include "Rules.h"
#include "Starfall.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <map>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 43;
constexpr uint64_t kSeed = 0x5ea5011ULL;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfallSeed = kSeed;
    return p;
}

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// Steps until a fall is told (at most `seconds`).
void stepFor(WorldMap& w, double seconds) {
    const int n = int(seconds * 60);
    for (int i = 0; i < n; i++) w.step();
}

/// The pilot drills deposit `index` of its region, holding the key: the load
/// it comes away with (0: none in 30 s).
int64_t drill(WorldMap& w, size_t index) {
    Simulation& sim = w.regionOf(me)->simulation();
    const OreDeposit& d = sim.state.patches[index];
    Unit* u = drivenUnit(w, me);
    const Vec2 away = Vec2(1, 0);
    u->position = d.position + away * (Rules::radius(UnitKind::prospector) + 1.0);
    u->heading = std::atan2(-away.y, -away.x);
    u->carrying = 0;
    u->hydrogen = std::nullopt;
    static int64_t tick = 0;
    for (int i = 0; i < 30 * 60; i++) {
        PilotInput in;
        in.tick = ++tick;
        in.visit = w.visit(me);
        in.heading = drivenUnit(w, me)->heading;
        in.hold = true;
        w.input(me, in);
        w.step();
        if (drivenUnit(w, me)->carrying > 0) break;
    }
    return drivenUnit(w, me)->carrying;
}

std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

TEST(starfall_schedule_two_or_three_a_day_by_the_seed) {
    const double day = WorldClock::dayLength;
    int64_t total = 0;
    for (int64_t d = 0; d < 30; d++) {
        const auto t = Starfall::impacts(kSeed, d, day);
        EXPECT_TRUE(int64_t(t.size()) >= Starfall::fewest && int64_t(t.size()) <= Starfall::most);
        total += int64_t(t.size());
        for (size_t k = 0; k < t.size(); k++) {
            // In its own share of the day, in its middle 70%.
            const double share = day / double(t.size());
            const double into = t[k] - double(d) * day - share * double(k);
            EXPECT_TRUE(into >= 0.15 * share - 1e-9 && into <= 0.85 * share + 1e-9);
        }
        // The same seed, the same times; another seed, others.
        EXPECT_TRUE(Starfall::impacts(kSeed, d, day) == t);
    }
    EXPECT_TRUE(total > 30 * 2 && total < 30 * 3);
    EXPECT_TRUE(Starfall::impacts(kSeed, 3, day) != Starfall::impacts(kSeed + 1, 3, day));
    // The next impact after a time: the first of the day, then on.
    const auto d0 = Starfall::impacts(kSeed, 0, day);
    EXPECT_NEAR(Starfall::next(kSeed, 0, day), d0[0], 1e-9);
    EXPECT_NEAR(Starfall::next(kSeed, d0[0], day), d0[1], 1e-9);
    EXPECT_NEAR(Starfall::next(kSeed, d0.back(), day), Starfall::impacts(kSeed, 1, day)[0], 1e-9);
    // From the giant's side of the sky.
    for (uint64_t i = 0; i < 50; i++) {
        const double b = Starfall::bearingFrom(i);
        EXPECT_TRUE(b >= Starfall::giantBearing - Starfall::spread && b <= Starfall::giantBearing + Starfall::spread);
    }
    // The streak: high and far before, at the point at the impact.
    const Vec3 h = Starfall::head(Starfall::streak, 270);
    EXPECT_TRUE(h.x < -500 && std::abs(h.z) < 1e-6 && h.y > 150);
    const Vec3 z = Starfall::head(0, 270);
    EXPECT_TRUE(std::abs(z.x) + std::abs(z.y) + std::abs(z.z) < 1e-9);
}

TEST(starfall_told_a_minute_before_lands_a_rich_deposit) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    const double impact = Starfall::next(kSeed, WorldClock::timeOf(9.0), WorldClock::dayLength);
    // Just short of the minute's warning: none told yet.
    w.setClock(impact - Starfall::warning - 2);
    stepFor(w, 1);
    EXPECT_TRUE(w.starfalls().empty());
    stepFor(w, 2);
    ASSERT_TRUE(w.starfalls().size() == 1);
    const WorldMap::Fall f = w.starfalls()[0];
    EXPECT_NEAR(f.impact, impact, 1e-9);
    EXPECT_TRUE(!f.patch.has_value());
    // Every pilot is told: the sky carries it.
    ASSERT_TRUE(w.sky().has_value());
    ASSERT_TRUE(w.sky()->starfalls.size() == 1);
    EXPECT_EQ(w.sky()->starfalls[0].id, f.id);
    EXPECT_EQ(w.sky()->starfalls[0].region, int64_t(f.region));
    EXPECT_NEAR(w.sky()->starfalls[0].impact, impact, 1e-3);
    // Told once.
    stepFor(w, 5);
    EXPECT_EQ(w.starfalls().size(), size_t(1));
    // The impact: a deposit after the region's own, three times a normal one.
    const size_t own = w.region(f.region).simulation().state.patches.size();
    w.setClock(impact - 0.5);
    stepFor(w, 1);
    ASSERT_TRUE(w.starfalls()[0].patch.has_value());
    const GameState& st = w.region(f.region).simulation().state;
    ASSERT_TRUE(st.patches.size() == own + 1);
    EXPECT_EQ(*w.starfalls()[0].patch, int64_t(own));
    const OreDeposit& d = st.patches[own];
    EXPECT_NEAR(d.position.x, f.at.x, 1e-9);
    EXPECT_NEAR(d.position.y, f.at.y, 1e-9);
    std::vector<int64_t> amounts;
    for (size_t i = 0; i < own; i++) amounts.push_back(st.patches[i].initial);
    std::sort(amounts.begin(), amounts.end());
    const int64_t normal = amounts[amounts.size() / 2];
    EXPECT_EQ(d.initial, std::max(Starfall::minOre, int64_t(std::llround(3.0 * double(normal)))));
    EXPECT_EQ(d.remaining, d.initial);
    std::set<int64_t> ids;
    for (const OreDeposit& p : st.patches) ids.insert(p.id);
    EXPECT_EQ(ids.size(), st.patches.size());
    ASSERT_TRUE(w.sky()->starfalls[0].patch.has_value());
}

TEST(starfall_lands_on_open_ground_only) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    const size_t s = *w.regionNamed("start");
    const Region& r = w.region(s);
    const Simulation& sim = r.simulation();
    // Not on a pad, a site, ore, a well, the map's edge or a cliff.
    for (const region::Place& p : r.places()->pads) EXPECT_TRUE(!w.craterFits(s, p.at));
    for (const region::Site& p : r.places()->sites) EXPECT_TRUE(!w.craterFits(s, p.at));
    for (const OreDeposit& d : sim.state.patches) EXPECT_TRUE(!w.craterFits(s, d.position));
    for (const Well& x : *sim.state.wells) EXPECT_TRUE(!w.craterFits(s, x.position));
    const GroundRect b = sim.map->bounds;
    EXPECT_TRUE(!w.craterFits(s, Vec2(b.minX + 3, (b.minZ + b.maxZ) / 2)));
    int cliffs = 0;
    for (double x = b.minX + 14; x < b.maxX - 14; x += 1)
        for (double z = b.minZ + 14; z < b.maxZ - 14; z += 1)
            if (sim.nav->cliff(Vec2(x, z))) {
                cliffs += 1;
                EXPECT_TRUE(!w.craterFits(s, Vec2(x, z)));
            }
    EXPECT_TRUE(cliffs > 0);
    // Weighted toward the pilot's region and its neighbours; every one placed
    // by the rules, clear of the region's pads, sites and ore.
    std::map<size_t, int> count;
    int placed = 0;
    for (int i = 0; i < 60; i++) {
        const auto f = w.forceStarfall(30);
        if (!f) continue;
        placed += 1;
        count[f->region] += 1;
        const Region& g = w.region(f->region);
        for (const region::Place& p : g.places()->pads)
            EXPECT_TRUE(length(p.at - f->at) >= Starfall::craterRadius + region::padReach + 3 - 1e-9);
        for (const region::Site& p : g.places()->sites) EXPECT_TRUE(length(p.at - f->at) >= Starfall::craterRadius + 4 - 1e-9);
        for (const OreDeposit& d : g.simulation().state.patches) EXPECT_TRUE(length(d.position - f->at) >= Starfall::craterRadius + 2.5 - 1e-9);
        EXPECT_TRUE(g.simulation().nav->standable(f->at));
        EXPECT_TRUE(!g.simulation().nav->cliff(f->at));
    }
    EXPECT_TRUE(placed >= 55);
    int most = 0;
    for (const auto& [region, n] : count)
        if (region != s) most = std::max(most, n);
    EXPECT_TRUE(count[s] > most);
    EXPECT_TRUE(count[s] * 5 >= placed);
}

TEST(starfall_deposit_pays_half_again_in_xp_and_is_gone_when_emptied) {
    WorldParams p = params();
    p.starfalls = false;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    w.setClock(WorldClock::timeOf(9.0));
    // A dev fall near the pilot, landing at once.
    const auto f = w.forceStarfall(0, std::nullopt, me);
    ASSERT_TRUE(f.has_value());
    const Unit* u = drivenUnit(w, me);
    const double from = length(f->at - u->position);
    EXPECT_TRUE(from >= 40 - 1e-9 && from <= 58 + 1e-9);
    w.step();
    ASSERT_TRUE(w.starfalls()[0].patch.has_value());
    const size_t index = size_t(*w.starfalls()[0].patch);
    GameState& st = w.regionOf(me)->simulation().state;
    // A plain deposit's load and its XP at the drill: none (it comes at the unload).
    const double xp0 = w.progress(me)->xp;
    const int64_t plain = drill(w, 0);
    ASSERT_TRUE(plain > 0);
    const double xp1 = w.progress(me)->xp;
    EXPECT_NEAR(xp1 - xp0, 0, 1e-9);
    // The starfall's: half a load's XP more at the drill.
    const int64_t before = st.patches[index].remaining;
    const int64_t load = drill(w, index);
    ASSERT_TRUE(load > 0);
    const int64_t drilled = before - st.patches[index].remaining;
    EXPECT_TRUE(drilled > 0);
    EXPECT_NEAR(w.progress(me)->xp - xp1, (Starfall::xp - 1) * double(drilled) * WorldXp::ore, 1e-9);
    // First come, first served: emptied, it never grows back.
    st.patches[index].remaining = 1;
    w.step();
    ASSERT_TRUE(drill(w, index) > 0);
    w.step();
    EXPECT_EQ(st.patches[index].remaining, int64_t(0));
    EXPECT_TRUE(std::isinf(*st.patches[index].depletedAt));
    st.time += Rules::regrowDelay * 4;
    stepFor(w, 1);
    EXPECT_EQ(st.patches[index].remaining, int64_t(0));
    // The crater fades after a day; then it is gone from the sky.
    EXPECT_EQ(w.sky()->starfalls.size(), size_t(1));
    w.setClock(f->impact + Starfall::craterLife + 1);
    w.step();
    EXPECT_TRUE(w.starfalls().empty());
    EXPECT_TRUE(w.sky()->starfalls.empty());
}

TEST(starfall_a_full_deposit_outlives_its_crater) {
    WorldParams p = params();
    p.starfalls = false;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    const auto f = w.forceStarfall(0, w.regionNamed("cold-fork"));
    ASSERT_TRUE(f.has_value());
    w.step();
    w.setClock(f->impact + Starfall::craterLife + 10);
    w.step();
    EXPECT_EQ(w.starfalls().size(), size_t(1));
}

TEST(starfall_dev_flag_drops_one_near_the_first_pilot) {
    WorldParams p = params();
    p.starfalls = false;
    p.devStarfall = 20;
    WorldMap w(first(), p);
    w.step();
    EXPECT_TRUE(w.starfalls().empty());
    ASSERT_TRUE(w.join(me));
    w.step();
    ASSERT_TRUE(w.starfalls().size() == 1);
    const WorldMap::Fall f = w.starfalls()[0];
    EXPECT_EQ(f.region, *w.regionNamed("start"));
    EXPECT_NEAR(f.impact - w.clock(), 20, 1e-6);
    const double from = length(f.at - drivenUnit(w, me)->position);
    EXPECT_TRUE(from >= 39 && from <= 59);
    // Once.
    stepFor(w, 1);
    EXPECT_EQ(w.starfalls().size(), size_t(1));
}

TEST(starfall_save_round_trip) {
    const std::string path = fresh("starfall");
    std::string why;
    WorldParams p = params();
    p.starfalls = false;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    // One landed and drilled a little, one still coming.
    const auto landed = w.forceStarfall(0, w.regionNamed("tern-ridge"));
    const auto coming = w.forceStarfall(45, w.regionNamed("brine-pans"));
    ASSERT_TRUE(landed && coming);
    w.step();
    const WorldMap::Fall l = w.starfalls()[0];
    ASSERT_TRUE(l.patch.has_value());
    OreDeposit& d = w.region(l.region).simulation().state.patches[size_t(*l.patch)];
    d.remaining -= 125;
    const int64_t id = d.id;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        // Unchanged: nothing written again.
        const auto n = store->write(w.save(), &why);
        ASSERT_TRUE(n.has_value());
        EXPECT_EQ(n->changed, int64_t(0));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    ASSERT_TRUE(back->starfalls.size() == 2);
    // The region file's deposits only in `deposits`.
    for (const SavedRegion& r : back->regions) {
        const auto i = w.regionNamed(r.id);
        ASSERT_TRUE(i.has_value());
        EXPECT_TRUE(r.deposits.size() < w.region(*i).simulation().state.patches.size() || *i != l.region);
    }
    WorldMap again(first(), p);
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.starfalls().size() == 2);
    const WorldMap::Fall a = again.starfalls()[0], b = again.starfalls()[1];
    EXPECT_EQ(a.id, l.id);
    EXPECT_EQ(a.region, l.region);
    EXPECT_NEAR(a.at.x, l.at.x, 1e-9);
    EXPECT_NEAR(a.impact, l.impact, 1e-9);
    EXPECT_NEAR(a.from, l.from, 1e-9);
    EXPECT_EQ(a.ore, l.ore);
    ASSERT_TRUE(a.patch.has_value());
    const OreDeposit& da = again.region(a.region).simulation().state.patches[size_t(*a.patch)];
    EXPECT_EQ(da.id, id);
    EXPECT_EQ(da.remaining, l.ore - 125);
    EXPECT_EQ(da.initial, l.ore);
    EXPECT_TRUE(!b.patch.has_value());
    EXPECT_EQ(b.id, coming->id);
    // The one coming lands when its time comes, and no other is told.
    ASSERT_TRUE(again.join(me));
    again.setClock(b.impact + 0.1);
    again.step();
    EXPECT_EQ(again.starfalls().size(), size_t(2));
    EXPECT_TRUE(again.starfalls()[1].patch.has_value());
}
