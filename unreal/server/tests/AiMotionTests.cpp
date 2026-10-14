// The pilot's Prospector while its AI drives it (handed back, gone mining),
// drawn by the client as the game draws it: the snapshots through the
// interpolator, 100 ms in the past, a frame every 1/60 s. In one process on
// the tests' network (net-tests' rig.h); a few seconds, no game played.

#include "test.h"

#include "HeadlessClient.h"
#include "NetInterpolator.h"
#include "Server.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {

using namespace ac;
using namespace ac::server;

/// The server and a client drawing others in the past, a step at a time.
/// The near band is small, so the Prospector at its deposit is past it,
/// as at a region's deposits far from its start base.
struct Scene {
    Scene()
        : serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress),
          server(serverPort, options()), client(clientPort, nettest::clientConfig()) {
        client.onSnapshot = [this](const net::SnapshotPart& part) {
            interp.heard(part.snapshot.serverTick, now);
            if (const net::Mirror* m = client.mirror()) interp.record(m->serverTick(), m->simulation().state.units);
        };
    }

    static ServerOptions options() {
        ServerOptions o;
        o.region.map = MapChoice{MapStyle::openField, MapSize::small, 2};
        o.token = nettest::kToken;
        o.secret = nettest::serverConfig().secret;
        o.interest.nearRadius = 3;
        o.interest.midRadius = 6;
        return o;
    }

    void step() {
        now += 1.0 / 60;
        server.tick(now);
        client.receive(now);
        client.sendInput();
        client.flush(now);
        if (net::Mirror* m = client.mirror())
            if (const auto at = interp.renderTick(now)) interp.apply(*at, m->simulation().state.units, m->driven());
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
    Server server;
    HeadlessClient client;
    net::Interpolator interp;
    double now = 100;
};

const Unit* unitIn(const Simulation& sim, int64_t id) {
    const auto i = sim.unitIndex(id);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

} // namespace

TEST(ai_driven_prospector_is_drawn_moving_smoothly) {
    Scene s;
    s.client.connect(s.now);
    s.client.join(net::newPilotKey());
    ASSERT_TRUE(s.until([&] { return s.client.mirror() && !s.client.mirror()->simulation().state.units.empty(); }, 10));
    const Simulation& truth = s.server.region().simulation();
    int64_t id = -1;
    for (const Unit& u : truth.state.units)
        if (u.owner == 0 && u.kind == Unit::Kind::prospector) { id = u.id; break; }
    ASSERT_TRUE(id >= 0);
    // Driven a moment, then let go: the side's AI has it again.
    s.client.take(id);
    ASSERT_TRUE(s.until([&] { return s.client.mirror()->driven() == id; }, 3));
    s.client.handBack();
    ASSERT_TRUE(s.until([&] { return !s.client.mirror()->driven(); }, 3));

    // Frames while the server moves it at a steady walk: how far the drawn
    // unit steps each frame, against how far the server's moved.
    std::vector<double> steps;
    std::vector<double> truthSteps;
    // As drawn: the mirror's simulation, which the blend writes (the
    // mirror's own copy is the newest snapshot's).
    auto drawnOf = [&]() { return unitIn(s.client.mirror()->simulation(), id); };
    const Unit* drawn = drawnOf();
    ASSERT_TRUE(drawn != nullptr);
    Vec2 last = drawn->position;
    Vec2 lastTruth = unitIn(truth, id)->position;
    for (int i = 0; i < 20 * 60; i++) {
        s.step();
        drawn = drawnOf();
        const Unit* server = unitIn(truth, id);
        if (!drawn || !server) break;
        steps.push_back(distance(drawn->position, last));
        truthSteps.push_back(distance(server->position, lastTruth));
        last = drawn->position;
        lastTruth = server->position;
    }
    // Only the frames the server moved it steadily for: the frames drawn
    // 100 ms (6 frames) late, from the two snapshots either side of that
    // moment, so its stops at the deposit and the depot are not counted.
    const int delay = 6;
    double sum = 0, jitter = 0, worst = 0;
    int frames = 0, stalls = 0;
    for (size_t i = 12; i + 12 < steps.size(); i++) {
        bool steady = true;
        for (int k = -6; k <= 3 && steady; k++) steady = truthSteps[size_t(int(i) - delay + k)] > 0.02;
        if (!steady) continue;
        frames += 1;
        sum += steps[i];
        const double d = std::abs(steps[i] - steps[i - 1]);
        jitter += d * d;
        worst = std::max(worst, d);
        if (steps[i] < 0.005) stalls += 1;
    }
    ASSERT_TRUE(frames > 30);
    const double mean = sum / frames;
    const double rms = std::sqrt(jitter / frames);
    std::printf("  AI-driven Prospector, %d moving frames: step %.4f cells a frame, jitter %.4f rms (%.0f%% of the step), "
                "worst %.4f, %d stalled frames\n",
                frames, mean, rms, 100 * rms / mean, worst, stalls);
    // As smooth as a steady walk: no stalled frames, the step steady to a
    // tenth of itself.
    EXPECT_EQ(stalls, 0);
    EXPECT_TRUE(rms < 0.1 * mean);
}
