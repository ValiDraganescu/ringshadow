// The siege heard (A2, docs/music.md "Siege layers"). Engine-free, on the
// music player's clock (seconds):
//
// - `SiegeMix`: two music layers over KSTR 88.7 while a canyon is fought
//   near the pilot. The wave layer rises while a wave is out (or the game
//   runs between waves), louder with the wave's number; the Matriarch layer
//   comes in while she comes for a canyon's turrets (FAILED) or stands in
//   sight; both fade out on HELD, a reset, or with no canyon near. The
//   radio ducks under them as under a narration (`MusicDuck::floorDb`) and
//   comes back up over `radioRelease` once they are gone.
// - `VoiceQueue`: KSTR's spoken lines (the goals, the canyon news). One at a
//   time, never over a narration (narration first: a line waits for it),
//   `gap` seconds between two, the oldest dropped past `most` waiting, a line
//   that waited longer than `stale` dropped, the same line twice in a row
//   kept once.
#pragma once

#include "MusicDuck.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <optional>
#include <string>

namespace ac {

struct SiegeMix {
    /// What the pilot is near (the zone in its region, else next door).
    struct Near {
        /// Its game: 0 waiting, 1 running, 2 failed, 3 held (`Sky::Zone::State`).
        int state = 0;
        int64_t wave = 0;
        int64_t waves = 10;
        /// A wave's bugs are out now.
        bool out = false;
        /// The Matriarch stands in the pilot's region.
        bool matriarch = false;
    };
    /// The layers' fades, seconds for the whole way.
    static constexpr double fadeIn = 3.0;
    static constexpr double fadeOut = 4.0;
    /// The radio's fall under the layers, and its return after them.
    static constexpr double radioAttack = 0.5;
    static constexpr double radioRelease = 2.0;
    /// The wave layer's gain: `waveLow` at wave 1 rising to 1 at the last.
    static constexpr double waveLow = 0.45;

    /// The wave layer's target for `n` (nil: no zone near).
    static double waveTarget(const std::optional<Near>& n) {
        if (!n) return 0;
        // A wave out, or the game running between waves; waiting with
        // wave 1 out too (the opening wave). Not FAILED (her layer), not HELD.
        const bool on = n->state == 1 || (n->state == 0 && n->out);
        if (!on) return 0;
        const double k = n->waves > 1 ? double(std::clamp<int64_t>(n->wave, 1, n->waves) - 1) / double(n->waves - 1) : 1.0;
        return waveLow + (1.0 - waveLow) * k;
    }
    /// FAILED (her siege), or she stands in the pilot's region, unless the
    /// canyon is HELD: HELD ends both layers.
    static double matriarchTarget(const std::optional<Near>& n) { return n && n->state != 3 && (n->state == 2 || n->matriarch) ? 1.0 : 0.0; }

    /// A step of `dt` seconds with what is near now.
    void step(double dt, const std::optional<Near>& n) {
        wave = toward(wave, waveTarget(n), dt);
        matriarch = toward(matriarch, matriarchTarget(n), dt);
        const bool on = waveTarget(n) > 0 || matriarchTarget(n) > 0 || wave > 1e-3 || matriarch > 1e-3;
        const double floor = MusicDuck::floorGain();
        if (on) radio = std::max(floor, radio - (1.0 - floor) * dt / radioAttack);
        else radio = std::min(1.0, radio + (1.0 - floor) * dt / radioRelease);
    }
    /// The layers' gains (0…1) and the radio's factor (1 at rest).
    double wave = 0;
    double matriarch = 0;
    double radio = 1;

private:
    static double toward(double v, double target, double dt) {
        if (v < target) return std::min(target, v + dt / fadeIn);
        return std::max(target, v - dt / fadeOut);
    }
};

struct VoiceQueue {
    struct Line {
        std::string key;
        /// Its length, and when it was asked for.
        double seconds = 0;
        double asked = 0;
    };
    static constexpr double gap = 0.6;
    static constexpr size_t most = 4;
    static constexpr double stale = 20.0;

    /// A line asked for at `now` (the same as the last waiting one: kept once).
    void push(double now, const std::string& key, double seconds) {
        if (!waiting.empty() && waiting.back().key == key) return;
        if (speaking && speaking->key == key && now < speakingUntil) return;
        waiting.push_back(Line{key, seconds, now});
        while (waiting.size() > most) waiting.pop_front();
    }
    /// A narration (a mission's briefing) plays from `now` for `seconds`:
    /// no line starts until it is over, and one speaking now is cut.
    void narration(double now, double seconds) {
        narrationUntil = std::max(narrationUntil, now + seconds);
        if (speaking && now < speakingUntil) {
            speaking.reset();
            speakingUntil = now;
        }
    }
    /// The narration was stopped early at `now`.
    void narrationEnded(double now) { narrationUntil = std::min(narrationUntil, now); }
    /// The line to start at `now`, if one may (it is then speaking).
    std::optional<Line> next(double now) {
        while (!waiting.empty() && now - waiting.front().asked > stale) waiting.pop_front();
        if (waiting.empty() || now < narrationUntil + gap || now < speakingUntil + gap) return std::nullopt;
        Line l = waiting.front();
        waiting.pop_front();
        speaking = l;
        speakingUntil = now + l.seconds;
        return l;
    }
    /// A line is speaking at `now`.
    bool busy(double now) const { return speaking && now < speakingUntil; }
    size_t size() const { return waiting.size(); }

private:
    std::deque<Line> waiting;
    std::optional<Line> speaking;
    double speakingUntil = -1e9;
    double narrationUntil = -1e9;
};

} // namespace ac
