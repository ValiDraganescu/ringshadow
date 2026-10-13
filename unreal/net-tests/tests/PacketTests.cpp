// The packet header, the wrapping sequence compare, the ack window and the
// keyed hash.
#include "test.h"

#include "NetPacket.h"
#include "SipHash.h"

#include <set>

using namespace ac::net;

TEST(packet_headerRoundTripsInNineteenBytes) {
    PacketHeader h;
    h.version = 7;
    h.type = PacketType::data;
    h.hasAck = true;
    h.connection = 0xCAFEF00D;
    h.sequence = 65534;
    h.ack = 3;
    h.ackBits = 0x80000001;
    BitWriter w;
    writeHeader(w, h);
    EXPECT_EQ(w.byteCount(), kHeaderBytes);
    BitReader r(w.bytes().data(), w.byteCount());
    PacketHeader back;
    ASSERT_TRUE(readHeader(r, back));
    EXPECT_EQ(back.protocolId, kProtocolId);
    EXPECT_EQ(back.version, uint16_t(7));
    EXPECT_EQ(back.type, PacketType::data);
    EXPECT_EQ(back.hasAck, true);
    EXPECT_EQ(back.connection, uint32_t(0xCAFEF00D));
    EXPECT_EQ(back.sequence, uint16_t(65534));
    EXPECT_EQ(back.ack, uint16_t(3));
    EXPECT_EQ(back.ackBits, uint32_t(0x80000001));
}

TEST(packet_foreignOrShortPacketsAreRejected) {
    PacketHeader h;
    BitWriter w;
    writeHeader(w, h);
    std::vector<uint8_t> bytes = w.bytes();
    {
        BitReader r(bytes.data(), kHeaderBytes - 1);
        PacketHeader out;
        EXPECT_TRUE(!readHeader(r, out));
    }
    bytes[0] ^= 1;
    {
        BitReader r(bytes.data(), bytes.size());
        PacketHeader out;
        EXPECT_TRUE(!readHeader(r, out));
    }
    bytes[0] ^= 1;
    bytes[6] = 0x7F;  // no such type
    {
        BitReader r(bytes.data(), bytes.size());
        PacketHeader out;
        EXPECT_TRUE(!readHeader(r, out));
    }
}

TEST(packet_sequenceCompareWraps) {
    EXPECT_TRUE(sequenceNewer(1, 0));
    EXPECT_TRUE(!sequenceNewer(0, 1));
    EXPECT_TRUE(!sequenceNewer(5, 5));
    EXPECT_TRUE(sequenceNewer(0, 65535));
    EXPECT_TRUE(sequenceNewer(10, 65530));
    EXPECT_TRUE(!sequenceNewer(65530, 10));
    EXPECT_TRUE(sequenceNewer(32767, 0));
    EXPECT_TRUE(!sequenceNewer(32768, 0));
}

TEST(packet_ackWindowAcrossTheWrap) {
    ReceiveWindow w;
    EXPECT_TRUE(!w.has(0));
    EXPECT_EQ(w.record(65530), ReceiveWindow::Arrival::newest);
    EXPECT_EQ(w.record(65533), ReceiveWindow::Arrival::newest);
    EXPECT_EQ(w.record(2), ReceiveWindow::Arrival::newest);  // across the wrap
    EXPECT_EQ(w.record(65535), ReceiveWindow::Arrival::older);
    EXPECT_EQ(w.record(65535), ReceiveWindow::Arrival::duplicate);
    EXPECT_EQ(w.record(2), ReceiveWindow::Arrival::duplicate);
    EXPECT_EQ(w.record(65000), ReceiveWindow::Arrival::tooOld);
    EXPECT_EQ(w.newest, uint16_t(2));

    std::set<uint16_t> acked;
    forEachAcked(w.newest, w.bits, [&](uint16_t s) { acked.insert(s); });
    EXPECT_TRUE(acked == std::set<uint16_t>({65530, 65533, 65535, 2}));
    for (uint16_t s : {uint16_t(65530), uint16_t(65533), uint16_t(65535), uint16_t(2)}) EXPECT_TRUE(w.has(s));
    EXPECT_TRUE(!w.has(0));
    EXPECT_TRUE(!w.has(1));
    EXPECT_TRUE(!w.has(3));

    // 32 back is the last one the bits hold; a jump of 33 forgets all.
    ReceiveWindow v;
    v.record(100);
    v.record(132);
    EXPECT_TRUE(v.has(100));
    v.record(166);
    EXPECT_TRUE(!v.has(132));
    EXPECT_EQ(v.bits, uint32_t(0));
}

TEST(packet_ackWindowAcksEveryPacketOfALongRun) {
    // 70,000 packets, some late: every one is named by some ack once
    // received, across the wrap.
    ReceiveWindow w;
    std::vector<int> ackedTimes(70000, 0);
    std::vector<bool> got(70000, false);
    for (int i = 0; i < 70000; i++) {
        const int s = (i % 7 == 3 && i + 2 < 70000) ? i + 2 : (i % 7 == 5 ? i - 2 : i);  // swaps neighbours
        w.record(static_cast<uint16_t>(s));
        got[s] = true;
        if (i % 20 != 19) continue;
        // An ack every 20 packets covers the last 33: mark those seen.
        forEachAcked(w.newest, w.bits, [&](uint16_t q) {
            for (int back = 0; back <= 40; back++) {
                const int idx = i - back;
                if (idx >= 0 && static_cast<uint16_t>(idx) == q && got[idx]) ackedTimes[idx] = 1;
            }
        });
    }
    int missing = 0;
    for (int i = 0; i < 69960; i++) missing += ackedTimes[i] == 0 ? 1 : 0;
    EXPECT_EQ(missing, 0);
}

TEST(packet_sipHashMatchesTheReferenceVectors) {
    uint8_t key[16];
    for (int i = 0; i < 16; i++) key[i] = static_cast<uint8_t>(i);
    uint8_t msg[64];
    for (int i = 0; i < 64; i++) msg[i] = static_cast<uint8_t>(i);
    // From the SipHash paper's reference implementation (vectors.h).
    EXPECT_EQ(sipHash24(key, msg, 0), uint64_t(0x726fdb47dd0e0e31ULL));
    EXPECT_EQ(sipHash24(key, msg, 1), uint64_t(0x74f839c593dc67fdULL));
    EXPECT_EQ(sipHash24(key, msg, 8), uint64_t(0x93f5f5799a932462ULL));
    EXPECT_EQ(sipHash24(key, msg, 15), uint64_t(0xa129ca6149be45e5ULL));
}
