// `worldbench [SECONDS] [WORLD] [REGION]`: the world's tick, timed
// (mmorpg-m2.md P5: every region steps every tick). The first world
// (worlds/first.json) by default; one pilot drives its Prospector round in a
// slow circle from the start pad (with REGION, a region id or `nest`: from
// that region's first pad, after 3 s there), the world steps SECONDS of game time (default 30) at 60 Hz and
// builds the pilot's snapshot every third tick. Prints the spread of the
// whole step (all regions), each region's mean, and a hash of every unit's
// position, heading and hp in every region at the end (equal before and
// after a change that must keep the simulation bit for bit). Built and run by
// `make bench` (-O3).

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

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

double at(std::vector<double> v, double f) {
    std::sort(v.begin(), v.end());
    return v.empty() ? 0 : v[std::min(size_t(double(v.size()) * f), v.size() - 1)];
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 30;
    const std::string path = argc > 2 ? argv[2] : "worlds/first.json";
    WorldLoad load = loadWorld(path);
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    WorldMap w(*load.world);
    const PilotId pilot = 1;
    w.join(pilot);
    if (argc > 3) {
        const std::string id = argv[3];
        const std::optional<size_t> r = id == "nest" ? w.nestRegion() : w.regionNamed(id);
        if (!r || (*r != w.where(pilot) && !w.devTransfer(pilot, *r, w.region(*r).places()->pads[0].at))) {
            std::fprintf(stderr, "%s: no region\n", id.c_str());
            return 1;
        }
        for (int i = 0; i < 180; i++) w.step();
    }
    const int ticks = int(seconds * 60);
    std::vector<double> step, snap;
    std::vector<double> regionSum(w.regionCount(), 0);
    for (int t = 0; t < ticks; t++) {
        PilotInput in;
        in.tick = t + 1;
        in.walk = Vec2(1, 0);
        in.heading = double(t) / 60 * 0.4;
        w.input(pilot, in);
        const double t0 = now();
        w.step();
        step.push_back((now() - t0) * 1000);
        // Each region alone, once more (same work, timed apart).
        if (t % 60 == 0)
            for (size_t r = 0; r < w.regionCount(); r++) {
                const double r0 = now();
                w.region(r).step();
                regionSum[r] += (now() - r0) * 1000;
            }
        if (t % 3 == 0) {
            const double s0 = now();
            const Snapshot s = w.snapshot(pilot);
            snap.push_back((now() - s0) * 1000);
        }
    }
    double sum = 0;
    for (double x : step) sum += x;
    std::printf("world %s, %zu regions, %d ticks, one pilot driving\n", load.world->file.id.c_str(), w.regionCount(), ticks);
    std::printf("  step (all regions): mean %.3f ms, p50 %.3f, p95 %.3f, max %.3f\n", sum / double(step.size()), at(step, 0.5),
                at(step, 0.95), at(step, 1.0));
    std::printf("  pilot's snapshot: p50 %.3f ms, max %.3f\n", at(snap, 0.5), at(snap, 1.0));
    std::vector<size_t> slow(step.size());
    for (size_t i = 0; i < slow.size(); i++) slow[i] = i;
    std::sort(slow.begin(), slow.end(), [&](size_t a, size_t b) { return step[a] > step[b]; });
    std::printf("  slowest steps:");
    for (size_t i = 0; i < std::min<size_t>(5, slow.size()); i++) std::printf(" tick %zu %.3f ms%s", slow[i] + 1, step[slow[i]], i < 4 ? "," : "\n");
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](double v) {
        uint64_t b;
        std::memcpy(&b, &v, sizeof b);
        hash = (hash ^ b) * 1099511628211ull;
    };
    for (size_t r = 0; r < w.regionCount(); r++)
        for (const Unit& u : w.region(r).simulation().state.units) {
            mix(u.position.x);
            mix(u.position.y);
            mix(u.heading);
            mix(u.hp);
        }
    std::printf("  state hash %016llx\n", (unsigned long long)hash);
    const double samples = double((ticks + 59) / 60);
    for (size_t r = 0; r < w.regionCount(); r++)
        std::printf("  %-14s %.3f ms a step%s\n", load.world->regions[r].file.id.c_str(), regionSum[r] / samples,
                    w.where(pilot) == r ? " (the pilot's)" : "");
    return 0;
}
