// The protocol's messages (NetProtocol.h) and snapshots (NetSnapshot.h):
// every message round trips, quantised values stay within their precision,
// and snapshot deltas rebuild exactly what the writer meant.

#include "test.h"

#include "NetMirror.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"
#include "NetTypes.h"

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
        in.walk = Vec2(0.5 - 0.3 * i, i % 2 ? -1.0 : 1.0);
        in.heading = 1.234 + i;
        in.act = i == 0;
        in.ability = i == 1;
        in.hold = i == 2;
        in.mend = i == 3;
        if (i == 1) in.crosshair = Pilot::Crosshair(Pilot::Crosshair::On{42});
        if (i == 2) in.crosshair = Pilot::Crosshair(Pilot::Crosshair::Clear{});
        m.inputs.push_back(in);
    }
    InputMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    ASSERT_TRUE(back.inputs.size() == 4);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_TRUE(back.inputs[i] == quantised(m.inputs[i]));
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

TEST(protocol_events_round_trip_every_kind) {
    using G = GameEvent;
    const Vec2 at(10.25, -20.5);
    std::vector<GameEvent> all = {
        G(G::Deposited{1, 5, at}),
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
    s.fog = fog;

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
        if (p.snapshot.fog) fogGot = p.snapshot.fog;
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
    s.fog = fog;
    SnapshotWriter w;
    SnapshotReader r;
    for (const SnapshotPart& p : roundTrip(w, r, s, 0)) mirror.apply(p);
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
