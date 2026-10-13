// The send budget: never more than bytes/s over time, reliable first, the
// rest reported for the caller's unreliable data.
#include "test.h"

#include "rig.h"

using namespace nettest;

namespace {

ServerConfig budgetServer(double bytesPerSecond) {
    ServerConfig c = serverConfig();
    c.connection.bytesPerSecond = bytesPerSecond;
    return c;
}

} // namespace

TEST(budget_neverMoreThanTheRate) {
    const double rate = 20000;
    Rig rig({}, budgetServer(rate));
    ASSERT_TRUE(rig.connect());
    const ConnectionId id = rig.serverConnection;
    // Far more than the budget: bulk, reliable, and unreliable sized to what is left.
    std::vector<uint8_t> big(300 * 1024, 0x5A);
    rig.server.send(id, Channel::bulk, big.data(), big.size());
    const uint64_t start = rig.server.stats(id)->bytesSent;
    const double from = rig.now;
    int64_t leftSeen = 0;
    for (int tick = 0; tick < 60 * 5; tick++) {
        rig.step([&] {
            std::vector<uint8_t> r(200, 1);
            rig.server.send(id, Channel::reliable, r.data(), r.size());
            const int64_t left = rig.server.budgetLeft(id, rig.now);
            leftSeen = std::max(leftSeen, left);
            if (left > int64_t(kPacketOverheadBytes + 3)) {
                std::vector<uint8_t> u(std::min<size_t>(size_t(left) - kPacketOverheadBytes - 3, kMaxUnreliableBytes), 2);
                rig.server.send(id, Channel::unreliable, u.data(), u.size());
            }
        });
    }
    const double seconds = rig.now - from;
    const double sent = double(rig.server.stats(id)->bytesSent - start);
    const double burst = std::max(rate * 0.1, double(kMaxPacketBytes));
    EXPECT_TRUE(sent <= rate * seconds + burst);
    EXPECT_TRUE(sent >= rate * seconds * 0.9);  // and it is used
    // Bulk took the budget after the reliable messages: never a whole
    // packet's worth was left over.
    EXPECT_TRUE(leftSeen < int64_t(kMaxPacketBytes));
}

TEST(budget_reliableGoesFirstAndTheRestIsReported) {
    const double rate = 12000;  // 200 bytes a tick, a burst of 1,200
    Rig rig({}, budgetServer(rate));
    ASSERT_TRUE(rig.connect());
    const ConnectionId id = rig.serverConnection;
    for (int i = 0; i < 120; i++) rig.step();  // saved up to the burst

    const int64_t full = rig.server.budgetLeft(id, rig.now);
    EXPECT_EQ(full, int64_t(kMaxPacketBytes));
    std::vector<uint8_t> r(500, 1);
    rig.server.send(id, Channel::reliable, r.data(), r.size());
    // A packet header and tag and the message's own five bytes come out of it too.
    EXPECT_EQ(rig.server.budgetLeft(id, rig.now), full - int64_t(kPacketOverheadBytes + 5 + 500));

    // An unreliable message bigger than what is left is dropped; the
    // reliable one goes.
    std::vector<uint8_t> u(1000, 2);
    rig.server.send(id, Channel::unreliable, u.data(), u.size());
    rig.server.flush(rig.now);
    EXPECT_EQ(rig.server.stats(id)->unreliableDropped, uint64_t(1));
    for (int i = 0; i < 10; i++) rig.step();
    EXPECT_EQ(Rig::messages(rig.clientEvents, Channel::reliable).size(), size_t(1));
    EXPECT_EQ(Rig::messages(rig.clientEvents, Channel::unreliable).size(), size_t(0));

    // One that fits what is reported goes.
    for (int i = 0; i < 120; i++) rig.step();
    const int64_t left = rig.server.budgetLeft(id, rig.now);
    std::vector<uint8_t> fits(size_t(left) - kPacketOverheadBytes - 3, 3);
    rig.server.send(id, Channel::unreliable, fits.data(), fits.size());
    rig.server.flush(rig.now);
    EXPECT_EQ(rig.server.stats(id)->unreliableDropped, uint64_t(1));
    for (int i = 0; i < 10; i++) rig.step();
    EXPECT_EQ(Rig::messages(rig.clientEvents, Channel::unreliable).size(), size_t(1));
}
