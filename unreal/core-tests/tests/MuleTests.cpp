// The Mule (C4, docs/haul-and-ferry.md section 2, `Mule.h`): the hauler the
// pilot drives; filled at its hopper, it books its load in a Citadel's hall.
// Hand-built scenes of a few seconds round the Citadel's hall (HallScene.h).
#include "HallScene.h"
#include "Mule.h"
#include "SimHelpers.h"

#include <cmath>

using namespace ac;
using namespace simtest;

namespace {

/// A Mule of blue's at `at` facing `heading`, as one rolls out (its load
/// empty).
int64_t addMule(Simulation& sim, Vec2 at, double heading = -pi / 2, int64_t owner = 0) {
    const Unit m = sim.freshUnit(UnitKind::mule, sim.state.nextID, owner, at, heading, Unit::Task::idle);
    sim.state.nextID += 1;
    sim.state.units.push_back(m);
    sim.reindex();
    return m.id;
}

/// Blue's driven Prospector climbs on Mule `mule` and drives it.
void board(Hall& h, int64_t mule) {
    unitOf(h.sim, h.pilot).position = Mule::perch(unitOf(h.sim, mule)) + Vec2(1.0, 0);
    EXPECT_TRUE(!h.sim.muleBoard(h.pilot, mule));
    EXPECT_TRUE(h.sim.take(mule));
    h.sim.pilot->heading = unitOf(h.sim, mule).heading;
}

std::vector<GameEvent::Booked> booked(const std::vector<GameEvent>& events) {
    std::vector<GameEvent::Booked> out;
    for (const GameEvent& e : events)
        if (const auto* b = e.as<GameEvent::Booked>()) out.push_back(*b);
    return out;
}

} // namespace

/// The kind: drivable, steered, a machine; no soldier, no picks, no AI, no
/// building trains it; 30% of the Prospector's walk; it fits the doorway.
TEST(mule_kind) {
    const UnitStats& st = Rules::stats(UnitKind::mule);
    EXPECT_TRUE(st.hauler && st.drivable && st.steers && st.machine && !st.turret && !st.brood && !st.trainedBy);
    EXPECT_EQ(st.hp, 300.0);
    EXPECT_EQ(Rules::speed(UnitKind::mule), 0.3 * Rules::prospectorSpeed);
    EXPECT_EQ(Mule::ore(), int64_t(600));
    EXPECT_EQ(Mule::hydrogen(), int64_t(150));
    EXPECT_EQ(Mule::rollOut(), 20.0);
    EXPECT_TRUE(Pilot::drivable.contains(UnitKind::mule));
    EXPECT_TRUE(Leveling::machines.contains(UnitKind::mule));
    const std::vector<UnitKind> foundry = Rules::trains(StructureKind::foundry);
    EXPECT_TRUE(std::find(foundry.begin(), foundry.end(), UnitKind::mule) == foundry.end());
    EXPECT_TRUE(!perkOffer(UnitKind::mule, 2).has_value());
    const Unit m(1, UnitKind::mule, 0, Vec2(0, 0), 0, Unit::Task::idle);
    EXPECT_TRUE(!m.soldier() && !m.brood());
    // It fits the Citadel's doorway (3 wide) and the bay (2 wide, 4 long),
    // its driver's top under 1.9.
    EXPECT_TRUE(Mule::width < 2.0 && Mule::length < 4.0 && Mule::deck + 1.5 < 1.9 && Mule::height < 2.0);
    EXPECT_EQ(Rules::radius(UnitKind::mule), Mule::width / 2);
}

/// E at the hopper: the whole load goes in, ore or MH; no wallet, no XP.
/// Too far, aboard or with nothing: refused.
TEST(mule_fillTakesTheWholeLoad) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 18));
    Unit& p = unitOf(h.sim, h.pilot);
    p.position = Mule::hopper(unitOf(h.sim, mule)) + Vec2(1.5, 0);
    EXPECT_TRUE(h.sim.muleFill(h.pilot, mule).value_or("") == "Nothing to put in");
    unitOf(h.sim, h.pilot).carrying = 25;
    const int64_t ore = h.sim.state.players[0].ore;
    EXPECT_TRUE(!h.sim.muleFill(h.pilot, mule));
    EXPECT_EQ(unitOf(h.sim, mule).haul->ore, int64_t(25));
    EXPECT_EQ(unitOf(h.sim, h.pilot).carrying, int64_t(0));
    EXPECT_EQ(h.sim.state.players[0].ore, ore);
    unitOf(h.sim, h.pilot).carrying = 20;
    unitOf(h.sim, h.pilot).hydrogen = true;
    EXPECT_TRUE(!h.sim.muleFill(h.pilot, mule));
    EXPECT_TRUE(unitOf(h.sim, mule).haul == (Haul{25, 20}));
    EXPECT_TRUE(!unitOf(h.sim, h.pilot).hydrogen);
    // Out of reach of the hopper.
    unitOf(h.sim, h.pilot).carrying = 5;
    unitOf(h.sim, h.pilot).position = Mule::hopper(unitOf(h.sim, mule)) + Vec2(Mule::reach + 0.1, 0);
    EXPECT_TRUE(h.sim.muleFill(h.pilot, mule).value_or("") == "Get to the Mule's hopper");
    EXPECT_EQ(unitOf(h.sim, mule).haul->total(), int64_t(45));
    // A Prospector of another side.
    const int64_t red = addUnit(h.sim.state, UnitKind::prospector, Mule::hopper(unitOf(h.sim, mule)), 1);
    h.sim.reindex();
    unitOf(h.sim, red).carrying = 5;
    EXPECT_TRUE(h.sim.muleFill(red, mule).has_value());
}

/// At or over 1000 nothing goes in; the load that crosses 1000 is taken
/// whole.
TEST(mule_fullRefusesAndTheLastLoadGoesOver) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 18));
    unitOf(h.sim, mule).haul = Haul{990, 0};
    unitOf(h.sim, h.pilot).position = Mule::hopper(unitOf(h.sim, mule));
    unitOf(h.sim, h.pilot).carrying = 25;
    EXPECT_TRUE(!h.sim.muleFill(h.pilot, mule));
    EXPECT_EQ(unitOf(h.sim, mule).haul->total(), int64_t(1015));
    unitOf(h.sim, h.pilot).carrying = 25;
    EXPECT_TRUE(h.sim.muleFill(h.pilot, mule).value_or("") == Mule::full);
    EXPECT_EQ(unitOf(h.sim, h.pilot).carrying, int64_t(25));
    // Exactly full: refused too; ore and MH count together.
    unitOf(h.sim, mule).haul = Haul{600, 400};
    EXPECT_TRUE(h.sim.muleFill(h.pilot, mule).value_or("") == Mule::full);
    EXPECT_EQ(Mule::room(unitOf(h.sim, mule)), int64_t(0));
    unitOf(h.sim, mule).haul = Haul{600, 399};
    EXPECT_TRUE(!h.sim.muleFill(h.pilot, mule));
    EXPECT_EQ(unitOf(h.sim, mule).haul->total(), int64_t(1024));
}

/// E at the perch: on it, riding along while it drives, untouchable; E
/// again: off beside it, on foot. Out of reach, or the seat taken: refused.
TEST(mule_boardRideAndLeave) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 18), 0);
    unitOf(h.sim, h.pilot).position = Mule::perch(unitOf(h.sim, mule)) + Vec2(Mule::reach + 0.1, 0);
    EXPECT_TRUE(h.sim.muleBoard(h.pilot, mule).value_or("") == "Get to the Mule's perch");
    board(h, mule);
    EXPECT_TRUE(h.sim.muleRider(mule) == h.pilot);
    EXPECT_TRUE(h.sim.ridden(unitOf(h.sim, h.pilot)) == mule);
    EXPECT_TRUE(h.sim.piloted(mule) && !h.sim.piloted(h.pilot));
    // A second Prospector finds the seat taken.
    const int64_t other = addUnit(h.sim.state, UnitKind::prospector, Mule::perch(unitOf(h.sim, mule)), 0);
    h.sim.reindex();
    EXPECT_TRUE(h.sim.muleBoard(other, mule).value_or("") == "Someone drives it already");
    // Driving ahead, the rider rides along.
    h.sim.pilot->walk = Vec2(1, 0);
    steps(h.sim, 2);
    const Unit& m = unitOf(h.sim, mule);
    EXPECT_TRUE(m.position.x > kAt.x + 2);
    EXPECT_EQ(unitOf(h.sim, h.pilot).position, m.position);
    // Aboard, as a Dropship's passenger: what the Brood, the rounds and the
    // AIs skip.
    EXPECT_TRUE(unitOf(h.sim, h.pilot).task == Unit::Task::aboard);
    EXPECT_TRUE(h.sim.targetable(m, 1));
    // Off: it stands beside the perch, on foot, driven again.
    h.sim.pilot->walk = Vec2(0, 0);
    EXPECT_TRUE(h.sim.take(h.pilot));
    const Unit& off = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(off.task == Unit::Task::idle && !h.sim.muleRider(mule));
    EXPECT_TRUE(distance(off.position, Mule::perch(unitOf(h.sim, mule))) < Mule::reach);
    EXPECT_TRUE(distance(off.position, unitOf(h.sim, mule).position) > Rules::radius(UnitKind::mule));
    // Left alone, it stands where it is.
    const Vec2 parked = unitOf(h.sim, mule).position;
    steps(h.sim, 3);
    EXPECT_EQ(unitOf(h.sim, mule).position, parked);
}

/// Driven, it goes 30% of the Prospector's walk, hero or not.
TEST(mule_speed) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(-10, 20), 0);
    board(h, mule);
    h.sim.pilot->walk = Vec2(1, 0);
    const Vec2 from = unitOf(h.sim, mule).position;
    steps(h.sim, 3);
    EXPECT_NEAR(distance(unitOf(h.sim, mule).position, from), 3 * 0.3 * Rules::prospectorSpeed, 1e-6);
}

/// Driven through the open door into the hall: its whole load to the
/// driver's bank, once, a Booked event; not in the doorway, not undriven.
TEST(mule_booksInTheHallNotInTheDoorway) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(4, 12));
    structureOf(h.sim, h.citadel).door = Door{1, true};
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 9));
    unitOf(h.sim, mule).haul = Haul{300, 40};
    // Undriven, set down in the hall: nothing.
    unitOf(h.sim, mule).position = kAt + Vec2(0, 2);
    EXPECT_TRUE(booked(steps(h.sim, 0.2)).empty());
    unitOf(h.sim, mule).position = kAt + Vec2(0, 9);
    board(h, mule);
    const int64_t ore = h.sim.state.players[0].ore, mh = h.sim.state.players[0].hydrogen;
    // Its centre in the doorway (outside the hall): not yet.
    unitOf(h.sim, mule).position = kAt + Vec2(0, 5.6);
    EXPECT_TRUE(booked(steps(h.sim, 0.2)).empty());
    EXPECT_EQ(h.sim.state.players[0].ore, ore);
    // Back out, then driven in through the doorway (it fits).
    unitOf(h.sim, mule).position = kAt + Vec2(0, 9);
    h.sim.pilot->walk = Vec2(1, 0);
    std::vector<GameEvent> events = steps(h.sim, 6);
    EXPECT_TRUE(unitOf(h.sim, mule).position.y < kAt.y + 4);
    const auto b = booked(events);
    ASSERT_TRUE(b.size() == size_t(1));
    EXPECT_EQ(b[0].unit, mule);
    EXPECT_EQ(b[0].ore, int64_t(300));
    EXPECT_EQ(b[0].hydrogen, int64_t(40));
    EXPECT_EQ(b[0].at, kAt);
    // No progress (not a world): no realm's cut.
    EXPECT_EQ(b[0].taxOre + b[0].taxHydrogen, int64_t(0));
    EXPECT_EQ(h.sim.state.players[0].ore, ore + 300);
    EXPECT_EQ(h.sim.state.players[0].hydrogen, mh + 40);
    EXPECT_EQ(unitOf(h.sim, mule).haul->total(), int64_t(0));
    // Nothing more while it stays.
    EXPECT_TRUE(booked(steps(h.sim, 0.5)).empty());
}

/// In a world (the driver's progress set): the realm's cut, and Stardust
/// heart counts the ore more, as a Prospector's unload.
TEST(mule_bookingIsTaxedAndSkilled) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(4, 12));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 2));
    unitOf(h.sim, mule).haul = Haul{100, 50};
    board(h, mule);
    ProspectorProgress g;
    g.xp = Progression::xp(Progression::cap);
    g.ranks = {{Skill::stardustHeart, 1}};
    h.sim.setProgress(g);
    const int64_t ore = h.sim.state.players[0].ore, mh = h.sim.state.players[0].hydrogen;
    const auto b = booked(steps(h.sim, 1.0 / 30));
    ASSERT_TRUE(b.size() == size_t(1));
    EXPECT_EQ(b[0].ore, int64_t(120));
    EXPECT_EQ(b[0].hydrogen, int64_t(50));
    EXPECT_EQ(b[0].taxOre, Progression::realmTax(120));
    EXPECT_EQ(b[0].taxHydrogen, Progression::realmTax(50));
    EXPECT_EQ(h.sim.state.players[0].ore, ore + 120 - Progression::realmTax(120));
    EXPECT_EQ(h.sim.state.players[0].hydrogen, mh + 50 - Progression::realmTax(50));
}

/// A Prospector walking its own load into the Citadel still unloads; a Hab
/// Dome takes none (ProgressionEffects_habDomeTakesNoLoad).
TEST(mule_prospectorStillUnloadsAtTheCitadel) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 3));
    unitOf(h.sim, h.pilot).carrying = 5;
    const int64_t ore = h.sim.state.players[0].ore;
    steps(h.sim, 0.2);
    EXPECT_EQ(unitOf(h.sim, h.pilot).carrying, int64_t(0));
    EXPECT_EQ(h.sim.state.players[0].ore, ore + 5);
}

/// Destroyed: its load is lost, its rider set down beside it unhurt.
TEST(mule_destroyedDropsItsRider) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 18));
    unitOf(h.sim, mule).haul = Haul{500, 0};
    board(h, mule);
    unitOf(h.sim, mule).hp = 0;
    const auto events = steps(h.sim, 1.0 / 30);
    EXPECT_TRUE(!h.sim.state.unit(mule));
    const Unit& p = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(p.task == Unit::Task::idle && p.hp > 0);
    EXPECT_TRUE(distance(p.position, kAt + Vec2(0, 18)) < Mule::reach + 1);
    bool died = false;
    for (const GameEvent& e : events)
        if (const auto* d = e.as<GameEvent::Died>(); d && d->unit == mule) died = true;
    EXPECT_TRUE(died);
}

/// The driven Prospector mends a hurt Mule with the key held, as a
/// building: a whole bar in its roll-out time at the hero's rate, half its
/// price, paid as it goes.
TEST(mule_repairedLikeABuilding) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 16), 0);
    unitOf(h.sim, mule).hp = 150;
    h.sim.state.players[0].ore = 1000;
    h.sim.state.players[0].hydrogen = 1000;
    h.sim.pilot->heading = pi / 2;
    unitOf(h.sim, h.pilot).heading = pi / 2;
    EXPECT_TRUE(h.sim.hurtMule(unitOf(h.sim, h.pilot)) == mule);
    h.sim.pilot->hold = true;
    const auto events = steps(h.sim, 2);
    // Half the bar in half its time over the hero's 2: 2.5 s; 2 s mends 120.
    EXPECT_NEAR(unitOf(h.sim, mule).hp, 150 + 2 * 300.0 / 20 * Rules::heroWork - 300.0 / 20 * Rules::heroWork / 30, 3);
    int64_t ore = 0, mh = 0;
    for (const GameEvent& e : events)
        if (const auto* r = e.as<GameEvent::Repaired>(); r && r->structure == mule) {
            ore += r->ore;
            mh += r->hydrogen;
        }
    const double share = (unitOf(h.sim, mule).hp - 150) / 300 * Rules::repairShare;
    EXPECT_NEAR(double(ore), share * 600, 1.01);
    EXPECT_NEAR(double(mh), share * 150, 1.01);
    EXPECT_EQ(h.sim.state.players[0].ore, 1000 - ore);
}

/// The Brood may strike it.
TEST(mule_theBroodCanHitIt) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t mule = addMule(h.sim, kAt + Vec2(0, 18));
    EXPECT_TRUE(h.sim.targetable(unitOf(h.sim, mule), 1));
    const double before = unitOf(h.sim, mule).hp;
    h.sim.hit(mule, 10, false);
    EXPECT_TRUE(unitOf(h.sim, mule).hp < before);
}

/// Rolled out of a Foundry: at its door facing out, empty, its Trained
/// with the next step's events.
TEST(mule_rollsOutOfTheFoundryDoor) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 14));
    const int64_t foundry = addStructure(h.sim.state, StructureKind::foundry, kAt + Vec2(20, 0), 0);
    const auto id = h.sim.rollOut(foundry, 0);
    ASSERT_TRUE(id.has_value());
    const Unit& m = unitOf(h.sim, *id);
    EXPECT_TRUE(m.kind == UnitKind::mule && m.haul == Haul{} && m.hp == 300);
    EXPECT_EQ(m.position, Mule::door(kAt + Vec2(20, 0)));
    EXPECT_EQ(m.heading, Mule::doorHeading);
    bool trained = false;
    for (const GameEvent& e : steps(h.sim, 1.0 / 30))
        if (const auto* t = e.as<GameEvent::Trained>(); t && t->unit == *id) trained = true;
    EXPECT_TRUE(trained);
    EXPECT_TRUE(Mule::bookedToast(300, 40) == "Booked · +300 ore, +40 MH");
    EXPECT_TRUE(Mule::bookedToast(0, 12) == "Booked · +12 MH");
}
