// The Mule on the wire (C4, protocol 41, docs/haul-and-ferry.md section 2):
// the BuyMule and MuleFill requests (indexes 26 and 27 of the 5-bit index),
// the Booked event (case 23), a Mule's haul on its unit in the snapshot, and
// the Progress message's mule.

#include "test.h"

#include "NetCodec.h"
#include "NetPacket.h"
#include "NetProtocol.h"
#include "NetSnapshot.h"

#include <variant>

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(mule_protocol_is_41) {
    EXPECT_TRUE(kProtocolVersion >= uint16_t(41));
    EXPECT_TRUE(std::variant_size_v<decltype(RequestMessage::what)> <= size_t(32));
    RequestMessage m;
    m.what = RequestMessage::BuyMule{1};
    EXPECT_EQ(m.what.index(), size_t(26));
    m.what = RequestMessage::MuleFill{1};
    EXPECT_EQ(m.what.index(), size_t(27));
    GameEvent e(GameEvent::Booked{1, 2, 3, Vec2(0, 0), 0, 0});
    EXPECT_EQ(e.index(), size_t(23));
    // Seventeen unit kinds: five bits a kind.
    EXPECT_EQ(codec::kUnitKindBits, 5);
}

TEST(mule_requests_round_trip) {
    for (const int64_t id : {int64_t(0), int64_t(7), int64_t(4097), int64_t(1) << 40}) {
        RequestMessage m;
        m.id = 91;
        m.what = RequestMessage::BuyMule{id};
        RequestMessage back;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
        m.what = RequestMessage::MuleFill{id + 1};
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
}

TEST(booked_event_round_trips) {
    using G = GameEvent;
    const std::vector<GameEvent> all{G(G::Booked{412, 980, 120, Vec2(50.5, 60.25), 98, 12}), G(G::Booked{9, 0, 4, Vec2(1, 2), 0, 0}),
                                     G(G::Trained{9, UnitKind::mule, Vec2(3.5, 7)})};
    const auto messages = encodeEvents(3, all);
    ASSERT_TRUE(messages.size() == 1);
    EventMessage m;
    ASSERT_TRUE(decode(messages[0], m));
    ASSERT_TRUE(m.events.size() == all.size());
    for (size_t i = 0; i < all.size(); i++) EXPECT_TRUE(m.events[i] == all[i]);
}

/// A Mule's load and rider ride on its unit; a change of load alone goes as
/// a delta and comes back.
TEST(mule_haul_rides_in_the_snapshot) {
    Unit u(5, UnitKind::mule, 0, Vec2(30.25, -12.5), 1.0, Unit::Task::idle);
    u.hp = 250;
    u.haul = Haul{640, 85};
    u.cargo = std::vector<int64_t>{4};
    Unit rider(4, UnitKind::prospector, 0, Vec2(30.25, -12.5), 1.0, Unit::Task::aboard);
    rider.hp = 45;
    rider.structure = 5;
    Snapshot s;
    s.entities.push_back(EntityState{u, false, Rank::driven});
    s.entities.push_back(EntityState{rider});
    s.driven = 5;
    SnapshotWriter w;
    SnapshotReader r;
    std::map<int64_t, Unit> got;
    for (const auto& m : w.write(s, 0, 1 << 20).messages)
        if (auto p = r.read(m))
            for (const EntityState& e : p->snapshot.entities) got[e.id()] = *e.unit();
    ASSERT_TRUE(got.count(5) && got.count(4));
    EXPECT_TRUE(got[5] == quantised(u));
    EXPECT_TRUE(got[5].haul == (Haul{640, 85}));
    EXPECT_TRUE(got[4].task == Unit::Task::aboard && got[4].structure == int64_t(5));
}

TEST(progress_mule_round_trips) {
    ProgressMessage m;
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(!back.mule);
    m.mule = ProgressMessage::Mule{"cold-fork", 0, 812, 1234.5};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back.mule == m.mule);
    m.mule = ProgressMessage::Mule{"start", 4410, 812, 0};
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(back == m);
}
