// The range model of every gun (docs/shooter-feel.md): a spread cone that
// grows with movement and firing, damage that falls off past range, attacks
// split into rounds, and the AI opening fire out to its engage reach.
#include "SimHelpers.h"

using namespace ac;
using namespace simtest;

namespace {

GameState empty() { return bare(2, 600, {}, {}, 1); }

/// Of `n` first rounds (no spread from earlier ones) of unit `me` at `them`,
/// a step apart, the share that hit.
double hitShare(Simulation& sim, int64_t me, int64_t them, double moving, int n = 400) {
    int hits = 0;
    for (int k = 0; k < n; k++) {
        sim.state.time += 1.0 / 30;
        Unit& u = *std::find_if(sim.state.units.begin(), sim.state.units.end(), [&](const Unit& x) { return x.id == me; });
        u.spread = std::nullopt;
        const auto t = sim.target(them);
        hits += sim.aimRound(u, sim.weapon(u, t->air), *t, moving).hit;
    }
    return double(hits) / n;
}

} // namespace

/// Past its range a round keeps `falloff` of its damage for every further
/// range it flies; inside, all of it; a flame all of it where it reaches.
TEST(spread_damageFallsOffPastRange) {
    const UnitStats rifle = Rules::stats(Unit::Kind::ranger);
    EXPECT_EQ(Simulation::falloffShare(rifle, 4, 5), 1.0);
    EXPECT_NEAR(Simulation::falloffShare(rifle, 10, 5), 0.9, 1e-12);
    EXPECT_NEAR(Simulation::falloffShare(rifle, 15, 5), 0.81, 1e-12);
    EXPECT_NEAR(Simulation::falloffShare(UnitStats::minigun, 10, 5), 0.5, 1e-12);
    EXPECT_EQ(Simulation::falloffShare(Rules::stats(Unit::Kind::firefly), 5.2, 5), 1.0);
}

/// The cone: the standing radius below a third of full speed, all of the
/// moving extra from 95%; each round widens it, and that fades by 90% in
/// the weapon's recovery time.
TEST(spread_coneGrowsWithMovingAndFiringAndFades) {
    GameState s = empty();
    const int64_t me = addUnit(s, Unit::Kind::ranger, Vec2::zero, 0);
    Simulation sim(s);
    Unit& u = sim.state.units[0];
    const UnitStats w = sim.weapon(u);
    EXPECT_NEAR(sim.cone(u, w, 0), w.coneStand, 1e-12);
    EXPECT_NEAR(sim.cone(u, w, 0.3), w.coneStand, 1e-12);
    EXPECT_NEAR(sim.cone(u, w, 1), w.coneStand + w.coneMove, 1e-12);
    u.spread = 0.01;
    EXPECT_NEAR(sim.cone(u, w, 0), w.coneStand + 0.01, 1e-12);
    for (int k = 0; k < 6; k++) sim.upkeep(u, w.coneRecovery / 6);
    EXPECT_NEAR(u.spread.value_or(0), 0.001, 1e-9);
    (void)me;
}

/// The coil rifle hits a Ranger every time standing, even 20 cells off, and
/// rarely on the run 10 off; the chain gun sprays: it hits a Ranger close
/// by, and about half the time 20 off.
TEST(spread_theRifleIsPreciseAndTheChainGunSprays) {
    GameState s = empty();
    const int64_t me = addUnit(s, Unit::Kind::ranger, Vec2::zero, 0);
    const int64_t near = addUnit(s, Unit::Kind::ranger, Vec2(4, 0), 1);
    const int64_t mid = addUnit(s, Unit::Kind::ranger, Vec2(0, 10), 1);
    const int64_t far = addUnit(s, Unit::Kind::ranger, Vec2(-20, 0), 1);
    Simulation sim(s);
    EXPECT_EQ(hitShare(sim, me, far, 0), 1.0);
    const double running = hitShare(sim, me, mid, 1);
    EXPECT_TRUE(running > 0.15 && running < 0.5);
    sim.state.players[0].upgrades = std::set<Upgrade>{Upgrade::minigun};
    EXPECT_EQ(hitShare(sim, me, near, 0), 1.0);
    const double spray = hitShare(sim, me, far, 0);
    EXPECT_TRUE(spray > 0.3 && spray < 0.65);
}

/// A miss says how far off the sight it flew, within the cone, and comes
/// down on the ground off its mark.
TEST(spread_aMissFliesOffTheSight) {
    GameState s = empty();
    const int64_t me = addUnit(s, Unit::Kind::ranger, Vec2::zero, 0);
    const int64_t them = addUnit(s, Unit::Kind::ranger, Vec2(12, 0), 1);
    Simulation sim(s);
    int misses = 0;
    for (int k = 0; k < 200; k++) {
        sim.state.time += 1.0 / 30;
        Unit& u = sim.state.units[0];
        u.spread = std::nullopt;
        const auto t = sim.target(them);
        const double c = sim.cone(u, sim.weapon(u), 1);
        const Simulation::Round r = sim.aimRound(u, sim.weapon(u), *t, 1);
        if (r.hit) continue;
        misses++;
        EXPECT_TRUE(std::hypot(r.yaw, r.pitch) <= c + 1e-12);
        EXPECT_EQ(r.share, 0.0);
        EXPECT_TRUE(distance(r.at, t->position) > t->radius);
    }
    EXPECT_TRUE(misses > 0);
    (void)me;
}

/// The same scene plays out the same every time: the rolls are seeded by
/// the unit and the time.
TEST(spread_rollsAreTheSameEveryRun) {
    auto play = [] {
        GameState s = empty();
        addUnit(s, Unit::Kind::ranger, Vec2::zero, 0, Unit::Task::idle);
        addUnit(s, Unit::Kind::ranger, Vec2(7, 0), 1, Unit::Task::idle, pi);
        Simulation sim(s);
        return run(sim, 6);
    };
    const auto a = play(), b = play();
    EXPECT_TRUE(a == b);
}

/// An AI Ranger on its way to a building opens fire before it is in range:
/// out to 1.6 times its range, closing in as it shoots.
TEST(spread_theAiOpensFireFromItsEngageReach) {
    GameState s = empty();
    const int64_t me = addUnit(s, Unit::Kind::ranger, Vec2::zero, 0);
    const int64_t dome = addStructure(s, Structure::Kind::habDome, Vec2(12, 0), 1);
    Simulation sim(s);
    sim.issue(Command::Attack{0, Vec2(12, 0)});
    std::optional<double> firstGap;
    for (int k = 0; k < 10 * 30 && !firstGap; k++) {
        for (const auto& e : sim.step(1.0 / 30)) {
            const auto sh = e.as<GameEvent::Shot>();
            const auto mi = e.as<GameEvent::Missed>();
            if ((sh && sh->unit == me) || (mi && mi->unit == me)) {
                firstGap = sim.gap(*sim.state.unit(me), *sim.target(dome));
                break;
            }
        }
    }
    ASSERT_TRUE(firstGap.has_value());
    EXPECT_TRUE(*firstGap > Rules::rangerRange + 0.5);
    EXPECT_TRUE(*firstGap <= Rules::rangerRange * Rules::engageReach + 0.2);
}
