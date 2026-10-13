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

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
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

    /// Wall-clock ms the last `step` took over all the regions.
    double lastStepMs() const { return lastStepMs_; }

private:
    struct Flight {
        Unit unit;
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
    /// Into `away_` (WorldMapSave.cpp).
    void park(const PilotState& p);
};

} // namespace ac::server
