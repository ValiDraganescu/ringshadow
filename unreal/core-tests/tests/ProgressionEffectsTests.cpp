// The Prospector's skill tree in the region's sim (docs/prospector-progression.md,
// "Tests", core effects): short scenes around one driven Prospector at blue's
// main with a ProspectorProgress set by hand, one per building block. No
// progress set: the old numbers.
#include "LevelingScene.h"
#include "Progression.h"

using namespace ac;
using namespace leveling;

namespace {

using Slots = std::array<std::optional<Skill>, Progression::slotCount>;

/// A level-100 pilot with these ranks and slots (set by hand: the gates
/// are the catalogue's tests).
void learn(Simulation& sim, const std::map<Skill, int64_t>& ranks, Slots slots = {}) {
    ProspectorProgress p;
    p.xp = Progression::xp(Progression::cap);
    p.ranks = ranks;
    p.slots = slots;
    sim.setProgress(p);
}

/// Slot key `slot` (1 Q, 2 C, 3 G) pressed for one step.
void press(Simulation& sim, int64_t slot) {
    if (sim.pilot) sim.pilot->slot = slot;
    run(sim, 1.0 / 30);
}

/// Out on the Citadel's open side, facing out.
std::pair<Vec2, Vec2> outside(Simulation& sim, int64_t prospector, const Structure& citadel) {
    const Vec2 out = openSide(sim, citadel);
    const Vec2 at = citadel.position + out * (Rules::radius(StructureKind::citadel) + 4);
    unitRef(sim, prospector).position = at;
    setHeading(sim, std::atan2(out.y, out.x));
    return {at, out};
}

/// Beside the deposit nearest the Citadel, facing it; its index.
size_t atPatch(Simulation& sim, int64_t prospector, const Structure& citadel) {
    size_t dep = 0;
    for (size_t i = 1; i < sim.state.patches.size(); i++)
        if (distance(sim.state.patches[i].position, citadel.position) < distance(sim.state.patches[dep].position, citadel.position))
            dep = i;
    const auto p = sim.state.patches[dep];
    Vec2 n(-std::sin(p.angle), std::cos(p.angle));
    if (dot(n, citadel.position - p.position) < 0) n = -n;
    unitRef(sim, prospector).position = p.position + n * 1.0;
    setHeading(sim, std::atan2(-n.y, -n.x));
    return dep;
}

/// The well nearest blue's Citadel, and a Derrick
/// of blue's on it (id 800) when `derrick`.
size_t aWell(Simulation& sim, const Structure& citadel, bool derrick) {
    size_t wi = 0;
    for (size_t i = 1; i < sim.state.wells->size(); i++)
        if (distance((*sim.state.wells)[i].position, citadel.position) < distance((*sim.state.wells)[wi].position, citadel.position))
            wi = i;
    if (derrick) {
        sim.state.structures.push_back(Structure(800, StructureKind::derrick, 0, (*sim.state.wells)[wi].position));
        sim.refreshNav();
    }
    return wi;
}

/// Loads drilled one after another by the driven Prospector at `dep`.
std::vector<int64_t> drillTrips(Simulation& sim, int64_t prospector, int n, double seconds = Rules::miningTime) {
    if (sim.pilot) sim.pilot->hold = true;
    std::vector<int64_t> loads;
    for (int k = 0; k < n; k++) {
        run(sim, seconds + 0.2);
        loads.push_back(unit(sim, prospector).carrying);
        unitRef(sim, prospector).carrying = 0;
    }
    return loads;
}

} // namespace

/// No progress set: the driven Prospector's numbers are the hero's, as
/// before the tree; a progress reaches only the driven Prospector.
TEST(ProgressionEffects_noProgressIsTheOldNumbers) {
    auto [sim, prospector, citadel] = driven();
    run(sim, 1.0 / 30);
    const Unit u = unit(sim, prospector);
    const double drill = sim.rate(Stat::Drill{}, u), speed = sim.rate(Stat::Speed{}, u), hp = sim.maxHP(u);
    const int64_t ore = sim.load(u, false), mh = sim.load(u, true);
    const Price foundry = sim.buildCost(StructureKind::foundry, 0);
    EXPECT_EQ(drill, 1.0);
    EXPECT_NEAR(speed, Rules::heroSpeed, 1e-12);
    EXPECT_EQ(ore, Rules::heroCarry);
    EXPECT_EQ(mh, Rules::heroHydrogenCarry);
    sim.setProgress(ProspectorProgress{});
    EXPECT_TRUE(sim.skillEffects.empty());
    EXPECT_EQ(sim.rate(Stat::Drill{}, u), drill);
    EXPECT_EQ(sim.load(u, false), ore);
    learn(sim, {{Skill::quickDrill, 5}, {Skill::bigHaul, 3}, {Skill::plating, 5}, {Skill::lightFrame, 5}});
    // Another Prospector of blue's, not driven: untouched.
    const int64_t other = add(sim, 950, UnitKind::prospector, citadel.position + openSide(sim, citadel) * 8);
    EXPECT_EQ(sim.rate(Stat::Drill{}, unit(sim, other)), 1.0);
    EXPECT_EQ(sim.load(unit(sim, other), false), Rules::carry);
    sim.setProgress(std::nullopt);
    EXPECT_TRUE(sim.skillEffects.empty());
    EXPECT_EQ(sim.rate(Stat::Drill{}, u), drill);
    EXPECT_EQ(sim.rate(Stat::Speed{}, u), speed);
    EXPECT_EQ(sim.maxHP(u), hp);
    EXPECT_EQ(sim.load(u, false), ore);
    EXPECT_TRUE(sim.buildCost(StructureKind::foundry, 0) == foundry);
}

/// The reused `Stat` blocks on the driven Prospector: a rank-3 Quick drill
/// drills at ×1.18 on top of the hero, Plating adds hp, the loads add up
/// (Deep Vein whole: 25 × 1.3 + 9 + 15 = 56.5, 57 a trip; Quicksilver
/// whole: 20 + 9 + 20 = 49).
TEST(ProgressionEffects_quickDrillAndTheReusedStats) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::quickDrill, 3}});
    EXPECT_NEAR(sim.rate(Stat::Drill{}, unit(sim, prospector)), 1.18, 1e-12);
    atPatch(sim, prospector, citadel);
    if (sim.pilot) sim.pilot->hold = true;
    run(sim, Rules::miningTime / 1.18 - 0.2);
    EXPECT_EQ(unit(sim, prospector).carrying, int64_t(0)); // not done yet
    run(sim, 0.4);
    EXPECT_EQ(unit(sim, prospector).carrying, Rules::heroCarry); // done in 1/1.18 of the time
    learn(sim, {{Skill::plating, 5}, {Skill::bigHaul, 3}, {Skill::oreSled, 3}, {Skill::densePack, 3},
                {Skill::bigTank, 3}, {Skill::mercuryLungs, 5}, {Skill::lightFrame, 5}});
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.maxHP(unit(sim, prospector)), Rules::hp(UnitKind::prospector) + 30);
    EXPECT_EQ(unit(sim, prospector).hp, Rules::hp(UnitKind::prospector) + 30); // raised as it comes on
    EXPECT_EQ(sim.load(unit(sim, prospector), false), int64_t(57));
    EXPECT_EQ(sim.load(unit(sim, prospector), true), int64_t(49));
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed * 1.25, 1e-12);
}

/// Rich vein rank 2: every 4th ore trip carries double.
TEST(ProgressionEffects_richVeinRankTwoDoublesEveryFourthTrip) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::richVein, 2}});
    atPatch(sim, prospector, citadel);
    const auto loads = drillTrips(sim, prospector, 4);
    EXPECT_TRUE((loads == std::vector<int64_t>{25, 25, 25, 50}));
}

/// Overcharge from the Q slot: drill +100% for 10 s, then it waits out 60
/// from the press; not slotted, its key does nothing.
TEST(ProgressionEffects_overchargeRunsTenSecondsThenWaitsSixty) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::overcharge, 1}});
    press(sim, 1);
    EXPECT_EQ(sim.rate(Stat::Drill{}, unit(sim, prospector)), 1.0); // in no slot
    learn(sim, {{Skill::overcharge, 1}}, Slots{Skill::overcharge});
    press(sim, 2);
    EXPECT_EQ(sim.rate(Stat::Drill{}, unit(sim, prospector)), 1.0); // C is empty
    press(sim, 1);
    EXPECT_NEAR(sim.rate(Stat::Drill{}, unit(sim, prospector)), 2.0, 1e-12);
    EXPECT_TRUE(sim.activeRunning(prospector, Skill::overcharge));
    run(sim, 9.8);
    EXPECT_NEAR(sim.rate(Stat::Drill{}, unit(sim, prospector)), 2.0, 1e-12);
    run(sim, 0.4);
    EXPECT_EQ(sim.rate(Stat::Drill{}, unit(sim, prospector)), 1.0);
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::overcharge) == std::optional<std::string>("Ready in 50 s"));
    press(sim, 1);
    EXPECT_EQ(sim.rate(Stat::Drill{}, unit(sim, prospector)), 1.0); // still waiting
    run(sim, 49.8);
    EXPECT_TRUE(!sim.activeRefusal(unit(sim, prospector), Skill::overcharge).has_value());
    press(sim, 1);
    EXPECT_NEAR(sim.rate(Stat::Drill{}, unit(sim, prospector)), 2.0, 1e-12);
}

/// The other actives' bursts: Arc surge (build and repair), Aegis (damage
/// taken), Sprint (the strategy game's burst), Hydrogen boost (walk, for
/// 10 MH: the cargo first, then the wallet).
TEST(ProgressionEffects_theBurstChannels) {
    auto [sim, prospector, citadel] = driven();
    outside(sim, prospector, citadel);
    learn(sim, {{Skill::arcSurge, 1}, {Skill::aegis, 1}, {Skill::sprint, 1}},
          Slots{Skill::arcSurge, Skill::aegis, Skill::sprint});
    press(sim, 1);
    EXPECT_NEAR(sim.rate(Stat::Build{}, unit(sim, prospector)), Rules::heroWork * 3, 1e-12);
    EXPECT_NEAR(sim.rate(Stat::Repair{}, unit(sim, prospector)), Rules::heroWork * 3, 1e-12);
    press(sim, 2);
    const double hp = unit(sim, prospector).hp;
    sim.hit(prospector, 10);
    EXPECT_NEAR(unit(sim, prospector).hp, hp - 5, 1e-9);
    press(sim, 3);
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed * 1.6, 1e-12);
    run(sim, 8.1);
    EXPECT_NEAR(sim.rate(Stat::Build{}, unit(sim, prospector)), Rules::heroWork, 1e-12);
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed, 1e-12);
    EXPECT_EQ(sim.rate(Stat::DamageTaken{}, unit(sim, prospector)), 1.0);

    learn(sim, {{Skill::hydrogenBoost, 1}}, Slots{Skill::hydrogenBoost});
    sim.state.players[0].hydrogen = 5;
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::hydrogenBoost) == std::optional<std::string>("Needs 10 MH"));
    unitRef(sim, prospector).carrying = 4;
    unitRef(sim, prospector).hydrogen = true;
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::hydrogenBoost) == std::optional<std::string>("Needs 10 MH"));
    sim.state.players[0].hydrogen = 100;
    press(sim, 1);
    EXPECT_EQ(unit(sim, prospector).carrying, int64_t(0));
    EXPECT_EQ(sim.state.players[0].hydrogen, int64_t(94));
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed * 1.8, 1e-12);
    run(sim, 6.1);
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed, 1e-12);
}

/// Shield, then Ironclad on it: 30 hp over 10 s; with Ironclad 70 hp over 5 s.
TEST(ProgressionEffects_shieldAndIronclad) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::shield, 3}});
    run(sim, 10.1);
    EXPECT_NEAR(unit(sim, prospector).shield.value_or(0), 30, 1e-9);
    learn(sim, {{Skill::shield, 3}, {Skill::ironclad, 1}});
    run(sim, 2.5);
    EXPECT_NEAR(unit(sim, prospector).shield.value_or(0), 65, 0.2); // 14 a second
    run(sim, 0.5);
    EXPECT_NEAR(unit(sim, prospector).shield.value_or(0), 70, 1e-9);
    sim.hit(prospector, 50);
    EXPECT_NEAR(unit(sim, prospector).shield.value_or(0), 20, 1e-9);
    EXPECT_EQ(unit(sim, prospector).hp, Rules::hp(UnitKind::prospector));
    sim.setProgress(std::nullopt);
    run(sim, 1.0 / 30);
    EXPECT_TRUE(!unit(sim, prospector).shield.has_value());
}

/// Self mend, Cliff hop and Pulse mine: the strategy game's blocks from
/// the tree.
TEST(ProgressionEffects_selfMendCliffHopAndPulseMine) {
    auto [sim, prospector, citadel] = driven();
    const auto [me, out] = outside(sim, prospector, citadel);
    EXPECT_TRUE(!sim.pilotJumps(unit(sim, prospector)));
    learn(sim, {{Skill::selfMend, 3}, {Skill::cliffHop, 1}, {Skill::pulseMine, 1}}, Slots{Skill::pulseMine});
    EXPECT_TRUE(sim.pilotJumps(unit(sim, prospector)));
    unitRef(sim, prospector).hp = 20;
    unitRef(sim, prospector).hitAt = sim.state.time;
    run(sim, 5);
    EXPECT_EQ(unit(sim, prospector).hp, 20.0); // not 6 s without a hit yet
    run(sim, 2);
    EXPECT_NEAR(unit(sim, prospector).hp, 23, 0.2); // 3 a second after that
    const int64_t enemy = add(sim, 970, UnitKind::ranger, me + out * 1.5, 1);
    unitRef(sim, enemy).hp = 1000;
    press(sim, 1);
    EXPECT_NEAR(unit(sim, enemy).hp, 980, 1e-9);
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::pulseMine) == std::optional<std::string>("Ready in 14 s"));
}

/// Torch rhythm rank 3: every 3rd strike of the driven Prospector lands
/// double.
TEST(ProgressionEffects_torchRhythmDoublesEveryThirdStrike) {
    auto [sim, prospector, citadel] = driven();
    const auto [me, out] = outside(sim, prospector, citadel);
    const int64_t enemy = add(sim, 970, UnitKind::ranger, me + out * 1.0, 1);
    learn(sim, {{Skill::torchRhythm, 3}});
    std::vector<double> hits;
    for (int k = 0; k < 3; k++) {
        unitRef(sim, enemy).hp = 1000;
        sim.land(unit(sim, prospector), enemy, *sim.target(enemy));
        hits.push_back(1000 - unit(sim, enemy).hp);
    }
    EXPECT_TRUE(hits[0] > 0);
    EXPECT_EQ(hits[1], hits[0]);
    EXPECT_NEAR(hits[2], 2 * hits[0], 1e-9);
}

/// Clean cut: a load takes 15% less from the deposit; Motherlode (tried at
/// a sure chance): the load is triple.
TEST(ProgressionEffects_depositYieldAndTripleLoads) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::cleanCut, 3}});
    const size_t dep = atPatch(sim, prospector, citadel);
    const int64_t before = sim.state.patches[dep].remaining;
    EXPECT_TRUE((drillTrips(sim, prospector, 1) == std::vector<int64_t>{25}));
    EXPECT_EQ(before - sim.state.patches[dep].remaining, int64_t(21)); // 25 × 0.85, rounded
    learn(sim, {{Skill::motherlode, 1}});
    sim.skillEffects = {SkillEffect::Stat{SkillStat::loadTriple, Change::Plus{1.0}}};
    EXPECT_TRUE((drillTrips(sim, prospector, 1) == std::vector<int64_t>{75}));
}

/// Quick regrowth rank 3: a deposit the pilot empties grows back in 55%
/// of the wait (165 s, not 300).
TEST(ProgressionEffects_quickRegrowth) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::quickRegrowth, 3}});
    const size_t dep = atPatch(sim, prospector, citadel);
    sim.state.patches[dep].remaining = 5;
    drillTrips(sim, prospector, 1);
    ASSERT_TRUE(sim.state.patches[dep].depletedAt.has_value());
    EXPECT_NEAR(sim.regrowShare[static_cast<int64_t>(dep)], 0.55, 1e-12);
    std::vector<GameEvent> events;
    sim.state.patches[dep].depletedAt = sim.state.time - 160;
    sim.regrow(events);
    EXPECT_EQ(sim.state.patches[dep].remaining, int64_t(0));
    sim.state.patches[dep].depletedAt = sim.state.time - 166;
    sim.regrow(events);
    EXPECT_EQ(sim.state.patches[dep].remaining, sim.state.patches[dep].initial);
    EXPECT_TRUE(!sim.regrowShare.count(static_cast<int64_t>(dep)));
}

/// Stardust heart and Silver tide: a load dropped off counts 20% more in
/// the wallet (and the XP); the realm's tax (C1) is a cut of the bigger load.
TEST(ProgressionEffects_unloadYield) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::stardustHeart, 1}, {Skill::silverTide, 1}});
    const int64_t ore = sim.state.players[0].ore, mh = sim.state.players[0].hydrogen;
    unitRef(sim, prospector).carrying = 25;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].ore, ore + 30 - 3);
    EXPECT_EQ(sim.levels().xp(UnitKind::prospector), 30.0);
    unitRef(sim, prospector).carrying = 20;
    unitRef(sim, prospector).hydrogen = true;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].hydrogen, mh + 24 - 2);
}

/// The realm's cut of a load (C1): 10% to the nearest unit, halves up, at
/// least 1, never all of it.
TEST(ProgressionEffects_realmTaxRounding) {
    EXPECT_EQ(Progression::realmTaxPercent, int64_t(10));
    EXPECT_EQ(Progression::realmTax(0), int64_t(0));
    EXPECT_EQ(Progression::realmTax(1), int64_t(0));
    EXPECT_EQ(Progression::realmTax(2), int64_t(1));
    EXPECT_EQ(Progression::realmTax(4), int64_t(1));
    EXPECT_EQ(Progression::realmTax(14), int64_t(1));
    EXPECT_EQ(Progression::realmTax(15), int64_t(2));
    EXPECT_EQ(Progression::realmTax(20), int64_t(2));
    EXPECT_EQ(Progression::realmTax(25), int64_t(3));
    EXPECT_EQ(Progression::realmTax(100), int64_t(10));
    EXPECT_EQ(Progression::realmTax(1004), int64_t(100));
    EXPECT_EQ(Progression::realmTax(1005), int64_t(101));
}

/// The tax on an unload at the Citadel (C1): the wallet gets the load less
/// the cut, the event tells both, XP counts the whole load; with no world
/// (no progress) nothing is taken.
TEST(ProgressionEffects_realmTaxOnUnload) {
    auto [sim, prospector, citadel] = driven();
    const int64_t ore = sim.state.players[0].ore;
    unitRef(sim, prospector).carrying = 25;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].ore, ore + 25);
    learn(sim, {});
    const double xp = sim.levels().xp(UnitKind::prospector);
    unitRef(sim, prospector).carrying = 25;
    const auto events = run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].ore, ore + 25 + 22);
    EXPECT_EQ(sim.levels().xp(UnitKind::prospector) - xp, 25.0 * Leveling::oreXP);
    int64_t seen = 0;
    for (const GameEvent& e : events)
        if (const auto* d = e.as<GameEvent::Deposited>(); d && d->unit == prospector) {
            seen += 1;
            EXPECT_EQ(d->amount, int64_t(25));
            EXPECT_EQ(d->tax, int64_t(3));
            EXPECT_TRUE(!d->hydrogen);
        }
    EXPECT_EQ(seen, int64_t(1));
    const int64_t mh = sim.state.players[0].hydrogen;
    unitRef(sim, prospector).carrying = 4;
    unitRef(sim, prospector).hydrogen = true;
    const auto small = run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].hydrogen, mh + 3);
    for (const GameEvent& e : small)
        if (const auto* d = e.as<GameEvent::Deposited>(); d && d->unit == prospector) {
            EXPECT_EQ(d->tax, int64_t(1));
            EXPECT_TRUE(d->hydrogen);
        }
}

/// Dust runner and Quicksilver stride: walking faster empty, and with MH.
TEST(ProgressionEffects_speedWhileCarrying) {
    auto [sim, prospector, citadel] = driven();
    outside(sim, prospector, citadel);
    learn(sim, {{Skill::dustRunner, 3}, {Skill::quicksilverStride, 3}});
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed * 1.15, 1e-12);
    unitRef(sim, prospector).carrying = 25;
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed, 1e-12);
    unitRef(sim, prospector).hydrogen = true;
    EXPECT_NEAR(sim.rate(Stat::Speed{}, unit(sim, prospector)), Rules::heroSpeed * 1.15, 1e-12);
}

/// Long arm: the torch repairs from 1.5 cells farther.
TEST(ProgressionEffects_workReach) {
    auto [sim, prospector, citadel] = driven();
    const Vec2 out = openSide(sim, citadel);
    unitRef(sim, prospector).position =
        citadel.position + out * (Rules::radius(StructureKind::citadel) + Simulation::pilotHydrogenReach + 1.0);
    setHeading(sim, std::atan2(-out.y, -out.x));
    for (auto& s : sim.state.structures)
        if (s.id == citadel.id) s.hp = 500;
    EXPECT_TRUE(!sim.pilotRepair(unit(sim, prospector)).has_value());
    learn(sim, {{Skill::longArm, 3}});
    EXPECT_NEAR(sim.workReach(unit(sim, prospector)), Simulation::pilotHydrogenReach + 1.5, 1e-12);
    EXPECT_TRUE(sim.pilotRepair(unit(sim, prospector)) == std::optional<PilotTarget>(PilotTarget::Repair{citadel.id}));
}

/// Reinforced and Hardened works: the pilot's own buildings, and only
/// those, have 30% more hp and take 20% less damage.
TEST(ProgressionEffects_ownBuildings) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::reinforced, 3}, {Skill::hardenedWorks, 5}});
    auto ref = [&]() -> Structure& {
        for (auto& s : sim.state.structures)
            if (s.id == citadel.id) return s;
        return sim.state.structures.front();
    };
    EXPECT_EQ(sim.fullHP(ref()), Rules::hp(StructureKind::citadel)); // not the pilot's
    ref().hp = 1000;
    sim.hit(citadel.id, 100);
    EXPECT_NEAR(ref().hp, 900, 1e-9);
    sim.pilotBuildings.insert(citadel.id);
    EXPECT_NEAR(sim.fullHP(ref()), Rules::hp(StructureKind::citadel) * 1.3, 1e-9);
    sim.hit(citadel.id, 100);
    EXPECT_NEAR(ref().hp, 820, 1e-9);
    sim.setProgress(std::nullopt);
    EXPECT_EQ(sim.fullHP(ref()), Rules::hp(StructureKind::citadel));
}

/// Head start: a building the pilot places starts a quarter built, and is
/// its own; Efficient plans and Prefab add up on the MH (42% less), on the
/// world's prices (a Foundry 300 ore + 150 MH).
TEST(ProgressionEffects_placingAndBuildingCost) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::prefab, 3}, {Skill::efficientPlans, 3}, {Skill::headStart, 1}});
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::foundry, 0), 264, 87));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::foundry, 1), 150, 100)); // not red's
    const auto [me, out] = outside(sim, prospector, citadel);
    sim.state.players[0].ore = 1000;
    sim.state.players[0].hydrogen = 100;
    // The first open spot out from the Prospector.
    std::optional<Vec2> at;
    for (double d = 3; d <= 12 && !at; d += 0.5) {
        const auto p = sim.snap(StructureKind::habDome, me + out * d);
        if (p && !sim.placementRefusal(StructureKind::habDome, *p)) at = p;
    }
    ASSERT_TRUE(at.has_value());
    const auto why = sim.pilotBuild(StructureKind::habDome, *at);
    EXPECT_TRUE(!why.has_value());
    const Structure& s = sim.state.structures.back();
    EXPECT_EQ(s.kind, StructureKind::habDome);
    EXPECT_NEAR(s.buildLeft.value_or(0), Rules::buildTime(StructureKind::habDome) * 0.75, 1e-9);
    EXPECT_NEAR(s.hp, Rules::hp(StructureKind::habDome) * (0.1 + 0.9 * 0.25), 1e-9);
    EXPECT_TRUE(sim.pilotBuildings.count(s.id) == 1);
}

/// Emergency weld: the nearest hurt building of the pilot's within 3 cells
/// gets 30% of its hp back; with none, the key is refused.
TEST(ProgressionEffects_emergencyWeld) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::emergencyWeld, 1}}, Slots{std::nullopt, Skill::emergencyWeld});
    for (auto& s : sim.state.structures)
        if (s.id == citadel.id) s.hp = 500;
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::emergencyWeld) ==
                std::optional<std::string>("Nothing of yours to weld"));
    sim.pilotBuildings.insert(citadel.id);
    press(sim, 2);
    EXPECT_NEAR(sim.state.structure(citadel.id)->hp, 500 + 0.3 * Rules::hp(StructureKind::citadel), 1e-9);
    EXPECT_TRUE(sim.activeReady(prospector, Skill::emergencyWeld).has_value());
}

/// Seismic charge: the cargo fills from a deposit within 3 cells; away
/// from ore, or full, the key is refused.
TEST(ProgressionEffects_seismicCharge) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {{Skill::seismicCharge, 1}}, Slots{Skill::seismicCharge});
    outside(sim, prospector, citadel);
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::seismicCharge) ==
                std::optional<std::string>("No ore within 3 cells"));
    const size_t dep = atPatch(sim, prospector, citadel);
    const int64_t before = sim.state.patches[dep].remaining;
    unitRef(sim, prospector).carrying = 5;
    press(sim, 1);
    EXPECT_EQ(unit(sim, prospector).carrying, Rules::heroCarry);
    EXPECT_EQ(before - sim.state.patches[dep].remaining, Rules::heroCarry - 5);
    EXPECT_TRUE(sim.activeRefusal(unit(sim, prospector), Skill::seismicCharge) ==
                std::optional<std::string>("Ready in 120 s"));
}

/// In a Derrick: Cold seal takes 15% less from the well, Twin canister rank
/// 3 doubles every 3rd pump, and Quickfill makes the next pump instant.
TEST(ProgressionEffects_derrickPumps) {
    auto [sim, prospector, citadel] = driven();
    ASSERT_TRUE(sim.state.wells.has_value() && !sim.state.wells->empty());
    const size_t wi = aWell(sim, citadel, true);
    outside(sim, prospector, citadel); // a load is not dropped off at once
    learn(sim, {{Skill::coldSeal, 3}, {Skill::twinCanister, 3}, {Skill::quickfill, 1}},
          Slots{std::nullopt, std::nullopt, Skill::quickfill});
    std::vector<int64_t> loads, drawn;
    for (int k = 0; k < 3; k++) {
        const int64_t before = (*sim.state.wells)[wi].remaining;
        sim.prospectorEnter(unitRef(sim, prospector), 800);
        run(sim, Rules::hydrogenTime + 0.2);
        loads.push_back(unit(sim, prospector).carrying);
        drawn.push_back(before - (*sim.state.wells)[wi].remaining);
        unitRef(sim, prospector).carrying = 0;
        unitRef(sim, prospector).hydrogen = std::nullopt;
    }
    EXPECT_TRUE((loads == std::vector<int64_t>{20, 20, 40}));
    EXPECT_TRUE((drawn == std::vector<int64_t>{17, 17, 34}));
    press(sim, 3);
    sim.prospectorEnter(unitRef(sim, prospector), 800);
    run(sim, 1.0 / 30);
    EXPECT_EQ(unit(sim, prospector).carrying, int64_t(20)); // at once
}

/// Derrick tender and Wellspring: the pilot's Derrick fills its tank (6 MH
/// a minute at rank 3), collected into the wallet on the way in; its well
/// grows back 15 MH a minute. Not the pilot's: nothing.
TEST(ProgressionEffects_derrickTenderAndWellspring) {
    auto [sim, prospector, citadel] = driven();
    ASSERT_TRUE(sim.state.wells.has_value() && !sim.state.wells->empty());
    const size_t wi = aWell(sim, citadel, true);
    learn(sim, {{Skill::derrickTender, 3}, {Skill::wellspring, 3}});
    (*sim.state.wells)[wi].remaining = 1000;
    sim.stepSkills(60);
    EXPECT_TRUE(!sim.derrickTanks.count(800));
    EXPECT_EQ((*sim.state.wells)[wi].remaining, int64_t(1000));
    sim.pilotBuildings.insert(800);
    sim.stepSkills(60);
    EXPECT_NEAR(sim.derrickTanks[800], 6, 1e-9);
    EXPECT_EQ((*sim.state.wells)[wi].remaining, int64_t(1015));
    sim.stepSkills(60 * 60);
    EXPECT_NEAR(sim.derrickTanks[800], 100, 1e-9); // the tank is full
    const int64_t mh = sim.state.players[0].hydrogen;
    sim.prospectorEnter(unitRef(sim, prospector), 800);
    EXPECT_EQ(sim.state.players[0].hydrogen, mh + 100);
    EXPECT_NEAR(sim.derrickTanks[800], 0, 1e-9);
}

/// Field pump rank 3: with no Derrick, the driven Prospector pumps a bare
/// well at 80% of a Derrick's rate while the key is held.
TEST(ProgressionEffects_fieldPump) {
    auto [sim, prospector, citadel] = driven();
    ASSERT_TRUE(sim.state.wells.has_value() && !sim.state.wells->empty());
    const size_t wi = aWell(sim, citadel, false);
    const Vec2 w = (*sim.state.wells)[wi].position;
    // Clear of whatever stands on it (a Derrick of the map's).
    std::erase_if(sim.state.structures, [&](const Structure& s) { return distance(s.position, w) < 0.5; });
    sim.refreshNav();
    const Vec2 from = w + Vec2(1.5, 0);
    unitRef(sim, prospector).position = from;
    setHeading(sim, std::atan2(w.y - from.y, w.x - from.x));
    unitRef(sim, prospector).heading = std::atan2(w.y - from.y, w.x - from.x);
    EXPECT_TRUE(!sim.fieldWell(unit(sim, prospector)).has_value());
    learn(sim, {{Skill::fieldPump, 3}});
    EXPECT_TRUE(sim.pilotTarget(unit(sim, prospector)) ==
                std::optional<PilotTarget>(PilotTarget::Well{static_cast<int64_t>(wi)}));
    const int64_t before = (*sim.state.wells)[wi].remaining;
    if (sim.pilot) sim.pilot->hold = true;
    run(sim, Rules::hydrogenTime / 0.8 - 0.2);
    EXPECT_EQ(unit(sim, prospector).carrying, int64_t(0));
    run(sim, 0.4);
    EXPECT_EQ(unit(sim, prospector).carrying, Rules::heroHydrogenCarry);
    EXPECT_TRUE(unit(sim, prospector).hydrogen == std::optional<bool>(true));
    EXPECT_EQ(before - (*sim.state.wells)[wi].remaining, Rules::heroHydrogenCarry);
    EXPECT_TRUE(sim.fieldPumps.empty());
}

/// Second wind rank 3: the driven Prospector dies with 40 ore; the server
/// is told to bring it back 75% sooner with 30 of it.
TEST(ProgressionEffects_secondWind) {
    auto [sim, prospector, citadel] = driven();
    outside(sim, prospector, citadel);
    learn(sim, {{Skill::secondWind, 3}});
    unitRef(sim, prospector).carrying = 40;
    unitRef(sim, prospector).hp = 0;
    run(sim, 1.0 / 30);
    ASSERT_TRUE(sim.fallen.has_value());
    EXPECT_EQ(sim.fallen->unit, prospector);
    EXPECT_EQ(sim.fallen->cargo, int64_t(30));
    EXPECT_TRUE(!sim.fallen->hydrogen);
    EXPECT_NEAR(sim.fallen->sooner, 0.75, 1e-12);
}

/// A world's buildings (P6): the world's prices with an MH part, no
/// strategy-game prerequisite (a Bastion needs no Garrison), and a refusal
/// when the MH is short. With no progress: the strategy game's rules.
TEST(ProgressionEffects_worldBuildingPrices) {
    auto [sim, prospector, citadel] = driven();
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::foundry, 0), 150, 100));
    sim.state.players[0].ore = 1000;
    sim.state.players[0].hydrogen = 1000;
    EXPECT_EQ(sim.buildRefusal(StructureKind::bastion, 0).value_or(""), std::string("Needs a Garrison"));
    learn(sim, {});
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::citadel, 0), 400, 0));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::derrick, 0), 75, 0));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::habDome, 0), 150, 50));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::foundry, 0), 300, 150));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::lab, 0), 200, 200));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::spacedock, 0), 400, 300));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::bastion, 0), 250, 100));
    EXPECT_TRUE(isPrice(sim.buildCost(StructureKind::sentinel, 0), 150, 100));
    EXPECT_TRUE(!sim.buildRefusal(StructureKind::bastion, 0).has_value());
    EXPECT_TRUE(!sim.buildRefusal(StructureKind::spacedock, 0).has_value());
    sim.state.players[0].hydrogen = 49;
    EXPECT_EQ(sim.buildRefusal(StructureKind::habDome, 0).value_or(""), std::string("Not enough MH"));
    EXPECT_TRUE(!sim.buildRefusal(StructureKind::derrick, 0).has_value());
}

/// A finished Hab Dome of its side is a field depot in a world, whoever
/// built it (R1: the realm's): a load walked up to it goes into the wallet,
/// with its XP. An unfinished one, or with no progress (the strategy game):
/// nothing.
TEST(ProgressionEffects_habDomeDepot) {
    auto [sim, prospector, citadel] = driven();
    const auto [me, out] = outside(sim, prospector, citadel);
    Structure dome(801, StructureKind::habDome, 0, me + out * (Rules::radius(StructureKind::habDome) + 0.5));
    sim.state.structures.push_back(dome);
    const int64_t ore = sim.state.players[0].ore;
    // No progress: only a Citadel takes a load.
    unitRef(sim, prospector).carrying = 25;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].ore, ore);
    // A world: being built, not yet.
    learn(sim, {});
    sim.state.structures.back().buildLeft = 5;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].ore, ore);
    // Finished, built by another pilot (not in `pilotBuildings`): the depot.
    sim.state.structures.back().buildLeft = std::nullopt;
    run(sim, 1.0 / 30);
    // The realm's tax (C1) at a Hab Dome too: 3 of 25, 2 of 20.
    EXPECT_EQ(sim.state.players[0].ore, ore + 25 - 3);
    EXPECT_EQ(unit(sim, prospector).carrying, int64_t(0));
    unitRef(sim, prospector).carrying = 20;
    unitRef(sim, prospector).hydrogen = true;
    const int64_t mh = sim.state.players[0].hydrogen;
    run(sim, 1.0 / 30);
    EXPECT_EQ(sim.state.players[0].hydrogen, mh + 20 - 2);
}

/// A world's Lab stands on its own: the Prospector places it and welds it
/// up like any other building (the strategy game's Lab is an add-on).
TEST(ProgressionEffects_labStandsAlone) {
    auto [sim, prospector, citadel] = driven();
    learn(sim, {});
    const auto [me, out] = outside(sim, prospector, citadel);
    sim.state.players[0].ore = 1000;
    sim.state.players[0].hydrogen = 1000;
    std::optional<Vec2> at;
    for (double d = 3; d <= 12 && !at; d += 0.5) {
        const auto p = sim.snap(StructureKind::lab, me + out * d);
        if (p && !sim.placementRefusal(StructureKind::lab, *p)) at = p;
    }
    ASSERT_TRUE(at.has_value());
    EXPECT_TRUE(!sim.pilotBuild(StructureKind::lab, *at).has_value());
    EXPECT_EQ(sim.state.players[0].ore, int64_t(800));
    EXPECT_EQ(sim.state.players[0].hydrogen, int64_t(800));
    const Structure lab = sim.state.structures.back();
    EXPECT_EQ(lab.kind, StructureKind::lab);
    EXPECT_TRUE(!lab.parent.has_value());
    run(sim, 3);
    const auto now = sim.state.structure(lab.id);
    ASSERT_TRUE(now.has_value());
    EXPECT_TRUE(now->buildLeft.value_or(0) < Rules::buildTime(StructureKind::lab) - 2);
}
