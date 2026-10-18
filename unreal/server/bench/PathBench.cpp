// `pathbench [SEARCHES]`: the walking grid's searches in a world region,
// timed (docs/perf.md "Server"): Slag Pockets with the Matriarch's nest
// full (its grid with her horde on it), SEARCHES (default 20000) from a
// walkable cell to another 5 to 40 cells off, as the Brood's bugs ask, a
// fixed seed. Prints the mean and 99th percentile of `NavGrid::path`, the
// cells a search closes, and a hash of every path found (equal before and
// after a change that must keep the paths bit for bit). Built and run by
// `make bench` (-O3).

#include "NavGrid.h"
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

} // namespace

int main(int argc, char** argv) {
    const int searches = argc > 1 ? std::max(1, std::atoi(argv[1])) : 20000;
    WorldLoad load = loadWorld("worlds/first.json");
    if (!load.world) {
        for (const std::string& p : load.problems) std::fprintf(stderr, "%s\n", p.c_str());
        return 1;
    }
    WorldParams p;
    p.heldHour = 12;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    WorldMap w(*load.world, p);
    w.join(1);
    const size_t r = *w.nestRegion();
    w.devTransfer(1, r, w.region(r).places()->pads[0].at);
    for (int i = 0; i < 180; i++) w.step();
    const NavGrid& nav = *w.region(r).simulation().nav;
    SeededRandom dice(18102026);
    auto walkable = [&] {
        for (;;) {
            const Vec2 q = nav.origin + Vec2(dice.range(0, double(nav.width)), dice.range(0, double(nav.height))) * NavGrid::cell;
            if (nav.walkable(q)) return q;
        }
    };
    std::vector<double> us;
    us.reserve(size_t(searches));
    uint64_t hash = 1469598103934665603ull;
    auto mix = [&](double v) {
        uint64_t b;
        std::memcpy(&b, &v, sizeof b);
        hash = (hash ^ b) * 1099511628211ull;
    };
    int64_t found = 0;
    const int64_t closed0 = NavGrid::cellsClosed.load(), runs0 = NavGrid::searchesRun.load();
    for (int k = 0; k < searches; k++) {
        const Vec2 from = walkable();
        const double far = dice.range(5, 40), bearing = dice.range(0, 2 * pi);
        const Vec2 to = from + Vec2(std::cos(bearing), std::sin(bearing)) * far;
        const double stopAt = k % 3 == 0 ? 0.1 : 1.5;
        const double t0 = now();
        const auto way = nav.path(from, to, stopAt, k % 11 == 0);
        us.push_back(now() - t0);
        if (!way) {
            mix(-1);
            continue;
        }
        found++;
        for (const Vec2 q : *way) {
            mix(q.x);
            mix(q.y);
        }
    }
    double sum = 0;
    for (double x : us) sum += x;
    const int64_t runs = NavGrid::searchesRun.load() - runs0;
    std::printf("pathbench: %d asked, %lld found, %lld searched: path mean %.2f us, p99 %.2f us, %.0f cells closed a search, paths %016llx\n",
                searches, (long long)found, (long long)runs, sum / double(searches), at(us, 0.99),
                double(NavGrid::cellsClosed.load() - closed0) / double(std::max<int64_t>(1, runs)), (unsigned long long)hash);
    return 0;
}
