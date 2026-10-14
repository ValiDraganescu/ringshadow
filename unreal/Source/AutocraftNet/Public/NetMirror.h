// The client's mirror of a region (docs/networking.md, "A mirror of the
// world"): an `ac::Simulation` on the same map, never stepped, whose state
// the snapshots overwrite, so the game reads it as it reads its own
// simulation today (docs/mmorpg-m1.md, T3). Units and buildings in view, the
// players, the patches and wells, the side's fog and what it remembers.
#pragma once

#include "AutocraftNetApi.h"
#include "NetSnapshot.h"

#include "Simulation.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>

namespace ac::net {

class AUTOCRAFTNET_API Mirror {
public:
    /// Builds the region's map from its description. `side`: the player the
    /// pilot plays (the fog and the memory are its).
    explicit Mirror(const RegionDescription& region, int64_t side = Pilot::player);
    ~Mirror();
    /// A world's region's description as its server gives it (W4): `like`
    /// (another region of the same world: its fog, its step) with
    /// `regionFile`, its map built here for the name, version and seed. For
    /// a mirror made ahead of time (`Client::prepare`).
    static RegionDescription describe(const RegionDescription& like, const std::string& regionFile);

    /// The pilot's side from the mirror this one replaces (its wallet and
    /// levels, the sky, the XP), until its own first snapshot says (W4: the
    /// unit carried over a seamless edge walks on at its own pace).
    void inherit(const Mirror& from);

    /// Before its first snapshot: `unit` is the one the pilot drives (W4,
    /// carried over a seamless edge), so the game and the prediction drive
    /// it at once. The first snapshot says for itself.
    void expect(int64_t unit, double heading);

    Mirror(const Mirror&) = delete;
    Mirror& operator=(const Mirror&) = delete;

    /// The map built here is the one the server described (name, version,
    /// seed).
    bool matches() const { return matches_; }

    /// Writes what a snapshot message carried into the state.
    void apply(const SnapshotPart& part);

    const Simulation& simulation() const { return *sim_; }
    Simulation& simulation() { return *sim_; }
    const RegionDescription& region() const { return region_; }

    /// The newest snapshot applied, its header.
    int64_t snapshotId() const { return snapshotId_; }
    int64_t serverTick() const { return serverTick_; }
    int64_t lastInputTick() const { return lastInputTick_; }
    int64_t game() const { return game_; }
    std::optional<int64_t> driven() const { return driven_; }
    /// The world's sky in the newest snapshot (nil: none came; M1). The
    /// state's `heldHour` and `dayLength` follow it, so the mirror's sight
    /// at night is the server's.
    const std::optional<Sky>& sky() const { return sky_; }
    /// A world's pilot's XP in all in the newest snapshot (nil: none came).
    const std::optional<double>& xp() const { return xp_; }
    /// A snapshot brought the players (the wallet): before it, a new
    /// mirror's players are the map's start values, not the pilot's.
    bool playersKnown() const { return playersKnown_; }

    /// A unit or building in the mirror now.
    const Unit* unit(int64_t id) const;
    const Structure* structure(int64_t id) const;
    /// The enemy buildings remembered out of sight.
    const std::map<int64_t, Structure>& remembered() const { return remembered_; }

private:
    void startOver();
    void rebuild();

    RegionDescription region_;
    int64_t side_;
    bool matches_ = false;
    std::unique_ptr<Simulation> sim_;
    std::map<int64_t, Unit> units_;
    std::map<int64_t, Structure> structures_;
    std::map<int64_t, Structure> remembered_;
    int64_t snapshotId_ = 0;
    int64_t serverTick_ = 0;
    int64_t lastInputTick_ = -1;
    int64_t game_ = -1;
    std::optional<int64_t> driven_;
    std::optional<Sky> sky_;
    std::optional<double> xp_;
    bool playersKnown_ = false;
};

} // namespace ac::net
