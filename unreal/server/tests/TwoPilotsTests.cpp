// Two humans driving in one region (W5, docs/mmorpg-play.md "Two pilots"): the
// core drives a unit for each (`Simulation::drivers`), the server's seats map
// one to one onto them. Both move by their own inputs, both drill, each
// levels on its own record and keeps its own skills, each has its own
// headlamp beam at night, and either leaving leaves the other driving.
// Isolated scenes of a few seconds on The First Reach; run from unreal/server.

#include <set>

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

/// Each keeps its own wallet in the region (W5): the side's bank is the
/// active driver's, the other's is swapped in for its own turn. A build
/// costs its builder only, each snapshot tells its own pilot's, and one
/// leaving and coming back finds its own again.
TEST(two_pilots_keep_their_own_wallets) {
    WorldMap w(first(), params());
    seatBoth(w);
    Region& r = *w.regionOf(a);
    Simulation& sim = r.simulation();
    r.setBank(a, 900, 30);
    r.setBank(b, 100, 5);
    w.step();
    EXPECT_TRUE(w.wallet(a) == (WorldMap::Wallet{900, 30}));
    EXPECT_TRUE(w.wallet(b) == (WorldMap::Wallet{100, 5}));
    EXPECT_EQ(r.snapshot(a).players[0].ore, int64_t(900));
    EXPECT_EQ(r.snapshot(b).players[0].ore, int64_t(100));
    // B (the other driver, not the active one) builds: B pays.
    ProspectorProgress pb = *w.progress(b);
    pb.xp = Progression::xp(20);
    pb.giveBlueprint(StructureKind::habDome);
    w.setProgress(b, pb);
    r.setBank(b, 2000, 500);
    w.step();
    // Off the start pad's ground.
    Unit* ub = driven(w, b);
    ub->position = ub->position + Vec2(0, 18);
    if (!sim.nav->walkable(ub->position)) ub->position = *sim.nav->nearestFree(ub->position);
    ub->task = Unit::Task::idle;
    w.step();
    const Vec2 from = driven(w, b)->position;
    const double reach = Region::buildReach(StructureKind::habDome);
    bool built = false;
    for (double d = reach; d <= reach + Region::buildSlack && !built; d += 0.5)
        for (int k = 0; k < 16 && !built; k++) {
            const double h = k * 2 * pi / 16;
            const auto spot = sim.snap(StructureKind::habDome, from + Vec2(std::cos(h), std::sin(h)) * d);
            if (!spot || sim.placementRefusal(StructureKind::habDome, *spot)) continue;
            const auto why = w.build(b, StructureKind::habDome, *spot);
            if (why && k == 0) std::printf("  refused: %s\n", why->c_str());
            built = !why;
        }
    ASSERT_TRUE(built);
    w.step();
    std::printf("  after B's Hab Dome: A %lld ore, B %lld ore\n", (long long)w.wallet(a)->ore, (long long)w.wallet(b)->ore);
    EXPECT_EQ(w.wallet(a)->ore, int64_t(900));
    EXPECT_TRUE(w.wallet(b)->ore < 2000 && w.wallet(b)->ore >= 2000 - 3 * Rules::cost(StructureKind::habDome));
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(30));
    // B leaves and comes back (a new connection, its key): its own again.
    const WorldMap::Wallet left = *w.wallet(b);
    w.leave(b);
    for (int i = 0; i < 30; i++) w.step();
    EXPECT_TRUE(w.wallet(a) == (WorldMap::Wallet{900, 30}));
    constexpr PilotId b2 = 103;
    EXPECT_TRUE(w.seat(b2, keyB) == WorldMap::Seated::restored);
    w.step();
    EXPECT_TRUE(w.wallet(b2) == left);
    EXPECT_TRUE(w.wallet(a) == (WorldMap::Wallet{900, 30}));
    EXPECT_TRUE(w.regionOf(b2)->driving(b2).has_value());
}

/// A dev line for the second human (`as 2 LINE`, Server::devControl): it
/// acts on that pilot, the first left as it was.
TEST(two_pilots_dev_lines_for_the_second) {
    WorldMap w(first(), params());
    seatBoth(w);
    const WorldMap::Wallet wa = *w.wallet(a);
    EXPECT_TRUE(w.devPilotId(1) == a && w.devPilotId(2) == b && !w.devPilotId(3));
    w.devAs = w.devPilotId(2);
    w.devFerry("ferry mh 77");
    w.devAs = std::nullopt;
    w.step();
    EXPECT_EQ(w.wallet(b)->hydrogen, int64_t(77));
    EXPECT_TRUE(w.wallet(a) == wa);
}

/// Dev lines go by when each pilot joined, not by its connection id (they
/// are random): the first to join is pilot 1, even with the larger id.
TEST(two_pilots_dev_lines_go_by_join_order) {
    WorldMap w(first(), params());
    constexpr PilotId big = 4000000000, small = 800000000;
    EXPECT_TRUE(w.seat(big, keyA) == WorldMap::Seated::fresh);
    w.step();
    EXPECT_TRUE(w.seat(small, keyB) == WorldMap::Seated::fresh);
    w.step();
    EXPECT_TRUE(w.devPilotId(1) == big && w.devPilotId(2) == small);
    w.devFerry("ferry mh 33");
    w.step();
    EXPECT_EQ(w.wallet(big)->hydrogen, int64_t(33));
    // The first leaves and comes back: last in the order now.
    w.leave(big);
    w.step();
    EXPECT_TRUE(w.seat(big + 1, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.devPilotId(1) == small && w.devPilotId(2) == big + 1);
}

/// Each welds what it puts up (W5): A's building and B's, each finished by
/// its own Prospector standing by it, the other driving too.
TEST(two_pilots_each_weld_their_own_building) {
    WorldMap w(first(), params());
    seatBoth(w);
    Region& r = *w.regionOf(a);
    Simulation& sim = r.simulation();
    std::map<PilotId, int64_t> built;
    for (const PilotId who : {a, b}) {
        ProspectorProgress pr = *w.progress(who);
        pr.xp = Progression::xp(20);
        pr.giveBlueprint(StructureKind::habDome);
        w.setProgress(who, pr);
        r.setBank(who, 3000, 500);
        Unit* u = driven(w, who);
        u->position = u->position + Vec2(who == a ? -16 : 16, 18);
        if (!sim.nav->walkable(u->position)) u->position = *sim.nav->nearestFree(u->position);
        u->task = Unit::Task::idle;
    }
    w.step();
    for (const PilotId who : {a, b}) {
        const Vec2 from = driven(w, who)->position;
        const double reach = Region::buildReach(StructureKind::habDome);
        for (double d = reach; d <= reach + Region::buildSlack && !built.count(who); d += 0.5)
            for (int k = 0; k < 16 && !built.count(who); k++) {
                const double h = k * 2 * pi / 16;
                const auto spot = sim.snap(StructureKind::habDome, from + Vec2(std::cos(h), std::sin(h)) * d);
                if (!spot || sim.placementRefusal(StructureKind::habDome, *spot)) continue;
                const int64_t id = sim.state.nextID;
                if (!w.build(who, StructureKind::habDome, *spot)) built[who] = id;
            }
    }
    ASSERT_TRUE(built.size() == 2);
    (void)w.takeNews(a);
    (void)w.takeNews(b);
    for (int i = 0; i < 60 * 120; i++) {
        drive(w, 1, Vec2(0, 0), 0.0, Vec2(0, 0), 0.0);
        if (sim.state.structure(built[a])->complete() && sim.state.structure(built[b])->complete()) break;
    }
    std::printf("  A's Hab Dome %.0f hp, B's %.0f hp, after %.0f s\n", double(sim.state.structure(built[a])->hp),
                double(sim.state.structure(built[b])->hp), sim.state.time);
    EXPECT_TRUE(sim.state.structure(built[a])->complete());
    EXPECT_TRUE(sim.state.structure(built[b])->complete());
    // Each is told of both: "PILOT 443E BUILT A HAB DOME" on the other's HUD.
    for (const PilotId who : {a, b}) {
        std::set<std::string> by;
        for (const auto& n : w.takeNews(who))
            if (n.kind == net::ProgressMessage::NewsKind::built) by.insert(n.by);
        EXPECT_EQ(by.size(), size_t(2));
    }
}
