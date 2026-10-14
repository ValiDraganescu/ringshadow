// The world's save (world/WorldStore.h, WorldMap::save/restore/seat): the
// store's round trip, its migrations, writes of what changed only, a region
// and a pilot put back from a save, and the Join over the protocol.
// Isolated scenes; files in build/save-tests. Run from unreal/server (as
// `make test` does).

#include "test.h"

#include "HeadlessClient.h"
#include "Server.h"
#include "Leveling.h"
#include "Session.h"
#include "WorldMap.h"
#include "WorldStore.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

const LoadedWorld& firstWorld() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint for a new pilot (`--dev-blueprints`): these scenes are
/// about building, not finding blueprints (SitesTests.cpp gates them).
WorldParams blueprinted() {
    WorldParams p;
    p.devBlueprints = true;
    return p;
}

/// A fresh file in build/save-tests (the old one and its log removed).
std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

const std::string keyA = "0123456789abcdef0123456789abcdef";
const std::string keyB = "fedcba9876543210fedcba9876543210";

WorldSave sample() {
    WorldSave s;
    SavedRegion r;
    r.id = "start";
    r.time = 123.25;
    r.nextId = 40;
    r.buildings.push_back(SavedBuilding{7, StructureKind::citadel, Vec2(10.5, -3.25), keyA, std::nullopt, 1500});
    r.buildings.push_back(SavedBuilding{9, StructureKind::habDome, Vec2(14, 2), keyB, 12.5, 40.75});
    r.units.push_back(SavedUnit{3, UnitKind::prospector, Vec2(1.5, 2.5), 0.75, 45.5, 5, true, keyA});
    r.units.push_back(SavedUnit{12, UnitKind::prospector, Vec2(-4, 8), 3.0, 60, 0, false, keyB});
    r.deposits.push_back(SavedDeposit{0, 1500, std::nullopt});
    r.deposits.push_back(SavedDeposit{1, 0, 99.5});
    r.wells.push_back(SavedWell{0, 2200});
    s.regions.push_back(r);
    SavedRegion o;
    o.id = "cold-fork";
    o.time = 5;
    o.nextId = 1;
    s.regions.push_back(o);
    PilotRecord rec;
    rec.kinds[UnitKind::prospector].xp = 1234.5;
    rec.kinds[UnitKind::prospector].picks = {Perk::prospectorQuickDrill};
    s.pilots.push_back(SavedPilot{keyA, 25, 7, "start", 3, SessionStore::encode(rec, false), ""});
    ProspectorProgress g;
    g.xp = 6543.5;
    g.ranks[Skill::quickDrill] = 2;
    g.feats = {"first-flight"};
    s.pilots.push_back(SavedPilot{keyB, 400, 0, "cold-fork", std::nullopt, "", SessionStore::encode(g, false)});
    // Its rig (schema 7).
    Rig rig;
    rig.fitted[Module::drillHead] = 2;
    rig.making = ModuleJob{Module::plate, 1, 99.5};
    s.pilots.back().rig = SessionStore::encode(rig, false);
    return s;
}

/// Sorted as `load` gives them back (regions and pilots by id).
WorldSave sorted(WorldSave s) {
    std::sort(s.regions.begin(), s.regions.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(s.pilots.begin(), s.pilots.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
    return s;
}

Unit* driven(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

int64_t inputTick = 0;

void stand(WorldMap& w, PilotId p, int ticks) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.heading = driven(w, p)->heading;
        w.input(p, in);
        w.step();
    }
}

const Structure* citadelOf(const Simulation& sim) {
    for (const Structure& s : sim.state.structures)
        if (s.kind == StructureKind::citadel) return &s;
    return nullptr;
}

/// A pilot with key A in the start region: a Citadel half built on the open
/// ground east of the ore, some ore mined out of a deposit, one run dry.
void played(WorldMap& w, PilotId me) {
    ASSERT_TRUE(w.seat(me, keyA) == WorldMap::Seated::fresh);
    Unit* u = driven(w, me);
    u->position = w.regionOf(me)->places()->ground(Vec2(72, 48));
    u->heading = 3.141592653589793;
    u->task = Unit::Task::idle;
    const Vec2 ahead(std::cos(u->heading), std::sin(u->heading));
    Simulation& sim = w.regionOf(me)->simulation();
    const Vec2 at = *sim.snap(StructureKind::citadel, u->position + ahead * Region::buildReach(StructureKind::citadel));
    ASSERT_TRUE(!w.build(me, StructureKind::citadel, at));
    stand(w, me, 300);
    sim.state.patches[0].remaining -= 37;
    sim.state.patches[1].remaining = 0;
    sim.state.patches[1].depletedAt = sim.state.time - 2;
    if (sim.state.wells && !sim.state.wells->empty()) (*sim.state.wells)[0].remaining -= 11;
}

} // namespace

// MARK: - The store

TEST(save_store_round_trips_every_saved_thing) {
    const std::string path = fresh("roundtrip");
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        const auto empty = store->load(&why);
        ASSERT_TRUE(empty.has_value());
        EXPECT_TRUE(empty->regions.empty() && empty->pilots.empty());
        ASSERT_TRUE(store->write(sample(), &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == sorted(sample()));
    // The levels read back as the core's record.
    const auto rec = SessionStore::decode<PilotRecord>(back->pilots[0].levels);
    ASSERT_TRUE(rec.has_value());
    EXPECT_EQ(rec->xp(UnitKind::prospector), 1234.5);
    EXPECT_TRUE(rec->picks(UnitKind::prospector) == std::vector<Perk>{Perk::prospectorQuickDrill});
    // Another world's save is refused, untouched.
    store.reset();
    EXPECT_TRUE(WorldStore::open(path, "second", &why) == nullptr);
    EXPECT_TRUE(why.find("a save of world first, not second") != std::string::npos);
}

TEST(save_migrations_v0_to_v7_and_a_future_step) {
    const std::string path = fresh("migrate");
    std::string why;
    {
        // v0 (a new file) to v7.
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(7));
        EXPECT_EQ(WorldStore::version(), int64_t(7));
        EXPECT_EQ(store->rows("SELECT count(*) FROM sqlite_master WHERE type = 'table' AND name IN "
                              "('meta', 'regions', 'buildings', 'units', 'deposits', 'wells', 'pilots', 'world_clock', 'starfalls')")[0][0],
                  std::string("9"));
        ASSERT_TRUE(store->write(sample(), &why).has_value());
    }
    // A step that fails: the file stays at v7, its rows kept.
    std::vector<WorldStore::Migration> broken = WorldStore::migrations();
    broken.push_back({8, "ALTER TABLE no_such_table ADD COLUMN x TEXT"});
    EXPECT_TRUE(WorldStore::open(path, "first", &why, broken) == nullptr);
    EXPECT_TRUE(why.find("migration to version 8") != std::string::npos);
    // A future step: v7 to v8, the rows kept, the column there.
    std::vector<WorldStore::Migration> next = WorldStore::migrations();
    next.push_back({8, "ALTER TABLE pilots ADD COLUMN title TEXT NOT NULL DEFAULT 'prospector'"});
    {
        auto store = WorldStore::open(path, "first", &why, next);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(8));
        const auto rows = store->rows("SELECT id, title FROM pilots ORDER BY id");
        ASSERT_TRUE(rows.size() == 2);
        EXPECT_TRUE(rows[0][1] == "prospector");
        const auto back = store->load(&why);
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == sorted(sample()));
    }
    // This build (v7) refuses a v8 file.
    EXPECT_TRUE(WorldStore::open(path, "first", &why) == nullptr);
    EXPECT_TRUE(why.find("schema version 8, this server knows 7") != std::string::npos);
}

TEST(save_v1_file_gets_the_worlds_clock_from_its_regions) {
    // A save made before the world's clock (schema 1): opened by this
    // build, it migrates to 7 and the world's clock starts at the furthest
    // region's, so the hour goes on from about where it was.
    const std::string path = fresh("v1clock");
    std::string why;
    const std::vector<WorldStore::Migration> v1(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 1);
    {
        auto store = WorldStore::open(path, "first", &why, v1);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(1));
        // Its regions as that build wrote them.
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 50, 1), ('tern-ridge', 321.5, 4)", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), int64_t(7));
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    const double furthest = 321.5;
    EXPECT_NEAR(back->clock, furthest, 1e-9);
    // Written and read back.
    WorldSave later = *back;
    later.clock = furthest + 1234.5;
    auto n = store->write(later, &why);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->changed, int64_t(1));
    store.reset();
    store = WorldStore::open(path, "first", &why);
    EXPECT_NEAR(store->load(&why)->clock, furthest + 1234.5, 1e-9);
}

TEST(save_v2_level_5_prospector_loads_as_level_10_with_its_blueprints) {
    // A pilot saved before the skill tree (schema 2): a level-5 Prospector
    // (600 XP earned at a tenth) with picks, a finished Citadel and a
    // Derrick going up. Loaded, it is level 10 with 9 points to spend, the
    // picks gone, the Citadel and Derrick blueprints, the Citadel's feat
    // without its XP, and the reset told on its login; saved, it is v7.
    const std::string path = fresh("v2levels");
    std::string why;
    const std::vector<WorldStore::Migration> v2(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 2);
    PilotRecord rec;
    rec.kinds[UnitKind::prospector].xp = double(Leveling::xp(5));
    rec.kinds[UnitKind::prospector].picks = {Perk::prospectorQuickDrill, Perk::prospectorRigMaster};
    ASSERT_TRUE(rec.level(UnitKind::prospector) == 5);
    {
        auto store = WorldStore::open(path, "first", &why, v2);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(2));
        // Its rows as that build wrote them.
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 50, 9)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO buildings (region, id, kind, x, y, owner, build_left, hp) VALUES "
                                "('start', 4, 'citadel', 60, 40, '" + keyA + "', NULL, 1500), "
                                "('start', 5, 'derrick', 70, 40, '" + keyA + "', 12.5, 100)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels) VALUES ('" + keyA +
                                "', 10, 3, 'start', NULL, '" + SessionStore::encode(rec, false) + "')", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), int64_t(7));
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->pilots.size() == 1);
    EXPECT_TRUE(loaded->pilots[0].progress.empty());
    WorldMap w(firstWorld(), blueprinted());
    EXPECT_TRUE(w.restore(*loaded).empty());
    constexpr PilotId me = 9;
    ASSERT_TRUE(w.seat(me, keyA) == WorldMap::Seated::restored);
    const ProspectorProgress g = *w.progress(me);
    EXPECT_NEAR(g.xp, 6000, 1e-9);
    EXPECT_EQ(g.level(), int64_t(10));
    EXPECT_EQ(g.unspent(), int64_t(9));
    EXPECT_TRUE(g.ranks.empty());
    EXPECT_TRUE(g.respecIsFree());
    EXPECT_TRUE((g.blueprints == std::set<StructureKind>{StructureKind::citadel, StructureKind::derrick}));
    EXPECT_TRUE((g.feats == std::set<std::string>{WorldXp::built(StructureKind::citadel)}));
    // The strategy game's record is not a world's any more.
    EXPECT_TRUE(!w.levels(me) || w.levels(me)->picks(UnitKind::prospector).empty());
    const auto news = w.takeNews(me);
    ASSERT_TRUE(news.size() == 1);
    EXPECT_TRUE(news[0].kind == net::ProgressMessage::NewsKind::reset);
    EXPECT_EQ(news[0].value, int64_t(9));
    // Saved as v5: the progression written, the levels empty.
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    const auto rows = store->rows("SELECT levels, progress FROM pilots");
    ASSERT_TRUE(rows.size() == 1);
    EXPECT_TRUE(rows[0][0].empty());
    std::string bad;
    const auto back = SessionStore::decode<ProspectorProgress>(rows[0][1], &bad);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == g);
}

TEST(save_writes_only_the_rows_that_changed) {
    const std::string path = fresh("changed");
    std::string why;
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    WorldSave s = sample();
    auto n = store->write(s, &why);
    ASSERT_TRUE(n.has_value());
    // 2 regions, 2 buildings, 2 units, 2 deposits, 1 well, 2 pilots, the clock.
    EXPECT_EQ(n->changed, int64_t(12));
    n = store->write(s, &why);
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->changed, int64_t(0));
    EXPECT_EQ(n->removed, int64_t(0));
    // A unit moved, a deposit mined: those two rows.
    s.regions[0].units[0].at.x += 1;
    s.regions[0].deposits[0].remaining -= 5;
    n = store->write(s, &why);
    EXPECT_EQ(n->changed, int64_t(2));
    // A building gone: deleted.
    s.regions[0].buildings.erase(s.regions[0].buildings.begin());
    n = store->write(s, &why);
    EXPECT_EQ(n->changed, int64_t(0));
    EXPECT_EQ(n->removed, int64_t(1));
    EXPECT_EQ(store->rows("SELECT count(*) FROM buildings")[0][0], std::string("1"));
    // After a load the file's rows count as written.
    store.reset();
    store = WorldStore::open(path, "first", &why);
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    n = store->write(*back, &why);
    EXPECT_EQ(n->changed, int64_t(0));
    EXPECT_EQ(n->removed, int64_t(0));
}

TEST(save_corrupt_or_held_file_refused_untouched) {
    // Not a database: refused, the bytes as they were.
    const std::string path = fresh("corrupt");
    const std::string junk(5000, 'x');
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(junk.data(), 1, junk.size(), f);
        std::fclose(f);
    }
    std::string why;
    EXPECT_TRUE(WorldStore::open(path, "first", &why) == nullptr);
    EXPECT_TRUE(why.find("not a database") != std::string::npos);
    std::string after;
    if (FILE* f = std::fopen(path.c_str(), "rb")) {
        char b[8192];
        for (size_t k; (k = std::fread(b, 1, sizeof b, f)) > 0;) after.append(b, k);
        std::fclose(f);
    }
    EXPECT_TRUE(after == junk);
    // Held by another store: refused.
    const std::string held = fresh("held");
    auto first = WorldStore::open(held, "first", &why);
    ASSERT_TRUE(first != nullptr);
    ASSERT_TRUE(first->write(sample(), &why).has_value());
    why.clear();
    EXPECT_TRUE(WorldStore::open(held, "first", &why) == nullptr);
    EXPECT_TRUE(why.find("locked") != std::string::npos);
    std::printf("  %s\n", why.c_str());
    // The first still writes.
    EXPECT_TRUE(first->write(sample(), &why).has_value());
}

// MARK: - The world put back

TEST(save_restart_restores_the_region_exactly) {
    constexpr PilotId me = 7;
    WorldMap w(firstWorld(), blueprinted());
    played(w, me);
    const Simulation& sim = w.regionOf(me)->simulation();
    const Structure c = *citadelOf(sim);
    EXPECT_TRUE(!c.complete());
    const std::string path = fresh("region");
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap again(firstWorld());
    EXPECT_TRUE(again.restore(*loaded).empty());
    EXPECT_EQ(again.away(), size_t(1));
    const GameState& was = sim.state;
    const GameState& now = again.start().simulation().state;
    EXPECT_EQ(now.time, was.time);
    EXPECT_EQ(now.nextID, was.nextID);
    const Structure* b = citadelOf(again.start().simulation());
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->id, c.id);
    EXPECT_TRUE(b->position == c.position);
    EXPECT_TRUE(b->buildLeft == c.buildLeft);
    EXPECT_EQ(b->hp, c.hp);
    ASSERT_TRUE(now.patches.size() == was.patches.size());
    for (size_t k = 0; k < was.patches.size(); k++) {
        EXPECT_EQ(now.patches[k].remaining, was.patches[k].remaining);
        EXPECT_TRUE(now.patches[k].depletedAt == was.patches[k].depletedAt);
    }
    if (was.wells)
        for (size_t k = 0; k < was.wells->size(); k++) EXPECT_EQ((*now.wells)[k].remaining, (*was.wells)[k].remaining);
    // Its unit stands where it stood, under its id.
    ASSERT_TRUE(now.units.size() == 1);
    EXPECT_EQ(now.units[0].id, was.units[0].id);
    EXPECT_TRUE(now.units[0].position == was.units[0].position);
    // The same save again, row for row.
    EXPECT_TRUE(again.save() == w.save());
}

TEST(save_pilot_restored_by_id_where_it_left) {
    constexpr PilotId me = 7;
    WorldParams fuel = blueprinted();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(firstWorld(), fuel);
    ASSERT_TRUE(w.seat(me, keyA) == WorldMap::Seated::fresh);
    // Flies to Cold Fork, lands, steps off the pad.
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing"));
    for (int i = 0; i < 600 && w.flying(me); i++) w.step();
    ASSERT_TRUE(!w.flying(me));
    ASSERT_TRUE(*w.where(me) == *w.regionNamed("cold-fork"));
    Unit* u = driven(w, me);
    ASSERT_TRUE(u != nullptr);
    u->position = u->position + Vec2(1.5, -0.5);
    u->heading = 1.25;
    u->carrying = 4;
    Region& there = *w.regionOf(me);
    there.simulation().state.players[0].ore = 123;
    there.simulation().state.players[0].hydrogen = 17;
    ProspectorProgress g = *w.progress(me);
    g.xp += Progression::xp(10);
    w.setProgress(me, g);
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    stand(w, me, 1);
    const Unit left = *driven(w, me);
    const auto progress = w.progress(me);
    w.leave(me);
    EXPECT_EQ(w.away(), size_t(1));

    const std::string path = fresh("pilot");
    std::string why;
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    store.reset();
    store = WorldStore::open(path, "first", &why);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());

    WorldMap again(firstWorld());
    EXPECT_TRUE(again.restore(*loaded).empty());
    // Another connection, the same key.
    constexpr PilotId later = 21;
    EXPECT_TRUE(again.seat(later, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(*again.where(later) == *again.regionNamed("cold-fork"));
    const Unit* back = driven(again, later);
    ASSERT_TRUE(back != nullptr);
    EXPECT_EQ(back->id, left.id);
    EXPECT_TRUE(back->position == left.position);
    EXPECT_EQ(back->heading, left.heading);
    EXPECT_EQ(back->hp, left.hp);
    EXPECT_EQ(back->carrying, int64_t(4));
    EXPECT_EQ(again.wallet(later)->ore, int64_t(123));
    EXPECT_EQ(again.wallet(later)->hydrogen, int64_t(17));
    // Its progression as it left: the XP, the point spent, the one-offs of
    // the flight (a first flight, Cold Fork, its pad).
    ASSERT_TRUE(again.progress(later).has_value());
    EXPECT_TRUE(*again.progress(later) == *progress);
    EXPECT_EQ(again.progress(later)->rank(Skill::quickDrill), int64_t(1));
    EXPECT_TRUE(again.progress(later)->regions.count("cold-fork") == 1);
    EXPECT_TRUE(again.progress(later)->pads.count("cold-fork/landing") == 1);
    EXPECT_TRUE(again.progress(later)->feats.count(WorldXp::flightFeat) == 1);
    // The bank of its region is its wallet.
    EXPECT_EQ(again.regionOf(later)->simulation().state.players[0].ore, int64_t(123));
    // The same key twice on line: refused.
    EXPECT_TRUE(again.seat(22, keyA) == WorldMap::Seated::refused);
}

TEST(save_unknown_id_is_a_new_pilot_at_the_pad) {
    WorldMap w(firstWorld(), blueprinted());
    played(w, 7);
    w.leave(7);
    WorldMap again(firstWorld());
    again.restore(w.save());
    const size_t before = again.start().simulation().state.units.size();
    EXPECT_TRUE(again.seat(30, keyB) == WorldMap::Seated::fresh);
    EXPECT_TRUE(*again.where(30) == *again.regionNamed("start"));
    EXPECT_EQ(again.wallet(30)->ore, int64_t(600));
    EXPECT_EQ(again.start().simulation().state.units.size(), before + 1);
    const region::Place& pad = firstWorld().regions[firstWorld().start].built.pads[firstWorld().startPad];
    EXPECT_TRUE(distance(driven(again, 30)->position, pad.at) < 3);
    // The known one is still waiting, apart.
    EXPECT_EQ(again.away(), size_t(1));
    EXPECT_TRUE(again.save().pilots.size() == 2);
}

// MARK: - Over the protocol

TEST(save_join_seats_the_pilot_and_restores_it_on_return) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    std::vector<std::string> lines;
    ServerOptions o;
    o.world = firstWorld();
    o.worldParams.devBlueprints = true;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    o.log = [&](const std::string& l) { lines.push_back(l); };
    Server server(serverPort, o);
    double now = 100;
    auto run = [&](HeadlessClient& c, auto&& done, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            server.tick(now);
            c.tick(now);
            if (done()) return true;
        }
        return false;
    };
    const net::PilotKey key = *net::pilotKeyFromHex(keyA);
    Vec2 where;
    int64_t unit = -1;
    {
        HeadlessClient client(clientPort, nettest::clientConfig());
        client.connect(now);
        // Connected, but no Join: not seated, no Welcome.
        ASSERT_TRUE(run(client, [&] { return client.connected(); }, 5));
        run(client, [] { return false; }, 0.5);
        EXPECT_TRUE(!client.welcome().has_value());
        EXPECT_EQ(server.world().start().pilots().size(), size_t(0));
        client.join(key);
        ASSERT_TRUE(run(client, [&] { return client.mirror() && client.mirror()->driven() && client.world(); }, 10));
        const PilotId pilot = server.world().start().pilots().front();
        Unit* u = driven(server.world(), pilot);
        u->position = u->position + Vec2(2, 1);
        unit = u->id;
        run(client, [] { return false; }, 0.2);
        where = driven(server.world(), pilot)->position;
    }
    // The old connection times out: the pilot waits for its key.
    for (int i = 0; i < 12 * 60 && server.clients() > 0; i++) {
        now += 1.0 / 60;
        server.tick(now);
    }
    EXPECT_EQ(server.clients(), size_t(0));
    EXPECT_EQ(server.world().away(), size_t(1));
    // A new connection, the same key.
    nettest::MemoryPort clientPort2(network, net::Address::ipv4(10, 0, 0, 3, 5001));
    HeadlessClient again(clientPort2, nettest::clientConfig());
    again.connect(now);
    again.join(key);
    ASSERT_TRUE(run(again, [&] { return again.mirror() && again.mirror()->driven(); }, 10));
    EXPECT_EQ(*again.mirror()->driven(), unit);
    const PilotId pilot = server.world().start().pilots().front();
    EXPECT_TRUE(distance(driven(server.world(), pilot)->position, where) < 1e-9);
    bool fresh = false, restored = false;
    for (const std::string& l : lines) {
        if (l.find("id 01234567: new at the start pad in start, 600 ore") != std::string::npos) fresh = true;
        if (l.find("id 01234567: restored in start") != std::string::npos) restored = true;
    }
    EXPECT_TRUE(fresh);
    EXPECT_TRUE(restored);
}

TEST(save_server_writes_every_5_s_what_changed_and_on_stop) {
    const std::string path = fresh("server");
    std::string why;
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = firstWorld();
    o.worldParams.devBlueprints = true;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    o.store = store.get();
    o.saved = store->load(&why);
    Server server(serverPort, o);
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    auto run = [&](double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            server.tick(now);
            client.tick(now);
        }
    };
    client.connect(now);
    client.join(*net::pilotKeyFromHex(keyA));
    run(4.5);
    // Nothing written before the first 5 s.
    EXPECT_EQ(store->rows("SELECT count(*) FROM pilots")[0][0], std::string("0"));
    run(1);
    EXPECT_EQ(store->rows("SELECT count(*) FROM pilots")[0][0], std::string("1"));
    const int64_t deposits = std::atoll(store->rows("SELECT count(*) FROM deposits")[0][0].c_str());
    EXPECT_TRUE(deposits > 0);
    EXPECT_TRUE(server.lastSave().changed > deposits);
    // The next: the regions' clocks, and nothing of the ground that stood still.
    run(5);
    EXPECT_TRUE(server.lastSave().changed <= int64_t(server.world().regionCount()) + 2);
    EXPECT_EQ(server.lastSave().removed, int64_t(0));
    // On stop: written, the pilot kept off line.
    server.stop(now, "the server stopped");
    EXPECT_EQ(server.world().away(), size_t(1));
    EXPECT_EQ(store->rows("SELECT region FROM pilots")[0][0], std::string("start"));
}
