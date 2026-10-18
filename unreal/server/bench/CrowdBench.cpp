// `crowdbench [SECONDS]`: what each pilot on line costs the server
// (docs/perf.md "Server"): N pilots (1, 4, 16) in the first world's start
// region at noon, each driving its Prospector round its own slow circle;
// the world steps at 60 Hz and every pilot's snapshot is built and written
// 20 times a second, acknowledged at once. SECONDS of game time a crowd
// (default 10) after 3 s. Prints the world's step, and a pilot's snapshot
// and writing, mean and 99th percentile, and the server's whole tick
// (step plus all the snapshots) per pilot, and hashes of every byte written
// and of every unit at the end (equal before and after a change that must
// keep the wire and the simulation). Built and run by `make bench`.

#include "NetSnapshot.h"
#include "WorldMap.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
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

void run(const LoadedWorld& world, int pilots, double seconds) {
    WorldParams p;
    p.heldHour = 12;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    WorldMap w(world, p);
    std::map<PilotId, net::SnapshotWriter> writers;
    for (int k = 1; k <= pilots; k++) {
        w.join(PilotId(k));
        writers[PilotId(k)];
    }
    int64_t tick = 0;
    auto drive = [&] {
        tick++;
        for (int k = 1; k <= pilots; k++) {
            PilotInput in;
            in.tick = tick;
            in.visit = w.visit(PilotId(k));
            in.walk = Vec2(1, 0);
            in.heading = double(tick) / 60 * 0.4 + double(k);
            w.input(PilotId(k), in);
        }
    };
    for (int i = 0; i < 180; i++) {
        drive();
        w.step();
    }
    std::vector<double> step, build, write, whole;
    uint64_t wire = 1469598103934665603ull, state = 1469598103934665603ull;
    double clock = 100;
    const int ticks = int(seconds * 60);
    for (int t = 0; t < ticks; t++) {
        drive();
        clock += 1.0 / 60;
        const double t0 = now();
        w.step();
        const double t1 = now();
        step.push_back(t1 - t0);
        double all = t1 - t0;
        if (t % 3 == 0)
            for (auto& [id, writer] : writers) {
                const double s0 = now();
                const net::Snapshot s = w.snapshot(id);
                const double s1 = now();
                const net::SnapshotWriter::Output out = writer.write(s, clock, 1 << 20);
                const double s2 = now();
                build.push_back(s1 - s0);
                write.push_back(s2 - s1);
                all += s2 - s0;
                net::AckMessage ack;
                ack.received.push_back({out.id, out.messages.size() >= 32 ? ~0u : (uint32_t(1) << out.messages.size()) - 1});
                writer.ack(ack, clock);
                for (const auto& m : out.messages)
                    for (uint8_t b : m) wire = (wire ^ b) * 1099511628211ull;
            }
        whole.push_back(all);
    }
    for (size_t r = 0; r < w.regionCount(); r++)
        for (const Unit& u : w.region(r).simulation().state.units)
            for (const double v : {u.position.x, u.position.y, u.heading, u.hp}) {
                uint64_t b;
                std::memcpy(&b, &v, sizeof b);
                state = (state ^ b) * 1099511628211ull;
            }
    std::printf("  %2d pilots: wire %016llx, state %016llx\n", pilots, (unsigned long long)wire, (unsigned long long)state);
    std::printf("  %2d pilots: step %6.1f us (p99 %6.1f), snapshot %5.1f us (p99 %5.1f), write %5.1f us (p99 %5.1f), tick %6.1f us, %5.1f us a pilot\n",
                pilots, mean(step), at(step, 0.99), mean(build), at(build, 0.99), mean(write), at(write, 0.99), mean(whole),
                mean(whole) / pilots);
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 10;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    std::printf("crowdbench: the start region at noon, %.0f s a crowd\n", seconds);
    for (const int n : {1, 4, 16}) run(*load.world, n, seconds);
    return 0;
}
