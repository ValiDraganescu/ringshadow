// The driven machine's prediction (net::Predictor) against the server, in
// one process on the tests' network (net-tests' rig.h), the client's end
// through the lag simulator: 150 ms each way, up to 20 ms more, 5% lost. A
// scripted drive of a few seconds; no game played.

#include "test.h"

#include "HeadlessClient.h"
#include "NetPredictor.h"
#include "Server.h"
#include "rig.h"

#include <cmath>
#include <cstdio>

namespace {

using namespace ac;
using namespace ac::server;

net::LagSettings lagged() {
    net::LagSettings s;
    s.delay = 0.15;
    s.jitter = 0.02;
    s.loss = 0.05;
    return s;
}

/// The server and a predicting client, a step at a time as the game ticks.
struct Scene {
    Scene()
        : serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress), clientLag(clientPort, 23),
          server(serverPort, options()), client(clientLag, nettest::clientConfig()) {
        clientLag.setDefault(lagged());
    }

    static ServerOptions options() {
        ServerOptions o;
        o.region.map = MapChoice{MapStyle::openField, MapSize::small, 2};
        o.token = nettest::kToken;
        o.secret = nettest::serverConfig().secret;
        return o;
    }

    /// One 1/60 s: the server's tick, then the client's: what came in, the
    /// prediction put right, this tick's input sent and applied at once,
    /// the offset eased.
    void step() {
        now += 1.0 / 60;
        server.tick(now);
        client.receive(now);
        if (client.mirror()) predictor.reconcile(*client.mirror());
        if (const auto in = client.sendInput(); in && client.mirror()) predictor.input(*client.mirror(), *in);
        client.flush(now);
        predictor.ease(1.0 / 60);
    }
    template <class F> bool until(F&& done, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            step();
            if (done()) return true;
        }
        return false;
    }

    nettest::MemoryNet network;
    nettest::MemoryPort serverPort;
    nettest::MemoryPort clientPort;
    net::LagSimulator clientLag;
    Server server;
    HeadlessClient client;
    net::Predictor predictor;
    double now = 100;
};

const Unit* unitIn(const Simulation& sim, int64_t id) {
    const auto i = sim.unitIndex(id);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

} // namespace

TEST(prediction_follows_the_server_under_lag) {
    Scene s;
    s.client.connect(s.now);
    s.client.join(net::newPilotKey());
    ASSERT_TRUE(s.until([&] { return s.client.mirror() && !s.client.mirror()->simulation().state.units.empty(); }, 10));
    const Simulation& truth = s.server.region().simulation();
    int64_t id = -1;
    for (const Unit& u : truth.state.units)
        if (u.owner == 0 && u.kind == Unit::Kind::prospector) { id = u.id; break; }
    ASSERT_TRUE(id >= 0);
    const uint32_t req = s.client.take(id);
    ASSERT_TRUE(s.until([&] { return s.client.mirror()->driven() == id && s.predictor.predicted(); }, 3));
    EXPECT_TRUE(s.client.replies[req].done);

    // Where the ground is open longest, as the mirror's map says.
    const Simulation& mirror = s.client.mirror()->simulation();
    const Unit start = *s.predictor.predicted();
    double h0 = 0, best = -1;
    for (int k = 0; k < 16; k++) {
        const double h = k * 2 * pi / 16;
        double clear = 0;
        while (clear < 12 && mirror.pilotCanStand(start, start.position + Vec2(std::cos(h), std::sin(h)) * (clear + 0.25))) clear += 0.25;
        if (clear > best) { best = clear; h0 = h; }
    }

    // Three seconds of forward and turning: ahead, a weave while
    // sidestepping, a turn back.
    s.predictor.clearStats();
    double heading = h0;
    bool movedAtOnce = false;
    for (int i = 0; i < 180; i++) {
        const double t = i / 60.0;
        heading = h0 + 0.6 * std::sin(t * 2.5);
        s.client.steer.heading = heading;
        s.client.steer.walk = t < 1 ? Vec2(1, 0) : t < 2 ? Vec2(1, 0.5) : Vec2(0.8, -0.4);
        const Vec2 before = s.predictor.predicted()->position;
        s.step();
        // The first input moves it, a round trip before the server could.
        if (i == 0) movedAtOnce = distance(s.predictor.predicted()->position, before) > 0.01;
    }
    const net::Predictor::Stats driving = s.predictor.stats();
    const Unit* server = unitIn(truth, id);
    ASSERT_TRUE(server != nullptr);
    const double walked = distance(server->position, start.position);

    // The keys let go; the server uses the last inputs and the snapshots
    // after them come in.
    s.client.steer.walk = Vec2(0, 0);
    s.until([] { return false; }, 1.5);
    server = unitIn(truth, id);
    ASSERT_TRUE(server != nullptr);
    const Unit& predicted = *s.predictor.predicted();
    const Unit shown = *s.predictor.shown();
    const double gap = distance(predicted.position, server->position);
    const double turn = std::abs(std::remainder(predicted.heading - server->heading, 2 * pi));

    const double meanError = driving.corrections ? driving.errorSum / double(driving.corrections) : 0;
    const double meanEased = driving.frames ? driving.easedSum / double(driving.frames) : 0;
    std::printf("  drove %.2f cells in 3 s; %lld corrections (mean %.4f, max %.4f cells), %lld snaps; "
                "drawn unit corrected %.5f cells a frame on average (%lld frames)\n",
                walked, (long long)driving.corrections, meanError, driving.errorMax, (long long)driving.snaps, meanEased,
                (long long)driving.frames);
    std::printf("  at rest: predicted %.4f cells and %.6f rad from the server's; drawn %.4f cells from it\n", gap, turn,
                distance(shown.position, server->position));

    EXPECT_TRUE(movedAtOnce);
    EXPECT_TRUE(walked > 3);
    // Within the snapshot's centimetres, the heading as the input set it.
    EXPECT_TRUE(gap <= 0.008);
    EXPECT_TRUE(turn <= 1e-3);
    EXPECT_TRUE(distance(shown.position, server->position) <= 0.01);
    // Small corrections while driving: no jumps, and the drawn unit eased
    // by a few millimetres a frame.
    EXPECT_EQ(driving.snaps, int64_t(0));
    EXPECT_TRUE(meanEased < 0.01);
    EXPECT_TRUE(s.clientLag.lost() > 0);
}
