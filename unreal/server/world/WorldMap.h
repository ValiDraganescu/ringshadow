// The server's world (docs/worlds.md): regions side by side on a grid, each
// a Region on its region file's wild ground, every one stepped every tick.
// A pilot is in one region at a time: it starts at the start pad driving a
// Prospector, drives across an edge's opening into the neighbour, or flies
// from a pad to a pad. Its wallet (ore and MH) and its levels go with it.
// Engine-free, like World.h.
#pragma once

#include "World.h"
#include "WorldStore.h"

#include "Flight.h"
#include "RealmProjects.h"
#include "RegionFile.h"
#include "Weather.h"
#include "WorldClock.h"

#include <array>
#include <cstdint>
#include <functional>
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
    /// How a pilot reaches each of `built.sites`, from the load's check.
    std::vector<region::SiteReach> siteReach;
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

/// L1, KSTR's next goal (docs/contracts.md "What next"; WorldGoals.cpp):
/// after the first ten minutes, one goal at a time from a fixed chain,
/// chosen by level and what the pilot has done; goals met before they come
/// up are skipped, the one shown pays a little when done.
struct WorldGoals {
    struct Entry {
        const char* key;
        /// The level it opens at; before it the next goal waits.
        int64_t level;
        const char* title;
        /// The radio's line (`{what}`: a place's name, `{n}`: a count).
        const char* pitch;
        const char* why;
        const char* hint;
    };
    /// The chain, in order.
    static const std::vector<Entry>& chain();
    static const Entry* find(const std::string& key);
    /// What a goal pays at `level`: 3 minutes of play (XP, to 5), `5 + level`
    /// MH (to 5).
    static double xp(int64_t level);
    static int64_t hydrogen(int64_t level);
    /// How often the goals are checked (world seconds).
    static constexpr double every = 1.0;
};

/// Citadel missions (K2, docs/missions.md; WorldMissions.cpp): a board of
/// offers per pilot at the realm's Citadel, drawn by the server's dice per
/// pilot and world day, in three tiers; the pilot signs up for up to three
/// at once and follows them in its Journal.
struct WorldMissions {
    /// A Matriarch hunt's target (N3): her nest's region, marked.
    static std::string queenTarget(const std::string& region) { return "matriarch:" + region; }
    /// Offers a tier on the board, and missions taken at once.
    static constexpr size_t offers = 2;
    static constexpr size_t journal = 3;
    /// Minutes of play a tier's XP is worth (`WorldContracts::perMinute`).
    static double minutes(net::CitadelMission::Tier tier);
    /// What a mission of `tier` pays at `level`: XP rounded to 5; ore
    /// (75, 225, 500 at level 1, +2% a level, to 25); MH (10, 30, 60 and
    /// 2, 4, 8 a level, to 5). B2: 3, 8, 15 minutes (docs/balance.md).
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

/// Mercenary prospectors (H2, docs/prospector-progression.md
/// "Mercenaries"; WorldMercs.cpp): hired at the realm's Citadel, each
/// mines a deposit field and hauls the ore to the nearest realm depot, for
/// the pilot's wallet (taxed), at a slow rate: in a day what the pilot
/// mines in an hour.
struct WorldMercs {
    /// Held at once at `level`: 1 from level 5, 2 from 15, 3 from 30.
    static int64_t limit(int64_t level);
    /// The level the place after `held` opens at (0: none more).
    static int64_t nextLevel(int64_t held);
    /// The fee for one more while holding `held`: 500 ore and 50 MH,
    /// doubling with each one held.
    static int64_t feeOre(int64_t held);
    static int64_t feeHydrogen(int64_t held);
    /// The wage each takes at the world's dawn, MH: 5 + level / 5.
    static int64_t wage(int64_t level);
    /// Ore a second at the pilot's `level`: the pace model's hour of work
    /// (`WorldContracts::perMinute` × 60, 1 XP an ore) over `hours` hours.
    static constexpr double hours = 24;
    /// R2: `dockCrew` (the realm's Dock Crew), over `RealmProject::dockCrewHours`.
    static double rate(int64_t level, bool dockCrew = false);
    /// A trip's load: it drills until it has worked this much (at least
    /// `Rules::miningTime` s), so the realm's tax is 10% of it.
    static constexpr int64_t load = 10;
    /// A downed mercenary is back at its depot at once and resumes after
    /// this many world seconds.
    static constexpr double downFor = 600;
    /// Deposits this close (cells) are one field.
    static constexpr double fieldLink = 8;
    /// Their names (a mercenary's tag: "MERC · <name>").
    static const std::vector<std::string>& names();
};

/// Where unit `u` stands at `at`, or the nearest open ground within `reach`
/// cells round it (still `at` if none).
Vec2 openGround(const Simulation& sim, const Unit& u, Vec2 at, double reach);

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
    /// The Dropship is called from within this many cells of a Landing
    /// Pad's centre (F2; Pad link reaches farther).
    double padReach = region::padReach;
    /// Dev and tests (`--dev-xp`, as the game's -AcEarn): XP a new pilot's
    /// Prospector earns at once, when seated at the start pad. 0: none.
    double devXp = 0;
    /// The world's day, seconds (`WorldClock::dayLength`, 40 minutes).
    double dayLength = WorldClock::dayLength;
    /// E1: the share of its full ore a region file's deposit grows back a
    /// world day, up to full (`region::regrowPerDay`). 0: none (tests that
    /// count ore to the unit).
    double veinRegrow = region::regrowPerDay;
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
    /// Dev and shots (`--dev-seen a,b`): a new pilot has been to these
    /// regions (the world map shows them; M2).
    std::vector<std::string> devSeen;
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
    /// Threads the regions with a pilot step on, the calling thread among
    /// them (docs/perf.md "Server"): 0, the caller alone, region after
    /// region; -1, half the cores and at most 8; more, that many. The
    /// regions without a pilot and the world's own work after the steps
    /// stay on the caller, in today's order. With 0 the
    /// `AUTOCRAFT_STEP_THREADS` environment variable sets it. A region goes
    /// to the other threads only when its last two steps each took longer
    /// than waking one costs (timed at the start), and two or more do;
    /// `stepAlways` (or `AUTOCRAFT_STEP_ALWAYS=1`) sends every region with a
    /// pilot, for the tests that the threads change nothing.
    int64_t stepThreads = 0;
    bool stepAlways = false;
    /// The canyon game (Z2, WorldCanyon.cpp) in every canyon zone (`--canyons
    /// 0` turns it off); it runs whether the director spawns or not.
    bool canyons = true;
    /// S2: the longest time the server was down that a start catches the
    /// world up on (`WorldMap::catchUp`): a day of the Mac's clock.
    double catchUpCap = 24 * 3600;
    /// P1: frontier pilots the server runs (`--frontier N`, 4 to 6; 0: none),
    /// and their dice (0: seeded by the world's id).
    int64_t frontier = 5;
    uint64_t frontierSeed = 0;
};

/// The Matriarch's nest in a world (N3; WorldNest.cpp): the groups' keys
/// (`Poi::nest`) and their places round her (`Matriarch` has her numbers).
struct WorldNest {
    static constexpr int64_t queenKey = 0;
    static constexpr int64_t packs = 4;
    static constexpr int64_t flyersKey = 5;
    static constexpr int64_t escortKey = 9;
    /// The Shardling packs linger this far out from her, each in a site
    /// `packSite` wide; her Gloamwings over her within `flyersSite`.
    static constexpr double packOut = 11;
    static constexpr double packSite = 6;
    static constexpr double flyersSite = 10;
    /// A roar is told on the wire this long.
    static constexpr double roarTold = 4;
};

/// The hunt (N4, docs/mmorpg-play.md "The hunt"; WorldHunt.cpp): the
/// Matriarch as the realm's raid, its numbers.
struct WorldHunt {
    /// The gathering: two minutes from the call; the hunt fails ten minutes
    /// after the call if she still stands (world seconds: the world's clock
    /// runs with the wall clock while the server runs).
    static constexpr double gather = 120;
    static constexpr double lasts = 600;
    /// The ring round her nest, and how near its place on it a hunter is
    /// there; a hunter's place on the ring every `spacing` cells.
    static constexpr double ring = 30;  // Matriarch::nestRing
    static constexpr double there = 4;
    static constexpr double spacing = 6;
    /// A guard holding the ring fights a bug within this many cells.
    static constexpr double guard = 10;
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
    /// raids the Citadel itself (its corner turrets answer, D1) and other
    /// buildings only at least `raidFromCitadel` from it.
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

/// The canyon game (Z2, docs/canyon.md; WorldCanyon.cpp): a canyon zone's
/// waves of the Brood along its paths to the prize, its numbers.
struct WorldCanyon {
    /// One group of a wave: `count` bugs of `kind` (and `escort` Shardlings
    /// in a group of their own) down path `path` (taken modulo the zone's
    /// paths; wave 1's turns with each replay).
    struct Group {
        size_t path = 0;
        UnitKind kind = UnitKind::shardling;
        int64_t count = 0;
        int64_t escort = 0;
    };
    struct Wave {
        std::vector<Group> groups;
        /// A mini-boss wave: the hoard (Lodebacks with a heavy escort).
        bool boss = false;
    };
    /// The waves, 1 to N (N = 10 for every zone, The Fork first); a zone's
    /// own table by its id (Z3: The Drowned Cut's waders, The Roost's
    /// flyers), The Fork's for any other.
    static const std::vector<Wave>& waves();
    static const std::vector<Wave>& waves(std::string_view zone);
    /// Z3: a wave's pay by its region's brood level: × 1 at level 2, × 1.3
    /// at level 3 (and on).
    static double payScale(int64_t broodLevel);
    /// Z3, a flooded floor: the Brood's walkers in it are slowed for this
    /// long past each think (the canyon's think renews it).
    static constexpr double floodHold = 0.4;
    static int64_t count() { return int64_t(waves().size()); }
    /// WAITING: wave 1 comes this long after the zone is quiet (a reset, the
    /// hold's end, the server's start) and again this often while it is not
    /// beaten, counted only while a pilot is in the region or next to it.
    static constexpr double firstWait = 60;
    static constexpr double replayEvery = 180;
    /// RUNNING: the break between two waves (its countdown on the panel).
    static constexpr double breakFor = 30;
    /// A wave's time: its walk down the longest path at its slowest bug's
    /// march, and this; past it, a bug of the wave on the prize fails it.
    static constexpr double hold = 90;
    /// The fail mark: ore of the prize the wave's bugs take (eaten into a
    /// Lodeback's sac and carried off, or gnawed by Shardlings).
    static constexpr int64_t failOre = 300;
    /// A Shardling of a wave on the prize, with nothing to fight, gnaws this
    /// much ore a second from the prize's deposit nearest it.
    static constexpr double gnaw = 0.5;
    /// "On the prize": within this many cells of the prize's middle.
    static constexpr double onPrize = 9;
    /// FAILED: the Matriarch's siege, with this many Shardlings; it ends
    /// when she goes home, or after `failedMax` s (then every turret left
    /// in the zone falls as if she came).
    static constexpr int64_t siegeEscort = 6;
    static constexpr double failedMax = 600;
    /// HELD: this many world days.
    static constexpr double heldDays = 1;
    /// The canyon's think: every this many seconds.
    static constexpr double think = 0.25;
    /// A wave beaten pays its fighters, split by the damage each did to the
    /// wave's bugs (its Prospector, and the defences it built while on line),
    /// at least `floorShare` each: XP, ore and MH rising with the wave (80,
    /// 60 and 16 × n; B2: 0.4 of Z2's 200, 150 and 40, docs/balance.md); the
    /// last wave pays `lastBonus` times and a rare blueprint copy to each.
    static double xp(int64_t wave);
    static int64_t ore(int64_t wave);
    static int64_t hydrogen(int64_t wave);
    static constexpr double floorShare = 0.1;
    static constexpr double lastBonus = 2;
};

/// The buildings' jobs for the pilot in a world (P6, docs/prospector-progression.md
/// "Buildings in the MMO"; WorldRoles.cpp): where a fallen Prospector comes
/// back, and the realm's Landing Pads (F2) the Dropship flies between.
struct WorldRoles {
    /// A Spacedock or a Landing Pad counts as one within this many cells of
    /// its edge (at least the pilot's pad reach from its centre).
    static constexpr double buildingPadReach = 2.0;
};

/// Frontier pilots (P1, docs/frontier.md; WorldFrontier.cpp): the pilots the
/// server runs so a world with one human is not empty. Each is a pilot in
/// the world by the pilot's rules (its own wallet, XP, skills, blueprints
/// bought at the shop, the Citadel's tax), with a callsign, a paint and a
/// habit. Their numbers and caps.
struct WorldFrontier {
    /// Miner: drills and hauls, builds Derricks and Hab Domes by its fields.
    /// Builder: realm buildings where they help (a Landing Pad, a Hab Dome,
    /// a Gatling Gun by a raided field). Defender: patrols the realm's
    /// buildings, fights bugs, mends, joins a canyon wave a human holds.
    /// Wanderer: crosses regions on foot and by Dropship, opens sites,
    /// takes Citadel missions.
    using Habit = net::FrontierHabit;
    using Doing = net::FrontierDoing;
    /// The pilots a world runs (`WorldParams::frontier`, 4 to 6), the `i`th
    /// one's habit: miner, builder, defender, wanderer, then miner, builder.
    static Habit habitOf(size_t i);
    /// Their callsigns (the tag reads "VESPER · LV 14").
    static const std::vector<std::string>& callsigns();
    /// Their paints: the team palette's green, gold, violet, teal and pink
    /// (never the pilots' blue, the red, the mercenaries' orange).
    static constexpr std::array<int32_t, 5> paints{2, 3, 4, 5, 7};
    /// They come this long after the first human finishes the opening chain
    /// (O1), one every `arriveEvery` (world seconds), by Dropship.
    static constexpr double arriveAfter = 45;
    static constexpr double arriveEvery = 40;
    /// Their level when they come: the first human's, the `i`th one this
    /// many levels on (wrapping), at least 2.
    static int64_t levelAt(int64_t human, size_t i);
    /// Their think, world seconds.
    static constexpr double think = 1.0;
    /// While a human is on the opening chain, nothing of theirs comes within
    /// this many cells of its places (the start pad, the chain's sites, the
    /// start region's first deposit and well).
    static constexpr double keepOff = 14;
    /// Booked work, where no human is near: an unskilled Prospector's trips,
    /// `Rules::carry` ore over its drilling and a walk of `bookWalk` cells
    /// each way: ore a second.
    static constexpr double bookWalk = 12;
    static double bookRate();
    /// One pump trip in this many is MH (a miner with a Derrick by its field).
    static constexpr int64_t pumpEvery = 4;
    /// Caps on what they build, so the realm is never overbuilt and its
    /// limits stay the humans': in the whole world at most `habDomes` Hab
    /// Domes, `derricks` Derricks and `gatlings` Gatling Guns of theirs, a
    /// Landing Pad a region (`landingPads`); a kind only while the realm
    /// has two or more under its limit (one is always left for a human);
    /// a Gatling Gun only by a raided field with none of the realm's turrets
    /// within `turretField`; at most `buildingsInAll` of theirs; none within
    /// `apart` cells of another building of the realm.
    static constexpr int64_t habDomes = 1;
    static constexpr int64_t derricks = 2;
    static constexpr int64_t gatlings = 2;
    static constexpr int64_t landingPads = 1;
    static constexpr double turretField = 12;
    static constexpr int64_t buildingsInAll = 8;
    static constexpr double apart = 6;
    /// A field was raided when a group of the Brood lingered at one of its
    /// deposits within this many world seconds.
    static constexpr double raidMemory = 900;
    /// What they keep back when they buy: ore and MH.
    static constexpr int64_t reserveOre = 50;
    static constexpr int64_t reserveHydrogen = 10;
    /// A defender guards its region's realm buildings: bugs within this of
    /// one draw it; it patrols between them.
    static constexpr double guard = 16;
    /// A frontier pilot's hp share under which it walks away from a fight
    /// (and calls for help).
    static constexpr double flee = 0.3;
    /// KSTR radio (text): at most one line every `radioEvery` world seconds
    /// across the world, chatter at most every `chatterEvery`.
    static constexpr double radioEvery = 45;
    static constexpr double chatterEvery = 600;
    /// A walk to the next region's opening counts as there within this.
    static constexpr double edgeReach = 2.5;
};

/// A frontier pilot's next act (`WorldMap::frontierDecide`): the habit's
/// decision, made on the world's state, in plain data (the tests read it).
struct FrontierPlan {
    enum class Act : uint8_t { rest, mine, haul, pump, buy, build, repair, fight, patrol, open, walk, fly, mission };
    Act act = Act::rest;
    /// buy, build: the building.
    StructureKind kind = StructureKind::habDome;
    /// A ground point in the pilot's region: where it goes, or builds.
    Vec2 at{0, 0};
    /// walk, fly, mission: the region it goes to.
    size_t region = 0;
    /// mine: a deposit; pump, repair, build's depot: a building; fight: a
    /// bug; open: a site (index in the region's).
    int64_t target = -1;
    /// Why, one line (the log, the tests).
    std::string why;
};

/// A Landing Pad (F2): a finished Landing Pad or Spacedock of the realm's in
/// region `region`, where the Dropship sets down. Every pilot uses every
/// one. `name` is what the Fly request names with the region: "Landing Pad
/// 1" or "Landing Pad 2" (by building id within the region) or "Spacedock".
struct LandingPad {
    size_t region = 0;
    int64_t building = 0;
    StructureKind kind = StructureKind::landingPad;
    std::string name;
    /// The building's centre on the region's ground.
    Vec2 at{0, 0};
    bool operator==(const LandingPad&) const = default;
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
/// The threads a world steps its regions with a pilot on (WorldMap.cpp).
struct StepPool;

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
    /// E1: region `r`'s saved deposits and wells adopted into its region
    /// file's layout (`restore`, after the buildings): by their places
    /// (schema 26), else by index while the file has as many. What the
    /// file no longer lays out is gone, but a well a Derrick stands on is
    /// kept; the file's new veins come in full; a mound under a building is
    /// left empty for good.
    void adopt(size_t r, const SavedRegion& saved);
    bool joined(PilotId pilot) const { return pilots_.count(pilot) > 0; }
    /// The region the pilot is in (in flight: the one it lifted off from).
    std::optional<size_t> where(PilotId pilot) const;
    Region* regionOf(PilotId pilot);
    /// Region changes so far.
    int64_t visit(PilotId pilot) const;
    bool flying(PilotId pilot) const;

    /// Dev lines (Server::devControl) act on the first pilot on line; `as
    /// N LINE` on the N-th by its id (W5: 2, the second human). Nil: none.
    std::optional<PilotId> devPilotId(int64_t n) const {
        if (n < 1) return std::nullopt;
        std::vector<std::pair<int64_t, PilotId>> order;
        for (const auto& [id, p] : pilots_) {
            const auto o = joinOrder_.find(id);
            order.push_back({o == joinOrder_.end() ? 0 : o->second, id});
        }
        std::sort(order.begin(), order.end());
        return size_t(n) <= order.size() ? std::optional(order[size_t(n - 1)].second) : std::nullopt;
    }
    std::optional<PilotId> devAs;

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

    /// C4 (docs/haul-and-ferry.md section 3): the take-off button. The
    /// pilot's driven unit is in a grounded Dropship's bay at its console,
    /// the ramp shut (`Simulation::takeOffRefusal`); it flies to Landing Pad
    /// `pad` of `region`, a region the pilot has been to, everyone in the bay
    /// aboard. The pilot pays (`ac::Flight::price` from the pad the ship
    /// stands on: half from a Spacedock, nothing with Ringwalker). At the
    /// dash's end the ship and its riders move into the target region (each
    /// human's region change told); it touches down on the target pad with
    /// the ramp shut. The reason when refused.
    std::optional<std::string> fly(PilotId pilot, std::string_view region, std::string_view pad);
    /// C4: call the walk-in Dropship down onto the realm Landing Pad the
    /// driven unit stands on (within its edge and `buildingPadReach`, or
    /// Pad link's cells past its edge; a Spacedock's too), its ramp toward
    /// the caller. No destination and
    /// no fare yet. The reason when refused.
    std::optional<std::string> callShuttle(PilotId pilot);
    /// C4: the ramp button of the Dropship the driven unit is in
    /// (`Simulation::ramp`). The reason when refused.
    std::optional<std::string> shuttleRamp(PilotId pilot, bool open);
    /// C4: the Dropship the pilot's driven unit is in (riding it in flight,
    /// or in a grounded one's bay or on its ramp): its region and id. Nil:
    /// none.
    std::optional<std::pair<size_t, int64_t>> shuttleOf(PilotId pilot) const;
    /// C4: the pilot rides a Dropship in flight (`flying` says so too).
    bool riding(PilotId pilot) const;
    /// C4: the Dropship that stands on, or comes down onto, Landing Pad
    /// `pad` of region `r` (its id there; nil: none).
    std::optional<int64_t> shuttleAt(size_t r, int64_t pad) const;
    /// The pilot's pad reach (Pad link).
    double padReach(PilotId pilot) const;
    /// The flight's plan from the pilot's region to region `to` (its grid
    /// steps, Short hop, Ringwalker).
    Ferry::Plan ferryPlan(PilotId pilot, size_t to) const;
    /// The realm's Landing Pads across the world (F2).
    std::vector<LandingPad> landingPads() const;
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

    // MARK: The world runs on (S2, docs/mmorpg.md "The world runs on")

    /// No pilot on line: the world's clock moves `seconds` on (the server's
    /// real time since the last), and what runs on it with it: the sky, the
    /// sites, contracts and missions, starfalls, respawns, the Foundry and
    /// Lab jobs, the wells growing back, the Brood's director (no pilot near:
    /// no spawn, no raid; the bugs left go after `WorldBrood::idleDespawn`).
    /// The regions are held still (their units wait, as while the server is
    /// down); the server calls it at a
    /// few Hz instead of stepping at 60. Under a second each: a starfall's
    /// schedule reads each step's span.
    void idle(double seconds);
    /// The server was down `seconds` (the Mac's clock, from the save's
    /// `savedAt`): the world's clock jumps on that far, at most
    /// `WorldParams::catchUpCap`, as a restart's did; the jobs and timers
    /// on it are due at once, starfalls skipped in the gap are not told,
    /// the regions resume as saved. Then `offlineWork`. The seconds moved.
    double catchUp(double seconds);
    /// What happened while the server was down, `seconds` of it (capped):
    /// called once by `catchUp`. H2 pays the mercenaries for the gap here.
    void offlineWork(double seconds);

    /// What the pilot is told now (its region's snapshot, the game stamped
    /// with its visit).
    Snapshot snapshot(PilotId pilot, const InterestParams& params = {});
    /// A snapshot round over every region (`Region::beginSnapshots`): call
    /// `snapshot` for each pilot between them, with no step, input or
    /// command in between.
    void beginSnapshots();
    void endSnapshots();
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
    /// W5: with the human pilots on line, each with the Prospector it drives.
    std::optional<net::Sky> sky() const;
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
    /// The realm's buildings of a kind across the world, finished or not.
    int64_t realmCount(StructureKind kind) const;
    /// What a kind's limit counts for a pilot in region `region`: the
    /// realm's buildings of it in that region, finished or not (C3: every
    /// limit is a region's, `Progression::perRegion`; else `realmCount`).
    int64_t limitCount(StructureKind kind, size_t region) const;
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

    // MARK: Realm projects (R2; docs/mmorpg-play.md "Realm projects"; WorldProjects.cpp)

    /// A project running: its kind, the world clock it ends at, who started
    /// it (a callsign).
    struct Project {
        RealmProject::Kind kind = RealmProject::Kind::beacon;
        double ends = 0;
        std::string by;
        bool operator==(const Project&) const = default;
    };
    /// Each kind's project running now (nil: none), by kind.
    const std::array<std::optional<Project>, RealmProject::count>& projects() const { return projects_; }
    /// How many of `kind` the realm has started (the price rises with them).
    int64_t projectRuns(RealmProject::Kind kind) const { return projectRuns_[size_t(kind)]; }
    bool projectOn(RealmProject::Kind kind) const { return projects_[size_t(kind)].has_value(); }
    /// What `kind` costs the realm wallet now.
    RealmProject::Price projectPrice(RealmProject::Kind kind) const { return RealmProject::price(kind, projectRuns(kind)); }
    /// Why the pilot with key `key` can't start `kind` now (one of each
    /// kind at a time, the realm wallet short; a frontier pilot never), or
    /// nil.
    std::optional<std::string> projectRefusal(const std::string& key, RealmProject::Kind kind) const;
    /// Starts `kind`, paid from the realm wallet, for a world day: any human
    /// pilot, from the Citadel's REALM tab (at the realm's Citadel, where it
    /// unloads). Every pilot on line is told. The reason when refused.
    std::optional<std::string> startProject(PilotId pilot, RealmProject::Kind kind);
    /// Saves and tests: the projects and their runs as they are.
    void setProjects(const std::array<std::optional<Project>, RealmProject::count>& p,
                     const std::array<int64_t, RealmProject::count>& runs);

    // MARK: The defences (D1, D2; docs/mmorpg-m8.md; WorldDefence.cpp)

    /// The marks the realm's Citadel turrets have (saved with the world).
    TurretMarks citadelMarks() const { return citadelMarks_; }
    void setCitadelMarks(const TurretMarks& m) { citadelMarks_ = m; }
    /// The realm's Citadel turrets take their next mark of `stat`, paid
    /// from the realm wallet (`Defence::citadelPrice`), by any pilot
    /// standing where it unloads at the realm's Citadel. The reason when
    /// refused (away from the Citadel, the last mark, the wallet short).
    std::optional<std::string> upgradeCitadel(PilotId pilot, DefenceStat stat);
    /// D3: placed turret `turret` (a Sentinel or a Gatling Gun of the
    /// realm in the pilot's region) takes its next mark of `stat`, paid
    /// from the pilot's own wallet (`Defence::turretPrice`, no tax), by any
    /// pilot within `Defence::turretReach` of it. Every pilot in the region
    /// is told who paid. The reason when refused (no such turret, not
    /// finished, too far, the wallet short, the last mark).
    std::optional<std::string> upgradeTurret(PilotId pilot, int64_t turret, DefenceStat stat);
    /// C1: a pilot comes back at a complete Citadel here, from its centre:
    /// in the hall, away from the door (on the +y face).
    static constexpr Vec2 citadelRespawn{0, -3};
    /// The bugs building `id` of region `r` has killed (D3: the turret card).
    int64_t turretKills(size_t r, int64_t id) const;

    // MARK: - The Citadel's interior (C1, WorldCitadel.cpp, docs/citadel-interior.md)

    /// The pilot opens (`open`) or closes the door of the realm's complete
    /// Citadel nearest its Prospector in its region (`Simulation::door`:
    /// within `Act::serverReach` of the door's leaf or panels, either side; to close,
    /// the doorway clear). The reason when refused.
    std::optional<std::string> door(PilotId pilot, bool open);
    /// The pilot's Prospector mans corner gun `gun` (0 to 3) of the realm's
    /// Citadel it is sheltered in (`Simulation::man`: within
    /// `Act::serverReach` of the gun's console, the gun free), or
    /// leaves the gun it mans (-1). The reason when refused.
    std::optional<std::string> man(PilotId pilot, int gun);
    /// A human pilot is on line in region `r`, on the ground (not in
    /// flight): C1, the Brood raids a Citadel only then (its guns need a
    /// gunner; nobody there, it is not a raid's target).
    bool humanIn(size_t r) const;
    /// Blueprint site `i` of region `r` lies under a complete Citadel
    /// (`Simulation::swallowedByCitadel`, the predicate the game hides its
    /// crate by): never seen, never opened, never a contract's.
    bool siteSwallowed(size_t r, size_t i) const;
    /// Schema 27 (`restore`, after the buildings): each Citadel's grown
    /// footprint, its bounding square + 1, cleared of ore (the deposits
    /// emptied for good); the sites there are swallowed (`siteSwallowed`),
    /// the map's doodads there are the core's and the game's (C1, the shared
    /// predicate); another building overlapping it is left. One line each
    /// for the log.
    std::vector<std::string> adoptCitadels(size_t r);
    /// The last `restore`'s `adoptCitadels` lines, every region's.
    const std::vector<std::string>& grownCitadels() const { return grown_; }
    /// Schema 28 (C2, `restore`): a pilot saved before blueprints were
    /// learned learns every kind it built (`built`) or holds a copy of (the
    /// kinds it bought come learned from the core's decode of the old
    /// `bought`); the copies of the kinds it knows are dropped. One line for the log, or nothing when there was nothing
    /// to learn. Idempotent.
    static std::optional<std::string> adoptLearned(ProspectorProgress& g, const std::set<StructureKind>& built, const std::string& who);
    /// The last `restore`'s `adoptLearned` lines, the frontier's included.
    const std::vector<std::string>& learnedOnLoad() const { return learnedOnLoad_; }
    /// Dev and the flow tests (`citadel …` lines), as the seated pilot (`as
    /// N` for another): `citadel here` (a complete, armed Citadel of the
    /// realm on clear ground ahead, door shut, the Prospector 4 cells out
    /// from its door facing it; its id), `citadel door open|close` and
    /// `citadel man N|none` (the Door and Man requests).
    std::string devCitadel(const std::string& line);
    /// Dev and the flow tests (C4, `carry ore|mh N`, as the seated pilot):
    /// the pilot's Prospector holds a load of N ore or MH in its gripper
    /// (`carry none` empties it), for the grab's pictures.
    std::string devCarry(const std::string& line);
    /// Dev and the flow tests (C2, `dummy here [D]`, as the seated pilot): a
    /// complete Target Dummy of the realm D cells (20; 2 to 80) ahead of
    /// the pilot's unit (along its gun's aim when it mans one), on the
    /// nearest clear ground to that (within 30% of D, at most 6 cells); its
    /// id.
    std::string devDummy(const std::string& line);
    /// Dev and the mmosize flow (C5, WorldStreet.cpp): finished buildings
    /// of the realm for the pictures of their sizes, those of an earlier
    /// line taken away first. `street KIND` (habdome, garrison, bastion,
    /// foundry, spacedock, lab, sentinel, gatling, pad, derrick): one ahead
    /// of the pilot on clear ground (a Derrick on the nearest free well), its
    /// Prospector at its front corner; its id. `street all`: a Citadel of the
    /// realm on the nearest open ground that takes it and one of each kind
    /// down both sides of a road 8 cells wide out of its door, a Derrick on
    /// a well near it, the Prospector at the door; where each went.
    std::string devStreet(const std::string& line);
    /// The share of a kill's XP a defence's builder earns when the defence
    /// hits a bug (docs/creatures.md, "XP and rewards").
    static constexpr double defenceXpShare = 0.5;

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
    /// C2: learns the building `kind` from a blueprint copy the pilot
    /// holds, anywhere, using the copy up (docs/guns-and-practice.md,
    /// section 4). The reason when refused (it knows the kind, or holds no
    /// copy).
    std::optional<std::string> learn(PilotId pilot, StructureKind kind);

    // MARK: The Mule (C4, docs/haul-and-ferry.md section 2; WorldMule.cpp)

    /// The pilot buys the Mule at finished realm Foundry `foundry` of its
    /// region, paid from its wallet (`Mule::ore`, `Mule::hydrogen`); it rolls
    /// out of the Foundry's door `Mule::rollOut` s later (the world's clock,
    /// on line or not). One a pilot: refused while it has one (rolling out
    /// or out); a destroyed one frees the slot. C5: its unit within
    /// `Act::serverReach` of the Foundry's call post (`Act::foundryPost`).
    std::optional<std::string> buyMule(PilotId pilot, int64_t foundry);
    /// The pilot's Prospector, on foot with a load, puts it all into its own
    /// Mule `mule` (`Simulation::muleFill`: within `Act::serverReach` of
    /// the hopper, refused at `Mule::capacity`). No XP, no wallet: the Mule books
    /// it when driven into a Citadel's hall (`Simulation::stepMules`; the XP
    /// and the rest in `workAfter`, the realm's cut in `realmTaxes`).
    std::optional<std::string> muleFill(PilotId pilot, int64_t mule);
    /// The Take request (the server's, every mode): in a world, Take on the
    /// pilot's own Mule climbs aboard it at its perch (`Simulation::
    /// muleBoard`) and drives it; Take on its own Prospector while driving
    /// the Mule climbs off. Another's Mule: refused. Anything else as
    /// `Region::take`. The reason when refused.
    std::optional<std::string> take(PilotId pilot, int64_t unit);
    /// The HandBack request: off the Mule while driving it (a world), else
    /// the driven unit back to its AI. The reason when refused.
    std::optional<std::string> handBack(PilotId pilot);
    /// The pilot's Mule: its region and its unit there (nil: none, or still
    /// rolling out).
    std::optional<std::pair<size_t, int64_t>> mule(PilotId pilot) const;
    /// It drives its Mule now.
    bool drivesMule(PilotId pilot) const;
    /// Dev and the flow tests (`mule …` lines, as the seated pilot; `as N`
    /// for another): `mule foundry` (a complete Foundry of the realm on
    /// clear ground ahead, the Prospector 3 cells out from its door facing
    /// it; its id), `mule here` (the pilot's Mule out at once beside its
    /// Prospector, free; refused while it has one), `mule ready` (the one
    /// rolling out is out now), `mule door` (the Mule out of the realm's
    /// Citadel here on its door's axis facing in, 9.5 cells from its middle,
    /// the door open, the Prospector on foot beside its hopper), `mule load N [MH]` (its
    /// Mule holds N ore and MH MH).
    std::string devMule(const std::string& line);
    /// C3: the pilot demolishes building `structure` of the realm in its
    /// region (WorldDemolish.cpp, docs/guns-and-practice.md "Limits per
    /// region and demolish"), finished or a scaffold: driving, within the
    /// repair reach of its unit (`Simulation::demolishReach`, and
    /// `demolishSlack`); from the top-down view (nothing driven), anywhere
    /// in the region. Half its price comes back to the pilot's own wallet
    /// (`Simulation::salvage`); the building leaves at once (its shatter,
    /// the `Demolished` event to the pilot), and the region's count for its
    /// limit falls. Refused: the realm's last finished Citadel (a home must
    /// remain), a Citadel with someone in its hall or at its guns
    /// (`Simulation::demolishRefusal`). A frontier pilot working on it
    /// stops. Frontier pilots never demolish.
    std::optional<std::string> demolish(PilotId pilot, int64_t structure);
    /// How far past the repair reach a demolish may be asked from, in
    /// cells: the unit moves on while the request travels.
    static constexpr double demolishSlack = 1.5;
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
    /// The most falls a pilot's snapshot lists (`fallsFor`).
    static constexpr size_t skyFallsMost = 24;
    std::optional<Fall> forceStarfall(double in, std::optional<size_t> region = std::nullopt,
                                      std::optional<PilotId> near = std::nullopt);
    /// Whether ground point `at` of region `r` takes a crater: open, flat
    /// ground joined to the region's pads, clear of its sites, pads, ore and
    /// wells, and of the map's edge (E1: buildings and units are in the
    /// blast's way, not the crater's). Its blast never reaches the home's
    /// ground (`homeSpared`): the start region's Citadels, or with none its
    /// pads, and `Starfall::homeSpared` cells round them.
    bool craterFits(size_t r, Vec2 at) const;
    /// E1: whether a blast at `at` in region `r` reaches the home's ground.
    bool reachesHome(size_t r, Vec2 at) const;
    /// The starfall dice's seed (`params().starfallSeed`, or the world's id).
    uint64_t starfallSeed() const;

    // MARK: The Brood (docs/creatures.md; WorldBrood.cpp)

    /// What draws a group: a point of interest of its region.
    enum class Poi : uint8_t { deposit, well, site, pad, building, crater, nest };
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
        /// X2: the raided building's kind, kept for the away report's "took".
        std::optional<StructureKind> raidKind;
        std::optional<int64_t> prey;
        /// A defence of the realm that shot it (D1, D2): the group attacks it
        /// while it stands within the leash, before its raid's building.
        std::optional<int64_t> turret;
        /// Dev and shots (`defence air|flyers`): it hovers where it is, never
        /// turning on a pilot or a defence, never leaving.
        bool still = false;
        /// The Matriarch's nest (N3, `Poi::nest`): her horde (it lingers
        /// always and chases within the nest's ring), or the queen herself
        /// (her orders are the nest's, WorldNest.cpp).
        bool nest = false;
        bool queen = false;
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
    /// A canyon zone's path (Z1; Z2's waves): a group of `count` bugs of
    /// `kind` (with `escort` Shardlings) out of the mouth of path `path` of
    /// zone `zone` of region `r`, walking the path along the canyon's floor
    /// to the prize (its site the prize deposit at the path's end, a
    /// Lodeback's meal; a well when the prize has no ore). Nil: no such zone
    /// or path, or no brood side.
    std::optional<int64_t> sendAlongPath(size_t r, const std::string& zone, size_t path, UnitKind kind, int64_t count,
                                         int64_t escort = 0);
    /// Tests: the director's next group in region `r` comes now.
    void broodNow(size_t r);
    /// Dev and shots (`--dev-control`'s `brood …` lines): a scene near the
    /// first pilot on line (`brood zone PATH [KIND COUNT]`: a group along a
    /// path of its region's first zone; `brood zone here`: the pilot to the
    /// first zone's region, in sight of its prize). What it did, or why not.
    std::string devBrood(const std::string& line);
    /// Dev and shots (`defence …` lines, WorldDefence.cpp): the realm wallet
    /// filled (`wallet`), gatling and AA tower blueprint copies for the
    /// first pilot (`blueprints`), a raid on the realm's Citadel nearest it
    /// (`citadel`), a finished gatling gun ahead of it with Shardlings on it
    /// (`gatling`, a raid on it), a finished AA tower with Gloamwings set on it (`aa`).
    std::string devDefence(const std::string& line);

    /// Tests, dev and shots: the pilot's Prospector taken to region `to` at
    /// its ground point `at` at once, handed over as an edge crossing hands
    /// it. False: in flight, no unit, already there, or another pilot drives
    /// there.
    bool devTransfer(PilotId pilot, size_t to, Vec2 at);

    // MARK: The Dropship (F2; WorldFerry.cpp)

    /// Tests, dev and shots: a finished Landing Pad of the realm in region
    /// `r` on open ground at or round ground point `near` (ahead along
    /// `heading` first). Its building id; nil: no room.
    std::optional<int64_t> devLandingPad(size_t r, Vec2 near, std::optional<double> heading = std::nullopt);
    /// Dev and shots (`ferry pad [here|REGION]|seen|go REGION|mh N`), for the
    /// first pilot: what was done, one line.
    std::string devFerry(const std::string& line);

    // MARK: The Matriarch (N3, docs/creatures.md "The Matriarch"; WorldNest.cpp)

    /// Region `r`'s nest, ground point (nil: none).
    std::optional<Vec2> nestAt(size_t r) const;
    /// The region with the Matriarch's nest (nil: none in the world).
    std::optional<size_t> nestRegion() const;
    /// Her unit and the region she is in now (nil: down, or her region
    /// asleep with no pilot near).
    std::optional<std::pair<size_t, int64_t>> matriarch() const;
    /// The world clock she comes back at (nil: she is not down).
    std::optional<double> matriarchBack() const;
    /// Her nest's horde in its region now: Shardlings and Gloamwings alive.
    std::pair<int64_t, int64_t> nestHorde() const;
    /// Who has hurt her since she was last whole: pilot → hp.
    const std::map<PilotId, double>& matriarchDamage() const;
    /// The Z2 hook: she leaves her nest with `escort` Shardlings for region
    /// `r`'s ground point `at`, fells every turret (a Sentinel, a Gatling
    /// Gun) of the realm within `radius` of it, then goes home. False: no
    /// nest, she is down, or out on a siege already.
    /// `from`: where she comes in, out of sight (a canyon's tunnel, Z2;
    /// nil: her nest in its region, else the region's edge).
    bool matriarchSiege(size_t r, Vec2 at, double radius, int64_t escort = 8, std::optional<Vec2> from = std::nullopt);
    /// A siege under way (nil: none): where, and whether she is on her way home.
    struct Siege {
        size_t region = 0;
        Vec2 at{0, 0};
        double radius = 0;
        bool home = false;
        int64_t escort = 0;
    };
    std::optional<Siege> siege() const { return siege_; }
    /// Tests: she comes back now (the return's rule done).
    void matriarchReturnNow();
    /// Dev and shots (`matriarch …` lines): the first pilot taken to the
    /// nest (`here`), she roars at it (`roar`), slams (`slam`), is felled by
    /// it (`down`), back (`back`), a siege at the pilot (`siege`).
    std::string devMatriarch(const std::string& line);
    // MARK: The canyon game (Z2, docs/canyon.md; WorldCanyon.cpp)

    using CanyonState = net::Sky::Zone::State;
    /// A canyon zone's game: its state, its wave, the wave's bugs and what
    /// they took, the damage each pilot did to them.
    struct Canyon {
        size_t region = 0;
        /// Index in the region's `zones`, and its id and name.
        size_t zone = 0;
        std::string id;
        std::string name;
        CanyonState state = CanyonState::waiting;
        /// The wave out, or the next one (1…`WorldCanyon::count()`).
        int64_t wave = 1;
        /// Its bugs are out.
        bool out = false;
        /// World clock: WAITING and RUNNING the next wave (no wave out),
        /// FAILED and HELD their end.
        double next = 0;
        /// The wave out's time runs out (world clock).
        double ends = 0;
        /// Wave 1 sent while WAITING, in all (its arm turns with each).
        int64_t replays = 0;
        /// No pilot in the region or next to it: the waves wait.
        bool paused = false;
        /// The wave's bugs (alive, a body, or gone), those killed, each
        /// Lodeback's sac at the last think; the ore gnawed and carried off.
        std::set<int64_t> bugs;
        std::set<int64_t> killed;
        std::map<int64_t, double> sacs;
        double gnawed = 0;
        int64_t carried = 0;
        /// The entrances the wave came from (bit k: `entrances[k]`).
        uint32_t tunnels = 0;
        /// Hp of the wave's bugs each pilot took (its Prospector, or a
        /// defence it built).
        std::map<PilotId, double> damage;
        /// P1: hp of the wave's bugs each frontier pilot took (its Prospector,
        /// or a defence it built), paid as a pilot's.
        std::map<uint32_t, double> frontierDamage;
        /// FAILED: the Matriarch came (a siege under way, or done).
        bool siege = false;
        /// The prize's ground middle; the zone's turrets stand within
        /// `reach` of it (its farthest build spot and 3 cells).
        Vec2 middle{0, 0};
        double reach = 0;
        /// Bugs of the wave out alive at the last think.
        int64_t left = 0;
        /// Ore the wave out took: gnawed, carried off, in its Lodebacks' sacs.
        int64_t taken() const;
    };
    /// Every canyon zone of the world, in region order.
    const std::vector<Canyon>& canyons() const { return canyons_; }
    /// Zone `zone` of region `r` (nil: none).
    const Canyon* canyon(size_t r, std::string_view zone) const;
    /// Tests, dev and shots: wave `wave` out now (RUNNING; wave 1 while
    /// WAITING is its replay), the wave out failed now, beaten now (its
    /// bugs gone, its fighters paid as they fought), the zone reset (the
    /// wave out taken away at once, wave 1's arm the first again). False:
    /// no such zone, or no brood side.
    bool canyonSend(size_t r, std::string_view zone, int64_t wave);
    bool canyonFail(size_t r, std::string_view zone);
    bool canyonBeat(size_t r, std::string_view zone);
    void canyonReset(size_t r, std::string_view zone);
    /// Dev and shots (`canyon …` lines): the first pilot's region's first
    /// zone: `send N` (wave N out now), `fail`, `beat`, `hold` (the last
    /// wave beaten), `reset`, `turrets G [A [FIRST]]` (G finished Gatling
    /// Guns and A AA towers of the realm beside its paths, built by the
    /// pilot; Z4: no build spots, the first FIRST free places passed over),
    /// `place gatling|sentinel X Y [X Y …]` (guns at those region cells,
    /// under the ordinary build rules),
    /// `break S` (the next wave in S seconds).
    std::string devCanyon(const std::string& line);

    /// Dev and shots (L1): `goal` says the goal shown; `goal pass KEY` passes
    /// one unpaid; `goal level N` gives the first pilot level N's XP and
    /// passes the goals under it unpaid.
    std::string devGoal(const std::string& line);
    /// Dev and shots (K2, Q2): `missions here` draws the first pilot's board
    /// again until an easy survey in its region is on it.
    std::string devMissions(const std::string& line);

    // MARK: Mercenaries (H2, `WorldMercs`; WorldMercs.cpp)

    /// Hires one at the realm's Citadel, standing where it unloads, for the
    /// fee from the wallet; it starts at the nearest field to the Citadel
    /// with ore. Refused below level 5, at the limit, short of the fee, and
    /// with no field. The reason when refused.
    std::optional<std::string> hireMerc(PilotId pilot, bool guard = false);
    /// N4: makes one a guard (it answers the hunt) or a miner again,
    /// anywhere; a guard on a hunt finishes it.
    std::optional<std::string> mercRole(PilotId pilot, uint32_t merc, bool guard);
    /// Lets one go (no refund), anywhere.
    std::optional<std::string> dismissMerc(PilotId pilot, uint32_t merc);
    /// Sends one to field `field` of region `region` (a world region id;
    /// `mercFields`): a field with ore, in a region with a realm depot. In
    /// another region it is there at once, at that depot.
    std::optional<std::string> assignMerc(PilotId pilot, uint32_t merc, std::string_view region, int64_t field);
    /// The deposit fields of region `r`: the region file's own deposits
    /// within `WorldMercs::fieldLink` of each other, each a list of patch
    /// indices; a field's index is its place in the list.
    const std::vector<std::vector<size_t>>& fields(size_t r) const;
    /// A mercenary of a pilot as the tests see it.
    struct MercView {
        uint32_t id = 0;
        std::string name;
        size_t region = 0;
        int64_t field = 0;
        std::optional<int64_t> unit;
        double owed = 0;
        double downUntil = 0;
        int64_t delivered = 0;
        /// N4: a guard; answering a hunt; the region its unit is in.
        bool guard = false;
        bool hunting = false;
        size_t in = 0;
    };
    /// The pilot's mercenaries, on line or off (by key).
    std::vector<MercView> mercs(PilotId pilot) const;
    std::vector<MercView> mercsOf(const std::string& key) const;

    // MARK: Frontier pilots (P1, `WorldFrontier`; WorldFrontier.cpp)

    /// A frontier pilot: a pilot of the world the server runs.
    struct FrontierPilot {
        uint32_t id = 0;
        /// Its key in the save and in `builtBy` ("frontier:vesper"; never a
        /// human's, which are 32 hex digits), its callsign, paint and habit.
        std::string key;
        std::string callsign;
        WorldFrontier::Habit habit = WorldFrontier::Habit::miner;
        int32_t paint = 2;
        ProspectorProgress progress;
        Wallet wallet;
        Wallet taxed;
        /// Come (by Dropship) or still to come, at world clock `comes`.
        bool arrived = false;
        double comes = 0;
        /// Its region, and its Prospector's ground point there (kept while it
        /// has no unit: booked, down or on its way); its unit while live.
        size_t region = 0;
        Vec2 at{0, 0};
        std::optional<int64_t> unit;
        /// What it carries (MH if `hydrogen`).
        int64_t cargo = 0;
        bool hydrogen = false;
        WorldFrontier::Doing doing = WorldFrontier::Doing::coming;
        /// C4: its walk-in Dropship flight while live (WorldShuttle.cpp,
        /// `frontierRide`): the ship (its id in `region`), the target pad,
        /// the fare, and the step it is at: the ship called, walking in,
        /// shutting, at the console, in flight, opening, walking out.
        struct Ride {
            enum class Step : uint8_t { called, walkIn, shut, console, flying, open, walkOut };
            int64_t ship = 0;
            LandingPad target;
            int64_t price = 0;
            Step step = Step::called;
            /// Seconds at this step (a ship that never comes is given up).
            double since = 0;
        };
        std::optional<Ride> ride;
        FrontierPlan plan;
        /// Down (its Prospector fell): back at world clock `downUntil`.
        double downUntil = 0;
        /// On its way to region `travelTo` (through an opening on foot, or
        /// aboard the Dropship when `aboard`): there at world clock `until`,
        /// at its ground point `travelAt`.
        std::optional<size_t> travelTo;
        Vec2 travelAt{0, 0};
        double until = 0;
        bool aboard = false;
        /// The building it welds (its id in `region`).
        std::optional<int64_t> building;
        /// A site it holds open since world clock `openSince` (index in its
        /// region's sites).
        std::optional<size_t> opening;
        double openSince = 0;
        /// A wanderer's Citadel mission: survey region `missionRegion` (taken
        /// at the realm's Citadel, paid on reaching its middle), its tier.
        std::optional<size_t> missionRegion;
        net::CitadelMission::Tier missionTier = net::CitadelMission::Tier::easy;
        /// Booked ore not whole yet; trips since its last pump.
        double owed = 0;
        /// A booked mend's price not whole yet, ore and MH (`Simulation::
        /// repairCost`; not saved).
        double mendOre = 0;
        double mendHydrogen = 0;
        int64_t trips = 0;
        /// Its last think (world clock); its counts.
        double thought = -1e9;
        int64_t built = 0;
        int64_t kills = 0;
        /// Its hp after the last step (a fall, a cry for help).
        double hp = 0;
        double lastCry = -1e9;
    };
    /// Every frontier pilot, come or still to come (tests may change them).
    const std::vector<FrontierPilot>& frontier() const { return frontier_; }
    std::vector<FrontierPilot>& frontier() { return frontier_; }
    FrontierPilot* frontierPilot(uint32_t id);
    /// The first human pilot (any pilot kept by key, on line or not) has
    /// finished the opening chain: the frontier pilots may come.
    bool frontierOpen() const;
    /// Some human on line is on the opening chain now.
    bool chainRunning() const;
    /// The opening chain's places in region `r` (ground points): the start
    /// pad, the chain's sites, the start region's deposit nearest its pad
    /// and its well nearest the Derrick's wreck. None outside the start region.
    std::vector<Vec2> chainPlaces(size_t r) const;
    /// Ground point `at` of region `r` is within `WorldFrontier::keepOff`
    /// of a chain place while a human is on the chain.
    bool keptOff(size_t r, Vec2 at) const;
    /// A frontier pilot's Prospector is simulated (live): come, on its feet,
    /// not on its way, and a human in its region or next to it. Else its
    /// work is booked on the world's clock.
    bool frontierLive(const FrontierPilot& f) const;
    /// The roster now (`frontierOpen` and none yet): `WorldParams::frontier`
    /// pilots, coming one after another. Done by the step; tests call it.
    void frontierRoster();
    /// The habit's decision now (no change to the world).
    FrontierPlan frontierDecide(const FrontierPilot& f) const;
    /// The rules, as for a pilot: a blueprint copy bought at the realm's
    /// Citadel (the pilot's unit, or its booked place, where it unloads; the
    /// shop's price from its own wallet), a building put up (its blueprint,
    /// its floor and the realm's limits, the frontier caps, the pad plates,
    /// the ground, the world's price from its wallet), a Dropship flight
    /// from a realm Landing Pad to one in region `to` (`Flight::price`), the
    /// Citadel mission taken. The reason when refused.
    std::optional<std::string> frontierBuy(uint32_t id, StructureKind kind);
    std::optional<std::string> frontierBuild(uint32_t id, StructureKind kind, Vec2 at);
    std::optional<std::string> frontierFly(uint32_t id, size_t to);
    /// C4: one step of `f`'s walk-in Dropship flight (`FrontierPilot::Ride`):
    /// it walks, the server presses the buttons for it. False: the ride is
    /// over (or given up: it was booked, or the ship left).
    bool frontierRide(FrontierPilot& f, double dt);
    /// C4: `f`, live, standing at Landing Pad `from`, calls the Dropship
    /// there for a flight to region `to`'s first Landing Pad (`f.ride`).
    /// False: no pad there, or the fare is short (it flies booked instead).
    bool frontierCall(FrontierPilot& f, const LandingPad& from, size_t to);
    std::optional<std::string> frontierMission(uint32_t id, size_t to);
    /// Why it may not build a `kind` at ground point `at` of region `r` by
    /// the frontier's caps (none: it may).
    std::optional<std::string> frontierCap(size_t r, StructureKind kind, Vec2 at) const;
    /// The canyon wave pay a frontier pilot earned (Z2: its share by damage).
    /// The radio lines told so far (tests).
    const std::vector<std::string>& radioLog() const { return radioLog_; }
    /// Dev and shots (`frontier …` lines): `here HABIT [SIDE [poor]]` (one of
    /// that habit beside the first pilot, come and live; `poor`: its wallet
    /// emptied, so a miner only drills), `build KIND` (the nearest one
    /// puts up a KIND ahead of it, its blueprint and price given), `radio
    /// hello|raid|cheer|help`, `fight` (the nearest defender at the first
    /// zone's prize with its wave), `list`. What it did, one line.
    std::string devFrontier(const std::string& line);

    /// What the pilot's client is told of its progression now (no news).
    std::optional<net::ProgressMessage> progressMessage(PilotId pilot) const;
    /// What happened to the pilot's progression since the last call.
    std::vector<net::ProgressMessage::News> takeNews(PilotId pilot);
    /// X2: the away report made on the pilot's return, once (nil after).
    std::optional<net::AwayReport> takeAway(PilotId pilot);
    /// X2: the ledger of a pilot off line so far (null: on line or unknown).
    const std::vector<net::AwayRow>* ledgerOf(const std::string& key) const;
    /// X2: the home screen's two lines for every pilot off line, by key
    /// (`net::awaySummary` of its ledger now); the server writes them beside
    /// the save (`Server::writeAway`).
    std::map<std::string, std::array<std::string, 2>> awaySummaries() const;
    /// Q3: the rows in every ledger of a pilot off line (the soak's count).
    size_t ledgerRows() const;
    /// Q3: the ore left in every region's deposits (the soak's count).
    int64_t depositsOre() const;

    // MARK: Realm standings (S4, docs/mmorpg-play.md "Standings"; WorldStandings.cpp)

    /// The realm week's counts by key (a human's 32 hex digits or a frontier
    /// pilot's "frontier:…"): ore delivered, bugs felled, waves held,
    /// buildings raised. Saved with the world (schema 24).
    struct Standing {
        int64_t ore = 0;
        int64_t bugs = 0;
        int64_t waves = 0;
        int64_t built = 0;
        bool operator==(const Standing&) const = default;
    };
    const std::map<std::string, Standing>& standings() const { return standings_; }
    /// The week's start on the world clock, its length (`net::kWeekDays`
    /// world days) and its end.
    double weekStart() const { return weekStart_; }
    double weekLength() const { return double(net::kWeekDays) * params_.dayLength; }
    double weekEnds() const { return weekStart_ + weekLength(); }
    /// The table as pilot `pilot` is told it (nil: not a world, or not
    /// joined): ranked, its own row always in it.
    std::optional<net::StandingsMessage> standingsMessage(PilotId pilot) const;
    /// The week ends now (its top hauler told, the counts start again,
    /// the next week from the clock now): tests, dev and shots. What KSTR
    /// says (`net::weekTopLine`).
    std::string weekEndNow();
    /// Tests: a count added to the pilot with key `key` (frontier keys too).
    void standNote(const std::string& key, int64_t Standing::*stat, int64_t n);
    /// Dev and shots (`week …` lines): `end` (the week ends now), `left S`
    /// (the week ends S seconds from now), `list`. What it did, one line.
    std::string devWeek(const std::string& line);
    /// E1's scenes (shots): `starfall victims`, in the way of the fall told
    /// in the pilot's region and not yet down: a Hab Dome at its heart, a
    /// Gatling and a Sentinel inside its blast, a Hab Dome near its edge (the
    /// realm's, built by the pilot), and Prospectors from the heart to the
    /// edge.
    std::string devStarfall(const std::string& line);

    // MARK: The hunt (N4, docs/mmorpg-play.md "The hunt"; WorldHunt.cpp)

    /// The realm's hunt on the Matriarch: called by a human (free, once a
    /// world day realm-wide); the frontier defenders and the guard
    /// mercenaries (`party`, chosen at the call) walk to their places on
    /// the ring round her nest, or fly the Dropship between the realm's
    /// pads, and hold; at `WorldHunt::gather` seconds, or when the caller
    /// steps inside the ring if later, they go in. Her fall (N3's, her pay
    /// and her return as they are) wins it; the party fallen or
    /// `WorldHunt::lasts` seconds fail it. Saved with the world (schema 25).
    struct Hunt {
        using State = net::HuntMessage::State;
        State state = State::none;
        /// The caller's key and callsign.
        std::string callerKey;
        std::string caller;
        /// World clock: the call, the charge, the end (won or failed).
        double calledAt = 0;
        double chargedAt = 0;
        double endedAt = 0;
        /// The world day of the last call (`contractDay`; none yet: min).
        int64_t day = INT64_MIN;
        /// Her nest: its region and centre.
        size_t region = 0;
        Vec2 at{0, 0};
        /// One of the party: a frontier pilot's key ("frontier:…") or a
        /// mercenary's ("merc:<hirer key>:<id>"), its place on the ring, at
        /// it (since world clock `reached`), fallen in the hunt.
        struct Member {
            std::string key;
            Vec2 slot{0, 0};
            bool there = false;
            double reached = -1;
            bool fell = false;
            /// A frontier pilot down at the call: back at world clock
            /// `downAtCall` (a later fall is the hunt's).
            double downAtCall = 0;
            bool operator==(const Member&) const = default;
        };
        std::vector<Member> party;
        bool gatheredTold = false;
        bool operator==(const Hunt&) const = default;
    };
    const Hunt& hunt() const { return hunt_; }
    /// Why the pilot with key `key` can't call the hunt now (nil: it can,
    /// at the realm's Citadel): a frontier pilot never; none while she is
    /// down, out on a siege, a hunt is on, or the day's was called.
    std::optional<std::string> huntRefusal(const std::string& key) const;
    /// THE HUNT called by pilot `pilot` at the realm's Citadel (where it
    /// unloads); why not.
    std::optional<std::string> callHunt(PilotId pilot);
    /// The hunt as pilot `pilot` is told it (nil: not a world, or not
    /// joined).
    std::optional<net::HuntMessage> huntMessage(PilotId pilot) const;
    /// Dev and shots (`hunt …` lines): `call` (the first pilot calls it,
    /// anywhere), `gathered` (the gathering's two minutes over now), `fail`,
    /// `open` (the day's call open again), `list`. What it did, one line.
    std::string devHunt(const std::string& line);

    /// Wall-clock ms the last `step` took over all the regions.
    double lastStepMs() const { return lastStepMs_; }
    /// With `WorldParams::stepThreads`: what one dispatch to the pool costs,
    /// microseconds, timed when the pool was made (0: no pool), and how many
    /// steps went to it.
    double stepWakeUs() const;
    size_t pooledSteps() const { return pooledSteps_; }

private:
    /// A pilot unit's skill clocks on the way to another region: when its
    /// actives are ready and its bursts end, as seconds from `at` (the
    /// world's clock when it left), and its counts.
    struct Carried {
        Simulation::SkillClock clock;
        double at = 0;
    };
    /// Aboard the Dropship (F2): the Prospector out of every region, from
    /// region `from`'s pad at `boarded` (ground) to the pad `pad` of region
    /// `to` at `at` (ground, its building's centre). The pilot moves into
    /// `to` at tick `switchAt` (`switched`), the Prospector lands at
    /// `landsAt`; `started` is the world's clock at the call.
    struct Flight {
        Unit unit;
        std::optional<Carried> clocks;
        size_t from = 0;
        Vec2 boarded{0, 0};
        size_t to = 0;
        std::string pad;
        Vec2 at{0, 0};
        Ferry::Plan plan;
        double started = 0;
        int64_t switchAt = 0;
        bool switched = false;
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
        /// Its last region change was an edge crossing (W4): inputs its
        /// client made before it heard of it still steer the unit.
        bool walkedIn = false;
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
        /// The Landing Pads as its client was last told (the World message).
        std::vector<LandingPad> padsTold;
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
        /// H2: its mercenaries (saved): each one's field (region, index),
        /// its unit there, the ore it has worked and not delivered yet, when
        /// it is back from a fall, its load (for a fall), what it delivered
        /// in all; the world day their wages were last paid for, the next
        /// mercenary's number, the ore they delivered while it was off line
        /// (told on its return).
        struct Merc {
            uint32_t id = 0;
            std::string name;
            size_t region = 0;
            int64_t field = 0;
            std::optional<int64_t> unit;
            double owed = 0;
            double downUntil = 0;
            int64_t cargo = 0;
            int64_t delivered = 0;
            /// Not saved: the world's clock it started drilling.
            std::optional<double> drilling;
            /// N4: a guard (mines as a miner until a hunt is called, then
            /// answers it); answering one now, or walking back to its field
            /// after; the region its unit is in while away (nil: `region`).
            bool guard = false;
            bool hunting = false;
            bool homeward = false;
            std::optional<size_t> in;
        };
        std::vector<Merc> mercs;
        int64_t mercDay = 0;
        uint32_t nextMerc = 1;
        int64_t mercAway = 0;
        /// X2, the away report (saved, WorldLedger.cpp): what happened while
        /// it was off line, when it left (world clock; -1: never) and the
        /// realm wallet then; the report made of them on its return, sent in
        /// its next Progress.
        std::vector<net::AwayRow> ledger;
        double leftAt = -1;
        Wallet realmAtLeave;
        std::optional<net::AwayReport> awayReport;
        /// L1, KSTR's next goal (saved, WorldGoals.cpp): the one shown now
        /// (its key; "level:N" a wait for level N; empty: none yet), the
        /// goals passed (paid, or skipped as met before they came up), and
        /// what it has done that its state no longer shows (a tank pumped, a
        /// mission taken, a supply bought, a mercenary hired, a module made,
        /// the Matriarch fought down).
        std::string goal;
        std::set<std::string> goalsPassed;
        std::set<std::string> goalsDid;
        /// C4: its Mule (saved as a unit of its own): the region it is in,
        /// its unit there (nil while it rolls out), and while it rolls out
        /// the Foundry, its door (region cells) and the world clock it is out.
        struct OwnMule {
            size_t region = 0;
            std::optional<int64_t> unit;
            int64_t foundry = 0;
            Vec2 door{0, 0};
            double readyAt = 0;
        };
        std::optional<OwnMule> mule;
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
    /// `WorldParams::stepThreads` as worked out (0: off), the pool (made at
    /// the start with up to that many threads, one a region, the caller's
    /// among them) and a step's jobs.
    size_t stepThreads_ = 0;
    bool stepAlways_ = false;
    std::shared_ptr<StepPool> stepPool_;
    std::vector<Region*> stepJobs_;
    size_t pooledSteps_ = 0;
    size_t start_ = 0;
    std::map<PilotId, PilotState> pilots_;
    /// When each pilot on line joined (a count; connection ids are random).
    std::map<PilotId, int64_t> joinOrder_;
    int64_t joins_ = 0;
    /// The pilot a dev line acts on (`devAs`, else the first to join).
    std::map<PilotId, PilotState>::iterator devPilot() {
        const auto first = devPilotId(1);
        const auto it = pilots_.find(devAs ? *devAs : first.value_or(0));
        return it != pilots_.end() ? it : pilots_.begin();
    }
    /// Kept pilots off line, by key.
    std::map<std::string, PilotState> away_;
    /// Who built each building, per region (building id → key).
    std::vector<std::map<int64_t, std::string>> builtBy_;
    /// The buildings the dev line `street` set out (C5).
    std::vector<int64_t> devStreet_;
    /// The realm wallet (C1).
    Wallet realm_;
    /// R2: the projects running and how many of each the realm started.
    std::array<std::optional<Project>, RealmProject::count> projects_{};
    std::array<int64_t, RealmProject::count> projectRuns_{};
    /// The realm's Citadel turrets' marks (D1).
    TurretMarks citadelMarks_;
    /// C1: what the last `restore` grew the Citadels over (`grownCitadels`).
    std::vector<std::string> grown_;
    /// C2: what the last `restore` taught the pilots (`learnedOnLoad`).
    std::vector<std::string> learnedOnLoad_;
    /// The bugs each defence has killed, per region (building id → kills;
    /// D3). Saved with the building, gone with it.
    std::vector<std::map<int64_t, int64_t>> kills_;
    std::vector<Moved> moves_;
    /// C4: what the server keeps of each walk-in Dropship beside the
    /// region's `Shuttle` (WorldShuttle.cpp): where it is (`region`, `id`),
    /// the pad it stands on or comes down onto, and its flight: the target
    /// region, pad and its ground point there, who pressed take-off.
    struct ShuttleTrip {
        size_t region = 0;
        int64_t id = 0;
        int64_t pad = 0;
        StructureKind padKind = StructureKind::landingPad;
        std::string padName;
        std::optional<size_t> to;
        std::string toPad;
        int64_t toBuilding = 0;
        Vec2 toAt{0, 0};
        bool switched = false;
        bool flying = false;
        /// The pilots aboard at take-off (each told of its landing).
        std::vector<PilotId> humans;
    };
    std::vector<ShuttleTrip> trips_;
    /// Each step after the regions': the ships moved into their target
    /// regions at the dash's end, the touch downs paid, the gone forgotten.
    void shuttlesStep();
    /// The ship of `t` in its region (nil: gone).
    Shuttle* shipOf(const ShuttleTrip& t);
    /// The flight `p`'s Prospector rides (nil: none in flight).
    const ShuttleTrip* riderTrip(const PilotState& p) const;
    /// The take-off of `t`'s ship by unit `unit` for Landing Pad `target`
    /// (`plan`), its trip's bookkeeping (the humans aboard told): the
    /// core's refusal, or nil. The fare is the caller's.
    std::optional<std::string> launch(ShuttleTrip& t, int64_t unit, const LandingPad& target, const Ferry::Plan& plan);
    /// Ship `t` and its riders into its target region, the frame on the
    /// target pad.
    void shuttleSwitch(ShuttleTrip& t);
    /// Region `from`'s ground point `g` on region `to`'s ground (the
    /// world's grid, `net::atlas::layout`).
    Vec2 groundIn(size_t from, Vec2 g, size_t to) const;
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
    /// How a pilot reaches each region's sites (`region::siteReach`): the
    /// load's (`WorldRegion::siteReach`), else worked out the first time a
    /// contract needs it.
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

    /// A new unit of `kind` on the start pad, facing the start region's nearest
    /// ore deposit (one with ore left first), so the first thing in view is
    /// work.
    Unit startUnit(UnitKind kind) const;
    /// The neighbour across an edge (nil: the world's border, or no region).
    std::optional<size_t> neighbour(size_t r, region::Edge e) const;
    /// Puts the pilot and its unit into region `to` at region position `at`
    /// (or the nearest open spot), driving it unless another pilot drives
    /// there (false). `lastInput`: its inputs carry on from there. `exact`
    /// (C4, a Dropship's rider): `at` is a ground point, taken as it is.
    bool arrive(PilotId pilot, PilotState& p, size_t to, const Unit& u, Vec2 at, int64_t lastInput, bool exact = false);
    /// C4 (WorldMule.cpp): the pilot's driven Mule `mule` and its rider
    /// `rider` (taken out of their region) into region `to` at region
    /// position `at`, the rider on the Mule again, the pilot driving it.
    /// `exact` (a Dropship's rider): `at` is a ground point, taken as it is.
    void arriveMule(PilotId pilot, PilotState& p, size_t to, const Unit& mule, const Unit& rider, Vec2 at, int64_t lastInput,
                    bool exact = false);
    /// `p` drives its own Mule in its region now (`pilot` its id).
    bool drivesMule(PilotId pilot, const PilotState& p) const;
    /// After a step: the Mules rolled out at their time, a destroyed one
    /// forgotten (its rider driven again).
    void mulesStep();
    /// The pilot's Mule into its Progress message.
    void muleTold(const PilotState& p, net::ProgressMessage& m) const;
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
    /// The kind `p` drives, every spawn of its own unit's (docs/kind-registry.md,
    /// "MMORPG: the pilot's kind"): its progress's, the Prospector unless saved
    /// otherwise.
    static UnitKind pilotKind(const PilotState& p) { return p.progress.kind; }
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
    /// C2: `n` blueprints of `kind` for a pilot (a site, a chip, a mission,
    /// the Matriarch, a canyon wave): copies to learn from, or, of a kind it
    /// knows, the blueprint's ore price each into its wallet, told as a
    /// blueprint news with `by` "known" and the ore (`xp` on that news).
    /// The ore paid (0: copies given).
    int64_t giveBlueprint(PilotState& p, StructureKind kind, int64_t n = 1, double xp = 0);
    /// The same for a frontier pilot, into its own wallet.
    static int64_t giveBlueprint(FrontierPilot& f, StructureKind kind, int64_t n = 1);
    /// Repairs cost materials (docs/balance.md "Repairs"): each world
    /// region's `Simulation::repairPayer`, installed every step before the
    /// regions step (so a moved map never leaves one behind). It charges the
    /// frontier pilot of region `r` whose unit mends, from its own wallet,
    /// false when it is short; any other unit not a pilot's driven one mends
    /// free, as before.
    void installRepairPayers();
    /// A frontier pilot's wallet holds the next whole unit of `b`'s mend.
    bool canMend(const FrontierPilot& f, const Structure& b) const;
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
    /// Arms every Citadel of the realm with the realm's marks (D1): its
    /// four corner turrets, which fire only with a gunner (C1,
    /// `Simulation::stepCornerGun`); a complete one gets its door, shut.
    void armCitadels();
    void regrowWells(double dt);
    /// E1: each region file's deposit below full grows back
    /// `veinRegrow` of its full ore a world day, from world time `from` to
    /// `to`, up to full; Quick regrowth's share (the Simulation's
    /// `regrowShare`) shortens it. A starfall's deposit never grows.
    void regrowVeins(double from, double to);
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
    void landingPadsChanged();
    /// The pilot's pad reach: a region pad's plate and Pad link's cells past
    /// its edge (C5); `padLink` the cells past it alone (0 without it).
    double padReach(const PilotState& p) const;
    double padLink(const PilotState& p) const;
    Ferry::Plan ferryPlan(const PilotState& p, size_t to) const;
    /// Has the pilot been to region `r` (its region, the start region, or
    /// one it visited)?
    bool seen(const PilotState& p, size_t r) const;
    /// Where `p` comes back: its Citadel or Hab Dome nearest where it fell
    /// (`from` in region `region`; a region nearer on the grid first, one
    /// another pilot drives skipped), else the start pad. Nil: nowhere free.
    std::optional<Home> home(const PilotState& p, size_t region, Vec2 from) const;
    /// A new Prospector for `p` at `h`, with `cargo` (MH if `hydrogen`); into
    /// that region, its client told.
    void respawn(PilotId pilot, PilotState& p, const Home& h, int64_t cargo, bool hydrogen);
    /// The region change `p` is told after it moved into region `to`.
    void regionChanged(PilotId pilot, PilotState& p, size_t to);
    // WorldContracts.cpp.
    /// Each step: the Foundries' and Labs' jobs done by now (P7).
    void rigs();
    /// `WorldParams::devRig` for a pilot just come.
    void devRig(PilotId pilot);
    /// `p` built a finished building of `kind` (its credit).
    bool builtFinished(const PilotState& p, StructureKind kind) const;
    // WorldLedger.cpp (X2).
    /// A row of the away ledger, to one pilot off line (its mercenaries'
    /// deliveries) or to every pilot off line (`awayNoteAll`: the world's
    /// news). Deliveries and waves add up into one row each.
    void awayNote(PilotState& p, net::AwayRow row);
    void awayNoteAll(const net::AwayRow& row);
    /// What `p`'s ledger says now: the world's clock since it left, the
    /// realm wallet's change, the rows.
    net::AwayReport awayReport(const PilotState& p) const;
    std::string encodeAway(const PilotState& p) const;
    bool decodeAway(const std::string& text, PilotState& p) const;

    /// The realm's finished Citadel (region, id), the first by region.
    std::optional<std::pair<size_t, int64_t>> realmCitadel() const;
    /// The finished realm Citadel the pilot's Prospector stands at, where it
    /// unloads (R3: one a region, every one a home).
    std::optional<std::pair<size_t, int64_t>> citadelAt(PilotId pilot, const PilotState& p);
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
    // WorldGoals.cpp.
    /// Each step: every pilot's next goal checked, paid when done, the next
    /// one chosen.
    void goals();
    void goalStep(PilotId pilot, PilotState& p);
    bool goalMet(const PilotState& p, const WorldGoals::Entry& e) const;
    std::optional<net::Goal> goalMessage(PilotId pilot, const PilotState& p) const;
    /// Something done that the goals count (`PilotState::goalsDid`).
    static void goalDid(PilotState& p, const std::string& what) { p.goalsDid.insert(what); }
    /// The world's first blueprint site holding `kind`'s blueprint (region, site).
    std::optional<std::pair<size_t, size_t>> worldSite(StructureKind kind) const;
    static std::string encodeGoals(const PilotState& p);
    static bool decodeGoals(const std::string& json, PilotState& p);
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
    // WorldMercs.cpp.
    /// Each step: the mercenaries' units steered, their drilling paced,
    /// their unloads moved from the region's bank to their pilots' wallets
    /// (taxed), a fallen one back at its depot; the wages at dawn.
    void mercsStep(double dt);
    /// World time `from` to `to` with the regions held (S2's idle, a catch-up):
    /// their work as bookkeeping, ore from their fields to the wallets, the
    /// wages at each dawn on the way.
    void mercsBook(double from, double to);
    void mercWages(double at);
    /// Adds `w` to a pilot's wallet: on line through its region's bank.
    void credit(PilotState& p, bool online, const Wallet& w);
    /// A mercenary's unit beside the realm depot nearest its field (with
    /// its load); false: no depot there.
    bool mercUnit(PilotState::Merc& m);
    /// N4: the region a mercenary's unit is in (away on a hunt: there).
    static size_t mercIn(const PilotState::Merc& m) { return m.in.value_or(m.region); }
    /// N4: a guard's walk to its place on the hunt's ring, its part in the
    /// assault, and its walk back to its field after; through the openings
    /// on foot, as any Prospector.
    void mercTrek(PilotState& p, PilotState::Merc& m);
    /// A region's realm depot nearest `at` (its centre), and the field's
    /// centre and the ore left in it.
    std::optional<Vec2> depot(size_t r, Vec2 at) const;
    Vec2 fieldAt(size_t r, int64_t field) const;
    int64_t fieldOre(size_t r, int64_t field) const;
    /// The ore `m` worked, its unload: taxed into the realm wallet.
    void mercDelivered(PilotState& p, bool online, PilotState::Merc& m, int64_t ore);
    std::string encodeMercs(const PilotState& p) const;
    void mercsTold(const PilotState& p, net::ProgressMessage& m) const;
    bool decodeMercs(const std::string& json, PilotState& p) const;
    mutable std::vector<std::optional<std::vector<std::vector<size_t>>>> fields_;
    // WorldStarfall.cpp.
    /// Each step: the schedule's falls told, the landings, the deposits'
    /// XP and emptying, the faded ones dropped, the sky's list.
    void starfall();
    /// A fall's deposit into its region (`fall.patch` set): at the impact,
    /// and from a save.
    void landFall(Fall& fall, int64_t remaining, std::optional<int64_t> id = std::nullopt);
    /// E1: a fall's blast at its impact (not from a save): every building
    /// and unit within `Starfall::blastRadius` of it loses
    /// `Starfall::blastShare` of its whole hp, by its nearest edge.
    void blast(const Fall& fall);
    /// A spot for a crater in region `r` by the dice (nil: none found);
    /// `near`: 40-58 cells from it, ahead along `heading` first.
    std::optional<Vec2> craterSpot(size_t r, std::mt19937_64& dice, std::optional<Vec2> near = std::nullopt,
                                   double heading = 0) const;
    /// A region for a fall by the dice: each counts 1, a pilot in it 8, a
    /// pilot next door 3.
    size_t fallRegion(std::mt19937_64& dice) const;
    std::vector<net::Sky::Starfall> skyFalls() const;
    /// The falls a pilot in `region` is told in its snapshots (the sky's
    /// list, cut: `WorldMap::snapshot`).
    std::vector<net::Sky::Starfall> fallsFor(size_t region) const;
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
        /// A leaving bug's last headway: where, and when (Z4: one that can
        /// get no nearer its exit goes where it stands).
        std::map<int64_t, std::pair<Vec2, double>> leaveStall;
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
    // WorldNest.cpp.
    struct NestRegion {
        /// Her unit (nil: none spawned) and her group.
        std::optional<int64_t> queen;
        /// The horde's counts (0: not drawn yet), and when the next bug
        /// hatches to keep them up.
        int64_t shardlings = 0;
        int64_t gloamwings = 0;
        double nextRefill = 0;
        /// After her death the nest grows back from empty; after the region
        /// slept it comes back full.
        bool regrow = false;
        double lastRoar = -1e9;
        /// Pilots she roared at on first sight (cleared once they go).
        std::set<PilotId> seen;
        std::map<PilotId, double> damage;
        std::optional<double> downAt, backAt;
        /// The region's bugs stay away after her death until then.
        double calmUntil = -1e9;
        /// Dev and shots (`matriarch calm`): she picks no foe herself and
        /// holds her spit; `matriarch slam` names one.
        bool held = false;
    };
    /// Each think in a region with a nest: her return, her spawn, the
    /// horde's numbers, her foe, her roars.
    void nestThink(size_t r);
    /// A hit on her: who did it (a pilot's Prospector, or its defence);
    /// her death.
    void queenHit(size_t r, const Simulation::BugHit& h);
    void queenDown(size_t r);
    void roar(size_t r, Unit& queen, std::optional<int64_t> at);
    void siegeThink();
    /// The nest's bugs alive (not her): Shardlings, Gloamwings.
    std::pair<int64_t, int64_t> hordeIn(size_t r) const;
    /// One bug hatched at the nest for its group `key` (made if gone).
    void hatch(size_t r, UnitKind kind, int64_t key, Vec2 from);
    NestRegion nest_;
    std::optional<Siege> siege_;
    /// The siege's escort group, and her group out there.
    int64_t siegeEscort_ = 0;
    std::vector<net::Sky::Roar> roars_;
    uint32_t nextRoar_ = 1;
    std::vector<BroodRegion> brood_;
    std::vector<net::Sky::Raid> raids_;
    uint32_t nextRaid_ = 1;
    int64_t nextGroup_ = 1;
    std::mt19937_64 broodDice_;
    double broodThought_ = -1e9;
    // WorldCanyon.cpp.
    /// Each step (and idle): every `WorldCanyon::think` s each zone's game.
    void canyonsStep();
    /// R2: projects that ran out end (every pilot on line told); the
    /// running ones' effects on every region (sight, turrets).
    void projectsStep();
    void canyonThink(Canyon& c, double dt);
    /// A hit on a bug of a wave: who did it, toward the wave's pay.
    void canyonHit(size_t r, const Simulation::BugHit& h);
    /// The wave `c.wave` out now, down its paths.
    bool sendWave(Canyon& c);
    /// The wave out beaten (its fighters paid; the next wave, or HELD), or
    /// broken through (WAITING: wave 1 comes again; RUNNING: FAILED).
    void waveBeaten(Canyon& c);
    void waveFailed(Canyon& c);
    /// The wave's bugs left go on as any group: they linger a while and leave.
    void releaseWave(Canyon& c);
    /// Back to WAITING and wave 1, the prize full again.
    void resetCanyon(Canyon& c);
    void refillPrize(const Canyon& c);
    /// Every turret of the realm still standing in the zone falls (FAILED's
    /// end without the Matriarch there).
    void fellZoneTurrets(const Canyon& c);
    /// A pilot (not in flight) in region `r` or next to it.
    bool pilotNear(size_t r) const;
    Canyon* canyonOf(size_t r, std::string_view zone);
    /// The zones' games as the sky carries them.
    std::vector<net::Sky::Zone> skyZones() const;
    std::vector<Canyon> canyons_;
    double canyonThought_ = -1e9;
    /// The clock the goals were last checked at (L1).
    double goalsAt_ = -1e9;
    /// Falls whose impact the bugs felt; pilots' Prospectors last seen (a
    /// new one is a respawn: when, where).
    std::set<uint32_t> fallsFelt_;
    std::map<PilotId, int64_t> broodUnits_;
    std::map<PilotId, std::pair<double, Vec2>> respawned_;
    // WorldFrontier.cpp.
    /// Each step: the roster, the arrivals, every pilot's think, its unit
    /// live or booked, its unloads, its fall, its welding, the radio.
    void frontierStep(double dt);
    /// `seconds` of the world's clock with the regions held (S2's idle):
    /// their work as bookkeeping.
    void frontierIdle(double seconds);
    void frontierThink(FrontierPilot& f, double dt);
    /// Acts on its plan: live, by its unit's orders; booked, at once on the
    /// clock.
    void frontierAct(FrontierPilot& f, double dt);
    void frontierBookWork(FrontierPilot& f, double dt);
    /// Its unit into its region (it comes live) at `f.at`, or out of it.
    void frontierUnit(FrontierPilot& f, bool live);
    /// Its unit's unloads (out of the region's bank, into its wallet, taxed,
    /// with XP), its welding done, its fall.
    void frontierEvents(FrontierPilot& f);
    /// A hit on a bug by a frontier pilot's Prospector or a defence it built:
    /// its XP, its kills (the Brood's step calls it).
    void frontierHit(size_t r, const Simulation::BugHit& h);
    /// A load of `amount` (MH if `hydrogen`) into its wallet: the realm's tax
    /// on it, its XP.
    void frontierUnload(FrontierPilot& f, int64_t amount, bool hydrogen);
    void frontierEarn(FrontierPilot& f, double xp);
    /// It comes into region `to` at ground point `at` (a walk's end, a
    /// landing, an arrival).
    void frontierEnter(FrontierPilot& f, size_t to, Vec2 at);
    /// Its skill points spent by its habit's build (each its branch, in the
    /// tree's order).
    void frontierSkills(FrontierPilot& f);
    /// Its unit (nil: none live).
    Unit* frontierUnitOf(const FrontierPilot& f);
    const Unit* frontierUnitOf(const FrontierPilot& f) const;
    /// Where it is: its unit's ground point, else `f.at`.
    Vec2 frontierAt(const FrontierPilot& f) const;
    /// The realm depot (a finished Citadel; C4: a Hab Dome takes no loads)
    /// nearest ground point `at` of region `r`: its id.
    std::optional<int64_t> frontierDepot(size_t r, Vec2 at) const;
    /// Its unit (or booked place) where it unloads at a finished realm building
    /// `building`.
    bool frontierBeside(const FrontierPilot& f, int64_t building) const;
    /// The next region on the way from `from` to `to` on foot (through the
    /// openings), and the ground point it walks out at; nil: no way.
    std::optional<std::pair<size_t, Vec2>> frontierStepToward(size_t from, size_t to, Vec2 at) const;
    /// The plans by habit.
    FrontierPlan minerPlan(const FrontierPilot& f) const;
    FrontierPlan builderPlan(const FrontierPilot& f) const;
    FrontierPlan defenderPlan(const FrontierPilot& f) const;
    FrontierPlan wandererPlan(const FrontierPilot& f) const;
    /// Shared pieces: a mining plan in its region (nil: no field with ore and
    /// a depot), a haul, a blueprint to buy or a building to put up when it
    /// has one, the building's spot near `near`.
    std::optional<FrontierPlan> minePlan(const FrontierPilot& f) const;
    std::optional<FrontierPlan> buildPlan(const FrontierPilot& f, StructureKind kind, Vec2 near, const std::string& why) const;
    std::optional<Vec2> frontierSpot(const FrontierPilot& f, StructureKind kind, Vec2 near) const;
    /// A field of region `r` raided lately (`WorldFrontier::raidMemory`) with
    /// no turret of the realm by it: its middle.
    std::optional<Vec2> raidedField(size_t r) const;
    /// The bug of the Brood a defender should fight in its region (nil:
    /// none near the realm's buildings or its post).
    std::optional<int64_t> frontierFoe(const FrontierPilot& f) const;
    /// A canyon a defender should join (a wave out, a human in its region).
    const Canyon* frontierCanyon(const FrontierPilot& f) const;
    /// KSTR radio (text): a line of `kind` ("hello", "raid", "cheer", "help",
    /// "chatter", "deed") by `f`, told to every human on line; `force` skips
    /// the pacing. The deeds are toasts.
    void frontierRadio(const FrontierPilot& f, const std::string& kind, const std::string& what = "", bool force = false);
    void frontierDeed(const FrontierPilot& f, const std::string& text);
    /// The radio's pacing, each step: a raid told, a wave beaten, chatter.
    void frontierChatter();
    /// What the clients are told: the sky's list, the roster.
    std::vector<net::Sky::Frontier> skyFrontier() const;
    void frontierTold(net::ProgressMessage& m) const;
    std::string encodeFrontier() const;
    bool decodeFrontier(const std::string& json);
    /// Its key is a frontier pilot's.
    static bool frontierKey(const std::string& key) { return key.rfind("frontier:", 0) == 0; }
    const FrontierPilot* frontierByKey(const std::string& key) const;
    std::vector<FrontierPilot> frontier_;
    std::vector<std::string> radioLog_;
    /// S4: the week's counts by key, and its start on the world clock.
    std::map<std::string, Standing> standings_;
    double weekStart_ = 0;
    /// Each step: the week ended (`clock_` past its end): told, started again.
    void standingsStep();
    std::string weekEnd();
    /// A pilot's name in the standings by its key: a frontier pilot's
    /// callsign, else `callsign`.
    std::string standingName(const std::string& key) const;
    std::string encodeStandings() const;
    bool decodeStandings(const std::string& json);
    /// N4: the hunt, each step (its gathering, its charge, its failure),
    /// its end (`won`: her fall; else `fellParty`: the party fell, or the
    /// time ran out), a frontier defender's part in it (nil: none), a guard
    /// mercenary's walk to the ring and back (WorldMercs.cpp).
    Hunt hunt_;
    void huntStep();
    /// The hunt called by the pilot with key `key` (its rules checked).
    void huntStart(const std::string& key);
    void huntEnd(bool won, bool fellParty, const std::string& top, int64_t fighters);
    std::optional<FrontierPlan> huntPlan(const FrontierPilot& f) const;
    const Hunt::Member* huntMember(const std::string& key) const;
    /// A hunt is on (gathering or in the assault).
    bool huntOn() const { return hunt_.state == Hunt::State::gathering || hunt_.state == Hunt::State::assault; }
    /// The ground point in region `next` across the opening at `out` (a
    /// point at region `r`'s edge, `frontierStepToward`'s).
    Vec2 farSide(size_t r, size_t next, Vec2 out) const;
    std::string encodeHunt() const;
    bool decodeHunt(const std::string& json);
    /// X2: the world's clock and realm wallet when the save was loaded
    /// (the start of the time away of a pilot saved on line); -1: not loaded.
    double loadedAt_ = -1;
    Wallet loadedRealm_;
    double radioAt_ = -1e9;
    double chatterAt_ = -1e9;
    std::set<uint32_t> raidsHeard_;
    std::set<std::pair<size_t, int64_t>> wavesHeard_;
    /// The clock a group of the Brood last lingered at a field (region,
    /// field index): a raided field.
    std::map<std::pair<size_t, int64_t>, double> fieldRaided_;
    std::mt19937_64 frontierDice_;
};

} // namespace ac::server
