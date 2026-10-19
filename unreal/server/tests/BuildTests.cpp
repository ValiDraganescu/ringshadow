// Building anywhere in a world (WorldMap::build, Region::build): the driven
// Prospector puts a building up where it fits, paid from the pilot's wallet;
// refusals say why; ore mined and unloaded at the pilot's own Citadel goes
// into the wallet; and a client's Build request over the protocol. Isolated
// scenes on the start region; run from unreal/server (as `make test` does).

#include "test.h"

#include "CitadelSpot.h"

#include "HeadlessClient.h"
#include "Server.h"
#include "WorldMap.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 7;
constexpr double west = 3.141592653589793;

const LoadedWorld& firstWorld() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint for a new pilot (`--dev-blueprints`): these scenes are
/// about building, not finding blueprints (SitesTests.cpp gates them).
WorldParams blueprinted() {
    WorldParams p;
    p.devBlueprints = true;
    return p;
}

Unit* driven(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// The driven unit at a region cell, facing `heading`, standing.
void put(WorldMap& w, PilotId p, Vec2 cell, double heading) {
    Unit* u = driven(w, p);
    u->position = w.regionOf(p)->places()->ground(cell);
    u->heading = heading;
    u->task = Unit::Task::idle;
}

/// Facing west on the open ground east of the ore, where the 12x12 Citadel
/// (C1) places ahead of it, nearest the old spot (72, 48).
void putHome(WorldMap& w, PilotId p) {
    Unit* u = driven(w, p);
    u->position = rig::citadelStand(w, p, w.regionOf(p)->places()->ground(Vec2(72, 48)));
    u->heading = west;
    u->task = Unit::Task::idle;
}

/// Where the game holds a `kind` out ahead of the driven unit (AcPilotBuild's `Spot`).
Vec2 spot(WorldMap& w, PilotId p, StructureKind kind) {
    const Unit* u = driven(w, p);
    const Vec2 ahead(std::cos(u->heading), std::sin(u->heading));
    return *w.regionOf(p)->simulation().snap(kind, u->position + ahead * Region::buildReach(kind));
}

int64_t inputTick = 0;

/// Steps with the walk, the heading and the held key, until `done` or `ticks`.
template <class F> bool run(WorldMap& w, PilotId p, int ticks, F&& done, Vec2 walk = Vec2(0, 0), bool hold = false) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.walk = walk;
        in.heading = driven(w, p)->heading;
        in.hold = hold;
        in.act = hold && i == 0;
        w.input(p, in);
        w.step();
        if (done()) return true;
    }
    return false;
}

const Structure* citadelOf(const Simulation& sim) {
    for (const Structure& s : sim.state.structures)
        if (s.kind == StructureKind::citadel) return &s;
    return nullptr;
}

} // namespace

TEST(build_a_citadel_paid_from_the_wallet) {
    WorldMap w(firstWorld(), blueprinted());
    ASSERT_TRUE(w.join(me));
    // On the open ground east of the ore, facing west.
    putHome(w, me);
    const Vec2 at = spot(w, me, StructureKind::citadel);
    const Simulation& sim = w.regionOf(me)->simulation();
    const int64_t price = sim.buildCost(StructureKind::citadel, Pilot::player).ore;
    EXPECT_EQ(price, int64_t(400));
    const auto why = w.build(me, StructureKind::citadel, at);
    EXPECT_TRUE(!why);
    if (why) std::printf("  refused: %s\n", why->c_str());
    EXPECT_EQ(w.wallet(me)->ore, int64_t(600) - price);
    const Structure* c = citadelOf(sim);
    ASSERT_TRUE(c != nullptr);
    EXPECT_TRUE(!c->complete());
    EXPECT_EQ(c->owner, Pilot::player);
    EXPECT_TRUE(distance(c->position, at) < 1e-9);
    EXPECT_TRUE(driven(w, me)->task == Unit::Task::building);
    // Standing still, it welds it to the end; the region carries on (no victory).
    const int64_t game = w.regionOf(me)->game();
    EXPECT_TRUE(run(w, me, int((Rules::buildTime(StructureKind::citadel) + 5) * 60), [&] {
        const Structure* s = citadelOf(sim);
        return s && s->complete();
    }));
    EXPECT_EQ(w.regionOf(me)->game(), game);
    EXPECT_EQ(w.wallet(me)->ore, int64_t(200));
}

TEST(build_refused_when_too_poor) {
    WorldMap w(firstWorld(), blueprinted());
    ASSERT_TRUE(w.join(me));
    put(w, me, Vec2(72, 48), west);
    w.regionOf(me)->simulation().state.players[0].ore = 399;
    const auto why = w.build(me, StructureKind::citadel, spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Not enough ore");
    EXPECT_TRUE(w.regionOf(me)->simulation().state.structures.empty());
    EXPECT_EQ(w.regionOf(me)->simulation().state.players[0].ore, int64_t(399));
}

TEST(build_refused_on_a_blocked_spot) {
    WorldMap w(firstWorld(), blueprinted());
    ASSERT_TRUE(w.join(me));
    const Simulation& sim = w.regionOf(me)->simulation();
    // Facing the west vein (E1): on its mounds.
    put(w, me, Vec2(58.5, 50), west);
    auto why = w.build(me, StructureKind::citadel, spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    // A Citadel by the ore: too close to it (C1: the 12x12's band, r + 3 from
    // the ore, is x 71 to 72.5 here; on the mounds below 71, placed from 73).
    put(w, me, Vec2(71.5, 50), west);
    why = w.build(me, StructureKind::citadel, spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    if (*why != "Too close to resources") std::printf("  refused: %s\n", why->c_str());
    EXPECT_TRUE(*why == "Too close to resources");
    // Off the cliff of the north-east shelf.
    put(w, me, Vec2(88, 20), 0);
    why = w.build(me, StructureKind::citadel, spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    // Far from the Prospector, though the ground is fine.
    put(w, me, Vec2(72, 48), west);
    why = w.build(me, StructureKind::citadel, w.regionOf(me)->places()->ground(Vec2(64, 70)));
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Too far away");
    EXPECT_TRUE(sim.state.structures.empty());
    EXPECT_EQ(w.wallet(me)->ore, int64_t(600));
    // Not driving: nothing to build with (and no unit to hold a spot out
    // ahead of: a fine one by the pad).
    w.regionOf(me)->handBack(me);
    why = w.build(me, StructureKind::citadel, w.regionOf(me)->places()->ground(Vec2(64, 48)));
    EXPECT_TRUE(why.has_value());
}

// A world builds from the pilot's blueprints (docs/prospector-progression.md,
// "Buildings in the MMO"): a new pilot has none, so every kind is refused
// with the reason, where it would fit and with the ore to pay; with the
// Citadel and Derrick blueprints at level 3 the two are put up.
TEST(build_world_from_blueprints_only) {
    WorldMap w(firstWorld());
    ASSERT_TRUE(w.join(me));
    Simulation& sim = w.regionOf(me)->simulation();
    sim.state.players[0].ore = 5000;
    sim.state.players[0].hydrogen = 5000;
    putHome(w, me);
    for (const StructureKind k : PilotBuild::kinds) {
        const auto why = w.build(me, k, spot(w, me, StructureKind::citadel));
        ASSERT_TRUE(why.has_value());
        EXPECT_TRUE(why->find("blueprint") != std::string::npos);
    }
    EXPECT_TRUE(sim.state.structures.empty());
    EXPECT_EQ(sim.state.players[0].ore, int64_t(5000));
    ProspectorProgress g;
    g.xp = Progression::xp(3);
    g.learned = {StructureKind::citadel, StructureKind::derrick};
    w.setProgress(me, g);
    // A Citadel where it fits.
    EXPECT_TRUE(!w.build(me, StructureKind::citadel, spot(w, me, StructureKind::citadel)).has_value());
    ASSERT_TRUE(sim.state.structures.size() == 1);
    // A Derrick on the start region's nearest well, from beside it.
    ASSERT_TRUE(sim.state.wells && !sim.state.wells->empty());
    const Vec2 here = driven(w, me)->position;
    const Well* well = &sim.state.wells->front();
    for (const Well& x : *sim.state.wells)
        if (distance(x.position, here) < distance(well->position, here)) well = &x;
    const Vec2 wellAt = well->position;
    Unit* u = driven(w, me);
    u->task = Unit::Task::idle;
    double h = 0;
    for (int k = 0; k < 16; k++) {
        const double a = k * 2 * 3.141592653589793 / 16;
        const Vec2 p = wellAt - Vec2(std::cos(a), std::sin(a)) * Region::buildReach(StructureKind::derrick);
        if (sim.pilotCanStand(*u, p)) {
            h = a;
            u->position = p;
            break;
        }
    }
    u->heading = h;
    const auto why = w.build(me, StructureKind::derrick, wellAt);
    if (why) std::printf("  Derrick refused: %s\n", why->c_str());
    EXPECT_TRUE(!why.has_value());
    EXPECT_EQ(sim.state.structures.size(), size_t(2));
}

// Nothing stands on a pad's plate (`region::touchesPadPlate`, the disc
// `padReach` round the pad): a Citadel whose footprint touches it is refused
// with the reason the client's ghost shows; one just outside goes up.
TEST(build_refused_touching_a_pad_plate) {
    // The rule: a square footprint against the disc, corners included
    // (`padReach` since C5 the Landing Pad's half side).
    const std::vector<region::Place> pads{region::Place{"p", Vec2(0, 0)}};
    const double R = region::padReach;
    EXPECT_TRUE(region::touchesPadPlate(pads, Vec2(R + 2.4, 0), 2.5));
    EXPECT_TRUE(!region::touchesPadPlate(pads, Vec2(R + 2.5, 0), 2.5));
    // Diagonally the footprint's corner counts: its corner 0.2 inside the
    // disc touches, 0.2 outside is clear.
    const double in = (R - 0.2) / std::sqrt(2.0) + 2.5, out = (R + 0.2) / std::sqrt(2.0) + 2.5;
    EXPECT_TRUE(region::touchesPadPlate(pads, Vec2(in, in), 2.5));
    EXPECT_TRUE(!region::touchesPadPlate(pads, Vec2(out, out), 2.5));

    WorldMap w(firstWorld(), blueprinted());
    ASSERT_TRUE(w.join(me));
    Simulation& sim = w.regionOf(me)->simulation();
    const Vec2 pad = w.regionOf(me)->places()->ground(Vec2(76, 48));
    ASSERT_TRUE(!firstWorld().regions.empty());
    // On the start pad, facing east (away from the ore).
    put(w, me, Vec2(76, 48), 0);
    // The nearest snapped spot east that touches the plate, and the first clear one.
    std::optional<Vec2> touching, clear;
    // (C1: the Citadel is 12x12, so the spots run farther out.)
    for (double d = 4.0; d < region::padReach + 12; d += 0.25) {
        const Vec2 at = *sim.snap(StructureKind::citadel, pad + Vec2(d, 0));
        const double edge = std::abs(at.x - pad.x) - Rules::radius(StructureKind::citadel);
        if (edge < region::padReach) touching = at;
        else if (!clear) clear = at;
    }
    ASSERT_TRUE(touching.has_value() && clear.has_value());
    EXPECT_TRUE(std::abs(clear->x - touching->x) <= 1.0 + 1e-9);
    auto why = w.build(me, StructureKind::citadel, *touching);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == region::padPlateRefusal);
    if (*why != region::padPlateRefusal) std::printf("  refused: %s\n", why->c_str());
    EXPECT_TRUE(sim.state.structures.empty());
    EXPECT_EQ(w.wallet(me)->ore, int64_t(600));
    // Just outside the plate it goes up.
    why = w.build(me, StructureKind::citadel, *clear);
    if (why) std::printf("  refused: %s\n", why->c_str());
    EXPECT_TRUE(!why.has_value());
    EXPECT_EQ(sim.state.structures.size(), size_t(1));
}

TEST(build_mined_ore_unloaded_at_the_own_citadel_fills_the_wallet) {
    WorldMap w(firstWorld(), blueprinted());
    ASSERT_TRUE(w.join(me));
    Simulation& sim = w.regionOf(me)->simulation();
    const Vec2 home = *sim.snap(StructureKind::citadel, w.regionOf(me)->places()->ground(Vec2(66, 48)));
    ASSERT_TRUE(!sim.placementRefusal(StructureKind::citadel, home));
    sim.placeStructure(StructureKind::citadel, Pilot::player, home);
    w.regionOf(me)->simulation().state.players[0].ore = 100;
    // Drill the west vein's east mound at (55.7, 50.1) with the key held until it
    // carries a load.
    put(w, me, Vec2(57.4, 50.1), west);
    ASSERT_TRUE(run(w, me, 60 * 30, [&] { return driven(w, me)->carrying > 0; }, Vec2(0, 0), true));
    const int64_t load = driven(w, me)->carrying;
    EXPECT_EQ(w.wallet(me)->ore, int64_t(100));
    // Drive east into the Citadel: the load goes into the wallet, less the
    // realm's tax (C1).
    put(w, me, Vec2(66 - Rules::radius(StructureKind::citadel) - 2, 48), 0);
    ASSERT_TRUE(run(w, me, 60 * 5, [&] { return driven(w, me)->carrying == 0; }, Vec2(1, 0)));
    EXPECT_EQ(w.wallet(me)->ore, 100 + load - Progression::realmTax(load));
}

TEST(build_request_over_the_protocol) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = firstWorld();
    o.worldParams.devBlueprints = true;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    auto until = [&](auto&& done, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            server.tick(now);
            client.tick(now);
            if (done()) return true;
        }
        return false;
    };
    client.connect(now);
    client.join(net::newPilotKey());
    ASSERT_TRUE(until([&] { return client.mirror() && client.mirror()->driven(); }, 10));
    WorldMap& w = server.world();
    const PilotId pilot = w.start().pilots().front();
    putHome(w, pilot);
    client.steer.heading = west;
    const Vec2 there = driven(w, pilot)->position;
    ASSERT_TRUE(until([&] { return distance(client.mirror()->unit(*client.mirror()->driven())->position, there) < 0.1; }, 3));
    EXPECT_EQ(client.mirror()->simulation().state.players[0].ore, int64_t(600));
    // Refused first: something in the way (a deposit), the reason comes back.
    const uint32_t refused = client.build(StructureKind::citadel, w.start().places()->ground(Vec2(53, 46)));
    ASSERT_TRUE(until([&] { return client.replies.count(refused) > 0; }, 3));
    EXPECT_TRUE(!client.replies[refused].done);
    EXPECT_TRUE(!client.replies[refused].reason.empty());
    // Then where it fits, ahead of the Prospector.
    const uint32_t asked = client.build(StructureKind::citadel, spot(w, pilot, StructureKind::citadel));
    ASSERT_TRUE(until([&] { return client.replies.count(asked) > 0; }, 3));
    EXPECT_TRUE(client.replies[asked].done);
    // The scaffold and the wallet reach the mirror.
    EXPECT_TRUE(until([&] {
        const GameState& s = client.mirror()->simulation().state;
        return citadelOf(client.mirror()->simulation()) && s.players[0].ore == 200;
    }, 3));
}
