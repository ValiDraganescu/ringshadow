// The kind table (`UnitStats::byKind`, docs/kind-registry.md): every row is
// complete, and every fact the table took over from an if-chain is what
// the chain said before the refactor (written out here as it was).
#include "test.h"

#include "Pilot.h"
#include "Rules.h"
#include "Simulation.h"
#include "Types.h"

#include <set>
#include <string>
#include <vector>

using namespace ac;

namespace {

using UK = UnitKind;
using SK = StructureKind;

std::vector<UK> oldTrains(SK k) {
    switch (k) {
    case SK::citadel: return {UK::prospector};
    case SK::garrison: return {UK::ranger, UK::comet, UK::juggernaut};
    case SK::foundry: return {UK::firefly, UK::longbow, UK::hailstorm, UK::atlas, UK::scorpion};
    case SK::spacedock: return {UK::dropship, UK::kestrel, UK::peregrine};
    default: return {};
    }
}

int64_t oldSlots(UK k) { return k == UK::atlas ? 16 : k == UK::juggernaut ? 2 : k == UK::longbow ? 4 : 1; }

bool oldNeedsLab(UK k) { return k == UK::juggernaut || k == UK::longbow || k == UK::kestrel || k == UK::atlas; }

double oldVision(UK k) {
    switch (k) {
    case UK::prospector: case UK::scorpion: case UK::mule: return 12;
    case UK::ranger: case UK::comet: return 13;
    case UK::juggernaut: case UK::firefly: case UK::kestrel: return 14;
    case UK::longbow: case UK::dropship: case UK::hailstorm: case UK::peregrine: case UK::atlas: return 15;
    case UK::shardling: case UK::gloamwing: case UK::lodeback: return 8;
    case UK::matriarch: return 25;
    }
    return 0;
}

double oldFlight(UK kind, bool anchored, double d) {
    switch (kind) {
    case UK::juggernaut: return 0.15 + d / 14;
    case UK::longbow: if (anchored) return 0.3 + d * 0.035; return 0;
    case UK::firefly: return 0.09;
    case UK::kestrel: return 0.12 + d / 22;
    case UK::hailstorm: return 0.1 + d / 28;
    case UK::peregrine: return 0.1 + d / 30;
    case UK::scorpion: return 0.2;
    default: return 0;
    }
}

std::string oldTitle(UK k) {
    switch (k) {
    case UK::prospector: return "Prospector"; case UK::ranger: return "Ranger"; case UK::comet: return "Comet";
    case UK::firefly: return "Firefly"; case UK::juggernaut: return "Juggernaut"; case UK::dropship: return "Dropship";
    case UK::longbow: return "Longbow"; case UK::kestrel: return "Kestrel"; case UK::hailstorm: return "Hailstorm";
    case UK::peregrine: return "Peregrine"; case UK::atlas: return "Atlas"; case UK::scorpion: return "Scorpion";
    case UK::shardling: return "Shardling"; case UK::gloamwing: return "Gloamwing"; case UK::lodeback: return "Lodeback";
    case UK::matriarch: return "the Matriarch";
    case UK::mule: return "Mule";
    }
    return "";
}

bool oldTurret(UK k) { return k == UK::longbow || k == UK::firefly || k == UK::hailstorm || k == UK::atlas; }
double oldTurretRateAi(UK k) {
    return k == UK::firefly ? 6.0 : k == UK::hailstorm ? 5.0 : k == UK::atlas ? Rules::atlasTorsoRate : 4.0;
}
std::optional<double> oldTurretRatePilot(UK k) {
    switch (k) {
    case UK::longbow: return 2.4;
    case UK::firefly: return 3.5;
    case UK::hailstorm: return 3.2;
    case UK::atlas: return Rules::atlasTorsoRate;
    default: return std::nullopt;
    }
}
double oldHullAi(UK k) {
    return k == UK::longbow ? 5 : k == UK::hailstorm ? 6 : k == UK::firefly ? 9
        : k == UK::atlas ? Rules::atlasBodyRate : Rules::prospectorTurnRate;
}
// C4: the Mule's 1.6 went with the drive: wheels turn by `turnRadius` as they roll.
double oldHullPilot(UK k) { return k == UK::firefly ? 3.0 : k == UK::atlas ? Rules::atlasBodyRate : 2.0; }
bool oldAnchors(UK k) { return k == UK::longbow || k == UK::scorpion; }
bool oldBoards(UK k) { return Rules::stats(k).bio || k == UK::scorpion; }

// C4: the Mule, a machine the pilot drives (not in the old table).
const EnumSet<UK> oldMachines{UK::prospector, UK::firefly, UK::longbow, UK::dropship, UK::kestrel, UK::hailstorm,
                              UK::peregrine, UK::atlas, UK::scorpion, UK::mule};
const EnumSet<UK> oldBio{UK::ranger, UK::comet, UK::juggernaut};
const EnumSet<UK> oldDrivable{UK::prospector, UK::ranger, UK::comet, UK::firefly, UK::juggernaut, UK::dropship,
                              UK::longbow, UK::kestrel, UK::hailstorm, UK::peregrine, UK::atlas, UK::scorpion, UK::mule};

} // namespace

/// Every row is complete: a title, unique; a player kind is driven and
/// trained by a building, a bug of the Brood neither; a hauler (C4, the
/// Mule) is driven, bought (it has a price) and trained by none.
TEST(kindTable_everyRowIsComplete) {
    EXPECT_EQ(UnitStats::byKind.size(), allCases<UK>().size());
    std::set<std::string_view> titles;
    size_t players = 0;
    for (UK k : allCases<UK>()) {
        const UnitStats& st = Rules::stats(k);
        EXPECT_TRUE(!st.title.empty());
        EXPECT_TRUE(titles.insert(st.title).second);
        EXPECT_TRUE(st.vision > 0);
        EXPECT_TRUE(st.cargoSlots >= 1);
        if (st.brood) {
            EXPECT_TRUE(!st.drivable && !st.trainedBy && !st.machine && !st.aiTrains && !st.hauler);
        } else if (st.hauler) {
            EXPECT_TRUE(st.drivable && !st.trainedBy && !st.aiTrains && st.steers && !st.turret);
            EXPECT_TRUE(Rules::stats(k).ore > 0);
        } else {
            players++;
            EXPECT_TRUE(st.drivable && st.trainedBy.has_value());
            EXPECT_TRUE(Rules::stats(k).ore > 0);
        }
        // A turret has its rates, a steered vehicle its turret (but a
        // hauler, which has none).
        EXPECT_TRUE(!st.turret || (st.turretRateAi > 0 && st.turretRatePilot > 0));
        EXPECT_TRUE(!st.burrows || st.anchors);
        // Every current kind has a hand-written place in the AI's chain.
        EXPECT_TRUE(!st.aiTrains);
    }
    size_t trained = 0;
    for (SK s : allCases<SK>()) trained += Rules::trains(s).size();
    EXPECT_EQ(trained, players);
}

/// `trains` filters the table in kind order: the same lists as before.
TEST(kindTable_trainsAsBefore) {
    for (SK s : allCases<SK>()) {
        EXPECT_TRUE(Rules::trains(s) == oldTrains(s));
        const auto t = oldTrains(s);
        EXPECT_TRUE(Rules::produces(s) == (t.empty() ? std::nullopt : std::optional<UK>(t.front())));
    }
}

/// Every fact the table took over keeps its old value, for every kind.
TEST(kindTable_factsAsBefore) {
    for (UK k : allCases<UK>()) {
        const UnitStats& st = Rules::stats(k);
        EXPECT_EQ(Rules::slots(k), oldSlots(k));
        EXPECT_EQ(Rules::needsLab(k), oldNeedsLab(k));
        EXPECT_EQ(Rules::vision(k), oldVision(k));
        EXPECT_EQ(Rules::boards(k), oldBoards(k));
        EXPECT_TRUE(Simulation::title(k) == oldTitle(k));
        EXPECT_EQ(st.brood, k >= UK::shardling && k <= UK::matriarch);
        EXPECT_EQ(st.turret, oldTurret(k));
        if (st.turret) EXPECT_EQ(st.turretRateAi, oldTurretRateAi(k));
        EXPECT_TRUE(Pilot::turretRate(k) == oldTurretRatePilot(k));
        EXPECT_EQ(Pilot::steers(k), oldTurret(k) || k == UK::mule);
        EXPECT_EQ(st.hullTurnRateAi > 0 ? st.hullTurnRateAi : Rules::prospectorTurnRate, oldHullAi(k));
        EXPECT_EQ(Pilot::hullTurnRate(k), oldHullPilot(k));
        EXPECT_EQ(st.anchors, oldAnchors(k));
        EXPECT_EQ(st.burrows, k == UK::scorpion);
        EXPECT_EQ(Pilot::jumps(k), k == UK::comet);
        EXPECT_EQ(PilotBuild::builds(k), k == UK::prospector);
        EXPECT_EQ(Pilot::drivable.contains(k), oldDrivable.contains(k));
        EXPECT_EQ(Leveling::machines.contains(k), oldMachines.contains(k));
        EXPECT_EQ(Leveling::bio.contains(k), oldBio.contains(k));
        // Bit for bit, anchored or not, near and far.
        for (bool anchored : {false, true}) {
            for (double d : {0.0, 0.37, 1.0, 2.5, 5.0, 7.3, 13.0, 21.9}) {
                EXPECT_EQ(Rules::flight(k, anchored, d), oldFlight(k, anchored, d));
            }
        }
    }
    EXPECT_TRUE(Leveling::machines == oldMachines);
    EXPECT_TRUE(Leveling::bio == oldBio);
    EXPECT_TRUE(Pilot::drivable == oldDrivable);
}

/// `Unit::brood`, `planted` and `burrowed` read the table.
TEST(kindTable_unitFlagsReadTheTable) {
    for (UK k : allCases<UK>()) {
        Unit u(1, k, 0, Vec2::zero, 0, Unit::Task::idle);
        EXPECT_EQ(u.brood(), k >= UK::shardling && k <= UK::matriarch);
        u.anchor = 1;
        u.anchored = true;
        EXPECT_EQ(u.planted(), oldAnchors(k));
        EXPECT_EQ(u.burrowed(), k == UK::scorpion);
    }
}

// The game layer reads the table at compile time.
static_assert(Rules::stats(UK::matriarch).brood && !Rules::stats(UK::scorpion).brood);
