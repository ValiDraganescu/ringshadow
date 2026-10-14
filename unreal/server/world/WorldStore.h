// The world's save (docs/mmorpg-m3.md, docs/networking.md "Saves"): one
// SQLite file per world, written by the server alone. Per region the pilots'
// buildings and units, what is left in each deposit and well, the region's
// clock; the world's clock (schema 2); per pilot its id, wallet, region, driven unit and
// progression (schema 3; its old levels before), gated by blueprints (schema 4), its contracts (schema 5), its rig (schema 7); the starfalls (schema 6); the realm wallet and what each pilot gave it (schema 9); its blueprint copies (schema 10); its Citadel missions (schema 12; 11 is the Brood's). The
// schema is versioned: each migration is a step from one version to the
// next, run in order in its own transaction when the file is opened.
//
// Engine-free: SQLite's C API (the system's libsqlite3, as core-tests). One
// store is one connection, holding the file for itself (a second server on
// the same file is refused). Not thread-safe.
#pragma once

#include "Types.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct sqlite3;

namespace ac::server {

/// A building of the realm as saved (R1: the world's, used by every
/// pilot): the core's `Structure` rebuilt from it.
struct SavedBuilding {
    int64_t id = 0;
    StructureKind kind = StructureKind::citadel;
    Vec2 at{0, 0};
    /// The pilot's id (hex) that built it: its credit, not its owner.
    std::string builtBy;
    /// Construction seconds left; nil once built.
    std::optional<double> buildLeft;
    double hp = 0;
    bool operator==(const SavedBuilding&) const = default;
};

/// A pilot's unit (the Prospector) as saved.
struct SavedUnit {
    int64_t id = 0;
    UnitKind kind = UnitKind::prospector;
    Vec2 at{0, 0};
    double heading = 0;
    double hp = 0;
    /// The load it carries, and whether it is MH.
    int64_t cargo = 0;
    bool hydrogen = false;
    std::string owner;
    bool operator==(const SavedUnit&) const = default;
};

/// An ore deposit (`GameState.patches`, by index) or an MH well
/// (`GameState.wells`): what is left.
struct SavedDeposit {
    int64_t index = 0;
    int64_t remaining = 0;
    std::optional<double> depletedAt;
    bool operator==(const SavedDeposit&) const = default;
};
struct SavedWell {
    int64_t index = 0;
    int64_t remaining = 0;
    bool operator==(const SavedWell&) const = default;
};

struct SavedRegion {
    /// The world's region id.
    std::string id;
    /// Its game clock, and the next id it gives a unit or a building.
    double time = 0;
    int64_t nextId = 1;
    std::vector<SavedBuilding> buildings;
    std::vector<SavedUnit> units;
    std::vector<SavedDeposit> deposits;
    std::vector<SavedWell> wells;
    bool operator==(const SavedRegion&) const = default;
};

/// A pilot, on line or not.
struct SavedPilot {
    /// Its key, 32 hex digits (net::pilotKeyHex).
    std::string id;
    int64_t ore = 0;
    int64_t hydrogen = 0;
    /// The region it is in (in flight: the one it flies to).
    std::string region;
    /// The unit it drove there (an id in that region's `units`).
    std::optional<int64_t> unit;
    /// Its levels and picks per kind: the core's `PilotRecord` as the
    /// core's JSON (`SessionStore::encode`, compact). Empty: none, or
    /// turned into `progress` (schema 3 writes it empty).
    std::string levels;
    /// Its progression: the core's `ProspectorProgress` as the core's JSON
    /// (schema 3). Empty: an old pilot (its `levels` turn into it on load)
    /// or none yet.
    std::string progress;
    /// Saved since buildings need blueprints (schema 4). False: an older
    /// pilot, given the blueprints of the buildings it owns on load.
    bool gated = true;
    /// Its KSTR contracts (docs/contracts.md) as JSON (schema 5). Empty:
    /// none drawn yet.
    std::string contracts{};
    /// Its rig of modules (P7), the core's `Rig` as JSON (schema 7). Empty:
    /// none yet.
    std::string rig{};
    /// What it has given the realm wallet in all (C1, schema 9): in tax,
    /// and donated.
    int64_t taxedOre = 0;
    int64_t taxedHydrogen = 0;
    int64_t donatedOre = 0;
    int64_t donatedHydrogen = 0;
    /// Saved since blueprints stack (C2, schema 10). False: an older pilot,
    /// its blueprints counted again as copies on load.
    bool stacked = true;
    /// Its Citadel missions (K2, schema 12): the board and the Journal as
    /// JSON. Empty: none drawn yet.
    std::string missions{};
    bool operator==(const SavedPilot&) const = default;
};

/// A starfall told or landed (docs/weather.md "Starfall"; schema 6): its
/// region and impact point (ground), its impact time and bearing, and its
/// deposit once landed (the patch's id, the ore it came with and has left).
struct SavedStarfall {
    int64_t id = 0;
    std::string region;
    Vec2 at{0, 0};
    double impact = 0;
    double from = 0;
    std::optional<int64_t> patch;
    int64_t ore = 0;
    int64_t remaining = 0;
    bool operator==(const SavedStarfall&) const = default;
};

struct WorldSave {
    std::vector<SavedRegion> regions;
    std::vector<SavedPilot> pilots;
    /// The world's clock, seconds (`WorldMap::clock`; schema 2).
    double clock = 0;
    /// Starfalls coming and craters with their deposits (schema 6).
    std::vector<SavedStarfall> starfalls{};
    /// The realm wallet (C1, schema 9): every pilot's tax and donations.
    int64_t realmOre = 0;
    int64_t realmHydrogen = 0;
    bool operator==(const WorldSave&) const = default;
};

class WorldStore {
public:
    /// One step of the schema: from `version - 1` to `version`.
    struct Migration {
        int64_t version = 0;
        std::string sql;
    };
    /// This build's steps; the last one's version is the schema's (2: the
    /// world's clock; 3: the pilots' progression; 4: gated by blueprints;
    /// 5: contracts; 6: starfalls; 7: the rig; 8: the realm's buildings;
    /// 9: the realm wallet; 10: blueprints stack; 12: Citadel missions).
    static const std::vector<Migration>& migrations();
    static int64_t version() { return migrations().back().version; }

    /// Opens (creating) the file at `path` for world `world`, holds it for
    /// this connection alone, and runs the steps still due, each in its own
    /// transaction. Null, with the reason in `error`, when the file is not a
    /// database, another process holds it, it was made by a newer build, a
    /// step fails (the file stays at the last good version) or it is
    /// another world's. A file that cannot be opened is never written.
    static std::unique_ptr<WorldStore> open(const std::string& path, const std::string& world, std::string* error,
                                            const std::vector<Migration>& steps = migrations());

    ~WorldStore();
    WorldStore(const WorldStore&) = delete;
    WorldStore& operator=(const WorldStore&) = delete;

    /// The schema's version in the file.
    int64_t schemaVersion();

    /// Everything saved (empty: a new file). What it read counts as
    /// written: the next `write` sends only what differs from it.
    std::optional<WorldSave> load(std::string* error);

    /// What a write changed.
    struct Written {
        /// Rows inserted or changed, and rows deleted.
        int64_t changed = 0;
        int64_t removed = 0;
    };
    /// Writes `save` in one transaction: only the rows that differ from the
    /// last write (or load), and deletes the rows gone (a region's buildings
    /// and units not in it any more). Nil, with the reason, when it could
    /// not (nothing of it is in the file then, and the next write tries the
    /// same rows again).
    std::optional<Written> write(const WorldSave& save, std::string* error);

    /// The rows of a query as text (NULL is ""), for tests.
    std::vector<std::vector<std::string>> rows(const std::string& sql, std::string* error = nullptr);
    bool exec(const std::string& sql, std::string* error);

private:
    WorldStore() = default;
    bool migrate(const std::vector<Migration>& steps, std::string* error);
    void remember(const WorldSave& save);

    sqlite3* db_ = nullptr;
    std::string path_;
    // What the file holds now, by key.
    std::map<std::string, std::tuple<double, int64_t>> regions_;
    std::map<std::pair<std::string, int64_t>, SavedBuilding> buildings_;
    std::map<std::pair<std::string, int64_t>, SavedUnit> units_;
    std::map<std::pair<std::string, int64_t>, SavedDeposit> deposits_;
    std::map<std::pair<std::string, int64_t>, SavedWell> wells_;
    std::map<std::string, SavedPilot> pilots_;
    std::optional<double> clock_;
    std::optional<std::pair<int64_t, int64_t>> realm_;
    std::vector<SavedStarfall> starfalls_;
};

} // namespace ac::server
