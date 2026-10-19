// Guns and practice on the wire (C2, protocol 37,
// docs/guns-and-practice.md): the Fired, Impact and DummyDown events and
// their widths, the Learn request, the Progress message's learned kinds, a
// structure's hits, downUntil, heat, overheat, spin and (the gatling's)
// barrels' pitch, the corner guns'
// heat, overheat and spin, the Target Dummy's kind.

#include "test.h"

#include "NetCodec.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <algorithm>
#include <cmath>
#include <variant>

namespace {

using namespace ac;
using namespace ac::net;

std::optional<Structure> roundTrip(const Structure& b) {
    Snapshot s;
    s.entities.push_back(EntityState{b});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    if (o.messages.size() != 1) return std::nullopt;
    const auto p = r.read(o.messages[0]);
    if (!p || p->snapshot.entities.size() != 1 || !p->snapshot.entities[0].structure()) return std::nullopt;
    return *p->snapshot.entities[0].structure();
}

std::optional<GameEvent> roundTrip(const GameEvent& e) {
    const auto messages = encodeEvents(1, {e});
    if (messages.size() != 1) return std::nullopt;
    EventMessage m;
    if (!decode(messages[0], m) || m.events.size() != 1) return std::nullopt;
    return m.events[0];
}

/// The width of one event on the wire, in bits, past the message's own.
size_t eventBits(const GameEvent& e) {
    BitWriter w;
    writeEvent(w, e);
    return w.bitCount();
}

} // namespace

TEST(guns_protocol_is_37_and_the_widths_hold) {
    EXPECT_TRUE(kProtocolVersion >= uint16_t(37));
    // Learn is request 24 of the 5-bit index; three events past the 18
    // (and protocol 39's Repaired after them, protocol 40's Demolish and
    // Demolished; protocol 41's BuyMule, MuleFill and Booked).
    EXPECT_EQ(std::variant_size_v<decltype(RequestMessage::what)>, size_t(28));
    EXPECT_EQ(std::variant_size_v<GameEvent::Value>, size_t(24));
    EXPECT_TRUE(std::variant_size_v<GameEvent::Value> <= size_t(32));
    // The dummy is the 12th kind; the kind stays 4 bits.
    EXPECT_EQ(codec::kStructureKindCount, size_t(12));
    EXPECT_EQ(codec::kStructureKindBits, 4);
    EXPECT_TRUE(allCases<StructureKind>().back() == StructureKind::dummy);
    // A round's point: 1/64 cell, 22 + 22 + 16 bits; its velocity: 1/8
    // cell/s, 12 bits an axis.
    EXPECT_EQ(q::kPointSteps, int64_t(64));
    EXPECT_EQ(q::kPointBits, 22);
    EXPECT_EQ(q::kPointHeightBits, 16);
    EXPECT_EQ(q::kVelocitySteps, int64_t(8));
    EXPECT_EQ(q::kVelocityBits, 12);
    // A Fired with small ids: the case (5), two one-byte ids (16), the
    // muzzle (60) and the velocity (36).
    using G = GameEvent;
    EXPECT_EQ(eventBits(G(G::Fired{3, 4, Vec3(10, 20, 1), Vec3(90, 0, 0)})), size_t(5 + 16 + 60 + 36));
    // An Impact: the case, the round, the point, what (2 bits), the victim.
    EXPECT_EQ(eventBits(G(G::Impact{4, Vec3(30, 20, 0), G::Impact::What::ground, 0})), size_t(5 + 8 + 60 + 2 + 8));
}

TEST(guns_fired_and_impact_round_trip_quantised) {
    using G = GameEvent;
    // On the grid: exact.
    const G fired(G::Fired{1234567, 42, Vec3(100.015625, -7.5, 1.546875), Vec3(89.875, -3.125, 4.625)});
    auto back = roundTrip(fired);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == fired);
    // Off the grid: within half a step.
    const Vec3 muzzle(12.3456, 78.9012, 1.2345);
    const Vec3 velocity(88.123, -11.777, 6.061);
    back = roundTrip(G(G::Fired{7, 8, muzzle, velocity}));
    ASSERT_TRUE(back && back->as<G::Fired>());
    const G::Fired& f = *back->as<G::Fired>();
    for (int i = 0; i < 3; i++) {
        EXPECT_NEAR(f.muzzle[i], muzzle[i], 0.5 / 64 + 1e-12);
        EXPECT_NEAR(f.velocity[i], velocity[i], 0.5 / 8 + 1e-12);
    }
    // Past the widths: clamped, not wrapped.
    back = roundTrip(G(G::Fired{7, 9, Vec3(1e6, -1e6, 900), Vec3(400, -400, 1e9)}));
    ASSERT_TRUE(back && back->as<G::Fired>());
    const G::Fired& wild = *back->as<G::Fired>();
    EXPECT_TRUE(wild.muzzle.x > 32000 && wild.muzzle.y < -32000 && wild.muzzle.z > 511);
    EXPECT_TRUE(wild.velocity.x > 255 && wild.velocity.y < -255 && wild.velocity.z > 255);
    // Not a number: 0.
    back = roundTrip(G(G::Fired{7, 10, Vec3(std::nan(""), 1, 1), Vec3(1, std::nan(""), 1)}));
    ASSERT_TRUE(back && back->as<G::Fired>());
    EXPECT_EQ(back->as<G::Fired>()->muzzle.x, 0.0);
    EXPECT_EQ(back->as<G::Fired>()->velocity.y, 0.0);

    // Impact: every what, the victim (0 for the ground).
    for (const auto what : {G::Impact::What::ground, G::Impact::What::unit, G::Impact::What::structure}) {
        const G impact(G::Impact{42, Vec3(130.25, -7.5, 0.5), what, what == G::Impact::What::ground ? 0 : 991});
        back = roundTrip(impact);
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == impact);
    }
    // A what past structure is a broken message.
    BitWriter w;
    w.writeBits(19, 5);
    writeSigned(w, 1);
    codec::Point{}.write(w, Vec3(1, 2, 3));
    w.writeBits(3, 2);
    writeSigned(w, 0);
    BitReader r(w.bytes().data(), w.bytes().size());
    GameEvent e;
    EXPECT_TRUE(!readEvent(r, e));
}

TEST(guns_dummy_down_round_trips) {
    using G = GameEvent;
    for (const G& e : {G(G::DummyDown{5, 0}), G(G::DummyDown{123456, 37}), G(G::DummyDown{9, 100000})}) {
        const auto back = roundTrip(e);
        ASSERT_TRUE(back.has_value());
        EXPECT_TRUE(*back == e);
    }
}

TEST(guns_learn_request_round_trips) {
    for (const StructureKind k : allCases<StructureKind>()) {
        RequestMessage m;
        m.id = 77;
        m.what = RequestMessage::Learn{k};
        EXPECT_EQ(m.what.index(), size_t(24));
        RequestMessage back;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
    // A kind past the dummy is refused.
    RequestMessage m;
    m.id = 1;
    m.what = RequestMessage::Learn{static_cast<StructureKind>(14)};
    RequestMessage back;
    EXPECT_TRUE(!decode(encode(m), back));
}

TEST(guns_progress_carries_the_learned_kinds) {
    ProgressMessage m;
    m.blueprints[StructureKind::derrick] = 2;
    m.learned = {StructureKind::citadel, StructureKind::gatling, StructureKind::landingPad};
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.learned == m.learned);
    EXPECT_TRUE(back.blueprints == m.blueprints);
    m.learned.clear();
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.learned.empty());
}

TEST(guns_dummy_and_gatling_fields_in_the_snapshot) {
    // A dummy down with its hits; it stands at an absolute time (ms).
    Structure d(8, StructureKind::dummy, 0, Vec2(20.5, 30.5));
    d.hp = 0;
    d.hits = 37;
    d.downUntil = 123.4567;
    auto back = roundTrip(d);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->kind == StructureKind::dummy);
    EXPECT_EQ(back->hits, int64_t(37));
    ASSERT_TRUE(back->downUntil.has_value());
    EXPECT_NEAR(*back->downUntil, 123.4567, 0.0005 + 1e-12);
    EXPECT_TRUE(*back == quantised(d));
    d.downUntil = std::nullopt;
    back = roundTrip(d);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(!back->downUntil.has_value());

    // A gatling's heat and spin in 255ths, its overheat; its spread stays.
    Structure g(9, StructureKind::gatling, 0, Vec2(5, 5));
    g.hp = 200;
    for (const double heat : {0.0, 0.4, 0.731, 1.0, 1.7, -0.2}) {
        g.heat = heat;
        g.spin = 1 - std::clamp(heat, 0.0, 1.0);
        g.overheated = heat >= 1;
        g.bloom = 0.012;
        back = roundTrip(g);
        ASSERT_TRUE(back.has_value());
        EXPECT_NEAR(back->heat, std::clamp(heat, 0.0, 1.0), 0.5 / 255 + 1e-12);
        EXPECT_NEAR(back->spin, g.spin, 0.5 / 255 + 1e-12);
        EXPECT_EQ(back->overheated, g.overheated);
        EXPECT_EQ(back->bloom, 0.0);
        EXPECT_TRUE(*back == quantised(g));
    }
    // The barrels' pitch: 8 bits over ±90° (the gatling's −60…+70° fits), level exact.
    for (const double pitch : {0.0, -1.0471975511965976, 1.2217304763960306, -0.25, 0.4}) {
        g.pitch = pitch;
        back = roundTrip(g);
        ASSERT_TRUE(back.has_value());
        EXPECT_NEAR(back->pitch, pitch, 3.141592653589793 / 2 / q::kPitchSteps / 2 + 1e-12);
        EXPECT_TRUE(*back == quantised(g));
    }
    g.pitch = 0;
    EXPECT_EQ(roundTrip(g)->pitch, 0.0);
    g.heat = 1;
    EXPECT_EQ(roundTrip(g)->heat, 1.0);
    g.heat = 0.4;
    EXPECT_EQ(roundTrip(g)->heat, 102.0 / 255);
}

TEST(guns_corner_guns_carry_heat_overheat_and_spin) {
    Structure c(3, StructureKind::citadel, 0, Vec2(10, 10));
    c.hp = 1500;
    c.guns = std::array<CornerGun, 4>{};
    for (int i = 0; i < 4; i++) {
        CornerGun& x = (*c.guns)[size_t(i)];
        x.heat = 0.3 * i;
        x.overheated = i == 3;
        x.spin = 0.25 * i;
        x.bloom = 0.004 * i;
    }
    const auto back = roundTrip(c);
    ASSERT_TRUE(back.has_value() && back->guns.has_value());
    for (int i = 0; i < 4; i++) {
        const CornerGun& x = (*back->guns)[size_t(i)];
        EXPECT_NEAR(x.heat, std::min(1.0, 0.3 * i), 0.5 / 255 + 1e-12);
        EXPECT_EQ(x.overheated, i == 3);
        EXPECT_NEAR(x.spin, 0.25 * i, 0.5 / 255 + 1e-12);
        // The spread stays on the server.
        EXPECT_EQ(x.bloom, 0.0);
    }
    EXPECT_TRUE(*back == quantised(c));
}

TEST(guns_hits_tick_as_a_delta) {
    Structure d(8, StructureKind::dummy, 0, Vec2(20.5, 30.5));
    d.hp = 150;
    SnapshotWriter w;
    SnapshotReader r;
    Snapshot s;
    s.entities.push_back(EntityState{d});
    size_t deltas = 0;
    for (int tick = 0; tick <= 10; tick++) {
        d.hits = tick * 3;
        d.hp = 150 - tick * 3;
        s.entities[0] = EntityState{d};
        const double now = tick / 20.0;
        const auto o = w.write(s, now, 1 << 20);
        ASSERT_TRUE(o.messages.size() == 1);
        const auto p = r.read(o.messages[0]);
        ASSERT_TRUE(p && p->snapshot.entities.size() == 1 && p->snapshot.entities[0].structure());
        EXPECT_EQ(p->snapshot.entities[0].structure()->hits, int64_t(tick * 3));
        deltas += o.deltas;
        w.ack(r.ack(), now);
    }
    EXPECT_TRUE(deltas >= 10);
}
