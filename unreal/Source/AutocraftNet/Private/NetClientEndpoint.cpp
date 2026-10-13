#include "NetConnection.h"
#include "NetEndpoint.h"

#include <algorithm>

namespace ac::net {

namespace {

constexpr int kMaxReadsPerUpdate = 1024;

} // namespace

ClientEndpoint::ClientEndpoint(PacketIo& io, const ClientConfig& config) : io_(io), config_(config) {}

ClientEndpoint::~ClientEndpoint() = default;

void ClientEndpoint::connect(double now) {
    connection_.reset();
    connectionId_ = 0;
    cookie_ = 0;
    state_ = State::hello;
    started_ = now;
    sendHandshake(now);
}

void ClientEndpoint::sendHandshake(double now) {
    PacketHeader h;
    h.version = config_.version;
    h.type = state_ == State::hello ? PacketType::hello : PacketType::answer;
    BitWriter w;
    writeHeader(w, h);
    if (state_ == State::answer) {
        const size_t length = std::min<size_t>(config_.token.size(), 255);
        w.writeU64(cookie_);
        w.writeU8(static_cast<uint8_t>(length));
        w.writeBytes(config_.token.data(), length);
    }
    // A full packet: the server never answers with more than it got.
    while (w.byteCount() < kMaxPacketBytes) w.writeU8(0);
    io_.send(now, config_.server, w.bytes().data(), w.byteCount());
    lastHandshake_ = now;
}

void ClientEndpoint::fail(const std::string& reason) {
    Event e;
    e.type = Event::Type::disconnected;
    e.connection = connectionId_;
    e.reason = reason;
    events_.push_back(std::move(e));
    state_ = State::disconnected;
    connection_.reset();
}

void ClientEndpoint::handle(double now, const uint8_t* data, size_t size) {
    if (size > kMaxPacketBytes) return;
    BitReader r(data, size);
    PacketHeader h;
    if (!readHeader(r, h)) return;
    const bool handshaking = state_ == State::hello || state_ == State::answer;
    if (h.type == PacketType::refuse) {  // any version: it may be why
        if (!handshaking) return;
        std::string reason(r.readU8(), '\0');
        if (!r.readBytes(reinterpret_cast<uint8_t*>(reason.data()), reason.size())) return;
        fail(reason);
        return;
    }
    if (h.version != config_.version) return;
    switch (h.type) {
    case PacketType::challenge: {
        if (state_ != State::hello) return;
        const uint64_t c = r.readU64();
        if (!r.ok()) return;
        cookie_ = c;
        state_ = State::answer;
        sendHandshake(now);
        return;
    }
    case PacketType::welcome: {
        if (state_ != State::answer || h.connection == 0) return;
        connectionId_ = h.connection;
        connection_ = std::make_unique<Connection>(connectionId_, config_.server, config_.version, config_.connection, now);
        state_ = State::connected;
        Event e;
        e.type = Event::Type::connected;
        e.connection = connectionId_;
        events_.push_back(std::move(e));
        return;
    }
    case PacketType::data:
        if (state_ == State::connected && h.connection == connectionId_) connection_->receive(now, h, r, size, events_);
        return;
    case PacketType::disconnect: {
        if (state_ != State::connected || h.connection != connectionId_) return;
        std::string reason(r.readU8(), '\0');
        if (!r.readBytes(reinterpret_cast<uint8_t*>(reason.data()), reason.size())) reason = "disconnected";
        fail(reason);
        return;
    }
    default:
        return;  // the client's own kinds
    }
}

void ClientEndpoint::update(double now) {
    uint8_t buffer[1500];
    for (int i = 0; i < kMaxReadsPerUpdate; i++) {
        Address from;
        size_t size = 0;
        if (!io_.receive(now, from, buffer, sizeof buffer, size)) break;
        if (from != config_.server) continue;
        handle(now, buffer, size);
    }
    if (state_ == State::hello || state_ == State::answer) {
        if (now - started_ >= config_.connectSeconds) fail("timed out");
    } else if (state_ == State::connected && connection_->timedOut(now)) {
        fail("timed out");
    }
}

bool ClientEndpoint::poll(Event& out) {
    if (events_.empty()) return false;
    out = std::move(events_.front());
    events_.pop_front();
    return true;
}

bool ClientEndpoint::send(Channel channel, const uint8_t* data, size_t size) {
    return state_ == State::connected && connection_->queue(channel, data, size);
}

int64_t ClientEndpoint::budgetLeft(double now) const {
    return state_ == State::connected ? connection_->budgetLeft(now) : 0;
}

void ClientEndpoint::flush(double now) {
    if (state_ == State::hello || state_ == State::answer) {
        if (now - lastHandshake_ >= config_.handshakeResendSeconds) sendHandshake(now);
    } else if (state_ == State::connected) {
        connection_->flush(now, io_);
    }
}

void ClientEndpoint::disconnect(double now, const std::string& reason) {
    if (state_ == State::connected) connection_->sendDisconnect(now, io_, reason);
    connection_.reset();
    state_ = State::disconnected;
}

const ConnectionStats* ClientEndpoint::stats() const {
    return state_ == State::connected ? &connection_->stats() : nullptr;
}

} // namespace ac::net
