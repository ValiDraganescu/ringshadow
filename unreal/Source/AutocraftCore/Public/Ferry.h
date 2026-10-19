// The Dropship's flight's stages (F2, docs/mmorpg-m8.md; C4,
// docs/haul-and-ferry.md): their seconds, the server's clock for the switch
// into the target region and the touch down, the client's for the picture.
// Apart from Flight.h (its prices) so the walk-in Dropship's state
// (`Shuttle`, Types.h) can hold a plan.
#pragma once

#include <algorithm>
#include <cstdint>

namespace ac {

/// The Dropship's flight (F2): it comes down onto the Landing Pad, the
/// Prospector walks up its ramp, it lifts off, banks through a couple of
/// turns, dashes across the sky, and comes in over the target pad. The
/// pilot is aboard (out of every region, untouchable) from the call to the
/// touch down. The server moves the pilot into the target region when the
/// approach starts (`switchAt`), and puts the Prospector on the target pad
/// at `total`; the client steps it out after (`stepOut`).
struct Ferry {
    /// The Dropship coming down onto the pad.
    static constexpr double descend = 3.0;
    /// The Prospector walking up the ramp, the ramp closing.
    static constexpr double board = 1.5;
    /// Up off the pad, climbing.
    static constexpr double lift = 2.5;
    /// The banking turns, the giant swinging through the view.
    static constexpr double turns = 3.0;
    /// The dash across the sky: a base and a share a grid step, at most
    /// `dashMost`.
    static constexpr double dashBase = 2.0;
    static constexpr double dashStep = 1.0;
    static constexpr double dashMost = 6.0;
    /// Slowing, coming down onto the target pad.
    static constexpr double approach = 4.0;
    /// After the touch down: the ramp down, the Prospector walking out
    /// (the client's; control is back at its end).
    static constexpr double stepOut = 1.5;

    struct Plan {
        double descend = Ferry::descend;
        double board = Ferry::board;
        double lift = Ferry::lift;
        double turns = Ferry::turns;
        double dash = dashBase;
        double approach = Ferry::approach;
        /// Seconds from the call to each stage's start.
        double boardAt() const { return descend; }
        double liftAt() const { return descend + board; }
        double turnsAt() const { return liftAt() + lift; }
        double dashAt() const { return turnsAt() + turns; }
        double approachAt() const { return dashAt() + dash; }
        /// The touch down.
        double total() const { return approachAt() + approach; }
        bool operator==(const Plan&) const = default;
    };

    /// The plan of a flight `steps` grid steps long; Short hop's share off
    /// the dash (`shortHop`: −0.2 a rank, as `SkillStat::flightTime`), and
    /// Ringwalker's `faster` (0.3: every stage 30% faster).
    static Plan plan(int64_t steps, double shortHop = 0, double faster = 0) {
        Plan p;
        p.dash = std::min(dashMost, dashBase + dashStep * double(steps > 0 ? steps : 0)) * std::max(0.0, 1 + shortHop);
        if (faster > 0) {
            const double k = 1 / (1 + faster);
            p.descend *= k;
            p.board *= k;
            p.lift *= k;
            p.turns *= k;
            p.dash *= k;
            p.approach *= k;
        }
        return p;
    }
};

} // namespace ac
