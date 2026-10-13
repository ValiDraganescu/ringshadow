// The two ends of the protocol: a server with many connections and a
// client with one. Both are polled and never wait, and every call takes
// the time (seconds, any origin, never going back). A tick:
//
//   endpoint.update(now);              // read packets, time out the silent
//   while (endpoint.poll(event)) ...   // connected, disconnected, message
//   endpoint.send(..reliable..)        // requests, replies, events, bulk
//   endpoint.budgetLeft(..., now)      // what is left for unreliable data
//   endpoint.send(..unreliable..)      // the snapshot or the input
//   endpoint.flush(now);               // write and send the packets
//
// The handshake (docs/networking.md, "The connection", "Encryption"): the
// client sends a hello; the server answers with a cookie, a keyed hash of
// the client's address and the time window, and keeps nothing; the client
// sends the cookie back with its public key; the server checks the cookie,
// makes its own key pair and welcomes it with a connection id and its
// public key. Both ends now have the session keys (NetCrypto.h). The client
// sends its token sealed until the server's first sealed packet comes; the
// server checks the token and only then says connected. Hellos and answers
// fill a whole packet, so nothing the server sends before the keys is
// bigger than what it got.
#pragma once

#include "AutocraftNetApi.h"
#include "NetSocket.h"
#include "NetTypes.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ac::net {

class Connection;

struct ServerConfig {
    uint16_t version = kProtocolVersion;
    /// The key of the cookies and connection ids: random, from secureRandom.
    std::array<uint8_t, 16> secret{};
    /// What a client must answer with: the local server's one-time secret
    /// (at most 255 bytes). Later the login takes its place.
    std::vector<uint8_t> token;
    ConnectionSettings connection;
    size_t maxConnections = 256;
    /// A cookie is good in the window it was made in and the next one.
    double cookieSeconds = 10;
};

class AUTOCRAFTNET_API ServerEndpoint {
public:
    ServerEndpoint(PacketIo& io, const ServerConfig& config);
    ~ServerEndpoint();
    ServerEndpoint(const ServerEndpoint&) = delete;
    ServerEndpoint& operator=(const ServerEndpoint&) = delete;

    void update(double now);
    bool poll(Event& out);
    /// Queues a message for one client; false if there is no such
    /// connection or the message is too big for its channel.
    bool send(ConnectionId connection, Channel channel, const uint8_t* data, size_t size);
    /// Bytes of budget left after the reliable messages and bulk pieces due.
    int64_t budgetLeft(ConnectionId connection, double now) const;
    void flush(double now);
    /// Tells the client why and forgets it. No event for it here.
    void disconnect(ConnectionId connection, double now, const std::string& reason);

    /// The connections whose token came.
    size_t connectionCount() const;
    /// Null if there is no such connection.
    const ConnectionStats* stats(ConnectionId connection) const;
    /// The tests' look at a connection (its keys, its counter); null if none.
    const Connection* connectionForTests(ConnectionId connection) const;

private:
    void handle(double now, const Address& from, const uint8_t* data, size_t size);
    /// Drops a connection, with a disconnected event if it was connected.
    void forget(ConnectionId connection, const std::string& reason);
    uint64_t cookie(const Address& from, int64_t window) const;
    void sendHandshake(double now, const Address& to, PacketType type, uint32_t connection, const uint8_t* body,
                       size_t bodySize, size_t limit);
    void refuse(double now, const Address& to, const std::string& reason, size_t limit);
    ConnectionId newConnectionId();

    PacketIo& io_;
    ServerConfig config_;
    std::map<ConnectionId, std::unique_ptr<Connection>> byId_;
    std::map<uint64_t, ConnectionId> byAddress_;
    std::deque<Event> events_;
    uint64_t idCounter_ = 0;
};

struct ClientConfig {
    Address server;
    uint16_t version = kProtocolVersion;
    /// The local server's one-time secret (at most 255 bytes).
    std::vector<uint8_t> token;
    ConnectionSettings connection;
    /// Give up a handshake after this long.
    double connectSeconds = 10;
    /// Hello and answer are sent again this often until answered.
    double handshakeResendSeconds = 0.25;
};

class AUTOCRAFTNET_API ClientEndpoint {
public:
    /// `token`: welcomed, the keys set, the token sent until the server's
    /// first sealed packet comes.
    enum class State { idle, hello, answer, token, connected, disconnected };

    ClientEndpoint(PacketIo& io, const ClientConfig& config);
    ~ClientEndpoint();
    ClientEndpoint(const ClientEndpoint&) = delete;
    ClientEndpoint& operator=(const ClientEndpoint&) = delete;

    /// Starts the handshake (again, after a disconnect).
    void connect(double now);
    void update(double now);
    bool poll(Event& out);
    /// False unless connected, or if the message is too big for its channel.
    bool send(Channel channel, const uint8_t* data, size_t size);
    int64_t budgetLeft(double now) const;
    void flush(double now);
    /// Tells the server why and stops. No event for it here.
    void disconnect(double now, const std::string& reason);

    State state() const { return state_; }
    ConnectionId connectionId() const { return connectionId_; }
    /// Null unless connected.
    const ConnectionStats* stats() const;
    /// The tests' look at the connection; null before the welcome.
    const Connection* connectionForTests() const { return connection_.get(); }

private:
    void handle(double now, const uint8_t* data, size_t size);
    void sendHandshake(double now);
    void fail(const std::string& reason);

    PacketIo& io_;
    ClientConfig config_;
    State state_ = State::idle;
    double started_ = 0;
    double lastHandshake_ = 0;
    uint64_t cookie_ = 0;
    /// This connection's X25519 key pair, made at connect.
    std::array<uint8_t, 32> secret_{};
    std::array<uint8_t, 32> publicKey_{};
    ConnectionId connectionId_ = 0;
    std::unique_ptr<Connection> connection_;
    std::deque<Event> events_;
};

} // namespace ac::net
