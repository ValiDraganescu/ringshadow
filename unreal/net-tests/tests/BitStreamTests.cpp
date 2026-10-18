// The bit writer and reader, and the quantisation helpers.
#include "test.h"

#include "NetBitStream.h"
#include "NetProtocol.h"

#include <cmath>
#include <cstdint>
#include <vector>

using namespace ac::net;

TEST(bitStream_roundTripsMixedWidths) {
    BitWriter w;
    w.writeBool(true);
    w.writeBits(5, 3);
    w.writeU16(0xBEEF);
    w.writeBits(0x1FFFF, 17);
    w.writeU64(0x0123456789ABCDEFULL);
    w.writeBool(false);
    w.writeU32(0xDEADBEEF);
    w.writeBits(0, 0);
    w.writeBits(0x3FFFFFFFFULL, 34);
    EXPECT_EQ(w.bitCount(), size_t(1 + 3 + 16 + 17 + 64 + 1 + 32 + 34));

    BitReader r(w.bytes().data(), w.byteCount());
    EXPECT_EQ(r.readBool(), true);
    EXPECT_EQ(r.readBits(3), uint64_t(5));
    EXPECT_EQ(r.readU16(), uint16_t(0xBEEF));
    EXPECT_EQ(r.readBits(17), uint64_t(0x1FFFF));
    EXPECT_EQ(r.readU64(), uint64_t(0x0123456789ABCDEFULL));
    EXPECT_EQ(r.readBool(), false);
    EXPECT_EQ(r.readU32(), uint32_t(0xDEADBEEF));
    EXPECT_EQ(r.readBits(34), uint64_t(0x3FFFFFFFFULL));
    EXPECT_TRUE(r.ok());
}

TEST(bitStream_bytesAlignedAndNot) {
    const uint8_t data[5] = {1, 2, 250, 251, 252};
    BitWriter w;
    w.writeBytes(data, 5);  // aligned
    w.writeBits(1, 1);
    w.writeBytes(data, 5);  // one bit off
    w.align();
    w.writeU8(77);
    BitReader r(w.bytes().data(), w.byteCount());
    uint8_t a[5] = {}, b[5] = {};
    EXPECT_TRUE(r.readBytes(a, 5));
    EXPECT_EQ(r.readBits(1), uint64_t(1));
    EXPECT_TRUE(r.readBytes(b, 5));
    r.align();
    EXPECT_EQ(r.readU8(), uint8_t(77));
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(a[i], data[i]);
        EXPECT_EQ(b[i], data[i]);
    }
    EXPECT_TRUE(r.ok());
}

TEST(bitStream_readingPastTheEndFails) {
    const uint8_t data[2] = {0xFF, 0xFF};
    BitReader r(data, 2);
    EXPECT_EQ(r.readBits(12), uint64_t(0xFFF));
    EXPECT_TRUE(r.ok());
    EXPECT_EQ(r.readBits(5), uint64_t(0));
    EXPECT_TRUE(!r.ok());
    EXPECT_EQ(r.readU8(), uint8_t(0));
    uint8_t out[4];
    EXPECT_TRUE(!r.readBytes(out, 4));
}

TEST(bitStream_intsInTheirRange) {
    EXPECT_EQ(bitsForRange(0), 0);
    EXPECT_EQ(bitsForRange(1), 1);
    EXPECT_EQ(bitsForRange(255), 8);
    EXPECT_EQ(bitsForRange(256), 9);
    BitWriter w;
    w.writeInt(-5, -10, 10);      // 5 bits
    w.writeInt(1000, 0, 100);     // clamped to 100
    w.writeInt(-7, -7, -7);       // no bits
    w.writeInt(123456789, -1000000000, 1000000000);
    EXPECT_EQ(w.bitCount(), size_t(5 + 7 + 0 + 31));
    BitReader r(w.bytes().data(), w.byteCount());
    EXPECT_EQ(r.readInt(-10, 10), int64_t(-5));
    EXPECT_EQ(r.readInt(0, 100), int64_t(100));
    EXPECT_EQ(r.readInt(-7, -7), int64_t(-7));
    EXPECT_EQ(r.readInt(-1000000000, 1000000000), int64_t(123456789));
    EXPECT_TRUE(r.ok());
}

TEST(bitStream_floatsInRangeAtNBits) {
    // 12 bits over 0...100: a step of 100/4095.
    const double step = 100.0 / 4095;
    for (float v : {0.0f, 0.01f, 33.3f, 50.0f, 99.99f, 100.0f}) {
        const float back = dequantizeFloat(quantizeFloat(v, 0, 100, 12), 0, 100, 12);
        EXPECT_NEAR(back, v, step / 2 + 1e-4);
    }
    EXPECT_EQ(quantizeFloat(-5, 0, 100, 12), uint32_t(0));
    EXPECT_EQ(quantizeFloat(500, 0, 100, 12), uint32_t(4095));
    EXPECT_EQ(quantizeFloat(NAN, 0, 100, 12), uint32_t(0));
    BitWriter w;
    w.writeFloat(-3.25f, -10, 10, 8);
    BitReader r(w.bytes().data(), w.byteCount());
    EXPECT_NEAR(r.readFloat(-10, 10, 8), -3.25, 20.0 / 255 / 2 + 1e-6);
}

TEST(bitStream_anglesSixteenBitsATurn) {
    const double turn = 6.283185307179586;
    const double step = turn / 65536;
    for (double a : {0.0, 0.5, 1.0, 3.14159, 6.0, -1.0, 12.5, -20.0}) {
        const double back = dequantizeAngle(quantizeAngle(float(a)));
        double diff = std::fmod(back - a, turn);
        if (diff > turn / 2) diff -= turn;
        if (diff < -turn / 2) diff += turn;
        EXPECT_NEAR(diff, 0.0, step / 2 + 1e-6);
        EXPECT_TRUE(back >= 0 && back < turn);
    }
    EXPECT_EQ(quantizeAngle(float(turn)), uint16_t(0));
    BitWriter w;
    w.writeAngle(1.5f);
    EXPECT_EQ(w.bitCount(), size_t(16));
}

// docs/perf.md "Server" round 6: the writer puts a value's bytes in one
// pass, the reader takes them so, and `appendBits` copies up to 56 bits at
// a time. Against a writer of one bit at a time: the same bytes for values
// of every width (0 to 64) at every bit offset, read back the same, and
// any stretch of them appended anywhere the same as its bits one by one.
TEST(bitStream_wordWritesAreTheBitsOneByOne) {
    uint64_t seed = 0x9e3779b97f4a7c15ull;
    auto next = [&] {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return seed;
    };
    for (int round = 0; round < 200; round++) {
        BitWriter w;
        std::vector<uint8_t> ref;
        size_t refBits = 0;
        auto refBit = [&](bool b) {
            if (refBits % 8 == 0) ref.push_back(0);
            if (b) ref.back() = static_cast<uint8_t>(ref.back() | (1u << (refBits % 8)));
            refBits++;
        };
        std::vector<std::pair<uint64_t, int>> written;
        for (int k = 0; k < 60; k++) {
            const int bits = static_cast<int>(next() % 65);
            const uint64_t value = next();
            w.writeBits(value, bits);
            for (int b = 0; b < bits; b++) refBit((value >> b) & 1);
            written.push_back({bits >= 64 ? value : value & ((uint64_t(1) << bits) - 1), bits});
        }
        EXPECT_EQ(w.bitCount(), refBits);
        EXPECT_TRUE(w.bytes() == ref);
        BitReader r(w.bytes().data(), w.bytes().size());
        bool same = true;
        for (const auto& [value, bits] : written) same = same && r.readBits(bits) == value;
        EXPECT_TRUE(same && r.ok());
        // A stretch appended after a few odd bits.
        const size_t from = next() % (refBits + 1), count = next() % (refBits - from + 1);
        const int lead = static_cast<int>(next() % 8);
        BitWriter a, b;
        a.writeBits(0x5a, lead);
        b.writeBits(0x5a, lead);
        appendBits(a, w.bytes().data(), from, count);
        for (size_t i = from; i < from + count; i++) b.writeBool((ref[i / 8] >> (i % 8)) & 1);
        EXPECT_TRUE(a.bytes() == b.bytes() && a.bitCount() == b.bitCount());
    }
}
