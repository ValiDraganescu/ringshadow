// The away report (X2, docs/mmorpg-play.md "Coming back"): a pilot off line
// keeps a ledger of what the world did; it is saved with the world (schema
// 23), made into the report on the pilot's return, sent once in its
// Progress and cleared; the summary file beside the save holds the same
// two lines the ledger makes. Each source's row: the tests of that source
// (Brood, Canyon, Matriarch, RealmProjects, Frontier, Mercs).

#include "test.h"

#include "AwayReport.h"
#include "HeadlessClient.h"
#include "Server.h"
#include "WorldMap.h"
#include "WorldStore.h"
#include "rig.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 41;
const std::string keyA = "0123456789abcdef0123456789abcdef";
constexpr PilotId b = 42;
const std::string keyB = "fedcba9876543210fedcba9876543210";
const std::string fork = "the-fork";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Only what the tests make happen.
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.missions = false;
    p.brood = false;
    p.frontier = 0;
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

size_t coldFork(const WorldMap& w) { return *w.regionNamed("cold-fork"); }

/// The Fork held: ten waves out, the last beaten (two rows to every pilot
/// off line, "waves" and "held").
void hold(WorldMap& w) {
    ASSERT_TRUE(w.canyonSend(coldFork(w), fork, 10));
    ASSERT_TRUE(w.canyonBeat(coldFork(w), fork));
}

} // namespace

// Saved with the world and loaded as it was; on the return, the report is
// made once and the ledger starts again empty.
TEST(away_ledger_saved_loaded_and_cleared_on_show) {
    const std::string path = fresh("away");
    std::string why;
    std::vector<net::AwayRow> kept;
    {
        WorldMap w(first(), params());
        ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
        ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
        w.step();
        w.leave(b);
        ASSERT_TRUE(w.ledgerOf(keyB) != nullptr && w.ledgerOf(keyB)->empty());
        // On line: nothing kept for it.
        EXPECT_TRUE(w.ledgerOf(keyA) == nullptr);
        for (int i = 0; i < 60; i++) w.step();
        hold(w);
        kept = *w.ledgerOf(keyB);
        ASSERT_TRUE(kept.size() == size_t(2));
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(23));
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        // The pilot on line: an empty column; the one off line: its ledger.
        EXPECT_TRUE(store->rows("SELECT away FROM pilots WHERE id = '" + keyA + "'")[0][0].empty());
        EXPECT_TRUE(!store->rows("SELECT away FROM pilots WHERE id = '" + keyB + "'")[0][0].empty());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.ledgerOf(keyB) != nullptr);
    EXPECT_TRUE(*w.ledgerOf(keyB) == kept);
    // Saved again unchanged: the same rows.
    EXPECT_TRUE(store->write(w.save(), &why)->changed == int64_t(0));
    // Down an hour: caught up.
    w.catchUp(3600);
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::restored);
    const auto report = w.takeAway(b);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report->rows == kept);
    EXPECT_TRUE(report->ran >= 3600 + 1);
    // Shown once.
    EXPECT_TRUE(!w.takeAway(b).has_value());
    // Back on line, nothing kept; away again with nothing happening: no report.
    EXPECT_TRUE(w.ledgerOf(keyB) == nullptr);
    w.leave(b);
    ASSERT_TRUE(w.ledgerOf(keyB) != nullptr && w.ledgerOf(keyB)->empty());
    for (int i = 0; i < 60; i++) w.step();
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::restored);
    EXPECT_TRUE(!w.takeAway(b).has_value());
    // The one saved on line came back too: away since the load, nothing told.
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(!w.takeAway(a).has_value());
}

// Mercenary deliveries add up by mercenary; the waves of a canyon by canyon.
TEST(away_ledger_adds_up_deliveries_and_waves) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    w.leave(b);
    for (int n = 1; n <= 3; n++) {
        ASSERT_TRUE(w.canyonSend(coldFork(w), fork, n));
        ASSERT_TRUE(w.canyonBeat(coldFork(w), fork));
    }
    const auto* rows = w.ledgerOf(keyB);
    ASSERT_TRUE(rows != nullptr && rows->size() == size_t(1));
    EXPECT_TRUE((*rows)[0].kind == net::AwayKind::waves && (*rows)[0].what == "The Fork");
    EXPECT_EQ((*rows)[0].value, int64_t(3));
    EXPECT_EQ(net::awayLine((*rows)[0]), std::string("The Fork: 3 waves beaten"));
}

// The server sends the report once, in the Progress after the join, and
// writes the summary file beside the save with the ledger's two lines.
TEST(away_server_sends_once_and_writes_the_summary_file) {
    const std::string path = fresh("away-server");
    const std::string file = net::awayFilePath(path);
    std::remove(file.c_str());
    EXPECT_EQ(file, std::string("build/save-tests/away-server.away"));
    std::string why;
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = first();
    o.worldParams = params();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    o.store = store.get();
    o.saved = store->load(&why);
    o.awayFile = file;
    Server server(serverPort, o);
    double now = 100;
    auto run = [&](HeadlessClient* c, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            if (c && c->connected()) server.tick(now);
            else server.idle(now);
            if (c) c->tick(now);
        }
    };
    {
        HeadlessClient client(clientPort, nettest::clientConfig());
        client.connect(now);
        client.join(*net::pilotKeyFromHex(keyA));
        run(&client, 3);
        ASSERT_TRUE(client.progress().has_value());
        EXPECT_TRUE(!client.away.has_value());
        client.disconnect(now, "bye");
        run(&client, 1);
    }
    run(nullptr, 1);
    EXPECT_EQ(server.world().away(), size_t(1));
    // What happens meanwhile.
    WorldMap& w = server.world();
    hold(w);
    run(nullptr, 2);
    const auto mine = w.awaySummaries();
    ASSERT_TRUE(mine.count(keyA) == 1);
    EXPECT_TRUE(mine.at(keyA)[1].find("The Fork held") != std::string::npos);
    const auto read = net::readAwayFile(file, keyA);
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(*read == mine.at(keyA));
    std::printf("  %s\n  %s\n", (*read)[0].c_str(), (*read)[1].c_str());
    // Back: told once.
    HeadlessClient again(clientPort, nettest::clientConfig());
    again.connect(now);
    again.join(*net::pilotKeyFromHex(keyA));
    run(&again, 3);
    ASSERT_TRUE(again.away.has_value());
    // The rows the file summed up (its time a little longer now).
    EXPECT_EQ(net::awaySummary(*again.away)[1], mine.at(keyA)[1]);
    EXPECT_TRUE(again.away->rows.size() == size_t(2));
    again.away = std::nullopt;
    run(&again, 6);
    EXPECT_TRUE(!again.away.has_value());
    // On line: out of the file.
    EXPECT_TRUE(!net::readAwayFile(file, keyA).has_value());
}
