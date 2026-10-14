// The Matriarch in the core (docs/creatures.md, "The Matriarch"; N3): her
// slam, her spit and its puddle, her roar's pose, her tether and her mending,
// each from a hand-built scene of a few seconds, never a game. Her nest, her
// roars' rally and her death's rewards are the server's
// (server/tests/MatriarchTests.cpp).
#include "Brood.h"
#include "SimHelpers.h"

using namespace ac;
using namespace simtest;

namespace {

struct Scene {
    GameState s;
    Vec2 at;
};

/// Blue's open ground and nothing on it; player 1 is the Brood.
Scene scene() {
    GameState s = GameState::new_(homeMap());
    Vec2 at;
    for (const auto& b : s.structures) if (b.owner == 0) { at = b.position + Vec2(0, 12); break; }
    s.units.clear();
    s.structures.clear();
    return Scene{s, at};
}

Simulation sim(const GameState& s) {
    Simulation out(s, homeMap());
    out.broodSide = 1;
    return out;
}

/// The Matriarch at `p` facing east, her nest's centre there.
int64_t queen(GameState& s, Vec2 p) {
    const int64_t id = addUnit(s, Unit::Kind::matriarch, p, 1);
    Unit& u = s.units.back();
    u.bug = Bug{};
    u.bug->site = p;
    u.bug->coming = false;
    u.bug->tether = Matriarch::tether;
    return id;
}

Unit& unitIn(GameState& s, int64_t id) {
    for (Unit& u : s.units) if (u.id == id) return u;
    return s.units.front();
}

const double edge = Rules::radius(Unit::Kind::matriarch);

} // namespace

/// Her slam: the forelimbs rise for 0.8 s, then 40 to every ground foe in
/// front of her within 3 cells of her edge; one behind her is spared.
TEST(Matriarch_slamStrikesWhatIsInFront) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    const int64_t front = addUnit(s, Unit::Kind::prospector, at + Vec2(edge + 1.5, 0.6), 0);
    const int64_t side = addUnit(s, Unit::Kind::prospector, at + Vec2(edge + 1.0, -0.8), 0);
    const int64_t behind = addUnit(s, Unit::Kind::prospector, at + Vec2(-edge - 1.5, 0), 0);
    unitIn(s, q).bug->prey = front;
    unitIn(s, q).bug->spitReady = 1e9;  // the slam alone
    Simulation m = sim(s);
    const double full = Rules::hp(Unit::Kind::prospector);
    run(m, 0.5);
    // Rising: nothing yet, the strike counted for the client.
    EXPECT_NEAR(hp(m, front), full, 1e-9);
    EXPECT_TRUE(m.state.unit(q)->bug->slamAt.has_value());
    EXPECT_EQ(m.state.unit(q)->bug->strikes, 1);
    EXPECT_NEAR(m.state.unit(q)->bug->sac, Matriarch::struckSlam, 1e-9);
    EXPECT_TRUE(m.state.unit(q)->bug->state == Bug::State::attack);
    run(m, 0.6);
    EXPECT_NEAR(hp(m, front), full - Matriarch::slamUnit, 1e-6);
    EXPECT_NEAR(hp(m, side), full - Matriarch::slamUnit, 1e-6);
    EXPECT_NEAR(hp(m, behind), full, 1e-9);
}

/// A building in front of her takes 60 less its armour.
TEST(Matriarch_slamCracksABuilding) {
    auto [s, at] = scene();
    const double r = Rules::radius(Structure::Kind::habDome);
    const int64_t dome = addStructure(s, Structure::Kind::habDome, at + Vec2(edge + r + 1, 0), 0);
    const int64_t q = queen(s, at);
    unitIn(s, q).bug->raid = dome;
    unitIn(s, q).bug->spitReady = 1e9;
    Simulation m = sim(s);
    run(m, 1.2);
    const auto d = m.state.structure(dome);
    ASSERT_TRUE(d.has_value());
    const double lost = Rules::hp(Structure::Kind::habDome) - d->hp;
    EXPECT_TRUE(lost > Matriarch::slamBuilding - 6 && lost <= Matriarch::slamBuilding);
}

/// Her spit: 18 to a foe 10 cells off, a puddle under it, again 3 s later.
TEST(Matriarch_spitReachesTwelveCells) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    const int64_t far = addUnit(s, Unit::Kind::prospector, at + Vec2(edge + 10, 0), 0);
    unitIn(s, q).bug->prey = far;
    unitIn(s, q).bug->tether = 0;  // she stays put
    Simulation m = sim(s);
    run(m, 0.2);
    const double full = Rules::hp(Unit::Kind::prospector);
    EXPECT_NEAR(hp(m, far), full - Matriarch::spitUnit, 1.0);
    ASSERT_TRUE(m.puddles.size() == 1);
    EXPECT_TRUE(distance(m.puddles.front().at, at + Vec2(edge + 10, 0)) < 0.5);
    EXPECT_NEAR(m.state.unit(q)->bug->sac, Matriarch::struckSpit, 1e-9);
    // Standing in the puddle burns 4 a second; the second spit at 3 s.
    run(m, 2.0);
    EXPECT_NEAR(hp(m, far), full - Matriarch::spitUnit - Matriarch::puddleDps * 2.2, 0.5);
    EXPECT_EQ(m.state.unit(q)->bug->strikes, 1);
    run(m, 1.0);
    EXPECT_EQ(m.state.unit(q)->bug->strikes, 2);
}

/// A puddle burns only the ground foes in it, and dries after 4 s.
TEST(Matriarch_puddleBurnsThenDries) {
    auto [s, at] = scene();
    const int64_t in = addUnit(s, Unit::Kind::prospector, at, 0);
    const int64_t out = addUnit(s, Unit::Kind::prospector, at + Vec2(3, 0), 0);
    Simulation m = sim(s);
    m.puddles.push_back({at, m.state.time + Matriarch::puddleFor, 1});
    const double full = Rules::hp(Unit::Kind::prospector);
    run(m, 2.0);
    EXPECT_NEAR(hp(m, in), full - 8, 0.2);
    EXPECT_NEAR(hp(m, out), full, 1e-9);
    run(m, 3.0);
    EXPECT_TRUE(m.puddles.empty());
    EXPECT_NEAR(hp(m, in), full - 16, 0.2);
}

/// Her spit reaches the air.
TEST(Matriarch_spitHitsAFlyer) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    const int64_t flyer = addUnit(s, Unit::Kind::kestrel, at + Vec2(edge + 6, 0), 0);
    unitIn(s, q).bug->prey = flyer;
    unitIn(s, q).bug->tether = 0;
    Simulation m = sim(s);
    ASSERT_TRUE(m.state.unit(flyer)->airborne());
    run(m, 0.2);
    EXPECT_TRUE(hp(m, flyer) < Rules::hp(Unit::Kind::kestrel));
}

/// She chases no farther than her tether from her nest's centre.
TEST(Matriarch_staysOnHerTether) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    const int64_t far = addUnit(s, Unit::Kind::prospector, at + Vec2(edge + 18, 0), 0);
    unitIn(s, q).bug->prey = far;
    unitIn(s, q).bug->spitReady = 1e9;
    Simulation m = sim(s);
    run(m, 15);
    EXPECT_TRUE(distance(m.state.unit(q)->position, at) <= Matriarch::tether + 0.5);
    EXPECT_TRUE(distance(m.state.unit(q)->position, at) >= Matriarch::tether - 1.5);
}

/// Rearing in a roar she neither strikes nor walks.
TEST(Matriarch_roarHoldsHerStill) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    const int64_t front = addUnit(s, Unit::Kind::prospector, at + Vec2(edge + 1.5, 0), 0);
    unitIn(s, q).bug->prey = front;
    unitIn(s, q).bug->roarUntil = Matriarch::roarFor;
    Simulation m = sim(s);
    run(m, 1.0);
    EXPECT_TRUE(m.state.unit(q)->bug->state == Bug::State::roar);
    EXPECT_EQ(m.state.unit(q)->bug->strikes, 0);
    run(m, 1.5);
    EXPECT_TRUE(m.state.unit(q)->bug->strikes >= 1);
}

/// She mends 20 a second, but only after 20 s without a hit.
TEST(Matriarch_mendsAfterTwentySeconds) {
    auto [s, at] = scene();
    const int64_t q = queen(s, at);
    unitIn(s, q).hp = 5000;
    unitIn(s, q).hitAt = 0;
    Simulation m = sim(s);
    run(m, 19);
    EXPECT_NEAR(hp(m, q), 5000, 1e-9);
    run(m, 6);
    EXPECT_TRUE(hp(m, q) > 5000 + Matriarch::mendRate * 4 && hp(m, q) < 5000 + Matriarch::mendRate * 6.5);
}

/// A bug she rallied runs × 1.3 until the rally ends.
TEST(Matriarch_rallyQuickensABug) {
    auto [s, at] = scene();
    auto walk = [&](bool rallied) {
        GameState t = s;
        const int64_t id = addUnit(t, Unit::Kind::shardling, at, 1);
        Unit& u = t.units.back();
        u.bug = Bug{};
        u.bug->site = at + Vec2(40, 0);
        u.bug->siteRadius = 2;
        u.bug->coming = true;
        if (rallied) u.bug->rallyUntil = 1e9;
        Simulation m = sim(t);
        // The ground it covered, step by step (its way bends round rocks).
        double walked = 0;
        Vec2 was = at;
        for (int k = 0; k < 90; k++) {
            m.step(1.0 / 30);
            walked += distance(m.state.unit(id)->position, was);
            was = m.state.unit(id)->position;
        }
        return walked;
    };
    const double plain = walk(false), quick = walk(true);
    EXPECT_NEAR(quick / plain, Matriarch::rallySpeed, 0.08);
}
