// The world runs on (S2, docs/mmorpg.md "The world runs on"): with no one on
// line the server idles (its clock and timers exact, the regions held), a
// start catches the world up on the time the server was down (capped), and
// the run file says who serves a save. Isolated scenes on The First; the
// real processes (detached, reattached, replaced, stopped) are in
// EndToEndTests.cpp.

#include "test.h"

#include "NetPacket.h"
#include "NetRunFile.h"
#include "Server.h"
#include "WorldMap.h"
#include "WorldStore.h"
#include "rig.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 72;
const std::string key = "aabbccddeeff00112233445566778899";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// A pilot with a Foundry job (done 150 s on) and a Lab job (420 s on).
WorldParams rigged() {
    WorldParams p;
    p.devRig = true;
    p.contracts = false;
    return p;
}

std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

ServerOptions options(WorldStore* store, std::string* why, double* wall) {
    ServerOptions o;
    o.world = first();
    o.worldParams.contracts = false;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    o.store = store;
    o.saved = store->load(why);
    o.wallClock = [wall] { return *wall; };
    return o;
}

} // namespace

// Idle at a few Hz, at uneven times: the world's clock is on by exactly the
// time that passed, the regions have not stepped, and after 60 Hz ticks the
// first idle call counts from its own moment.
TEST(runon_idle_keeps_the_clock_exact_and_the_regions_still) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress);
    ServerOptions o;
    o.world = first();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    double now = 50;
    for (int i = 0; i < 30; i++) server.tick(now += 1.0 / 60);
    const double ticked = server.world().clock();
    EXPECT_NEAR(ticked, 30.0 / 60, 1e-9);
    const int64_t regionTick = server.world().start().tick();
    // The first idle call: no time yet.
    server.idle(now);
    EXPECT_NEAR(server.world().clock(), ticked, 1e-12);
    const double spans[] = {0.5, 0.13, 0.5, 0.37, 0.5, 0.5, 0.02, 0.49, 0.5, 0.5};
    double passed = 0;
    for (int round = 0; round < 100; round++)
        for (const double s : spans) {
            now += s;
            passed += s;
            server.idle(now);
        }
    EXPECT_NEAR(server.world().clock(), ticked + passed, 1e-6);
    EXPECT_EQ(server.world().start().tick(), regionTick);
    // Awake again: 60 Hz steps of the world from there.
    server.tick(now += 1.0 / 60);
    EXPECT_NEAR(server.world().clock(), ticked + passed + 1.0 / 60, 1e-6);
    EXPECT_EQ(server.world().start().tick(), regionTick + 1);
}

// A pilot off line, the server idle: its Foundry job (150 s) and Lab job
// (420 s) finish on the world's clock, told when it comes back.
TEST(runon_idle_finishes_the_jobs_of_a_pilot_off_line) {
    WorldMap w(first(), rigged());
    w.seat(me, key);
    w.step();
    const auto job = w.progress(me)->rig.making;
    ASSERT_TRUE(job.has_value());
    const double done = job->done;
    w.leave(me);
    while (w.clock() < done - 1) w.idle(0.5);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.progress(me)->rig.making.has_value());
    w.leave(me);
    w.idle(0.5);
    w.idle(0.5);
    w.idle(0.5);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(!w.progress(me)->rig.making.has_value());
    EXPECT_TRUE(w.progress(me)->rig.research.has_value());
}

// The time it was down: the clock jumps on that far, at most a day; the
// jobs due in the gap are done at once.
TEST(runon_catch_up_moves_the_clock_with_the_cap) {
    WorldMap w(first(), rigged());
    w.seat(me, key);
    w.step();
    w.leave(me);
    const double c0 = w.clock();
    EXPECT_NEAR(w.catchUp(200), 200, 1e-12);
    EXPECT_NEAR(w.clock(), c0 + 200, 1e-9);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(!w.progress(me)->rig.making.has_value());
    EXPECT_TRUE(w.progress(me)->rig.research.has_value());
    w.leave(me);
    // Two days down: one caught up.
    EXPECT_NEAR(w.catchUp(48 * 3600), 24 * 3600, 1e-12);
    EXPECT_NEAR(w.clock(), c0 + 200 + 24 * 3600, 1e-6);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(!w.progress(me)->rig.research.has_value());
    // Nothing back in time, nothing for no gap.
    EXPECT_EQ(w.catchUp(-50), 0.0);
    EXPECT_EQ(w.catchUp(0), 0.0);
}

// Over a save: each write stamped with the Mac's clock; a server started an
// hour later catches the world up an hour, two days later a day.
TEST(runon_restart_catches_up_on_the_time_down) {
    const std::string path = fresh("runon-catchup");
    std::string why;
    double wall = 1'800'000'000;
    double clockAtStop = 0;
    nettest::MemoryNet network;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        nettest::MemoryPort port(network, nettest::kServerAddress);
        Server server(port, options(store.get(), &why, &wall));
        EXPECT_EQ(server.caughtUp(), 0.0);
        double now = 10;
        for (int i = 0; i < 120; i++) server.tick(now += 1.0 / 60);
        server.stop(now, "the server stopped");
        clockAtStop = server.world().clock();
    }
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->load(&why)->savedAt, 1'800'000'000.0);
    }
    wall += 3600;
    std::vector<std::string> lines;
    {
        auto store = WorldStore::open(path, "first", &why);
        nettest::MemoryPort port(network, nettest::kServerAddress);
        ServerOptions o = options(store.get(), &why, &wall);
        o.log = [&](const std::string& l) { lines.push_back(l); };
        Server server(port, o);
        EXPECT_NEAR(server.caughtUp(), 3600, 1e-6);
        EXPECT_NEAR(server.world().clock(), clockAtStop + 3600, 1e-6);
        server.stop(10, "the server stopped");
        clockAtStop = server.world().clock();
    }
    bool said = false;
    for (const std::string& l : lines) said = said || l.find("catch-up: down 3600 s, the world moved on 3600 s (cap 86400 s)") == 0;
    EXPECT_TRUE(said);
    wall += 48 * 3600;
    {
        auto store = WorldStore::open(path, "first", &why);
        nettest::MemoryPort port(network, nettest::kServerAddress);
        Server server(port, options(store.get(), &why, &wall));
        EXPECT_NEAR(server.caughtUp(), 24 * 3600, 1e-6);
        EXPECT_NEAR(server.world().clock(), clockAtStop + 24 * 3600, 1e-6);
    }
}

// The run file: its name beside the save, its lines, what is missing.
TEST(runon_run_file_round_trips) {
    EXPECT_EQ(net::runFilePath("/a/mmo/first.sqlite"), std::string("/a/mmo/first.run"));
    EXPECT_EQ(net::runFilePath("/a/mmo/world.db"), std::string("/a/mmo/world.db.run"));
    net::RunFile f;
    f.pid = 4242;
    f.port = 51234;
    f.protocol = net::kProtocolVersion;
    f.build = "3473920-1792006482.449102540";
    f.token = "0123abcd";
    f.since = 1'792'000'000;
    f.save = "/a/mmo/first.sqlite";
    EXPECT_TRUE(net::decodeRunFile(net::encodeRunFile(f)) == f);
    EXPECT_TRUE(!net::decodeRunFile("pid 4242\nport 51234\nprotocol 16\nbuild x\n").has_value());
    EXPECT_TRUE(!net::decodeRunFile("pid 0\nport 1\nprotocol 16\nbuild x\ntoken t\n").has_value());
    EXPECT_TRUE(net::decodeRunFile("pid 9\nport 1\nprotocol 16\nbuild x\ntoken t\nlater 5\n").has_value());
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/runon.run";
    ASSERT_TRUE(net::writeRunFile(path, f));
    struct stat st {};
    ASSERT_TRUE(stat(path.c_str(), &st) == 0);
    EXPECT_EQ(int(st.st_mode & 0777), 0600);
    EXPECT_TRUE(net::readRunFile(path) == f);
    // Removed only by the pid it names.
    net::removeRunFile(path, 99);
    EXPECT_TRUE(net::readRunFile(path).has_value());
    net::removeRunFile(path, 4242);
    EXPECT_TRUE(!net::readRunFile(path).has_value());
}

// Stale: a pid that is gone, or one that is not a world server (this test
// program), or a file that does not read: removed when checked. Another
// protocol or build is not current.
TEST(runon_stale_run_files_are_removed) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/runon-stale.run";
    net::RunFile f;
    f.pid = int(getpid());
    f.port = 50000;
    f.protocol = net::kProtocolVersion;
    f.build = net::programBuild("./build/autocraft-server");
    f.token = "t";
    ASSERT_TRUE(!f.build.empty());
    ASSERT_TRUE(net::writeRunFile(path, f));
    EXPECT_TRUE(!net::serverRunning(f.pid));
    net::RunCheck c = net::checkRunFile(path);
    EXPECT_TRUE(c.state == net::RunState::stale);
    EXPECT_TRUE(!net::readRunFile(path).has_value());
    // A pid no process has.
    f.pid = 999'999;
    ASSERT_TRUE(net::writeRunFile(path, f));
    EXPECT_TRUE(net::checkRunFile(path).state == net::RunState::stale);
    if (FILE* junk = std::fopen(path.c_str(), "w")) {
        std::fputs("not a run file\n", junk);
        std::fclose(junk);
    }
    EXPECT_TRUE(net::checkRunFile(path).state == net::RunState::stale);
    EXPECT_TRUE(net::checkRunFile(path).state == net::RunState::none);
    // Current: this protocol and the program's build.
    EXPECT_TRUE(net::runFileCurrent(f, "./build/autocraft-server"));
    net::RunFile older = f;
    older.protocol = uint16_t(net::kProtocolVersion - 1);
    EXPECT_TRUE(!net::runFileCurrent(older, "./build/autocraft-server"));
    older = f;
    older.build = "1-2.3";
    EXPECT_TRUE(!net::runFileCurrent(older, "./build/autocraft-server"));
    EXPECT_TRUE(!net::runFileCurrent(f, "./build/no-such-server"));
}

// The Brood with no one on line: the bugs left behind go two minutes after
// the pilot left, as anywhere no pilot is near; hours of idle spawn none and
// pile up no raid; a catch-up of a day neither.
TEST(runon_idle_and_catch_up_keep_the_brood_quiet) {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.broodSeed = 0xb00d1e5ULL;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    w.step();
    const size_t r = *w.where(me);
    const Simulation& sim = w.region(r).simulation();
    ASSERT_TRUE(!sim.state.patches.empty());
    ASSERT_TRUE(w.spawnBroodGroup(r, UnitKind::shardling, 3, WorldMap::Poi::deposit, 0, sim.state.patches[0].position, true).has_value());
    for (int i = 0; i < 60; i++) w.step();
    EXPECT_TRUE(w.broodAlive(r) >= 3);
    w.leave(me);
    const double left = w.clock();
    // As at 60 Hz: the director's last two minutes, then the region empties.
    while (w.clock() < left + WorldBrood::idleDespawn - 5) w.idle(0.5);
    EXPECT_TRUE(w.broodAlive(r) > 0);
    while (w.clock() < left + WorldBrood::idleDespawn + 5) w.idle(0.5);
    EXPECT_EQ(w.broodAlive(r), int64_t(0));
    EXPECT_TRUE(w.broodGroups(r).empty());
    // Two hours idle: nothing anywhere, no raid told.
    for (int i = 0; i < 2 * 3600 * 2; i++) w.idle(0.5);
    int64_t alive = 0;
    for (size_t k = 0; k < w.regionCount(); k++) alive += w.broodAlive(k);
    EXPECT_EQ(alive, int64_t(0));
    EXPECT_TRUE(w.sky()->raids.empty());
    EXPECT_NEAR(w.catchUp(24 * 3600), 24 * 3600, 1e-9);
    alive = 0;
    for (size_t k = 0; k < w.regionCount(); k++) alive += w.broodAlive(k);
    EXPECT_EQ(alive, int64_t(0));
    EXPECT_TRUE(w.sky()->raids.empty());
}

// Q3, the soak's catch-up: a world with mercenaries, the frontier pilots
// landed and a pilot off line, down 30 h. The clock moves 24 h (the cap);
// the mercenaries' work is booked for those 24 h and not more; the frontier
// pilots book nothing for the gap (docs/frontier.md "Server downtime"); the
// realm week that ended in it is told once, however many passed; the away
// ledger gains a few rows, not one per tick or per week.
TEST(runon_catch_up_of_a_day_books_the_mercs_and_stays_bounded) {
    WorldParams p;
    p.devRig = true;
    p.firstStep = 0;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    w.step();
    ProspectorProgress g = *w.progress(me);
    g.xp = Progression::xp(30);
    w.setProgress(me, g);
    w.step();
    for (const bool guard : {false, false, true}) ASSERT_TRUE(!w.hireMerc(me, guard).has_value());
    auto landed = [&] {
        size_t n = 0;
        for (const auto& f : w.frontier()) n += f.arrived ? 1 : 0;
        return n;
    };
    for (int i = 0; i < 20 * 60 * 60 && landed() < w.frontier().size(); i++) w.step();
    ASSERT_TRUE(landed() == w.frontier().size() && landed() > 0);
    w.leave(me);
    w.step();
    auto delivered = [&] {
        int64_t ore = 0;
        for (const net::AwayRow& r : *w.ledgerOf(key)) ore += r.kind == net::AwayKind::merc ? r.value : 0;
        return ore;
    };
    auto weeks = [&] {
        size_t n = 0;
        for (const net::AwayRow& r : *w.ledgerOf(key)) n += r.kind == net::AwayKind::week ? 1 : 0;
        return n;
    };
    std::vector<WorldMap::Wallet> frontierBefore;
    for (const auto& f : w.frontier()) frontierBefore.push_back(f.wallet);
    const size_t rows = w.ledgerOf(key)->size();
    const int64_t ore = delivered();
    const double clock = w.clock();
    const auto t0 = std::chrono::steady_clock::now();
    EXPECT_NEAR(w.catchUp(30 * 3600), 24 * 3600, 1e-9);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_NEAR(w.clock() - clock, 24 * 3600, 1e-6);
    // Two miners and a guard (a guard mines until a hunt is called): a
    // day's rate each at most, whole loads.
    const int64_t most = 3 * (int64_t(WorldMercs::rate(30) * 24 * 3600) + WorldMercs::load);
    EXPECT_TRUE(delivered() > ore);
    EXPECT_TRUE(delivered() - ore <= most);
    for (size_t i = 0; i < w.frontier().size(); i++) EXPECT_TRUE(w.frontier()[i].wallet == frontierBefore[i]);
    // 24 h is 5 realm weeks of The First's 40-minute days: told once.
    EXPECT_TRUE(24 * 3600 > 5 * w.weekLength());
    EXPECT_EQ(weeks(), size_t(1));
    // A row a mercenary (their deliveries add up) and the week's.
    EXPECT_TRUE(w.ledgerOf(key)->size() <= rows + 3 + 1);
    EXPECT_TRUE(ms < 1000);
    std::printf("  caught up 24 h in %.1f ms: mercenaries %lld ore (at most %lld), ledger %zu -> %zu rows\n", ms,
                (long long)(delivered() - ore), (long long)most, rows, w.ledgerOf(key)->size());
}

// Q3: the server's log past its size at start moves to .1, the one before
// dropped; under it, left.
TEST(runon_log_rotates_past_its_size) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/rotate.log";
    std::remove(path.c_str());
    std::remove((path + ".1").c_str());
    auto write = [&](const std::string& file, size_t bytes) {
        FILE* f = std::fopen(file.c_str(), "wb");
        for (size_t i = 0; i < bytes; i++) std::fputc('x', f);
        std::fclose(f);
    };
    auto sizeOf = [](const std::string& file) {
        struct stat st {};
        return stat(file.c_str(), &st) == 0 ? (long long)st.st_size : -1;
    };
    EXPECT_TRUE(!rotateLog(path, 100));
    write(path, 100);
    EXPECT_TRUE(!rotateLog(path, 100));
    EXPECT_EQ(sizeOf(path), 100ll);
    write(path + ".1", 7);
    write(path, 101);
    EXPECT_TRUE(rotateLog(path, 100));
    EXPECT_EQ(sizeOf(path), -1ll);
    EXPECT_EQ(sizeOf(path + ".1"), 101ll);
}
