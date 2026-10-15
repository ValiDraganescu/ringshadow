// The world's save (WorldStore.h): the schema, its migrations, the diffed
// writes.

#include "WorldStore.h"

#include <sqlite3.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <set>

namespace ac::server {

namespace {

/// A prepared statement whose values are bound in order.
class Stmt {
public:
    Stmt(sqlite3* db, const std::string& sql) : db_(db) { status_ = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr); }
    ~Stmt() { sqlite3_finalize(stmt_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    Stmt& text(const std::string& v) {
        sqlite3_bind_text(stmt_, ++at_, v.c_str(), int(v.size()), SQLITE_TRANSIENT);
        return *this;
    }
    Stmt& integer(int64_t v) {
        sqlite3_bind_int64(stmt_, ++at_, v);
        return *this;
    }
    Stmt& real(double v) {
        sqlite3_bind_double(stmt_, ++at_, v);
        return *this;
    }
    Stmt& null() {
        sqlite3_bind_null(stmt_, ++at_);
        return *this;
    }
    Stmt& real(const std::optional<double>& v) { return v ? real(*v) : null(); }
    Stmt& integer(const std::optional<int64_t>& v) { return v ? integer(*v) : null(); }

    /// Runs it to the end (no rows wanted).
    bool run(std::string* error) {
        if (status_ != SQLITE_OK) return fail(error);
        int rc = sqlite3_step(stmt_);
        while (rc == SQLITE_ROW) rc = sqlite3_step(stmt_);
        return rc == SQLITE_DONE || fail(error);
    }
    /// The next row (false at the end, or on an error: `failed`).
    bool row() {
        if (status_ != SQLITE_OK) return false;
        status_ = sqlite3_step(stmt_);
        if (status_ == SQLITE_ROW) {
            status_ = SQLITE_OK;
            return true;
        }
        if (status_ == SQLITE_DONE) status_ = SQLITE_OK;
        else failed_ = true;
        return false;
    }
    bool failed(std::string* error) {
        if (status_ != SQLITE_OK || failed_) {
            if (error) *error = sqlite3_errmsg(db_);
            return true;
        }
        return false;
    }
    std::string colText(int c) const {
        const unsigned char* t = sqlite3_column_text(stmt_, c);
        return t ? std::string(reinterpret_cast<const char*>(t), size_t(sqlite3_column_bytes(stmt_, c))) : std::string();
    }
    int64_t colInteger(int c) const { return sqlite3_column_int64(stmt_, c); }
    double colReal(int c) const { return sqlite3_column_double(stmt_, c); }
    bool isNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
    std::optional<double> optReal(int c) const { return isNull(c) ? std::nullopt : std::optional<double>(colReal(c)); }
    std::optional<int64_t> optInteger(int c) const { return isNull(c) ? std::nullopt : std::optional<int64_t>(colInteger(c)); }
    int columns() const { return sqlite3_column_count(stmt_); }

private:
    bool fail(std::string* error) {
        if (error) *error = sqlite3_errmsg(db_);
        return false;
    }
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
    int status_ = SQLITE_OK;
    bool failed_ = false;
    int at_ = 0;
};

/// Schema v1. Positions in the region's ground cells (the core's units).
const char* const kV1 = R"sql(
CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE regions (region TEXT PRIMARY KEY, time REAL NOT NULL, next_id INTEGER NOT NULL);
CREATE TABLE buildings (region TEXT NOT NULL, id INTEGER NOT NULL, kind TEXT NOT NULL, x REAL NOT NULL, y REAL NOT NULL,
  owner TEXT NOT NULL, build_left REAL, hp REAL NOT NULL, PRIMARY KEY (region, id));
CREATE TABLE units (region TEXT NOT NULL, id INTEGER NOT NULL, kind TEXT NOT NULL, x REAL NOT NULL, y REAL NOT NULL,
  heading REAL NOT NULL, hp REAL NOT NULL, cargo INTEGER NOT NULL, hydrogen INTEGER NOT NULL, owner TEXT NOT NULL,
  PRIMARY KEY (region, id));
CREATE TABLE deposits (region TEXT NOT NULL, idx INTEGER NOT NULL, remaining INTEGER NOT NULL, depleted_at REAL,
  PRIMARY KEY (region, idx));
CREATE TABLE wells (region TEXT NOT NULL, idx INTEGER NOT NULL, remaining INTEGER NOT NULL, PRIMARY KEY (region, idx));
CREATE TABLE pilots (id TEXT PRIMARY KEY, ore INTEGER NOT NULL, hydrogen INTEGER NOT NULL, region TEXT NOT NULL,
  unit INTEGER, levels TEXT NOT NULL);
)sql";

/// Schema v2: the world's clock, one row. A save from before starts it at
/// the furthest region's clock.
const char* const kV2 = R"sql(
CREATE TABLE world_clock (id INTEGER PRIMARY KEY CHECK (id = 1), time REAL NOT NULL);
INSERT INTO world_clock (id, time) SELECT 1, COALESCE(MAX(time), 0) FROM regions;
)sql";

/// Schema v3: each pilot's progression (docs/prospector-progression.md,
/// "Old MMO saves"). An old pilot's `levels` turn into it when the world
/// loads (WorldMapSave.cpp); `levels` is written empty from then on.
const char* const kV3 = R"sql(
ALTER TABLE pilots ADD COLUMN progress TEXT NOT NULL DEFAULT '';
)sql";

/// Schema v4: buildings gated by blueprints (docs/prospector-progression.md,
/// "Blueprint sites"). A pilot saved before (`gated` 0) is given, on load,
/// the blueprints of the buildings it owns, so they stay its own to build
/// again (WorldMapSave.cpp); every pilot is written gated from then on.
const char* const kV4 = R"sql(
ALTER TABLE pilots ADD COLUMN gated INTEGER NOT NULL DEFAULT 0;
)sql";

/// Schema v5: each pilot's KSTR contracts (docs/contracts.md), JSON. A pilot
/// saved before has none: the next step draws them.
const char* const kV5 = R"sql(
ALTER TABLE pilots ADD COLUMN contracts TEXT NOT NULL DEFAULT '';
)sql";

/// Schema v6: starfalls told and landed (docs/weather.md "Starfall"): the
/// crater's place and time, its deposit's patch id and ore. A save from
/// before has none.
const char* const kV6 = R"sql(
CREATE TABLE starfalls (id INTEGER PRIMARY KEY, region TEXT NOT NULL, x REAL NOT NULL, y REAL NOT NULL, impact REAL NOT NULL,
  bearing REAL NOT NULL, patch INTEGER, ore INTEGER NOT NULL, remaining INTEGER NOT NULL);
)sql";

/// Schema v7: each pilot's rig (P7, docs/prospector-progression.md "Foundry
/// and Lab"), JSON. A pilot saved before has none.
const char* const kV7 = R"sql(
ALTER TABLE pilots ADD COLUMN rig TEXT NOT NULL DEFAULT '';
)sql";

/// Schema v8: buildings are the realm's (R1, docs/prospector-progression.md
/// "Buildings in the MMO"): no pilot owns one, every pilot uses it. `owner`
/// becomes `built_by`, who put it up (its credit); an old save's buildings
/// load as the realm's, their builder kept.
const char* const kV8 = R"sql(
ALTER TABLE buildings RENAME COLUMN owner TO built_by;
)sql";

/// Schema v9: the realm wallet (C1, docs/prospector-progression.md "The
/// realm's tax"), one row, and what each pilot has given it in all, in tax
/// and donated. A save from before starts with an empty realm wallet.
const char* const kV9 = R"sql(
CREATE TABLE realm_wallet (id INTEGER PRIMARY KEY CHECK (id = 1), ore INTEGER NOT NULL, hydrogen INTEGER NOT NULL);
INSERT INTO realm_wallet (id, ore, hydrogen) VALUES (1, 0, 0);
ALTER TABLE pilots ADD COLUMN taxed_ore INTEGER NOT NULL DEFAULT 0;
ALTER TABLE pilots ADD COLUMN taxed_hydrogen INTEGER NOT NULL DEFAULT 0;
ALTER TABLE pilots ADD COLUMN donated_ore INTEGER NOT NULL DEFAULT 0;
ALTER TABLE pilots ADD COLUMN donated_hydrogen INTEGER NOT NULL DEFAULT 0;
)sql";

/// Schema v10: blueprints stack (C2, docs/prospector-progression.md "The
/// Citadel shop"): a pilot holds copies, and building uses one up. The
/// copies live in the progress JSON; a pilot saved before (`stacked` 0) is
/// given, on load, as many copies of each blueprint it had as the realm may
/// still build of that kind, at least 1 (WorldMapSave.cpp).
const char* const kV10 = R"sql(
ALTER TABLE pilots ADD COLUMN stacked INTEGER NOT NULL DEFAULT 0;
)sql";

/// Schema 12 (K2): a pilot's Citadel missions, its board and its Journal
/// as JSON (WorldMissions.cpp). Empty: none drawn yet (an older pilot gets
/// its board on its next step). 11 is the Brood's (N2).
const char* const kV12 = R"sql(
ALTER TABLE pilots ADD COLUMN missions TEXT NOT NULL DEFAULT '';
)sql";

/// Schema 14 (H2): a pilot's mercenaries as JSON (WorldMercs.cpp). Empty:
/// none ever hired. (13 was kept for D1, which came as 15.)
const char* const kV14 = R"sql(
ALTER TABLE pilots ADD COLUMN mercs TEXT NOT NULL DEFAULT '';
)sql";

/// Schema 15 (D1): the marks the realm's Citadel turrets have, on the realm
/// wallet's row. A save from before has none.
const char* const kV15 = R"sql(
ALTER TABLE realm_wallet ADD COLUMN mark_rate INTEGER NOT NULL DEFAULT 0;
ALTER TABLE realm_wallet ADD COLUMN mark_damage INTEGER NOT NULL DEFAULT 0;
ALTER TABLE realm_wallet ADD COLUMN mark_range INTEGER NOT NULL DEFAULT 0;
)sql";

/// Schema 16 (D3): a placed turret's marks and the bugs it has killed, on
/// its building's row. A save from before has none.
const char* const kV16 = R"sql(
ALTER TABLE buildings ADD COLUMN mark_rate INTEGER NOT NULL DEFAULT 0;
ALTER TABLE buildings ADD COLUMN mark_damage INTEGER NOT NULL DEFAULT 0;
ALTER TABLE buildings ADD COLUMN mark_range INTEGER NOT NULL DEFAULT 0;
ALTER TABLE buildings ADD COLUMN mark_air INTEGER NOT NULL DEFAULT 0;
ALTER TABLE buildings ADD COLUMN kills INTEGER NOT NULL DEFAULT 0;
)sql";

/// Schema 17 (N3): the Matriarch's nests while she is down: when she fell
/// and when she comes back (world clock). A save from before has her home.
const char* const kV17 = R"sql(
CREATE TABLE nests (region TEXT PRIMARY KEY, down_at REAL NOT NULL, back_at REAL NOT NULL);
)sql";

/// Schema 18 (L1): a pilot's next goal from KSTR as JSON (WorldGoals.cpp).
/// Empty: none chosen yet (an older pilot picks up at the first goal it
/// hasn't met).
const char* const kV18 = R"sql(
ALTER TABLE pilots ADD COLUMN goals TEXT NOT NULL DEFAULT '';
)sql";

/// Schema 20 (Z2; 19 is kept for F2): each canyon zone's game, its state,
/// wave and next change. A save from before has every zone waiting.
const char* const kV20 = R"sql(
CREATE TABLE canyons (region TEXT NOT NULL, zone TEXT NOT NULL, state INTEGER NOT NULL, wave INTEGER NOT NULL, next REAL NOT NULL,
                      PRIMARY KEY (region, zone));
)sql";

/// Schema 21 (P1): the frontier pilots the server runs, one row, as JSON
/// (WorldFrontier.cpp). A save from before has none: they come after the
/// opening chain, as in a new world.
const char* const kV21 = R"sql(
CREATE TABLE frontier (id INTEGER PRIMARY KEY CHECK (id = 1), state TEXT NOT NULL);
)sql";

/// Schema 22 (R2): the realm's projects, one row a kind that ever ran: the
/// count started and the one running (`ends` the world clock; 0: none). A
/// save from before has none.
const char* const kV22 = R"sql(
CREATE TABLE realm_projects (kind TEXT PRIMARY KEY, runs INTEGER NOT NULL, ends REAL NOT NULL, started_by TEXT NOT NULL);
)sql";

/// Schema 23 (X2): a pilot's away ledger (JSON, WorldLedger.cpp). A save from
/// before has none: the pilots come back with no report.
const char* const kV23 = R"sql(
ALTER TABLE pilots ADD COLUMN away TEXT NOT NULL DEFAULT '';
)sql";

/// Schema 24 (S4): the realm week's standings, one row, as JSON
/// (WorldStandings.cpp): the week's start on the world clock and each
/// pilot's counts. A save from before has none: the week starts at the load.
const char* const kV24 = R"sql(
CREATE TABLE standings (id INTEGER PRIMARY KEY CHECK (id = 1), state TEXT NOT NULL);
)sql";

using Key = std::pair<std::string, int64_t>;

} // namespace

const std::vector<WorldStore::Migration>& WorldStore::migrations() {
    // A later version adds its step here: {6, "ALTER TABLE ... ADD COLUMN ..."}.
    static const std::vector<Migration> steps = {{1, kV1}, {2, kV2}, {3, kV3}, {4, kV4}, {5, kV5}, {6, kV6}, {7, kV7}, {8, kV8}, {9, kV9}, {10, kV10}, {12, kV12}, {14, kV14}, {15, kV15}, {16, kV16}, {17, kV17}, {18, kV18}, {20, kV20}, {21, kV21}, {22, kV22}, {23, kV23}, {24, kV24}};
    return steps;
}

std::unique_ptr<WorldStore> WorldStore::open(const std::string& path, const std::string& world, std::string* error,
                                             const std::vector<Migration>& steps) {
    std::unique_ptr<WorldStore> s(new WorldStore());
    s->path_ = path;
    auto fail = [&](const std::string& why) -> std::unique_ptr<WorldStore> {
        if (error) *error = path + ": " + why;
        return nullptr;
    };
    if (sqlite3_open_v2(path.c_str(), &s->db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return fail(s->db_ ? sqlite3_errmsg(s->db_) : "out of memory");
    sqlite3_busy_timeout(s->db_, 500);
    std::string why;
    // The file is this server's alone: the lock is taken at the first read
    // below and kept until the store closes.
    if (!s->exec("PRAGMA locking_mode = EXCLUSIVE", &why)) return fail(why);
    // Reads the header first: a file that is not a database stops here,
    // untouched; one another process holds is "locked".
    const auto mode = s->rows("PRAGMA journal_mode = WAL", &why);
    if (!why.empty()) return fail(why + (why.find("locked") != std::string::npos ? " (another server has it open)" : ""));
    if (mode.empty() || mode[0].empty() || mode[0][0] != "wal") return fail("cannot use a write-ahead log");
    if (!s->exec("PRAGMA synchronous = NORMAL", &why)) return fail(why);
    if (!s->migrate(steps, &why)) return fail(why);
    // Whose world it is.
    const auto have = s->rows("SELECT value FROM meta WHERE key = 'world'", &why);
    if (!why.empty()) return fail(why);
    if (have.empty()) {
        if (!Stmt(s->db_, "INSERT INTO meta (key, value) VALUES ('world', ?)").text(world).run(&why)) return fail(why);
    } else if (have[0][0] != world) {
        return fail("a save of world " + have[0][0] + ", not " + world);
    }
    return s;
}

WorldStore::~WorldStore() { sqlite3_close(db_); }

bool WorldStore::exec(const std::string& sql, std::string* error) {
    char* message = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &message) != SQLITE_OK) {
        if (error) *error = message ? message : sqlite3_errmsg(db_);
        sqlite3_free(message);
        return false;
    }
    return true;
}

std::vector<std::vector<std::string>> WorldStore::rows(const std::string& sql, std::string* error) {
    std::vector<std::vector<std::string>> out;
    Stmt s(db_, sql);
    while (s.row()) {
        std::vector<std::string> r;
        for (int c = 0; c < s.columns(); c++) r.push_back(s.colText(c));
        out.push_back(r);
    }
    s.failed(error);
    return out;
}

int64_t WorldStore::schemaVersion() {
    const auto r = rows("SELECT version FROM schema_version");
    return r.empty() ? 0 : std::atoll(r[0][0].c_str());
}

/// The version in `schema_version` (one row; none: 0, a new file). Each
/// step after it runs in its own transaction with the new version, so a
/// step that fails leaves the file at the one before.
bool WorldStore::migrate(const std::vector<Migration>& steps, std::string* error) {
    if (!exec("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER NOT NULL)", error)) return false;
    const int64_t have = schemaVersion();
    const int64_t want = steps.empty() ? 0 : steps.back().version;
    if (have > want) {
        if (error) *error = "the save is schema version " + std::to_string(have) + ", this server knows " + std::to_string(want);
        return false;
    }
    for (const Migration& m : steps) {
        if (m.version <= have) continue;
        if (!exec("BEGIN IMMEDIATE", error)) return false;
        std::string why;
        bool ok = exec(m.sql, &why) && exec("DELETE FROM schema_version", &why) &&
                  Stmt(db_, "INSERT INTO schema_version (version) VALUES (?)").integer(m.version).run(&why);
        if (!ok) {
            exec("ROLLBACK", nullptr);
            if (error) *error = "migration to version " + std::to_string(m.version) + ": " + why;
            return false;
        }
        if (!exec("COMMIT", error)) return false;
    }
    return true;
}

std::optional<WorldSave> WorldStore::load(std::string* error) {
    WorldSave out;
    std::map<std::string, size_t> at;
    auto region = [&](const std::string& id) -> SavedRegion& {
        const auto it = at.find(id);
        if (it != at.end()) return out.regions[it->second];
        at[id] = out.regions.size();
        SavedRegion r;
        r.id = id;
        out.regions.push_back(r);
        return out.regions.back();
    };
    {
        Stmt s(db_, "SELECT region, time, next_id FROM regions ORDER BY region");
        while (s.row()) {
            SavedRegion& r = region(s.colText(0));
            r.time = s.colReal(1);
            r.nextId = s.colInteger(2);
        }
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, id, kind, x, y, built_by, build_left, hp, mark_rate, mark_damage, mark_range, mark_air, kills "
                    "FROM buildings ORDER BY region, id");
        while (s.row()) {
            const auto kind = parse<StructureKind>(s.colText(2));
            if (!kind) {
                if (error) *error = "a building of an unknown kind: " + s.colText(2);
                return std::nullopt;
            }
            region(s.colText(0)).buildings.push_back(
                SavedBuilding{s.colInteger(1), *kind, Vec2(s.colReal(3), s.colReal(4)), s.colText(5), s.optReal(6), s.colReal(7),
                              TurretMarks{s.colInteger(8), s.colInteger(9), s.colInteger(10), s.colInteger(11)}, s.colInteger(12)});
        }
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, id, kind, x, y, heading, hp, cargo, hydrogen, owner FROM units ORDER BY region, id");
        while (s.row()) {
            const auto kind = parse<UnitKind>(s.colText(2));
            if (!kind) {
                if (error) *error = "a unit of an unknown kind: " + s.colText(2);
                return std::nullopt;
            }
            region(s.colText(0)).units.push_back(SavedUnit{s.colInteger(1), *kind, Vec2(s.colReal(3), s.colReal(4)), s.colReal(5), s.colReal(6),
                                                        s.colInteger(7), s.colInteger(8) != 0, s.colText(9)});
        }
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, idx, remaining, depleted_at FROM deposits ORDER BY region, idx");
        while (s.row()) region(s.colText(0)).deposits.push_back(SavedDeposit{s.colInteger(1), s.colInteger(2), s.optReal(3)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, idx, remaining FROM wells ORDER BY region, idx");
        while (s.row()) region(s.colText(0)).wells.push_back(SavedWell{s.colInteger(1), s.colInteger(2)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT id, ore, hydrogen, region, unit, levels, progress, gated, contracts, rig, taxed_ore, taxed_hydrogen, "
                    "donated_ore, donated_hydrogen, stacked, missions, mercs, goals, away FROM pilots ORDER BY id");
        while (s.row())
            out.pilots.push_back(SavedPilot{s.colText(0), s.colInteger(1), s.colInteger(2), s.colText(3), s.optInteger(4),
                                            s.colText(5), s.colText(6), s.colInteger(7) != 0, s.colText(8), s.colText(9),
                                            s.colInteger(10), s.colInteger(11), s.colInteger(12), s.colInteger(13),
                                            s.colInteger(14) != 0, s.colText(15), s.colText(16), s.colText(17), s.colText(18)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT time FROM world_clock WHERE id = 1");
        if (s.row()) out.clock = s.colReal(0);
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT value FROM meta WHERE key = 'saved_at'");
        if (s.row()) out.savedAt = std::strtod(s.colText(0).c_str(), nullptr);
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT ore, hydrogen, mark_rate, mark_damage, mark_range FROM realm_wallet WHERE id = 1");
        if (s.row()) {
            out.realmOre = s.colInteger(0);
            out.realmHydrogen = s.colInteger(1);
            out.citadelMarks = TurretMarks{s.colInteger(2), s.colInteger(3), s.colInteger(4)};
        }
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT id, region, x, y, impact, bearing, patch, ore, remaining FROM starfalls ORDER BY id");
        while (s.row())
            out.starfalls.push_back(SavedStarfall{s.colInteger(0), s.colText(1), Vec2(s.colReal(2), s.colReal(3)), s.colReal(4),
                                                  s.colReal(5), s.optInteger(6), s.colInteger(7), s.colInteger(8)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, down_at, back_at FROM nests ORDER BY region");
        while (s.row()) out.nests.push_back(SavedNest{s.colText(0), s.colReal(1), s.colReal(2)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT region, zone, state, wave, next FROM canyons ORDER BY region, zone");
        while (s.row()) out.canyons.push_back(SavedCanyon{s.colText(0), s.colText(1), s.colInteger(2), s.colInteger(3), s.colReal(4)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT state FROM frontier WHERE id = 1");
        if (s.row()) out.frontier = s.colText(0);
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT kind, runs, ends, started_by FROM realm_projects ORDER BY kind");
        while (s.row()) out.projects.push_back(SavedProject{s.colText(0), s.colInteger(1), s.colReal(2), s.colText(3)});
        if (s.failed(error)) return std::nullopt;
    }
    {
        Stmt s(db_, "SELECT state FROM standings WHERE id = 1");
        if (s.row()) out.standings = s.colText(0);
        if (s.failed(error)) return std::nullopt;
    }
    regions_.clear();
    clock_ = std::nullopt;
    realm_ = std::nullopt;
    marks_ = std::nullopt;
    buildings_.clear();
    units_.clear();
    deposits_.clear();
    wells_.clear();
    pilots_.clear();
    remember(out);
    return out;
}

void WorldStore::remember(const WorldSave& save) {
    clock_ = save.clock;
    savedAt_ = save.savedAt;
    realm_ = {save.realmOre, save.realmHydrogen};
    marks_ = save.citadelMarks;
    starfalls_ = save.starfalls;
    nests_ = save.nests;
    canyons_ = save.canyons;
    frontier_ = save.frontier;
    projects_ = save.projects;
    standings_ = save.standings;
    for (const SavedRegion& r : save.regions) {
        regions_[r.id] = {r.time, r.nextId};
        // A region written whole: its rows are these and no others.
        auto drop = [&](auto& m) {
            for (auto it = m.lower_bound(Key{r.id, INT64_MIN}); it != m.end() && it->first.first == r.id;) it = m.erase(it);
        };
        drop(buildings_);
        drop(units_);
        drop(deposits_);
        drop(wells_);
        for (const SavedBuilding& b : r.buildings) buildings_[{r.id, b.id}] = b;
        for (const SavedUnit& u : r.units) units_[{r.id, u.id}] = u;
        for (const SavedDeposit& d : r.deposits) deposits_[{r.id, d.index}] = d;
        for (const SavedWell& w : r.wells) wells_[{r.id, w.index}] = w;
    }
    for (const SavedPilot& p : save.pilots) pilots_[p.id] = p;
}

std::optional<WorldStore::Written> WorldStore::write(const WorldSave& save, std::string* error) {
    Written n;
    std::string why;
    if (!exec("BEGIN IMMEDIATE", &why)) {
        if (error) *error = why;
        return std::nullopt;
    }
    bool ok = true;
    auto run = [&](Stmt&& s) { ok = ok && s.run(&why); };
    // Rows of a region that are there now and were not before, or differ.
    auto rowsOf = [&](const std::string& region, const auto& now, const auto& before, auto keyOf, auto put, const char* table,
                      const char* key) {
        std::set<int64_t> seen;
        for (const auto& v : now) {
            const int64_t k = keyOf(v);
            seen.insert(k);
            const auto it = before.find(Key{region, k});
            if (it != before.end() && it->second == v) continue;
            put(v);
            n.changed += 1;
        }
        for (auto it = before.lower_bound(Key{region, INT64_MIN}); it != before.end() && it->first.first == region; ++it) {
            if (seen.count(it->first.second)) continue;
            run(std::move(Stmt(db_, std::string("DELETE FROM ") + table + " WHERE region = ? AND " + key + " = ?")
                              .text(region)
                              .integer(it->first.second)));
            n.removed += 1;
        }
    };
    for (const SavedRegion& r : save.regions) {
        const auto before = regions_.find(r.id);
        if (before == regions_.end() || before->second != std::tuple<double, int64_t>{r.time, r.nextId}) {
            run(std::move(Stmt(db_, "INSERT INTO regions (region, time, next_id) VALUES (?, ?, ?) ON CONFLICT(region) DO UPDATE "
                                    "SET time = excluded.time, next_id = excluded.next_id")
                              .text(r.id)
                              .real(r.time)
                              .integer(r.nextId)));
            n.changed += 1;
        }
        rowsOf(r.id, r.buildings, buildings_, [](const SavedBuilding& b) { return b.id; },
               [&](const SavedBuilding& b) {
                   run(std::move(Stmt(db_, "INSERT OR REPLACE INTO buildings (region, id, kind, x, y, built_by, build_left, hp, "
                                           "mark_rate, mark_damage, mark_range, mark_air, kills) "
                                           "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                                     .text(r.id)
                                     .integer(b.id)
                                     .text(std::string(rawValue(b.kind)))
                                     .real(b.at.x)
                                     .real(b.at.y)
                                     .text(b.builtBy)
                                     .real(b.buildLeft)
                                     .real(b.hp)
                                     .integer(b.marks.rate)
                                     .integer(b.marks.damage)
                                     .integer(b.marks.range)
                                     .integer(b.marks.air)
                                     .integer(b.kills)));
               },
               "buildings", "id");
        rowsOf(r.id, r.units, units_, [](const SavedUnit& u) { return u.id; },
               [&](const SavedUnit& u) {
                   run(std::move(Stmt(db_, "INSERT OR REPLACE INTO units (region, id, kind, x, y, heading, hp, cargo, hydrogen, "
                                           "owner) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                                     .text(r.id)
                                     .integer(u.id)
                                     .text(std::string(rawValue(u.kind)))
                                     .real(u.at.x)
                                     .real(u.at.y)
                                     .real(u.heading)
                                     .real(u.hp)
                                     .integer(u.cargo)
                                     .integer(int64_t(u.hydrogen))
                                     .text(u.owner)));
               },
               "units", "id");
        rowsOf(r.id, r.deposits, deposits_, [](const SavedDeposit& d) { return d.index; },
               [&](const SavedDeposit& d) {
                   run(std::move(Stmt(db_, "INSERT OR REPLACE INTO deposits (region, idx, remaining, depleted_at) VALUES (?, ?, ?, ?)")
                                     .text(r.id)
                                     .integer(d.index)
                                     .integer(d.remaining)
                                     .real(d.depletedAt)));
               },
               "deposits", "idx");
        rowsOf(r.id, r.wells, wells_, [](const SavedWell& w) { return w.index; },
               [&](const SavedWell& w) {
                   run(std::move(Stmt(db_, "INSERT OR REPLACE INTO wells (region, idx, remaining) VALUES (?, ?, ?)")
                                     .text(r.id)
                                     .integer(w.index)
                                     .integer(w.remaining)));
               },
               "wells", "idx");
    }
    // The starfalls: a handful, written whole when any changed.
    if (starfalls_ != save.starfalls) {
        run(Stmt(db_, "DELETE FROM starfalls"));
        for (const SavedStarfall& f : save.starfalls)
            run(std::move(Stmt(db_, "INSERT INTO starfalls (id, region, x, y, impact, bearing, patch, ore, remaining) "
                                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")
                              .integer(f.id)
                              .text(f.region)
                              .real(f.at.x)
                              .real(f.at.y)
                              .real(f.impact)
                              .real(f.from)
                              .integer(f.patch)
                              .integer(f.ore)
                              .integer(f.remaining)));
        n.changed += 1;
    }
    // The nests: one row a fallen Matriarch, written whole when any changed.
    if (nests_ != save.nests) {
        run(Stmt(db_, "DELETE FROM nests"));
        for (const SavedNest& x : save.nests)
            run(std::move(Stmt(db_, "INSERT INTO nests (region, down_at, back_at) VALUES (?, ?, ?)")
                              .text(x.region)
                              .real(x.downAt)
                              .real(x.backAt)));
        n.changed += 1;
    }
    // The canyons: one row a zone, written whole when any changed.
    if (canyons_ != save.canyons) {
        run(Stmt(db_, "DELETE FROM canyons"));
        for (const SavedCanyon& x : save.canyons)
            run(std::move(Stmt(db_, "INSERT INTO canyons (region, zone, state, wave, next) VALUES (?, ?, ?, ?, ?)")
                              .text(x.region)
                              .text(x.zone)
                              .integer(x.state)
                              .integer(x.wave)
                              .real(x.next)));
        n.changed += 1;
    }
    // The frontier pilots (P1): one row, written whole when it changed.
    if (frontier_ != save.frontier) {
        run(std::move(Stmt(db_, "INSERT INTO frontier (id, state) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET state = excluded.state")
                          .text(save.frontier)));
        n.changed += 1;
    }
    // The realm's projects (R2): every row written when any changed.
    if (projects_ != save.projects) {
        run(Stmt(db_, "DELETE FROM realm_projects"));
        for (const SavedProject& x : save.projects)
            run(std::move(Stmt(db_, "INSERT INTO realm_projects (kind, runs, ends, started_by) VALUES (?, ?, ?, ?)")
                              .text(x.kind)
                              .integer(x.runs)
                              .real(x.ends)
                              .text(x.by)));
        n.changed += 1;
    }
    // The week's standings (S4): one row, written whole when it changed.
    if (standings_ != save.standings) {
        run(std::move(Stmt(db_, "INSERT INTO standings (id, state) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET state = excluded.state")
                          .text(save.standings)));
        n.changed += 1;
    }
    if (clock_ != save.clock) {
        run(std::move(Stmt(db_, "INSERT INTO world_clock (id, time) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET time = excluded.time")
                          .real(save.clock)));
        n.changed += 1;
    }
    if (savedAt_ != save.savedAt) {
        char at[40];
        std::snprintf(at, sizeof at, "%.6f", save.savedAt);
        run(std::move(Stmt(db_, "INSERT INTO meta (key, value) VALUES ('saved_at', ?) ON CONFLICT(key) DO UPDATE SET value = excluded.value")
                          .text(at)));
        n.changed += 1;
    }
    if (realm_ != std::pair{save.realmOre, save.realmHydrogen}) {
        run(std::move(Stmt(db_, "INSERT INTO realm_wallet (id, ore, hydrogen) VALUES (1, ?, ?) ON CONFLICT(id) DO UPDATE SET "
                                "ore = excluded.ore, hydrogen = excluded.hydrogen")
                          .integer(save.realmOre)
                          .integer(save.realmHydrogen)));
        n.changed += 1;
    }
    // A new file's row has none (the columns' default).
    if (marks_.value_or(TurretMarks{}) != save.citadelMarks) {
        run(std::move(Stmt(db_, "UPDATE realm_wallet SET mark_rate = ?, mark_damage = ?, mark_range = ? WHERE id = 1")
                          .integer(save.citadelMarks.rate)
                          .integer(save.citadelMarks.damage)
                          .integer(save.citadelMarks.range)));
        n.changed += 1;
    }
    for (const SavedPilot& p : save.pilots) {
        const auto before = pilots_.find(p.id);
        if (before != pilots_.end() && before->second == p) continue;
        run(std::move(Stmt(db_, "INSERT OR REPLACE INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, contracts, rig, "
                                "taxed_ore, taxed_hydrogen, donated_ore, donated_hydrogen, stacked, missions, mercs, goals, away) "
                                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)")
                          .text(p.id)
                          .integer(p.ore)
                          .integer(p.hydrogen)
                          .text(p.region)
                          .integer(p.unit)
                          .text(p.levels)
                          .text(p.progress)
                          .integer(p.gated ? 1 : 0)
                          .text(p.contracts)
                          .text(p.rig)
                          .integer(p.taxedOre)
                          .integer(p.taxedHydrogen)
                          .integer(p.donatedOre)
                          .integer(p.donatedHydrogen)
                          .integer(p.stacked ? 1 : 0)
                          .text(p.missions)
                          .text(p.mercs)
                          .text(p.goals)
                          .text(p.away)));
        n.changed += 1;
    }
    if (!ok || !exec("COMMIT", &why)) {
        exec("ROLLBACK", nullptr);
        if (error) *error = why;
        return std::nullopt;
    }
    remember(save);
    return n;
}

} // namespace ac::server
