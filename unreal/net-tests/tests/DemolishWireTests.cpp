// Demolish on the wire (C3, protocol 40, docs/guns-and-practice.md "Limits
// per region and demolish"): the Demolish request (index 25 of the 5-bit
// index) and the Demolished event (case 22).

#include "test.h"

#include "NetCodec.h"
#include "NetPacket.h"
#include "NetProtocol.h"

#include <variant>

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(demolish_protocol_is_40) {
    EXPECT_TRUE(kProtocolVersion >= uint16_t(40));
    EXPECT_TRUE(std::variant_size_v<decltype(RequestMessage::what)> <= size_t(32));
    RequestMessage m;
    m.what = RequestMessage::Demolish{1};
    EXPECT_EQ(m.what.index(), size_t(25));
    GameEvent e(GameEvent::Demolished{1, 2, 3});
    EXPECT_EQ(e.index(), size_t(22));
}

TEST(demolish_request_round_trips) {
    for (const int64_t id : {int64_t(0), int64_t(7), int64_t(4097), int64_t(1) << 40}) {
        RequestMessage m;
        m.id = 91;
        m.what = RequestMessage::Demolish{id};
        RequestMessage back;
        ASSERT_TRUE(decode(encode(m), back));
        EXPECT_TRUE(back == m);
    }
}

TEST(demolished_event_round_trips) {
    using G = GameEvent;
    const std::vector<GameEvent> all{G(G::Demolished{412, 62, 12}), G(G::Destroyed{412, StructureKind::gatling, 0, Vec2(10.5, 20.25)}),
                                     G(G::Demolished{9, 5, 0})};
    const auto messages = encodeEvents(3, all);
    ASSERT_TRUE(messages.size() == 1);
    EventMessage m;
    ASSERT_TRUE(decode(messages[0], m));
    ASSERT_TRUE(m.events.size() == all.size());
    for (size_t i = 0; i < all.size(); i++) EXPECT_TRUE(m.events[i] == all[i]);
}
