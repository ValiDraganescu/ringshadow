// The server's world: one region running the game's simulation headless, the
// pilots in it, and what each pilot is told (docs/networking.md, "The tick",
// "Interest"). Engine-free: AutocraftCore, and the protocol's types from
// AutocraftNet (NetProtocol.h: `PilotInput`, `Snapshot` and what it holds,
// `RegionDescription`), which the server program writes to the wire.
#pragma once

#include "NetProtocol.h"

#include "Pilot.h"
#include "Simulation.h"
#include "Types.h"
#include "WindowMaps.h"

#include <cstdint>
#include <deque>
#include <map>
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

    bool join(PilotId pilot);
    /// Hands back whatever it drove.
    void leave(PilotId pilot);
    bool joined(PilotId pilot) const { return pilots_.count(pilot) > 0; }
    /// The side the pilot plays (`Pilot::player`).
    int64_t side(PilotId) const { return Pilot::player; }

    /// Take a unit of the pilot's side to drive (`Simulation::take`). False:
    /// not joined, another pilot drives, or the core refuses the unit.
    bool take(PilotId pilot, int64_t unit);
    /// Give the driven unit back to its AI.
    void handBack(PilotId pilot);
    std::optional<int64_t> driving(PilotId pilot) const;

    /// Queue an input for the server tick. Inputs at or before the last
    /// one applied, and repeats, are dropped (false). At most `maxBuffered`
    /// wait; past that the oldest are used up at once (their key presses
    /// carry over).
    bool input(PilotId pilot, const PilotInput& in);
    static constexpr size_t maxBuffered = 30;
    int64_t lastInputTick(PilotId pilot) const;

    /// One server tick: each pilot's next input, one 1/60 s step, the next
    /// game after a victory lap, and the events each pilot may know.
    void step();

    /// What the pilot is told now (call it at the snapshot rate).
    Snapshot snapshot(PilotId pilot, const InterestParams& params = {});
    /// Entities of the last snapshot that were not sent after all (the
    /// byte budget ran out): due again at the next, having waited one more.
    void deferred(PilotId pilot, const std::vector<int64_t>& ids);

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
        std::vector<GameEvent> events;
        int64_t snapshots = 0;
        std::unordered_map<int64_t, Waiting> waiting;
        std::set<int64_t> known;
    };

    MapDefinition map_;
    RegionDescription description_;
    Simulation sim_;
    int64_t tick_ = 0;
    int64_t game_ = 0;
    std::map<PilotId, Seat> pilots_;

    Vec2 focus(const Seat& seat, int64_t side) const;
    void nextInput(Seat& seat);
};

/// Whether the side `side` may know of an event (the fog: nothing hidden
/// leaves the server). Read right after the step that made it.
bool mayKnow(const Simulation& sim, int64_t side, const GameEvent& event);

/// A unit of another side as that side may show it: its plans left out.
Unit scrubbed(const Unit& u);

} // namespace ac::server
