// The walk-in Dropship on the server (C4, docs/haul-and-ferry.md section 3;
// WorldShuttle.cpp): two pilots ride one ship and the one who presses
// take-off pays; the ramp is refused while someone stands on it; a rider
// builds nothing in flight; the World message says where the ship flies.
// Isolated scenes of a few seconds on The First Reach; run from
// unreal/server.

#include "test.h"

#include "Mule.h"
#include "ShuttleBoard.h"
#include "WorldMap.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;
using namespace shuttletest;

constexpr PilotId a = 131;
constexpr PilotId b = 132;
const std::string keyA = "c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1c1";
const std::string keyB = "d2d2d2d2d2d2d2d2d2d2d2d2d2d2d2d2";

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
    p.startHydrogen = 100;
    p.devSeen = {"cold-fork"};
    return p;
}

/// A and B seated in the start region, a Landing Pad there and one in Cold
/// Fork; A on the start one's centre. The Cold Fork pad's ground point.
Vec2 rig(WorldMap& w) {
    EXPECT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    w.step();
    EXPECT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    const size_t fork = *w.regionNamed("cold-fork");
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    EXPECT_TRUE(w.devLandingPad(first().start, pad + Vec2(0, 10)).has_value());
    EXPECT_TRUE(w.devLandingPad(fork, first().regions[fork].built.pads[0].at + Vec2(0, 10)).has_value());
    Vec2 here{0, 0}, there{0, 0};
    for (const LandingPad& o : w.landingPads()) (o.region == fork ? there : here) = o.at;
    drivenBy(w, a)->position = here;
    drivenBy(w, b)->position = here + Vec2(-9, 0);
    w.step();
    return there;
}

} // namespace

/// Two ride one ship: A calls it and shuts the ramp, B presses take-off
/// and pays; both are carried into Cold Fork where they stood in the bay,
/// both told, both land; they walk out and the empty ship leaves.
TEST(shuttle_two_pilots_ride_one_ship) {
    WorldMap w(first(), params());
    const Vec2 there = rig(w);
    ASSERT_TRUE(!w.callShuttle(a));
    run(w, Ferry::descend + ShuttleBay::rampSeconds + 0.2);
    standIn(w, a, kAtRampButton);
    standIn(w, b, Vec2(-0.5, 0.5));
    ASSERT_TRUE(!w.shuttleRamp(a, false));
    run(w, ShuttleBay::rampSeconds + 0.2);
    standIn(w, b, kAtConsole);
    // A is too far from the take-off button; B at it presses and pays.
    EXPECT_EQ(w.fly(a, "cold-fork", "Landing Pad 1").value_or(""), std::string("Too far from the take-off button"));
    ASSERT_TRUE(!w.fly(b, "cold-fork", "Landing Pad 1"));
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(100));
    EXPECT_EQ(w.wallet(b)->hydrogen, int64_t(85));
    EXPECT_TRUE(w.riding(a) && w.riding(b));
    // Either may walk the bay meanwhile.
    const auto ship = w.shuttleOf(a);
    ASSERT_TRUE(ship && w.shuttleOf(b) == ship);
    const Shuttle* s = w.region(ship->first).simulation().shuttle(ship->second);
    ASSERT_TRUE(s != nullptr);
    const Vec2 la = ShuttleBay::local(*s, drivenBy(w, a)->position), lb = ShuttleBay::local(*s, drivenBy(w, b)->position);
    const Ferry::Plan plan = s->plan;
    w.takeMoves();
    run(w, plan.lift + plan.turns + plan.dash + 0.2);
    const size_t fork = *w.regionNamed("cold-fork");
    ASSERT_TRUE(*w.where(a) == fork && *w.where(b) == fork);
    int told = 0;
    for (const Moved& m : w.takeMoves()) told += m.change && m.change->regionId == "cold-fork" ? 1 : 0;
    EXPECT_EQ(told, 2);
    const auto now = w.shuttleOf(a);
    ASSERT_TRUE(now && now->first == fork && w.shuttleOf(b) == now);
    const Shuttle& t = *w.region(fork).simulation().shuttle(now->second);
    EXPECT_TRUE(distance(ShuttleBay::local(t, drivenBy(w, a)->position), la) < 0.05);
    EXPECT_TRUE(distance(ShuttleBay::local(t, drivenBy(w, b)->position), lb) < 0.05);
    EXPECT_TRUE(distance(t.position, there) < 1e-6);
    run(w, plan.approach + 0.2);
    EXPECT_TRUE(!w.flying(a) && !w.flying(b));
    EXPECT_TRUE(w.progress(a)->pads.count("cold-fork/Landing Pad 1") == 1);
    EXPECT_TRUE(w.progress(b)->pads.count("cold-fork/Landing Pad 1") == 1);
    // Out, both, and the ship lifts away empty.
    ASSERT_TRUE(!walkOut(w, a));
    standIn(w, b, Vec2(ShuttleBay::rampFoot() - 2.5, 1.5));
    EXPECT_TRUE(!w.shuttleOf(a) && !w.shuttleOf(b));
    run(w, ShuttleBay::leaveSeconds + ShuttleBay::rampSeconds + ShuttleBay::departSeconds + 0.5);
    EXPECT_TRUE(w.region(fork).simulation().state.shuttles.empty());
}

/// The ramp won't shut on someone standing on it; in flight a rider builds
/// nothing; the World message says where the ship flies until it is down.
TEST(shuttle_ramp_build_and_world_message) {
    WorldMap w(first(), params());
    rig(w);
    ASSERT_TRUE(!w.callShuttle(a));
    run(w, Ferry::descend + ShuttleBay::rampSeconds + 0.2);
    standIn(w, a, kAtRampButton);
    standIn(w, b, Vec2(-3.6, 0.2));
    EXPECT_EQ(w.shuttleRamp(a, false).value_or(""), std::string("Someone is on the ramp"));
    EXPECT_EQ(w.shuttleRamp(b, false).value_or(""), std::string("Too far from the ramp button"));
    standIn(w, b, Vec2(-9, 0));
    ASSERT_TRUE(!w.shuttleRamp(a, false));
    run(w, ShuttleBay::rampSeconds + 0.2);
    // Shut: B outside can't come in; nor open it from outside.
    EXPECT_EQ(w.shuttleRamp(b, true).value_or(""), std::string("Not in a Dropship"));
    standIn(w, a, kAtConsole);
    EXPECT_TRUE(!w.worldMessage(a)->flyingTo.has_value());
    ASSERT_TRUE(!w.fly(a, "cold-fork", "Landing Pad 1"));
    EXPECT_EQ(w.worldMessage(a)->flyingTo.value_or(-1), int64_t(*w.regionNamed("cold-fork")));
    EXPECT_TRUE(!w.worldMessage(a)->ferry.has_value());
    EXPECT_TRUE(!w.worldMessage(b)->flyingTo.has_value());
    EXPECT_TRUE(!w.riding(b));
    EXPECT_EQ(w.build(a, StructureKind::habDome, drivenBy(w, a)->position + Vec2(6, 0)).value_or(""), std::string("In flight"));
    EXPECT_EQ(w.callShuttle(a).value_or(""), std::string("In flight"));
    EXPECT_EQ(w.shuttleRamp(a, true).value_or(""), std::string("Wait for the landing"));
    const Ferry::Plan plan = w.region(w.shuttleOf(a)->first).simulation().shuttle(w.shuttleOf(a)->second)->plan;
    run(w, flightSeconds(plan) + 0.2);
    EXPECT_TRUE(!w.flying(a));
    EXPECT_TRUE(!w.worldMessage(a)->flyingTo.has_value());
}

/// A Prospector driving its Mule rides with it (C4): the Mule driven up the
/// ramp, the ramp shut and the take-off pressed from the Mule; in Cold Fork
/// the Mule is still its own, driven, its rider on it and its haul in it,
/// both where they stood in the bay.
TEST(shuttle_mule_rides_with_its_driver) {
    WorldMap w(first(), params());
    rig(w);
    const auto said = w.devMule("mule here");
    ASSERT_TRUE(said.rfind("dev: mule here: mule ", 0) == 0);
    const int64_t mule = std::atoll(said.c_str() + std::string("dev: mule here: mule ").size());
    const int64_t prospector = *w.regionOf(a)->driving(a);
    ASSERT_TRUE(!w.callShuttle(a));
    run(w, Ferry::descend + ShuttleBay::rampSeconds + 0.2);
    Shuttle* s = nearestShip(w, a);
    ASSERT_TRUE(s != nullptr);
    Simulation& sim = w.regionOf(a)->simulation();
    auto unit = [&](int64_t id) { return &sim.state.units[size_t(*sim.unitIndex(id))]; };
    // On the Mule in the bay by the ramp button, the ramp shut from it.
    unit(mule)->position = ShuttleBay::world(*s, Vec2(-0.95, 0));
    unit(mule)->heading = s->heading;
    unit(mule)->haul = Haul{300, 12};
    unit(prospector)->position = Mule::perch(*unit(mule));
    ASSERT_TRUE(!w.take(a, mule));
    w.step();
    ASSERT_TRUE(w.drivesMule(a));
    ASSERT_TRUE(!w.shuttleRamp(a, false));
    run(w, ShuttleBay::rampSeconds + 0.2);
    // Up to the console on it, and off.
    s = nearestShip(w, a);
    unit(mule)->position = ShuttleBay::world(*s, Vec2(0.8, 0));
    unit(prospector)->position = Mule::perch(*unit(mule));
    w.step();
    ASSERT_TRUE(!w.fly(a, "cold-fork", "Landing Pad 1"));
    s = nearestShip(w, a);
    ASSERT_TRUE(s->rides(mule) && s->rides(prospector));
    const Vec2 was = ShuttleBay::local(*s, unit(mule)->position);
    run(w, flightSeconds(s->plan) + 0.3);
    const size_t fork = *w.regionNamed("cold-fork");
    ASSERT_TRUE(*w.where(a) == fork && w.drivesMule(a));
    const auto mine = w.mule(a);
    ASSERT_TRUE(mine && mine->first == fork);
    Simulation& there = w.region(fork).simulation();
    const auto m = there.state.unit(mine->second);
    ASSERT_TRUE(m.has_value());
    EXPECT_TRUE(m->haul == (Haul{300, 12}));
    const auto rider = there.muleRider(mine->second);
    ASSERT_TRUE(rider.has_value());
    EXPECT_TRUE(there.state.unit(*rider)->task == Unit::Task::aboard);
    const Shuttle& down = there.state.shuttles.front();
    EXPECT_TRUE(down.grounded());
    EXPECT_TRUE(distance(ShuttleBay::local(down, m->position), was) < 0.1);
    EXPECT_TRUE(w.region(first().start).simulation().state.unit(mule) == std::nullopt);
}

/// `ferry mule` (the mmoferrymule flow's rig): the pilot's Mule on the
/// grounded ship's axis behind the ramp, facing it, its Prospector beside the
/// perch; taken and driven straight on, it goes up the ramp into the bay.
TEST(shuttle_dev_ferry_mule_drives_up_the_ramp) {
    WorldMap w(first(), params());
    rig(w);
    ASSERT_TRUE(!w.callShuttle(a));
    run(w, Ferry::descend + ShuttleBay::rampSeconds + 0.2);
    const std::string said = w.devFerry("ferry mule");
    std::printf("  %s\n", said.c_str());
    ASSERT_TRUE(said.rfind("dev: ferry: the Mule behind the ramp", 0) == 0);
    // On foot beside the perch: the take is the game's (E at the perch).
    ASSERT_TRUE(!w.drivesMule(a));
    const auto own = w.mule(a);
    ASSERT_TRUE(own.has_value());
    EXPECT_TRUE(distance(drivenBy(w, a)->position, Mule::perch(*w.regionOf(a)->simulation().state.unit(own->second))) <= Mule::reach);
    ASSERT_TRUE(!w.take(a, own->second));
    w.step();
    ASSERT_TRUE(w.drivesMule(a));
    const Shuttle* s = nearestShip(w, a);
    ASSERT_TRUE(s != nullptr);
    const Vec2 l = ShuttleBay::local(*s, drivenBy(w, a)->position);
    EXPECT_NEAR(l.x, ShuttleBay::rampFoot() - 3.5, 1e-6);
    EXPECT_NEAR(l.y, 0, 1e-6);
    // Straight on, up the ramp, until its tail is in (its middle past
    // −0.9; the ramp button is in reach until its middle passes 0.74).
    const int64_t mule = *w.regionOf(a)->driving(a);
    const double heading = s->heading;
    for (int i = 0; i < 60 * 12 && ShuttleBay::local(*nearestShip(w, a), drivenBy(w, a)->position).x < -0.6; i++) {
        PilotInput in;
        in.tick = w.regionOf(a)->lastInputTick(a) + 1;
        in.visit = w.visit(a);
        in.heading = heading;
        in.walk = Vec2(1, 0);
        w.input(a, in);
        w.step();
    }
    for (int i = 0; i < 40; i++) {
        PilotInput in;
        in.tick = w.regionOf(a)->lastInputTick(a) + 1;
        in.visit = w.visit(a);
        in.heading = heading;
        w.input(a, in);
        w.step();
    }
    ASSERT_TRUE(*w.regionOf(a)->driving(a) == mule);
    const Vec2 in = ShuttleBay::local(*nearestShip(w, a), drivenBy(w, a)->position);
    std::printf("  stopped at %.2f %.2f\n", in.x, in.y);
    EXPECT_TRUE(ShuttleBay::inBay(in) && in.x > -1.0 && in.x < 0.74);
    EXPECT_TRUE(!w.shuttleRamp(a, false));
}
