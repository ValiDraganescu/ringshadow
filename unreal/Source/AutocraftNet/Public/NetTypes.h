// What the endpoints hand the caller: the channels, the events, a
// connection's settings and counters.
#pragma once

#include "NetPacket.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ac::net {

/// docs/networking.md, "Channels".
enum class Channel : uint8_t {
    unreliable = 0,  // newest wins: a message from a packet older than one already received is dropped
    reliable = 1,    // resent until acked, delivered in order exactly once
    bulk = 2,        // large data split into pieces, each resent until acked, delivered whole and in order
};

/// The largest message each channel takes in one call.
inline constexpr size_t kMaxUnreliableBytes = kMaxPayloadBytes - 3;
inline constexpr size_t kMaxReliableBytes = kMaxPayloadBytes - 5;
inline constexpr size_t kBulkPieceBytes = kMaxPayloadBytes - 11;
inline constexpr size_t kMaxBulkBytes = size_t(64) << 20;

using ConnectionId = uint32_t;

struct Event {
    enum class Type { connected, disconnected, message };

    Type type = Type::message;
    ConnectionId connection = 0;
    Channel channel = Channel::unreliable;
    std::vector<uint8_t> data;
    /// Why, for `disconnected`: "timed out", a refusal ("the server runs
    /// version 2", "bad cookie", "bad token") or the peer's own words.
    std::string reason;
};

struct ConnectionSettings {
    /// The send budget. Everything a connection sends after the handshake
    /// counts against it, headers included.
    double bytesPerSecond = 64 * 1024;
    /// How much unspent budget saves up, in seconds of it (at least a packet).
    double burstSeconds = 0.1;
    /// Send at least a packet this often.
    double keepAliveSeconds = 0.5;
    /// End the connection after this long without a packet.
    double timeoutSeconds = 10;
};

struct ConnectionStats {
    /// Smoothed round trip, seconds; 0 until the first ack.
    double rtt = 0;
    uint64_t packetsSent = 0;
    uint64_t packetsReceived = 0;
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    /// Reliable messages and bulk pieces sent again.
    uint64_t resends = 0;
    /// Unreliable messages that did not fit the budget.
    uint64_t unreliableDropped = 0;
};

} // namespace ac::net
