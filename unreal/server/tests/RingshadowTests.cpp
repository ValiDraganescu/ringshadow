// The ringshadow sweep's drilling bonus (Ringshadow.h, WorldProgress.cpp;
// docs/weather.md, "Ringshadow sweep"): a load drilled at a deposit under
// the band carries half again, and its unload pays that ore and its XP; the
// same load in the sun carries the plain amount. Isolated scenes of a few
// seconds; run from unreal/server.

#include "test.h"

#include "Ringshadow.h"
#include "Rules.h"
#include "WorldMap.h"
#include "rig.h"

#include <cmath>
#include <cstdio>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 41;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// The pilot drills the start region's first deposit, holding the key, from
/// world time `clock`: the load it comes away with (0: none in 30 s).
int64_t drill(double clock) {
    WorldMap w(first());
    w.join(me);
    w.setClock(clock);
    Simulation& sim = w.start().simulation();
    const OreDeposit& d = sim.state.patches[0];
    Unit* u = drivenUnit(w, me);
    const Vec2 away = Vec2(1, 0);
    u->position = d.position + away * (Rules::radius(UnitKind::prospector) + 1.0);
    u->heading = std::atan2(-away.y, -away.x);
    u->carrying = 0;
    u->hydrogen = std::nullopt;
    int64_t tick = 0;
    for (int i = 0; i < 30 * 20; i++) {
        PilotInput in;
        in.tick = ++tick;
        in.visit = w.visit(me);
        in.heading = drivenUnit(w, me)->heading;
        in.hold = true;
        w.input(me, in);
        w.step();
        if (drivenUnit(w, me)->carrying > 0) break;
    }
    return drivenUnit(w, me)->carrying;
}

} // namespace

TEST(ringshadow_load_drilled_under_the_band_carries_half_again) {
    const LoadedWorld& wd = first();
    WorldMap probe(wd);
    const Simulation& sim = probe.start().simulation();
    ASSERT_TRUE(sim.map.has_value());
    const GroundRect bounds = sim.map->bounds;
    const Vec2 at = sim.state.patches[0].position;
    const int64_t plain = drill(Ringshadow::next(0).start - 200);
    ASSERT_TRUE(plain > 0);
    // Both sweeps of the day, each moving its own way (the ring's arc
    // crosses the sun differently at 11:30 and 16:00).
    double from = 0;
    for (int i = 0; i < 2; ++i) {
        const auto s = Ringshadow::next(from);
        from = s.end;
        EXPECT_EQ(s.index, i);
        // A moment the deposit is deep in the band and stays there 30 s.
        double under = -1;
        for (double t = s.start; t + 30 < s.end && under < 0; t += 0.5)
            if (Ringshadow::cover(t, at, bounds) >= 1.0 && Ringshadow::cover(t + 30, at, bounds) >= 1.0) under = t;
        if (under < 0) {
            // The edges move ~2.3 cells a second: 30 s inside always exists on a medium region.
            for (double t = s.start; t + 20 < s.end && under < 0; t += 0.5)
                if (Ringshadow::cover(t, at, bounds) >= 1.0 && Ringshadow::cover(t + 20, at, bounds) >= 1.0) under = t;
        }
        ASSERT_TRUE(under > 0);
        const int64_t swept = drill(under);
        EXPECT_EQ(swept, int64_t(std::llround(double(plain) * Ringshadow::oreBonus)));
        // After the sweep it is plain again.
        EXPECT_EQ(drill(s.end + 10), plain);
    }
}
