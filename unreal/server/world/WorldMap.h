// The server's world (docs/worlds.md): regions side by side on a grid, each
// a Region on its region file's wild ground, every one stepped every tick.
// A pilot is in one region at a time: it starts at the start pad driving a
// Prospector, drives across an edge's opening into the neighbour, or flies
// from a pad to a pad. Its wallet (ore and MH) and its levels go with it.
// Engine-free, like World.h.
#pragma once

#include "World.h"
#include "WorldStore.h"

#include "RegionFile.h"
#include "Weather.h"
#include "WorldClock.h"

#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace ac::server {

/// A world file: the grid of region files, the start.
struct WorldFile {
    std::string id;
    std::string name;
    /// Region file paths, as written (relative to the world file's folder);
    /// rows north to south, each west to east. Empty: no region there.
    std::vector<std::vector<std::string>> grid;
    std::string startRegion;
    std::string startPad;
};

/// One region of a world, loaded.
struct WorldRegion {
    std::string path;
    int64_t column = 0;
    int64_t row = 0;
    region::RegionFile file;
    /// The file's text, as the clients get it.
    std::string text;
    region::Built built;
};

struct LoadedWorld {
    WorldFile file;
    std::vector<WorldRegion> regions;
    /// Index in `regions`.
    size_t start = 0;
    size_t startPad = 0;
};

/// Reads a world file and every region file it names, and checks them: each
/// region file valid, at least one pad in each region, neighbours' openings
/// overlapping, the start region and pad there. Problems one a line; nil
/// with any.
struct WorldLoad {
    std::optional<LoadedWorld> world;
    std::vector<std::string> problems;
};
WorldLoad loadWorld(const std::string& path);
/// The same from text; region paths are read relative to `folder`.
WorldLoad loadWorld(std::string_view json, const std::string& folder);

/// A world's XP (docs/prospector-progression.md, "XP sources"), at ×1:
/// counted by the server from the work its regions' simulations did, and
/// the one-offs, paid once per pilot ever.
struct WorldXp {
    /// An ore and an MH unloaded.
    static constexpr double ore = 1;
    static constexpr double hydrogen = 2;
    /// Welding a building of the pilot's own: this × its cost (ore + MH) ×
    /// the share welded. Repairing one: this × its cost × the share of hp
    /// mended, only hp lost to something else, at most its welding XP a day.
    static constexpr double weld = 0.5;
    static constexpr double repair = 0.25;
    static constexpr double repairDay = 86400;
    /// The one-offs. The start region and its start pad pay nothing.
    static constexpr double region = 250;
    static constexpr double pad = 50;
    static constexpr double citadel = 150;
    static constexpr double building = 100;
    static constexpr double firstHydrogen = 50;
    static constexpr double firstFlight = 50;
    static constexpr double firstEmptied = 50;
    /// A blueprint found: this × its level floor; a survey chip (P4).
    static constexpr double blueprintPerFloor = 50;
    static constexpr double chip = 25;
    /// The feats' ids (`ProspectorProgress::feats`): "built-citadel" and
    /// one a building kind, "first-hydrogen", "first-flight", "first-emptied".
    static std::string built(StructureKind kind);
    static constexpr const char* hydrogenFeat = "first-hydrogen";
    static constexpr const char* flightFeat = "first-flight";
    static constexpr const char* emptiedFeat = "first-emptied";
    /// The XP a feat pays.
    static double feat(const std::string& id);
};

/// Blueprint sites and survey chips in a world (docs/prospector-progression.md,
/// "Blueprint sites"; the region files' `sites`). Each pilot opens each site
/// for itself, once, keyed "<region id>/<site name>".
struct WorldSites {
    /// A site is reached when the driven Prospector's centre is this close
    /// to its point (cells): the Prospector touching a crate or a wreck's
    /// hull.
    static constexpr double reach = 2.0;
    /// Seconds of the act key held (`PilotInput::hold`) within reach to open
    /// one: a crate opens on reaching it. Stepping out of reach or letting go
    /// starts over. A vault counts only at its blueprint's level floor (less
    /// Blueprint savant).
    static constexpr double wreck = 5;
    static constexpr double cache = 8;
    static constexpr double vault = 10;
    static double needs(SiteKind kind);
    /// A site goes on the pilot's map (`ProspectorProgress::known`) when its
    /// Prospector comes this close: a crate, a wreck or a vault within its
    /// sight, a buried cache within 6 cells; within Surveyor's radius
    /// whatever its kind. A Scanner pulse marks a region's sites of its kinds
    /// on arriving there; a survey chip marks the nearest one not known.
    static constexpr double seen = 12;
    static constexpr double cacheSeen = 6;
    /// At most one survey chip every this many seconds (the world's clock);
    /// one chip in this many is a blueprint, among those the pilot lacks
    /// whose floor is at most its level + `chipBlueprintReach`.
    static constexpr double chipEvery = 300;
    static constexpr int64_t chipBlueprintOneIn = 20;
    static constexpr int64_t chipBlueprintReach = 10;
};

/// KSTR contracts (docs/contracts.md): three short jobs per pilot from the
/// radio, drawn by the server's dice per pilot and world day, paid in XP and
/// MH, replaced at the world's dawn when done or abandoned.
struct WorldContracts {
    static constexpr size_t slots = 3;
    /// XP a minute of play at `level` (the pace model,
    /// docs/prospector-progression.md: 4,500 an hour at level 1, +2% a
    /// level).
    static double perMinute(int64_t level);
    /// Minutes of play a kind's reward is worth.
    static double minutes(net::Contract::Kind kind);
    /// What a contract of `kind` pays at `level`: XP rounded to 5, MH
    /// `10 + 2 × level` rounded to 5.
    static double xp(net::Contract::Kind kind, int64_t level);
    static int64_t hydrogen(int64_t level);
    /// What it asks at `level`: ore (300 at level 1, +2% a level, to 50),
    /// MH (60, +2% a level, to 10), ore loads drilled (6 + level / 20).
    static int64_t ore(int64_t level);
    static int64_t hydrogenNeed(int64_t level);
    static int64_t loads(int64_t level);
    /// The first ten minutes (docs/contracts.md, "First ten minutes"): a new
    /// pilot's chain of radio jobs, one at a time, before the daily three.
    /// 1 the Landing crate, 2 the Citadel, 3 a load of ore home, 4 the
    /// Derrick's wreck, 5 the Derrick built and pumped once, 6 a skill
    /// point spent.
    static constexpr int firstSteps = 6;
    /// What step `step` pays: XP (the chain takes a new pilot past the
    /// Derrick's floor, level 3, by step 4's end, and to level 4 by its
    /// last) and a little MH.
    static double firstXp(int step);
    static int64_t firstHydrogen(int step);
};

/// Citadel missions (K2, docs/missions.md; WorldMissions.cpp): a board of
/// offers per pilot at the realm's Citadel, drawn by the server's dice per
/// pilot and world day, in three tiers; the pilot signs up for up to three
/// at once and follows them in its Journal.
struct WorldMissions {
    /// Offers a tier on the board, and missions taken at once.
    static constexpr size_t offers = 2;
    static constexpr size_t journal = 3;
    /// Minutes of play a tier's XP is worth (`WorldContracts::perMinute`).
    static double minutes(net::CitadelMission::Tier tier);
    /// What a mission of `tier` pays at `level`: XP rounded to 5; ore
    /// (150, 450, 1,000 at level 1, +2% a level, to 50); MH (10, 30, 60 and
    /// 2, 4, 8 a level, to 5).
    static double xp(net::CitadelMission::Tier tier, int64_t level);
    static int64_t ore(net::CitadelMission::Tier tier, int64_t level);
    static int64_t hydrogen(net::CitadelMission::Tier tier, int64_t level);
    /// What it asks at `level`: ore or MH hauled home (easy and hard),
    /// seconds of scanning, seconds held under the band, bugs killed.
    static int64_t haulOre(net::CitadelMission::Tier tier, int64_t level);
    static int64_t haulHydrogen(net::CitadelMission::Tier tier, int64_t level);
    static int64_t scan(net::CitadelMission::Tier tier);
    static int64_t hold(net::CitadelMission::Tier tier);
    static int64_t kills(net::CitadelMission::Tier tier);
    /// A survey's and a hold's ring round its point, cells.
    static constexpr double surveyRadius = 6;
    static constexpr double holdRadius = 10;
    /// The tier of a region at `cells` grid steps from the start region
    /// (easy 0, hard 1, perilous 2 and more: the far corners, bug country).
    static net::CitadelMission::Tier tierAt(int64_t steps);
};

/// What the world's rules are.
struct WorldParams {
    /// A new pilot's wallet: a Citadel (400) and a Derrick (75) with some
    /// left over. A kept pilot keeps its own.
    int64_t startOre = 600;
    int64_t startHydrogen = 0;
    /// The driven unit crosses an edge once its centre is this close to it
    /// (cells), in an opening, moving out. A Prospector driven at the edge
    /// stops 1.56 cells from it (the edge's cell and its radius).
    double crossBand = 2.0;
    /// Where it comes in: this deep inside the neighbour, past the band (the
    /// opening's lane, 2 cells, is checked open by the region's validation;
    /// a blocked spot gives the nearest open one).
    double arriveDepth = 2.5;
    /// A flight leaves from within this many cells of a pad's centre.
    double padReach = region::padReach;
    /// Seconds a pad-to-pad flight takes.
    double flightTime = 4.0;
    /// Dev and tests (`--dev-xp`, as the game's -AcEarn): XP a new pilot's
    /// Prospector earns at once, when seated at the start pad. 0: none.
    double devXp = 0;
    /// The world's day, seconds (`WorldClock::dayLength`, 40 minutes).
    double dayLength = WorldClock::dayLength;
    /// Dev and shots (`--hour 22`): hold the world's hour (0-24).
    std::optional<double> heldHour;
    /// Dev and shots (`--weather storm`): hold the weather, 0 clear .. 3
    /// storm (fractions blend).
    std::optional<double> weatherHold;
    /// A well no one pumps grows back 1 MH every this many seconds, up to
    /// what it held at the start (docs/mmorpg-m6.md, "Decided"). 0: never.
    double wellRegrowEvery = 10;
    /// Dev and shots (`--dev-blueprints`): a new pilot has every blueprint,
    /// `devCopies` copies of each (C2).
    bool devBlueprints = false;
    static constexpr int64_t devCopies = 9;
    /// Dev and shots (`--dev-rig`): a new pilot owns a finished Citadel,
    /// Foundry and Lab round its Prospector (which stands where it unloads
    /// at the Citadel), and its rig is stocked: marks researched, modules
    /// in the stores and fitted, a job at each (WorldRig.cpp `devRig`).
    bool devRig = false;
    /// The survey chips' dice (0: seeded by the world's id).
    uint64_t chipSeed = 0;
    /// KSTR contracts are drawn (tests that count the wallet exactly turn
    /// them off), and their dice (0: seeded by the world's id).
    bool contracts = true;
    uint64_t contractSeed = 0;
    /// Contracts to drill under a ringshadow sweep are drawn (off until the
    /// server pays the sweep, E1: `WorldMap::underSweep`).
    bool sweepContracts = false;
    /// Citadel missions (K2) are drawn, and their dice (0: seeded by the
    /// world's id). Hunts are drawn now the Brood is in the world (N2; its
    /// kills count through `broodKill`).
    bool missions = true;
    uint64_t missionSeed = 0;
    bool huntMissions = true;
    /// Starfalls (Starfall.h, WorldStarfall.cpp) fall, and their dice (0:
    /// seeded by the world's id).
    bool starfalls = true;
    uint64_t starfallSeed = 0;
    /// Dev, tests and shots (`--starfall S`): a starfall lands near the
    /// first pilot on line, S seconds after it comes (told at once).
    std::optional<double> devStarfall;
    /// Seconds (the world's clock) a fallen Prospector waits before it comes
    /// back at its Citadel or nearest Hab Dome (P6); Second wind cuts it.
    double respawnDelay = 10;
    /// The step of the first ten minutes a new pilot starts at (1; dev and
    /// shots `--first-step N`: later; 0: none, as a kept pilot from an old
    /// save).
    int firstStep = 1;
    /// The Brood (docs/creatures.md; WorldBrood.cpp): its director spawns
    /// bugs in the regions with a brood level (`--brood 0` turns it off),
    /// and its dice (0: seeded by the world's id).
    bool brood = true;
    uint64_t broodSeed = 0;
};

/// The Brood's director in a world (docs/creatures.md, "Where they come
/// from"; WorldBrood.cpp): its numbers.
struct WorldBrood {
    /// By brood level (1 Prospect Landing … 4 Slag Pockets): bugs alive at
    /// most outside a nest, and a new group every so many seconds.
    static int64_t cap(int64_t level);
    static double every(int64_t level);
    /// A group lingers 3 to 6 minutes (× `nightLinger` at night); by night
    /// the director's interval is × `nightEvery`.
    static constexpr double lingerMin = 180;
    static constexpr double lingerMax = 360;
    static constexpr double nightLinger = 1.5;
    static constexpr double nightEvery = 0.75;
    /// The director runs only while a pilot is in the region or next to it;
    /// `idleDespawn` s after the last one left, the region's bugs go.
    static constexpr double idleDespawn = 120;
    /// Spawns: this far from every Prospector and every building, and from
    /// a pilot that came back from a fall in the last `respawnCalm` s.
    static constexpr double spawnFromProspector = 18;
    static constexpr double spawnFromBuilding = 12;
    static constexpr double spawnFromRespawn = 25;
    static constexpr double respawnCalm = 30;
    /// Out of sight, for a bug that leaves: this far from every Prospector
    /// and every building.
    static constexpr double sightProspector = 16;
    static constexpr double sightBuilding = 14;
    /// Sites: a deposit's, a well's, a pad's, a blueprint site's radius; a
    /// building's footprint plus this; a crater's. A ringshadow sweep
    /// makes a site × `sweepSite` wide and its leash `sweepLeash` longer.
    static constexpr double siteRadius = 6;
    static constexpr double buildingSite = 5;
    static constexpr double craterSite = 8;
    static constexpr double leash = 8;
    static constexpr double sweepSite = 1.5;
    static constexpr double sweepLeash = 4;
    /// Nothing lingers within this of a Citadel (raids aside); in Prospect
    /// Landing (level 1) nothing lingers within `startCalm` of its start pad,
    /// raids only buildings at least `raidFromCitadel` from the Citadel,
    /// and never the Citadel itself (until D1 arms it).
    static constexpr double citadelCalm = 15;
    static constexpr double startCalm = 25;
    static constexpr double raidFromCitadel = 25;
    /// Raids: at most one a region every `raidEvery` s (level 1:
    /// `raidEveryGentle`), warned `raidWarning` s before they arrive.
    static constexpr double raidEvery = 240;
    static constexpr double raidEveryGentle = 600;
    static constexpr double raidWarning = 20;
    /// A Lodeback eats only an unprotected deposit: no realm building within
    /// `guardBuilding`, no Prospector within `guardProspector`. Its ore goes
    /// back to the nearest deposit within `returnReach` if its own is gone.
    static constexpr double guardBuilding = 10;
    static constexpr double guardProspector = 8;
    static constexpr double returnReach = 10;
    /// A starfall's impact kills every bug within `impactKill` and scares
    /// those within `impactScare`; its crater draws nothing for `craterHot` s.
    static constexpr double impactKill = 4;
    static constexpr double impactScare = 20;
    static constexpr double craterHot = 180;
    /// The director's think: every this many seconds.
    static constexpr double think = 0.25;
};

/// The buildings' jobs for the pilot in a world (P6, docs/prospector-progression.md
/// "Buildings in the MMO"; WorldRoles.cpp): where a fallen Prospector comes
/// back, and the pads of the pilot's own (its Spacedock, and with Homing
/// beacon its Citadel, Hab Domes and Derricks).
struct WorldRoles {
    /// A building counts as the pilot's pad within this many cells of its
    /// edge (at least the pilot's pad reach from its centre).
    static constexpr double buildingPadReach = 2.0;
};

/// A pad of the pilot's own: a finished building of the realm's in region
/// `region` (R1: a Spacedock, or one its Homing beacon makes a pad), where
/// it flies from and lands. `name` is what the Fly request names:
/// "Spacedock", "Citadel", or "Hab Dome 2" and "Derrick 3" (by the
/// building's id among the realm's of that kind, from 1).
struct OwnPad {
    size_t region = 0;
    int64_t building = 0;
    StructureKind kind = StructureKind::spacedock;
    std::string name;
    /// The building's centre on the region's ground.
    Vec2 at{0, 0};
    bool operator==(const OwnPad&) const = default;
};

/// What the server tells a pilot after a step: its place in the world
/// changed (a new World message), and its region with it (a RegionChange).
struct Moved {
    PilotId pilot = 0;
    std::optional<net::RegionChange> change;
};

/// The running world: its regions (all stepped every tick), its pilots.
/// A world of one generated region with its AI (the M1 server, `--map`) has
/// no places: pilots join it and take units as before.
class WorldMap {
public:
    /// One generated region (M1).
    explicit WorldMap(const RegionConfig& config);
    /// A world from its files.
    WorldMap(const LoadedWorld& world, const WorldParams& params = {}, const RegionConfig& base = {});

    bool isWorld() const { return world_.has_value(); }
    size_t regionCount() const { return regions_.size(); }
    Region& region(size_t i) { return *regions_[i]; }
    const Region& region(size_t i) const { return *regions_[i]; }
    /// The start region (M1: the one region).
    Region& start() { return *regions_[start_]; }
    const Region& start() const { return *regions_[start_]; }
    const std::optional<LoadedWorld>& loaded() const { return world_; }
    const WorldParams& params() const { return params_; }
    /// A region's index by its id.
    std::optional<size_t> regionNamed(std::string_view id) const;

    /// A new pilot: in a world, at the start pad driving a new Prospector
    /// with the starting wallet.
    bool join(PilotId pilot);
    /// How `seat` seated a pilot.
    enum class Seated { refused, fresh, restored };
    /// A pilot by its key (net::pilotKeyHex; docs/mmorpg-m3.md "Pilot
    /// identity"): one known from before (left, or from the save) comes back
    /// where it left, in its region, driving its own Prospector where it
    /// stands, with its wallet and levels; an unknown one is new, as `join`.
    /// Refused: that key is on line already, or the pilot number is taken.
    /// An empty key (and any key outside a world) is a pilot never kept.
    Seated seat(PilotId pilot, const std::string& key);
    /// Off line: a kept pilot's Prospector stays where it is (one in
    /// flight lands at its pad), and the pilot waits for its key.
    void leave(PilotId pilot);
    /// Pilots known by key that are off line.
    size_t away() const { return away_.size(); }

    /// The world as the save keeps it (WorldStore.h): every region's
    /// buildings, the pilots' units, what is left in the deposits and wells,
    /// and every kept pilot, on line or not.
    WorldSave save() const;
    /// Puts a save in place, before any pilot joins: the regions' state, and
    /// the pilots off line until they come back. What could not be used,
    /// one a line (a region the world has no more, levels that do not read).
    std::vector<std::string> restore(const WorldSave& save);
    bool joined(PilotId pilot) const { return pilots_.count(pilot) > 0; }
    /// The region the pilot is in (in flight: the one it lifted off from).
    std::optional<size_t> where(PilotId pilot) const;
    Region* regionOf(PilotId pilot);
    /// Region changes so far.
    int64_t visit(PilotId pilot) const;
    bool flying(PilotId pilot) const;

    struct Wallet {
        int64_t ore = 0;
        int64_t hydrogen = 0;
        bool operator==(const Wallet&) const = default;
    };
    /// The pilot's bank: its side's ore and MH in the region it is in.
    std::optional<Wallet> wallet(PilotId pilot) const;

    /// The pilot's levels and picks per kind (the core's `PilotRecord`,
    /// docs/leveling.md): its own, wherever it is, in flight too. Nil: not
    /// joined, or never driven.
    std::optional<PilotRecord> levels(PilotId pilot) const;
    /// Sets them (a pilot loaded from a save): its region's simulation has
    /// them at once if it drives.
    void setLevels(PilotId pilot, const std::optional<PilotRecord>& levels);
    /// The level-up card pick (`Region::pick`). The reason when refused.
    std::optional<std::string> pick(PilotId pilot, UnitKind kind, Perk perk);

    /// Fly from the pad the driven Prospector stands at to `pad` of `region`:
    /// a region file's pad, or one of the pilot's own (`ownPads`). It flies
    /// from within `padReach` of a pad's centre, or near a pad of its own
    /// (`WorldRoles::buildingPadReach`); with Ringwalker from anywhere, once
    /// every its cooldown. The reason when refused.
    std::optional<std::string> fly(PilotId pilot, std::string_view region, std::string_view pad);
    /// The pilot's pad reach (Pad link) and flight time (Short hop).
    double padReach(PilotId pilot) const;
    double flightTime(PilotId pilot) const;
    /// The pilot's own pads across the world (P6; R1: the realm's
    /// buildings its skills make pads, until F2's landing pads).
    std::vector<OwnPad> ownPads(PilotId pilot) const;
    /// The pilot's Prospector fell and waits to come back (P6): when, where
    /// and with what (nil: on its feet, or not a world).
    std::optional<net::ProgressMessage::Respawn> respawning(PilotId pilot) const;

    /// Build with the driven Prospector where the pilot is (`Region::build`),
    /// paid from its wallet; in a world only `net::kWorldBuildKinds`, and
    /// none touching a pad's plate (`region::touchesPadPlate`). The reason
    /// when refused.
    std::optional<std::string> build(PilotId pilot, StructureKind kind, Vec2 at);

    /// Every region one tick; then edge crossings and landings.
    void step();
    /// Region changes since the last call.
    std::vector<Moved> takeMoves();

    /// What the pilot is told now (its region's snapshot, the game stamped
    /// with its visit).
    Snapshot snapshot(PilotId pilot, const InterestParams& params = {});
    void deferred(PilotId pilot, const std::vector<int64_t>& ids);
    /// Queues the pilot's input in its region (`Region::input`); in a world,
    /// one of an earlier visit than the pilot's (`PilotInput::visit`) is
    /// dropped (false).
    bool input(PilotId pilot, const PilotInput& in);
    /// What the pilot's client builds its mirror from.
    RegionDescription description(PilotId pilot) const;
    /// The world map for the pilot's client (nil: not a world).
    std::optional<net::WorldMessage> worldMessage(PilotId pilot) const;

    // MARK: The world's clock and weather (docs/mmorpg-m6.md, C1)

    /// World seconds since the world began: one clock for every region,
    /// advanced a server tick each step and saved with the world.
    double clock() const { return clock_; }
    /// Sets it (tests; a save sets it in `restore`).
    void setClock(double time);
    /// The world's hour now (`WorldClock::hourAt`; held by `heldHour`).
    double hour() const { return sky_.hour(); }
    /// The clock and the weather as the snapshots carry them (nil: not a
    /// world). The weather is the same over the whole world: `Weather`
    /// seeded by the world's id, sampled at the clock.
    std::optional<net::Sky> sky() const { return world_ ? std::optional<net::Sky>(sky_) : std::nullopt; }
    /// The weather sampled at the clock now.
    const WeatherSample& weather() const { return weatherNow_; }

    // MARK: The Prospector's progression (docs/prospector-progression.md, P3)

    /// The pilot's progression (nil: not joined, or not a world).
    std::optional<ProspectorProgress> progress(PilotId pilot) const;
    /// Sets it (tests, dev): its region's simulation has it at once.
    void setProgress(PilotId pilot, const ProspectorProgress& progress);
    /// XP for work: the level it reaches is news.
    void earn(PilotId pilot, double xp);
    /// The skill tree's requests, checked here (`ProspectorProgress`'s
    /// refusals, and a respec only at the pilot's own Citadel, its MH from
    /// the wallet). The reason when refused.
    std::optional<std::string> spend(PilotId pilot, Skill skill);
    std::optional<std::string> undo(PilotId pilot, Skill skill);
    std::optional<std::string> respec(PilotId pilot);
    std::optional<std::string> slot(PilotId pilot, int64_t index, std::optional<Skill> skill);
    /// The one-offs: a first visit to a region, a first landing on a pad
    /// (region/pad), a feat (`WorldXp`'s ids). Paid once per pilot ever:
    /// false when it was paid before.
    bool firstVisit(PilotId pilot, const std::string& region);
    bool firstLanding(PilotId pilot, const std::string& pad);
    bool feat(PilotId pilot, const std::string& id);
    /// P4's finds: a blueprint copy (C2: from every site and blueprint
    /// chip, 50 × its level floor each) and a survey chip (25 each): it marks the nearest site
    /// the pilot doesn't know of, here or in a neighbour region, or one in
    /// `WorldSites::chipBlueprintOneIn` is a blueprint itself.
    bool findBlueprint(PilotId pilot, StructureKind kind, int64_t floor);
    void findChip(PilotId pilot);
    /// An ore load drilled by the pilot's Prospector (`step` calls it): it
    /// rolls for a survey chip (`ProspectorProgress::chipChance`, at most one
    /// every `WorldSites::chipEvery`). True when a chip came.
    bool oreLoad(PilotId pilot);
    /// The realm's buildings of a kind across the world, finished or not
    /// (R1: the limits are the realm's).
    int64_t realmCount(StructureKind kind) const;
    /// A site's id, "<region id>/<site name>".
    std::string siteId(size_t region, size_t site) const;
    // MARK: The rig (P7, docs/prospector-progression.md "Foundry and Lab"; WorldRig.cpp)

    /// The pilot's Foundry starts making `module` at `mark`, paid from the
    /// wallet; its Lab starts researching `mark`; `mark` is fitted from the
    /// stores at its own Citadel or Hab Dome (0: taken off). The reason
    /// when refused.
    std::optional<std::string> make(PilotId pilot, Module module, int64_t mark);
    std::optional<std::string> research(PilotId pilot, Module module, int64_t mark);
    std::optional<std::string> fit(PilotId pilot, Module module, int64_t mark);
    /// The realm has a finished building of `kind` somewhere in the world
    /// (R1: every pilot uses it).
    bool realmFinished(StructureKind kind) const;
    /// Who built a building of region `r`: the pilot's callsign (`callsign`),
    /// empty when not known.
    std::string builder(size_t r, int64_t building) const;
    /// A pilot's name to other pilots: "Pilot " and four hex digits of a
    /// hash of its key (the key itself never goes on the wire).
    static std::string callsign(const std::string& key);

    // MARK: The realm wallet (C1, docs/prospector-progression.md "The
    // realm's tax"; WorldRealm.cpp)

    /// Every pilot's tax and donations, the world's (saved with it).
    Wallet realmWallet() const { return realm_; }
    void setRealmWallet(const Wallet& w) { realm_ = w; }
    /// Gives `ore` and `hydrogen` from the pilot's wallet to the realm's, at
    /// the realm's Citadel, standing where it unloads. The reason when
    /// refused (away from the Citadel, nothing given, more than it has).
    std::optional<std::string> donate(PilotId pilot, int64_t ore, int64_t hydrogen);
    /// Takes `cost` from the realm wallet (D1: the Citadel's upgrades).
    /// False, taking nothing, when the wallet is short or `cost` negative.
    bool spendRealm(const Wallet& cost);
    /// What a pilot has given the realm in all: in tax, and donated. Nil:
    /// not joined.
    std::optional<Wallet> taxed(PilotId pilot) const;
    std::optional<Wallet> donated(PilotId pilot) const;

    // MARK: The Citadel shop (C2, docs/prospector-progression.md "The
    // Citadel shop"; WorldShop.cpp)

    /// Buys one copy of `blueprint`'s blueprint (set), or one `item`, from
    /// the pilot's wallet at the core's price (`Shop`), standing where it
    /// unloads at the realm's Citadel. The reason when refused (away from
    /// the Citadel, no such blueprint, the stack full, the wallet short).
    std::optional<std::string> buy(PilotId pilot, std::optional<StructureKind> blueprint, Item item);
    /// Uses one `item` from the pilot's stacks on its Prospector where it
    /// stands. The reason when refused (none held, cooling down, in flight
    /// or down, a repair kit on a Prospector not hurt).
    std::optional<std::string> use(PilotId pilot, Item item);
    /// The world's clock when the pilot's `item` is ready again (nil: now).
    std::optional<double> itemReady(PilotId pilot, Item item) const;


    // MARK: Citadel missions (K2, docs/missions.md; WorldMissions.cpp)

    /// The pilot's board today (offers), and its Journal (taken and done
    /// missions). Empty: not a world, or not drawn yet.
    std::vector<net::CitadelMission> board(PilotId pilot) const;
    std::vector<net::CitadelMission> journal(PilotId pilot) const;
    /// Puts a board or a Journal in place for today (tests, dev).
    void setBoard(PilotId pilot, const std::vector<net::CitadelMission>& offers);
    void setJournal(PilotId pilot, const std::vector<net::CitadelMission>& missions);
    /// Signs up for an offer (`net::RequestMessage::MissionAsk`): at the
    /// realm's Citadel, standing where it unloads, at most
    /// `WorldMissions::journal` at once. Drops a taken one, anywhere. The
    /// reason when refused.
    std::optional<std::string> takeMission(PilotId pilot, uint32_t mission);
    std::optional<std::string> abandonMission(PilotId pilot, uint32_t mission);
    /// A bug of `kind` killed by the pilot (its Prospector, or a defence it
    /// built) in region `region`: the hunts count it. N2's Brood calls it.
    void broodKill(PilotId pilot, UnitKind kind, size_t region);

    /// The pilot's contracts today (empty: not a world, or not drawn yet:
    /// the next step draws them).
    std::vector<net::Contract> contracts(PilotId pilot) const;
    /// Puts contracts in place for today (tests, dev).
    void setContracts(PilotId pilot, const std::vector<net::Contract>& list);
    /// Drops an open contract (`net::RequestMessage::Abandon`). The reason
    /// when refused.
    std::optional<std::string> abandon(PilotId pilot, uint32_t contract);
    /// The world day at `time` (they start at dawn, `Rules::sunrise`), and
    /// when the next one starts.
    int64_t contractDay(double time) const;
    double nextDawn(double time) const;
    /// The pilot's Prospector drills under a ringshadow sweep now. A stub
    /// (false) until E1 hands the server the sweep (`Ringshadow::cover` at
    /// the unit's ground point); the sweep contracts count on it.
    bool underSweep(PilotId pilot) const;

    // MARK: Starfall (docs/weather.md "Starfall", E2; WorldStarfall.cpp)

    /// A fall told or landed: where, when, from which way, and its deposit
    /// (an index in its region's patches once landed, and the ore it had).
    struct Fall {
        uint32_t id = 0;
        size_t region = 0;
        Vec2 at{0, 0};
        double impact = 0;
        double from = 0;
        std::optional<int64_t> patch;
        int64_t ore = 0;
        /// Its deposit's ore after the last step (the XP bonus counts what
        /// is drilled).
        int64_t left = 0;
        bool operator==(const Fall&) const = default;
    };
    /// The falls the pilots are told now: coming, and landed until the
    /// crater has faded and the deposit is empty.
    const std::vector<Fall>& starfalls() const { return falls_; }
    /// Picks a place now and tells every pilot of a fall `in` seconds from
    /// now: in region `region` (nil: weighted toward the regions with
    /// pilots, as the schedule does), or near `near` (a pilot: 40-58 cells
    /// from its Prospector, ahead of it if it can). Nil: no open ground.
    std::optional<Fall> forceStarfall(double in, std::optional<size_t> region = std::nullopt,
                                      std::optional<PilotId> near = std::nullopt);
    /// Whether ground point `at` of region `r` takes a crater: open, flat
    /// ground joined to the region's pads, clear of its sites, pads, ore,
    /// wells and buildings, and of the map's edge.
    bool craterFits(size_t r, Vec2 at) const;
    /// The starfall dice's seed (`params().starfallSeed`, or the world's id).
    uint64_t starfallSeed() const;

    // MARK: The Brood (docs/creatures.md; WorldBrood.cpp)

    /// What draws a group: a point of interest of its region.
    enum class Poi : uint8_t { deposit, well, site, pad, building, crater };
    /// A group of bugs and what it is doing: its site (ground point, radius,
    /// leash), how long it lingers, its raid, its prey.
    struct BroodGroup {
        int64_t id = 0;
        Poi poi = Poi::deposit;
        /// The POI: a deposit's or well's index, a site's or pad's index, a
        /// building's id, a fall's id.
        int64_t key = 0;
        Vec2 site{0, 0};
        double radius = WorldBrood::siteRadius;
        double leash = WorldBrood::leash;
        std::vector<int64_t> bugs;
        double lingerUntil = 0;
        bool leaving = false;
        Vec2 exit{0, 0};
        /// A raid on building `key`: told (`Sky::Raid`) once.
        bool raid = false;
        uint32_t raidId = 0;
        std::optional<int64_t> prey;
    };
    /// The groups of region `r` now.
    const std::vector<BroodGroup>& broodGroups(size_t r) const;
    /// The bugs alive in region `r`.
    int64_t broodAlive(size_t r) const;
    /// The Brood's side in region `r`'s simulation (nil: no brood level).
    std::optional<int64_t> broodSide(size_t r) const;
    /// Tests, dev and shots: a group of `count` bugs of `kind` at a POI of
    /// region `r` (with a Lodeback's escort when `escort`), spawned at
    /// `from` (nil: out of sight, as the director spawns), lingering
    /// already when `arrived`. A raid's POI is a building. Nil: no brood side.
    std::optional<int64_t> spawnBroodGroup(size_t r, UnitKind kind, int64_t count, Poi poi, int64_t key,
                                           std::optional<Vec2> from = std::nullopt, bool arrived = false, int64_t escort = 0);
    /// Tests: the director's next group in region `r` comes now.
    void broodNow(size_t r);
    /// Dev and shots (`--dev-control`'s `brood …` lines): a scene near the
    /// first pilot on line. What it did, or why not.
    std::string devBrood(const std::string& line);

    /// What the pilot's client is told of its progression now (no news).
    std::optional<net::ProgressMessage> progressMessage(PilotId pilot) const;
    /// What happened to the pilot's progression since the last call.
    std::vector<net::ProgressMessage::News> takeNews(PilotId pilot);

    /// Wall-clock ms the last `step` took over all the regions.
    double lastStepMs() const { return lastStepMs_; }

private:
    /// A pilot unit's skill clocks on the way to another region: when its
    /// actives are ready and its bursts end, as seconds from `at` (the
    /// world's clock when it left), and its counts.
    struct Carried {
        Simulation::SkillClock clock;
        double at = 0;
    };
    struct Flight {
        Unit unit;
        std::optional<Carried> clocks;
        size_t to = 0;
        size_t pad = 0;
        /// Landing on a pad of the pilot's own: its building's centre on
        /// the ground (nil: the region's pad `pad`).
        std::optional<Vec2> own;
        int64_t landsAt = 0;
    };
    /// A fallen Prospector (P6): back at world time `until`, with `cargo`
    /// (Second wind); it fell in region `region` at region position `at`.
    struct Down {
        double until = 0;
        size_t region = 0;
        Vec2 at{0, 0};
        int64_t cargo = 0;
        bool hydrogen = false;
    };
    /// Where a fallen Prospector comes back: beside a building of its own in
    /// region `region` (`at`: the building's centre on the ground), or the
    /// world's start pad (`kind` nil).
    struct Home {
        size_t region = 0;
        Vec2 at{0, 0};
        std::optional<StructureKind> kind;
    };
    struct PilotState {
        size_t region = 0;
        int64_t visit = 0;
        Wallet wallet;
        std::optional<PilotRecord> record;
        std::optional<Flight> flight;
        /// The driven unit's region position after the last step.
        std::optional<Vec2> last;
        /// Its key (empty: not kept), and its own Prospector in `region`
        /// (driven or not; in flight: the one it left).
        std::string key;
        std::optional<int64_t> unit;
        /// Its progression in the world, and what happened to it since its
        /// client was last told.
        ProspectorProgress progress;
        std::vector<net::ProgressMessage::News> news;
        /// The site it holds the act key at (`WorldSites`), not saved.
        struct Opening {
            size_t region = 0;
            size_t site = 0;
            double since = 0;
            std::string refusal;
        };
        std::optional<Opening> opening;
        /// Its KSTR contracts, the world day they were last refreshed for,
        /// and its next contract number.
        std::vector<net::Contract> contracts;
        std::optional<int64_t> contractDay;
        uint32_t nextContract = 1;
        /// The first ten minutes' step open now, 1…`WorldContracts::firstSteps`
        /// (its only contract); 0: done with it, or never on it.
        int firstStep = 0;
        /// Its Prospector fell (P6), not saved: a pilot that comes back
        /// with none gets a new one at home.
        std::optional<Down> down;
        /// Ringwalker: the world's clock when a flight from anywhere is
        /// ready again.
        double ringwalkerReady = 0;
        /// Its own pads as its client was last told (the World message).
        std::vector<OwnPad> padsTold;
        /// C1: what it has given the realm wallet in all, in tax and
        /// donated (saved).
        Wallet taxed;
        Wallet donated;
        /// C2: when each consumable used is ready again (the world's
        /// clock), and a survey flare burning (its region, the Prospector's
        /// position there when lit, until the world's clock). Not saved.
        std::map<Item, double> itemReady;
        struct Flare {
            size_t region = 0;
            Vec2 at{0, 0};
            double until = 0;
        };
        std::optional<Flare> flare;
        /// K2: its Citadel board, the world day it was drawn for, its
        /// Journal, its next mission number (saved); the seconds a survey
        /// or a hold has counted toward its next whole second (not saved).
        std::vector<net::CitadelMission> board;
        std::optional<int64_t> boardDay;
        std::vector<net::CitadelMission> journal;
        uint32_t nextMission = 1;
        std::map<uint32_t, double> missionSeconds;
    };
    /// What the driven unit was doing before a step (the work XP).
    struct Work {
        PilotId pilot = 0;
        size_t region = 0;
        int64_t unit = 0;
        bool hydrogen = false;
        int64_t carrying = 0;
        std::optional<int64_t> patch;
        /// The buildings of its own it welds, and their build time left.
        std::map<int64_t, double> welding;
        /// Its own finished buildings' hp.
        std::map<int64_t, double> hp;
    };

    std::optional<LoadedWorld> world_;
    WorldParams params_;
    std::vector<std::unique_ptr<Region>> regions_;
    size_t start_ = 0;
    std::map<PilotId, PilotState> pilots_;
    /// Kept pilots off line, by key.
    std::map<std::string, PilotState> away_;
    /// Who built each building, per region (building id → key).
    std::vector<std::map<int64_t, std::string>> builtBy_;
    /// The realm wallet (C1).
    Wallet realm_;
    std::vector<Moved> moves_;
    int64_t tick_ = 0;
    double lastStepMs_ = 0;
    double clock_ = 0;
    std::unique_ptr<Weather> weather_;
    WeatherSample weatherNow_;
    net::Sky sky_;
    /// Each region's wells: what they held at the start, the seconds each
    /// has stood unpumped since it last grew, and what it held then.
    std::vector<std::vector<int64_t>> wellFull_;
    std::vector<std::vector<double>> wellIdle_;
    std::vector<std::vector<int64_t>> wellLast_;
    /// Hp each region's buildings lost to something else, not mended yet,
    /// and repair XP paid per building on the day (`WorldXp::repairDay`).
    std::vector<std::map<int64_t, double>> owed_;
    std::map<std::pair<size_t, int64_t>, std::pair<int64_t, double>> repaid_;
    std::mt19937_64 chips_;
    /// How a pilot reaches each region's sites (`region::siteReach`),
    /// worked out the first time a contract needs it.
    std::vector<std::optional<std::vector<region::SiteReach>>> siteReach_;
    /// Starfall (WorldStarfall.cpp): the falls told, the clock the schedule
    /// was last read at, the next fall's id, each region's own deposits
    /// (the region file's: starfall deposits come after them), and the dev
    /// fall still to come.
    std::vector<Fall> falls_;
    std::optional<double> fallsRead_;
    uint32_t nextFall_ = 1;
    std::vector<size_t> ownPatches_;
    bool devFallDone_ = false;

    /// A new Prospector on the start pad, facing the start region's nearest
    /// ore deposit (one with ore left first), so the first thing in view is
    /// work.
    Unit startProspector() const;
    /// The neighbour across an edge (nil: the world's border, or no region).
    std::optional<size_t> neighbour(size_t r, region::Edge e) const;
    /// Puts the pilot and its unit into region `to` at region position `at`
    /// (or the nearest open spot), driving it unless another pilot drives
    /// there (false). `lastInput`: its inputs carry on from there.
    bool arrive(PilotId pilot, PilotState& p, size_t to, const Unit& u, Vec2 at, int64_t lastInput);
    void bankIn(const PilotState& p, size_t r);
    void bankOut(PilotState& p, size_t r);
    void crossings();
    void landings();
    /// The sky and the weather at the clock now, and every region's hour
    /// held at it (their sight at night).
    void updateSky();
    /// Into `away_` (WorldMapSave.cpp).
    void park(const PilotState& p);
    // WorldProgress.cpp.
    PilotState* pilotState(PilotId pilot);
    /// `p`'s own buildings in region `r` (the ids it built): its skills
    /// that reach buildings (Reinforced, Head start) reach these.
    std::set<int64_t> ownBuildings(const PilotState& p, size_t r) const;
    /// A building of the realm (R1, docs/prospector-progression.md
    /// "Buildings in the MMO"): in a world every building on the pilots'
    /// side is the world's, and every pilot uses it.
    bool realm(const Structure& b) const;
    /// `p` built it: its credit (feats, build contracts), not its use.
    bool built(const PilotState& p, size_t r, int64_t building) const;
    /// The progression into the pilot's region's simulation.
    void pushProgress(PilotId pilot);
    void earn(PilotState& p, double xp);
    /// A one-off into `set` under `id`, paid `xp`, told as `kind`.
    bool oneOff(PilotId pilot, std::set<std::string> ProspectorProgress::*set, const std::string& id, double xp,
                net::ProgressMessage::NewsKind kind);
    /// Before and after the regions step: the work XP and the feats.
    std::vector<Work> workBefore() const;
    void workAfter(const std::vector<Work>& before);
    /// After a step: the realm's buildings finished, told to every pilot.
    void realmNews();
    /// After a step: the Citadel's tax on each unload into the realm wallet
    /// (C1), credited to the pilot that paid it.
    void realmTaxes();
    void regrowWells(double dt);
    // WorldSites.cpp.
    /// After a step: sites seen, reached, held open.
    void sites();
    void openSite(PilotId pilot, PilotState& p, size_t r, size_t i);
    /// A Scanner pulse on arriving in region `r`.
    void scan(PilotState& p, size_t r);
    /// The site nearest the pilot that it doesn't know of, here or in a
    /// neighbour region (nil: none left).
    std::optional<std::pair<size_t, size_t>> nearestUnknown(const PilotState& p) const;
    // WorldRoles.cpp.
    /// Each step: fallen Prospectors noted and brought back home; the World
    /// message again when the pilot's own pads change.
    void respawns();
    void ownPadsChanged();
    std::vector<OwnPad> ownPads(const PilotState& p) const;
    double padReach(const PilotState& p) const;
    double flightTime(const PilotState& p) const;
    /// Where `p` comes back: its Citadel or Hab Dome nearest where it fell
    /// (`from` in region `region`; a region nearer on the grid first, one
    /// another pilot drives skipped), else the start pad. Nil: nowhere free.
    std::optional<Home> home(const PilotState& p, size_t region, Vec2 from) const;
    /// A new Prospector for `p` at `h`, with `cargo` (MH if `hydrogen`); into
    /// that region, its client told.
    void respawn(PilotId pilot, PilotState& p, const Home& h, int64_t cargo, bool hydrogen);
    /// The region change `p` is told after it moved into region `to`.
    void regionChanged(PilotId pilot, PilotState& p, size_t to);
    /// Where a flight lands, on the ground of its region.
    Vec2 landing(const Flight& f) const;
    // WorldContracts.cpp.
    /// Each step: the Foundries' and Labs' jobs done by now (P7).
    void rigs();
    /// `WorldParams::devRig` for a pilot just come.
    void devRig(PilotId pilot);
    /// `p` built a finished building of `kind` (its credit).
    bool builtFinished(const PilotState& p, StructureKind kind) const;
    /// The realm's finished Citadel (region, id), the first by region.
    std::optional<std::pair<size_t, int64_t>> realmCitadel() const;
    /// The driven Prospector stands where it unloads at one of the realm's
    /// finished buildings of these kinds.
    bool atRealm(PilotId pilot, const PilotState& p, std::initializer_list<StructureKind> kinds);
    /// Each step: a new day's draw, the reach contracts.
    void contracts();
    /// Fills the done and abandoned slots (all three on the first day).
    void drawContracts(PilotId pilot, PilotState& p, int64_t day);
    net::Contract drawContract(PilotId pilot, const PilotState& p, std::mt19937_64& dice,
                               const std::vector<net::Contract::Kind>& taken);
    /// Work toward the pilot's open contracts of `kind` (and `target`, when
    /// set); pays the ones it finishes.
    void contractWork(PilotId pilot, PilotState& p, net::Contract::Kind kind, const std::string& target, int64_t amount);
    /// Pays a contract its work just finished.
    void payContract(PilotId pilot, PilotState& p, net::Contract& c);
    /// The first ten minutes, each step: the open step's work counted, paid
    /// when done, the next one opened; after the last, the daily draw.
    void firstTen(PilotId pilot, PilotState& p);
    net::Contract firstContract(const PilotState& p, int step) const;
    /// The start region's first site holding `kind`'s blueprint.
    std::optional<size_t> startSite(StructureKind kind) const;
    /// A pilot's Prospector filled a tank at a Derrick (step 5).
    void pumped(PilotId pilot, PilotState& p);
    // WorldMissions.cpp.
    /// Each step: a new day's board, the surveys, holds and salvages.
    void missions();
    void drawBoard(PilotId pilot, PilotState& p, int64_t day);
    std::optional<net::CitadelMission> drawMission(const PilotState& p, std::mt19937_64& dice, net::CitadelMission::Tier tier,
                                            const std::vector<net::CitadelMission::Kind>& taken);
    /// Work toward the pilot's taken missions of `kind` and `target`; pays
    /// the ones it finishes.
    void missionWork(PilotId pilot, PilotState& p, net::CitadelMission::Kind kind, const std::string& target, int64_t amount);
    void payMission(PilotId pilot, PilotState& p, net::CitadelMission& m);
    /// Ringshadow cover at ground point `at` of region `r` now.
    double sweepCover(size_t r, Vec2 at) const;
    static std::string encodeMissions(const PilotState& p);
    static bool decodeMissions(const std::string& json, PilotState& p);
    static std::string encodeContracts(const PilotState& p);
    static bool decodeContracts(const std::string& json, PilotState& p);
    /// A unit's skill clocks out of region `r` (it leaves), and into `r` as
    /// unit `unit` (it arrives), its cooldowns running on meanwhile.
    std::optional<Carried> takeClocks(size_t r, int64_t unit);
    void giveClocks(size_t r, int64_t unit, const std::optional<Carried>& c);
    // WorldStarfall.cpp.
    /// Each step: the schedule's falls told, the landings, the deposits'
    /// XP and emptying, the faded ones dropped, the sky's list.
    void starfall();
    /// A fall's deposit into its region (`fall.patch` set): at the impact,
    /// and from a save.
    void landFall(Fall& fall, int64_t remaining, std::optional<int64_t> id = std::nullopt);
    /// A spot for a crater in region `r` by the dice (nil: none found);
    /// `near`: 40-58 cells from it, ahead along `heading` first.
    std::optional<Vec2> craterSpot(size_t r, std::mt19937_64& dice, std::optional<Vec2> near = std::nullopt,
                                   double heading = 0) const;
    /// A region for a fall by the dice: each counts 1, a pilot in it 8, a
    /// pilot next door 3.
    size_t fallRegion(std::mt19937_64& dice) const;
    std::vector<net::Sky::Starfall> skyFalls() const;
    // WorldBrood.cpp.
    struct BroodRegion {
        /// When the next group comes (world clock; nil: not running).
        std::optional<double> next;
        double lastRaid = -1e9;
        /// The clock a pilot was last in the region or next to it.
        double lastNear = -1e9;
        std::vector<BroodGroup> groups;
        /// Each Lodeback's sac and the deposit it ate from, after the last
        /// step: its ore goes back when it dies.
        std::map<int64_t, std::pair<double, std::optional<int64_t>>> sacs;
    };
    /// Each step: the hits on bugs (XP, aggro), a dead Lodeback's ore back,
    /// a starfall's impact; every `WorldBrood::think` s the groups' orders,
    /// leaving and spawning.
    void brood();
    void broodThink(size_t r);
    /// A group's POI picked by its kind's weights; spawned out of sight.
    bool spawnGroup(size_t r);
    std::optional<Vec2> spawnPoint(size_t r, std::optional<Vec2> toward) const;
    /// Ground point `at` of region `r` is in sight of a Prospector or a
    /// building of the realm.
    bool inSight(size_t r, Vec2 at) const;
    /// The way out of region `r` from `at`: the nearest edge point out of sight.
    Vec2 exitPoint(size_t r, Vec2 at) const;
    void tellRaids();
    std::vector<BroodRegion> brood_;
    std::vector<net::Sky::Raid> raids_;
    uint32_t nextRaid_ = 1;
    int64_t nextGroup_ = 1;
    std::mt19937_64 broodDice_;
    double broodThought_ = -1e9;
    /// Falls whose impact the bugs felt; pilots' Prospectors last seen (a
    /// new one is a respawn: when, where).
    std::set<uint32_t> fallsFelt_;
    std::map<PilotId, int64_t> broodUnits_;
    std::map<PilotId, std::pair<double, Vec2>> respawned_;
};

} // namespace ac::server
