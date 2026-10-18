// The server's world: one region running the game's simulation headless, the
// pilots in it, and what each pilot is told (docs/networking.md, "The tick",
// "Interest"). Engine-free: AutocraftCore, and the protocol's types from
// AutocraftNet (NetProtocol.h: `PilotInput`, `Snapshot` and what it holds,
// `RegionDescription`), which the server program writes to the wire.
#pragma once

#include "NetProtocol.h"

#include "Pilot.h"
#include "RegionFile.h"
#include "Simulation.h"
#include "Types.h"
#include "WindowMaps.h"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ac::server {

using net::EntityState;
using net::FogView;
using net::PatchView;
using net::PilotInput;
using net::Rank;
using net::RegionDescription;
using net::Snapshot;
using net::WellView;

/// A pilot: one connected player. Its own number, given by the server.
using PilotId = uint64_t;

/// The strategy game's leveling in a world's regions (`Simulation::xpScale`):
/// off. A world's Prospector climbs the skill tree instead
/// (docs/prospector-progression.md), its XP counted by the server at ×1
/// (`WorldXp`, WorldMap.h); the strategy game's cards never come. The RTS
/// game and the M1 region keep the game's rates.
inline constexpr double kWorldXpScale = 0;
/// A Pick request in a world: refused with this.
inline constexpr const char* kWorldPickRefusal = "No level-up cards in a world: spend skill points (K)";
/// What a pilot's hired mercenary (H2) and a frontier pilot drive: the
/// Prospector, whatever the pilot drives (`WorldMap::pilotKind`).
inline constexpr UnitKind kMercenaryKind = UnitKind::prospector;
inline constexpr UnitKind kFrontierKind = UnitKind::prospector;

/// How a region starts.
struct RegionConfig {
    /// Today's generated map: a style, a size and its starts (2: mirrored;
    /// 4 or 8: the square team maps).
    MapChoice map{MapStyle::highlands, MapSize::small, 2};
    /// The AI armies play (every side, the pilot's included, as in the game).
    bool ai = true;
    bool fog = true;
    /// Teams for a map with more starts (`GameState::new_`); nil: one each.
    std::optional<std::vector<int64_t>> teams;
    /// Start from this state on the map (a fixture) instead of a new game.
    std::optional<GameState> state;
    /// A world's region: its region file built (`region::build`) and its
    /// text, for the clients. Set: the map is the file's, not `map`'s.
    std::optional<region::Built> built;
    std::string regionFile;
    /// Wild ground (a world's region): the map's ore and wells, no bases,
    /// no units, one player (the pilots' side), no AI. Only what pilots bring.
    bool wild = false;
    /// The game's route finding (`ac.SimFastPaths`, `ac.SimPathBudget`).
    bool fastPaths = true;
    int64_t pathBudget = 32;
};

/// Who decides what a pilot is told, and how often (networking.md,
/// "Interest"). Starting points, to measure.
struct InterestParams {
    /// Within this many cells of the pilot's focus: every snapshot.
    double nearRadius = 24;
    /// Within this: every `midEvery`th snapshot; past it, every `farEvery`th.
    double midRadius = 48;
    int64_t midEvery = 2;
    int64_t farEvery = 4;
    /// At most this many entities in one snapshot.
    int64_t maxEntities = 256;
    /// An entity left out for the budget counts this many cells nearer for
    /// every snapshot it has waited, so none waits for ever.
    double agingCells = 8;
};

/// One region: the game's simulation on one map, stepped at 60 Hz, with
/// its AI armies, and the pilots in it.
///
/// Core limits (M1): the simulation has one `pilot` and drives units of
/// `Pilot::player` only, so every pilot is on that side and one drives at a
/// time.
class Region {
public:
    explicit Region(const RegionConfig& config = {});

    const RegionDescription& description() const { return description_; }
    const MapDefinition& map() const { return map_; }
    const Simulation& simulation() const { return sim_; }
    /// For tests and the bench: set a scene up by hand.
    Simulation& simulation() { return sim_; }
    /// Ticks stepped.
    int64_t tick() const { return tick_; }
    /// Games started after a victory (0: the first).
    int64_t game() const { return game_; }
    /// A world's region: its pads, spawns and openings (nil: a generated map).
    const std::optional<region::Built>& places() const { return built_; }

    /// `lastInput`: the pilot's last input applied where it was before (a
    /// region change), so its next inputs carry on.
    bool join(PilotId pilot, int64_t lastInput = -1);
    /// F2: while the pilot is aboard the Dropship (driving nothing), its
    /// snapshots follow ground point `at` (a pad), not its last unit; nil:
    /// as before.
    void watch(PilotId pilot, std::optional<Vec2> at);
    std::vector<PilotId> pilots() const;
    /// Hands back whatever it drove. Its levels go with the seat: read
    /// `levels` first to keep them.
    void leave(PilotId pilot);
    bool joined(PilotId pilot) const { return pilots_.count(pilot) > 0; }
    /// Any pilot in the region (seated, driving or aboard).
    bool seated() const { return !pilots_.empty(); }
    /// How long its last step and the one before took, microseconds, timed
    /// while the world steps its regions on threads
    /// (`WorldParams::stepThreads`): which go to the pool, and the longest
    /// first. Nothing else reads them.
    double stepCost = 0, stepCostBefore = 0;
    /// The side the pilot plays (`Pilot::player`).
    int64_t side(PilotId) const { return Pilot::player; }

    /// Take a unit of the pilot's side to drive (`Simulation::take`). False:
    /// not joined, another pilot drives, or the core refuses the unit.
    bool take(PilotId pilot, int64_t unit);
    /// Give the driven unit back to its AI.
    void handBack(PilotId pilot);
    /// The pilot's bank here, its wallet while in the region (W5): the
    /// side's while it is the only one driving, its own driver's context
    /// while another human drives too (the core swaps each in for its own
    /// turn). Nil: not joined, or its own not known while another drives.
    std::optional<std::pair<int64_t, int64_t>> bank(PilotId pilot);
    void setBank(PilotId pilot, int64_t ore, int64_t hydrogen);

    /// The pilot's levels and picks, per kind (the core's own record, as
    /// a game's session saves it; docs/leveling.md). Each pilot has its
    /// own: the simulation holds the driver's (its side's `pilot`, where
    /// XP lands and the picks act) while it drives, its seat holds them
    /// otherwise. Nil: never driven, or not joined.
    std::optional<PilotRecord> levels(PilotId pilot) const;
    /// Sets them (a pilot coming in from another region, or from a save).
    void setLevels(PilotId pilot, const std::optional<PilotRecord>& levels);
    /// Keep `perk` for `kind` (the core's pick: one of the two on offer at
    /// the kind's lowest pending level), driving or not. The reason when
    /// refused.
    std::optional<std::string> pick(PilotId pilot, UnitKind kind, Perk perk);
    /// A world's pilot's progression and its own buildings (ids here), for
    /// its skills (`Simulation::setProgress`): in the simulation while it
    /// drives, in its seat otherwise, as its levels.
    void setProgress(PilotId pilot, const std::optional<ProspectorProgress>& progress, const std::set<int64_t>& buildings);
    std::optional<int64_t> driving(PilotId pilot) const;
    /// Some pilot drives here (the core drives one unit a region).
    bool driven() const;
    /// A world's region by day (S3): its pilots see all of it, the sight
    /// closing in through dusk to the night's (`Simulation::daylightAt`).
    /// On; the bench turns it off to measure against the sight before it.
    void setDaylight(bool on) { daylight_ = on; }

    /// The pilot's driven Prospector puts up a `kind` near `at` (snapped as
    /// `Simulation::snap` does), paid from its side's bank: the core's own
    /// hand-build (`Simulation::pilotBuild`). The reason when refused: not
    /// driving, a kind a Prospector cannot build, too far from the spot, or
    /// the core's (the price, the ground, something in the way).
    std::optional<std::string> build(PilotId pilot, StructureKind kind, Vec2 at);
    /// How far past where the game holds a building out (`buildReach`) the
    /// spot may be, in cells: the unit moves on while the request travels.
    static constexpr double buildSlack = 3.0;
    /// Where the game holds a `kind` out ahead of the Prospector: its
    /// radius and 2.2 cells, a Derrick 3 (AcPilotBuild's `Spot`).
    static double buildReach(StructureKind kind);

    /// A unit brought in from another region (a world): a new id here, its
    /// plans and its ties to this region's things left behind (`arrival`).
    int64_t admit(const Unit& u);
    /// Takes a unit out (it leaves for another region); whoever drove it
    /// stops. Nil: no such unit.
    std::optional<Unit> release(int64_t unit);

    /// Queue an input for the server tick. Inputs at or before the last
    /// one applied, and repeats, are dropped (false). At most `maxBuffered`
    /// wait; past that the oldest are used up at once (their key presses
    /// carry over).
    bool input(PilotId pilot, const PilotInput& in);
    static constexpr size_t maxBuffered = 30;
    int64_t lastInputTick(PilotId pilot) const;
    /// The input in use now (the last one applied, its presses spent).
    std::optional<PilotInput> held(PilotId pilot) const;
    /// What it has sent that is not used yet, and the input in use: an edge
    /// crossing (W4) takes them from the region it leaves to the next, so the
    /// unit walks on through it.
    struct Inputs {
        std::deque<PilotInput> pending;
        PilotInput held;
    };
    std::optional<Inputs> inputs(PilotId pilot) const;
    void carryOn(PilotId pilot, const Inputs& in);

    /// One server tick: each pilot's next input, one 1/60 s step, the next
    /// game after a victory lap, and the events each pilot may know.
    void step();

    /// What the pilot is told now (call it at the snapshot rate).
    Snapshot snapshot(PilotId pilot, const InterestParams& params = {});
    /// Entities of the last snapshot that were not sent after all (the
    /// byte budget ran out): due again at the next, having waited one more.
    void deferred(PilotId pilot, const std::vector<int64_t>& ids);

    /// Every event of the last step, whoever may know it (the world's XP
    /// reads them).
    const std::vector<GameEvent>& lastEvents() const { return lastEvents_; }

    /// Events kept for a pilot that takes no snapshots.
    static constexpr size_t maxEvents = 4096;

private:
    struct Waiting {
        int64_t sentAt = -1;
        int64_t waited = 0;
    };
    struct Seat {
        std::deque<PilotInput> pending;
        PilotInput held;
        int64_t lastApplied = -1;
        std::optional<int64_t> drives;
        /// The unit it drove last: while it drives none, its snapshots
        /// follow that one (its AI at work), not the start base.
        std::optional<int64_t> drove;
        /// What its snapshots follow while it drives nothing (`watch`).
        std::optional<Vec2> watching;
        std::vector<GameEvent> events;
        int64_t snapshots = 0;
        std::unordered_map<int64_t, Waiting> waiting;
        std::set<int64_t> known;
        /// Its levels while it does not drive (`levels`).
        std::optional<PilotRecord> levels;
        /// Its progression and own buildings (`setProgress`).
        std::optional<ProspectorProgress> progress;
        std::set<int64_t> buildings;
        /// Its bank (ore, MH) as last set or kept (`bank`).
        std::optional<std::pair<int64_t, int64_t>> bank;
    };
    /// Its bank out of the simulation before its driver's context goes.
    void keepBank(Seat& seat);

    std::optional<region::Built> built_;
    MapDefinition map_;
    RegionDescription description_;
    Simulation sim_;
    int64_t tick_ = 0;
    int64_t game_ = 0;
    std::map<PilotId, Seat> pilots_;
    bool daylight_ = true;
    std::vector<GameEvent> lastEvents_;
    /// The map's own deposits; those after them (a starfall's, told to
    /// every pilot) go to every pilot here, seen or not.
    size_t ownPatches_ = 0;
    /// Each side's fog as its pilots' snapshots carry it, and the look it
    /// is of (`Simulation::lookId`): one copy between two looks, shared.
    struct FogShare {
        uint64_t look = 0;
        std::shared_ptr<const FogView> fog;
    };
    std::map<int64_t, FogShare> fogShares_;

    Vec2 focus(const Seat& seat, int64_t side) const;
    /// The seat drives the simulation's pilot now.
    bool drivesNow(const Seat& seat) const;
    /// The side's record (`Pilot::player`'s `pilot`).
    std::optional<PilotRecord>* sideLevels();
    /// The seat's levels into the simulation (it starts to drive), and
    /// back out (it stops).
    void loadLevels(Seat& seat);
    void stashLevels(Seat& seat);
    /// The seat's progression into the simulation (it drives), and out.
    void loadProgress(const Seat& seat);
    void clearProgress(const Seat& seat);
    void nextInput(Seat& seat);
};

/// Whether the side `side` may know of an event (the fog: nothing hidden
/// leaves the server). Read right after the step that made it.
bool mayKnow(const Simulation& sim, int64_t side, const GameEvent& event);

/// A unit of another side as that side may show it: its plans left out.
Unit scrubbed(const Unit& u);

/// A unit as it arrives in another region: its plans and its ties to the
/// region it left (a patch, a building, a target) dropped, idle; its load,
/// hit points and the rest kept.
Unit arrival(const Unit& u);

} // namespace ac::server
