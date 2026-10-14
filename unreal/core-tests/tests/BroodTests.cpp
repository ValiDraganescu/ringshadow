// The Brood in the core (docs/creatures.md): a bug carries out its orders
// (`Bug`) by itself, from a hand-built scene of a few seconds, never a game.
// The orders' director is the server's (server/tests/BroodTests.cpp).
#include "Brood.h"
#include "SimHelpers.h"

using namespace ac;
using namespace simtest;

namespace {

/// Blue's open ground and nothing on it; player 1 is the Brood.
struct Scene {
    GameState s;
    Vec2 at;
};

Scene scene() {
    GameState s = GameState::new_(homeMap());
    Vec2 at;
    for (const auto& b : s.structures) if (b.owner == 0) { at = b.position + Vec2(0, 8); break; }
    s.units.clear();
    s.structures.clear();
    return Scene{s, at};
}

Simulation sim(const GameState& s) {
    Simulation out(s, homeMap());
    out.broodSide = 1;
    return out;
}

/// A bug of the Brood at `p`, its site there.
int64_t bug(GameState& s, Unit::Kind kind, Vec2 p) {
    const int64_t id = addUnit(s, kind, p, 1);
    Unit& u = s.units.back();
    u.bug = Bug{};
    u.bug->site = p;
    u.bug->coming = false;
    if (kind == Unit::Kind::gloamwing) u.bug->height = Brood::hoverHeight;
    return id;
}

Bug bugOf(const Simulation& sim, int64_t id) { return *sim.state.unit(id)->bug; }

} // namespace

/// A Shardling on a Prospector bites it every 1.2 s for 2, and counts its
/// strikes for the client.
TEST(Brood_shardlingBitesItsPrey) {
    auto [s, at] = scene();
    const int64_t prey = addUnit(s, Unit::Kind::prospector, at + Vec2(2, 0), 0);
    const int64_t b = bug(s, Unit::Kind::shardling, at);
    s.units.back().bug->prey = prey;
    Simulation m = sim(s);
    run(m, 4);
    EXPECT_TRUE(hp(m, prey) < Rules::hp(Unit::Kind::prospector));
    EXPECT_TRUE(hp(m, prey) >= Rules::hp(Unit::Kind::prospector) - 2 * 4);
    EXPECT_TRUE(bugOf(m, b).strikes >= 2);
}

/// Bugs never win a game: a world with a Brood side never ends.
TEST(Brood_noVictoryWithTheBrood) {
    auto [s, at] = scene();
    addStructure(s, Structure::Kind::habDome, at, 0);
    bug(s, Unit::Kind::shardling, at + Vec2(10, 0));
    Simulation m = sim(s);
    run(m, 1);
    EXPECT_TRUE(!m.state.winner && !m.state.endedAt);
}

/// A raid bites a building for 3 less its armour (2).
TEST(Brood_raidBitesABuilding) {
    auto [s, at] = scene();
    const int64_t dome = addStructure(s, Structure::Kind::habDome, at, 0);
    bug(s, Unit::Kind::shardling, at + Vec2(4, 0));
    s.units.back().bug->raid = dome;
    Simulation m = sim(s);
    run(m, 6);
    const auto d = m.state.structure(dome);
    ASSERT_TRUE(d.has_value());
    const double lost = Rules::hp(Structure::Kind::habDome) - d->hp;
    EXPECT_TRUE(lost >= 2 * 3 && lost <= 2 * 6);
}

/// A Gloamwing hovering is in the air (the torch can't reach it); in its
/// dive it is at torch height and counts as ground, then climbs again.
TEST(Brood_gloamwingDivesLowToSpit) {
    auto [s, at] = scene();
    const int64_t prey = addUnit(s, Unit::Kind::prospector, at + Vec2(2.5, 0), 0);
    const int64_t g = bug(s, Unit::Kind::gloamwing, at);
    s.units.back().bug->prey = prey;
    Simulation m = sim(s);
    EXPECT_TRUE(m.state.unit(g)->airborne());
    bool low = false, spat = false;
    for (int k = 0; k < 120; k++) {
        m.step(1.0 / 30);
        const Unit u = *m.state.unit(g);
        if (!u.airborne()) {
            low = true;
            EXPECT_TRUE(u.bug->state == Bug::State::dive);
        }
        if (hp(m, prey) < Rules::hp(Unit::Kind::prospector)) spat = true;
    }
    EXPECT_TRUE(low);
    EXPECT_TRUE(spat);
    EXPECT_NEAR(Rules::hp(Unit::Kind::prospector) - hp(m, prey), 3.0 * double(bugOf(m, g).strikes), 1e-6);
}

/// A Lodeback eats its deposit 1.5 a second into its sac, the ore gone from
/// the deposit, until its sac is full; then it leaves.
TEST(Brood_lodebackEatsThenLeaves) {
    auto [s, at] = scene();
    OreDeposit p;
    p.id = 900;
    p.position = at + Vec2(3, 0);
    p.initial = p.remaining = 500;
    s.patches = {p};
    const int64_t l = bug(s, Unit::Kind::lodeback, at);
    s.units.back().bug->deposit = 0;
    s.units.back().bug->exit = at + Vec2(-30, 0);
    Simulation m = sim(s);
    run(m, 10);
    const Bug b = bugOf(m, l);
    EXPECT_TRUE(b.state == Bug::State::eat);
    EXPECT_TRUE(b.sac > 8 && b.sac < 15);
    EXPECT_EQ(m.state.patches[0].remaining, 500 - static_cast<int64_t>(std::floor(b.sac)));
    run(m, 40);
    const Bug full = bugOf(m, l);
    EXPECT_TRUE(full.leaving);
    EXPECT_NEAR(full.sac, Brood::sacFull, 1e-6);
    EXPECT_EQ(m.state.patches[0].remaining, 500 - 60);
}

/// A Lodeback hit while it eats stops and flees at once, with what it has.
TEST(Brood_lodebackFleesWhenHit) {
    auto [s, at] = scene();
    OreDeposit p;
    p.id = 900;
    p.position = at + Vec2(3, 0);
    p.initial = p.remaining = 500;
    s.patches = {p};
    const int64_t l = bug(s, Unit::Kind::lodeback, at);
    s.units.back().bug->deposit = 0;
    s.units.back().bug->exit = at + Vec2(-30, 0);
    const int64_t pro = addUnit(s, Unit::Kind::prospector, at + Vec2(0, 8), 0);
    Simulation m = sim(s);
    run(m, 5);
    const double had = bugOf(m, l).sac;
    const Unit shooter = *m.state.unit(pro);
    m.hit(l, 4, false, &shooter);
    ASSERT_TRUE(m.bugHits.size() == size_t(1));
    EXPECT_EQ(m.bugHits[0].shooter, pro);
    const Vec2 before = m.state.unit(l)->position;
    run(m, 2);
    const Bug b = bugOf(m, l);
    EXPECT_TRUE(b.state == Bug::State::flee);
    EXPECT_TRUE(b.sac >= had && b.sac < had + 0.5);
    EXPECT_TRUE(distance(m.state.unit(l)->position, at + Vec2(-30, 0)) < distance(before, at + Vec2(-30, 0)) - 4);
}

/// A bug drops its prey past the leash and walks back to its site.
TEST(Brood_leashDropsTheChase) {
    auto [s, at] = scene();
    const int64_t prey = addUnit(s, Unit::Kind::prospector, at + Vec2(16, 0), 0);
    const int64_t b = bug(s, Unit::Kind::shardling, at);
    s.units.back().bug->prey = prey;
    s.units.back().bug->siteRadius = 6;
    s.units.back().bug->leash = 8;
    Simulation m = sim(s);
    run(m, 1);
    EXPECT_TRUE(!bugOf(m, b).prey.has_value());
    run(m, 6);
    EXPECT_TRUE(distance(m.state.unit(b)->position, at) <= 6.5);
}

/// Back in its site and unhit for 8 s, a bug mends to full.
TEST(Brood_mendsInItsSite) {
    auto [s, at] = scene();
    const int64_t b = bug(s, Unit::Kind::shardling, at);
    s.units.back().hp = 5;
    Simulation m = sim(s);
    run(m, 7);
    EXPECT_TRUE(hp(m, b) < Rules::hp(Unit::Kind::shardling));
    run(m, 2);
    EXPECT_NEAR(hp(m, b), Rules::hp(Unit::Kind::shardling), 1e-9);
}
