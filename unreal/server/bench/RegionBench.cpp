// `regionbench [SECONDS] [FIXTURE...]`: the region's tick and one pilot's
// snapshot, timed. For each map (a new game on The Open Field, small, medium
// and large, AIs on, fog on), the core bench's late 4v4 (Badlands · Large ·
// 8, staged by core-tests/bench/LateStage.h) and each fixture given (a saved
// session, from ../core-tests/fixtures), one pilot drives a unit of its side forward and
// the region steps SECONDS of game time (default 30) at 60 Hz, building the
// pilot's snapshot every third tick (20 Hz). Prints step and snapshot time
// spreads and the snapshot sizes. Built and run by `make bench` (-O3).

#include "LateStage.h"
#include "Session.h"
#include "World.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::server;

namespace {

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct Spread { double mean = 0, p50 = 0, p95 = 0, max = 0; };

Spread spread(std::vector<double> v) {
    Spread s;
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    double sum = 0;
    for (double x : v) sum += x;
    auto q = [&](double f) { return v[std::min(static_cast<size_t>(double(v.size()) * f), v.size() - 1)]; };
    s.mean = sum / double(v.size());
    s.p50 = q(0.5);
    s.p95 = q(0.95);
    s.max = v.back();
    return s;
}

/// A unit of side 0 to drive: a soldier if there is one, else a Prospector.
std::optional<int64_t> pick(const Simulation& sim) {
    std::optional<int64_t> worker;
    for (const Unit& u : sim.state.units) {
        if (u.owner != Pilot::player || !sim.canDrive(u)) continue;
        if (u.soldier()) return u.id;
        if (!worker) worker = u.id;
    }
    return worker;
}

void run(const std::string& name, const RegionConfig& config, double seconds) {
    Region region(config);
    const PilotId pilot = 1;
    region.join(pilot);
    const std::optional<int64_t> unit = pick(region.simulation());
    if (!unit || !region.take(pilot, *unit)) {
        std::printf("%s: no unit to drive\n", name.c_str());
        return;
    }
    const double heading = region.simulation().pilotUnit()->heading;
    std::vector<double> steps, snaps;
    std::vector<double> entities, removed, events, units;
    const int64_t ticks = static_cast<int64_t>(seconds * 60);
    for (int64_t k = 1; k <= ticks; ++k) {
        PilotInput in;
        in.tick = k;
        // Forward a second, then a turn, so it goes somewhere and comes back.
        in.walk = Vec2(1, 0);
        in.heading = heading + 0.5 * double((k / 60) % 4);
        region.input(pilot, in);
        const double t0 = now();
        region.step();
        steps.push_back((now() - t0) * 1000);
        if (k % 3 == 0) {
            const double t1 = now();
            const Snapshot s = region.snapshot(pilot);
            snaps.push_back((now() - t1) * 1000);
            entities.push_back(double(s.entities.size()));
            removed.push_back(double(s.removed.size()));
            events.push_back(double(s.events.size()));
            units.push_back(double(region.simulation().state.units.size() + region.simulation().state.structures.size()));
        }
        if (!region.driving(pilot) && unit) {
            if (const auto again = pick(region.simulation())) region.take(pilot, *again);
        }
    }
    const Spread st = spread(steps), sn = spread(snaps), en = spread(entities), un = spread(units), ev = spread(events);
    std::printf("%s: %lld ticks (%.0f game s), things in the region mean %.0f (max %.0f)\n", name.c_str(),
                static_cast<long long>(ticks), seconds, un.mean, un.max);
    std::printf("  step      mean %6.3f  p50 %6.3f  p95 %6.3f  max %7.2f ms\n", st.mean, st.p50, st.p95, st.max);
    std::printf("  snapshot  mean %6.3f  p50 %6.3f  p95 %6.3f  max %7.2f ms  (%zu snapshots)\n", sn.mean, sn.p50, sn.p95, sn.max,
                snaps.size());
    std::printf("  entities a snapshot mean %.0f (max %.0f), events mean %.1f (max %.0f)\n", en.mean, en.max, ev.mean, ev.max);
}

} // namespace

int main(int argc, char** argv) {
    const double seconds = argc > 1 ? std::atof(argv[1]) : 30;
    for (MapSize size : {MapSize::small, MapSize::medium, MapSize::large}) {
        RegionConfig c;
        c.map = MapChoice{MapStyle::openField, size, 2};
        run(c.map.name() + ", new game", c, seconds);
    }
    {
        RegionConfig c;
        c.map = MapChoice{MapStyle::badlands, MapSize::large, 8};
        c.state = bench::stageLate(WindowMaps::buildSquare(c.map), {0, 0, 0, 0, 1, 1, 1, 1});
        run(c.map.name() + ", late 4v4 staged", c, seconds);
    }
    for (int i = 2; i < argc; ++i) {
        std::string error;
        const std::optional<Session> session = SessionStore::load(argv[i], &error);
        if (!session) {
            std::fprintf(stderr, "%s: %s\n", argv[i], error.c_str());
            return 1;
        }
        std::optional<MapChoice> choice;
        for (int64_t players : {2, 4, 8})
            for (MapStyle st : allCases<MapStyle>())
                for (MapSize sz : allCases<MapSize>())
                    if (!choice && MapChoice{st, sz, players}.name() == session->mapName) choice = MapChoice{st, sz, players};
        if (!choice) {
            std::fprintf(stderr, "%s: no window map %s\n", argv[i], session->mapName.c_str());
            return 1;
        }
        RegionConfig c;
        c.map = *choice;
        c.state = session->state;
        run(session->mapName + ", " + argv[i], c, seconds);
    }
    return 0;
}
