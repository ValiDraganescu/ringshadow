// The server and the headless client in one process on the tests' network
// (net-tests' rig.h), the client's end through the lag simulator. Isolated
// scenes of a few seconds.

#include "test.h"

#include "HeadlessClient.h"
#include "Server.h"
#include "rig.h"

#include <cstdio>
#include <map>

namespace {

using namespace ac;
using namespace ac::server;

/// 150 ms each way, up to 20 ms more, 5% lost, duplicated and held back.
net::LagSettings rough() {
    net::LagSettings s;
    s.delay = 0.15;
    s.jitter = 0.02;
    s.loss = 0.05;
    s.duplicate = 0.05;
    s.reorder = 0.05;
    return s;
}

struct Scene {
    explicit Scene(const net::LagSettings& lag, MapChoice map = MapChoice{MapStyle::openField, MapSize::small, 2})
        : serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress), clientLag(clientPort, 11),
          server(serverPort, options(map)), client(clientLag, nettest::clientConfig()) {
        clientLag.setDefault(lag);
    }

    static ServerOptions options(MapChoice map) {
        ServerOptions o;
        o.region.map = map;
        o.token = nettest::kToken;
        o.secret = nettest::serverConfig().secret;
        return o;
    }

    void step(bool stepRegion = true) {
        now += 1.0 / 60;
        server.tick(now, stepRegion);
        client.tick(now);
    }
    /// Runs until `done` or for `seconds`; false if it never was.
    template <class F> bool until(F&& done, double seconds, bool stepRegion = true) {
        for (int i = 0; i < int(seconds * 60); i++) {
            step(stepRegion);
            if (done()) return true;
        }
        return false;
    }
    bool connect() {
        client.connect(now);
        client.join(net::newPilotKey());
        return until([&] { return client.mirror() != nullptr; }, 10);
    }

    nettest::MemoryNet network;
    nettest::MemoryPort serverPort;
    nettest::MemoryPort clientPort;
    net::LagSimulator clientLag;
    Server server;
    HeadlessClient client;
    double now = 100;
};

/// What side 0 knows now, as its client should mirror it.
struct Known {
    std::map<int64_t, Unit> units;
    std::map<int64_t, Structure> structures;
    std::map<int64_t, Structure> remembered;
};

Known known(const Simulation& sim, int64_t me) {
    Known k;
    const GameState& s = sim.state;
    for (const Unit& u : s.units) {
        if (s.allied(u.owner, me)) k.units[u.id] = net::quantised(u);
        else if (sim.sees(me, u.id)) k.units[u.id] = net::quantised(scrubbed(u));
    }
    for (const Structure& b : s.structures)
        if (s.allied(b.owner, me) || sim.sees(me, b.id)) k.structures[b.id] = net::quantised(b);
    if (s.intel)
        for (const Structure& b : (*s.intel)[size_t(me)].buildings)
            if (!sim.sees(me, b.id)) k.remembered[b.id] = net::quantised(b);
    return k;
}

/// The mirror holds what the server's side knows, after quantisation.
bool mirrored(const Simulation& truth, const net::Mirror& m) {
    const Known k = known(truth, 0);
    const GameState& ms = m.simulation().state;
    if (ms.units.size() != k.units.size() || ms.structures.size() != k.structures.size()) return false;
    for (const Unit& u : ms.units) {
        const auto it = k.units.find(u.id);
        if (it == k.units.end() || !(it->second == u)) return false;
    }
    for (const Structure& b : ms.structures) {
        const auto it = k.structures.find(b.id);
        if (it == k.structures.end() || !(it->second == b)) return false;
    }
    if (m.remembered() != k.remembered) return false;
    if (ms.players.size() != truth.state.players.size()) return false;
    if (!(ms.players[0] == net::quantised(truth.state.players[0]))) return false;
    // The fog: what side 0 sees, has explored, and its sites.
    net::FogView f;
    f.seen = truth.sights[0].seen;
    f.near = truth.sights[0].near;
    f.explored = (*truth.state.intel)[0].explored;
    f.sites = (*truth.state.intel)[0].sites;
    const net::FogView q = net::quantised(f);
    const Simulation& sim = m.simulation();
    if (sim.sights.empty() || sim.sights[0].seen != q.seen || sim.sights[0].near != q.near) return false;
    if (!ms.intel || (*ms.intel)[0].explored != q.explored || (*ms.intel)[0].sites != q.sites) return false;
    // The patches and wells in sight.
    for (size_t i = 0; i < truth.state.patches.size(); i++)
        if (truth.sees(0, truth.state.patches[i].position) && !(ms.patches[i] == net::quantised(truth.state.patches[i]))) return false;
    return true;
}

int64_t ownProspector(const Simulation& sim) {
    for (const Unit& u : sim.state.units)
        if (u.owner == 0 && u.kind == Unit::Kind::prospector) return u.id;
    return -1;
}

} // namespace

TEST(network_welcome_and_mirror) {
    Scene s({});
    ASSERT_TRUE(s.connect());
    const net::Welcome& w = *s.client.welcome();
    EXPECT_TRUE(w.region == s.server.region().description());
    EXPECT_TRUE(s.client.mirror()->matches());
    EXPECT_EQ(s.server.clients(), size_t(1));
    // A snapshot or two later the mirror has the side's start.
    ASSERT_TRUE(s.until([&] { return !s.client.mirror()->simulation().state.units.empty(); }, 1));
    EXPECT_TRUE(s.client.mirror()->unit(ownProspector(s.server.region().simulation())) != nullptr);
}

TEST(network_mirror_converges_under_lag) {
    Scene s(rough());
    ASSERT_TRUE(s.connect());
    const int64_t unit = ownProspector(s.server.region().simulation());
    const uint32_t req = s.client.take(unit);
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(req) > 0; }, 3));
    EXPECT_TRUE(s.client.replies[req].done);
    // Four seconds of the game, driving, the AIs at work; then the server
    // holds still and the mirror catches up.
    s.client.steer.walk = Vec2(1, 0);
    s.until([] { return false; }, 2);
    s.client.steer.walk = Vec2(0, 0);
    s.until([] { return false; }, 2);
    const bool caught = s.until([&] { return mirrored(s.server.region().simulation(), *s.client.mirror()); }, 2, false);
    ASSERT_TRUE(caught);
    // And it stays so.
    s.until([] { return false; }, 0.5, false);
    EXPECT_TRUE(mirrored(s.server.region().simulation(), *s.client.mirror()));
    EXPECT_TRUE(s.client.partsRead > 0);
    EXPECT_TRUE(s.clientLag.lost() > 0);
    // The comparison sees a stale mirror: a second on without the client.
    for (int i = 0; i < 60; i++) s.server.region().step();
    EXPECT_TRUE(!mirrored(s.server.region().simulation(), *s.client.mirror()));
}

TEST(network_mirror_catches_up_within_a_few_snapshots) {
    Scene s(rough());
    ASSERT_TRUE(s.connect());
    s.until([] { return false; }, 3);
    // Held still: count the snapshots until the mirror matches.
    int ticks = 0;
    const bool caught = s.until(
        [&] {
            ticks += 1;
            return mirrored(s.server.region().simulation(), *s.client.mirror());
        },
        3, false);
    ASSERT_TRUE(caught);
    const int snapshots = (ticks + 2) / 3;
    std::printf("  caught up %d snapshots after the server held still\n", snapshots);
    // A round trip under this lag is about 7 snapshots; a lost packet adds one.
    EXPECT_TRUE(snapshots <= 16);
}

TEST(network_take_refused_with_a_reason) {
    Scene s({});
    ASSERT_TRUE(s.connect());
    // Another side's unit.
    int64_t theirs = -1;
    for (const Unit& u : s.server.region().simulation().state.units)
        if (u.owner == 1) theirs = u.id;
    const uint32_t a = s.client.take(theirs);
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(a) > 0; }, 2));
    EXPECT_TRUE(!s.client.replies[a].done);
    EXPECT_TRUE(s.client.replies[a].reason == "that unit cannot be driven");
    const uint32_t b = s.client.handBack();
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(b) > 0; }, 2));
    EXPECT_TRUE(!s.client.replies[b].done);
    // Taken and handed back.
    const uint32_t c = s.client.take(ownProspector(s.server.region().simulation()));
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(c) > 0; }, 2));
    EXPECT_TRUE(s.client.replies[c].done);
    ASSERT_TRUE(s.until([&] { return s.client.mirror()->driven().has_value(); }, 1));
    const uint32_t d = s.client.handBack();
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(d) > 0; }, 2));
    EXPECT_TRUE(s.client.replies[d].done);
    ASSERT_TRUE(s.until([&] { return !s.client.mirror()->driven().has_value(); }, 1));
}

TEST(network_disconnect_leaves_the_region) {
    Scene s({});
    ASSERT_TRUE(s.connect());
    const uint32_t c = s.client.take(ownProspector(s.server.region().simulation()));
    ASSERT_TRUE(s.until([&] { return s.client.replies.count(c) > 0; }, 2));
    const PilotId pilot = s.client.welcome()->pilot;
    EXPECT_TRUE(s.server.region().driving(pilot).has_value());
    s.server.stop(s.now, "bye");
    EXPECT_EQ(s.server.clients(), size_t(0));
    EXPECT_TRUE(!s.server.region().joined(pilot));
    ASSERT_TRUE(s.until([&] { return !s.client.connected(); }, 2));
    EXPECT_TRUE(s.client.disconnectReason() == "bye");
}
