#include "NetConnection.h"

#include <algorithm>
#include <cmath>

namespace ac::net {

namespace {

// The chunks a data packet's payload is made of, each led by its kind:
//   unreliable  u8 0, u16 length, bytes
//   reliable    u8 1, u16 id, u16 length, bytes
//   bulk piece  u8 2, u16 transfer id, u32 total size, u16 piece, u16 length, bytes
constexpr uint8_t kUnreliable = 0;
constexpr uint8_t kReliable = 1;
constexpr uint8_t kBulk = 2;
constexpr size_t kUnreliableHead = 3;
constexpr size_t kReliableHead = 5;
constexpr size_t kBulkHead = 11;

/// Sent packets remembered for their acks.
constexpr size_t kSentRing = 1024;
/// Reliable messages in flight at most; the receiver holds twice as many.
constexpr size_t kReliableWindow = 512;
constexpr size_t kReliableRing = 1024;
/// Bulk transfers in flight at most.
constexpr size_t kBulkWindow = 64;

uint32_t piecesFor(size_t total) { return total == 0 ? 1 : static_cast<uint32_t>((total + kBulkPieceBytes - 1) / kBulkPieceBytes); }

size_t pieceBytes(size_t total, uint32_t piece) {
    const size_t start = size_t(piece) * kBulkPieceBytes;
    return std::min(kBulkPieceBytes, total - std::min(total, start));
}

/// How packets fill up: an item goes in the open packet if it fits,
/// otherwise it starts a new one with its header.
struct Packer {
    size_t open = 0;  // bytes in the open packet; 0 when none is open

    bool fits(size_t item) const { return open > 0 && open + item <= kMaxPacketBytes; }
    size_t cost(size_t item) const { return fits(item) ? item : kPacketOverheadBytes + item; }
    void add(size_t item) { open = fits(item) ? open + item : kPacketOverheadBytes + item; }
};

} // namespace

Connection::Connection(ConnectionId id, const Address& remote, uint16_t version, const ConnectionSettings& settings,
                       double now, const crypto::SessionKeys& keys, bool server)
    : id_(id), remote_(remote), version_(version), settings_(settings), allowanceTime_(now), lastSent_(now),
      lastReceived_(now), keys_(keys),
      sendDirection_(server ? crypto::Direction::serverToClient : crypto::Direction::clientToServer),
      receiveDirection_(server ? crypto::Direction::clientToServer : crypto::Direction::serverToClient),
      sent_(kSentRing), held_(kReliableRing) {
    allowance_ = std::max(settings_.bytesPerSecond * settings_.burstSeconds, double(kMaxPacketBytes));
}

double Connection::allowanceAt(double now) const {
    const double cap = std::max(settings_.bytesPerSecond * settings_.burstSeconds, double(kMaxPacketBytes));
    if (allowance_ >= cap) return allowance_;
    return std::min(cap, allowance_ + settings_.bytesPerSecond * std::max(0.0, now - allowanceTime_));
}

double Connection::resendDelay() const {
    if (srtt_ < 0) return 0.25;
    return std::clamp(srtt_ + 4 * rttvar_, 0.05, 1.0);
}

double Connection::plan(double now, double budget, bool withUnreliable, std::vector<Planned>& out) const {
    const double delay = resendDelay();
    double left = budget;
    Packer packer;
    bool stopped = false;
    const auto take = [&](uint8_t kind, size_t index, uint32_t piece, size_t bytes) {
        const double cost = double(packer.cost(bytes));
        if (cost > left) return false;
        left -= cost;
        packer.add(bytes);
        out.push_back(Planned{kind, index, piece, bytes});
        return true;
    };
    const size_t reliableCount = std::min(reliable_.size(), kReliableWindow);
    for (size_t i = 0; i < reliableCount && !stopped; i++) {
        const Reliable& m = reliable_[i];
        if (m.acked || (m.lastSent >= 0 && now - m.lastSent < delay)) continue;
        stopped = !take(kReliable, i, 0, kReliableHead + m.data.size());
    }
    const size_t bulkCount = std::min(bulk_.size(), kBulkWindow);
    for (size_t i = 0; i < bulkCount && !stopped; i++) {
        const BulkOut& t = bulk_[i];
        for (uint32_t p = 0; p < t.pieces && !stopped; p++) {
            if (t.acked[p] || (t.lastSent[p] >= 0 && now - t.lastSent[p] < delay)) continue;
            stopped = !take(kBulk, i, p, kBulkHead + pieceBytes(t.data.size(), p));
        }
    }
    if (withUnreliable && !stopped) {
        for (size_t i = 0; i < unreliable_.size(); i++) take(kUnreliable, i, 0, kUnreliableHead + unreliable_[i].size());
    }
    return budget - left;
}

int64_t Connection::budgetLeft(double now) const {
    const double allowance = allowanceAt(now);
    std::vector<Planned> planned;
    const double left = allowance - plan(now, allowance, false, planned);
    return left > 0 ? static_cast<int64_t>(std::floor(left)) : 0;
}

bool Connection::queue(Channel channel, const uint8_t* data, size_t size) {
    switch (channel) {
    case Channel::unreliable:
        if (size > kMaxUnreliableBytes) return false;
        unreliable_.emplace_back(data, data + size);
        return true;
    case Channel::reliable: {
        if (size > kMaxReliableBytes) return false;
        Reliable m;
        m.id = nextReliableId_++;
        m.data.assign(data, data + size);
        reliable_.push_back(std::move(m));
        return true;
    }
    case Channel::bulk: {
        if (size > kMaxBulkBytes) return false;
        BulkOut t;
        t.id = nextBulkId_++;
        t.data.assign(data, data + size);
        t.pieces = piecesFor(size);
        t.lastSent.assign(t.pieces, -1.0);
        t.acked.assign(t.pieces, 0);
        bulk_.push_back(std::move(t));
        return true;
    }
    }
    return false;
}

void Connection::writePacketStart(BitWriter& w, PacketType type) {
    PacketHeader h;
    h.version = version_;
    h.type = type;
    h.connection = id_;
    h.sequence = static_cast<uint16_t>(sendCounter_);
    h.hasAck = window_.any;
    h.ack = window_.newest;
    h.ackBits = window_.bits;
    writeHeader(w, h);
}

void Connection::transmit(double now, PacketIo& io, const BitWriter& w) {
    crypto::seal(keys_.sending(sendDirection_), sendDirection_, sendCounter_, w.bytes().data(), kHeaderBytes,
                 w.byteCount(), sealed_);
    sendCounter_ += 1;
    io.send(now, remote_, sealed_.data(), sealed_.size());
    stats_.packetsSent += 1;
    stats_.bytesSent += sealed_.size();
}

void Connection::sendPacket(double now, PacketIo& io, const BitWriter& w, std::vector<SentItem>&& items) {
    const uint16_t sequence = static_cast<uint16_t>(sendCounter_);
    SentPacket& record = sent_[sequence % kSentRing];
    record.used = true;
    record.acked = false;
    record.sequence = sequence;
    record.time = now;
    record.items = std::move(items);
    transmit(now, io, w);
    allowance_ -= double(sealed_.size());
    lastSent_ = now;
    ackPending_ = false;
}

void Connection::sendToken(double now, PacketIo& io, const std::vector<uint8_t>& token) {
    const size_t length = std::min<size_t>(token.size(), 255);
    BitWriter w;
    writePacketStart(w, PacketType::token);
    w.writeU8(static_cast<uint8_t>(length));
    w.writeBytes(token.data(), length);
    transmit(now, io, w);
}

void Connection::flush(double now, PacketIo& io) {
    allowance_ = allowanceAt(now);
    allowanceTime_ = now;
    std::vector<Planned> planned;
    plan(now, allowance_, true, planned);

    size_t unreliableSent = 0;
    BitWriter w;
    std::vector<SentItem> items;
    size_t open = 0;
    for (const Planned& p : planned) {
        if (open == 0 || open + p.bytes > kMaxPacketBytes) {
            if (open > 0) sendPacket(now, io, w, std::move(items));
            w.clear();
            items.clear();
            writePacketStart(w, PacketType::data);
            open = kPacketOverheadBytes;
        }
        open += p.bytes;
        if (p.kind == kReliable) {
            Reliable& m = reliable_[p.index];
            w.writeU8(kReliable);
            w.writeU16(m.id);
            w.writeU16(static_cast<uint16_t>(m.data.size()));
            w.writeBytes(m.data.data(), m.data.size());
            if (m.lastSent >= 0) stats_.resends += 1;
            m.lastSent = now;
            items.push_back(SentItem{kReliable, m.id, 0});
        } else if (p.kind == kBulk) {
            BulkOut& t = bulk_[p.index];
            const size_t start = size_t(p.piece) * kBulkPieceBytes;
            const size_t bytes = pieceBytes(t.data.size(), p.piece);
            w.writeU8(kBulk);
            w.writeU16(t.id);
            w.writeU32(static_cast<uint32_t>(t.data.size()));
            w.writeU16(static_cast<uint16_t>(p.piece));
            w.writeU16(static_cast<uint16_t>(bytes));
            w.writeBytes(t.data.data() + start, bytes);
            if (t.lastSent[p.piece] >= 0) stats_.resends += 1;
            t.lastSent[p.piece] = now;
            items.push_back(SentItem{kBulk, t.id, static_cast<uint16_t>(p.piece)});
        } else {
            const std::vector<uint8_t>& m = unreliable_[p.index];
            w.writeU8(kUnreliable);
            w.writeU16(static_cast<uint16_t>(m.size()));
            w.writeBytes(m.data(), m.size());
            unreliableSent += 1;
        }
    }
    if (open > 0) {
        sendPacket(now, io, w, std::move(items));
    } else if (ackPending_ || now - lastSent_ >= settings_.keepAliveSeconds) {
        // Out of the budget if need be: the debt is paid back from later sends.
        w.clear();
        writePacketStart(w, PacketType::data);
        sendPacket(now, io, w, {});
    }
    stats_.unreliableDropped += unreliable_.size() - unreliableSent;
    unreliable_.clear();
}

void Connection::sendDisconnect(double now, PacketIo& io, const std::string& reason) {
    const size_t length = std::min<size_t>(reason.size(), 255);
    for (int i = 0; i < 3; i++) {
        BitWriter w;
        writePacketStart(w, PacketType::disconnect);
        w.writeU8(static_cast<uint8_t>(length));
        w.writeBytes(reinterpret_cast<const uint8_t*>(reason.data()), length);
        transmit(now, io, w);
    }
}

void Connection::ackPacket(double now, uint16_t sequence) {
    SentPacket& record = sent_[sequence % kSentRing];
    if (!record.used || record.acked || record.sequence != sequence) return;
    record.acked = true;
    const double sample = std::max(0.0, now - record.time);
    if (srtt_ < 0) {
        srtt_ = sample;
        rttvar_ = sample / 2;
    } else {
        rttvar_ = 0.75 * rttvar_ + 0.25 * std::fabs(srtt_ - sample);
        srtt_ = 0.875 * srtt_ + 0.125 * sample;
    }
    stats_.rtt = srtt_;
    for (const SentItem& item : record.items) {
        if (item.kind == kReliable) {
            if (reliable_.empty()) continue;
            const uint16_t index = static_cast<uint16_t>(item.id - reliable_.front().id);
            if (index < reliable_.size()) reliable_[index].acked = true;
        } else if (item.kind == kBulk) {
            for (BulkOut& t : bulk_) {
                if (t.id != item.id) continue;
                if (item.piece < t.pieces && !t.acked[item.piece]) {
                    t.acked[item.piece] = 1;
                    t.ackedCount += 1;
                }
                break;
            }
        }
    }
    record.items.clear();
    while (!reliable_.empty() && reliable_.front().acked) reliable_.pop_front();
    while (!bulk_.empty() && bulk_.front().ackedCount == bulk_.front().pieces) bulk_.pop_front();
}

bool Connection::open(const PacketHeader& header, const uint8_t* packet, size_t size, std::vector<uint8_t>& plain) {
    const uint64_t counter = crypto::widenCounter(opened_.newest(), header.sequence);
    if (!crypto::open(keys_.sending(receiveDirection_), receiveDirection_, counter, packet, kHeaderBytes, size, plain)) {
        stats_.packetsRejected += 1;
        return false;
    }
    if (!opened_.fresh(counter)) {
        stats_.packetsReplayed += 1;
        plain.clear();
        return false;
    }
    opened_.record(counter);
    return true;
}

bool Connection::receive(double now, const PacketHeader& header, const uint8_t* packet, size_t size,
                         std::deque<Event>& events) {
    if (!open(header, packet, size, plain_)) return false;
    BitReader r(plain_.data(), plain_.size());
    lastReceived_ = now;
    stats_.packetsReceived += 1;
    stats_.bytesReceived += size;
    const ReceiveWindow::Arrival arrival = window_.record(header.sequence);
    if (header.hasAck) forEachAcked(header.ack, header.ackBits, [&](uint16_t s) { ackPacket(now, s); });
    // Unreliable messages only from the newest packet yet: anything older is stale.
    const bool fresh = arrival == ReceiveWindow::Arrival::newest;

    const auto message = [&](Channel channel, std::vector<uint8_t>&& data) {
        Event e;
        e.type = Event::Type::message;
        e.connection = id_;
        e.channel = channel;
        e.data = std::move(data);
        events.push_back(std::move(e));
    };

    std::vector<uint8_t> scratch;
    while (r.ok() && r.bitsLeft() >= 8) {
        const uint8_t kind = r.readU8();
        if (kind == kUnreliable) {
            scratch.resize(r.readU16());
            if (!r.readBytes(scratch.data(), scratch.size())) break;
            if (fresh) message(Channel::unreliable, std::vector<uint8_t>(scratch));
        } else if (kind == kReliable) {
            const uint16_t id = r.readU16();
            scratch.resize(r.readU16());
            if (!r.readBytes(scratch.data(), scratch.size())) break;
            ackPending_ = true;
            const uint16_t ahead = static_cast<uint16_t>(id - expectReliable_);
            if (ahead >= kReliableRing) continue;  // delivered already
            Held& slot = held_[id % kReliableRing];
            if (!slot.has) {
                slot.has = true;
                slot.data = scratch;
            }
            for (;;) {
                Held& next = held_[expectReliable_ % kReliableRing];
                if (!next.has) break;
                next.has = false;
                message(Channel::reliable, std::move(next.data));
                next.data = {};
                expectReliable_ += 1;
            }
        } else if (kind == kBulk) {
            const uint16_t id = r.readU16();
            const uint32_t total = r.readU32();
            const uint16_t piece = r.readU16();
            scratch.resize(r.readU16());
            if (!r.readBytes(scratch.data(), scratch.size())) break;
            ackPending_ = true;
            if (total > kMaxBulkBytes) continue;
            const uint32_t pieces = piecesFor(total);
            if (piece >= pieces || scratch.size() != pieceBytes(total, piece)) continue;
            if (static_cast<uint16_t>(id - expectBulk_) >= kBulkWindow) continue;  // done already
            auto it = std::find_if(bulkIn_.begin(), bulkIn_.end(), [&](const BulkIn& b) { return b.id == id; });
            if (it == bulkIn_.end()) {
                if (bulkInBytes_ + total > kMaxBulkBytes) continue;
                BulkIn b;
                b.id = id;
                b.total = total;
                b.pieces = pieces;
                b.data.resize(total);
                b.got.assign(pieces, 0);
                bulkInBytes_ += total;
                bulkIn_.push_back(std::move(b));
                it = bulkIn_.end() - 1;
            }
            if (it->total != total || it->got[piece]) continue;
            it->got[piece] = 1;
            it->gotCount += 1;
            if (!scratch.empty()) std::copy(scratch.begin(), scratch.end(), it->data.begin() + size_t(piece) * kBulkPieceBytes);
            for (;;) {
                auto done = std::find_if(bulkIn_.begin(), bulkIn_.end(),
                                         [&](const BulkIn& b) { return b.id == expectBulk_ && b.gotCount == b.pieces; });
                if (done == bulkIn_.end()) break;
                bulkInBytes_ -= done->total;
                message(Channel::bulk, std::move(done->data));
                bulkIn_.erase(done);
                expectBulk_ += 1;
            }
        } else {
            break;  // not ours: the rest cannot be read
        }
    }
    return true;
}

} // namespace ac::net
