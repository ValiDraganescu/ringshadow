// The Citadel's interior on the wire (C1, protocol 36,
// docs/citadel-interior.md section 9): the Door and Man requests, the
// input's pitch, a Citadel's door and its guns' pitch and gunner.

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

constexpr double kQuarter = 3.141592653589793 / 2;

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

} // namespace

TEST(citadel_door_and_man_requests_round_trip) {
    EXPECT_TRUE(kProtocolVersion >= uint16_t(36));
    for (bool open : {true, false}) {
        RequestMessage m;
        m.id = 31;
        m.what = RequestMessage::Door{open};
        EXPECT_EQ(m.what.index(), size_t(22));
        RequestMessage back;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
    for (int gun = -1; gun < 4; gun++) {
        RequestMessage m;
        m.id = 32;
        m.what = RequestMessage::Man{int8_t(gun)};
        EXPECT_EQ(m.what.index(), size_t(23));
        RequestMessage back;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
    // A gun past the four goes as the last; one below -1 as -1.
    RequestMessage wild;
    wild.what = RequestMessage::Man{int8_t(9)};
    RequestMessage back;
    ASSERT_TRUE(decode(encode(wild), back));
    EXPECT_EQ(int(std::get<RequestMessage::Man>(back.what).gun), 3);
    wild.what = RequestMessage::Man{int8_t(-7)};
    ASSERT_TRUE(decode(encode(wild), back));
    EXPECT_EQ(int(std::get<RequestMessage::Man>(back.what).gun), -1);
}

TEST(citadel_widths_hold_the_new_cases) {
    // The request's index in 5 bits; the unit's task in 5 (atGun the 19th).
    EXPECT_TRUE(std::variant_size_v<decltype(RequestMessage::what)> <= size_t(32));
    EXPECT_TRUE(std::variant_size_v<decltype(RequestMessage::what)> >= size_t(24));
    EXPECT_EQ(EnumInfo<Unit::Task>::names.size(), size_t(19));
    EXPECT_TRUE((size_t(1) << codec::kTaskBits) >= EnumInfo<Unit::Task>::names.size());
    EXPECT_TRUE(allCases<Unit::Task>().back() == Unit::Task::atGun);
    Unit u(5, UnitKind::prospector, 0, Vec2(3, 4), 1.0, Unit::Task::atGun, 0, 0, 0, 0);
    u.structure = 9;
    Snapshot s;
    s.entities.push_back(EntityState{u});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    ASSERT_TRUE(o.messages.size() == 1);
    const auto p = r.read(o.messages[0]);
    ASSERT_TRUE(p && p->snapshot.entities.size() == 1 && p->snapshot.entities[0].unit());
    EXPECT_TRUE(p->snapshot.entities[0].unit()->task == Unit::Task::atGun);
    // The pitch: 8 bits over ±90°, 0 and the ends exact, 254 the top code.
    EXPECT_EQ(q::kPitchBits, 8);
    EXPECT_EQ(int(q::pitch(0.0)), 127);
    EXPECT_EQ(q::pitch(q::pitch(0.0)), 0.0);
    EXPECT_EQ(int(q::pitch(kQuarter)), 254);
    EXPECT_EQ(int(q::pitch(-kQuarter)), 0);
    EXPECT_EQ(q::pitch(uint8_t(254)), kQuarter);
    EXPECT_EQ(q::pitch(uint8_t(0)), -kQuarter);
    // Past the ends and not a number: clamped, level.
    EXPECT_EQ(int(q::pitch(3.0)), 254);
    EXPECT_EQ(int(q::pitch(-3.0)), 0);
    EXPECT_EQ(int(q::pitch(std::nan(""))), 127);
    EXPECT_EQ(q::pitch(uint8_t(255)), kQuarter);
}

TEST(citadel_input_carries_the_pitch) {
    InputMessage m;
    for (double pitch : {0.0, 0.87, -0.3, -0.349}) {
        PilotInput in;
        // Newest first, as the client sends them.
        in.tick = 100 - int64_t(m.inputs.size());
        in.heading = 2.0;
        in.pitch = pitch;
        m.inputs.push_back(in);
    }
    InputMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    ASSERT_TRUE(back.inputs.size() == m.inputs.size());
    for (size_t i = 0; i < m.inputs.size(); i++) {
        EXPECT_TRUE(back.inputs[i] == quantised(m.inputs[i]));
        EXPECT_NEAR(back.inputs[i].pitch, m.inputs[i].pitch, kQuarter / q::kPitchSteps / 2 + 1e-12);
    }
    EXPECT_EQ(back.inputs[0].pitch, 0.0);
}

TEST(citadel_door_and_guns_in_the_snapshot) {
    Structure c(3, StructureKind::citadel, 0, Vec2(10, 10));
    c.hp = 1500;
    c.guns = std::array<CornerGun, 4>{};
    for (int i = 0; i < 4; i++) {
        CornerGun& g = (*c.guns)[size_t(i)];
        g.aim = 0.4 * i;
        g.pitch = -0.3 + 0.25 * i;
        g.target = 70 + i;
        g.cooldown = 0.2;
    }
    (*c.guns)[1].gunner = 41;
    (*c.guns)[3].gunner = 1234567;
    // No door: an unfinished Citadel, an old save.
    auto back = roundTrip(c);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(!back->door.has_value());
    ASSERT_TRUE(back->guns.has_value());
    for (int i = 0; i < 4; i++) {
        const CornerGun& g = (*back->guns)[size_t(i)];
        EXPECT_NEAR(g.pitch, (*c.guns)[size_t(i)].pitch, kQuarter / q::kPitchSteps / 2 + 1e-12);
        EXPECT_TRUE(g.gunner == (*c.guns)[size_t(i)].gunner);
        // Its targets and reloads stay on the server.
        EXPECT_TRUE(!g.target.has_value());
        EXPECT_EQ(g.cooldown, 0.0);
    }
    EXPECT_TRUE(*back == quantised(c));
    // The door: closed, half way and opening, open; 255ths.
    for (const Door d : {Door{0, false}, Door{0.5, true}, Door{1, false}, Door{0.951, true}}) {
        c.door = d;
        back = roundTrip(c);
        ASSERT_TRUE(back.has_value() && back->door.has_value());
        EXPECT_NEAR(back->door->open, d.open, 0.5 / 255 + 1e-12);
        EXPECT_EQ(back->door->opening, d.opening);
        EXPECT_TRUE(*back == quantised(c));
    }
    c.door = Door{1, false};
    EXPECT_EQ(roundTrip(c)->door->open, 1.0);
    c.door = Door{0, true};
    EXPECT_EQ(roundTrip(c)->door->open, 0.0);
}

TEST(citadel_door_moves_as_a_delta) {
    Structure c(3, StructureKind::citadel, 0, Vec2(10, 10));
    c.hp = 1500;
    c.door = Door{0, false};
    SnapshotWriter w;
    SnapshotReader r;
    Snapshot s;
    s.entities.push_back(EntityState{c});
    size_t deltas = 0;
    // Each tick the door moves (an unchanged acked item is not written).
    for (int tick = 0; tick <= 10; tick++) {
        c.door = Door{std::min(1.0, tick / 10.0), tick < 10};
        s.entities[0] = EntityState{c};
        const double now = tick / 20.0;
        const auto o = w.write(s, now, 1 << 20);
        ASSERT_TRUE(o.messages.size() == 1);
        const auto p = r.read(o.messages[0]);
        ASSERT_TRUE(p && p->snapshot.entities.size() == 1 && p->snapshot.entities[0].structure());
        const Structure* got = p->snapshot.entities[0].structure();
        ASSERT_TRUE(got->door.has_value());
        EXPECT_NEAR(got->door->open, std::min(1.0, tick / 10.0), 0.5 / 255 + 1e-12);
        EXPECT_EQ(got->door->opening, tick < 10);
        deltas += o.deltas;
        w.ack(r.ack(), now);
    }
    // After the first, the door went as a delta against the acked state.
    EXPECT_TRUE(deltas >= 10);
}
