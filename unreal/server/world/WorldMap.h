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
    /// Dev and shots (`--dev-blueprints`): a new pilot has every blueprint.
    bool devBlueprints = false;
    /// The survey chips' dice (0: seeded by the world's id).
    uint64_t chipSeed = 0;
    /// KSTR contracts are drawn (tests that count the wallet exactly turn
    /// them off), and their dice (0: seeded by the world's id).
    bool contracts = true;
    uint64_t contractSeed = 0;
    /// Contracts to drill under a ringshadow sweep are drawn (off until the
    /// server pays the sweep, E1: `WorldMap::underSweep`).
    bool sweepContracts = false;
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

    /// Fly from the pad the driven Prospector stands at to `pad` of `region`.
    /// The reason when refused.
    std::optional<std::string> fly(PilotId pilot, std::string_view region, std::string_view pad);

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
    /// P4's finds: a blueprint (once each, 50 × its level floor; false when
    /// it was known) and a survey chip (25 each): it marks the nearest site
    /// the pilot doesn't know of, here or in a neighbour region, or one in
    /// `WorldSites::chipBlueprintOneIn` is a blueprint itself.
    bool findBlueprint(PilotId pilot, StructureKind kind, int64_t floor);
    void findChip(PilotId pilot);
    /// An ore load drilled by the pilot's Prospector (`step` calls it): it
    /// rolls for a survey chip (`ProspectorProgress::chipChance`, at most one
    /// every `WorldSites::chipEvery`). True when a chip came.
    bool oreLoad(PilotId pilot);
    /// The pilot's buildings of a kind across the world, finished or not.
    int64_t owned(PilotId pilot, StructureKind kind) const;
    /// A site's id, "<region id>/<site name>".
    std::string siteId(size_t region, size_t site) const;
    // MARK: KSTR contracts (docs/contracts.md, K1; WorldContracts.cpp)

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
        int64_t landsAt = 0;
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
    /// `p`'s own buildings in region `r` (the ids it built).
    std::set<int64_t> ownBuildings(const PilotState& p, size_t r) const;
    bool owns(const PilotState& p, size_t r, int64_t building) const;
    /// The progression into the pilot's region's simulation.
    void pushProgress(PilotId pilot);
    void earn(PilotState& p, double xp);
    /// A one-off into `set` under `id`, paid `xp`, told as `kind`.
    bool oneOff(PilotId pilot, std::set<std::string> ProspectorProgress::*set, const std::string& id, double xp,
                net::ProgressMessage::NewsKind kind);
    /// Before and after the regions step: the work XP and the feats.
    std::vector<Work> workBefore() const;
    void workAfter(const std::vector<Work>& before);
    void regrowWells(double dt);
    // WorldSites.cpp.
    int64_t owned(const PilotState& p, StructureKind kind) const;
    /// After a step: sites seen, reached, held open.
    void sites();
    void openSite(PilotId pilot, PilotState& p, size_t r, size_t i);
    /// A Scanner pulse on arriving in region `r`.
    void scan(PilotState& p, size_t r);
    /// The site nearest the pilot that it doesn't know of, here or in a
    /// neighbour region (nil: none left).
    std::optional<std::pair<size_t, size_t>> nearestUnknown(const PilotState& p) const;
    // WorldContracts.cpp.
    /// Each step: a new day's draw, the reach contracts.
    void contracts();
    /// Fills the done and abandoned slots (all three on the first day).
    void drawContracts(PilotId pilot, PilotState& p, int64_t day);
    net::Contract drawContract(PilotId pilot, const PilotState& p, std::mt19937_64& dice,
                               const std::vector<net::Contract::Kind>& taken);
    /// Work toward the pilot's open contracts of `kind` (and `target`, when
    /// set); pays the ones it finishes.
    void contractWork(PilotId pilot, PilotState& p, net::Contract::Kind kind, const std::string& target, int64_t amount);
    static std::string encodeContracts(const PilotState& p);
    static bool decodeContracts(const std::string& json, PilotState& p);
    /// A unit's skill clocks out of region `r` (it leaves), and into `r` as
    /// unit `unit` (it arrives), its cooldowns running on meanwhile.
    std::optional<Carried> takeClocks(size_t r, int64_t unit);
    void giveClocks(size_t r, int64_t unit, const std::optional<Carried>& c);
};

} // namespace ac::server
