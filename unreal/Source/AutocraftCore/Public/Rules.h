// Port of `Rules` and `UnitStats` of Sources/GameCore/State.swift, and the
// `Rules` extension of Vision.swift (sight).
#pragma once

#include "AutocraftCoreApi.h"

#include "SimdMath.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace ac {

/// `Unit.Kind`, declared here so `Rules` can take it; `Types.h` nests it as
/// `Unit::Kind`.
enum class UnitKind : uint8_t {
    prospector, ranger, comet, firefly, juggernaut, dropship, longbow, kestrel, hailstorm,
    // docs/new-units.md: appended. Saves store the name; only the wire
    // stores the index, so inserting a kind renumbers the ones after it there.
    peregrine, atlas, scorpion,
    // The Brood (docs/creatures.md): the MMO's bugs, on their own side.
    shardling, gloamwing, lodeback, matriarch
};
/// Its place in `allCases`: a case without a payload is stored as
/// its index in declaration order (`UnitTableTests` checks).
constexpr int64_t ordinal(UnitKind k) { return static_cast<int64_t>(k); }

/// The raw strings of an enum without payloads, in declaration order
/// (specialised for each enum; the rest are in `Types.h`).
template <class E> struct EnumInfo;
template <> struct EnumInfo<UnitKind> {
    static constexpr std::array<std::string_view, 16> names{
        "prospector", "ranger", "comet", "firefly", "juggernaut", "dropship", "longbow", "kestrel", "hailstorm",
        "peregrine", "atlas", "scorpion", "shardling", "gloamwing", "lodeback", "matriarch"};
    static constexpr size_t count = names.size();
};
// The last kind: move it when you append.
static_assert(EnumInfo<UnitKind>::count == size_t(UnitKind::matriarch) + 1);

/// `Structure.Kind`, declared here so `Rules` can take it; `Types.h` nests it
/// as `Structure::Kind`.
enum class StructureKind : uint8_t {
    citadel, habDome, garrison, bastion, derrick, foundry, spacedock, lab, sentinel,
    // D2 (docs/mmorpg-m8.md): the MMO's gatling gun. Appended: saves store
    // the name, the wire the index (4 bits).
    gatling,
    // F2: the Landing Pad, where the Dropship sets down (docs/mmorpg-m8.md).
    landingPad,
    // C2: the Target Dummy, to practise on (docs/guns-and-practice.md section 3).
    dummy
};

/// One unit type's numbers (in real seconds; ranges edge to edge).
struct UnitStats {
    double hp = 0;
    double armor = 0;
    double speed = 0;
    double radius = 0;
    int64_t supply = 0;
    int64_t ore = 0;
    int64_t hydrogen = 0;
    double trainTime = 0;
    /// Damage per hit, hits per attack, and the extra per hit against
    /// light or armoured targets.
    double damage = 0;
    int64_t hits = 1;
    double bonusVsLight = 0;
    double bonusVsArmored = 0;
    double cooldown = 1;
    double range = 0;
    /// Closest it can shoot (an anchored Longbow cannot fire at its own feet).
    double minRange = 0;
    bool light = false;
    bool armored = false;
    bool bio = false;
    bool air = false;
    bool hitsGround = false;
    bool hitsAir = false;
    /// A weapon that fires in bursts: it fires for `burst` seconds, then
    /// pauses `pause` seconds to reload (0: no bursts).
    double burst = 0;
    double pause = 0;
    /// A rail shot: hitscan, with its own cooldown (`Unit::railCooldown`).
    bool rail = false;
    /// The spread cone, radians (`Simulation::cone`): its radius standing
    /// still, the extra at full speed, the extra each round adds, and the
    /// seconds that extra takes to fade by 90%. Every round leaves along the
    /// sight turned by a random angle inside it (docs/shooter-feel.md).
    double coneStand = 0.01;
    double coneMove = 0.03;
    double coneFire = 0;
    double coneRecovery = 0.3;
    /// Past `range` a round flies on and keeps this share of its damage for
    /// every further `range` it flies; 0: it stops at `range` (a flame, a
    /// cutter, a sting).
    double falloff = 0.7;
    /// Rounds an attack is split into: the damage, less armour, shared
    /// between them, fired `cooldown / rounds` apart. The same damage a
    /// second, in smaller, faster rounds.
    int64_t rounds = 1;
    /// How far it reaches an air target, when that is shorter than `range`
    /// (a gatling gun's close air reach); 0: `range`.
    double airRange = 0;

    // The kind table (docs/kind-registry.md): what a kind is beyond its
    // numbers, read from `byKind` (the weapon rows below leave them at
    // their defaults, but for the anchored shell's flight).
    /// Its name as the game writes it (docs/naming.md).
    std::string_view title;
    /// A bug of the Brood (docs/creatures.md): never trained, never driven.
    bool brood = false;
    /// A Dropship takes it aboard (`Rules::boards`). `bio` is what a
    /// Dropship heals and a pilot walks with.
    bool boards = false;
    /// What a Prospector mends (`Leveling::machines`); a player kind that is
    /// not a machine is bio (`Leveling::bio`).
    bool machine = false;
    /// The player can take it over (`Pilot::drivable`).
    bool drivable = false;
    /// Dropship cargo slots it takes (`Rules::slots`).
    int64_t cargoSlots = 1;
    /// It needs a Lab on its building (`Rules::needsLab`).
    bool needsLab = false;
    /// The building that trains it (`Rules::trains`); none for the Brood.
    std::optional<StructureKind> trainedBy;
    /// How far it sees, its sight radius (`Rules::vision`): four cells more
    /// than the Swift game's, for guns whose rounds fly on
    /// (docs/shooter-feel.md). The Brood hunt by their site, not by sight
    /// (the director, Brood.h).
    double vision = 0;
    /// How long its attack takes to land on a target `d` away
    /// (`Rules::flight`): `shotFlightBase`, plus `d × shotFlightPerCell`,
    /// plus `d / shotSpeed` (cells a second; 0: none). Both forms are kept
    /// so every figure stays exactly as it was written.
    double shotFlightBase = 0;
    double shotFlightPerCell = 0;
    double shotSpeed = 0;
    /// A turret (a tank's, a Firefly's tail, a Hailstorm's flak mount, an
    /// Atlas's torso) that traverses after the target, not the hull, and
    /// how fast, radians a second: when the AI fights with it, and when
    /// the player drives it (`Pilot::turretRate`).
    bool turret = false;
    double turretRateAi = 0;
    double turretRatePilot = 0;
    /// Driven as a vehicle: W/S drive it, A/D turn its hull (`Pilot::steers`).
    bool steers = false;
    /// How fast its hull turns, radians a second, when it walks
    /// (`Simulation::move`; 0: `Rules::prospectorTurnRate`) and when the
    /// player turns it (`Pilot::hullTurnRate`; 0: 2).
    double hullTurnRateAi = 0;
    double hullTurnRatePilot = 0;
    /// It anchors (a Longbow) or buries (a Scorpion, which `burrows` too)
    /// in place (`Unit::planted`); a Comet `jumps` cliffs; a Prospector
    /// `builds`.
    bool anchors = false;
    bool burrows = false;
    bool jumps = false;
    bool builds = false;
    /// The AI trains it from its building after its own picks
    /// (`Commander`'s training chain), for a kind with no hand-written
    /// place there yet. False for every kind the chain names.
    bool aiTrains = false;

    /// `table` in `Unit.Kind.allCases` order, read by `Unit.Kind.ordinal`:
    /// a dictionary keyed by the kind hashes its name on every look-up, and
    /// the simulation looks up stats hundreds of thousands of times a step.
    /// (Swift's `table` dictionary is this array here.)
    static const std::array<UnitStats, EnumInfo<UnitKind>::count> byKind;
    /// A Ranger's mini gun (the Mini gun upgrade): 4 a round, five rounds a
    /// second for 3 s, then a 1 s pause to reload. About 15 damage a second
    /// against the rifle's 10.
    static const UnitStats minigun;
    /// An anchored Longbow's shock cannon: 40 (+30 armoured), splash, 13 range.
    static const UnitStats anchor;
    /// A Sentinel's missile pods: 12 ×2 at the air only, range 7 from its edge.
    static const UnitStats sentinel;
    /// A Kestrel's rail gun, at the air only: one hitscan slug, 34 (+14 armoured)
    /// every 3.2 s, range 8. About 11 (+4) a second against the Hailstorm's 12
    /// (+8) and the Sentinel's 24 a second, but a Kestrel is one gun, not a battery.
    static const UnitStats railgun;
    /// D2, a gatling gun's amalgam rounds: 4 every 0.25 s (16 a second) at
    /// ground targets in range 6, and at the air within 4.5 cells (it
    /// outreaches a Gloamwing's spit, 4).
    static const UnitStats gatling;
    /// D1, one of a Citadel's four corner turrets in a world (a Citadel
    /// fires only when the server arms it, `Structure::marks`): rapid rotary
    /// close-in guns, 2.5 every 0.25 s (10 a second each) at ground and air
    /// in range 7.5 from the turret, before the marks.
    static const UnitStats citadelGuns;

    bool operator==(const UnitStats&) const = default;
};

/// An axis-aligned box of a building's solid parts (C1,
/// docs/citadel-interior.md section 2), cells in the building's own frame
/// (relative to `Structure::position`), `z0` to `z1` up from its ground.
struct Slab {
    Vec2 min, max;
    double z0 = 0, z1 = 0;
    /// `local` is on it seen from above, the slab grown by `grow` on every side.
    bool contains(Vec2 local, double grow = 0) const {
        return local.x > min.x - grow && local.x < max.x + grow && local.y > min.y - grow && local.y < max.y + grow;
    }
    /// How far `local` is from it seen from above (0 on it).
    double distance(Vec2 local) const {
        const double dx = local.x < min.x ? min.x - local.x : local.x > max.x ? local.x - max.x : 0;
        const double dy = local.y < min.y ? min.y - local.y : local.y > max.y ? local.y - max.y : 0;
        return std::sqrt(dx * dx + dy * dy);
    }
    bool operator==(const Slab&) const = default;
};

/// A kind's solid parts (`Rules::footprint`): one truth for the pilot's
/// walk, the cameras' sweep and the shelter. Every kind but the Citadel is
/// one wall slab of its whole square; the Citadel is a hollow hall with a
/// door in its +y face.
struct AUTOCRAFTCORE_API Footprint {
    /// The bounding half side (= `Rules::radius`).
    double radius = 0;
    /// Solid, always.
    std::vector<Slab> walls;
    /// Solid while the door is not open (`Simulation::doorOpen`).
    std::optional<Slab> door;
    /// The hall: inside it a unit is sheltered.
    std::optional<Slab> interior;
    /// Over the interior (the cameras' ceiling).
    std::optional<Slab> roof;
    /// `local` is on a wall standing on the ground (the lintel is overhead),
    /// or on the door while it is shut, each grown by `grow`: where a
    /// walker can't stand.
    bool blocks(Vec2 local, bool doorOpen, double grow) const;
    /// `local` is in the interior (the doorway is not).
    bool inside(Vec2 local) const;
};

/// The game's numbers, in real seconds.
struct AUTOCRAFTCORE_API Rules {
    /// Prospector move speed, cells per second.
    static constexpr double prospectorSpeed = 3.94;
    /// Prospector turn rate, radians per second.
    static constexpr double prospectorTurnRate = 14.0;
    /// Time a Prospector drills a patch before it carries a load away, three
    /// times a brisk 2 s: the wallpaper lingers on the drilling, so trips take
    /// longer and the stockpile grows about half as fast.
    static constexpr double miningTime = 6.0;
    /// Ore per trip.
    static constexpr int64_t carry = 5;
    /// How far from a Citadel's centre a returning Prospector stops, and a
    /// new one starts: half a cell past its edge (3 for the 5x5; C1, the
    /// 12x12's follows its side).
    static double depositRange() { return radius(StructureKind::citadel) + 0.5; }
    /// Distance from a patch centre at which a Prospector mines it.
    static constexpr double mineRange = 1.2;
    static constexpr int64_t startingOre = 50;
    /// The hard supply limit.
    static constexpr int64_t maxSupply = 200;
    /// A dry patch grows back this long after its last load (the
    /// wallpaper never runs out, and regrown fields are worth fighting over).
    static constexpr double regrowDelay = 300.0;

    static constexpr int64_t prospectorCost = 50;
    static constexpr double prospectorTrainTime = 12.0;
    static constexpr int64_t rangerCost = 50;
    static constexpr double rangerTrainTime = 18.0;
    /// Ranger move speed, cells per second.
    static constexpr double rangerSpeed = 3.15;
    /// Production queue length of one building.
    static constexpr int64_t maxQueue = 5;

    // Combat.
    /// Ranger coil rifle: damage per shot, seconds between shots, range
    /// (edge to edge, cells) and how far a Ranger sees to pick targets.
    static constexpr double rangerDamage = 6.0;
    static constexpr double rangerCooldown = 0.61;
    static constexpr double rangerRange = 5.0;
    static constexpr double sight = 13.0;
    /// An AI soldier opens fire out to this many times its weapon's range
    /// (a weapon whose rounds fly on, `UnitStats::falloff`), closing in as it
    /// shoots: long shots land less often and do less.
    static constexpr double engageReach = 1.6;
    /// Unit footprint radius (Rangers and Prospectors), for range checks.
    static constexpr double unitRadius = 0.375;
    /// Rangers a Bastion holds, and the range they gain inside it.
    static constexpr int64_t bastionCapacity = 4;
    static constexpr double bastionRangeBonus = 1.0;
    /// Extra range for a shooter standing on a higher level than its
    /// target (a Ranger on a plateau, or a Bastion on one, firing down).
    static constexpr double highGroundRangeBonus = 1.0;
    /// Seconds a won game stays on screen before a new one starts.
    static constexpr double victoryLap = 30.0;

    /// A Comet heals this many hp a second once it has gone
    /// `cometRegenDelay` seconds without being hit.
    static constexpr double cometRegen = 2.8;
    static constexpr double cometRegenDelay = 5.0;
    /// A Longbow takes this long to anchor up or pack up.
    static constexpr double anchorTime = 2.7;
    /// A tank with nothing to shoot anchored for this long packs up (to move
    /// on, or closer).
    static constexpr double anchorIdle = 2.0;
    /// One ring of a splash: Swift's `(radius: Double, factor: Double)`.
    struct Ring { double radius, factor; };
    /// An anchored shell's splash: full damage out to the first radius, then
    /// half, then a quarter. It hurts everyone there, friends too.
    static constexpr std::array<Ring, 3> splash{{{0.47, 1}, {0.78, 0.5}, {1.25, 0.25}}};
    /// A Hailstorm's flak burst: full damage to the air target it bursts on,
    /// then half to enemy flyers within about a cell of it (edge to centre).
    /// Only enemies: it bursts in the sky, away from its own side.
    static constexpr std::array<Ring, 2> flakSplash{{{0.5, 1}, {1.0, 0.5}}};
    /// Half-width of the line a Firefly's flame burns.
    static constexpr double flameWidth = 0.4;
    /// How long an attack takes to land on a target `distance` away
    /// (centre to centre): a Juggernaut's grenade and an anchored shell fly, a
    /// Firefly's jet licks out; coil rounds, pistols and the 90 mm guns hit
    /// at once. The damage lands then, and the scene flies its rounds for
    /// exactly this long.
    static double flight(UnitKind kind, bool anchored, double d);
    /// An Atlas's cannon shell: full damage out to the first radius, then half.
    /// Only enemies, and only on the ground: the Atlas fights in the front line.
    static constexpr std::array<Ring, 2> atlasSplash{{{0.5, 1}, {1.0, 0.5}}};
    /// A Scorpion's sting: full damage to the target, half to enemies (ground
    /// or air) within 1.25 cells; never its own side.
    static constexpr std::array<Ring, 2> scorpionSplash{{{0.5, 1}, {1.25, 0.5}}};
    /// An Atlas's Quake stomp: damage to every enemy on the ground within
    /// `stompRadius` (edge to the Atlas's centre), which also slows it
    /// (`slowTime`), every `stompCooldown` s. Fired by itself when
    /// `stompCrowd` or more stand that close.
    static constexpr double stompDamage = 30.0;
    static constexpr double stompRadius = 2.0;
    static constexpr double stompCooldown = 20.0;
    static constexpr int64_t stompCrowd = 3;
    /// An Atlas's torso (its cannons) and legs (the whole body) turn this
    /// fast, radians a second.
    static constexpr double atlasTorsoRate = 3.0;
    static constexpr double atlasBodyRate = 1.5;
    /// A Scorpion buries and digs out in this long (`anchorTime` is a Longbow's).
    static constexpr double buryTime = 2.0;
    /// Seconds a buried Scorpion needs to lock onto a victim before it stings.
    static constexpr double scorpionLock = 1.0;
    /// A buried Scorpion is seen by enemy units and buildings this close,
    /// and by an enemy Sentinel's whole sight; it is seen by all for
    /// `strikeReveal` s after a strike (and, as it reloads, throughout).
    static constexpr double revealRange = 2.0;
    static constexpr double strikeReveal = 3.0;
    /// A Juggernaut's grenade slows what it hits to half speed for this long.
    static constexpr double slowTime = 1.07;
    /// Dropship: hp healed a second, reach, and energy (1 per 3 hp healed,
    /// regrowing 0.7875 a second, 200 at most).
    static constexpr double healRate = 12.6;
    static constexpr double healRange = 4.0;
    static constexpr double maxEnergy = 200.0;
    static constexpr double energyRegen = 0.7875;
    /// Cargo slots of a Dropship, and how many each unit takes.
    static constexpr int64_t dropshipSlots = 8;
    /// The unit the player drives is a hero (why else take the
    /// wheel): its weapon hits harder and reaches farther, it moves faster,
    /// a Prospector carries five loads a trip and builds and repairs twice as
    /// fast. Only while driven. These are level 1 of pilot leveling
    /// (`Leveling.hero`); the sim reads them through `Simulation.boost`,
    /// which adds the picks on top.
    static constexpr double heroDamage = 1.5;
    static constexpr double heroRange = 1.25;
    static constexpr double heroSpeed = 1.2;
    static constexpr int64_t heroCarry = 25;
    static constexpr int64_t heroHydrogenCarry = 20;
    static constexpr double heroWork = 2.0;
    /// What a Dropship takes aboard: the infantry (bio units), and the
    /// Scorpion, which the harass AI carries to the enemy's ore lines. An
    /// Atlas would need more slots than any Dropship has.
    static bool boards(UnitKind k) { return stats(k).boards; }
    /// An Atlas takes more slots than any Dropship has (even with Cargo bay).
    static int64_t slots(UnitKind k) { return stats(k).cargoSlots; }

    static int64_t cost(StructureKind k);
    static int64_t hydrogenCost(StructureKind k);
    /// Built with no blueprint, at no level, by anyone (C2): the Target
    /// Dummy only (`PilotProgress::buildRefusal` skips the blueprint and the
    /// floor for it).
    static bool freeBuild(StructureKind k) { return k == StructureKind::dummy; }
    static double buildTime(StructureKind k);
    static int64_t supply(StructureKind k);
    /// The Citadel's hall (C1, docs/citadel-interior.md section 1): on, a
    /// Citadel is 12x12 cells, hollow, with a door and manned corner guns
    /// (the MMO); off, the strategy game's solid 5x5 one. One switch for the
    /// process, set at every game start: the MMO's start and the server turn
    /// it on, the strategy game's start turns it off. `radius`, `footprint`,
    /// `height`, `Defence::cornerInset` and the gun stations all read it.
    static void setCitadelHall(bool on);
    static bool citadelHall();
    /// Half the footprint side: 12x12 Citadel with the hall (5x5 without);
    /// 3x3 Garrison, Bastion, Derrick, Foundry, Spacedock and Landing Pad;
    /// 2x2 Hab Dome, Lab, Sentinel and gatling gun.
    static double radius(StructureKind k);
    /// Its solid parts: walls, the door, the hall, the roof (C1).
    static const Footprint& footprint(StructureKind k);
    /// How tall its body stands, cells, its highest point (the game's model
    /// and pose; a hall Citadel's deck is at `Defence::deckHeight` 7, its
    /// corner towers' mounts at `Defence::mountHeight` 9, the central tower's
    /// beacon at 12.3).
    static double height(StructureKind k);
    /// The Brood never takes a Citadel below this share of its hit points
    /// (`Simulation::hit`): the worst a raid leaves is a wreck to repair.
    static constexpr double citadelBugFloor = 0.10;
    /// A Citadel's door runs from shut to open (and back) in this long.
    static constexpr double doorSeconds = 2.5;
    /// A pilot opens the door standing within this many cells of the door
    /// slab, on either side.
    static constexpr double doorReach = 2.0;
    /// Units walk over it (F2: a Landing Pad is a deck on the ground, the
    /// Prospector stands on it to call the Dropship and steps out onto it).
    static bool walkable(StructureKind k) { return k == StructureKind::landingPad; }
    /// A building needs one of these finished first (the tech tree).
    /// (`requires` is a C++ keyword.)
    static std::optional<StructureKind> requires_(StructureKind k);
    /// The units a building trains, the ones needing a Lab included.
    static std::vector<UnitKind> trains(StructureKind k);
    /// The unit a building trains when none is named (the first it trains).
    static std::optional<UnitKind> produces(StructureKind k);
    /// Units that need a Lab on their building (an add-on).
    static bool needsLab(UnitKind k) { return stats(k).needsLab; }
    /// Where an add-on stands: on the building's +X side, toward the camera.
    static constexpr Vec2 addonOffset{2.5, 0.5};
    /// MH a well holds, taken 4 per trip, 3 Prospectors per Derrick.
    static constexpr int64_t wellHydrogen = 2250;
    static constexpr int64_t hydrogenCarry = 4;
    static constexpr int64_t workersPerWell = 3;
    /// Seconds a Prospector spends inside a Derrick per trip: 1.4 s slowed
    /// like the ore drilling.
    static constexpr double hydrogenTime = 3.5;

    static constexpr const UnitStats& stats(UnitKind k) { return UnitStats::byKind[static_cast<size_t>(ordinal(k))]; }
    static int64_t cost(UnitKind k) { return stats(k).ore; }
    static int64_t hydrogenCost(UnitKind k) { return stats(k).hydrogen; }
    static double trainTime(UnitKind k) { return stats(k).trainTime; }
    static double speed(UnitKind k) { return stats(k).speed; }
    static double hp(UnitKind k) { return stats(k).hp; }
    static int64_t supply(UnitKind k) { return stats(k).supply; }
    static double radius(UnitKind k) { return stats(k).radius; }
    static double hp(StructureKind k);
    /// Hp as a card or a bar's label writes it: whole, never past `max`
    /// (the snapshot's quantised hp can come a hair over), and never 0 while
    /// any is left.
    static int64_t hpShown(double hp, double max);
    /// The weapon of a building that shoots by itself (a Sentinel, a gatling
    /// gun), if any, before its marks (`Defence::weapon`).
    static std::optional<UnitStats> turret(StructureKind k);
    /// Damage a Ranger shot does after armour: buildings have 1 armour,
    /// and a hit always does at least 0.5.
    static double damage(bool toStructure);
    /// Damage one attack of `attacker` does to a target with these traits:
    /// per hit, the base plus any bonus against its attribute, less its
    /// armour (at least 0.5), times the hits. (Swift's `anchored` defaults
    /// to false.)
    static double damage(UnitKind attacker, bool anchored, bool armored, bool light, double armor);
    /// The same for weapon `a`.
    static double damage(const UnitStats& a, bool armored, bool light, double armor);

    // From Vision.swift.
    /// How far each kind sees (sight radii).
    static double vision(UnitKind k) { return stats(k).vision; }
    static double vision(StructureKind k);
    /// The day, as the game layer draws it (AAcDaylight): `dayLength`
    /// seconds of game time, a game starting at `startHour`, the sun up
    /// from `sunrise` to `sunset`.
    static constexpr double dayLength = 600.0;
    static constexpr double startHour = 8.0;
    static constexpr double sunrise = 5.5;
    static constexpr double sunset = 18.5;
    /// The hour (0-24) `time` seconds into a game whose day is `day`
    /// seconds long; `held` holds the clock at an hour.
    static double hourAt(double time, double day = dayLength, std::optional<double> held = std::nullopt);
    /// How high the sun stands at `hour`, degrees (below 0 at night).
    static double sunHeight(double hour);
    /// How dark it is at `hour`: 0 with the sun up, 1 soon after sunset
    /// (the lamps come on with it; sight closes in with it).
    static double dark(double hour);
    /// How far everything sees at full night, a share of its day sight.
    static constexpr double nightSight = 0.5;
    /// What every sight radius is multiplied by at `hour`: 1 by day,
    /// `nightSight` at night, eased through dusk and dawn.
    static double sightScale(double hour) { return sightScale(hour, nightSight); }
    /// The same with `night` the share at full night (R2: the realm's Relay
    /// widens the pilots' to `nightSight` × 1.5).
    static double sightScale(double hour, double night) { return 1.0 - (1.0 - night) * dark(hour); }
    /// A driven Prospector's headlamp: its beam is this many degrees either
    /// side of where the crosshair is (`Pilot::heading`), and it sees along
    /// it as far as the Prospector sees by day, night or not. The game
    /// layer's spot light has the same cone.
    static constexpr double headlampCone = 22.5;
    /// A world's region by day (S3, "zones are not that big"): the pilots'
    /// sight reaches `span` (the whole region) with the sun up and closes in
    /// through dusk to `night`, its sight at night (`sightScale`, the
    /// headlamp's wedge on top), by the same share at every step of
    /// `dark` (geometric: half dark is the middle of the two on a log
    /// scale, 6 to 180 cells meets at 33). Back up at dawn the same way.
    static double daylightReach(double hour, double night, double span);
    /// A ground unit this close to a watchtower's centre holds it, and sees
    /// `towerSight` round the tower (22).
    static constexpr double towerReach = 2.6;
    static constexpr double towerSight = 22.0;
    /// How long a player keeps an enemy unit it has lost sight of in mind.
    static constexpr double memory = 60.0;
    /// How long a unit shot from out of its side's sight goes for where the
    /// shot came from.
    static constexpr double unseenChase = 3.0;
    /// A fighter on a follow order engages the enemy flyers within this
    /// many cells of the unit it follows (centre to centre), and holds its
    /// formation place about `escortGap` cells beside it.
    static constexpr double escortReach = 7.0;
    static constexpr double escortGap = 1.6;

    // Rounds (C2, docs/guns-and-practice.md section 1): the minigun class's
    // real projectiles (`Round`, `Simulation::stepRounds`). Tuned by feel.
    /// Muzzle speed, cells a second: at 30 cells a round takes 0.33 s and
    /// drops 0.55 cells.
    static constexpr double roundSpeed = 90.0;
    /// Its fall, cells a second squared.
    static constexpr double roundGravity = 9.8;
    /// Seconds a round flies before it is gone.
    static constexpr double roundLife = 3.0;
    /// The hit sweep's thickness, cells.
    static constexpr double roundRadius = 0.08;
    /// A round passes through its own shooter's building this long after it
    /// left the muzzle.
    static constexpr double roundSelfGrace = 0.2;
    /// Where a round is `t` seconds after it left `muzzle` at `velocity`: the
    /// parabola under `roundGravity`. The core steps rounds by this same
    /// closed form, so the client's streak flies the core's path at any
    /// frame rate.
    static Vec3 roundAt(Vec3 muzzle, Vec3 velocity, double t) {
        return {muzzle.x + velocity.x * t, muzzle.y + velocity.y * t,
                muzzle.z + velocity.z * t - 0.5 * roundGravity * t * t};
    }
    /// A round's velocity leaving along `yaw` (world radians) and `pitch`
    /// (up from level): `roundSpeed` along that way.
    static Vec3 roundVelocity(double yaw, double pitch);
    /// The low-arc pitch that lands a round at `roundSpeed` on a point
    /// `ahead` cells away along the ground and `rise` cells up from the
    /// muzzle; nil when it is out of the round's reach.
    static std::optional<double> ballisticPitch(double ahead, double rise);
    /// The minigun class's barrels and heat (a Citadel's corner gun, a
    /// gatling gun): from the trigger to full rate in `gunSpinUp` s (the rate
    /// ramps linearly with the barrels' spin); each round heats the gun by
    /// `gunHeatPerRound`, it cools `gunCooling` a second, and at 1 it stops
    /// (OVERHEAT) until it has cooled to `gunHeatResume`.
    static constexpr double gunSpinUp = 0.4;
    static constexpr double gunHeatPerRound = 0.012;
    static constexpr double gunCooling = 0.35;
    static constexpr double gunHeatResume = 0.4;
    /// A Target Dummy at 0 hp lies down this long, then stands with full hp.
    static constexpr double dummyDownTime = 3.0;
};

inline constexpr std::array<UnitStats, EnumInfo<UnitKind>::count> UnitStats::byKind{{
    // Fusion cutter: 5 a hit, in melee reach. It fights only when it
    // has to (hit, or driven by the player).
    /* prospector */ {.hp = 45, .speed = Rules::prospectorSpeed, .radius = 0.375, .supply = 1, .ore = 50, .trainTime = 12,
                      .damage = 5, .cooldown = 1.07, .range = 0.1, .light = true, .bio = true, .hitsGround = true,
                      .falloff = 0,
                      .title = "Prospector", .boards = true, .machine = true, .drivable = true, .trainedBy = StructureKind::citadel, .vision = 12, .builds = true},
    /* ranger */ {.hp = 45, .speed = 3.15, .radius = 0.375, .supply = 1, .ore = 50, .trainTime = 18,
                  .damage = 6, .cooldown = 0.61, .range = 5, .light = true, .bio = true, .hitsGround = true, .hitsAir = true,
                  .coneStand = 0.003, .coneMove = 0.12, .coneFire = 0.004, .coneRecovery = 0.35, .falloff = 0.9, .rounds = 3,
                  .title = "Ranger", .boards = true, .drivable = true, .trainedBy = StructureKind::garrison, .vision = 13},
    // Twin pistols, 4 (+5 light) each; heals out of combat; jumps cliffs.
    /* comet */ {.hp = 60, .speed = 5.25, .radius = 0.375, .supply = 1, .ore = 50, .hydrogen = 50, .trainTime = 32,
                 .damage = 4, .hits = 2, .bonusVsLight = 5, .cooldown = 0.79, .range = 5, .light = true, .bio = true,
                 .hitsGround = true,
                 .title = "Comet", .boards = true, .drivable = true, .trainedBy = StructureKind::garrison, .vision = 13, .jumps = true},
    // Flamethrower, 8 (+6 light) to everything in a line.
    /* firefly */ {.hp = 90, .speed = 5.95, .radius = 0.625, .supply = 2, .ore = 100, .trainTime = 21,
                   .damage = 8, .bonusVsLight = 6, .cooldown = 1.79, .range = 5, .light = true, .hitsGround = true, .falloff = 0,
                   .title = "Firefly", .machine = true, .drivable = true, .trainedBy = StructureKind::foundry, .vision = 14, .shotFlightBase = 0.09, .turret = true, .turretRateAi = 6, .turretRatePilot = 3.5, .steers = true, .hullTurnRateAi = 9, .hullTurnRatePilot = 3},
    // Breacher grenades, 10 (+10 armoured); each hit slows.
    /* juggernaut */ {.hp = 125, .armor = 1, .speed = 3.15, .radius = 0.5625, .supply = 2, .ore = 100, .hydrogen = 25,
                      .trainTime = 21, .damage = 10, .bonusVsArmored = 10, .cooldown = 1.07, .range = 6, .armored = true,
                      .bio = true, .hitsGround = true,
                      .title = "Juggernaut", .boards = true, .drivable = true, .cargoSlots = 2, .needsLab = true, .trainedBy = StructureKind::garrison, .vision = 14, .shotFlightBase = 0.15, .shotSpeed = 14},
    /* dropship */ {.hp = 150, .armor = 1, .speed = 3.5, .radius = 0.75, .supply = 2, .ore = 100, .hydrogen = 100,
                    .trainTime = 30, .armored = true, .air = true,
                    .title = "Dropship", .machine = true, .drivable = true, .trainedBy = StructureKind::spacedock, .vision = 15},
    // Tank mode; `anchor` holds the anchored numbers.
    /* longbow */ {.hp = 175, .armor = 1, .speed = 3.15, .radius = 0.875, .supply = 3, .ore = 150, .hydrogen = 125,
                   .trainTime = 32, .damage = 15, .bonusVsArmored = 10, .cooldown = 1.04, .range = 7, .armored = true,
                   .hitsGround = true,
                   .title = "Longbow", .machine = true, .drivable = true, .cargoSlots = 4, .needsLab = true, .trainedBy = StructureKind::foundry, .vision = 15, .turret = true, .turretRateAi = 4, .turretRatePilot = 2.4, .steers = true, .hullTurnRateAi = 5, .anchors = true},
    // Twin rockets, 12 ×2, at the ground (`hitsGround`), and a rail gun at
    // the air (`UnitStats::railgun`, picked by `Simulation::weapon(u, air)`);
    // it flies over cliffs like the Dropship.
    /* kestrel */ {.hp = 140, .speed = 3.85, .radius = 0.625, .supply = 3, .ore = 150, .hydrogen = 100, .trainTime = 43,
                   .damage = 12, .hits = 2, .cooldown = 1.25, .range = 6, .armored = true, .air = true, .hitsGround = true,
                   .hitsAir = true,
                   .title = "Kestrel", .machine = true, .drivable = true, .needsLab = true, .trainedBy = StructureKind::spacedock, .vision = 14, .shotFlightBase = 0.12, .shotSpeed = 22},
    // Flak, 12 (+8 armoured), at the air only, splashing enemy flyers
    // round its target (`Rules.flakSplash`).
    /* hailstorm */ {.hp = 135, .armor = 1, .speed = 3.15, .radius = 0.75, .supply = 2, .ore = 125, .hydrogen = 50,
                     .trainTime = 30, .damage = 12, .bonusVsArmored = 8, .cooldown = 1.0, .range = 7, .armored = true,
                     .hitsAir = true,
                     .title = "Hailstorm", .machine = true, .drivable = true, .trainedBy = StructureKind::foundry, .vision = 15, .shotFlightBase = 0.1, .shotSpeed = 28, .turret = true, .turretRateAi = 5, .turretRatePilot = 3.2, .steers = true, .hullTurnRateAi = 6},
    // Seeker missiles, 10 ×2 (+6 armoured), at the air only; a light flyer.
    /* peregrine */ {.hp = 80, .speed = 5.6, .radius = 0.5, .supply = 2, .ore = 100, .hydrogen = 75, .trainTime = 28,
                     .damage = 10, .hits = 2, .bonusVsArmored = 6, .cooldown = 1.25, .range = 7, .light = true, .air = true,
                     .hitsAir = true,
                     .title = "Peregrine", .machine = true, .drivable = true, .trainedBy = StructureKind::spacedock, .vision = 15, .shotFlightBase = 0.1, .shotSpeed = 30},
    // Twin siege cannons, 25 ×2 (+15 armoured), at the ground only, with
    // splash (`Rules::atlasSplash`); the Quake stomp is `Rules::stomp*`.
    /* atlas */ {.hp = 500, .armor = 2, .speed = 2.2, .radius = 1.25, .supply = 6, .ore = 300, .hydrogen = 200,
                 .trainTime = 60, .damage = 25, .hits = 2, .bonusVsArmored = 15, .cooldown = 2.0, .range = 7, .armored = true,
                 .hitsGround = true,
                 .title = "Atlas", .machine = true, .drivable = true, .cargoSlots = 16, .needsLab = true, .trainedBy = StructureKind::foundry, .vision = 15, .turret = true, .turretRateAi = Rules::atlasTorsoRate, .turretRatePilot = Rules::atlasTorsoRate, .steers = true, .hullTurnRateAi = Rules::atlasBodyRate, .hullTurnRatePilot = Rules::atlasBodyRate},
    // Sting, 60 (+30 armoured), ground and air, but only buried: `cooldown`
    // is its reload (it ticks while the Scorpion moves, too).
    /* scorpion */ {.hp = 90, .speed = 3.9, .radius = 0.5, .supply = 2, .ore = 75, .hydrogen = 25, .trainTime = 21,
                    .damage = 60, .bonusVsArmored = 30, .cooldown = 25, .range = 5, .light = true, .hitsGround = true,
                    .hitsAir = true, .falloff = 0,
                    .title = "Scorpion", .boards = true, .machine = true, .drivable = true, .trainedBy = StructureKind::foundry, .vision = 12, .shotFlightBase = 0.2, .anchors = true, .burrows = true},
    // The Brood (docs/creatures.md; `Brood` has the rest of their numbers).
    // No cost: a bug is never trained. `speed` is the top speed (a
    // Shardling's chase); `Bug::pace` slows it to a wander.
    // A Shardling's bite: 2 to a unit, every 1.2 s, 0.3 past its radius.
    /* shardling */ {.hp = 20, .speed = 4.4, .radius = 0.45, .damage = 2, .cooldown = 1.2, .range = 0.3, .light = true,
                     .bio = true, .hitsGround = true, .falloff = 0,
                     .title = "Shardling", .brood = true, .boards = true, .vision = 8},
    // A Gloamwing's acid spit: 3 to a unit, every 1.6 s, range 4; in the
    // air but for its dive (`Unit::airborne`).
    /* gloamwing */ {.hp = 30, .speed = 5.0, .radius = 0.5, .damage = 3, .cooldown = 1.6, .range = 4, .light = true,
                     .bio = true, .air = true, .hitsGround = true, .falloff = 0,
                     .title = "Gloamwing", .brood = true, .boards = true, .vision = 8},
    // A Lodeback's bite, only when cornered: 8 every 2 s.
    /* lodeback */ {.hp = 120, .armor = 1, .speed = 3.0, .radius = 1.0, .damage = 8, .cooldown = 2.0, .range = 0.4,
                    .bio = true, .hitsGround = true, .falloff = 0,
                    .title = "Lodeback", .brood = true, .boards = true, .vision = 8},
    // The Matriarch's slam: 40 to the ground in front of her, every 4 s (N3).
    /* matriarch */ {.hp = 6000, .armor = 3, .speed = 0.8, .radius = 3.5, .damage = 40, .cooldown = 4.0, .range = 3,
                     .armored = true, .bio = true, .hitsGround = true, .hitsAir = true, .falloff = 0,
                     .title = "the Matriarch", .brood = true, .boards = true, .vision = 25},
}};

inline constexpr UnitStats UnitStats::minigun{.hp = 45, .speed = 3.15, .radius = 0.375, .supply = 1, .ore = 50, .trainTime = 18,
                                              .damage = 4, .cooldown = 0.2, .range = 5, .light = true, .bio = true, .hitsGround = true,
                                              .hitsAir = true, .burst = 3, .pause = 1, .coneStand = 0.04, .coneMove = 0.12,
                                              .coneFire = 0.006, .coneRecovery = 0.5, .falloff = 0.5};
inline constexpr UnitStats UnitStats::anchor{.hp = 175, .armor = 1, .speed = 0, .radius = 0.875, .supply = 3, .ore = 150, .hydrogen = 125,
                                             .trainTime = 32, .damage = 40, .bonusVsArmored = 30, .cooldown = 2.14, .range = 13,
                                             .minRange = 2, .armored = true, .hitsGround = true, .shotFlightBase = 0.3,
                                             .shotFlightPerCell = 0.035};
inline constexpr UnitStats UnitStats::railgun{.hp = 140, .speed = 3.85, .radius = 0.625, .supply = 3, .ore = 150, .hydrogen = 100, .trainTime = 43,
                                              .damage = 34, .bonusVsArmored = 14, .cooldown = 3.2, .range = 8, .armored = true, .air = true,
                                              .hitsAir = true, .rail = true};
inline constexpr UnitStats UnitStats::sentinel{.hp = 250, .armor = 1, .speed = 0, .radius = 1, .supply = 0, .ore = 100, .trainTime = 18,
                                               .damage = 12, .hits = 2, .cooldown = 1.0, .range = 7, .armored = true, .hitsAir = true};
inline constexpr UnitStats UnitStats::gatling{.hp = 300, .armor = 1, .speed = 0, .radius = 1, .supply = 0, .ore = 100, .trainTime = 18,
                                              .damage = 4, .cooldown = 0.25, .range = 6, .armored = true, .hitsGround = true,
                                              .hitsAir = true, .airRange = 4.5};
inline constexpr UnitStats UnitStats::citadelGuns{.hp = 1500, .armor = 1, .speed = 0, .radius = 0.5, .damage = 2.5, .cooldown = 0.25,
                                                  .range = 7.5, .armored = true, .hitsGround = true, .hitsAir = true};

} // namespace ac
