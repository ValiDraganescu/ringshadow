// Demolish (C3, docs/guns-and-practice.md "Limits per region and demolish"):
// half the price back whole, finished or a scaffold; the building leaves at
// once with its shatter and the Demolished event; a Citadel with someone in
// its hall or at its guns stays. Hand-built scenes of a step or two.
#include "HallScene.h"
#include "Progression.h"
#include "SimHelpers.h"

using namespace ac;
using namespace simtest;

namespace {

template <class E> std::vector<E> all(const std::vector<GameEvent>& events) {
    std::vector<E> out;
    for (const auto& e : events) if (const E* x = e.as<E>()) out.push_back(*x);
    return out;
}

} // namespace

TEST(demolish_salvageIsHalfThePriceWhole) {
    EXPECT_EQ(Rules::salvageShare, 0.5);
    EXPECT_EQ(Rules::demolishHold, 1.5);
    EXPECT_TRUE(Simulation::salvage(StructureKind::dummy, true) == (Price{5, 0}));
    // 125 ore and 25 MH: 62 and 12, rounded down.
    EXPECT_TRUE(Simulation::salvage(StructureKind::gatling, true) == (Price{62, 12}));
    EXPECT_TRUE(Simulation::salvage(StructureKind::citadel, true) == (Price{200, 0}));
    EXPECT_TRUE(Simulation::salvage(StructureKind::lab, true) == (Price{100, 100}));
    // Off a world: the strategy game's prices.
    EXPECT_TRUE(Simulation::salvage(StructureKind::habDome, false) ==
                (Price{Rules::cost(StructureKind::habDome) / 2, Rules::hydrogenCost(StructureKind::habDome) / 2}));
    for (const StructureKind k : Progression::blueprintKinds()) {
        const Price p = Simulation::salvage(k, true);
        EXPECT_TRUE(p.ore * 2 <= Progression::worldOre(k) && p.ore * 2 + 1 >= Progression::worldOre(k));
        EXPECT_TRUE(p.hydrogen * 2 <= Progression::worldHydrogen(k) && p.hydrogen * 2 + 1 >= Progression::worldHydrogen(k));
    }
}

TEST(demolish_leavesAtOnceAndTellsTheNextStep) {
    GameState s = bare(1, 0, {}, {}, 1);
    const int64_t gun = addStructure(s, StructureKind::gatling, Vec2(30, 30), 0);
    const int64_t pro = addUnit(s, Unit::Kind::prospector, Vec2(30 - Rules::radius(StructureKind::gatling) - 0.4, 30), 0);
    Simulation sim(s, std::nullopt);
    ASSERT_TRUE(sim.take(pro));
    const Unit u = *sim.state.unit(pro);
    const Structure g = *sim.state.structure(gun);
    EXPECT_TRUE(sim.demolishReach(u, g));
    EXPECT_TRUE(!sim.demolishRefusal(g));
    ASSERT_TRUE(sim.demolish(gun, Price{62, 12}));
    EXPECT_TRUE(!sim.state.structure(gun).has_value());
    EXPECT_TRUE(!sim.demolish(gun, Price{}));
    const auto events = sim.step(1.0 / 30);
    const auto gone = all<GameEvent::Destroyed>(events);
    const auto paid = all<GameEvent::Demolished>(events);
    ASSERT_TRUE(gone.size() == size_t(1) && paid.size() == size_t(1));
    EXPECT_TRUE(gone[0].structure == gun && gone[0].kind == StructureKind::gatling);
    EXPECT_TRUE(paid[0] == (GameEvent::Demolished{gun, 62, 12}));
    // Told once.
    EXPECT_TRUE(all<GameEvent::Demolished>(sim.step(1.0 / 30)).empty());
    // Nothing paid by the core.
    EXPECT_EQ(sim.state.players[0].ore, s.players[0].ore);
    // Out of reach: past the repair reach from its edge.
    Unit far = u;
    far.position = Vec2(30, 30) + Vec2(Rules::radius(StructureKind::gatling) + sim.workReach(u) + 1, 0);
    EXPECT_TRUE(!sim.demolishReach(far, g));
    EXPECT_TRUE(sim.demolishReach(far, g, 1.5));
}

TEST(demolish_aScaffoldStopsItsWelder) {
    GameState s = bare(1, 0, {}, {}, 1);
    s.players[0].ore = 500;
    s.players[0].hydrogen = 500;
    const int64_t pro = addUnit(s, Unit::Kind::prospector, Vec2(20, 20), 0);
    Simulation sim(s, std::nullopt);
    ASSERT_TRUE(sim.take(pro));
    const int64_t id = sim.state.nextID;
    ASSERT_TRUE(!sim.pilotBuild(StructureKind::dummy, Vec2(22.5, 20.5)));
    sim.step(1.0 / 30);
    ASSERT_TRUE(sim.state.structure(id).has_value() && !sim.state.structure(id)->complete());
    EXPECT_TRUE(sim.state.unit(pro)->task == Unit::Task::building);
    ASSERT_TRUE(sim.demolish(id, Simulation::salvage(StructureKind::dummy, true)));
    EXPECT_TRUE(sim.state.unit(pro)->task == Unit::Task::idle);
    EXPECT_TRUE(!sim.state.unit(pro)->structure.has_value());
    const auto paid = all<GameEvent::Demolished>(sim.step(1.0 / 30));
    ASSERT_TRUE(paid.size() == size_t(1));
    EXPECT_EQ(paid[0].ore, int64_t(5));
}

TEST(demolish_aCitadelWithSomeoneInItStays) {
    const CitadelHall on(true);
    // The pilot outside: it may go.
    Hall out = hallScene(kAt + Vec2(0, Rules::radius(StructureKind::citadel) + 3));
    EXPECT_TRUE(!out.sim.demolishRefusal(structureOf(out.sim, out.citadel)));
    // In the hall.
    Hall in = hallScene(kAt + Vec2(1, 1));
    EXPECT_EQ(in.sim.demolishRefusal(structureOf(in.sim, in.citadel)), std::optional<std::string>("Someone is inside"));
    // At a gun.
    Hall gun = hallScene(kAt + Vec2(1, 1));
    manGun(gun, 0);
    EXPECT_EQ(gun.sim.demolishRefusal(structureOf(gun.sim, gun.citadel)), std::optional<std::string>("Someone is at its guns"));
    // Another kind never asks.
    GameState s = bare(1, 0, {}, {}, 1);
    const int64_t d = addStructure(s, StructureKind::habDome, Vec2(30, 30), 0);
    addUnit(s, Unit::Kind::prospector, Vec2(30, 30), 0);
    Simulation sim(s, std::nullopt);
    EXPECT_TRUE(!sim.demolishRefusal(*sim.state.structure(d)));
}
