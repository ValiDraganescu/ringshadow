// Leveling online (docs/mmorpg-m3.md, S3; docs/leveling.md): the pilot's
// levels and picks are its own record (`Region::levels`, `WorldMap::levels`),
// in the simulation only while it drives. Work done driving earns XP and
// levels; the two cards on offer reach the client in the snapshot's player
// data; a Pick request applies on the server, an invalid one is refused with
// its reason; the levels go with the pilot across an edge and on a flight.
// Isolated scenes of a few seconds; run from unreal/server.

#include "test.h"

#include "HeadlessClient.h"
#include "Leveling.h"
#include "Rules.h"
#include "Server.h"
#include "WorldMap.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;
using region::Edge;

constexpr PilotId me = 11;
constexpr PilotId other = 12;

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

/// A Prospector record at `level` (its XP exactly there) with `picks`.
PilotRecord record(int64_t level, const std::vector<Perk>& picks) {
    PilotRecord r;
    KindRecord& k = r.kinds[UnitKind::prospector];
    k.xp = double(Leveling::xp(level));
    k.picks = picks;
    return r;
}

std::vector<GameEvent> leveledUps(const std::vector<GameEvent>& events) {
    std::vector<GameEvent> out;
    for (const GameEvent& e : events)
        if (e.is<GameEvent::LeveledUp>()) out.push_back(e);
    return out;
}

int64_t inputTick = 0;

/// Steps with the pilot's walk at `heading` until `done`, at most `ticks`.
template <class F> bool drive(WorldMap& w, PilotId p, double heading, int ticks, F&& done, Vec2 walk = Vec2(1, 0)) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.walk = walk;
        in.heading = heading;
        w.input(p, in);
        w.step();
        if (done()) return true;
    }
    return false;
}

} // namespace

// Driving work: a load dropped off at the pilot's own Citadel earns its XP
// (a tenth of the game's 1 an ore), 60 is level 2, and the level-up and the first pair of cards
// come in the pilot's snapshot.
TEST(levels_driving_work_earns_xp_and_a_level) {
    WorldMap w(first());
    w.join(me);
    Region& r = w.start();
    Simulation& sim = r.simulation();
    const Unit u = *drivenUnit(w, me);
    // A Citadel where one fits round the pad, built with the wallet, then
    // finished by hand.
    std::optional<Vec2> at;
    const double reach = Region::buildReach(StructureKind::citadel);
    // Off the pad's plate (a world builds nothing on it): up to the server's
    // slack past the reach, as the pilot spawns on the pad.
    for (double d = reach; d <= reach + Region::buildSlack && !at; d += 0.5) {
        for (int k = 0; k < 16 && !at; k++) {
            const double h = k * 2 * pi / 16;
            const auto p = sim.snap(StructureKind::citadel, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (p && distance(*p, u.position) <= reach + Region::buildSlack && !sim.placementRefusal(StructureKind::citadel, *p)
                && !region::touchesPadPlate(first().regions[first().start].built.pads, *p, Rules::radius(StructureKind::citadel)))
                at = p;
        }
    }
    ASSERT_TRUE(at.has_value());
    ASSERT_TRUE(!w.build(me, StructureKind::citadel, *at).has_value());
    ASSERT_TRUE(sim.state.structures.size() == 1);
    Structure& citadel = sim.state.structures[0];
    citadel.buildLeft = std::nullopt;
    citadel.builder = std::nullopt;
    citadel.hp = Rules::hp(StructureKind::citadel);
    EXPECT_TRUE(!w.levels(me) || w.levels(me)->xp(UnitKind::prospector) == 0);
    // At its edge with a load of 600 ore: 60 XP at a world's tenth of the
    // game's rate (`kWorldXpScale`).
    Unit* v = drivenUnit(w, me);
    v->position = citadel.position + normalize(v->position - citadel.position) * (Rules::radius(StructureKind::citadel) + 0.4);
    v->carrying = 600;
    v->hydrogen = false;
    std::vector<GameEvent> told;
    for (int i = 0; i < 10; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.heading = v->heading;
        w.input(me, in);
        w.step();
    }
    const Snapshot s = w.snapshot(me);
    EXPECT_EQ(drivenUnit(w, me)->carrying, int64_t(0));
    ASSERT_TRUE(w.levels(me).has_value());
    EXPECT_NEAR(w.levels(me)->xp(UnitKind::prospector), 60.0, 1e-9);
    EXPECT_EQ(w.levels(me)->level(UnitKind::prospector), int64_t(2));
    EXPECT_TRUE((leveledUps(s.events) == std::vector<GameEvent>{GameEvent::LeveledUp{UnitKind::prospector, 2}}));
    // The cards on offer come with the snapshot's player data.
    ASSERT_TRUE(!s.players.empty() && s.players[0].pilot.has_value());
    EXPECT_TRUE(*s.players[0].pilot == *w.levels(me));
    const auto offers = s.players[0].pilot->pending(UnitKind::prospector);
    ASSERT_TRUE(offers.size() == 1);
    EXPECT_EQ(offers[0].level, int64_t(2));
    EXPECT_TRUE((offers[0].perks() == std::vector<Perk>{Perk::prospectorQuickDrill, Perk::prospectorLightFrame}));
}

// Each pilot's levels are its own: a second pilot in the region (seated, not
// driving) has none, hears nothing of the driver's level-up, and has no
// pick to make; the driver's record is not in its snapshot.
TEST(levels_are_each_pilots_own) {
    WorldMap w(first());
    w.join(me);
    w.join(other);
    ASSERT_TRUE(w.start().driving(me).has_value());
    ASSERT_TRUE(!w.start().driving(other).has_value());
    const Unit u = *drivenUnit(w, me);
    w.start().simulation().earn(70 / kWorldXpScale, u);
    w.step();
    EXPECT_EQ(w.levels(me)->level(UnitKind::prospector), int64_t(2));
    EXPECT_TRUE(!w.levels(other).has_value());
    const Snapshot mine = w.snapshot(me), theirs = w.snapshot(other);
    EXPECT_EQ(leveledUps(mine.events).size(), size_t(1));
    EXPECT_EQ(leveledUps(theirs.events).size(), size_t(0));
    EXPECT_TRUE(mine.players[0].pilot.has_value());
    EXPECT_TRUE(!theirs.players[0].pilot.has_value());
    EXPECT_TRUE(w.pick(other, UnitKind::prospector, Perk::prospectorQuickDrill) == std::optional<std::string>("No pick waiting"));
    // The seated pilot's own record, set (a save), stays its own and can pick.
    w.setLevels(other, record(2, {}));
    EXPECT_TRUE(!w.pick(other, UnitKind::prospector, Perk::prospectorLightFrame).has_value());
    EXPECT_TRUE((w.levels(other)->picks(UnitKind::prospector) == std::vector<Perk>{Perk::prospectorLightFrame}));
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector).empty());
    EXPECT_TRUE(w.start().simulation().levels() == *w.levels(me));
    // Driving a Prospector of its own later brings its record into the
    // simulation: the driver hands back first.
    w.start().handBack(me);
    EXPECT_TRUE(w.start().simulation().levels().kinds.empty());
    EXPECT_EQ(w.levels(me)->level(UnitKind::prospector), int64_t(2));
    int64_t theirUnit = -1;
    for (const Unit& x : w.start().simulation().state.units)
        if (x.id != u.id) theirUnit = x.id;
    ASSERT_TRUE(w.start().take(other, theirUnit));
    EXPECT_TRUE((w.start().simulation().levels().picks(UnitKind::prospector) == std::vector<Perk>{Perk::prospectorLightFrame}));
    EXPECT_TRUE(w.start().simulation().active(Perk::prospectorLightFrame));
}

// Over the network: a pilot staged at level 4 (`devXp`, as the dev XP cheat)
// sees the cards in its mirror, picks through Pick requests, the server
// refuses what is not on offer with its reason, and Big haul shows: the
// driven Prospector's load is 35 ore.
TEST(levels_pick_requests_apply_on_the_server) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = first();
    o.worldParams.devXp = double(Leveling::xp(4));
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
    auto mirrored = [&]() -> PilotRecord {
        return client.mirror() ? client.mirror()->simulation().levels() : PilotRecord{};
    };
    client.join(net::newPilotKey());
    client.connect(now);
    ASSERT_TRUE(until([&] { return client.mirror() && client.mirror()->driven() && mirrored().level(UnitKind::prospector) == 4; }, 10));
    const PilotId pilot = server.world().start().pilots().front();
    const auto offers = mirrored().pending(UnitKind::prospector);
    ASSERT_TRUE(offers.size() == 3);
    // The card to pick now is level 2's, under a level 4 badge.
    EXPECT_EQ(offers[0].level, int64_t(2));
    EXPECT_EQ(offers[2].level, int64_t(4));
    EXPECT_TRUE((offers[0].perks() == std::vector<Perk>{Perk::prospectorQuickDrill, Perk::prospectorLightFrame}));
    EXPECT_TRUE((offers[2].perks() == std::vector<Perk>{Perk::prospectorBigHaul, Perk::prospectorFastWelder}));

    auto ask = [&](UnitKind kind, Perk perk) -> net::ReplyMessage {
        const uint32_t id = client.pick(kind, perk);
        if (!until([&] { return client.replies.count(id) > 0; }, 3)) return net::ReplyMessage{id, false, "no reply"};
        return client.replies[id];
    };
    // Refused: level 4's card while level 2's waits, another kind's card,
    // a card named for the wrong kind.
    net::ReplyMessage r = ask(UnitKind::prospector, Perk::prospectorBigHaul);
    EXPECT_TRUE(!r.done);
    EXPECT_TRUE(r.reason == "Not on offer");
    r = ask(UnitKind::ranger, Perk::rangerDrillSergeant);
    EXPECT_TRUE(!r.done);
    EXPECT_TRUE(r.reason == "No pick waiting");
    r = ask(UnitKind::ranger, Perk::prospectorQuickDrill);
    EXPECT_TRUE(!r.done);
    EXPECT_TRUE(r.reason == "Not a card of that kind");
    EXPECT_TRUE(server.world().levels(pilot)->picks(UnitKind::prospector).empty());
    // Kept, lowest level first.
    EXPECT_TRUE(ask(UnitKind::prospector, Perk::prospectorQuickDrill).done);
    EXPECT_TRUE(ask(UnitKind::prospector, Perk::prospectorRigMaster).done);
    const Unit before = *drivenUnit(server.world(), pilot);
    EXPECT_EQ(server.world().start().simulation().load(before, false), int64_t(25));
    EXPECT_TRUE(ask(UnitKind::prospector, Perk::prospectorBigHaul).done);
    EXPECT_EQ(server.world().start().simulation().load(*drivenUnit(server.world(), pilot), false), int64_t(35));
    const std::vector<Perk> three{Perk::prospectorQuickDrill, Perk::prospectorRigMaster, Perk::prospectorBigHaul};
    EXPECT_TRUE(server.world().levels(pilot)->picks(UnitKind::prospector) == three);
    r = ask(UnitKind::prospector, Perk::prospectorFastWelder);
    EXPECT_TRUE(!r.done);
    EXPECT_TRUE(r.reason == "No pick waiting");
    // And the mirror has them.
    EXPECT_TRUE(until([&] { return mirrored().picks(UnitKind::prospector) == three; }, 3));
    EXPECT_TRUE(mirrored().pending(UnitKind::prospector).empty());
    EXPECT_EQ(client.mirror()->simulation().load(*client.mirror()->unit(*client.mirror()->driven()), false), int64_t(35));
}

// The levels go with the pilot: across an edge into the neighbour, and on a
// flight back to the start pad. The region left keeps none; the picks act
// where the pilot drives now.
TEST(levels_kept_through_an_edge_and_a_flight) {
    WorldMap w(first());
    w.join(me);
    const std::vector<Perk> picks{Perk::prospectorQuickDrill, Perk::prospectorRigMaster, Perk::prospectorBigHaul};
    w.setLevels(me, record(5, picks));
    EXPECT_EQ(w.start().simulation().levels().level(UnitKind::prospector), int64_t(5));
    EXPECT_EQ(w.start().simulation().load(*drivenUnit(w, me), false), int64_t(35));
    // East, out of the opening.
    const Vec2 ext = w.start().places()->extent();
    const auto& span = w.start().places()->openings[size_t(Edge::east)][0];
    Unit* u = drivenUnit(w, me);
    u->position = w.start().places()->ground(Vec2(ext.x - 4, (span.from + span.to) / 2));
    u->heading = 0;
    ASSERT_TRUE(drive(w, me, 0, 180, [&] { return w.visit(me) == 1; }));
    ASSERT_TRUE(w.where(me) == w.regionNamed("tern-ridge"));
    Region& there = *w.regionOf(me);
    EXPECT_TRUE(!w.start().simulation().state.players[0].pilot.has_value());
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector) == picks);
    EXPECT_EQ(w.levels(me)->level(UnitKind::prospector), int64_t(5));
    EXPECT_TRUE(there.simulation().levels() == *w.levels(me));
    EXPECT_EQ(there.simulation().load(*drivenUnit(w, me), false), int64_t(35));
    // Level 5's card, picked there.
    EXPECT_TRUE(!w.pick(me, UnitKind::prospector, Perk::prospectorGroupRepair).has_value());
    const std::vector<Perk> four{Perk::prospectorQuickDrill, Perk::prospectorRigMaster, Perk::prospectorBigHaul, Perk::prospectorGroupRepair};
    // To the pad there, and a flight to the start's.
    const region::Built& places = *there.places();
    drivenUnit(w, me)->position = places.pads[0].at;
    ASSERT_TRUE(!w.fly(me, first().regions[first().start].file.id, first().regions[first().start].file.pads[first().startPad].name).has_value());
    EXPECT_TRUE(w.flying(me));
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector) == four);
    EXPECT_TRUE(!there.simulation().state.players[0].pilot.has_value());
    for (int i = 0; i < int(std::llround(w.params().flightTime * 60)) + 1 && w.flying(me); i++) w.step();
    ASSERT_TRUE(!w.flying(me));
    ASSERT_TRUE(w.where(me) == first().start);
    ASSERT_TRUE(w.start().driving(me).has_value());
    EXPECT_TRUE(w.levels(me)->picks(UnitKind::prospector) == four);
    EXPECT_TRUE(w.start().simulation().levels() == *w.levels(me));
    EXPECT_EQ(w.start().simulation().load(*drivenUnit(w, me), false), int64_t(35));
    EXPECT_TRUE(!there.simulation().state.players[0].pilot.has_value());
}

// A world's XP is a tenth of the game's (`kWorldXpScale`): welding a whole
// Citadel by hand, which in the game takes a Prospector from level 1 to 4,
// earns a tenth of the Citadel's worth there and leaves it at level 1. The
// RTS game's own simulation keeps the full rate.
TEST(levels_world_xp_is_a_tenth_a_citadel_weld_stays_level_1) {
    WorldMap w(first());
    w.join(me);
    Simulation& sim = w.start().simulation();
    EXPECT_EQ(sim.xpScale, kWorldXpScale);
    const Unit u = *drivenUnit(w, me);
    std::optional<Vec2> at;
    const double reach = Region::buildReach(StructureKind::citadel);
    // Off the pad's plate (a world builds nothing on it): up to the server's
    // slack past the reach, as the pilot spawns on the pad.
    for (double d = reach; d <= reach + Region::buildSlack && !at; d += 0.5) {
        for (int k = 0; k < 16 && !at; k++) {
            const double h = k * 2 * pi / 16;
            const auto p = sim.snap(StructureKind::citadel, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (p && distance(*p, u.position) <= reach + Region::buildSlack && !sim.placementRefusal(StructureKind::citadel, *p)
                && !region::touchesPadPlate(first().regions[first().start].built.pads, *p, Rules::radius(StructureKind::citadel)))
                at = p;
        }
    }
    ASSERT_TRUE(at.has_value());
    ASSERT_TRUE(!w.build(me, StructureKind::citadel, *at).has_value());
    const bool done = drive(w, me, u.heading, int((Rules::buildTime(StructureKind::citadel) + 5) * 60), [&] {
        return !sim.state.structures.empty() && sim.state.structures[0].complete();
    }, Vec2(0, 0));
    ASSERT_TRUE(done);
    const double xp = w.levels(me)->xp(UnitKind::prospector);
    const double full = Leveling::worth(StructureKind::citadel);
    std::printf("  a Citadel welded: %.1f XP (the game's %.0f), level %lld\n", xp, full,
                (long long)w.levels(me)->level(UnitKind::prospector));
    EXPECT_NEAR(xp, full * kWorldXpScale, full * kWorldXpScale * 0.02);
    EXPECT_EQ(w.levels(me)->level(UnitKind::prospector), int64_t(1));
    EXPECT_TRUE(leveledUps(w.snapshot(me).events).empty());
    // In the game's rate, the same work is level 4 (the problem it fixes).
    EXPECT_TRUE(Leveling::level(full) >= 4);
}
