// `daybench [SECONDS]`: what daylight sight (S3: by day a pilot sees its
// whole region, and all of it goes in its snapshots) costs. The world server
// and one headless client in one process on the tests' in-memory network,
// the pilot standing on the region's first pad, for SECONDS of game time
// (default 20) per run, after 2 s for everything new. Two full regions: Slag
// Pockets with the Matriarch's nest full (her horde of Shardlings and
// Gloamwings), and Cold Fork with its last canyon wave out (the biggest); each at noon with the
// daylight, at noon without it (the sight before S3) and at midnight. Prints
// bytes a second to the client, the snapshot messages a snapshot, entities
// the budget held back, the world's tick (all regions) and the client's tick
// (reading the snapshots into its mirror), mean and 99th percentile. Built
// and run by `make bench` (-O3).

#include "HeadlessClient.h"
#include "Server.h"
#include "WorldMap.h"
#include "WorldClock.h"
#include "rig.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

double wall() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

double at(std::vector<double> v, double f) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(size_t(double(v.size()) * f), v.size() - 1)];
}

double mean(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.empty() ? 0 : s / double(v.size());
}

enum class Scene { nest, canyon };

void run(const LoadedWorld& world, Scene scene, double hour, bool daylight, double seconds) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = world;
    o.worldParams.heldHour = hour;
    o.worldParams.contracts = false;
    o.worldParams.starfalls = false;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    WorldMap& w = server.world();
    for (size_t r = 0; r < w.regionCount(); r++) w.region(r).setDaylight(daylight);
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    std::vector<double> serverMs, clientMs;
    auto step = [&](bool timed) {
        now += 1.0 / 60;
        const double a = wall();
        server.tick(now);
        const double b = wall();
        client.tick(now);
        const double c = wall();
        if (timed) {
            serverMs.push_back(b - a);
            clientMs.push_back(c - b);
        }
    };
    client.connect(now);
    client.join(net::newPilotKey());
    for (int i = 0; i < 600 && !(client.mirror() && client.mirror()->driven()); i++) step(false);
    if (!client.mirror() || server.clientStats().empty()) {
        std::printf("no connection\n");
        return;
    }
    const PilotId pilot = server.clientStats().front().pilot;
    const size_t r = scene == Scene::nest ? *w.nestRegion() : *w.regionNamed("cold-fork");
    if (!w.devTransfer(pilot, r, w.region(r).places()->pads[0].at)) {
        std::printf("no transfer\n");
        return;
    }
    // The nest fills, or a wave walks out of its mouths, and the new
    // region's first snapshots come in.
    for (int i = 0; i < 60; i++) step(false);
    if (scene == Scene::canyon && !w.canyonSend(r, w.region(r).places()->zones.front().id, WorldCanyon::count())) std::printf("  (no wave)\n");
    for (int i = 0; i < 120; i++) step(false);
    const auto start = server.clientStats().front();
    const int64_t ticks = int64_t(seconds * 60);
    size_t most = 0;
    for (int64_t k = 0; k < ticks; k++) {
        step(true);
        if (client.mirror()) most = std::max(most, client.mirror()->simulation().state.units.size());
    }
    const auto end = server.clientStats().front();
    size_t units = 0, bugs = 0;
    for (const Unit& u : w.region(r).simulation().state.units)
        if (u.hp > 0) {
            units++;
            if (u.bug) bugs++;
        }
    const int64_t snaps = end.snapshots - start.snapshots;
    std::printf("%-6s %-9s %-11s %7.0f B/s  %5.2f msg/snap  %5lld held back  %3zu/%3zu units told (%zu bugs)"
                "  world tick %.2f ms (p99 %.2f)  client tick %.3f ms (p99 %.3f)\n",
                scene == Scene::nest ? "nest" : "canyon", hour == 12 ? "noon" : "midnight", daylight ? "daylight" : "before S3",
                double(end.bytesSent - start.bytesSent) / seconds,
                snaps ? double(end.snapshotMessages - start.snapshotMessages) / double(snaps) : 0.0,
                static_cast<long long>(end.deferred - start.deferred), most, units, bugs, mean(serverMs), at(serverMs, 0.99),
                mean(clientMs), at(clientMs, 0.99));
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 20;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    for (const Scene s : {Scene::nest, Scene::canyon}) {
        run(*load.world, s, 12, false, seconds);
        run(*load.world, s, 12, true, seconds);
        run(*load.world, s, 0, true, seconds);
    }
    return 0;
}
