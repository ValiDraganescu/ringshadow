// The walk-in Dropship (C4, docs/haul-and-ferry.md section 3): the bay's
// shape, the ramp and its button, the take-off button's refusals, the riders
// carried and untouchable, the touch down, the empty ship leaving, its pose,
// from hand-built scenes of a few seconds.
#include "Shuttle.h"
#include "HallScene.h"
#include "SimHelpers.h"

#include <cmath>

using namespace ac;
using namespace simtest;

namespace {

constexpr Vec2 kPad{50, 50};

/// Blue's Prospector at `pilotAt` driven by hand, and a Dropship on the pad
/// at `kPad` facing `heading` (its nose), its ramp `open` (0 shut, 1 down);
/// player 1 is the Brood.
struct Bay {
    Simulation sim;
    int64_t ship;
    int64_t pilot;
};

Bay bayScene(Vec2 pilotAt, double open = 1, double heading = 0, std::optional<MapDefinition> map = std::nullopt, Vec2 pad = kPad) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t p = addUnit(s, Unit::Kind::prospector, pilotAt, 0);
    Simulation sim(s, map);
    sim.broodSide = 1;
    EXPECT_TRUE(sim.take(p));
    const int64_t id = sim.callShuttle(pad, heading);
    Shuttle& ship = *sim.shuttle(id);
    ship.stage = Shuttle::Stage::ground;
    ship.ramp = Door{open, open > 0.5};
    sim.step(1.0 / 30);
    return Bay{sim, id, p};
}

Vec2 localOf(Bay& b, int64_t unit) { return ShuttleBay::local(*b.sim.shuttle(b.ship), unitOf(b.sim, unit).position); }

/// Another Prospector of blue's at `p`, on its own.
int64_t addUnitAt(Bay& b, Vec2 p) {
    const int64_t id = b.sim.state.nextID++;
    b.sim.state.units.push_back(Unit(id, Unit::Kind::prospector, 0, p, 0, Unit::Task::idle));
    b.sim.state.units.back().hp = Rules::stats(Unit::Kind::prospector).hp;
    b.sim.reindex();
    return id;
}

/// Walks the pilot along `heading` for `seconds`.
void walk(Bay& b, double heading, double seconds) {
    b.sim.pilot->heading = heading;
    b.sim.pilot->walk = Vec2(1, 0);
    steps(b.sim, seconds);
    b.sim.pilot->walk = Vec2(0, 0);
}

double dist3(Vec3 a, Vec3 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

} // namespace

TEST(shuttle_bayShape) {
    using B = ShuttleBay;
    // 2 wide, 4 long (and the cockpit nook), 2 tall on its deck.
    EXPECT_EQ(B::ceiling - B::deck, 2.0);
    EXPECT_TRUE(B::inBay({-2.4, 0}) && B::inBay({1.5, 0.9}) && B::inBay({2.3, -0.9}));
    EXPECT_TRUE(!B::inBay({-2.6, 0}) && !B::inBay({0, 1.05}) && !B::inBay({2.4, 0}));
    // The ramp lies 2 long down from the deck to the ground behind.
    EXPECT_NEAR(B::rampFoot(), -2.5 - std::sqrt(3.75), 1e-12);
    EXPECT_TRUE(B::onRamp({-3.5, 0}) && !B::onRamp({-4.5, 0}) && !B::onRamp({-3.5, 1.2}));
    EXPECT_EQ(B::floor({0, 0}, 0), B::deck);
    EXPECT_NEAR(B::floor({-2.5 - std::sqrt(3.75) / 2, 0}, 1), 0.25, 1e-9);
    EXPECT_NEAR(B::floor({B::rampFoot() + 1e-9, 0}, 1), 0.0, 1e-6);
    EXPECT_EQ(B::floor({-3.5, 0}, 0.3), 0.0);
    EXPECT_EQ(B::floor({-6, 0}, 1), 0.0);
    // Solid: the walls, the nose, the console, the ramp post, the shut ramp.
    EXPECT_TRUE(B::solidAt({0, 1.5}, 0, true) && B::solidAt({0, -2.0}, 0, true));
    EXPECT_TRUE(B::solidAt({3.5, 0}, 0, true) && B::solidAt({2.5, 0}, 0, true));
    EXPECT_TRUE(B::solidAt({-2.26, -0.95}, 0, true));
    EXPECT_TRUE(B::solidAt({-2.6, 0}, 0, false) && !B::solidAt({-2.6, 0}, 0, true));
    EXPECT_TRUE(!B::solidAt({0, 0}, 0.3, false) && !B::solidAt({-3.5, 0}, 0.3, true));
    // Grown by a walker, the bay's sides close in.
    EXPECT_TRUE(B::solidAt({0, 0.8}, 0.3, true) && !B::solidAt({0, 0.65}, 0.3, true));
    // The cameras' slabs: the bay inside, its ceiling, the shut ramp.
    const Footprint& f = B::footprint();
    ASSERT_TRUE(f.interior && f.roof && f.door);
    EXPECT_EQ(f.interior->z0, B::deck);
    EXPECT_EQ(f.roof->z0, B::ceiling);
    EXPECT_EQ(f.furniture.size(), size_t(2));
    // Its frame turns with its heading.
    Shuttle s;
    s.position = {10, 20};
    s.heading = pi / 2;
    EXPECT_NEAR(B::world(s, {1, 0}).x, 10, 1e-12);
    EXPECT_NEAR(B::world(s, {1, 0}).y, 21, 1e-12);
    const Vec2 back = B::local(s, B::world(s, {-1.5, 0.7}));
    EXPECT_NEAR(back.x, -1.5, 1e-12);
    EXPECT_NEAR(back.y, 0.7, 1e-12);
}

TEST(shuttle_landsClearsThePadAndLowersItsRamp) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t p = addUnit(s, Unit::Kind::prospector, kPad + Vec2(0.5, 0.2), 0);
    Simulation sim(s, std::nullopt);
    const int64_t id = sim.callShuttle(kPad, 0);
    EXPECT_TRUE(sim.shuttle(id)->stage == Shuttle::Stage::descend);
    // Coming down, it touches nobody.
    steps(sim, Ferry::descend - 0.5);
    EXPECT_TRUE(sim.shuttle(id)->stage == Shuttle::Stage::descend);
    EXPECT_TRUE(length(unitOf(sim, p).position - (kPad + Vec2(0.5, 0.2))) < 1e-9);
    steps(sim, 1);
    const Shuttle& ship = *sim.shuttle(id);
    EXPECT_TRUE(ship.grounded());
    // The one under it is out by the nearest side of the hull.
    const Vec2 l = ShuttleBay::local(ship, unitOf(sim, p).position);
    EXPECT_TRUE(!ShuttleBay::covers(l, Rules::radius(Unit::Kind::prospector)) || std::abs(l.y) >= ShuttleBay::hullHalfWide);
    // The ramp comes down over its seconds.
    EXPECT_TRUE(ship.ramp.opening && ship.ramp.open < 0.3);
    steps(sim, ShuttleBay::rampSeconds);
    EXPECT_EQ(sim.shuttle(id)->ramp.open, 1.0);
}

TEST(pilot_walksUpTheRampIntoTheBay) {
    // Behind the ship, facing its nose, the ramp down.
    Bay b = bayScene(kPad + Vec2(-6, 0));
    walk(b, 0, 4);
    Vec2 l = localOf(b, b.pilot);
    EXPECT_TRUE(ShuttleBay::inBay(l));
    // Up to the console's face and no further.
    walk(b, 0, 3);
    l = localOf(b, b.pilot);
    EXPECT_TRUE(l.x < ShuttleBay::front && l.x > ShuttleBay::front - 0.6);
    EXPECT_TRUE(b.sim.aboard(unitOf(b.sim, b.pilot)) != nullptr);
    // Never through a side wall.
    walk(b, pi / 2, 2);
    EXPECT_TRUE(localOf(b, b.pilot).y < ShuttleBay::halfWide);
    // Beside the hull, it walks along it and not in.
    Bay side = bayScene(kPad + Vec2(0, 4));
    walk(side, -pi / 2, 3);
    EXPECT_TRUE(localOf(side, side.pilot).y > ShuttleBay::hullHalfWide);
    // With the ramp shut it stops behind it.
    Bay shut = bayScene(kPad + Vec2(-6, 0), 0);
    walk(shut, 0, 4);
    EXPECT_TRUE(localOf(shut, shut.pilot).x < ShuttleBay::hullRear);
    // A turned ship: the same, in its frame.
    Bay turned = bayScene(kPad + Vec2(0, -6), 1, pi / 2);
    walk(turned, pi / 2, 4);
    EXPECT_TRUE(ShuttleBay::inBay(localOf(turned, turned.pilot)));
}

TEST(shuttle_rampButtonAndItsRefusals) {
    const Vec2 post(ShuttleBay::rampButton.x, ShuttleBay::rampButton.y);
    Bay b = bayScene(kPad + Vec2(-1.6, -0.3));
    // Far from the post in the bay, or outside: refused.
    Bay far = bayScene(kPad + Vec2(1.5, 0));
    EXPECT_EQ(far.sim.ramp(far.pilot, far.ship, false).value_or(""), std::string("Too far from the ramp button"));
    Bay out = bayScene(kPad + Vec2(-3.5, 0));
    EXPECT_EQ(out.sim.ramp(out.pilot, out.ship, false).value_or(""), std::string("Too far from the ramp button"));
    // Someone on the ramp: it won't shut.
    addUnitAt(b, kPad + Vec2(-3.4, 0.2));
    EXPECT_TRUE(length(localOf(b, b.pilot) - post) <= ShuttleBay::reach);
    EXPECT_EQ(b.sim.ramp(b.pilot, b.ship, false).value_or(""), std::string("Someone is on the ramp"));
    // Clear, it shuts over its seconds; on the ground it opens again.
    Bay c = bayScene(kPad + Vec2(-1.6, -0.3));
    EXPECT_TRUE(!c.sim.ramp(c.pilot, c.ship, false));
    steps(c.sim, ShuttleBay::rampSeconds + 0.1);
    EXPECT_EQ(c.sim.shuttle(c.ship)->ramp.open, 0.0);
    EXPECT_TRUE(!c.sim.ramp(c.pilot, c.ship, true));
    steps(c.sim, ShuttleBay::rampSeconds + 0.1);
    EXPECT_EQ(c.sim.shuttle(c.ship)->ramp.open, 1.0);
    // In flight it is shut and stays shut.
    c.sim.shuttle(c.ship)->stage = Shuttle::Stage::dash;
    EXPECT_EQ(c.sim.ramp(c.pilot, c.ship, true).value_or(""), std::string("Wait for the landing"));
}

TEST(shuttle_takeOffButtonAndItsRefusals) {
    const Ferry::Plan plan = Ferry::plan(2);
    // At the console with the ramp down: close it first.
    Bay b = bayScene(kPad + Vec2(1.6, 0));
    const Vec3 to(kPad.x + 200, kPad.y, 0);
    EXPECT_EQ(b.sim.takeOff(b.pilot, b.ship, plan, to, false).value_or(""), std::string("Close the ramp first"));
    // Shutting still: the same.
    b.sim.shuttle(b.ship)->ramp = Door{0.4, false};
    EXPECT_EQ(b.sim.takeOff(b.pilot, b.ship, plan, to, false).value_or(""), std::string("Close the ramp first"));
    // At the back of the bay: too far from the button.
    Bay back = bayScene(kPad + Vec2(-1.5, 0), 0);
    EXPECT_EQ(back.sim.takeOff(back.pilot, back.ship, plan, to, false).value_or(""), std::string("Too far from the take-off button"));
    // Shut, at the console: it lifts, everyone in the bay aboard, nobody else.
    b.sim.shuttle(b.ship)->ramp = Door{0, false};
    const int64_t mate = addUnitAt(b, kPad + Vec2(-1.8, 0.4));
    const int64_t outside = addUnitAt(b, kPad + Vec2(-6, 0));
    EXPECT_TRUE(!b.sim.takeOff(b.pilot, b.ship, plan, to, false));
    const Shuttle& ship = *b.sim.shuttle(b.ship);
    EXPECT_TRUE(ship.stage == Shuttle::Stage::lift);
    EXPECT_TRUE(ship.rides(b.pilot) && ship.rides(mate) && !ship.rides(outside));
    EXPECT_EQ(b.sim.takeOff(b.pilot, b.ship, plan, to, false).value_or(""), std::string("In flight"));
}

TEST(shuttle_ridersWalkTheBayUntouchableInFlight) {
    Bay b = bayScene(kPad + Vec2(1.6, 0), 0);
    const int64_t outside = addUnitAt(b, kPad + Vec2(-7, 0));
    EXPECT_TRUE(!b.sim.takeOff(b.pilot, b.ship, Ferry::plan(3), Vec3(kPad.x, kPad.y + 300, 0), false));
    steps(b.sim, 1);
    const Unit& rider = unitOf(b.sim, b.pilot);
    EXPECT_TRUE(b.sim.sheltered(rider));
    EXPECT_TRUE(!b.sim.sheltered(unitOf(b.sim, outside)));
    // Hits do nothing to it.
    const double hp0 = rider.hp;
    b.sim.hit(b.pilot, 50);
    EXPECT_EQ(unitOf(b.sim, b.pilot).hp, hp0);
    // It walks about the bay: to the back, against the shut ramp.
    walk(b, pi, 3);
    Vec2 l = localOf(b, b.pilot);
    EXPECT_TRUE(ShuttleBay::inBay(l) && l.x < -1.8);
    // Somebody on the pad below walks through where the bay was: it is up in the air.
    b.sim.take(outside);
    walk(b, 0, 3);
    EXPECT_TRUE(localOf(b, outside).x > -4.5);
}

TEST(shuttle_hopCarriesItsRidersAndLandsShut) {
    // A hop on this region's ground, 40 cells along +y.
    Bay b = bayScene(kPad + Vec2(1.6, 0.3), 0);
    const Vec3 to(kPad.x, kPad.y + 40, 0);
    const Ferry::Plan plan = Ferry::plan(0);
    EXPECT_TRUE(!b.sim.takeOff(b.pilot, b.ship, plan, to, true));
    const Vec2 before = localOf(b, b.pilot);
    steps(b.sim, plan.lift + plan.turns + plan.dash + 0.1);
    // The approach: the frame on the target pad, the rider where it stood in it.
    const Shuttle& ship = *b.sim.shuttle(b.ship);
    EXPECT_TRUE(ship.stage == Shuttle::Stage::approach);
    EXPECT_NEAR(ship.position.y, kPad.y + 40, 1e-9);
    EXPECT_NEAR(ship.heading, pi / 2, 1e-9);
    const Vec2 after = localOf(b, b.pilot);
    EXPECT_NEAR(after.x, before.x, 0.05);
    EXPECT_NEAR(after.y, before.y, 0.05);
    steps(b.sim, plan.approach);
    // Down, shut, nobody riding.
    EXPECT_TRUE(b.sim.shuttle(b.ship)->grounded());
    EXPECT_EQ(b.sim.shuttle(b.ship)->ramp.open, 0.0);
    EXPECT_TRUE(b.sim.shuttle(b.ship)->riders.empty());
    EXPECT_TRUE(!b.sim.sheltered(unitOf(b.sim, b.pilot)));
    // Out: to the post, the ramp down, out the back.
    walk(b, pi / 2 + pi, 3);
    ASSERT_TRUE(!b.sim.ramp(b.pilot, b.ship, true));
    steps(b.sim, ShuttleBay::rampSeconds + 0.1);
    walk(b, -pi / 2, 4);
    EXPECT_TRUE(b.sim.aboard(unitOf(b.sim, b.pilot)) == nullptr);
    // Empty for 5 s with the ramp down: it shuts and lifts away, then it is gone.
    steps(b.sim, ShuttleBay::leaveSeconds + 0.1);
    EXPECT_TRUE(!b.sim.shuttle(b.ship)->ramp.opening);
    steps(b.sim, ShuttleBay::rampSeconds + 0.1);
    EXPECT_TRUE(b.sim.shuttle(b.ship)->stage == Shuttle::Stage::depart);
    steps(b.sim, ShuttleBay::departSeconds + 0.1);
    EXPECT_TRUE(b.sim.shuttle(b.ship) == nullptr);
}

TEST(shuttle_calledAndLeftWaitsAMinute) {
    Bay b = bayScene(kPad + Vec2(-8, 0));
    steps(b.sim, ShuttleBay::leaveSeconds + 1);
    EXPECT_TRUE(b.sim.shuttle(b.ship)->ramp.opening);
    // Someone on the ramp as it shuts to leave: it opens again for them.
    steps(b.sim, ShuttleBay::waitSeconds - ShuttleBay::leaveSeconds);
    EXPECT_TRUE(!b.sim.shuttle(b.ship)->ramp.opening);
    walk(b, 0, 1.6);
    EXPECT_TRUE(b.sim.shuttle(b.ship)->ramp.opening);
}

TEST(shuttle_poseRunsSmoothlyPadToPad) {
    Shuttle s;
    s.position = kPad;
    s.heading = 0.4;
    s.from = Vec3(kPad.x, kPad.y, 2);
    s.ground = 2;
    s.fromHeading = 0.4;
    s.to = Vec3(kPad.x + 180, kPad.y - 90, 5);
    s.plan = Ferry::plan(3);
    // Down onto the pad and on it.
    EXPECT_TRUE(dist3(shuttlePose(s, Shuttle::Stage::descend, Ferry::descend).at, s.from) < 1e-9);
    EXPECT_TRUE(dist3(shuttlePose(s, Shuttle::Stage::ground, 7).at, s.from) < 1e-9);
    EXPECT_EQ(shuttlePose(s, Shuttle::Stage::ground, 7).yaw, 0.4);
    // No jump anywhere along the flight, stage to stage.
    const Shuttle::Stage stages[] = {Shuttle::Stage::lift, Shuttle::Stage::turns, Shuttle::Stage::dash, Shuttle::Stage::approach};
    Vec3 last = s.from;
    double most = 0;
    for (const Shuttle::Stage st : stages) {
        const double end = shuttleStageSeconds(s, st);
        for (double t = 0; t <= end + 1e-9; t += 0.02) {
            const Vec3 p = shuttlePose(s, st, t).at;
            most = std::max(most, dist3(p, last));
            last = p;
        }
    }
    EXPECT_TRUE(most < 3.0);
    // It lands on the target pad, its nose along the way.
    const ShuttlePose end = shuttlePose(s, Shuttle::Stage::approach, s.plan.approach);
    EXPECT_TRUE(dist3(end.at, s.to) < 1e-9);
    EXPECT_NEAR(end.yaw, shuttleLandingHeading(s), 1e-9);
    EXPECT_NEAR(shuttleLandingHeading(s), std::atan2(-90.0, 180.0), 1e-9);
    // High in the dash.
    EXPECT_TRUE(shuttlePose(s, Shuttle::Stage::dash, s.plan.dash / 2).at.z > 20);
}

TEST(shuttle_gridKeepsTheAiOutAndThePilotIn) {
    const MapDefinition& map = homeMap();
    const Vec2 base = map.bases.front().center + Vec2(-18, 14);
    Bay b = bayScene(base + Vec2(-1.0, 0), 1, 0, map, base);
    ASSERT_TRUE(b.sim.nav.has_value());
    // The AI's grid has the ship solid; the pilot in its bay stays put.
    steps(b.sim, 0.5);
    EXPECT_TRUE(!b.sim.nav->walkable(base));
    EXPECT_TRUE(NavGrid::solid(base, b.sim.state));
    EXPECT_TRUE(ShuttleBay::inBay(localOf(b, b.pilot)));
    EXPECT_NEAR(localOf(b, b.pilot).x, -1.0, 1e-6);
}
