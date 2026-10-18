// The protocol's messages (NetProtocol.h) and snapshots (NetSnapshot.h):
// every message round trips, quantised values stay within their precision,
// and snapshot deltas rebuild exactly what the writer meant.

#include "test.h"

#include "NetMirror.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"
#include "NetTypes.h"

#include "RegionFile.h"
#include "WindowMaps.h"

#include <cmath>
#include <limits>
#include <map>
#include <vector>

namespace {

using namespace ac;
using namespace ac::net;

constexpr double kTurn = 6.283185307179586;

/// A unit with every field set.
Unit fullUnit(int64_t id) {
    Unit u(id, UnitKind::dropship, 1, Vec2(12.3456, -7.891), 3.0, Unit::Task::attacking, 4, 1.25, 5, 17.5);
    u.hp = 120.3;
    u.order = BuildOrder{StructureKind::lab, Vec2(3.5, 4.25)};
    u.structure = 99;
    u.goal = Vec2(40.01, -2.5);
    u.waypoints = std::vector<Vec2>{Vec2(1, 2), Vec2(-3.3, 4.4)};
    u.target = 77;
    u.cooldown = 0.4567;
    u.moving = true;
    u.hydrogen = true;
    u.hitAt = 123.456;
    u.slowUntil = 130.5;
    u.slowedTo = 0.35;
    u.anchor = 0.5;
    u.anchored = false;
    u.aim = -1.0;
    u.jumpFrom = Vec2(5, 6);
    u.jumpTo = Vec2(7, 8);
    u.energy = 87.5;
    u.cargo = std::vector<int64_t>{3, 4, 5};
    u.mission = Mission(Mission::Assault{6, Vec2(9.5, 10.25)});
    u.autoFollow = true;
    u.burstFrom = 100.25;
    u.firedAt = 100.5;
    u.spread = 0.0123;
    u.railCooldown = 1.5;
    u.shotFrom = Vec2(-1, -2);
    u.shield = 30.5;
    u.bonusHP = 12;
    u.boardedAt = Vec2(11, 12);
    u.rush = Rush{3, 0.2, 0.1};
    u.rushUntil = 140;
    u.burning = Burning{150, 4.5, 33};
    u.lockTarget = 88;
    u.lockFrom = 101;
    u.lockAt = 101.5;
    u.stompReady = 160;
    u.stompedAt = 155;
    u.struckAt = 154;
    return u;
}

Structure fullStructure(int64_t id) {
    Structure b(id, StructureKind::garrison, 2, Vec2(-30.25, 41.5), 4.5, 12);
    b.hp = 777.7;
    b.training = 3.25;
    b.line = std::vector<UnitKind>{UnitKind::ranger, UnitKind::juggernaut};
    b.crew = std::vector<int64_t>{5, 6};
    b.addon = 13;
    b.parent = 14;
    b.research = Upgrade::aegisShield;
    b.researchLeft = 20.5;
    b.target = 15;
    b.cooldown = 0.75;
    b.aim = 2.5;
    return b;
}

Player fullPlayer() {
    Player p;
    p.ore = 1234;
    p.hydrogen = 567;
    p.supplyUsed = 33;
    p.supplyCap = 47;
    p.totalMined = 8000;
    p.attack = Vec2(50, 60);
    p.retreating = true;
    p.retreatedAt = 300.25;
    p.defending = true;
    p.reinforce = 6;
    p.waves = 3;
    p.aggression = 0.6789;
    p.greed = 0.25;
    p.garrisonPerBase = 2;
    p.comets = 1;
    p.start = 3;
    p.style = Player::Style::mech;
    p.fireflies = 4;
    p.drops = true;
    p.upgrades = std::set<Upgrade>{Upgrade::minigun, Upgrade::lifelineReactor};
    Directives d;
    Request q;
    q.id = 1;
    q.what = Request::What(Request::What::Unit{UnitKind::longbow});
    q.count = 3;
    q.repeats = true;
    q.at = 40;
    q.site = 2;
    d.queue.push_back(q);
    Request q2;
    q2.id = 2;
    q2.what = Request::What(Request::What::Upgrade{Upgrade::novaIgniters});
    d.queue.push_back(q2);
    d.keepOre = 100;
    d.keepHydrogen = 50;
    d.harvest = Harvest::hydrogen;
    d.objectives.push_back(Objective{4, Objective::Kind::defend, Vec2(1, 1), 9});
    d.stance = Stance::allIn;
    d.nextID = 5;
    p.directives = d;
    PilotRecord rec;
    KindRecord k;
    k.xp = 250.5;
    k.picks = {Perk::rangerDrillSergeant, Perk::rangerFlakVest};
    k.count = 2;
    k.burstUntil = 10;
    k.burstReady = 20;
    k.healedAt = 5;
    k.tally.seconds = 99.5;
    k.tally.kills = 7;
    k.tally.unitDamage = 450.25;
    k.tally.hiddenSeconds = 3;
    k.stamps.push_back(Stamp{2, 50, 40, {Standing{100, 10, 50, 0, 20, 30, 4, 900}}, 55});
    k.stamps.push_back(Stamp{3, 80, 70, {Standing{200, 12, 60, 5, 25, 38, 5, 1500}}, std::nullopt});
    rec.kinds[UnitKind::ranger] = k;
    rec.kinds[UnitKind::comet] = KindRecord{};
    p.pilot = rec;
    p.unitsLost = 9;
    p.team = 0;
    return p;
}

/// Writes `s` with room for everything and reads every message back.
std::vector<SnapshotPart> roundTrip(SnapshotWriter& w, SnapshotReader& r, const Snapshot& s, double now,
                                    int64_t budget = 1 << 20, bool ack = true) {
    std::vector<SnapshotPart> out;
    const SnapshotWriter::Output o = w.write(s, now, budget);
    for (const auto& m : o.messages) {
        auto p = r.read(m);
        if (p) out.push_back(*p);
    }
    if (ack) w.ack(r.ack(), now);
    return out;
}

size_t bytes(const SnapshotWriter::Output& o) {
    size_t n = 0;
    for (const auto& m : o.messages) n += m.size();
    return n;
}

} // namespace

TEST(protocol_welcome_round_trips) {
    Welcome w;
    w.pilot = 0x1234567890ULL;
    w.region.map = MapChoice{MapStyle::badlands, MapSize::large, 8};
    w.region.mapName = "Badlands · Large · 8";
    w.region.mapVersion = 3;
    w.region.seed = 0xDEADBEEFCAFEULL;
    w.region.fog = false;
    w.serverTick = 98765;
    w.time = 1646.0833333;
    Welcome back;
    ASSERT_TRUE(decode(encode(w), back));
    EXPECT_TRUE(back == w);
    InputMessage notIt;
    EXPECT_TRUE(!decode(encode(w), notIt));
}

TEST(protocol_input_round_trips_quantised) {
    InputMessage m;
    for (int i = 0; i < 4; i++) {
        PilotInput in;
        in.tick = 1000 - i;
        // The newest two after a region change (visit 3), the older before.
        in.visit = i < 2 ? 3 : 2;
        in.walk = Vec2(0.5 - 0.3 * i, i % 2 ? -1.0 : 1.0);
        in.heading = 1.234 + i;
        in.act = i == 0;
        in.ability = i == 1;
        in.hold = i == 2;
        in.mend = i == 3;
        if (i == 1) in.crosshair = Pilot::Crosshair(Pilot::Crosshair::On{42});
        if (i == 2) in.crosshair = Pilot::Crosshair(Pilot::Crosshair::Clear{});
        // A slot key: 1-3 (Q, C, G), 0 none.
        in.slot = i;
        m.inputs.push_back(in);
    }
    InputMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    ASSERT_TRUE(back.inputs.size() == 4);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(back.inputs[i] == quantised(m.inputs[i]));
        EXPECT_EQ(back.inputs[i].visit, m.inputs[i].visit);
        EXPECT_EQ(back.inputs[i].slot, int64_t(i));
        EXPECT_NEAR(back.inputs[i].walk.x, m.inputs[i].walk.x, 0.5 / q::kWalkSteps);
        EXPECT_NEAR(std::remainder(back.inputs[i].heading - m.inputs[i].heading, kTurn), 0.0, kTurn / 65536 / 2 + 1e-12);
    }
    // Standing still and full ahead are exact.
    EXPECT_EQ(back.inputs[1].walk.y, -1.0);
    EXPECT_EQ(back.inputs[0].walk.y, 1.0);
    PilotInput zero;
    EXPECT_EQ(quantised(zero).walk.x, 0.0);
    // Out of range and not a number are clamped.
    PilotInput wild;
    wild.walk = Vec2(7, std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(quantised(wild).walk.x, 1.0);
    EXPECT_EQ(quantised(wild).walk.y, 0.0);
}

TEST(protocol_ack_request_reply_round_trip) {
    AckMessage a;
    a.received = {{500, 0x3}, {499, 0x1}, {497, 0xFFFF}};
    AckMessage ab;
    ASSERT_TRUE(decode(encode(a), ab));
    EXPECT_TRUE(ab == a);

    RequestMessage t;
    t.id = 9;
    t.what = RequestMessage::Take{12345};
    RequestMessage tb;
    ASSERT_TRUE(decode(encode(t), tb));
    EXPECT_TRUE(tb == t);
    RequestMessage h;
    h.id = 10;
    h.what = RequestMessage::HandBack{};
    RequestMessage hb;
    ASSERT_TRUE(decode(encode(h), hb));
    EXPECT_TRUE(hb == h);

    ReplyMessage r;
    r.request = 9;
    r.done = false;
    r.reason = "another pilot drives";
    ReplyMessage rb;
    ASSERT_TRUE(decode(encode(r), rb));
    EXPECT_TRUE(rb == r);
    EXPECT_TRUE(messageType(encode(r)) == MessageType::reply);
    EXPECT_TRUE(!messageType(std::vector<uint8_t>{99}).has_value());
}

TEST(protocol_join_round_trips_and_the_key_in_hex) {
    JoinMessage j;
    for (size_t i = 0; i < j.pilot.size(); i++) j.pilot[i] = uint8_t(0xa0 + i * 7);
    JoinMessage jb;
    ASSERT_TRUE(decode(encode(j), jb));
    EXPECT_TRUE(jb == j);
    EXPECT_TRUE(messageType(encode(j)) == MessageType::join);
    // Cut short: refused.
    std::vector<uint8_t> cut = encode(j);
    cut.pop_back();
    EXPECT_TRUE(!decode(cut, jb));
    const std::string hex = pilotKeyHex(j.pilot);
    EXPECT_EQ(hex.size(), size_t(32));
    EXPECT_TRUE(hex.substr(0, 4) == "a0a7");
    EXPECT_TRUE(pilotKeyShort(j.pilot) == hex.substr(0, 8));
    EXPECT_TRUE(pilotKeyFromHex(hex) == j.pilot);
    EXPECT_TRUE(!pilotKeyFromHex("a0a7").has_value());
    EXPECT_TRUE(!pilotKeyFromHex(std::string(32, 'g')).has_value());
    EXPECT_TRUE(newPilotKey() != newPilotKey());
}

TEST(protocol_events_round_trip_every_kind) {
    using G = GameEvent;
    const Vec2 at(10.25, -20.5);
    std::vector<GameEvent> all = {
        G(G::Deposited{1, 25, at, 3, true}),
        G(G::PatchDepleted{2}),
        G(G::PatchRegrown{3}),
        G(G::Trained{4, UnitKind::kestrel, at}),
        G(G::ConstructionStarted{5, StructureKind::sentinel, at}),
        G(G::Constructed{6, StructureKind::lab, at}),
        G(G::Shot{7, 8, 1, at}),
        G(G::Missed{9, at, q::angle(q::angle(0.5)), q::angle(q::angle(0.25))}),
        G(G::Landed{10, 11}),
        G(G::Died{12, UnitKind::scorpion, 1, at}),
        G(G::Destroyed{13, StructureKind::citadel, 0, at}),
        G(G::Victory{1}),
        G(G::Researched{Upgrade::lifelineReactor, 1, at}),
        G(G::Stuck{14, UnitKind::atlas, 0, at}),
        G(G::Trapped{15, UnitKind::peregrine, 1, at}),
        G(G::LeveledUp{UnitKind::ranger, 4}),
        G(G::Blast{16, at, 2.5}),
        G(G::Stomp{17, at, 3.25}),
    };
    EXPECT_EQ(all.size(), std::variant_size_v<GameEvent::Value>);
    const auto messages = encodeEvents(77, all);
    ASSERT_TRUE(messages.size() == 1);
    EventMessage m;
    ASSERT_TRUE(decode(messages[0], m));
    EXPECT_EQ(m.serverTick, int64_t(77));
    ASSERT_TRUE(m.events.size() == all.size());
    for (size_t i = 0; i < all.size(); i++) EXPECT_TRUE(m.events[i] == all[i]);

    // Many events split over messages, each within a reliable message.
    std::vector<GameEvent> many(500, G(G::Shot{123456, 654321, 1, Vec2(100.5, -200.25)}));
    const auto split = encodeEvents(5, many);
    EXPECT_TRUE(split.size() > 1);
    size_t got = 0;
    for (const auto& s : split) {
        EXPECT_TRUE(s.size() <= kMaxReliableBytes);
        EventMessage part;
        ASSERT_TRUE(decode(s, part));
        got += part.events.size();
    }
    EXPECT_EQ(got, many.size());
}

TEST(protocol_quantisation_within_bounds) {
    const Unit u = fullUnit(5);
    const Unit q = quantised(u);
    EXPECT_NEAR(q.position.x, u.position.x, 0.005);
    EXPECT_NEAR(q.position.y, u.position.y, 0.005);
    EXPECT_NEAR(std::remainder(q.heading - u.heading, kTurn), 0.0, kTurn / 65536 / 2 + 1e-12);
    EXPECT_NEAR(q.hp, u.hp, 2 * Rules::hp(u.kind) / 4095 / 2 + 1e-9);
    EXPECT_NEAR(*q.cooldown, *u.cooldown, 0.0005);
    EXPECT_NEAR(*q.spread, *u.spread, 0.00005);
    // Quantising again changes nothing.
    EXPECT_TRUE(quantised(q) == q);
    // Past its range, hit points go exactly.
    Unit big = u;
    big.hp = 10 * Rules::hp(u.kind) + 0.123;
    EXPECT_EQ(quantised(big).hp, big.hp);
    const Structure b = fullStructure(6);
    EXPECT_TRUE(quantised(quantised(b)) == quantised(b));
    EXPECT_NEAR(quantised(b).hp, b.hp, 2 * Rules::hp(b.kind) / 4095 / 2 + 1e-9);
    const Player p = fullPlayer();
    EXPECT_TRUE(quantised(quantised(p)) == quantised(p));
    EXPECT_NEAR(quantised(p).aggression, p.aggression, 0.00005);
    EXPECT_TRUE(quantised(p).pilot->kinds.at(UnitKind::ranger).stamps == p.pilot->kinds.at(UnitKind::ranger).stamps);
    EXPECT_EQ(quantisedTime(-std::numeric_limits<double>::infinity()), -std::numeric_limits<double>::infinity());
}

TEST(snapshot_round_trips_every_item) {
    Snapshot s;
    s.serverTick = 600;
    s.lastInputTick = 590;
    s.game = 2;
    s.time = 10.5;
    s.driven = 5;
    s.winner = 1;
    s.endedAt = 9.25;
    s.score = {3, 4};
    s.entities.push_back(EntityState{fullUnit(5), false, Rank::driven});
    s.entities.push_back(EntityState{fullStructure(6), true});
    s.entities.push_back(EntityState{fullStructure(7), false});
    s.removed = {40, 41};
    s.players = {fullPlayer(), Player{}};
    OreDeposit patch;
    patch.id = 3;
    patch.position = Vec2(5.5, 6.5);
    patch.angle = 1;
    patch.initial = 1500;
    patch.remaining = 1200;
    patch.depletedAt = 3;
    patch.miner = 8;
    s.patches.push_back(PatchView{3, patch});
    s.wells.push_back(WellView{1, Well{Vec2(1, 2), 900, 4}});
    FogView fog;
    const size_t cells = 5000;
    fog.seen.resize(cells);
    fog.near.resize(cells);
    fog.explored.assign((cells + 63) / 64, 0);
    for (size_t i = 0; i < cells; i++) {
        fog.seen[i] = (i / 37) % 3 == 0;
        fog.near[i] = (i / 41) % 2 == 0;
        if ((i / 53) % 2) fog.explored[i / 64] |= uint64_t(1) << (i % 64);
    }
    fog.sites = {-std::numeric_limits<double>::infinity(), 3.25, 7.5};
    s.fog = std::make_shared<const FogView>(fog);

    SnapshotWriter w;
    SnapshotReader r;
    const auto parts = roundTrip(w, r, s, 1.0);
    ASSERT_TRUE(!parts.empty());
    std::map<int64_t, EntityState> got;
    std::vector<int64_t> removed;
    std::vector<Player> players;
    std::optional<FogView> fogGot;
    std::vector<PatchView> patches;
    std::vector<WellView> wells;
    for (const SnapshotPart& p : parts) {
        EXPECT_EQ(p.snapshot.serverTick, int64_t(600));
        EXPECT_EQ(p.snapshot.lastInputTick, int64_t(590));
        EXPECT_EQ(p.snapshot.game, int64_t(2));
        EXPECT_TRUE(p.snapshot.driven == std::optional<int64_t>(5));
        EXPECT_TRUE(p.snapshot.score == s.score);
        for (const EntityState& e : p.snapshot.entities) got[e.id()] = e;
        removed.insert(removed.end(), p.snapshot.removed.begin(), p.snapshot.removed.end());
        if (!p.snapshot.players.empty()) players = p.snapshot.players;
        if (p.snapshot.fog) fogGot = *p.snapshot.fog;
        patches.insert(patches.end(), p.snapshot.patches.begin(), p.snapshot.patches.end());
        wells.insert(wells.end(), p.snapshot.wells.begin(), p.snapshot.wells.end());
    }
    ASSERT_TRUE(got.size() == 3);
    EXPECT_TRUE(*got[5].unit() == quantised(fullUnit(5)));
    EXPECT_TRUE(*got[6].structure() == quantised(fullStructure(6)));
    EXPECT_TRUE(got[6].remembered);
    EXPECT_TRUE(!got[7].remembered);
    EXPECT_TRUE(removed == s.removed);
    ASSERT_TRUE(players.size() == 2);
    EXPECT_TRUE(players[0] == quantised(s.players[0]));
    EXPECT_TRUE(players[1] == quantised(s.players[1]));
    ASSERT_TRUE(fogGot.has_value());
    EXPECT_TRUE(*fogGot == quantised(fog));
    ASSERT_TRUE(patches.size() == 1 && wells.size() == 1);
    EXPECT_TRUE(patches[0].patch == quantised(patch));
    EXPECT_TRUE(wells[0].well == quantised(s.wells[0].well));
}

TEST(snapshot_deltas_carry_only_changes) {
    Snapshot s;
    for (int64_t i = 0; i < 100; i++) {
        Unit u(100 + i, UnitKind::ranger, 0, Vec2(double(i), 2.0 * double(i)), 0.1 * double(i), Unit::Task::idle);
        u.hp = 45;
        s.entities.push_back(EntityState{u});
    }
    s.players = {fullPlayer()};
    SnapshotWriter w;
    SnapshotReader r;
    const SnapshotWriter::Output first = w.write(s, 0.0, 1 << 20);
    for (const auto& m : first.messages) ASSERT_TRUE(r.read(m).has_value());
    w.ack(r.ack(), 0.0);
    EXPECT_EQ(first.whole, size_t(100 + 1 + 2 + 2));

    // Nothing changed: nothing but the header goes.
    s.serverTick += 3;
    const SnapshotWriter::Output still = w.write(s, 0.05, 1 << 20);
    EXPECT_EQ(still.unchanged, size_t(105));
    EXPECT_EQ(still.whole + still.deltas, size_t(0));
    EXPECT_TRUE(bytes(still) < 40);
    for (const auto& m : still.messages) ASSERT_TRUE(r.read(m).has_value());
    w.ack(r.ack(), 0.05);

    // One unit walks a little and is hit: a few bytes for it alone.
    Unit& moved = std::get<Unit>(s.entities[10].value);
    moved.position = moved.position + Vec2(0.3, -0.2);
    moved.hp = 30;
    const SnapshotWriter::Output one = w.write(s, 0.1, 1 << 20);
    EXPECT_EQ(one.deltas, size_t(1));
    EXPECT_TRUE(bytes(one) < 40);
    std::optional<Unit> got;
    for (const auto& m : one.messages) {
        const auto p = r.read(m);
        ASSERT_TRUE(p.has_value());
        for (const EntityState& e : p->snapshot.entities) got = *e.unit();
    }
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(*got == quantised(moved));
    EXPECT_TRUE(bytes(first) > 20 * bytes(one));
}

TEST(snapshot_unacked_changes_go_again_against_the_acked_baseline) {
    Snapshot s;
    Unit u(1, UnitKind::comet, 0, Vec2(0, 0), 0, Unit::Task::idle);
    u.hp = 60;
    s.entities.push_back(EntityState{u});
    SnapshotWriter w;
    SnapshotReader r;
    roundTrip(w, r, s, 0);
    // Two snapshots lost on the way; the third is read against the first.
    for (int k = 1; k <= 3; k++) {
        std::get<Unit>(s.entities[0].value).position = Vec2(double(k), 0);
        const auto o = w.write(s, 0.05 * k, 1 << 20);
        EXPECT_EQ(o.deltas, size_t(1));
        if (k < 3) continue;
        for (const auto& m : o.messages) {
            const auto p = r.read(m);
            ASSERT_TRUE(p.has_value());
            ASSERT_TRUE(p->snapshot.entities.size() == 1);
            EXPECT_EQ(p->snapshot.entities[0].unit()->position.x, 3.0);
        }
    }
    // A unit back where its acked baseline has it still goes: the client
    // may hold a newer, unacked state.
    w.ack(r.ack(), 0.2);
    std::get<Unit>(s.entities[0].value).position = Vec2(0, 0);
    const auto lost = w.write(s, 0.25, 1 << 20);
    EXPECT_EQ(lost.deltas, size_t(1));
    std::get<Unit>(s.entities[0].value).position = Vec2(3, 0);
    const auto back = w.write(s, 0.3, 1 << 20);
    EXPECT_EQ(back.deltas, size_t(1));
}

TEST(snapshot_removed_until_acked) {
    Snapshot s;
    s.entities.push_back(EntityState{Unit(1, UnitKind::ranger, 1, Vec2(0, 0), 0, Unit::Task::idle)});
    SnapshotWriter w;
    SnapshotReader r;
    roundTrip(w, r, s, 0);
    s.entities.clear();
    s.removed = {1};
    // Lost: the next says it again.
    w.write(s, 0.05, 1 << 20);
    s.removed.clear();
    const auto again = roundTrip(w, r, s, 0.1);
    ASSERT_TRUE(!again.empty());
    EXPECT_TRUE(again[0].snapshot.removed == std::vector<int64_t>{1});
    // Acked: not again.
    const auto after = roundTrip(w, r, s, 0.15);
    for (const auto& p : after) EXPECT_TRUE(p.snapshot.removed.empty());
    // Back in view: whole, its old baseline gone.
    s.entities.push_back(EntityState{Unit(1, UnitKind::ranger, 1, Vec2(5, 0), 0, Unit::Task::idle)});
    const auto o = w.write(s, 0.2, 1 << 20);
    EXPECT_EQ(o.whole, size_t(1));
}

TEST(snapshot_resends_whole_after_a_second_without_acks) {
    Snapshot s;
    for (int64_t i = 0; i < 5; i++) s.entities.push_back(EntityState{Unit(i + 1, UnitKind::ranger, 0, Vec2(double(i), 0), 0, Unit::Task::idle)});
    SnapshotWriter w;
    SnapshotReader r;
    roundTrip(w, r, s, 0);
    const auto o1 = w.write(s, 0.5, 1 << 20);
    EXPECT_EQ(o1.unchanged, size_t(5));
    const auto o2 = w.write(s, 1.6, 1 << 20);
    EXPECT_EQ(o2.whole, size_t(5));
    EXPECT_EQ(w.fullResets(), int64_t(1));
}

TEST(snapshot_spans_parts_each_readable_alone) {
    Snapshot s;
    for (int64_t i = 0; i < 400; i++) {
        Unit u = fullUnit(i + 1);
        u.position = Vec2(double(i), 0);
        s.entities.push_back(EntityState{u});
    }
    SnapshotWriter w;
    const auto o = w.write(s, 0, 1 << 20);
    EXPECT_TRUE(o.messages.size() > 2);
    EXPECT_TRUE(o.messages.size() <= kMaxSnapshotParts);
    // Every other part lost: the rest still read, and only they are acked.
    SnapshotReader r;
    size_t read = 0;
    for (size_t i = 0; i < o.messages.size(); i += 2) {
        EXPECT_TRUE(o.messages[i].size() <= kMaxUnreliableBytes);
        const auto p = r.read(o.messages[i]);
        ASSERT_TRUE(p.has_value());
        EXPECT_EQ(p->part, int(i));
        read += p->snapshot.entities.size();
    }
    EXPECT_TRUE(read > 0);
    const AckMessage a = r.ack();
    ASSERT_TRUE(a.received.size() == 1);
    for (size_t i = 0; i < o.messages.size(); i++) EXPECT_EQ(bool(a.received[0].parts & (1u << i)), i % 2 == 0);
}

TEST(snapshot_budget_defers_the_rest) {
    Snapshot s;
    for (int64_t i = 0; i < 200; i++)
        s.entities.push_back(EntityState{Unit(i + 1, UnitKind::ranger, 0, Vec2(double(i), 0), 0, Unit::Task::idle), false,
                                         i == 0 ? Rank::driven : Rank::nearby});
    SnapshotWriter w;
    const int64_t budget = 1000;
    const auto o = w.write(s, 0, budget);
    int64_t spent = 0;
    for (const auto& m : o.messages) spent += int64_t(m.size()) + SnapshotWriter::kOverhead;
    EXPECT_TRUE(spent <= budget);
    EXPECT_TRUE(!o.deferred.empty());
    EXPECT_EQ(o.whole + o.deferred.size(), size_t(200));
    // The driven unit goes first.
    SnapshotReader r;
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p.has_value() && !p->snapshot.entities.empty());
    EXPECT_EQ(p->snapshot.entities[0].id(), int64_t(1));
    // No budget: nothing at all.
    EXPECT_TRUE(SnapshotWriter().write(s, 0, 10).messages.empty());
}

TEST(snapshot_reader_refuses_a_missing_baseline) {
    Snapshot s;
    s.entities.push_back(EntityState{Unit(1, UnitKind::ranger, 0, Vec2(0, 0), 0, Unit::Task::idle)});
    SnapshotWriter w;
    SnapshotReader r;
    roundTrip(w, r, s, 0);
    std::get<Unit>(s.entities[0].value).position = Vec2(1, 0);
    const auto o = w.write(s, 0.05, 1 << 20);
    SnapshotReader fresh;
    EXPECT_TRUE(!fresh.read(o.messages[0]).has_value());
    EXPECT_TRUE(fresh.ack().received.empty());
    // Broken bytes are refused too.
    std::vector<uint8_t> cut(o.messages[0].begin(), o.messages[0].begin() + 3);
    EXPECT_TRUE(!r.read(cut).has_value());
}

TEST(snapshot_carries_the_worlds_sky_in_every_part) {
    // The world's clock and weather ride in the header: every part has it,
    // so a lost part never leaves the sky behind.
    Snapshot s;
    for (int64_t i = 0; i < 400; i++) s.entities.push_back(EntityState{fullUnit(i + 1)});
    Sky sky;
    sky.time = 12345.678;
    sky.dayLength = 2400;
    sky.heldHour = 22.25;
    sky.weatherSeed = 0xDEADBEEF;
    sky.weatherHold = 3.0;
    sky.weather = WeatherState::storm;
    s.sky = sky;
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    EXPECT_TRUE(o.messages.size() > 1);
    for (const auto& m : o.messages) {
        const auto p = r.read(m);
        ASSERT_TRUE(p.has_value());
        ASSERT_TRUE(p->snapshot.sky.has_value());
        EXPECT_TRUE(*p->snapshot.sky == sky);
        EXPECT_NEAR(p->snapshot.sky->hour(), 22.25, 1e-9);
    }
    // None: none comes (a region of no world).
    Snapshot plain;
    SnapshotReader r2;
    SnapshotWriter w2;
    const auto parts = roundTrip(w2, r2, plain, 0);
    ASSERT_TRUE(!parts.empty());
    EXPECT_TRUE(!parts[0].snapshot.sky.has_value());
    // Running, not held: the hour is the world clock's at its time.
    sky.heldHour = std::nullopt;
    sky.weatherHold = std::nullopt;
    sky.weather = WeatherState::cloudy;
    s.sky = sky;
    SnapshotReader r3;
    SnapshotWriter w3;
    const auto again = roundTrip(w3, r3, s, 0);
    ASSERT_TRUE(!again.empty());
    EXPECT_TRUE(*again[0].snapshot.sky == sky);
    EXPECT_NEAR(again[0].snapshot.sky->hour(), WorldClock::hourAt(12345.678), 1e-9);
}

TEST(snapshot_carries_the_pilots_xp) {
    Snapshot s;
    s.xp = 145775.5;
    SnapshotWriter w;
    SnapshotReader r;
    const auto parts = roundTrip(w, r, s, 0);
    ASSERT_TRUE(!parts.empty());
    ASSERT_TRUE(parts[0].snapshot.xp.has_value());
    EXPECT_NEAR(*parts[0].snapshot.xp, 145775.5, 1e-3);
    Mirror m(RegionDescription{});
    m.apply(parts[0]);
    ASSERT_TRUE(m.xp().has_value());
    EXPECT_NEAR(*m.xp(), 145775.5, 1e-3);
    // None off a world.
    Snapshot plain;
    SnapshotWriter w2;
    SnapshotReader r2;
    EXPECT_TRUE(!roundTrip(w2, r2, plain, 0)[0].snapshot.xp.has_value());
}

// H2: the mercenaries, the fields, the terms, the tags, the news.
TEST(protocol_progress_carries_the_mercenaries) {
    using Merc = ProgressMessage::Merc;
    ProgressMessage m;
    m.mercs = {Merc{1, "Flint", Merc::State::working, "start", 2, 0, 1240, 88},
               Merc{4, "Wren", Merc::State::down, "rust-hollow", 0, 1834.5, 0, 0},
               Merc{5, "Slate", Merc::State::noDepot, "tern-ridge", 3, 0, 9, 0}};
    m.mercFields = {ProgressMessage::MercField{"start", "Prospect Landing", 2, Vec2(40.5, 61.25), 6, 4200},
                    ProgressMessage::MercField{"rust-hollow", "Rust Hollow", 0, Vec2(12, 80), 4, 900}};
    m.mercLimit = 2;
    m.mercFee = {1000, 100};
    m.mercWage = 8;
    m.mercRate = 230.625;
    m.mercNextLevel = 30;
    m.mercTags = {{88, "Flint"}, {91, "Kiln"}};
    m.regionsSeen = {"start", "rust-hollow"};
    m.news = {ProgressMessage::News{ProgressMessage::NewsKind::merc, "away", 0, 1240, ""},
              ProgressMessage::News{ProgressMessage::NewsKind::merc, "left", 0, 8, "Wren"}};
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
}

TEST(protocol_progress_round_trips) {
    ProgressMessage m;
    m.xp = 6123.25;
    m.level = 10;
    m.points = 3;
    m.ranks = {{Skill::quickDrill, 3}, {Skill::bigHaul, 2}, {Skill::overcharge, 1}, {allCases<Skill>().back(), 1}};
    m.slots = {Skill::overcharge, std::nullopt, Skill::aegis};
    m.ready = {{Skill::overcharge, 512.125}};
    m.undo = {{Skill::bigHaul, 1234.5}};
    m.respecPrice = 40;
    m.respecFree = false;
    m.blueprints = {{StructureKind::citadel, 1}, {StructureKind::derrick, 2}, {StructureKind::sentinel, 12}};
    m.sitesKnown = {"tern-ridge/wreck", "start/crate"};
    m.sitesFound = {"start/crate"};
    m.news = {{ProgressMessage::NewsKind::level, "", 0, 10, ""},
              {ProgressMessage::NewsKind::region, "tern-ridge", 250, 0, ""},
              {ProgressMessage::NewsKind::feat, "first-flight", 50, 0, ""},
              {ProgressMessage::NewsKind::reset, "", 0, 9, ""},
              {ProgressMessage::NewsKind::site, "start/crate", 0, 1, ""},
              {ProgressMessage::NewsKind::chip, "tern-ridge/wreck", 25, 2, ""}};
    m.opening = ProgressMessage::Opening{"tern-ridge/vault", 0, 10, "The Sentinel vault opens at level 55"};
    Contract ore;
    ore.id = 7;
    ore.kind = Contract::Kind::deliverOre;
    ore.title = "Ore Run";
    ore.pitch = "Plain work, honest pay. Haul 300 ore to your Citadel and the station sends something back.";
    ore.need = 300;
    ore.have = 120;
    ore.xp = 600;
    ore.hydrogen = 10;
    Contract pad = ore;
    pad.id = 8;
    pad.kind = Contract::Kind::reach;
    pad.state = Contract::State::done;
    pad.target = "cold-fork/pad/landing";
    pad.region = "cold-fork";
    pad.need = pad.have = 1;
    Contract sweep = ore;
    sweep.id = 9;
    sweep.kind = Contract::Kind::sweep;
    sweep.state = Contract::State::abandoned;
    m.contracts = {ore, pad, sweep};
    m.contractsDawn = 4855.5;
    m.news.push_back({ProgressMessage::NewsKind::contract, "8", 600, 10, ""});
    const std::vector<uint8_t> bytes = encode(m);
    EXPECT_TRUE(messageType(bytes) == MessageType::progress);
    ProgressMessage back;
    ASSERT_TRUE(decode(bytes, back));
    EXPECT_TRUE(back == m);
    // A fresh one: nothing learned.
    ProgressMessage fresh;
    ASSERT_TRUE(decode(encode(fresh), back));
    EXPECT_TRUE(back == fresh);
    // Cut short: refused.
    std::vector<uint8_t> cut = bytes;
    cut.resize(cut.size() / 2);
    EXPECT_TRUE(!decode(cut, back));
    // A wreck held open since world time 812.5.
    m.opening = ProgressMessage::Opening{"start/wreck", 812.5, 5, ""};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    // Fallen (P6): back at a Hab Dome in Cold Fork at 920.25 with 30 MH;
    // then at the start pad.
    m.respawn = ProgressMessage::Respawn{920.25, "cold-fork", StructureKind::habDome, 30, true};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    m.respawn = ProgressMessage::Respawn{12, "start", std::nullopt, 0, false};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    // The rig (P7): two fitted, stores, a mark researched, both jobs running,
    // the Foundry and the Lab owned, and a module made.
    m.rig.fitted = {{Module::drillHead, 2}, {Module::surveyLens, 1}};
    m.rig.stores = {{Module::drillHead, 1, 1}, {Module::plate, 3, 12}};
    m.rig.researched = {{Module::drillHead, 2}, {Module::plate, 3}};
    m.rig.making = ModuleJob{Module::torchTip, 1, 1060.5};
    m.rig.research = ModuleJob{Module::cargoPod, 2, 1600};
    m.foundry = m.lab = true;
    m.news.push_back({ProgressMessage::NewsKind::made, "torchTip", 0, 1, ""});
    m.news.push_back({ProgressMessage::NewsKind::researched, "cargoPod", 0, 2, ""});
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    // The realm's buildings (R1, v12): counts, who built them here, the
    // pilot's callsign, and a Hab Dome finished by another pilot.
    m.realm = {{StructureKind::citadel, 1}, {StructureKind::habDome, 3}};
    m.builders = {{4, "Pilot 045B"}, {9, "Pilot C0DE"}};
    m.callsign = "Pilot C0DE";
    m.news.push_back({ProgressMessage::NewsKind::built, "habDome", 0, 1, "Pilot 045B"});
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(12));
    // The realm wallet (C1, v13): the realm's, the tax, what the pilot gave.
    m.realmWallet = {123456, 789};
    m.taxPercent = 10;
    m.taxed = {3000, 40};
    m.donated = {1000, 0};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(13));
    // The Citadel shop (C2, v14): copies bought, consumables held, their
    // cooldowns, a shield running, a flare burning.
    m.bought = {{StructureKind::habDome, 2}, {StructureKind::sentinel, 1}};
    m.items = {{Item::repairKit, 5}, {Item::surveyFlare, 1}};
    m.itemReady = {{Item::shieldCharge, 4021.5}};
    m.shieldUntil = 4005.25;
    m.citadelMarks = TurretMarks{.rate = 1, .damage = 3, .range = 2};
    m.flare = ProgressMessage::Flare{"rust-hollow", Vec2(48.5, 102.25), 40, 4060};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(14));
    // Citadel missions (K2, v16): the board and the Journal, every kind,
    // tier and state, a reward with a blueprint and one with supplies; the
    // mission news.
    CitadelMission survey;
    survey.id = 7;
    survey.kind = CitadelMission::Kind::survey;
    survey.tier = CitadelMission::Tier::perilous;
    survey.key = "survey-perilous";
    survey.title = "Readings from the Dark Country";
    survey.briefing = "KSTR dispatch to pilot, do you copy?";
    survey.instructions = "Drive to the marker in Slag Pockets and stay inside its ring for 20 s.";
    survey.target = survey.region = "slag-pockets";
    survey.at = Vec2(-40.5, 12.25);
    survey.radius = 6;
    survey.need = 20;
    survey.xp = 3000;
    survey.ore = 1000;
    survey.hydrogen = 70;
    survey.blueprint = StructureKind::foundry;
    survey.count = 2;
    CitadelMission haul;
    haul.id = 8;
    haul.state = CitadelMission::State::taken;
    haul.target = "ore";
    haul.need = 200;
    haul.have = 150;
    haul.item = Item::shieldCharge;
    haul.count = 3;
    CitadelMission hunt = haul;
    hunt.id = 9;
    hunt.kind = CitadelMission::Kind::hunt;
    hunt.tier = CitadelMission::Tier::hard;
    hunt.state = CitadelMission::State::done;
    hunt.item.reset();
    m.board = {survey};
    m.journal = {haul, hunt};
    m.news = {ProgressMessage::News{ProgressMessage::NewsKind::mission, "9", 1500, 450, ""}};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
    EXPECT_TRUE(kProtocolVersion >= uint16_t(16));
}

TEST(mirror_takes_snapshots_into_its_simulation) {
    RegionDescription region;
    region.map = MapChoice{MapStyle::openField, MapSize::small, 2};
    const MapDefinition map = WindowMaps::build(region.map);
    region.mapName = map.name;
    region.mapVersion = map.version;
    region.seed = map.seed;
    Mirror mirror(region);
    EXPECT_TRUE(mirror.matches());
    EXPECT_TRUE(mirror.simulation().state.units.empty());
    // The players (the wallet) are the map's start values until a snapshot brings them.
    EXPECT_TRUE(!mirror.playersKnown());

    Snapshot s;
    s.serverTick = 30;
    s.driven = 1;
    s.time = 0.5;
    Unit mine(1, UnitKind::ranger, 0, Vec2(1, 1), 0, Unit::Task::idle);
    mine.hp = 45;
    Unit theirs(2, UnitKind::ranger, 1, Vec2(3, 3), 0, Unit::Task::idle);
    theirs.hp = 45;
    s.entities = {EntityState{mine}, EntityState{theirs}, EntityState{fullStructure(3), true}};
    s.players = {Player{}, Player{}};
    FogView fog;
    const size_t cells = size_t(mirror.simulation().vision->width * mirror.simulation().vision->height);
    fog.seen.assign(cells, 0);
    fog.near.assign(cells, 0);
    fog.explored.assign((cells + 63) / 64, 0);
    fog.seen[7] = 1;
    s.fog = std::make_shared<const FogView>(fog);
    SnapshotWriter w;
    SnapshotReader r;
    Sky sky;
    sky.time = 100;
    sky.heldHour = 23.0;
    s.sky = sky;
    for (const SnapshotPart& p : roundTrip(w, r, s, 0)) mirror.apply(p);
    EXPECT_TRUE(mirror.playersKnown());
    // The world's hour holds the mirror's: its sight at night is the server's.
    ASSERT_TRUE(mirror.sky().has_value());
    ASSERT_TRUE(mirror.simulation().state.heldHour.has_value());
    EXPECT_NEAR(*mirror.simulation().state.heldHour, 23.0, 1e-9);
    const Simulation& sim = mirror.simulation();
    EXPECT_EQ(sim.state.units.size(), size_t(2));
    EXPECT_TRUE(sim.unitIndex(2).has_value());
    EXPECT_TRUE(sim.sees(0, 2));
    EXPECT_TRUE(sim.pilot.has_value() && sim.pilot->unit == 1);
    EXPECT_EQ(sim.state.time, 0.5);
    ASSERT_TRUE(sim.state.intel.has_value());
    EXPECT_EQ((*sim.state.intel)[0].buildings.size(), size_t(1));
    EXPECT_EQ(sim.sights[0].seen[7], uint8_t(1));

    // Gone from sight: out of the mirror.
    s.entities = {EntityState{mine}};
    s.removed = {2};
    for (const SnapshotPart& p : roundTrip(w, r, s, 0.05)) mirror.apply(p);
    EXPECT_EQ(mirror.simulation().state.units.size(), size_t(1));
    EXPECT_TRUE(!mirror.simulation().sees(0, 2));
}

TEST(protocol_world_messages_round_trip) {
    // A world's region: its region file whole, wild ground.
    Welcome w;
    w.pilot = 3;
    w.region.map = MapChoice{MapStyle::ridge, MapSize::medium, 2};
    w.region.regionFile = std::string(3000, 'x');
    w.region.wild = true;
    w.region.mapName = "Tern Ridge";
    Welcome wb;
    ASSERT_TRUE(decode(encode(w), wb));
    EXPECT_TRUE(wb == w);

    RegionChange c;
    c.visit = 4;
    c.region = w.region;
    c.regionId = "tern-ridge";
    c.driven = 77;
    c.serverTick = 1234;
    c.time = 20.5;
    RegionChange cb;
    ASSERT_TRUE(decode(encode(c), cb));
    EXPECT_TRUE(cb == c);
    c.driven = std::nullopt;
    ASSERT_TRUE(decode(encode(c), cb));
    EXPECT_TRUE(cb == c);
    EXPECT_TRUE(messageType(encode(c)) == MessageType::regionChange);

    WorldMessage m;
    m.columns = 3;
    m.rows = 2;
    for (int i = 0; i < 6; i++) {
        WorldMessage::Region g;
        g.id = "r" + std::to_string(i);
        g.name = "Region " + std::to_string(i);
        g.style = MapStyle(i % 4);
        g.size = MapSize::medium;
        // Every ground palette goes across (W3).
        g.ground = region::Ground(i + 2);
        g.column = i % 3;
        g.row = i / 3;
        g.pads = {{"landing", Vec2(72, 48)}, {"far", Vec2(10.5, 90)}};
        // The realm's Landing Pads and Spacedock (F2).
        if (i == 2) g.pads.push_back({"Spacedock", Vec2(30.5, 41), StructureKind::spacedock});
        if (i == 4) g.pads.push_back({"Landing Pad 2", Vec2(12, 60.5), StructureKind::landingPad});
        // The Matriarch's nest (N3), in one region.
        if (i == 5) g.nest = Vec2(122, 44.5);
        m.regions.push_back(g);
    }
    m.here = 4;
    m.flyingTo = 1;
    WorldMessage mb;
    ASSERT_TRUE(decode(encode(m), mb));
    EXPECT_TRUE(mb == m);
    // F2: aboard the Dropship, its flight with it.
    WorldMessage::Ferry f0;
    f0.from = 4;
    f0.boarded = Vec2(12, 60.5);
    f0.pad = "Landing Pad 1";
    f0.at = Vec2(70.25, 33);
    f0.plan = ac::Ferry::plan(2, -0.4, 0.3);
    f0.elapsed = 7.125;
    m.ferry = f0;
    ASSERT_TRUE(decode(encode(m), mb));
    EXPECT_TRUE(mb == m);
    EXPECT_TRUE(mb.ferry->plan == ac::Ferry::plan(2, -0.4, 0.3));
    m.ferry = std::nullopt;
    m.flyingTo = std::nullopt;
    ASSERT_TRUE(decode(encode(m), mb));
    EXPECT_TRUE(mb == m);
    EXPECT_TRUE(messageType(encode(m)) == MessageType::world);
    EXPECT_TRUE(!decode(encode(c), mb));

    RequestMessage f;
    f.id = 11;
    f.what = RequestMessage::Fly{"cold-fork", "landing"};
    RequestMessage fb;
    ASSERT_TRUE(decode(encode(f), fb));
    EXPECT_TRUE(fb == f);

    RequestMessage b;
    b.id = 12;
    b.what = RequestMessage::Build{StructureKind::citadel, Vec2(-7.5, 12.5)};
    RequestMessage bb;
    ASSERT_TRUE(decode(encode(b), bb));
    EXPECT_TRUE(bb == b);
    b.what = RequestMessage::Build{StructureKind::derrick, Vec2(-12.25, -0.75)};
    ASSERT_TRUE(decode(encode(b), bb));
    EXPECT_TRUE(bb == b);

    // A level-up card: the kind and the pick; one past the catalogue is refused.
    RequestMessage k;
    k.id = 13;
    k.what = RequestMessage::Pick{UnitKind::prospector, Perk::prospectorBigHaul};
    RequestMessage kb;
    ASSERT_TRUE(decode(encode(k), kb));
    EXPECT_TRUE(kb == k);
    k.what = RequestMessage::Pick{UnitKind::scorpion, allCases<Perk>().back()};
    ASSERT_TRUE(decode(encode(k), kb));
    EXPECT_TRUE(kb == k);
    k.what = RequestMessage::Pick{UnitKind::prospector, static_cast<Perk>(allCases<Perk>().size())};
    EXPECT_TRUE(!decode(encode(k), kb));

    // The skill tree: Spend, Undo, Respec, Slot (and emptying one); a skill
    // past the catalogue is refused.
    for (const auto& what : std::vector<decltype(RequestMessage::what)>{
             RequestMessage::Spend{Skill::quickDrill}, RequestMessage::Spend{allCases<Skill>().back()},
             RequestMessage::Undo{Skill::overcharge}, RequestMessage::Respec{}, RequestMessage::Slot{2, Skill::aegis},
             RequestMessage::Slot{0, std::nullopt}, RequestMessage::Abandon{4000000000u},
             RequestMessage::Make{Module::drillHead, 1}, RequestMessage::Research{Module::surveyLens, 3},
             RequestMessage::Fit{Module::plate, 2}, RequestMessage::Fit{Module::cargoPod, 0},
             RequestMessage::Donate{1000, 0}, RequestMessage::Donate{0, 250},
             RequestMessage::Buy{StructureKind::sentinel, Item::repairKit}, RequestMessage::Buy{std::nullopt, Item::surveyFlare},
             RequestMessage::Use{Item::hydrogenCanister}, RequestMessage::Use{Item::shieldCharge},
             RequestMessage::MissionAsk{3, true}, RequestMessage::MissionAsk{4000000000u, false},
             RequestMessage::UpgradeCitadel{DefenceStat::rate}, RequestMessage::UpgradeCitadel{DefenceStat::range},
             RequestMessage::Buy{StructureKind::gatling, Item::repairKit}}) {
        RequestMessage t;
        t.id = 21;
        t.what = what;
        RequestMessage tb;
        ASSERT_TRUE(decode(encode(t), tb));
        EXPECT_TRUE(tb == t);
    }
    k.what = RequestMessage::Spend{static_cast<Skill>(allCases<Skill>().size())};
    EXPECT_TRUE(!decode(encode(k), kb));
    // C2: Buy and Use sit at 14 and 15 of a 5-bit index; an item past the
    // four is refused.
    {
        RequestMessage shop;
        shop.what = RequestMessage::Buy{std::nullopt, Item::repairKit};
        EXPECT_EQ(shop.what.index(), size_t(14));
        shop.what = RequestMessage::Use{Item::repairKit};
        EXPECT_EQ(shop.what.index(), size_t(15));
        shop.what = RequestMessage::Use{static_cast<Item>(allCases<Item>().size())};
        EXPECT_TRUE(!decode(encode(shop), kb));
        // K2: MissionAsk at 16; D1: UpgradeCitadel at 17 (D3: the four
        // stats fill its 2 bits).
        shop.what = RequestMessage::MissionAsk{1, true};
        EXPECT_EQ(shop.what.index(), size_t(16));
        shop.what = RequestMessage::UpgradeCitadel{DefenceStat::damage};
        EXPECT_EQ(shop.what.index(), size_t(17));
        shop.what = RequestMessage::UpgradeCitadel{DefenceStat::air};
        EXPECT_TRUE(decode(encode(shop), kb) && kb == shop);
        // H2: Merc at 18; D3: UpgradeTurret at 19.
        shop.what = RequestMessage::Merc{};
        EXPECT_EQ(shop.what.index(), size_t(18));
        shop.what = RequestMessage::UpgradeTurret{4711, DefenceStat::air};
        EXPECT_EQ(shop.what.index(), size_t(19));
        EXPECT_TRUE(decode(encode(shop), kb) && kb == shop);
    }
    // H2: hire, dismiss, send to a field.
    for (const auto& ask : {RequestMessage::Merc{RequestMessage::Merc::Act::hire, 0, "", 0},
                            RequestMessage::Merc{RequestMessage::Merc::Act::dismiss, 3, "", 0},
                            RequestMessage::Merc{RequestMessage::Merc::Act::assign, 4000000000u, "rust-hollow", 7}}) {
        RequestMessage t;
        t.id = 22;
        t.what = ask;
        RequestMessage tb;
        ASSERT_TRUE(decode(encode(t), tb));
        EXPECT_TRUE(tb == t);
    }

    // A snapshot's game carries the visit.
    EXPECT_EQ(visitOf(visitGame(5, 2)), int64_t(5));
    EXPECT_EQ(visitGame(5, 2) & 0xffffffff, int64_t(2));
    EXPECT_EQ(visitOf(visitGame(0, 3)), int64_t(0));
}

TEST(mirror_builds_a_region_file) {
    const region::RegionFile file = region::starter("tern", MapStyle::ridge, 15, MapSize::medium);
    const region::Built built = region::build(file);
    RegionDescription region;
    region.map = MapChoice{MapStyle::ridge, MapSize::medium, 2};
    region.regionFile = region::write(file);
    region.wild = true;
    region.mapName = built.map.name;
    region.mapVersion = built.map.version;
    region.seed = built.map.seed;
    Mirror mirror(region);
    EXPECT_TRUE(mirror.matches());
    EXPECT_EQ(mirror.simulation().map->bounds.maxX - mirror.simulation().map->bounds.minX, 144.0);
    // The generated map of the same style is another map.
    region.regionFile.clear();
    EXPECT_TRUE(!Mirror(region).matches());
}

// docs/perf.md "Server" rounds 4 and 5: the writer builds a fog chunk eight
// cells to a word, and keeps the chunks it built for the same inputs. Any
// byte not 0 is a cell seen (or near), a ragged last chunk and arrays
// shorter than the cells are read as before: what comes back is the fog
// quantised, whole, as a delta and after the fog's size changed.
TEST(protocol_fog_chunks_of_any_bytes_and_sizes_round_trip) {
    SeededRandom dice(4242);
    for (const size_t cells : {size_t(1), size_t(63), size_t(2048), size_t(4099), size_t(2 * 2048 + 517)}) {
        for (const bool shortArrays : {false, true}) {
            Snapshot s;
            s.serverTick = 10;
            FogView fog;
            fog.seen.resize(cells);
            fog.near.resize(shortArrays && cells > 70 ? cells - 70 : cells);
            fog.explored.assign(shortArrays && cells > 200 ? (cells - 200 + 63) / 64 : (cells + 63) / 64, 0);
            const uint8_t values[] = {0, 0, 0, 1, 2, 128, 255};
            // Runs of cells, as fog comes (cell by cell noise would not fit
            // the snapshot's parts), of any byte values.
            auto runs = [&](std::vector<uint8_t>& a) {
                for (size_t i = 0; i < a.size();) {
                    const size_t n = size_t(dice.random(1, 90));
                    const uint8_t v = values[dice.random(0, 7)];
                    for (size_t k = 0; k < n && i < a.size(); k++) a[i++] = dice.random(0, 6) ? v : values[dice.random(0, 7)];
                }
            };
            auto shuffle = [&] {
                runs(fog.seen);
                runs(fog.near);
                for (auto& w : fog.explored) w = dice.random(0, 2) ? 0 : ~uint64_t(0) << dice.random(0, 64);
            };
            shuffle();
            s.fog = std::make_shared<const FogView>(fog);
            SnapshotWriter w;
            SnapshotReader r;
            // A snapshot whose fog did not change carries none: the last stands.
            std::optional<FogView> got;
            for (int round = 0; round < 3; round++) {
                for (const SnapshotPart& p : roundTrip(w, r, s, 1.0 + round))
                    if (p.snapshot.fog) got = *p.snapshot.fog;
                ASSERT_TRUE(got.has_value());
                EXPECT_TRUE(*got == quantised(fog));
                // Some cells change: the next is a delta.
                for (size_t k = 0; k < 40; k++) fog.seen[size_t(dice.random(0, int64_t(cells)))] ^= 1;
                s.fog = std::make_shared<const FogView>(fog);
            }
            // Another region's fog, through the same writer: no chunk is
            // taken for the last one's (perf.md round 5 keeps built chunks).
            fog.seen.resize(cells + 1500);
            fog.near.resize(fog.near.size() + 1500);
            fog.explored.resize((fog.seen.size() + 63) / 64);
            shuffle();
            s.fog = std::make_shared<const FogView>(fog);
            for (const SnapshotPart& p : roundTrip(w, r, s, 9.0))
                if (p.snapshot.fog) got = *p.snapshot.fog;
            ASSERT_TRUE(got.has_value());
            EXPECT_TRUE(*got == quantised(fog));
        }
    }
}
