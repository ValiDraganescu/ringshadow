// The realm's defences in the core (D1, D2; Defence.h): an armed Citadel's
// corner turrets, the AA tower (the Sentinel) and the gatling gun against
// bugs held still, from hand-built scenes of a few seconds.
#include "Brood.h"
#include "Defence.h"
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

/// A bug of the Brood at `p` that stays where it is (it never picks a spot
/// to wander to): only the defences act. A Gloamwing hovers (air).
int64_t bug(GameState& s, Unit::Kind kind, Vec2 p) {
    const int64_t id = addUnit(s, kind, p, 1);
    Unit& u = s.units.back();
    u.bug = Bug{};
    u.bug->site = p;
    u.bug->coming = false;
    u.bug->wanderAt = 1e9;
    if (kind == Unit::Kind::gloamwing) u.bug->height = Brood::hoverHeight;
    return id;
}

bool hurt(const Simulation& m, int64_t id) {
    const auto u = m.state.unit(id);
    return !u || u->hp < Rules::hp(u->kind);
}

} // namespace

/// C1: an armed Citadel's four corner turrets never fire by themselves, at
/// a Shardling or a Gloamwing in reach, with the hall (the MMO's) or
/// without (the strategy game's): only a gunner fires them
/// (CitadelTests, gun_*).
TEST(Defence_citadelTurretsNeverFireAlone) {
    for (const bool hall : {true, false}) {
        const CitadelHall h(hall);
        auto [s, at] = scene();
        const int64_t c = addStructure(s, Structure::Kind::citadel, at, 0);
        s.structures.back().marks = TurretMarks{.rate = 3, .damage = 3, .range = 3};
        const double r = Rules::radius(StructureKind::citadel);
        const int64_t ground = bug(s, Unit::Kind::shardling, at + Vec2(r + 3, 0));
        const int64_t air = bug(s, Unit::Kind::gloamwing, at + Vec2(-r - 3, 0));
        Simulation m = sim(s);
        const auto events = run(m, 4);
        EXPECT_EQ(shots(events, c), int64_t(0));
        EXPECT_TRUE(!hurt(m, ground));
        EXPECT_TRUE(!hurt(m, air));
        const std::optional<Structure> citadel = m.state.structure(c);
        ASSERT_TRUE(citadel->guns.has_value());
        for (const CornerGun& g : *citadel->guns) EXPECT_TRUE(!g.target && !g.gunner);
        EXPECT_TRUE(m.bugHits.empty());
    }
}

/// A manned corner gun's reach is its rounds' (C2, docs/guns-and-practice.md
/// section 1): with no marks it hits a bug 35 cells out, far past the old
/// 7.5 + 3 marks, on the home map's ground; its range marks are nothing to it.
TEST(Defence_citadelGunsReachWithoutRangeMarks) {
    const CitadelHall hall(true);
    auto [s, at] = scene();
    const int64_t c = addStructure(s, Structure::Kind::citadel, at, 0);
    s.structures.back().marks = TurretMarks{};
    const int64_t pilot = addUnit(s, Unit::Kind::prospector, Defence::gunStation(at, 0), 0);
    const Vec2 mount = Defence::cornerGun(at, 0);
    const double rest = Defence::cornerAngle(0);
    const Vec2 out(std::cos(rest), std::sin(rest));
    const Vec2 spot = mount + out * 35;
    const int64_t far = bug(s, Unit::Kind::lodeback, spot);
    // No fog: the bug stands past the side's sight (a world region by day
    // sees it, `daylightAt`).
    Simulation m(s, homeMap(), false);
    m.broodSide = 1;
    ASSERT_TRUE(m.take(pilot));
    m.step(1.0 / 30);
    ASSERT_TRUE(!m.man(pilot, c, 0));
    // The pitch that lands on it, from the muzzle over this ground.
    const double base = m.field->height(at), aim = m.field->height(spot) + 0.75;
    double pitch = 0;
    for (int k = 0; k < 4; k++) {
        const Vec3 q = Defence::gunMuzzle(at, 0, rest, pitch);
        pitch = Rules::ballisticPitch(distance(Vec2(q.x, q.y), spot), aim - (base + q.z)).value_or(0);
    }
    m.pilot->heading = rest;
    m.pilot->pitch = pitch;
    m.pilot->hold = true;
    m.pilot->crosshair = Pilot::Crosshair{Pilot::Crosshair::On{far}};
    run(m, 1.5);
    EXPECT_TRUE(hurt(m, far));
}

/// The AA tower (the Sentinel) hits only the air: a hovering Gloamwing, not
/// a Shardling. (A Gloamwing low in its dive is a ground target, N2's
/// `Unit::airborne`.)
TEST(Defence_sentinelHitsOnlyAir) {
    auto [s, at] = scene();
    addStructure(s, Structure::Kind::sentinel, at, 0);
    const int64_t air = bug(s, Unit::Kind::gloamwing, at + Vec2(6, 0));
    const int64_t ground = bug(s, Unit::Kind::shardling, at + Vec2(-4, 0));
    Simulation m = sim(s);
    run(m, 3);
    EXPECT_TRUE(hurt(m, air));
    EXPECT_TRUE(!hurt(m, ground));
    // Two volleys kill a Gloamwing.
    EXPECT_TRUE(!m.state.unit(air) || m.state.unit(air)->hp <= 0);

}

/// The gatling gun hits the ground in reach (20 since C2), and the air only within 4.5:
/// a Gloamwing at 5.5 from its edge is safe, one at 3.5 is shot.
TEST(Defence_gatlingGroundAndCloseAir) {
    // Ranges are from its edge (C5: the 5x5 gun's 2.5 from its centre).
    const double edge = Rules::radius(Structure::Kind::gatling);
    auto [s, at] = scene();
    addStructure(s, Structure::Kind::gatling, at, 0);
    const int64_t ground = bug(s, Unit::Kind::shardling, at + Vec2(edge + 6, 0));
    const int64_t high = bug(s, Unit::Kind::gloamwing, at + Vec2(0, edge + 6));
    Simulation m = sim(s);
    run(m, 3);
    EXPECT_TRUE(!m.state.unit(ground) || m.state.unit(ground)->hp <= 0);
    EXPECT_TRUE(!hurt(m, high));

    auto [s2, at2] = scene();
    addStructure(s2, Structure::Kind::gatling, at2, 0);
    const int64_t close = bug(s2, Unit::Kind::gloamwing, at2 + Vec2(0, edge + 4));
    Simulation m2 = sim(s2);
    run(m2, 1);
    EXPECT_TRUE(hurt(m2, close));
}

/// Marks: each adds a fifth to the fire rate, a quarter to the damage and a
/// cell to the range; a gatling gun's air marks half a cell each to its air
/// reach (4.5 to 6, short of the AA tower's 7), which range marks leave be.
/// (C2: the gatling a minigun, 25 rounds a second of 0.8, its AI's reach 20.)
TEST(Defence_marksApply) {
    const UnitStats w = Defence::apply(UnitStats::gatling, TurretMarks{.rate = 1, .damage = 2, .range = 3});
    EXPECT_NEAR(w.cooldown, (1.0 / 25) / 1.2, 1e-9);
    EXPECT_NEAR(w.damage, 0.8 * 1.5, 1e-9);
    EXPECT_NEAR(w.range, 23, 1e-9);
    EXPECT_NEAR(w.airRange, 4.5, 1e-9);
    const UnitStats air = Defence::apply(UnitStats::gatling, TurretMarks{.air = 3});
    EXPECT_NEAR(air.airRange, 6, 1e-9);
    EXPECT_TRUE(air.airRange < UnitStats::sentinel.range);
    EXPECT_NEAR(air.range, 20, 1e-9);
    // The AA tower's range marks are its air reach.
    const UnitStats aa = Defence::apply(UnitStats::sentinel, TurretMarks{.rate = 3, .range = 2});
    EXPECT_NEAR(aa.range, 9, 1e-9);
    EXPECT_NEAR(aa.cooldown, UnitStats::sentinel.cooldown / 1.6, 1e-9);
    // A placed turret's weapon carries its marks.
    Structure g(3, StructureKind::gatling, 0, Vec2(0, 0));
    EXPECT_NEAR(Defence::weapon(g)->damage, 0.8, 1e-9);
    g.marks = TurretMarks{.damage = 3};
    EXPECT_NEAR(Defence::weapon(g)->damage, 0.8 * 1.75, 1e-9);
    Structure c(1, StructureKind::citadel, 0, Vec2(0, 0));
    EXPECT_TRUE(!Defence::weapon(c).has_value());
    c.marks = TurretMarks{};
    EXPECT_TRUE(Defence::weapon(c).has_value());
    EXPECT_TRUE(Defence::weapon(Structure(2, StructureKind::habDome, 0, Vec2(0, 0))) == std::nullopt);
}

/// The Citadel's marks from the realm wallet: rising prices, a refusal when
/// it is short and one at the last mark.
TEST(Defence_citadelPricesAndRefusals) {
    EXPECT_TRUE(Defence::citadelPrice(DefenceStat::rate, 1) == (ShopPrice{250, 0}));
    EXPECT_TRUE(Defence::citadelPrice(DefenceStat::damage, 2) == (ShopPrice{500, 50}));
    EXPECT_TRUE(Defence::citadelPrice(DefenceStat::range, 3) == (ShopPrice{1000, 150}));
    EXPECT_TRUE(!Defence::citadelRefusal(DefenceStat::rate, TurretMarks{}, 250, 0));
    const auto shortOre = Defence::citadelRefusal(DefenceStat::rate, TurretMarks{}, 249, 0);
    ASSERT_TRUE(shortOre.has_value());
    EXPECT_TRUE(shortOre->find("realm wallet is short") != std::string::npos);
    EXPECT_TRUE(Defence::citadelRefusal(DefenceStat::damage, TurretMarks{.damage = 1}, 999, 10).has_value());
    const auto last = Defence::citadelRefusal(DefenceStat::range, TurretMarks{.range = 3}, 99999, 9999);
    ASSERT_TRUE(last.has_value());
    EXPECT_TRUE(last->find("last mark") != std::string::npos);
}

/// A placed turret's marks from the pilot's wallet (D3): the gatling gun's
/// table, half as much again for the AA tower, air reach the gatling gun's
/// alone; refusals when the wallet is short and at the last mark.
TEST(Defence_turretPricesAndRefusals) {
    using K = StructureKind;
    EXPECT_TRUE(Defence::turretPrice(K::gatling, DefenceStat::rate, 1) == (ShopPrice{100, 50}));
    EXPECT_TRUE(Defence::turretPrice(K::gatling, DefenceStat::air, 2) == (ShopPrice{300, 200}));
    EXPECT_TRUE(Defence::turretPrice(K::gatling, DefenceStat::damage, 3) == (ShopPrice{800, 500}));
    EXPECT_TRUE(Defence::turretPrice(K::sentinel, DefenceStat::range, 1) == (ShopPrice{150, 75}));
    EXPECT_TRUE(Defence::turretPrice(K::sentinel, DefenceStat::range, 3) == (ShopPrice{1200, 750}));
    EXPECT_TRUE(Defence::turretPrice(K::sentinel, DefenceStat::air, 1) == ShopPrice{});
    EXPECT_TRUE(Defence::turretPrice(K::habDome, DefenceStat::rate, 1) == ShopPrice{});
    // A whole gatling gun: four stats of three marks, 3,000 MH.
    int64_t mh = 0;
    for (const DefenceStat st : allCases<DefenceStat>())
        for (int64_t k = 1; k <= Defence::marksMax; k++) mh += Defence::turretPrice(K::gatling, st, k).hydrogen;
    EXPECT_EQ(mh, 3000);

    EXPECT_TRUE(!Defence::turretRefusal(K::gatling, DefenceStat::air, TurretMarks{}, 100, 50));
    const auto ore = Defence::turretRefusal(K::gatling, DefenceStat::rate, TurretMarks{}, 99, 50);
    ASSERT_TRUE(ore.has_value());
    EXPECT_TRUE(*ore == "Your wallet is short: 100 ore needed, you have 99");
    const auto h = Defence::turretRefusal(K::sentinel, DefenceStat::damage, TurretMarks{.damage = 1}, 9999, 299);
    ASSERT_TRUE(h.has_value());
    EXPECT_TRUE(*h == "Your wallet is short: 300 MH needed, you have 299");
    const auto last = Defence::turretRefusal(K::gatling, DefenceStat::air, TurretMarks{.air = 3}, 99999, 9999);
    ASSERT_TRUE(last.has_value());
    EXPECT_TRUE(*last == "Air reach is at its last mark");
    EXPECT_TRUE(Defence::turretRefusal(K::sentinel, DefenceStat::air, TurretMarks{}, 99999, 9999).has_value());
    EXPECT_TRUE(Defence::turretRefusal(K::habDome, DefenceStat::rate, TurretMarks{}, 99999, 9999).has_value());
    EXPECT_TRUE(Defence::citadelRefusal(DefenceStat::air, TurretMarks{}, 99999, 9999).has_value());
}

/// A gatling gun with its air marks reaches a Gloamwing it couldn't before:
/// one 5.5 cells from its edge.
TEST(Defence_gatlingAirMarksReach) {
    auto [s, at] = scene();
    addStructure(s, Structure::Kind::gatling, at, 0);
    const double edge = Rules::radius(StructureKind::gatling);
    const int64_t fly = bug(s, Unit::Kind::gloamwing, at + Vec2(0, edge + 5.5 + Rules::radius(UnitKind::gloamwing)));
    Simulation m = sim(s);
    run(m, 2);
    EXPECT_TRUE(!hurt(m, fly));
    m.state.structures.back().marks = TurretMarks{.air = 3};
    run(m, 2);
    EXPECT_TRUE(hurt(m, fly));
}

/// The hp a card writes (Rules::hpShown): the snapshot's quantised hp can
/// come a hair over the whole (300.07 of 300): never shown past it, never 0
/// while any is left.
TEST(Defence_hpShown) {
    EXPECT_EQ(Rules::hpShown(300.07, 300), int64_t(300));
    EXPECT_EQ(Rules::hpShown(300.6, 300), int64_t(300));
    EXPECT_EQ(Rules::hpShown(255.4, 300), int64_t(255));
    EXPECT_EQ(Rules::hpShown(0.2, 300), int64_t(1));
    EXPECT_EQ(Rules::hpShown(0, 300), int64_t(0));
    EXPECT_EQ(Rules::hpShown(-3, 300), int64_t(0));
}
