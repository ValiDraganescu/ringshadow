// `netbench [SECONDS]`: what the protocol costs on the wire. The server and
// one headless client in one process on the tests' in-memory network (no
// lag, a 60 Hz clock of its own), for SECONDS of game time (default 20) per
// scene: a quiet one (a new game on The Open Field, small, the pilot standing
// in its Prospector) and the core bench's late 4v4 (Badlands · Large · 8,
// staged by core-tests/bench/LateStage.h, the pilot driving a soldier
// forward and turning). Prints bytes a second each way, for the first 2 s
// (everything new) and after, and the snapshot messages a snapshot. Built
// and run by `make bench` (-O3).

#include "HeadlessClient.h"
#include "LateStage.h"
#include "Server.h"
#include "rig.h"

#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>

using namespace ac;
using namespace ac::server;

namespace {

std::optional<int64_t> pick(const Simulation& sim, bool soldier) {
    std::optional<int64_t> worker;
    for (const Unit& u : sim.state.units) {
        if (u.owner != Pilot::player || !sim.canDrive(u)) continue;
        if (soldier && u.soldier()) return u.id;
        if (!worker) worker = u.id;
    }
    return worker;
}

void run(const std::string& name, const RegionConfig& region, bool drive, double seconds) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.region = region;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    auto step = [&] {
        now += 1.0 / 60;
        server.tick(now);
        client.tick(now);
    };
    client.connect(now);
    for (int i = 0; i < 600 && !client.mirror(); i++) step();
    if (!client.mirror()) {
        std::printf("%s: no connection\n", name.c_str());
        return;
    }
    const std::optional<int64_t> unit = pick(server.region().simulation(), drive);
    if (unit) client.take(*unit);
    const double heading = unit ? server.region().simulation().state.unit(*unit)->heading : 0;
    auto stats = [&] { return server.clientStats().front(); };
    const auto start = stats();
    const uint64_t upStart = client.endpoint().stats() ? client.endpoint().stats()->bytesSent : 0;
    std::optional<Server::ClientStats> at2;
    uint64_t up2 = 0;
    const int64_t ticks = int64_t(seconds * 60);
    for (int64_t k = 1; k <= ticks; k++) {
        if (drive) {
            client.steer.walk = Vec2(1, 0);
            client.steer.heading = heading + 0.5 * double((k / 60) % 4);
        }
        step();
        if (k == 120) {
            at2 = stats();
            up2 = client.endpoint().stats()->bytesSent;
        }
    }
    const auto end = stats();
    const uint64_t up = client.endpoint().stats()->bytesSent;
    const double first = std::min(2.0, seconds), after = seconds - first;
    std::printf("%s: %.0f game s, %s, mirror holds %zu units and %zu buildings\n", name.c_str(), seconds,
                drive ? "driving" : "standing", client.mirror()->simulation().state.units.size(),
                client.mirror()->simulation().state.structures.size());
    if (at2) {
        std::printf("  server → client  first 2 s %7.0f B/s, after %7.0f B/s\n", double(at2->bytesSent - start.bytesSent) / first,
                    after > 0 ? double(end.bytesSent - at2->bytesSent) / after : 0);
        std::printf("  client → server  first 2 s %7.0f B/s, after %7.0f B/s\n", double(up2 - upStart) / first,
                    after > 0 ? double(up - up2) / after : 0);
    }
    const int64_t snaps = end.snapshots - start.snapshots;
    std::printf("  snapshots %lld, messages a snapshot %.2f, entities deferred for the budget %lld, full resends %lld\n",
                static_cast<long long>(snaps),
                snaps ? double(end.snapshotMessages - start.snapshotMessages) / double(snaps) : 0.0,
                static_cast<long long>(end.deferred - start.deferred), static_cast<long long>(end.fullResets));
    std::printf("  client read %lld snapshot messages, %lld unreadable\n", static_cast<long long>(client.partsRead),
                static_cast<long long>(client.partsFailed));
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 20;
    {
        RegionConfig c;
        c.map = MapChoice{MapStyle::openField, MapSize::small, 2};
        run(c.map.name() + ", new game (quiet)", c, false, seconds);
    }
    {
        RegionConfig c;
        c.map = MapChoice{MapStyle::badlands, MapSize::large, 8};
        c.state = bench::stageLate(WindowMaps::buildSquare(c.map), {0, 0, 0, 0, 1, 1, 1, 1});
        run(c.map.name() + ", late 4v4 staged", c, true, seconds);
    }
    return 0;
}
