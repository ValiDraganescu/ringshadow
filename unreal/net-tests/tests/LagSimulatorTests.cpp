// The lag simulator: delay, jitter, loss, duplicates and reordering, per
// remote, the same every run for a seed.
#include "test.h"

#include "rig.h"

using namespace nettest;

namespace {

/// What arrives at B, in order, when A sends `count` numbered packets one
/// per 10 ms through a simulator with `settings`, and when.
struct Arrivals {
    std::vector<uint32_t> numbers;
    std::vector<double> times;
    uint64_t lost = 0;
};

Arrivals run(const LagSettings& settings, uint64_t seed, int count) {
    MemoryNet net;
    MemoryPort a(net, kClientAddress), b(net, kServerAddress);
    LagSimulator lag(a, seed);
    lag.set(kServerAddress, settings);
    Arrivals out;
    double now = 0;
    for (int i = 0; i < count + 200; i++) {
        now += 0.01;
        if (i < count) {
            const auto m = u32Bytes(uint32_t(i));
            lag.send(now, kServerAddress, m.data(), m.size());
        }
        lag.pump(now);
        uint8_t buf[64];
        Address from;
        size_t size = 0;
        while (b.receive(now, from, buf, sizeof buf, size)) {
            out.numbers.push_back(bytesU32(std::vector<uint8_t>(buf, buf + size)));
            out.times.push_back(now);
        }
    }
    out.lost = lag.lost();
    return out;
}

} // namespace

TEST(lag_delaysByTheSetTime) {
    LagSettings s;
    s.delay = 0.1;
    const Arrivals a = run(s, 1, 100);
    ASSERT_TRUE(a.numbers.size() == 100);
    for (size_t i = 0; i < 100; i++) {
        EXPECT_EQ(a.numbers[i], uint32_t(i));
        EXPECT_NEAR(a.times[i] - 0.01 * double(i + 1), 0.1, 0.0101);
    }
}

TEST(lag_lossDuplicatesAndReorderingAtTheirRates) {
    LagSettings s;
    s.delay = 0.02;
    s.jitter = 0.005;
    s.loss = 0.2;
    s.duplicate = 0.1;
    s.reorder = 0.1;
    s.reorderHold = 0.05;
    const int n = 10000;
    const Arrivals a = run(s, 42, n);
    EXPECT_NEAR(double(a.lost) / n, 0.2, 0.02);
    std::vector<int> seen(n, 0);
    int outOfOrder = 0;
    for (size_t i = 0; i < a.numbers.size(); i++) {
        seen[a.numbers[i]] += 1;
        if (i > 0 && a.numbers[i] < a.numbers[i - 1]) outOfOrder += 1;
    }
    int twice = 0;
    for (int c : seen) twice += c == 2 ? 1 : 0;
    EXPECT_NEAR(double(twice) / (n * 0.8), 0.1, 0.02);
    EXPECT_TRUE(outOfOrder > n / 50);
}

TEST(lag_sameSeedSamePackets) {
    const LagSettings s = roughLag();
    const Arrivals a = run(s, 99, 2000), b = run(s, 99, 2000), c = run(s, 100, 2000);
    EXPECT_TRUE(a.numbers == b.numbers);
    EXPECT_TRUE(a.times == b.times);
    EXPECT_TRUE(a.numbers != c.numbers);
}

TEST(lag_settingsPerRemoteAndBothWays) {
    MemoryNet net;
    MemoryPort self(net, kClientAddress), near(net, Address::ipv4(10, 0, 0, 3, 1)), far(net, Address::ipv4(10, 0, 0, 4, 1));
    LagSimulator lag(self, 5);
    LagSettings slow;
    slow.delay = 0.5;
    lag.set(far.self(), slow);
    const uint8_t byte = 1;
    lag.send(0, near.self(), &byte, 1);
    lag.send(0, far.self(), &byte, 1);
    lag.pump(0.1);
    uint8_t buf[8];
    Address from;
    size_t size;
    EXPECT_TRUE(near.receive(0.1, from, buf, sizeof buf, size));
    EXPECT_TRUE(!far.receive(0.1, from, buf, sizeof buf, size));
    lag.pump(0.5);
    EXPECT_TRUE(far.receive(0.5, from, buf, sizeof buf, size));
    // Incoming from the far one is held too.
    far.send(1.0, kClientAddress, &byte, 1);
    EXPECT_TRUE(!lag.receive(1.0, from, buf, sizeof buf, size));
    EXPECT_TRUE(!lag.receive(1.4, from, buf, sizeof buf, size));
    EXPECT_TRUE(lag.receive(1.5, from, buf, sizeof buf, size));
    EXPECT_TRUE(from == far.self());
}

TEST(lag_drainSendsWhatItHolds) {
    // A game closing under the lag setting: its held packets (the goodbye
    // among them) leave at once, not never.
    MemoryNet net;
    MemoryPort a(net, kClientAddress), b(net, kServerAddress);
    LagSimulator lag(a, 3);
    LagSettings s;
    s.delay = 0.15;
    lag.set(kServerAddress, s);
    for (uint32_t i = 0; i < 3; i++) {
        const auto m = u32Bytes(i);
        lag.send(1.0, kServerAddress, m.data(), m.size());
    }
    uint8_t buf[64];
    Address from;
    size_t size = 0;
    EXPECT_TRUE(!b.receive(1.0, from, buf, sizeof buf, size));
    lag.drain(1.0);
    std::vector<uint32_t> got;
    while (b.receive(1.0, from, buf, sizeof buf, size)) got.push_back(bytesU32(std::vector<uint8_t>(buf, buf + size)));
    ASSERT_TRUE(got.size() == 3);
    for (uint32_t i = 0; i < 3; i++) EXPECT_EQ(got[i], i);
}
