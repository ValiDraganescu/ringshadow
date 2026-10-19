// The walk-in Dropship on the wire (C4, protocol 42,
// docs/haul-and-ferry.md section 3): the CallShuttle and Ramp requests
// (indices 28 and 29 of the 5-bit index, after the Mule's 26 and 27), and
// the snapshot's shuttles, whole in every message's header.

#include "test.h"

#include "NetCodec.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"
#include "Shuttle.h"

#include <cmath>
#include <variant>

namespace {

using namespace ac;
using namespace ac::net;

Shuttle aShip() {
    Shuttle s;
    s.id = 3;
    s.position = Vec2(120.25, 87.5);
    s.heading = 2.1;
    s.ground = 3.42;
    s.stage = Shuttle::Stage::dash;
    s.clock = 1.375;
    s.ramp = Door{0.25, false};
    s.plan = Ferry::plan(4, -0.2, 0.3);
    s.from = Vec3(120.25, 87.5, 3.42);
    s.to = Vec3(-410.5, 1240.75, 11.25);
    s.fromHeading = 2.1;
    s.hop = false;
    s.riders = {41, 7, 4097};
    s.idle = 0.5;
    s.boarded = true;
    s.leaving = false;
    return s;
}

bool near(double a, double b, double by) { return std::abs(a - b) <= by; }

} // namespace

TEST(shuttle_protocol_is_42) {
    EXPECT_TRUE(kProtocolVersion >= uint16_t(42));
    EXPECT_TRUE(std::variant_size_v<decltype(RequestMessage::what)> <= size_t(32));
    RequestMessage m;
    m.what = RequestMessage::CallShuttle{};
    EXPECT_EQ(m.what.index(), size_t(28));
    m.what = RequestMessage::Ramp{true};
    EXPECT_EQ(m.what.index(), size_t(29));
}

TEST(shuttle_requests_round_trip) {
    RequestMessage call;
    call.id = 77;
    call.what = RequestMessage::CallShuttle{};
    RequestMessage back;
    ASSERT_TRUE(decode(encode(call), back));
    EXPECT_TRUE(back == call);
    for (const bool open : {true, false}) {
        RequestMessage m;
        m.id = 78;
        m.what = RequestMessage::Ramp{open};
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
    // The take-off is the Fly request, as before.
    RequestMessage fly;
    fly.id = 79;
    fly.what = RequestMessage::Fly{"cold-fork", "Ridge Pad"};
    ASSERT_TRUE(decode(encode(fly), back));
    EXPECT_TRUE(back == fly);
    // The Mule's are before them (protocol 41).
    RequestMessage mule;
    mule.what = RequestMessage::MuleFill{5};
    EXPECT_EQ(mule.what.index(), size_t(27));
}

TEST(shuttle_rides_every_snapshot_message) {
    Snapshot s;
    s.serverTick = 9;
    s.shuttles = {aShip(), aShip()};
    s.shuttles[1].id = 4;
    s.shuttles[1].stage = Shuttle::Stage::ground;
    s.shuttles[1].ramp = Door{1, true};
    s.shuttles[1].riders.clear();
    // Enough units to need several messages: each carries the ships.
    for (int64_t i = 0; i < 400; i++) s.entities.push_back(EntityState{Unit(100 + i, UnitKind::prospector, 0, Vec2(double(i), 2), 0, Unit::Task::idle)});
    SnapshotWriter w;
    SnapshotReader r;
    const auto o = w.write(s, 0, 1 << 20);
    ASSERT_TRUE(o.messages.size() >= 2);
    for (const auto& message : o.messages) {
        const auto p = r.read(message);
        ASSERT_TRUE(p.has_value());
        ASSERT_TRUE(p->snapshot.shuttles.size() == 2);
        const Shuttle& a = p->snapshot.shuttles[0];
        const Shuttle want = s.shuttles[0];
        EXPECT_TRUE(a == quantised(want));
        EXPECT_EQ(a.id, want.id);
        EXPECT_TRUE(a.stage == Shuttle::Stage::dash);
        EXPECT_TRUE(a.riders == want.riders);
        EXPECT_TRUE(near(a.position.x, want.position.x, 0.006) && near(a.position.y, want.position.y, 0.006));
        EXPECT_TRUE(near(a.heading, want.heading, 1e-3));
        EXPECT_TRUE(near(a.clock, want.clock, 1e-3));
        EXPECT_TRUE(near(a.plan.dash, want.plan.dash, 1e-3) && near(a.plan.approach, want.plan.approach, 1e-3));
        EXPECT_TRUE(near(a.to.x, want.to.x, 0.02) && near(a.to.y, want.to.y, 0.02) && near(a.to.z, want.to.z, 0.02));
        EXPECT_TRUE(near(a.ramp.open, 0.25, 1e-4) && !a.ramp.opening);
        EXPECT_TRUE(a.boarded && !a.hop);
        // The pose the client draws is the server's, near enough.
        const ShuttlePose there = shuttlePose(want), here = shuttlePose(a);
        EXPECT_TRUE(near(there.at.x, here.at.x, 0.05) && near(there.at.y, here.at.y, 0.05) && near(there.at.z, here.at.z, 0.05));
        EXPECT_TRUE(near(there.yaw, here.yaw, 0.01));
        EXPECT_TRUE(p->snapshot.shuttles[1].grounded() && p->snapshot.shuttles[1].ramp.open == 1.0);
    }
    // None: none.
    Snapshot empty;
    empty.serverTick = 10;
    const auto e = w.write(empty, 0.1, 1 << 20);
    ASSERT_TRUE(e.messages.size() == 1);
    const auto pe = r.read(e.messages[0]);
    ASSERT_TRUE(pe.has_value());
    EXPECT_TRUE(pe->snapshot.shuttles.empty());
}
