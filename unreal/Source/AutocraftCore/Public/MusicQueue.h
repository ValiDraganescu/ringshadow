// Port of Sources/GameCore/MusicQueue.swift.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace ac {

/// Swift's `RandomNumberGenerator` algorithms on any generator with a
/// `uint64_t next()`, computed as the Swift standard library computes them,
/// so a seeded queue plays the same order as the Swift game's.
namespace swiftrandom {
    /// `multipliedFullWidth(by:)` without a 128-bit type (MSVC has none).
    inline void multipliedFullWidth(uint64_t a, uint64_t b, uint64_t& high, uint64_t& low) {
        const uint64_t aLo = a & 0xffffffffu, aHi = a >> 32, bLo = b & 0xffffffffu, bHi = b >> 32;
        const uint64_t ll = aLo * bLo, lh = aLo * bHi, hl = aHi * bLo, hh = aHi * bHi;
        const uint64_t mid = (ll >> 32) + (lh & 0xffffffffu) + (hl & 0xffffffffu);
        low = (mid << 32) | (ll & 0xffffffffu);
        high = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
    }
    /// `next(upperBound:)` for `UInt64`: Lemire's nearly divisionless method.
    template <class G> uint64_t next(G& g, uint64_t upperBound) {
        uint64_t random = g.next();
        uint64_t high = 0, low = 0;
        multipliedFullWidth(random, upperBound, high, low);
        if (low < upperBound) {
            const uint64_t t = (0 - upperBound) % upperBound;
            while (low < t) {
                random = g.next();
                multipliedFullWidth(random, upperBound, high, low);
            }
        }
        return high;
    }
    /// `Int.random(in: lower..<upper, using:)`.
    template <class G> int64_t random(G& g, int64_t lower, int64_t upper) {
        const uint64_t delta = static_cast<uint64_t>(upper) - static_cast<uint64_t>(lower);
        return static_cast<int64_t>(static_cast<uint64_t>(lower) + next(g, delta));
    }
    /// `shuffle(using:)`.
    template <class T, class G> void shuffle(std::vector<T>& v, G& g) {
        if (v.size() <= 1) return;
        int64_t amount = static_cast<int64_t>(v.size());
        size_t currentIndex = 0;
        while (amount > 1) {
            const int64_t r = random(g, 0, amount);
            amount -= 1;
            std::swap(v[currentIndex], v[currentIndex + static_cast<size_t>(r)]);
            currentIndex += 1;
        }
    }
} // namespace swiftrandom

/// `SystemRandomNumberGenerator`.
struct SystemRandom {
    std::random_device device;
    uint64_t next() { return (static_cast<uint64_t>(device()) << 32) ^ static_cast<uint64_t>(device()); }
};

/// The music player's playlist (docs/music.md): every track in a shuffled
/// order and the one playing. Past the last track it shuffles again,
/// never starting the new round on the track just played. A track listed
/// twice plays twice a round (the station's ads have weights, docs/music.md),
/// never twice running when the round allows it. Pure: the audio plays what
/// it says.
template <class Track>
struct MusicQueue {
    /// Read only (Swift's `private(set)`).
    std::vector<Track> order;
    /// The track playing, as an index into `order`.
    int64_t index = 0;

    /// Previous within this many seconds of a track's start goes back a
    /// track; later, it starts the track over.
    static constexpr double replayAfter = 3;

    template <class G> MusicQueue(std::vector<Track> tracks, G& g) : order(std::move(tracks)) {
        swiftrandom::shuffle(order, g);
        spread(std::nullopt);
    }

    explicit MusicQueue(std::vector<Track> tracks) : order(std::move(tracks)) {
        SystemRandom g;
        swiftrandom::shuffle(order, g);
        spread(std::nullopt);
    }

    std::optional<Track> current() const {
        return index >= 0 && index < static_cast<int64_t>(order.size()) ? std::optional<Track>(order[static_cast<size_t>(index)])
                                                                         : std::nullopt;
    }

    /// On to the next track; past the last, a new round.
    template <class G> void next(G& g) {
        if (order.empty()) return;
        if (index + 1 < static_cast<int64_t>(order.size())) { index += 1; return; }
        const Track last = order[static_cast<size_t>(index)];
        swiftrandom::shuffle(order, g);
        if (order.size() > 1 && order[0] == last) {
            std::swap(order[0], order[static_cast<size_t>(swiftrandom::random(g, 1, static_cast<int64_t>(order.size())))]);
        }
        spread(last);
        index = 0;
    }

    void next() {
        SystemRandom g;
        next(g);
    }

    /// Swaps a track that would play right after itself (after `before`
    /// at the start) with the first later one that differs, or, at the end
    /// of the round, with an earlier one whose neighbours both swaps keep
    /// apart. No-op when every track is listed once, so a seeded order
    /// stays Swift's.
    void spread(std::optional<Track> before) {
        const size_t n = order.size();
        // `t` may stand at `k`; `other`, the swap's other place, then holds
        // a different track, so it never clashes.
        const auto fits = [&](const Track& t, size_t k, size_t other) {
            const bool left = k == 0 ? before && *before == t : k - 1 != other && order[k - 1] == t;
            const bool right = k + 1 < n && k + 1 != other && order[k + 1] == t;
            return !left && !right;
        };
        for (size_t i = 0; i < n; ++i) {
            const std::optional<Track> prev = i == 0 ? before : std::optional<Track>(order[i - 1]);
            if (!prev || order[i] != *prev) continue;
            bool moved = false;
            for (size_t j = i + 1; j < n && !moved; ++j) {
                if (order[j] != *prev) { std::swap(order[i], order[j]); moved = true; }
            }
            for (size_t k = 0; k < i && !moved; ++k) {
                if (order[k] != order[i] && fits(order[i], k, i) && fits(order[k], i, k)) {
                    std::swap(order[i], order[k]);
                    moved = true;
                }
            }
        }
    }

    /// Previous, `position` seconds into the track playing: back to the
    /// track before it (true), or, past `replayAfter` or on the round's
    /// first track, this one over from the start (false).
    bool previous(double position) {
        if (!(position <= replayAfter && index > 0)) return false;
        index -= 1;
        return true;
    }
};

} // namespace ac
