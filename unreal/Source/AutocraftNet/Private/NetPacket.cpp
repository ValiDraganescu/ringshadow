#include "NetPacket.h"

namespace ac::net {

namespace {

constexpr uint8_t kAckFlag = 0x80;

} // namespace

void writeHeader(BitWriter& w, const PacketHeader& h) {
    w.writeU32(h.protocolId);
    w.writeU16(h.version);
    w.writeU8(static_cast<uint8_t>(static_cast<uint8_t>(h.type) | (h.hasAck ? kAckFlag : 0)));
    w.writeU32(h.connection);
    w.writeU16(h.sequence);
    w.writeU16(h.ack);
    w.writeU32(h.ackBits);
}

bool readHeader(BitReader& r, PacketHeader& h) {
    if (r.bitsLeft() < kHeaderBytes * 8) return false;
    h.protocolId = r.readU32();
    h.version = r.readU16();
    const uint8_t type = r.readU8();
    h.hasAck = (type & kAckFlag) != 0;
    const uint8_t kind = type & ~kAckFlag;
    h.connection = r.readU32();
    h.sequence = r.readU16();
    h.ack = r.readU16();
    h.ackBits = r.readU32();
    if (!r.ok() || h.protocolId != kProtocolId) return false;
    if (kind < uint8_t(PacketType::hello) || kind > uint8_t(PacketType::disconnect)) return false;
    h.type = static_cast<PacketType>(kind);
    return true;
}

ReceiveWindow::Arrival ReceiveWindow::record(uint16_t sequence) {
    if (!any) {
        any = true;
        newest = sequence;
        bits = 0;
        return Arrival::newest;
    }
    if (sequence == newest) return Arrival::duplicate;
    if (sequenceNewer(sequence, newest)) {
        const uint16_t shift = static_cast<uint16_t>(sequence - newest);
        uint64_t b = bits;
        b = shift > 32 ? 0 : ((b << shift) | (uint64_t(1) << (shift - 1)));
        bits = static_cast<uint32_t>(b);
        newest = sequence;
        return Arrival::newest;
    }
    const uint16_t back = static_cast<uint16_t>(newest - sequence);
    if (back > 32) return Arrival::tooOld;
    const uint32_t mask = uint32_t(1) << (back - 1);
    if (bits & mask) return Arrival::duplicate;
    bits |= mask;
    return Arrival::older;
}

bool ReceiveWindow::has(uint16_t sequence) const {
    if (!any) return false;
    if (sequence == newest) return true;
    if (sequenceNewer(sequence, newest)) return false;
    const uint16_t back = static_cast<uint16_t>(newest - sequence);
    return back <= 32 && (bits & (uint32_t(1) << (back - 1))) != 0;
}

} // namespace ac::net
