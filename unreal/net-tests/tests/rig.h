// The tests' network: an in-memory one (MemoryPort: a packet sent is in the
// other's inbox at once) and a Rig, a server and one client on it, stepped
// at 60 Hz on a clock the test owns. The client's lag simulator lags both
// ways, as the game's lag setting will.
#pragma once

#include "NetEndpoint.h"
#include "NetLagSimulator.h"

#include <cstdint>
#include <deque>
#include <map>
#include <utility>
#include <vector>

namespace nettest {

using namespace ac::net;

struct MemoryNet {
    std::map<uint64_t, std::deque<std::pair<Address, std::vector<uint8_t>>>> inbox;
};

class MemoryPort final : public PacketIo {
public:
    MemoryPort(MemoryNet& net, const Address& self) : net_(net), self_(self) {}

    bool send(double, const Address& to, const uint8_t* data, size_t size) override {
        net_.inbox[to.key()].emplace_back(self_, std::vector<uint8_t>(data, data + size));
        sent += 1;
        return true;
    }
    bool receive(double, Address& from, uint8_t* buffer, size_t capacity, size_t& size) override {
        auto& box = net_.inbox[self_.key()];
        if (box.empty()) return false;
        auto& [sender, bytes] = box.front();
        if (bytes.size() > capacity) {
            box.pop_front();
            return false;
        }
        from = sender;
        size = bytes.size();
        std::copy(bytes.begin(), bytes.end(), buffer);
        box.pop_front();
        return true;
    }
    const Address& self() const { return self_; }

    uint64_t sent = 0;

private:
    MemoryNet& net_;
    Address self_;
};

inline const Address kServerAddress = Address::ipv4(10, 0, 0, 1, 4000);
inline const Address kClientAddress = Address::ipv4(10, 0, 0, 2, 5000);
inline const std::vector<uint8_t> kToken = {1, 2, 3, 4, 5, 6, 7, 8};

inline ServerConfig serverConfig() {
    ServerConfig c;
    for (size_t i = 0; i < c.secret.size(); i++) c.secret[i] = static_cast<uint8_t>(0xA0 + i);
    c.token = kToken;
    return c;
}

inline ClientConfig clientConfig() {
    ClientConfig c;
    c.server = kServerAddress;
    c.token = kToken;
    return c;
}

inline std::vector<uint8_t> u32Bytes(uint32_t v) {
    return {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
}

inline uint32_t bytesU32(const std::vector<uint8_t>& b) {
    return b.size() < 4 ? 0xFFFFFFFF : uint32_t(b[0]) | (uint32_t(b[1]) << 8) | (uint32_t(b[2]) << 16) | (uint32_t(b[3]) << 24);
}

struct Rig {
    explicit Rig(const LagSettings& lag = {}, const ServerConfig& sc = serverConfig(),
                 const ClientConfig& cc = clientConfig())
        : serverPort(net, kServerAddress), clientPort(net, kClientAddress), clientLag(clientPort, 22),
          server(serverPort, sc), client(clientLag, cc) {
        clientLag.setDefault(lag);
    }

    /// One tick: both read and hand over their events, `between` queues
    /// messages, both send.
    template <class F> void step(F&& between, double dt = 1.0 / 60) {
        now += dt;
        server.update(now);
        for (Event e; server.poll(e);) serverEvents.push_back(std::move(e));
        client.update(now);
        for (Event e; client.poll(e);) clientEvents.push_back(std::move(e));
        between();
        server.flush(now);
        client.flush(now);
    }
    void step(double dt = 1.0 / 60) {
        step([] {}, dt);
    }

    /// Runs until both ends say connected; false after 10 s.
    bool connect() {
        client.connect(now);
        for (int i = 0; i < 600; i++) {
            step();
            if (client.state() == ClientEndpoint::State::connected && server.connectionCount() == 1) {
                serverConnection = client.connectionId();
                return true;
            }
        }
        return false;
    }

    /// The messages of one channel received so far, in order.
    static std::vector<std::vector<uint8_t>> messages(const std::vector<Event>& events, Channel channel) {
        std::vector<std::vector<uint8_t>> out;
        for (const Event& e : events) {
            if (e.type == Event::Type::message && e.channel == channel) out.push_back(e.data);
        }
        return out;
    }
    static const Event* find(const std::vector<Event>& events, Event::Type type) {
        for (const Event& e : events) {
            if (e.type == type) return &e;
        }
        return nullptr;
    }

    MemoryNet net;
    MemoryPort serverPort;
    MemoryPort clientPort;
    LagSimulator clientLag;
    ServerEndpoint server;
    ClientEndpoint client;
    std::vector<Event> serverEvents;
    std::vector<Event> clientEvents;
    ConnectionId serverConnection = 0;
    double now = 100;
};

/// The rough internet the channel tests run through: each way 50 ms, up to
/// 30 ms of jitter, 5% loss, 5% duplicates, 10% held back behind later ones.
inline LagSettings roughLag() {
    LagSettings s;
    s.delay = 0.05;
    s.jitter = 0.03;
    s.loss = 0.05;
    s.duplicate = 0.05;
    s.reorder = 0.10;
    s.reorderHold = 0.06;
    return s;
}

} // namespace nettest
