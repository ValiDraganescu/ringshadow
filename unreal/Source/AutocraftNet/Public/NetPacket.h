// The packet: at most 1,200 bytes, a 19-byte header on every one
// (docs/networking.md, "Packets"):
//
//   protocol id  u32   kProtocolId
//   version      u16   the sender's protocol version
//   type         u8    PacketType; the top bit says the ack fields are set
//   connection   u32   0 until the server's welcome names it
//   sequence     u16   this packet's number, wrapping
//   ack          u16   the newest sequence received from the other end
//   ack bits     u32   bit i: sequence ack-1-i received too
//
// After the handshake every packet is sealed (NetCrypto.h): the header stays
// readable and is signed with the payload, the payload is encrypted, and a
// 16-byte tag follows it (kMacBytes). The handshake's packets go in clear.
//
// The first 7 bytes, and the refuse packet that follows a full header, never
// change between versions: that is how a client of another version reads
// why it was refused.
#pragma once

#include "AutocraftNetApi.h"
#include "NetBitStream.h"

#include <cstddef>
#include <cstdint>

namespace ac::net {

inline constexpr uint32_t kProtocolId = 0x52534844;  // "RSHD"
inline constexpr uint16_t kProtocolVersion = 24;  // 2: inputs carry the visit; 3: encrypted; 4: the sky; 5: the skill tree; 6: sites; 7: contracts; 8: respawn, own pads; 9: starfall; 10: the rig; 11: the first ten minutes; 12: realm buildings; 13: the realm wallet; 14: the Citadel shop (a request index of 5 bits); 15: kept for the Brood, which came as 17; 16: Citadel missions; 17: the Brood (N2); 18: kept for the Citadel's upgrades, which came as 20; 19: mercenaries (H2); 20: the defences (D1, D2: the Citadel's marks and corner turrets, the gatling gun); 21: the regions seen (M2); 22: turret upgrades (D3: a placed turret's marks, air reach, kills); 23: the regions' ground palettes, seamless edges (W4); 24: the Matriarch (N3: her nest in the world map, her death's news)
inline constexpr size_t kMaxPacketBytes = 1200;
inline constexpr size_t kHeaderBytes = 19;
/// The tag after a sealed packet's payload.
inline constexpr size_t kMacBytes = 16;
/// What a sealed packet costs on top of its payload.
inline constexpr size_t kPacketOverheadBytes = kHeaderBytes + kMacBytes;
inline constexpr size_t kMaxPayloadBytes = kMaxPacketBytes - kPacketOverheadBytes;

enum class PacketType : uint8_t {
    hello = 1,       // client: padded to a full packet, so no reply is ever bigger
    challenge = 2,   // server: the cookie
    answer = 3,      // client: the cookie back and its public key, padded to a full packet
    welcome = 4,     // server: the connection id and its public key
    refuse = 5,      // server: why not (u8 length, text)
    data = 6,        // either, sealed: messages, or nothing (keep-alive, acks)
    disconnect = 7,  // either, sealed: why (u8 length, text)
    token = 8,       // client, sealed: the token (u8 length, bytes), until the server's first packet
};

struct PacketHeader {
    uint32_t protocolId = kProtocolId;
    uint16_t version = kProtocolVersion;
    PacketType type = PacketType::data;
    bool hasAck = false;
    uint32_t connection = 0;
    uint16_t sequence = 0;
    uint16_t ack = 0;
    uint32_t ackBits = 0;
};

AUTOCRAFTNET_API void writeHeader(BitWriter& w, const PacketHeader& h);
/// False if the bytes are too few or the protocol id or type is not ours.
/// The version is not checked here.
AUTOCRAFTNET_API bool readHeader(BitReader& r, PacketHeader& h);

/// `a` came after `b`, across the wrap from 65535 to 0.
constexpr bool sequenceNewer(uint16_t a, uint16_t b) { return static_cast<int16_t>(static_cast<uint16_t>(a - b)) > 0; }

/// The receiving side of the acks: the newest sequence received and which
/// of the 32 before it arrived.
struct AUTOCRAFTNET_API ReceiveWindow {
    enum class Arrival { newest, older, duplicate, tooOld };

    bool any = false;
    uint16_t newest = 0;
    uint32_t bits = 0;

    /// Records `sequence` and says how it arrived.
    Arrival record(uint16_t sequence);
    bool has(uint16_t sequence) const;
};

/// Calls `f(sequence)` for each sequence an ack field names: `ack` and the
/// set bits after it, 33 at most.
template <class F> void forEachAcked(uint16_t ack, uint32_t ackBits, F&& f) {
    f(ack);
    for (int i = 0; i < 32; i++) {
        if (ackBits & (uint32_t(1) << i)) f(static_cast<uint16_t>(ack - 1 - i));
    }
}

} // namespace ac::net
