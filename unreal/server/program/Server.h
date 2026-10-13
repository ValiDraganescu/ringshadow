// The server: one region behind the protocol (docs/networking.md). A
// connection is a pilot: it joins on connect and leaves on disconnect. Each
// tick reads what came in, steps the region once (1/60 s) and, every third
// tick (20 Hz), sends each client its events and its snapshot within what is
// left of its byte budget. Polled with the caller's clock, on any PacketIo:
// the program (main.cpp) runs it on a UDP socket, the tests on their own
// network.
#pragma once

#include "NetEndpoint.h"
#include "NetSnapshot.h"
#include "World.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ac::server {

struct ServerOptions {
    RegionConfig region;
    /// What a client must answer with (the local server's one-time secret).
    std::vector<uint8_t> token;
    /// The key of the handshake's cookies: random (net::secureRandom).
    std::array<uint8_t, 16> secret{};
    net::ConnectionSettings connection;
    InterestParams interest;
    /// Ticks between snapshots: 3 is 20 Hz.
    int64_t snapshotEvery = 3;
    /// Seconds between stats lines in the log.
    double statsEvery = 10;
    /// One line of the log (no newline).
    std::function<void(const std::string&)> log;
};

class Server {
public:
    Server(net::PacketIo& io, const ServerOptions& options);

    /// One server tick at `now` (seconds, never going back). `stepRegion`
    /// false holds the region still (the tests): the network goes on.
    void tick(double now, bool stepRegion = true);
    /// Tells every client the server stops, and sends it.
    void stop(double now, const std::string& reason);

    Region& region() { return region_; }
    const Region& region() const { return region_; }
    size_t clients() const { return clients_.size(); }

    /// One client's counters.
    struct ClientStats {
        net::ConnectionId connection = 0;
        PilotId pilot = 0;
        uint64_t bytesSent = 0;
        int64_t snapshots = 0;
        /// Snapshot messages sent, all parts counted.
        int64_t snapshotMessages = 0;
        int64_t deferred = 0;
        int64_t fullResets = 0;
    };
    std::vector<ClientStats> clientStats() const;

private:
    struct Client {
        PilotId pilot = 0;
        net::SnapshotWriter writer;
        int64_t snapshots = 0;
        int64_t snapshotMessages = 0;
        int64_t deferred = 0;
        uint64_t bytesAtStats = 0;
    };

    void handle(const net::Event& e, double now);
    void message(net::ConnectionId c, Client& client, const std::vector<uint8_t>& data, double now);
    void sendSnapshots(double now);
    void statsLine(double now);
    void log(const std::string& line) const;

    ServerOptions options_;
    Region region_;
    net::ServerEndpoint endpoint_;
    std::map<net::ConnectionId, Client> clients_;
    int64_t ticked_ = 0;
    // The tick's time, for the stats line.
    double statsFrom_ = -1;
    double tickSum_ = 0;
    double tickMax_ = 0;
    int64_t ticks_ = 0;
};

} // namespace ac::server
