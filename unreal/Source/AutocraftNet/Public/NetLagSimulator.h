// The lag simulator (docs/networking.md, "Testing"): wraps a PacketIo and
// gives every packet, in both directions, the delay, jitter, loss,
// duplicates and reordering set for its remote address. On 127.0.0.1 there
// is no lag at all, which hides every bug in prediction and correction; the
// tests and the game's lag setting run through this. Random but
// deterministic: the same seed and the same calls give the same packets.
#pragma once

#include "AutocraftNetApi.h"
#include "NetSocket.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <vector>

namespace ac::net {

/// A small, fast, seeded generator (SplitMix64).
class Random {
public:
    explicit Random(uint64_t seed) : state_(seed) {}
    uint64_t next() {
        uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    /// 0 <= x < 1.
    double uniform() { return double(next() >> 11) * (1.0 / 9007199254740992.0); }

private:
    uint64_t state_;
};

/// Each direction gets all of it: the round trip is twice the delay.
struct LagSettings {
    double delay = 0;      // seconds, one way
    double jitter = 0;     // up to this much more, uniformly
    double loss = 0;       // chance a packet is lost, 0...1
    double duplicate = 0;  // chance it arrives twice (each copy with its own delay)
    double reorder = 0;    // chance it is held back by reorderHold, behind later ones
    double reorderHold = 0.05;
};

class AUTOCRAFTNET_API LagSimulator final : public PacketIo {
public:
    LagSimulator(PacketIo& inner, uint64_t seed) : inner_(inner), random_(seed) {}

    /// For every remote without its own settings.
    void setDefault(const LagSettings& settings) { default_ = settings; }
    void set(const Address& remote, const LagSettings& settings) { perRemote_[remote.key()] = settings; }
    const LagSettings& settingsFor(const Address& remote) const;

    bool send(double now, const Address& to, const uint8_t* data, size_t size) override;
    bool receive(double now, Address& from, uint8_t* buffer, size_t capacity, size_t& size) override;
    /// Sends the outgoing packets that are due; send and receive do it too.
    void pump(double now);
    /// Sends every outgoing packet held, due or not: the game is closing
    /// and its last packets (the goodbye) have left the machine already.
    void drain(double now);

    /// Packets the simulator dropped, both ways.
    uint64_t lost() const { return lost_; }

private:
    struct Held {
        double due = 0;
        uint64_t order = 0;
        Address remote;
        std::vector<uint8_t> data;
        bool operator>(const Held& o) const { return due != o.due ? due > o.due : order > o.order; }
    };
    using Queue = std::priority_queue<Held, std::vector<Held>, std::greater<Held>>;

    void hold(Queue& queue, double now, const Address& remote, const uint8_t* data, size_t size);

    PacketIo& inner_;
    Random random_;
    LagSettings default_;
    std::map<uint64_t, LagSettings> perRemote_;
    Queue outgoing_;
    Queue incoming_;
    uint64_t order_ = 0;
    uint64_t lost_ = 0;
};

} // namespace ac::net
