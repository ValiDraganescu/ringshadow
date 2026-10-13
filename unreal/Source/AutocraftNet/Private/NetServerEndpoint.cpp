#include "NetConnection.h"
#include "NetEndpoint.h"
#include "SipHash.h"

#include <algorithm>
#include <cmath>

namespace ac::net {

namespace {

/// Packets read in one update at most, so a flood cannot hold the tick.
constexpr int kMaxReadsPerUpdate = 4096;

std::string readReason(BitReader& r) {
    std::string reason(r.readU8(), '\0');
    if (!r.readBytes(reinterpret_cast<uint8_t*>(reason.data()), reason.size())) return "disconnected";
    return reason;
}

} // namespace

ServerEndpoint::ServerEndpoint(PacketIo& io, const ServerConfig& config) : io_(io), config_(config) {}

ServerEndpoint::~ServerEndpoint() = default;

uint64_t ServerEndpoint::cookie(const Address& from, int64_t window) const {
    uint8_t buf[14];
    for (int i = 0; i < 4; i++) buf[i] = static_cast<uint8_t>(from.ip >> (8 * i));
    buf[4] = static_cast<uint8_t>(from.port);
    buf[5] = static_cast<uint8_t>(from.port >> 8);
    for (int i = 0; i < 8; i++) buf[6 + i] = static_cast<uint8_t>(static_cast<uint64_t>(window) >> (8 * i));
    return sipHash24(config_.secret.data(), buf, sizeof buf);
}

ConnectionId ServerEndpoint::newConnectionId() {
    for (;;) {
        uint8_t buf[9] = {'C'};
        const uint64_t n = idCounter_++;
        for (int i = 0; i < 8; i++) buf[1 + i] = static_cast<uint8_t>(n >> (8 * i));
        const ConnectionId id = static_cast<ConnectionId>(sipHash24(config_.secret.data(), buf, sizeof buf));
        if (id != 0 && byId_.find(id) == byId_.end()) return id;
    }
}

void ServerEndpoint::sendHandshake(double now, const Address& to, PacketType type, uint32_t connection,
                                   const uint8_t* body, size_t bodySize, size_t limit) {
    PacketHeader h;
    h.version = config_.version;
    h.type = type;
    h.connection = connection;
    BitWriter w;
    writeHeader(w, h);
    w.writeBytes(body, bodySize);
    if (w.byteCount() > limit) return;  // never more than was received
    io_.send(now, to, w.bytes().data(), w.byteCount());
}

void ServerEndpoint::refuse(double now, const Address& to, const std::string& reason, size_t limit) {
    std::vector<uint8_t> body;
    const size_t length = std::min<size_t>(reason.size(), 255);
    body.push_back(static_cast<uint8_t>(length));
    body.insert(body.end(), reason.begin(), reason.begin() + static_cast<std::ptrdiff_t>(length));
    sendHandshake(now, to, PacketType::refuse, 0, body.data(), body.size(), limit);
    Event e;
    e.type = Event::Type::refused;
    e.remote = to;
    e.reason = reason;
    events_.push_back(std::move(e));
}

void ServerEndpoint::handle(double now, const Address& from, const uint8_t* data, size_t size) {
    if (size > kMaxPacketBytes) return;
    BitReader r(data, size);
    PacketHeader h;
    if (!readHeader(r, h)) return;
    if (h.version != config_.version) {
        if (h.type == PacketType::hello && size == kMaxPacketBytes) {
            refuse(now, from, "the server runs version " + std::to_string(config_.version), size);
        }
        return;
    }
    const auto known = byAddress_.find(from.key());
    Connection* connection = known == byAddress_.end() ? nullptr : byId_[known->second].get();
    const int64_t window = static_cast<int64_t>(std::floor(now / config_.cookieSeconds));

    switch (h.type) {
    case PacketType::hello: {
        if (size != kMaxPacketBytes) return;
        uint8_t body[8];
        const uint64_t c = cookie(from, window);
        for (int i = 0; i < 8; i++) body[i] = static_cast<uint8_t>(c >> (8 * i));
        sendHandshake(now, from, PacketType::challenge, 0, body, sizeof body, size);
        return;
    }
    case PacketType::answer: {
        if (size != kMaxPacketBytes) return;
        const uint64_t c = r.readU64();
        crypto::Key clientPublic;
        if (!r.readBytes(clientPublic.data(), clientPublic.size())) return;
        if (c != cookie(from, window) && c != cookie(from, window - 1)) {
            refuse(now, from, "bad cookie", size);
            return;
        }
        if (connection && connection->clientPublic == clientPublic) {  // the welcome was lost: say it again
            connection->heard(now);
            sendHandshake(now, from, PacketType::welcome, connection->id(), connection->serverPublic.data(),
                          connection->serverPublic.size(), size);
            return;
        }
        if (connection) {  // a new handshake from the same address: the old connection is gone
            forget(connection->id(), "reconnected");
            connection = nullptr;
        }
        if (byId_.size() >= config_.maxConnections) {
            refuse(now, from, "the server is full", size);
            return;
        }
        crypto::KeyPair keys;
        crypto::SessionKeys session;
        if (!crypto::makeKeyPair(keys) ||
            !crypto::deriveKeys(keys.secret, clientPublic, clientPublic, keys.publicKey, c, session)) {
            return;
        }
        const ConnectionId id = newConnectionId();
        auto made = std::make_unique<Connection>(id, from, config_.version, config_.connection, now, session, true);
        crypto::wipe(&session, sizeof session);
        made->clientPublic = clientPublic;
        made->serverPublic = keys.publicKey;
        byId_[id] = std::move(made);
        byAddress_[from.key()] = id;
        sendHandshake(now, from, PacketType::welcome, id, keys.publicKey.data(), keys.publicKey.size(), size);
        return;
    }
    case PacketType::token: {
        if (!connection || h.connection != connection->id()) return;
        std::vector<uint8_t> plain;
        if (!connection->open(h, data, size, plain)) return;
        connection->heard(now);
        connection->answerSoon();  // the client waits for a sealed packet from here
        if (connection->accepted) return;  // that packet was lost: another goes at the flush
        BitReader t(plain.data(), plain.size());
        std::vector<uint8_t> token(t.readU8());
        const bool read = t.readBytes(token.data(), token.size());
        uint8_t diff = read && token.size() == config_.token.size() ? 0 : 1;
        for (size_t i = 0; i < token.size() && i < config_.token.size(); i++) diff |= token[i] ^ config_.token[i];
        crypto::wipe(plain.data(), plain.size());
        crypto::wipe(token.data(), token.size());
        if (diff != 0) {
            byId_.erase(connection->id());
            byAddress_.erase(known);
            refuse(now, from, "bad token", kMaxPacketBytes);
            return;
        }
        connection->accepted = true;
        Event e;
        e.type = Event::Type::connected;
        e.connection = connection->id();
        e.remote = from;
        events_.push_back(std::move(e));
        return;
    }
    case PacketType::data:
        if (connection && connection->accepted && h.connection == connection->id()) {
            connection->receive(now, h, data, size, events_);
        }
        return;
    case PacketType::disconnect: {
        if (!connection || h.connection != connection->id()) return;
        std::vector<uint8_t> plain;
        if (!connection->open(h, data, size, plain)) return;
        BitReader reason(plain.data(), plain.size());
        forget(connection->id(), readReason(reason));
        return;
    }
    default:
        return;  // the server's own kinds
    }
}

void ServerEndpoint::forget(ConnectionId id, const std::string& reason) {
    const auto it = byId_.find(id);
    if (it == byId_.end()) return;
    if (it->second->accepted) {
        Event e;
        e.type = Event::Type::disconnected;
        e.connection = id;
        e.reason = reason;
        events_.push_back(std::move(e));
    }
    byAddress_.erase(it->second->remote().key());
    byId_.erase(it);
}

void ServerEndpoint::update(double now) {
    uint8_t buffer[1500];
    for (int i = 0; i < kMaxReadsPerUpdate; i++) {
        Address from;
        size_t size = 0;
        if (!io_.receive(now, from, buffer, sizeof buffer, size)) break;
        handle(now, from, buffer, size);
    }
    std::vector<ConnectionId> silent;
    for (const auto& [id, connection] : byId_) {
        if (connection->timedOut(now)) silent.push_back(id);
    }
    for (ConnectionId id : silent) forget(id, "timed out");
}

size_t ServerEndpoint::connectionCount() const {
    size_t n = 0;
    for (const auto& [id, connection] : byId_) n += connection->accepted ? 1 : 0;
    return n;
}

const Connection* ServerEndpoint::connectionForTests(ConnectionId connection) const {
    const auto it = byId_.find(connection);
    return it == byId_.end() ? nullptr : it->second.get();
}

bool ServerEndpoint::poll(Event& out) {
    if (events_.empty()) return false;
    out = std::move(events_.front());
    events_.pop_front();
    return true;
}

bool ServerEndpoint::send(ConnectionId connection, Channel channel, const uint8_t* data, size_t size) {
    const auto it = byId_.find(connection);
    return it != byId_.end() && it->second->accepted && it->second->queue(channel, data, size);
}

int64_t ServerEndpoint::budgetLeft(ConnectionId connection, double now) const {
    const auto it = byId_.find(connection);
    return it == byId_.end() || !it->second->accepted ? 0 : it->second->budgetLeft(now);
}

void ServerEndpoint::flush(double now) {
    for (auto& [id, connection] : byId_) {
        if (connection->accepted) connection->flush(now, io_);
    }
}

void ServerEndpoint::disconnect(ConnectionId connection, double now, const std::string& reason) {
    const auto it = byId_.find(connection);
    if (it == byId_.end()) return;
    it->second->sendDisconnect(now, io_, reason);
    byAddress_.erase(it->second->remote().key());
    byId_.erase(it);
}

const ConnectionStats* ServerEndpoint::stats(ConnectionId connection) const {
    const auto it = byId_.find(connection);
    return it == byId_.end() || !it->second->accepted ? nullptr : &it->second->stats();
}

} // namespace ac::net
