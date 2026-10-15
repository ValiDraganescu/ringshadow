// Two humans driving in one region (W5, docs/mmorpg-play.md "Two pilots"): the
// core drives a unit for each (`Simulation::drivers`), the server's seats map
// one to one onto them. Both move by their own inputs, both drill, each
// levels on its own record and keeps its own skills, each has its own
// headlamp beam at night, and either leaving leaves the other driving.
// Isolated scenes of a few seconds on The First Reach; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldClock.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 101;
constexpr PilotId b = 102;
const std::string keyA = "a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5";
const std::string keyB = "b6b6b6b6b6b6b6b6b6b6b6b6b6b6b6b6";

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
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.frontier = 0;
    p.firstStep = 0;
    return p;
}

Unit* driven(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

int64_t tick = 0;

/// Steps the world, each pilot walking and facing as asked.
void drive(WorldMap& w, int ticks, Vec2 walkA, double headingA, Vec2 walkB, double headingB, bool act = false, bool hold = false) {
    for (int i = 0; i < ticks; i++) {
        tick++;
        for (const auto& [p, walk, heading] : {std::tuple{a, walkA, headingA}, std::tuple{b, walkB, headingB}}) {
            PilotInput in;
            in.tick = tick;
            in.visit = w.visit(p);
            in.walk = walk;
            in.heading = heading;
            in.act = act && i == 0;
            in.hold = hold;
            w.input(p, in);
        }
        w.step();
    }
}

/// Both seated in the start region, each driving its own Prospector.
void seatBoth(WorldMap& w) {
    EXPECT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    w.step();
    EXPECT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
}

} // namespace

/// Both seated in one region drive a unit each: the seats one to one onto
/// the core's drivers, and both move by their own inputs.
TEST(two_pilots_drive_in_one_region) {
    WorldMap w(first(), params());
    seatBoth(w);
    ASSERT_TRUE(w.where(a) == w.where(b));
    Region& r = *w.regionOf(a);
    ASSERT_TRUE(r.driving(a).has_value() && r.driving(b).has_value());
    EXPECT_TRUE(*r.driving(a) != *r.driving(b));
    EXPECT_TRUE(r.simulation().piloted(*r.driving(a)) && r.simulation().piloted(*r.driving(b)));
    EXPECT_EQ(r.simulation().drivers.size(), size_t(1));
    const Vec2 a0 = driven(w, a)->position, b0 = driven(w, b)->position;
    drive(w, 120, Vec2(1, 0), 0.0, Vec2(1, 0), pi / 2);
    const Vec2 a1 = driven(w, a)->position, b1 = driven(w, b)->position;
    std::printf("  A moved %.1f cells, B %.1f\n", distance(a0, a1), distance(b0, b1));
    EXPECT_TRUE(distance(a0, a1) > 3 && distance(b0, b1) > 3);
    // Each where its own heading took it.
    EXPECT_TRUE(a1.x - a0.x > 2);
    EXPECT_TRUE(b1.y - b0.y > 2);
}

/// Both drill a deposit by their own act and hold, and each unload counts
/// for its own pilot only: its XP, its level.
TEST(two_pilots_drill_and_level_on_their_own) {
    WorldMap w(first(), params());
    seatBoth(w);
    Simulation& sim = w.regionOf(a)->simulation();
    // Two deposits apart, each pilot in front of one.
    std::vector<size_t> deps;
    for (size_t k = 0; k < sim.state.patches.size() && deps.size() < 2; k++) {
        const OreDeposit& d = sim.state.patches[k];
        if (d.remaining < 500) continue;
        if (!deps.empty() && distance(sim.state.patches[deps[0]].position, d.position) < 6) continue;
        const Vec2 stand = d.position + Vec2(-1.2, 0);
        if (!sim.nav->walkable(stand)) continue;
        deps.push_back(k);
    }
    ASSERT_TRUE(deps.size() == 2);
    Unit* ua = driven(w, a);
    Unit* ub = driven(w, b);
    ua->position = sim.state.patches[deps[0]].position + Vec2(-1.2, 0);
    ub->position = sim.state.patches[deps[1]].position + Vec2(-1.2, 0);
    ua->task = ub->task = Unit::Task::idle;
    const int64_t ida = ua->id, idb = ub->id;
    bool drilledA = false, drilledB = false;
    for (int s = 0; s < 8 * 60 && !(drilledA && drilledB); s++) {
        drive(w, 1, Vec2(0, 0), 0.0, Vec2(0, 0), 0.0, s % 30 == 0, true);
        drilledA = drilledA || sim.state.unit(ida)->carrying > 0 || sim.state.unit(ida)->task == Unit::Task::mining;
        drilledB = drilledB || sim.state.unit(idb)->carrying > 0 || sim.state.unit(idb)->task == Unit::Task::mining;
    }
    EXPECT_TRUE(drilledA && drilledB);
    // Each one's XP from its own work: A earns, B not.
    const double xpA = w.progress(a)->xp, xpB = w.progress(b)->xp;
    w.earn(a, Progression::xp(5));
    w.step();
    EXPECT_NEAR(w.progress(a)->xp, xpA + Progression::xp(5), 1);
    EXPECT_NEAR(w.progress(b)->xp, xpB, 1e-9);
    EXPECT_EQ(w.progress(a)->level(), Progression::level(xpA + Progression::xp(5)));
    std::printf("  A at level %lld, B at level %lld\n", (long long)w.progress(a)->level(), (long long)w.progress(b)->level());
}

/// At night each driven Prospector's headlamp sees along its own beam: a
/// point 10 cells along A's heading and one along B's, both in the pilots'
/// sight; not without the beams.
TEST(two_pilots_each_have_a_headlamp) {
    WorldMap w(first(), params());
    seatBoth(w);
    Simulation& sim = w.regionOf(a)->simulation();
    w.setClock(WorldClock::timeOf(1.0, w.clock()));
    Unit* ua = driven(w, a);
    Unit* ub = driven(w, b);
    // Apart, on open ground.
    ub->position = ua->position + Vec2(0, 30);
    if (!sim.nav->walkable(ub->position)) ub->position = *sim.nav->nearestFree(ub->position);
    ua->task = ub->task = Unit::Task::idle;
    drive(w, 20, Vec2(0, 0), 0.0, Vec2(0, 0), pi);
    const Vec2 aheadA = driven(w, a)->position + Vec2(10, 0), aheadB = driven(w, b)->position + Vec2(-10, 0);
    sim.lookNow();
    EXPECT_TRUE(sim.sees(Pilot::player, aheadA));
    EXPECT_TRUE(sim.sees(Pilot::player, aheadB));
    // Behind each: night sight only (6 cells), 10 out of it.
    EXPECT_TRUE(!sim.sees(Pilot::player, driven(w, a)->position + Vec2(-10, 0)) ||
                distance(driven(w, a)->position + Vec2(-10, 0), driven(w, b)->position) < 12);
}

/// Each keeps its own skills (the core's skill effects per driver) and its
/// own buildings; one leaving leaves the other driving with its own.
TEST(two_pilots_keep_their_own_skills_and_one_leaves) {
    WorldMap w(first(), params());
    seatBoth(w);
    Region& r = *w.regionOf(a);
    Simulation& sim = r.simulation();
    ProspectorProgress pa = *w.progress(a), pb = *w.progress(b);
    pa.xp = Progression::xp(20);
    pa.ranks[Skill::lightFrame] = 5;
    w.setProgress(a, pa);
    w.setProgress(b, pb);
    w.step();
    const Unit ua = *driven(w, a), ub = *driven(w, b);
    const double speedA = sim.boost(Stat::Speed{}, ua).apply(1.0), speedB = sim.boost(Stat::Speed{}, ub).apply(1.0);
    std::printf("  speed boost A %.2f, B %.2f\n", speedA, speedB);
    EXPECT_TRUE(speedA > speedB + 0.2);
    // A leaves: B drives on, the active driver now, with its own skills.
    w.leave(a);
    w.step();
    ASSERT_TRUE(r.driving(b).has_value());
    EXPECT_TRUE(sim.pilot && sim.pilot->unit == *r.driving(b));
    EXPECT_TRUE(sim.drivers.empty());
    EXPECT_NEAR(sim.boost(Stat::Speed{}, *driven(w, b)).apply(1.0), speedB, 1e-9);
}
