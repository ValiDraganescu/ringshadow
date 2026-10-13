// One end of an established connection: the acks, the three channels and
// the send budget. The server endpoint keeps one per client, the client one
// for its server; neither side's handshake lives here.
#pragma once

#include "NetSocket.h"
#include "NetTypes.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ac::net {

class Connection {
public:
    Connection(ConnectionId id, const Address& remote, uint16_t version, const ConnectionSettings& settings, double now);

    ConnectionId id() const { return id_; }
    const Address& remote() const { return remote_; }
    const ConnectionStats& stats() const { return stats_; }

    /// A data packet from the remote, its header read; the messages it
    /// completes go to `events`.
    void receive(double now, const PacketHeader& header, BitReader& payload, size_t packetBytes,
                 std::deque<Event>& events);
    /// Any packet that proves the remote is there (the handshake's).
    void heard(double now) { lastReceived_ = now; }
    /// Queues a message; false if it is too big for its channel.
    bool queue(Channel channel, const uint8_t* data, size_t size);
    /// The budget left now after the reliable messages and bulk pieces due
    /// (headers included), for the caller's unreliable messages.
    int64_t budgetLeft(double now) const;
    /// Sends what is due: reliable first, then bulk, then the unreliable
    /// messages that fit the budget. A keep-alive or a bare ack if nothing else.
    void flush(double now, PacketIo& io);
    /// Three copies of a disconnect packet, out of the budget.
    void sendDisconnect(double now, PacketIo& io, const std::string& reason);
    bool timedOut(double now) const { return now - lastReceived_ >= settings_.timeoutSeconds; }

private:
    struct Reliable {
        uint16_t id = 0;
        std::vector<uint8_t> data;
        double lastSent = -1;
        bool acked = false;
    };
    struct BulkOut {
        uint16_t id = 0;
        std::vector<uint8_t> data;
        uint32_t pieces = 0;
        uint32_t ackedCount = 0;
        std::vector<double> lastSent;
        std::vector<uint8_t> acked;
    };
    struct BulkIn {
        uint16_t id = 0;
        uint32_t total = 0;
        uint32_t pieces = 0;
        uint32_t gotCount = 0;
        std::vector<uint8_t> data;
        std::vector<uint8_t> got;
    };
    struct Held {
        bool has = false;
        std::vector<uint8_t> data;
    };
    struct SentItem {
        uint8_t kind = 0;
        uint16_t id = 0;
        uint16_t piece = 0;
    };
    struct SentPacket {
        bool used = false;
        bool acked = false;
        uint16_t sequence = 0;
        double time = 0;
        std::vector<SentItem> items;
    };
    /// A message or bulk piece picked for this flush.
    struct Planned {
        uint8_t kind = 0;
        size_t index = 0;  // into reliable_, bulk_ or unreliable_
        uint32_t piece = 0;
        size_t bytes = 0;  // with its chunk header
    };

    double allowanceAt(double now) const;
    double resendDelay() const;
    /// Picks what goes out now within `budget`, in order; returns its cost.
    double plan(double now, double budget, bool withUnreliable, std::vector<Planned>& out) const;
    void ackPacket(double now, uint16_t sequence);
    void writePacketStart(BitWriter& w, PacketType type);
    void sendPacket(double now, PacketIo& io, const BitWriter& w, std::vector<SentItem>&& items);

    ConnectionId id_;
    Address remote_;
    uint16_t version_;
    ConnectionSettings settings_;
    ConnectionStats stats_;

    double allowance_;
    double allowanceTime_;
    double lastSent_;
    double lastReceived_;
    double srtt_ = -1;
    double rttvar_ = 0;

    uint16_t nextSequence_ = 0;
    std::vector<SentPacket> sent_;
    ReceiveWindow window_;
    bool ackPending_ = false;

    std::deque<Reliable> reliable_;
    uint16_t nextReliableId_ = 0;
    std::deque<BulkOut> bulk_;
    uint16_t nextBulkId_ = 0;
    std::vector<std::vector<uint8_t>> unreliable_;

    std::vector<Held> held_;
    uint16_t expectReliable_ = 0;
    std::vector<BulkIn> bulkIn_;
    uint16_t expectBulk_ = 0;
    size_t bulkInBytes_ = 0;
};

} // namespace ac::net
