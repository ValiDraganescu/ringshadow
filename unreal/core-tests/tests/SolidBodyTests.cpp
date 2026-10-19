// Solid bodies (C5, docs/haul-and-ferry.md "Solid bodies"): nothing on the
// ground walks or drives through another. A walking pilot stops at the
// Mule's box and slides along it, still climbs on at the perch and fills at
// the hopper; a driven Mule stops at a Prospector and at another Mule, its
// nose at a wall; walkers and the Brood are solid; one put inside another
// is pushed out; the AI's units are held off the pilot. Hand-built scenes
// of a few seconds, no map under them.
#include "HallScene.h"
#include "Mule.h"
#include "Shuttle.h"
#include "SimHelpers.h"

#include <cmath>

using namespace ac;
using namespace simtest;

namespace {

constexpr Vec2 kField{50, 50};

/// Blue's unit of `kind` at `at` facing `heading`, driven by hand; player 1
/// is the Brood.
struct Field {
    Simulation sim;
    int64_t pilot;
};

Field field(Vec2 at, UnitKind kind = UnitKind::prospector, double heading = 0) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t p = addUnit(s, kind, at, 0, Unit::Task::idle, heading);
    Simulation sim(s, std::nullopt);
    sim.broodSide = 1;
    EXPECT_TRUE(sim.take(p));
    sim.pilot->heading = heading;
    return Field{sim, p};
}

int64_t put(Simulation& sim, UnitKind kind, Vec2 at, double heading = 0, int64_t owner = 0) {
    const int64_t id = addUnit(sim.state, kind, at, owner, Unit::Task::idle, heading);
    sim.reindex();
    return id;
}

/// How far `p` is from Mule `m`'s box (under 0 inside).
double fromBox(const Unit& m, Vec2 p) {
    const Vec2 d = p - m.position;
    const double c = std::cos(m.heading), s = std::sin(m.heading);
    const double qx = std::abs(d.x * c + d.y * s) - Mule::length / 2, qy = std::abs(-d.x * s + d.y * c) - Mule::width / 2;
    if (qx > 0 || qy > 0) return std::hypot(std::max(qx, 0.0), std::max(qy, 0.0));
    return std::max(qx, qy);
}

/// A walker's body radius (its walk's circle).
double bodyR(UnitKind k) { return Rules::radius(k) * 0.8; }

/// Walks (or drives) the pilot ahead along `heading` for `seconds`,
/// calling `each` after every step.
template <typename Each>
void walk(Simulation& sim, double heading, double seconds, Each each) {
    sim.pilot->heading = heading;
    sim.pilot->walk = Vec2(1, 0);
    for (int k = 0; k < int(seconds * 30); k++) {
        sim.step(1.0 / 30);
        each();
    }
    sim.pilot->walk = Vec2(0, 0);
}

void walk(Simulation& sim, double heading, double seconds) { walk(sim, heading, seconds, [] {}); }

} // namespace

/// Square into a parked Mule (turned 0.4) from ahead, behind and either
/// side: never inside, stopped at its surface, its far side unreached.
TEST(solid_walkerStopsAtTheMulesSurfaceFromEverySide) {
    const double h = 0.4;
    const Vec2 ahead(std::cos(h), std::sin(h)), left(-std::sin(h), std::cos(h));
    for (const Vec2 from : {ahead * 4, -ahead * 4, left * 3, -left * 3}) {
        Field f = field(kField + from);
        const int64_t mule = put(f.sim, UnitKind::mule, kField, h);
        const Vec2 in = -from / length(from);
        double nearest = 1e9;
        walk(f.sim, std::atan2(in.y, in.x), 3, [&] { nearest = std::min(nearest, fromBox(unitOf(f.sim, mule), unitOf(f.sim, f.pilot).position)); });
        const Unit& u = unitOf(f.sim, f.pilot);
        EXPECT_TRUE(nearest >= bodyR(UnitKind::prospector) - 1e-6);
        EXPECT_TRUE(fromBox(unitOf(f.sim, mule), u.position) < bodyR(UnitKind::prospector) + 0.15);
        // Still on its own side, the Mule unmoved.
        EXPECT_TRUE(dot(u.position - kField, from) > 0);
        EXPECT_EQ(unitOf(f.sim, mule).position, kField);
    }
}

/// Walking aslant into the Mule's long side: it slides along it, never in
/// (and, past its end, on round it).
TEST(solid_walkerSlidesAlongTheMule) {
    Field f = field(kField + Vec2(-2.5, 2.0));
    const int64_t mule = put(f.sim, UnitKind::mule, kField, 0);
    double nearest = 1e9;
    walk(f.sim, -pi / 4, 1.2, [&] { nearest = std::min(nearest, fromBox(unitOf(f.sim, mule), unitOf(f.sim, f.pilot).position)); });
    const Unit& u = unitOf(f.sim, f.pilot);
    EXPECT_TRUE(nearest >= bodyR(UnitKind::prospector) - 1e-6);
    // Along the side, past its middle, still above it.
    EXPECT_TRUE(u.position.x > kField.x + 0.5);
    EXPECT_NEAR(u.position.y, kField.y + Mule::width / 2 + bodyR(UnitKind::prospector), 0.05);
}

/// From beside the Mule, stopped at its flank: E at the perch climbs on, E at
/// the hopper fills it; off again, beside it, it walks away.
TEST(solid_climbOnAndFillFromBeside) {
    Field f = field(kField + Vec2(Mule::perchLocal.x, 3));
    const int64_t mule = put(f.sim, UnitKind::mule, kField, 0);
    walk(f.sim, -pi / 2, 2);
    EXPECT_TRUE(fromBox(unitOf(f.sim, mule), unitOf(f.sim, f.pilot).position) >= bodyR(UnitKind::prospector) - 1e-6);
    EXPECT_TRUE(!f.sim.muleBoardRefusal(unitOf(f.sim, f.pilot), unitOf(f.sim, mule)));
    // At the hopper, from its flank.
    unitOf(f.sim, f.pilot).position = kField + Vec2(Mule::hopperLocal.x, 3);
    walk(f.sim, -pi / 2, 2);
    unitOf(f.sim, f.pilot).carrying = 10;
    EXPECT_TRUE(!f.sim.muleFill(f.pilot, mule));
    EXPECT_EQ(unitOf(f.sim, mule).haul->ore, int64_t(10));
    // On at the perch, driven, rider on it; off, beside it, on foot.
    unitOf(f.sim, f.pilot).position = kField + Vec2(Mule::perchLocal.x, 1.2);
    EXPECT_TRUE(!f.sim.muleBoard(f.pilot, mule));
    EXPECT_TRUE(f.sim.take(mule));
    walk(f.sim, 0, 1);
    EXPECT_TRUE(unitOf(f.sim, mule).position.x > kField.x + 0.5);
    EXPECT_EQ(unitOf(f.sim, f.pilot).position, unitOf(f.sim, mule).position);
    EXPECT_TRUE(f.sim.take(f.pilot));
    const Vec2 off = unitOf(f.sim, f.pilot).position;
    EXPECT_TRUE(fromBox(unitOf(f.sim, mule), off) >= bodyR(UnitKind::prospector) - 1e-6);
    walk(f.sim, pi / 2, 1);
    EXPECT_TRUE(distance(unitOf(f.sim, f.pilot).position, off) > 2);
}

/// A driven Mule stops at a Prospector in its way, and at another Mule;
/// neither is pushed.
TEST(solid_drivenMuleStopsAtAProspectorAndAMule) {
    Field f = field(kField, UnitKind::mule, 0);
    const Vec2 there = kField + Vec2(4, 0.3);
    const int64_t walker = put(f.sim, UnitKind::prospector, there);
    walk(f.sim, 0, 4);
    EXPECT_TRUE(fromBox(unitOf(f.sim, f.pilot), there) >= bodyR(UnitKind::prospector) - 1e-6);
    EXPECT_TRUE(fromBox(unitOf(f.sim, f.pilot), there) < bodyR(UnitKind::prospector) + 0.1);
    EXPECT_EQ(unitOf(f.sim, walker).position, there);
    // Another Mule across its way, its flank to the first's nose.
    Field g = field(kField, UnitKind::mule, 0);
    const Vec2 parked = kField + Vec2(4, 0.5);
    const int64_t other = put(g.sim, UnitKind::mule, parked, pi / 2);
    walk(g.sim, 0, 4);
    const Unit& m = unitOf(g.sim, g.pilot);
    // Its nose against the other's flank: the other's half width on, no
    // nearer.
    const double gap = (parked.x - Mule::width / 2) - (m.position.x + Mule::length / 2);
    EXPECT_TRUE(gap >= -1e-6 && gap < 0.1);
    EXPECT_EQ(unitOf(g.sim, other).position, parked);
}

/// A driven Firefly stops at a Prospector; a walking Prospector never walks
/// into a Ranger (walkers are solid to each other) or a Shardling (the
/// Brood is): it slides round them.
TEST(solid_vehiclesWalkersAndTheBroodBlock) {
    Field f = field(kField, UnitKind::firefly, 0);
    const Vec2 there = kField + Vec2(4, 0);
    put(f.sim, UnitKind::prospector, there);
    walk(f.sim, 0, 3);
    const double touch = bodyR(UnitKind::firefly) + bodyR(UnitKind::prospector);
    EXPECT_TRUE(distance(unitOf(f.sim, f.pilot).position, there) >= touch - 1e-6);
    EXPECT_TRUE(distance(unitOf(f.sim, f.pilot).position, there) < touch + 0.25);
    for (const UnitKind k : {UnitKind::ranger, UnitKind::shardling}) {
        Field g = field(kField);
        const int64_t v = k == UnitKind::shardling ? bugAt(g.sim, k, there) : put(g.sim, k, there);
        const double t = bodyR(UnitKind::prospector) + bodyR(k);
        double nearest = 1e9;
        walk(g.sim, 0, 3, [&] { nearest = std::min(nearest, distance(unitOf(g.sim, g.pilot).position, unitOf(g.sim, v).position)); });
        EXPECT_TRUE(nearest >= t - 1e-6);
        EXPECT_TRUE(nearest < t + 0.2);
    }
}

/// No bodies: a flyer over it, a buried Scorpion under it, a Mule's rider
/// on it (the Mule is one).
TEST(solid_flyersBuriedAndRidersAreNoBodies) {
    Field f = field(kField);
    // (Looked up each time: adding units moves them.)
    auto u = [&]() -> const Unit& { return unitOf(f.sim, f.pilot); };
    const int64_t kestrel = put(f.sim, UnitKind::kestrel, kField + Vec2(2, 0));
    const int64_t scorpion = put(f.sim, UnitKind::scorpion, kField + Vec2(4, 0));
    unitOf(f.sim, scorpion).anchor = 1;
    unitOf(f.sim, scorpion).anchored = true;
    EXPECT_TRUE(!f.sim.bodyBlocks(u(), unitOf(f.sim, kestrel)));
    EXPECT_TRUE(!f.sim.bodyBlocks(u(), unitOf(f.sim, scorpion)));
    EXPECT_TRUE(f.sim.pilotCanStand(u(), unitOf(f.sim, kestrel).position));
    EXPECT_TRUE(f.sim.pilotCanStand(u(), unitOf(f.sim, scorpion).position));
    const int64_t mule = put(f.sim, UnitKind::mule, kField + Vec2(0, 6), 0);
    const int64_t rider = put(f.sim, UnitKind::prospector, Mule::perch(unitOf(f.sim, mule)) + Vec2(0, 1.2));
    EXPECT_TRUE(!f.sim.muleBoard(rider, mule));
    EXPECT_TRUE(!f.sim.bodyBlocks(u(), unitOf(f.sim, rider)));
    EXPECT_TRUE(f.sim.bodyBlocks(u(), unitOf(f.sim, mule)));
}

/// Put inside a parked Mule (spawned, a Mule rolled out onto it), a
/// walker is pushed out of it, standing still, and then walks freely; a
/// Mule driven while inside another drives out of it.
TEST(solid_pushedOutNeverStuck) {
    Field f = field(kField + Vec2(0.4, 0.2));
    const int64_t mule = put(f.sim, UnitKind::mule, kField, 0);
    steps(f.sim, 1);
    EXPECT_TRUE(fromBox(unitOf(f.sim, mule), unitOf(f.sim, f.pilot).position) >= bodyR(UnitKind::prospector) - 1e-6);
    const Vec2 out = unitOf(f.sim, f.pilot).position;
    walk(f.sim, std::atan2(out.y - kField.y, out.x - kField.x), 1);
    EXPECT_TRUE(distance(unitOf(f.sim, f.pilot).position, out) > 2);
    // A Mule rolled out of a Foundry's door onto the pilot.
    Field g = field(Mule::door(kField + Vec2(0, -10)));
    const int64_t rolled = g.sim.rollOutAt(Mule::door(kField + Vec2(0, -10)), 0);
    steps(g.sim, 1);
    EXPECT_TRUE(fromBox(unitOf(g.sim, rolled), unitOf(g.sim, g.pilot).position) >= bodyR(UnitKind::prospector) - 1e-6);
    // A driven Mule half in a parked one drives out, and away.
    Field d = field(kField + Vec2(1.0, 0.3), UnitKind::mule, 0);
    const int64_t parked = put(d.sim, UnitKind::mule, kField, 0);
    walk(d.sim, 0, 3);
    EXPECT_TRUE(unitOf(d.sim, d.pilot).position.x > kField.x + Mule::length + 1);
    EXPECT_EQ(unitOf(d.sim, parked).position, kField);
}

/// A driven Mule's nose stops at a building's wall (its middle 1.5 back).
TEST(solid_mulesNoseStopsAtAWall) {
    Field f = field(kField, UnitKind::mule, 0);
    const Vec2 at = kField + Vec2(8, 0);
    addStructure(f.sim.state, StructureKind::foundry, at, 0);
    walk(f.sim, 0, 8);
    const double wall = at.x - Rules::radius(StructureKind::foundry);
    const double nose = unitOf(f.sim, f.pilot).position.x + Mule::length / 2;
    EXPECT_TRUE(nose <= wall + 1e-6);
    EXPECT_TRUE(nose > wall - 0.3);
}

/// An AI Ranger sent through where the pilot stands is held off it every
/// step; a bug set down in a Ranger steps apart from it, two bugs crawl over
/// one another.
TEST(solid_aiHeldOffThePilotAndTheBroodApart) {
    Field f = field(kField);
    const int64_t ranger = put(f.sim, UnitKind::ranger, kField + Vec2(-4, 0.05));
    unitOf(f.sim, ranger).goal = kField + Vec2(4, 0);
    unitOf(f.sim, ranger).task = Unit::Task::toRally;
    const double touch = bodyR(UnitKind::prospector) + bodyR(UnitKind::ranger);
    double nearest = 1e9;
    for (int k = 0; k < 90; k++) {
        f.sim.step(1.0 / 30);
        nearest = std::min(nearest, distance(unitOf(f.sim, ranger).position, unitOf(f.sim, f.pilot).position));
    }
    EXPECT_TRUE(nearest >= touch - 1e-6);
    EXPECT_TRUE(nearest < touch + 0.3);
    EXPECT_EQ(unitOf(f.sim, f.pilot).position, kField);
    const int64_t a = bugAt(f.sim, UnitKind::shardling, kField + Vec2(0, 8));
    const int64_t b = bugAt(f.sim, UnitKind::shardling, kField + Vec2(0.1, 8));
    const int64_t c = put(f.sim, UnitKind::ranger, kField + Vec2(0.05, 12), 0, 0);
    const int64_t d = bugAt(f.sim, UnitKind::shardling, kField + Vec2(0, 12));
    steps(f.sim, 1);
    EXPECT_NEAR(distance(unitOf(f.sim, a).position, unitOf(f.sim, b).position), 0.1, 1e-9);
    EXPECT_TRUE(distance(unitOf(f.sim, c).position, unitOf(f.sim, d).position) > 0.5);
}

/// A landed Dropship's hull is solid from outside: a walker into its flank
/// stops outside it.
TEST(solid_dropshipHullFromOutside) {
    Field f = field(kField + Vec2(0, 5));
    const int64_t id = f.sim.callShuttle(kField, 0);
    f.sim.shuttle(id)->stage = Shuttle::Stage::ground;
    f.sim.shuttle(id)->ramp = Door{0, false};
    walk(f.sim, -pi / 2, 3);
    const Vec2 l = ShuttleBay::local(*f.sim.shuttle(id), unitOf(f.sim, f.pilot).position);
    EXPECT_TRUE(l.y > ShuttleBay::hullHalfWide);
    EXPECT_TRUE(!ShuttleBay::inBay(l));
}

/// In a world the AI's Prospectors (the frontier pilots') step apart as
/// soldiers do; the strategy game's mine through one another.
TEST(solid_worldProspectorsStepApart) {
    for (const bool world : {true, false}) {
        Field f = field(kField + Vec2(0, 10));
        if (world) f.sim.xpScale = 0.5;
        const int64_t a = put(f.sim, UnitKind::prospector, kField);
        const int64_t b = put(f.sim, UnitKind::prospector, kField + Vec2(0.1, 0));
        steps(f.sim, 1);
        const double apart = distance(unitOf(f.sim, a).position, unitOf(f.sim, b).position);
        if (world) EXPECT_TRUE(apart > 0.5);
        else EXPECT_NEAR(apart, 0.1, 1e-9);
    }
}
