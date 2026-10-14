// The Metallic Hydrogen loop in a world, with only the pilot's Prospector
// (docs/mmorpg-play.md "The loop"): a Citadel built from the wallet, a
// Derrick built on a well of the start region (75 ore), the Prospector
// driven into the finished Derrick with the act key (it pumps a load of MH),
// driven into the Citadel, and the wallet's MH goes up. One isolated scene;
// run from unreal/server (as `make test` does).

#include "test.h"

#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 9;
constexpr double west = 3.141592653589793;

const LoadedWorld& firstWorld() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

Unit* driven(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

int64_t inputTick = 0;

/// Steps the world with the pilot walking forward (`walk`), facing what
/// `heading()` says each tick, the act key pressed on the first tick when
/// `act`, until `done` or `ticks`.
template <class H, class F>
bool run(WorldMap& w, PilotId p, int ticks, H&& heading, F&& done, double walk = 0, bool act = false) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.walk = Vec2(walk, 0);
        in.heading = heading();
        in.act = act && i == 0;
        w.input(p, in);
        w.step();
        if (done()) return true;
    }
    return false;
}

const Structure* own(const Simulation& sim, StructureKind kind) {
    for (const Structure& s : sim.state.structures)
        if (s.kind == kind && s.owner == Pilot::player) return &s;
    return nullptr;
}

} // namespace

TEST(hydrogen_derrick_pumped_and_unloaded_fills_the_wallet) {
    // No KSTR contracts: their MH would land in the wallet this counts.
    WorldParams params;
    params.contracts = false;
    WorldMap w(firstWorld(), params);
    ASSERT_TRUE(w.join(me));
    // The Citadel and Derrick blueprints, at the Derrick's level floor.
    ProspectorProgress g;
    g.xp = Progression::xp(Progression::blueprintFloor(StructureKind::derrick));
    g.blueprints = {StructureKind::citadel, StructureKind::derrick};
    w.setProgress(me, g);
    Simulation& sim = w.regionOf(me)->simulation();
    auto unit = [&] { return driven(w, me); };
    auto face = [&](Vec2 at) {
        return [&w, at] {
            const Unit* u = driven(w, me);
            return std::atan2(at.y - u->position.y, at.x - u->position.x);
        };
    };
    auto still = [&] { return unit()->heading; };
    EXPECT_EQ(w.wallet(me)->ore, int64_t(600));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(0));

    // The Citadel, east of the ore (as the build tests), welded to the end.
    Unit* u = unit();
    u->position = w.regionOf(me)->places()->ground(Vec2(72, 48));
    u->heading = west;
    u->task = Unit::Task::idle;
    const Vec2 home =
        *sim.snap(StructureKind::citadel, u->position + Vec2(std::cos(west), std::sin(west)) * Region::buildReach(StructureKind::citadel));
    auto why = w.build(me, StructureKind::citadel, home);
    if (why) std::printf("  Citadel refused: %s\n", why->c_str());
    ASSERT_TRUE(!why.has_value());
    ASSERT_TRUE(run(w, me, int((Rules::buildTime(StructureKind::citadel) + 5) * 60), still, [&] {
        const Structure* c = own(sim, StructureKind::citadel);
        return c && c->complete();
    }));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(200));

    // Over to the start region's south well (60, 61): the nearest spot the
    // Prospector stands on, the Derrick's reach away, facing the well.
    const Vec2 wellCell = w.regionOf(me)->places()->ground(Vec2(60, 61));
    const Well* well = nullptr;
    for (const Well& x : *sim.state.wells)
        if (distance(x.position, wellCell) < 1.0) well = &x;
    ASSERT_TRUE(well != nullptr);
    const Vec2 wellAt = well->position;
    const int64_t wellBefore = well->remaining;
    bool stood = false;
    for (int k = 0; k < 16 && !stood; k++) {
        const double a = k * 2 * 3.141592653589793 / 16;
        const Vec2 p = wellAt - Vec2(std::cos(a), std::sin(a)) * Region::buildReach(StructureKind::derrick);
        if (sim.pilotCanStand(*u, p)) {
            u->position = p;
            u->heading = a;
            stood = true;
        }
    }
    ASSERT_TRUE(stood);
    // The game holds the Derrick out on the well (`snap`); the server builds it.
    const auto snapped = sim.snap(StructureKind::derrick, u->position + Vec2(std::cos(u->heading), std::sin(u->heading)) * 3.0);
    ASSERT_TRUE(snapped.has_value());
    EXPECT_TRUE(distance(*snapped, wellAt) < 1e-9);
    EXPECT_EQ(sim.buildCost(StructureKind::derrick, Pilot::player).ore, int64_t(75));
    why = w.build(me, StructureKind::derrick, *snapped);
    if (why) std::printf("  Derrick refused: %s\n", why->c_str());
    ASSERT_TRUE(!why.has_value());
    EXPECT_EQ(w.wallet(me)->ore, int64_t(125));
    ASSERT_TRUE(run(w, me, int((Rules::buildTime(StructureKind::derrick) + 5) * 60), still, [&] {
        const Structure* d = own(sim, StructureKind::derrick);
        return d && d->complete();
    }));
    const int64_t derrick = own(sim, StructureKind::derrick)->id;

    // Up to the Derrick until the act key would go in; then press it.
    ASSERT_TRUE(run(w, me, 60 * 5, face(wellAt), [&] {
        const auto t = sim.pilotTarget(*unit());
        return t && t->is<PilotTarget::Derrick>();
    }, 0.6));
    const auto t = sim.pilotTarget(*unit());
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->as<PilotTarget::Derrick>()->_0, derrick);
    EXPECT_TRUE(!t->as<PilotTarget::Derrick>()->busy);
    ASSERT_TRUE(run(w, me, 2, face(wellAt), [&] { return unit()->task == Unit::Task::inDerrick; }, 0, true));
    // Pumping: a load of MH once the Derrick's time is up.
    ASSERT_TRUE(run(w, me, int((Rules::hydrogenTime + 2) * 60), face(wellAt), [&] { return unit()->carrying > 0; }));
    const int64_t load = unit()->carrying;
    EXPECT_TRUE(unit()->hydrogen == std::optional<bool>(true));
    EXPECT_TRUE(load > 0);
    EXPECT_EQ(sim.state.wells->at(size_t(well - sim.state.wells->data())).remaining, wellBefore - load);
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(0));

    // Driven to the Citadel: the load goes into the wallet's MH, not its
    // ore, less the realm's tax (C1).
    const Vec2 citadelAt = own(sim, StructureKind::citadel)->position;
    ASSERT_TRUE(run(w, me, 60 * 20, face(citadelAt), [&] { return unit()->carrying == 0; }, 1.0));
    EXPECT_EQ(w.wallet(me)->hydrogen, load - Progression::realmTax(load));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(125));
    std::printf("  pumped %lld MH, wallet %lld ore %lld MH\n", (long long)load, (long long)w.wallet(me)->ore,
                (long long)w.wallet(me)->hydrogen);
}
