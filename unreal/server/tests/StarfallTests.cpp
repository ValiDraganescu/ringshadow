// Starfall (Starfall.h, WorldStarfall.cpp; docs/weather.md "Starfall"): the
// schedule by a fixed seed, the place rules, the rich deposit that pays half
// again in XP and is gone when emptied, the dev fall near a pilot, and the
// save round trip. Isolated scenes of a few seconds; run from unreal/server.

#include "test.h"

#include "NavGrid.h"
#include "NetSnapshot.h"
#include "Rules.h"
#include "Starfall.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <sys/stat.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>

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

TEST(starfall_schedule_one_a_day_by_the_seed) {
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
    // E1: about one a world day, realm-wide.
    EXPECT_EQ(total, int64_t(30));
    EXPECT_TRUE(Starfall::impacts(kSeed, 3, day) != Starfall::impacts(kSeed + 1, 3, day));
    // The next impact after a time: the first of the day, then on.
    const auto d0 = Starfall::impacts(kSeed, 0, day);
    EXPECT_NEAR(Starfall::next(kSeed, 0, day), d0[0], 1e-9);
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
    // The impact: a deposit after the region's own, of 100 to 1,000 ore (E1).
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
    EXPECT_TRUE(d.initial >= Starfall::oreFewest && d.initial <= Starfall::oreMost);
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
    // by the rules, clear of the region's pads, sites and ore. Ten at a time
    // in a fresh world (E1: the home's ground spared, a region holds only so
    // many craters at once).
    std::map<size_t, int> count;
    int placed = 0;
    std::unique_ptr<WorldMap> v;
    for (int i = 0; i < 60; i++) {
        if (i % 10 == 0) {
            v = std::make_unique<WorldMap>(first(), params());
            ASSERT_TRUE(v->join(me));
            v->setClock(1 + 977.0 * i);
        }
        const auto f = v->forceStarfall(30);
        if (!f) continue;
        placed += 1;
        count[f->region] += 1;
        const Region& g = v->region(f->region);
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

// A world that had many falls (61, the user's, 2026-10-15): every fall stays
// in the world (its crater, its deposit), but a pilot's snapshot lists only
// the falls coming or just down and its own region's craters, at most 24, so
// a snapshot still fits in a part or two: with all of them the sky filled
// each part and no snapshot ever arrived whole (the client saw nothing).
TEST(starfall_many_falls_keep_the_snapshot_small) {
    WorldMap w(first(), params());
    w.join(me);
    const size_t regions = first().regions.size();
    for (int i = 0; i < 61; i++) {
        ASSERT_TRUE(w.forceStarfall(0, size_t(i) % regions).has_value());
        stepFor(w, 1.0);
    }
    stepFor(w, 31.0);
    const auto coming = w.forceStarfall(40, w.regionNamed("cold-fork"));
    ASSERT_TRUE(coming.has_value());
    w.step();
    const net::Snapshot s = w.snapshot(me);
    ASSERT_TRUE(s.sky.has_value());
    EXPECT_TRUE(s.sky->starfalls.size() <= WorldMap::skyFallsMost);
    bool told = false;
    for (const auto& f : s.sky->starfalls) {
        told = told || f.id == coming->id;
        // Landed and done: only this region's craters.
        if (w.clock() >= f.impact + 30.0) EXPECT_EQ(size_t(f.region), *w.where(me));
    }
    EXPECT_TRUE(told);
    net::SnapshotWriter wr;
    const auto o = wr.write(s, 0, 1 << 20);
    EXPECT_TRUE(o.messages.size() <= 2);
}

/// E1: a fall's ore is 100 to 1,000, spread over the range.
TEST(starfall_ore_one_hundred_to_a_thousand) {
    int64_t lo = Starfall::oreMost, hi = Starfall::oreFewest;
    for (uint64_t i = 0; i < 2000; i++) {
        const int64_t ore = Starfall::oreFrom(Starfall::mix(i));
        EXPECT_TRUE(ore >= Starfall::oreFewest && ore <= Starfall::oreMost);
        lo = std::min(lo, ore);
        hi = std::max(hi, ore);
    }
    EXPECT_TRUE(lo < 150 && hi > 950);
    // The blast: a quarter of a medium region's 144 cells across.
    EXPECT_NEAR(Starfall::blastRadius(Vec2(144, 96)), 18, 1e-9);
    EXPECT_NEAR(Starfall::blastShare(0, 18), 1, 1e-9);
    EXPECT_NEAR(Starfall::blastShare(18 * Starfall::blastKill, 18), 1, 1e-9);
    EXPECT_TRUE(Starfall::blastShare(12, 18) > 0 && Starfall::blastShare(12, 18) < 1);
    EXPECT_NEAR(Starfall::blastShare(18, 18), 0, 1e-9);
}

/// E1: the blast destroys a building and a unit it falls on and hurts, not
/// kills, a building and a unit at its edge; past its edge nothing.
TEST(starfall_blast_destroys_at_the_centre_and_hurts_to_its_edge) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    stepFor(w, 0.5);
    const size_t r = *w.regionNamed("tern-ridge");
    const auto f = w.forceStarfall(3, r);
    ASSERT_TRUE(f.has_value());
    // A moment before the impact, so the units stand where they are put.
    w.setClock(f->impact - 0.1);
    Simulation& sim = w.region(r).simulation();
    GameState& st = sim.state;
    const double R = Starfall::blastRadius(w.region(r).places()->extent());
    const auto kind = StructureKind::habDome;
    const double rb = Rules::radius(kind);
    // At the centre, at the edge (1.5 cells inside it, by its nearest edge),
    // and past it, each way round so they stand clear of each other.
    const int64_t centre = st.nextID++, edge = st.nextID++, past = st.nextID++;
    st.structures.push_back(Structure(centre, kind, Pilot::player, f->at + Vec2(2, 0)));
    st.structures.push_back(Structure(edge, kind, Pilot::player, f->at + Vec2(0, R - 1.5 + rb)));
    st.structures.push_back(Structure(past, kind, Pilot::player, f->at + Vec2(-(R + 2 + rb), 0)));
    const auto ranger = UnitKind::ranger;
    const double ru = Rules::radius(ranger);
    const int64_t uCentre = st.nextID++, uEdge = st.nextID++, uPast = st.nextID++;
    st.units.push_back(sim.freshUnit(ranger, uCentre, Pilot::player, f->at + Vec2(0, -2), 0));
    st.units.push_back(sim.freshUnit(ranger, uEdge, Pilot::player, f->at + Vec2(R - 1.5 + ru, 3), 0));
    st.units.push_back(sim.freshUnit(ranger, uPast, Pilot::player, f->at + Vec2(-3, -(R + 2 + ru)), 0));
    sim.reindex();
    const double full = Rules::hp(kind), unitFull = sim.maxHP(*st.unit(uEdge));
    stepFor(w, 0.25);
    ASSERT_TRUE(w.starfalls()[0].patch.has_value());
    EXPECT_TRUE(!st.structure(centre).has_value());
    EXPECT_TRUE(!st.unit(uCentre).has_value());
    ASSERT_TRUE(st.structure(edge).has_value() && st.structure(past).has_value());
    std::printf("  blast %.0f cells: the edge's Hab Dome %.0f of %.0f hp, its Ranger %.0f of %.0f\n", R, st.structure(edge)->hp, full,
                st.unit(uEdge)->hp, unitFull);
    EXPECT_TRUE(st.structure(edge)->hp > 0 && st.structure(edge)->hp < full);
    EXPECT_NEAR(st.structure(past)->hp, full, 1e-9);
    ASSERT_TRUE(st.unit(uEdge).has_value() && st.unit(uPast).has_value());
    EXPECT_TRUE(st.unit(uEdge)->hp > 0 && st.unit(uEdge)->hp < unitFull);
    EXPECT_NEAR(st.unit(uPast)->hp, unitFull, 1e-9);
}

/// E1: no fall's blast reaches the start region's Citadel or the 25 cells
/// round it (with no Citadel yet, its pad); elsewhere, beside another
/// region's Citadel too, nothing is spared.
TEST(starfall_spares_the_home_ground_only) {
    WorldMap w(first(), params());
    const size_t s = *w.regionNamed("start");
    GameState& st = w.region(s).simulation().state;
    const double R = Starfall::blastRadius(w.region(s).places()->extent());
    const Vec2 pad = w.region(s).places()->pads.at(0).at;
    const GroundRect b = w.region(s).simulation().map->bounds;
    // With no Citadel: round the pad.
    int fits = 0;
    for (double x = b.minX; x <= b.maxX; x += 2)
        for (double z = b.minZ; z <= b.maxZ; z += 2)
            if (w.craterFits(s, Vec2(x, z))) {
                fits += 1;
                EXPECT_TRUE(length(Vec2(x, z) - pad) >= R + Starfall::homeSpared);
            }
    EXPECT_TRUE(fits > 0);
    // A Citadel: round it (and no longer the pad).
    const Vec2 home = pad + Vec2(-30, 6);
    st.structures.push_back(Structure(st.nextID++, StructureKind::citadel, Pilot::player, home));
    w.region(s).simulation().reindex();
    const double reach = Rules::radius(StructureKind::citadel) + Starfall::homeSpared + R;
    fits = 0;
    int nearPad = 0;
    for (double x = b.minX; x <= b.maxX; x += 2)
        for (double z = b.minZ; z <= b.maxZ; z += 2)
            if (w.craterFits(s, Vec2(x, z))) {
                fits += 1;
                EXPECT_TRUE(length(Vec2(x, z) - home) >= reach);
                if (length(Vec2(x, z) - pad) < R + Starfall::homeSpared) nearPad += 1;
            }
    EXPECT_TRUE(fits > 0 && nearPad > 0);
    // Another region's Citadel: not spared.
    const size_t t = *w.regionNamed("tern-ridge");
    GameState& tt = w.region(t).simulation().state;
    const Vec2 there = w.region(t).places()->pads.at(0).at + Vec2(20, 10);
    tt.structures.push_back(Structure(tt.nextID++, StructureKind::citadel, Pilot::player, there));
    w.region(t).simulation().reindex();
    int beside = 0;
    for (double x = -24; x <= 24; x += 2)
        for (double z = -24; z <= 24; z += 2)
            if (w.craterFits(t, there + Vec2(x, z))) beside += 1;
    EXPECT_TRUE(beside > 0);
}
