// `spreadbench [SECONDS]`: a world with its pilots spread over its regions
// (docs/perf.md "Server"): The First at noon, one pilot on the first pad of
// each of 1, 2 and 4 regions (the start, Slag Pockets with the Matriarch's
// nest, Cold Fork with its canyon wave out, Glass Flats), then eight on each
// of the first two, each driving round a slow circle; the
// world steps SECONDS of game time (default 20) at 60 Hz after 3 s.
// Prints the world's step (all regions), mean, p95 and max, and a hash of
// every unit in every region at the end (equal however the regions are
// stepped). Built and run by `make bench` (-O3).

#include "WorldMap.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void run(const LoadedWorld& world, int spread, int each, double seconds) {
    WorldParams p;
    p.heldHour = 12;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    WorldMap w(world, p);
    const char* regions[] = {"start", "nest", "cold-fork", "glass-flats"};
    const int pilots = spread * each;
    for (int k = 0; k < pilots; k++) {
        const PilotId id = PilotId(k + 1);
        w.join(id);
        const std::string name = regions[k % spread];
        const std::optional<size_t> r = name == "nest" ? w.nestRegion() : w.regionNamed(name);
        if (r && *r != w.where(id)) w.devTransfer(id, *r, w.region(*r).places()->pads[0].at);
    }
    // Cold Fork's canyon wave out, a second region full of bugs.
    if (spread >= 3) {
        const size_t r = *w.regionNamed("cold-fork");
        for (int i = 0; i < 60; i++) w.step();
        w.canyonSend(r, w.region(r).places()->zones.front().id, WorldCanyon::count());
    }
    int64_t tick = 0;
    auto drive = [&] {
        tick++;
        for (int k = 0; k < pilots; k++) {
            PilotInput in;
            in.tick = tick;
            in.visit = w.visit(PilotId(k + 1));
            in.walk = Vec2(1, 0);
            in.heading = double(tick) / 60 * 0.4 + double(k);
            w.input(PilotId(k + 1), in);
        }
    };
    for (int i = 0; i < 180; i++) {
        drive();
        w.step();
    }
    std::vector<double> step;
    const int ticks = int(seconds * 60);
    for (int t = 0; t < ticks; t++) {
        drive();
        const double t0 = now();
        w.step();
        step.push_back(now() - t0);
    }
    uint64_t hash = 1469598103934665603ull;
    for (size_t r = 0; r < w.regionCount(); r++)
        for (const Unit& u : w.region(r).simulation().state.units)
            for (const double v : {u.position.x, u.position.y, u.heading, u.hp}) {
                uint64_t b;
                std::memcpy(&b, &v, sizeof b);
                hash = (hash ^ b) * 1099511628211ull;
            }
    double sum = 0;
    for (double x : step) sum += x;
    std::printf("  %d regions with %d pilot%s: step %6.1f us (p95 %6.1f, max %7.1f), state %016llx", spread, each, each == 1 ? " " : "s", sum / double(step.size()),
                at(step, 0.95), at(step, 1), (unsigned long long)hash);
    if (w.stepWakeUs() > 0) std::printf(", wake %.1f us, %zu of %d on the pool", w.stepWakeUs(), w.pooledSteps(), ticks + 180 + (spread >= 3 ? 60 : 0));
    std::printf("\n");
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 20;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    std::printf("spreadbench: The First at noon, %.0f s\n", seconds);
    for (const int n : {1, 2, 4}) run(*load.world, n, 1, seconds);
    run(*load.world, 2, 8, seconds);
    return 0;
}
