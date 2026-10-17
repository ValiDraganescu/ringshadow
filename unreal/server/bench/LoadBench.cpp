// `loadbench [RUNS] [WORLD]`: the world server's start, timed (docs/perf.md
// "Server"): reading and checking the world's files (`loadWorld`: each
// region built and validated) and making the world (`WorldMap`: each
// region's simulation, its terrain and walking grid), RUNS times (default
// 5). The first run is the server's start: the walking grids' ground is
// built then and kept for the process (`NavGrid::cache`), so the later runs
// are warm. Prints wall and CPU time (all threads) of the first and the
// mean of the rest. Built and run by `make bench` (-O3).

#include "WorldMap.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

double wall() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
double cpu() {
    rusage u{};
    getrusage(RUSAGE_SELF, &u);
    return double(u.ru_utime.tv_sec + u.ru_stime.tv_sec) * 1e3 + double(u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e3;
}

struct Spent {
    std::vector<double> wall, cpu;
    void add(double w, double c) {
        wall.push_back(w);
        cpu.push_back(c);
    }
    /// The mean of all but the first.
    static double warm(const std::vector<double>& v) {
        double s = 0;
        for (size_t i = 1; i < v.size(); i++) s += v[i];
        return v.size() > 1 ? s / double(v.size() - 1) : 0;
    }
    void print(const char* what) const {
        std::printf("  %-10s first: wall %7.1f ms, cpu %7.1f ms; warm mean: wall %6.1f ms, cpu %6.1f ms\n", what, wall[0], cpu[0],
                    warm(wall), warm(cpu));
    }
};

} // namespace

int main(int argc, char** argv) {
    const int runs = argc > 1 ? std::max(1, std::atoi(argv[1])) : 5;
    const std::string path = argc > 2 ? argv[2] : "worlds/first.json";
    Spent load, make;
    size_t regions = 0;
    for (int k = 0; k < runs; k++) {
        const double w0 = wall(), c0 = cpu();
        WorldLoad l = loadWorld(path);
        const double w1 = wall(), c1 = cpu();
        if (!l.world) {
            for (const std::string& p : l.problems) std::fprintf(stderr, "%s\n", p.c_str());
            return 1;
        }
        WorldMap w(*l.world);
        const double w2 = wall(), c2 = cpu();
        load.add(w1 - w0, c1 - c0);
        make.add(w2 - w1, c2 - c1);
        regions = w.regionCount();
    }
    std::printf("world %s, %zu regions, %d runs\n", path.c_str(), regions, runs);
    load.print("loadWorld");
    make.print("WorldMap");
    return 0;
}
