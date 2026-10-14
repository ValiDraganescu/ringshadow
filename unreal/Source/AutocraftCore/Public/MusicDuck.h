// The radio ducking under a mission narration (A1, docs/mmorpg-m8.md): the
// music (KSTR 88.7) falls to `floorDb` over `attack` seconds when a narration
// starts, stays down until it ends, and comes back over `release` seconds. A
// second narration while it is down keeps it down. Effects and voice lines
// are not ducked: only the music player multiplies its gain by this one.
// Engine-free, on the music player's clock (seconds).
#pragma once

#include <algorithm>
#include <cmath>
#include <optional>

namespace ac {

struct MusicDuck {
    /// How low the music goes, in dB (about a sixteenth of its gain).
    static constexpr double floorDb = -24.0;
    /// The fade down, and back up after the narration, in seconds.
    static constexpr double attack = 0.3;
    static constexpr double release = 1.0;

    /// The floor as a gain (about 0.063).
    static double floorGain() { return std::pow(10.0, floorDb / 20.0); }

    /// A narration starts at `now` and lasts `seconds` (0 or less: until
    /// `end`). One already playing is replaced: the music stays down.
    void start(double now, double seconds) {
        // From wherever it is now (part way back up, or already down).
        from = gain(now);
        startedAt = now;
        until = seconds > 0 ? now + seconds : 1e300;
        endedAt.reset();
    }
    /// The narration stopped (cut off, or the screen closed) at `now`.
    void end(double now) {
        if (!active(now)) return;
        endedFrom = gain(now);
        endedAt = now;
    }
    /// A narration is holding the music down at `now` (the fade back excluded).
    bool active(double now) const { return startedAt && !endedAt && now < until; }

    /// The music's gain factor at `now`: 1 at rest, `floorGain` while a
    /// narration plays, eased between.
    double gain(double now) const {
        if (!startedAt) return 1.0;
        const double floor = floorGain();
        // Its end: stopped early, or played out.
        const double stop = endedAt ? *endedAt : until;
        if (now < stop) {
            const double t = attack > 0 ? std::clamp((now - *startedAt) / attack, 0.0, 1.0) : 1.0;
            return from + (floor - from) * ease(t);
        }
        const double at = endedAt ? endedFrom : downAt(stop);
        const double t = release > 0 ? std::clamp((now - stop) / release, 0.0, 1.0) : 1.0;
        return at + (1.0 - at) * ease(t);
    }

private:
    static double ease(double t) { return t * t * (3 - 2 * t); }
    double downAt(double now) const {
        const double t = attack > 0 ? std::clamp((now - *startedAt) / attack, 0.0, 1.0) : 1.0;
        return from + (floorGain() - from) * ease(t);
    }

    std::optional<double> startedAt;
    std::optional<double> endedAt;
    double until = 0;
    double from = 1.0;
    double endedFrom = 1.0;
};

} // namespace ac
