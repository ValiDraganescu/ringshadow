// The three channels through the rough lag (rig.h: 5% loss, duplicates,
// reordering, jitter): reliable in order exactly once, unreliable newest
// only, bulk whole; and the acks across the sequence wrap.
#include "test.h"

#include "rig.h"

using namespace nettest;

namespace {

/// The u32 counters a list of messages carries.
std::vector<uint32_t> counters(const std::vector<std::vector<uint8_t>>& messages) {
    std::vector<uint32_t> out;
    for (const auto& m : messages) out.push_back(bytesU32(m));
    return out;
}

std::vector<uint8_t> pattern(size_t size, uint32_t seed) {
    std::vector<uint8_t> out(size);
    Random random(seed);
    for (uint8_t& b : out) b = static_cast<uint8_t>(random.next());
    return out;
}

} // namespace

TEST(channel_reliableInOrderExactlyOnceUnderLoss) {
    Rig rig(roughLag());
    ASSERT_TRUE(rig.connect());
    const uint32_t count = 3000;
    uint32_t sentUp = 0, sentDown = 0;
    for (int tick = 0; tick < 60 * 30; tick++) {
        rig.step([&] {
            for (int i = 0; i < 10 && sentUp < count; i++) {
                const auto m = u32Bytes(sentUp++);
                EXPECT_TRUE(rig.client.send(Channel::reliable, m.data(), m.size()));
            }
            for (int i = 0; i < 10 && sentDown < count; i++) {
                const auto m = u32Bytes(sentDown++);
                EXPECT_TRUE(rig.server.send(rig.serverConnection, Channel::reliable, m.data(), m.size()));
            }
        });
    }
    const auto up = counters(Rig::messages(rig.serverEvents, Channel::reliable));
    const auto down = counters(Rig::messages(rig.clientEvents, Channel::reliable));
    ASSERT_TRUE(up.size() == count);
    ASSERT_TRUE(down.size() == count);
    int wrong = 0;
    for (uint32_t i = 0; i < count; i++) wrong += (up[i] != i || down[i] != i) ? 1 : 0;
    EXPECT_EQ(wrong, 0);
    // The lag did its work: packets were lost, and resent.
    EXPECT_TRUE(rig.clientLag.lost() > 20);
    EXPECT_TRUE(rig.client.stats()->resends > 0);
}

TEST(channel_unreliableNewestWinsStaleDropped) {
    Rig rig(roughLag());
    ASSERT_TRUE(rig.connect());
    uint32_t tick = 0;
    for (int i = 0; i < 60 * 20; i++) {
        rig.step([&] {
            const auto m = u32Bytes(tick++);
            rig.client.send(Channel::unreliable, m.data(), m.size());
        });
    }
    const auto got = counters(Rig::messages(rig.serverEvents, Channel::unreliable));
    int backwards = 0;
    for (size_t i = 1; i < got.size(); i++) backwards += got[i] <= got[i - 1] ? 1 : 0;
    EXPECT_EQ(backwards, 0);  // never an older one, never one twice
    // Most arrive: lost and overtaken ones are gone, not waited for.
    // Jitter of 30 ms over 16 ms ticks overtakes a lot: about 2 in 3 arrive
    // newest. Overtaken and lost ones are gone, not waited for.
    EXPECT_TRUE(got.size() > tick / 2);
    EXPECT_TRUE(got.size() < tick);

    // With loss alone, all but the lost arrive.
    LagSettings lossOnly;
    lossOnly.delay = 0.05;
    lossOnly.loss = 0.05;
    Rig plain(lossOnly);
    ASSERT_TRUE(plain.connect());
    uint32_t sent = 0;
    for (int i = 0; i < 60 * 20; i++) {
        plain.step([&] {
            const auto m = u32Bytes(sent++);
            plain.client.send(Channel::unreliable, m.data(), m.size());
        });
    }
    const auto arrived = counters(Rig::messages(plain.serverEvents, Channel::unreliable));
    EXPECT_TRUE(arrived.size() > sent * 9 / 10);
    for (size_t i = 1; i < arrived.size(); i++) EXPECT_TRUE(arrived[i] > arrived[i - 1]);
}

TEST(channel_bulkHundredKilobytesUnderLoss) {
    Rig rig(roughLag());
    ASSERT_TRUE(rig.connect());
    const auto big = pattern(100 * 1024, 7);
    const auto second = pattern(30000, 8);
    const auto empty = std::vector<uint8_t>();
    ASSERT_TRUE(rig.server.send(rig.serverConnection, Channel::bulk, big.data(), big.size()));
    ASSERT_TRUE(rig.server.send(rig.serverConnection, Channel::bulk, empty.data(), 0));
    ASSERT_TRUE(rig.server.send(rig.serverConnection, Channel::bulk, second.data(), second.size()));
    // And the other channels keep working beside it.
    uint32_t reliableSent = 0;
    for (int tick = 0; tick < 60 * 10; tick++) {
        rig.step([&] {
            if (reliableSent < 100) {
                const auto m = u32Bytes(reliableSent++);
                rig.server.send(rig.serverConnection, Channel::reliable, m.data(), m.size());
            }
        });
    }
    const auto bulk = Rig::messages(rig.clientEvents, Channel::bulk);
    ASSERT_TRUE(bulk.size() == 3);
    EXPECT_TRUE(bulk[0] == big);
    EXPECT_TRUE(bulk[1].empty());
    EXPECT_TRUE(bulk[2] == second);
    const auto reliable = counters(Rig::messages(rig.clientEvents, Channel::reliable));
    ASSERT_TRUE(reliable.size() == 100);
    for (uint32_t i = 0; i < 100; i++) EXPECT_EQ(reliable[i], i);
    EXPECT_TRUE(rig.server.stats(rig.serverConnection)->resends > 0);
}

TEST(channel_messagesTooBigForTheirChannelAreRefused) {
    Rig rig;
    ASSERT_TRUE(rig.connect());
    std::vector<uint8_t> m(kMaxReliableBytes + 1);
    EXPECT_TRUE(!rig.client.send(Channel::reliable, m.data(), m.size()));
    EXPECT_TRUE(rig.client.send(Channel::reliable, m.data(), kMaxReliableBytes));
    m.resize(kMaxUnreliableBytes + 1);
    EXPECT_TRUE(!rig.client.send(Channel::unreliable, m.data(), m.size()));
    EXPECT_TRUE(rig.client.send(Channel::unreliable, m.data(), kMaxUnreliableBytes));
    for (int i = 0; i < 30; i++) rig.step();
    EXPECT_EQ(Rig::messages(rig.serverEvents, Channel::reliable).size(), size_t(1));
    EXPECT_EQ(Rig::messages(rig.serverEvents, Channel::unreliable).size(), size_t(1));
}

TEST(channel_acksAndReliableIdsAcrossTheWrap) {
    // More than 65,536 packets and reliable messages each way, so both the
    // packet sequence and the message ids wrap, with loss.
    LagSettings lag;
    lag.loss = 0.05;
    lag.reorder = 0.05;
    lag.reorderHold = 1.0 / 60;
    Rig rig(lag);
    ASSERT_TRUE(rig.connect());
    const uint32_t count = 70000;
    uint32_t sent = 0;
    while (sent < count) {
        rig.step([&] {
            const auto m = u32Bytes(sent++);
            rig.client.send(Channel::reliable, m.data(), m.size());
        });
    }
    for (int i = 0; i < 120; i++) rig.step();
    const auto got = counters(Rig::messages(rig.serverEvents, Channel::reliable));
    ASSERT_TRUE(got.size() == count);
    int wrong = 0;
    for (uint32_t i = 0; i < count; i++) wrong += got[i] != i ? 1 : 0;
    EXPECT_EQ(wrong, 0);
    EXPECT_TRUE(rig.client.stats()->packetsSent > 65536);
    EXPECT_EQ(rig.client.state(), ClientEndpoint::State::connected);
}
