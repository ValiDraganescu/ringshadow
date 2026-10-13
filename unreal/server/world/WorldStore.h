// The world's save (docs/mmorpg-m3.md, docs/networking.md "Saves"): one
// SQLite file per world, written by the server alone. Per region the pilots'
// buildings and units, what is left in each deposit and well, the region's
// clock; per pilot its id, wallet, region, driven unit and levels. The
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

/// A pilot's building as saved: the core's `Structure` rebuilt from it.
struct SavedBuilding {
    int64_t id = 0;
    StructureKind kind = StructureKind::citadel;
    Vec2 at{0, 0};
    /// The pilot's id (hex) that built it.
    std::string owner;
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
    /// core's JSON (`SessionStore::encode`, compact). Empty: none yet.
    std::string levels;
    bool operator==(const SavedPilot&) const = default;
};

struct WorldSave {
    std::vector<SavedRegion> regions;
    std::vector<SavedPilot> pilots;
    bool operator==(const WorldSave&) const = default;
};

class WorldStore {
public:
    /// One step of the schema: from `version - 1` to `version`.
    struct Migration {
        int64_t version = 0;
        std::string sql;
    };
    /// This build's steps; the last one's version is the schema's (1).
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
};

} // namespace ac::server
