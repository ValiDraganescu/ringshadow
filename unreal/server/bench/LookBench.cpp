// `lookbench [LOOKS]`: what the fog costs a world region (docs/perf.md
// "Server"): `Simulation::look`, which the region runs every
// `Simulation::lookEvery` (0.1 s), timed alone. The first world, one pilot
// on the first pad of three regions: the start, Slag Pockets with the
// Matriarch's nest full (her horde) and Cold Fork; each at noon (S3: by
// day the pilot sees the whole region) and at midnight. LOOKS times
// (default 2000) a scene, after 3 s of steps. Prints the mean and the
// 99th percentile of one look, and a hash of every side's sight and
// explored cells after the last (equal before and after a change that must
// keep the fog). Built and run by `make bench` (-O3).

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

void run(const LoadedWorld& world, const char* region, double hour, int looks) {
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
    if (!r) {
        std::printf("%s: no region\n", region);
        return;
    }
    if (*r != w.where(me) && !w.devTransfer(me, *r, w.region(*r).places()->pads[0].at)) {
        std::printf("%s: no transfer\n", region);
        return;
    }
    for (int i = 0; i < 180; i++) w.step();
    Simulation& sim = w.region(*r).simulation();
    std::vector<double> us;
    us.reserve(size_t(looks));
    for (int k = 0; k < looks; k++) {
        const double t0 = now();
        sim.lookNow();
        us.push_back(now() - t0);
    }
    double sum = 0;
    for (double x : us) sum += x;
    size_t units = 0;
    for (const Unit& u : sim.state.units) units += u.hp > 0 ? 1 : 0;
    // What the last look left: every side's sight and explored cells.
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&hash](uint64_t v) { hash = (hash ^ v) * 1099511628211ull; };
    for (const Sight& s : sim.sights) {
        for (const uint8_t b : s.seen) mix(b);
        for (const uint8_t b : s.near) mix(b);
        for (const int64_t i : s.ids) mix(uint64_t(i));
    }
    if (sim.state.intel)
        for (const Intel& k : *sim.state.intel)
            for (const uint64_t word : k.explored) mix(word);
    std::printf("  %-13s %-8s %4zu units, %zu sides: look mean %7.1f us, p99 %7.1f us, sights %016llx\n", region,
                hour == 12 ? "noon" : "midnight", units, sim.state.players.size(), sum / double(looks), at(us, 0.99),
                (unsigned long long)hash);
}

} // namespace

int main(int argc, char** argv) {
    const int looks = argc > 1 ? std::max(1, std::atoi(argv[1])) : 2000;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    std::printf("lookbench: %d looks a scene\n", looks);
    for (const char* region : {"start", "nest", "cold-fork"})
        for (const double hour : {12.0, 0.0}) run(*load.world, region, hour, looks);
    return 0;
}
