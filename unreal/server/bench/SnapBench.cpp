// `snapbench [SECONDS]`: the snapshots' cost on the server (docs/perf.md
// "Server"): building a pilot's snapshot (`WorldMap::snapshot`) and writing
// it for the wire (`net::SnapshotWriter::write`), each timed apart, 20 a
// second, the client acknowledging every part at once. The first world,
// one pilot on the first pad of the start region and of Slag Pockets (the
// Matriarch's nest full), at noon (S3: by day it sees the whole region,
// and the fog goes with it) and at midnight; SECONDS of game time a scene
// (default 20) after 3 s. Prints the mean and 99th percentile of each, the
// bytes a snapshot, and a hash of every byte written (equal before and
// after a change that must keep the wire). Built and run by `make bench`.

#include "NetSnapshot.h"
#include "WorldMap.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

double now() { return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

double at(std::vector<double> v, double f) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(size_t(double(v.size()) * f), v.size() - 1)];
}

double mean(const std::vector<double>& v) {
    double s = 0;
    for (double x : v) s += x;
    return v.empty() ? 0 : s / double(v.size());
}

void run(const LoadedWorld& world, const char* region, double hour, double seconds) {
    WorldParams p;
    p.heldHour = hour;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    WorldMap w(world, p);
    constexpr PilotId me = 1;
    w.join(me);
    const std::string id = region;
    const std::optional<size_t> r = id == "nest" ? w.nestRegion() : w.regionNamed(id);
    if (!r || (*r != w.where(me) && !w.devTransfer(me, *r, w.region(*r).places()->pads[0].at))) {
        std::printf("%s: no region\n", region);
        return;
    }
    for (int i = 0; i < 180; i++) w.step();
    net::SnapshotWriter writer;
    std::vector<double> build, write;
    double bytes = 0;
    uint64_t hash = 1469598103934665603ull;
    const int ticks = int(seconds * 60);
    double clock = 100;
    for (int t = 0; t < ticks; t++) {
        w.step();
        clock += 1.0 / 60;
        if (t % 3) continue;
        const double t0 = now();
        w.beginSnapshots();
        const net::Snapshot s = w.snapshot(me);
        w.endSnapshots();
        const double t1 = now();
        const net::SnapshotWriter::Output out = writer.write(s, clock, 1 << 20);
        const double t2 = now();
        build.push_back(t1 - t0);
        write.push_back(t2 - t1);
        net::AckMessage ack;
        ack.received.push_back({out.id, out.messages.size() >= 32 ? ~0u : (uint32_t(1) << out.messages.size()) - 1});
        writer.ack(ack, clock);
        for (const auto& m : out.messages) {
            bytes += double(m.size());
            for (uint8_t b : m) {
                hash ^= b;
                hash *= 1099511628211ull;
            }
        }
    }
    std::printf("  %-6s %-8s snapshot %6.1f us (p99 %6.1f), write %6.1f us (p99 %6.1f), %6.0f B a snapshot, wire %016llx\n",
                region, hour == 12 ? "noon" : "midnight", mean(build), at(build, 0.99), mean(write), at(write, 0.99),
                bytes / double(write.size()), (unsigned long long)hash);
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 20;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    std::printf("snapbench: %.0f s a scene, 20 snapshots a second\n", seconds);
    for (const char* region : {"start", "nest"})
        for (const double hour : {12.0, 0.0}) run(*load.world, region, hour, seconds);
    return 0;
}
