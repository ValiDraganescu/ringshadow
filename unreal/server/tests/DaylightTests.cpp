// Daylight sight in a world (S3, "during the day the line of sight should be
// everything in a zone, zones are not that big"): by day a pilot sees its
// whole region, every level, and every unit, bug and building in it goes in
// its snapshots; through dusk the sight closes in to the night's (half its
// day sight, the headlamp's wedge on top), and back up at dawn. The night is
// the same as without it. Isolated scenes of a few seconds on the first
// world; run from unreal/server (as `make test` does).

#include "test.h"

#include "Brood.h"
#include "Rules.h"
#include "WorldClock.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 83;

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
    p.starfalls = false;
    p.firstStep = 0;
    p.broodSeed = 0xda7119b7ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(seconds * 60);
    for (int i = 0; i < n; i++) w.step();
}

/// The world's clock at `hour`, on from now, and a few looks.
void at(WorldMap& w, double hour) {
    w.setClock(WorldClock::timeOf(hour, w.clock()));
    stepFor(w, 0.25);
}

Simulation& simOf(WorldMap& w) { return w.regionOf(me)->simulation(); }

/// Cells the pilots' side sees in the pilot's region now, and all of them.
size_t seenCells(WorldMap& w) {
    const auto s = simOf(w).sight(Pilot::player);
    size_t n = 0;
    if (s)
        for (uint8_t c : s->seen) n += c ? 1 : 0;
    return n;
}

size_t allCells(WorldMap& w) { return size_t(simOf(w).vision->width * simOf(w).vision->height); }

/// The pilot's driven unit.
const Unit* driven(WorldMap& w) {
    const auto id = w.regionOf(me)->driving(me);
    const auto i = id ? simOf(w).unitIndex(*id) : std::nullopt;
    return i ? &simOf(w).state.units[size_t(*i)] : nullptr;
}

/// The region's deposit farthest from the pilot, with ore.
size_t farDeposit(WorldMap& w) {
    const GameState& st = simOf(w).state;
    const Vec2 from = driven(w)->position;
    size_t best = 0;
    double d = -1;
    for (size_t i = 0; i < st.patches.size(); i++)
        if (st.patches[i].remaining > 100 && distance(st.patches[i].position, from) > d) {
            d = distance(st.patches[i].position, from);
            best = i;
        }
    return best;
}

} // namespace

/// The reach itself: the whole span with the sun up, the night's at night,
/// shrinking through dusk and growing through dawn, never past either end.
TEST(daylight_reach_eases_between_the_region_and_the_night) {
    const double night = Rules::vision(UnitKind::prospector) * Rules::nightSight, span = 180;
    EXPECT_NEAR(Rules::daylightReach(12.0, night, span), span, 1e-9);
    EXPECT_NEAR(Rules::daylightReach(0.0, night, span), night, 1e-9);
    double last = span + 1;
    int between = 0;
    for (double h = 15.0; h <= 22.0; h += 0.25) {
        const double r = Rules::daylightReach(h, night, span);
        EXPECT_TRUE(r <= last + 1e-9);
        EXPECT_TRUE(r >= night - 1e-9 && r <= span + 1e-9);
        if (r > night * 1.05 && r < span * 0.95) between++;
        last = r;
    }
    // Dusk takes a while, not a switch: several quarter hours in between.
    EXPECT_TRUE(between >= 4);
    last = 0;
    for (double h = 3.0; h <= 9.0; h += 0.25) {
        const double r = Rules::daylightReach(h, night, span);
        EXPECT_TRUE(r >= last - 1e-9);
        last = r;
    }
    // A span no wider than the night's sight changes nothing.
    EXPECT_NEAR(Rules::daylightReach(12.0, night, night * 0.5), night, 1e-9);
}

/// At noon the pilot sees every cell of its region, high ground and all,
/// and a group of bugs at the region's far deposit is in its snapshots.
TEST(daylight_noon_the_pilot_sees_its_whole_region) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    at(w, 12.0);
    ASSERT_TRUE(driven(w) != nullptr);
    EXPECT_EQ(seenCells(w), allCells(w));
    const auto r = *w.where(me);
    const size_t dep = farDeposit(w);
    const Vec2 site = simOf(w).state.patches[dep].position;
    std::printf("  the far deposit %.0f cells off\n", distance(site, driven(w)->position));
    EXPECT_TRUE(distance(site, driven(w)->position) > 2 * Rules::vision(UnitKind::prospector));
    const auto g = w.spawnBroodGroup(r, UnitKind::shardling, 4, WorldMap::Poi::deposit, int64_t(dep), site, true);
    ASSERT_TRUE(g.has_value());
    stepFor(w, 0.25);
    // Every bug, unit and building of the region in some snapshot (the far
    // ones come every few).
    std::set<int64_t> told;
    for (int k = 0; k < 8; k++) {
        const Snapshot s = w.snapshot(me);
        for (const EntityState& e : s.entities) told.insert(e.id());
        w.step();
    }
    size_t units = 0, bugs = 0;
    for (const Unit& u : simOf(w).state.units)
        if (u.hp > 0 && u.task != Unit::Task::aboard && u.task != Unit::Task::inBastion && u.task != Unit::Task::inDerrick) {
            units++;
            if (u.bug) bugs++;
            EXPECT_TRUE(told.count(u.id) > 0);
        }
    for (const Structure& b : simOf(w).state.structures) EXPECT_TRUE(told.count(b.id) > 0);
    std::printf("  %zu units (%zu bugs) and %zu buildings told\n", units, bugs, simOf(w).state.structures.size());
    EXPECT_TRUE(bugs >= 4);
}

/// Through dusk the region's seen cells fall step by step to the night's,
/// with hours in between; at dawn they come back up to the whole region.
TEST(daylight_dusk_eases_down_and_dawn_back_up) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    at(w, 12.0);
    const size_t all = allCells(w);
    at(w, 1.0);
    const size_t night = seenCells(w);
    EXPECT_TRUE(night < all / 10);
    // The same day's evening.
    w.setClock(WorldClock::timeOf(12.0, w.clock()));
    stepFor(w, 0.25);
    size_t last = seenCells(w), between = 0;
    // Dusk is the lamps' (`Rules::dark`): about an hour of the world's day.
    for (double h = 17.0; h <= 20.0; h += 0.1) {
        at(w, h);
        const size_t n = seenCells(w);
        if (n != last) std::printf("  %05.2f h: %zu of %zu cells\n", h, n, all);
        EXPECT_TRUE(n <= last);
        if (n > night + night / 2 && n < all - all / 20) between++;
        last = n;
    }
    EXPECT_TRUE(between >= 3);
    EXPECT_EQ(last, night);
    for (double h = 4.0; h <= 9.0; h += 0.1) {
        at(w, h);
        const size_t n = seenCells(w);
        EXPECT_TRUE(n >= last);
        last = n;
    }
    EXPECT_EQ(last, all);
}

/// At night the pilot sees what it saw before S3: the same cells with and
/// without the daylight, its headlamp's wedge on top of half its sight.
TEST(daylight_night_is_as_before) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    for (const double h : {23.0, 1.0, 3.5}) {
        at(w, h);
        Simulation with = simOf(w), without = simOf(w);
        without.daylightAt = std::nullopt;
        with.lookNow();
        without.lookNow();
        ASSERT_TRUE(with.sight(Pilot::player).has_value());
        EXPECT_TRUE(with.sight(Pilot::player)->seen == without.sight(Pilot::player)->seen);
        EXPECT_TRUE(with.sight(Pilot::player)->ids == without.sight(Pilot::player)->ids);
    }
    // And by day the daylight is what makes the difference.
    at(w, 12.0);
    Simulation without = simOf(w);
    without.daylightAt = std::nullopt;
    without.lookNow();
    size_t n = 0;
    const std::optional<Sight> night = without.sight(Pilot::player);
    for (uint8_t c : night->seen) n += c ? 1 : 0;
    EXPECT_TRUE(n < allCells(w) / 4);
}

/// A group that leaves by day walks to its exit at the region's edge and
/// goes there, not in the pilot's plain sight halfway.
TEST(daylight_a_leaving_group_goes_at_the_edge) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    at(w, 12.0);
    const auto r = *w.where(me);
    // The east vein (E1): a clear way from it to the region's edge.
    size_t dep = 0;
    for (size_t i = 0; i < simOf(w).state.patches.size(); i++)
        if (simOf(w).state.patches[i].position.x > simOf(w).state.patches[dep].position.x) dep = i;
    const Vec2 site = simOf(w).state.patches[dep].position;
    const auto g = w.spawnBroodGroup(r, UnitKind::shardling, 3, WorldMap::Poi::deposit, int64_t(dep), site, true);
    ASSERT_TRUE(g.has_value());
    // A whole day on: noon again, its lingering over.
    stepFor(w, 0.25);
    w.setClock(w.clock() + WorldClock::dayLength);
    double nearest = 1e9;
    bool gone = false;
    for (int k = 0; k < 90 * 4 && !gone; k++) {
        stepFor(w, 0.25);
        gone = true;
        for (const Unit& u : simOf(w).state.units)
            if (u.bug && u.bug->group == *g) {
                gone = false;
                if (u.bug->leaving) nearest = std::min(nearest, distance(u.position, u.bug->exit));
            }
    }
    EXPECT_TRUE(gone);
    // It got to its exit before it went.
    EXPECT_TRUE(nearest <= 1.5);
}
