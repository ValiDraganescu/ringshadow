// Others drawn in the past (docs/networking.md, "The tick"): the client
// shows every unit but the one it drives about 100 ms behind the server,
// blended between the two snapshots around that moment, so a lost snapshot
// still leaves two to blend between and the 20 Hz updates do not step.
//
// `heard` learns the server's clock from each snapshot's tick; `record`
// keeps where the units stood in each snapshot; `apply` writes the blend
// for a moment into the units the game draws (the mirror's state, rebuilt
// from the truth by the next snapshot). Position, heading, turret and the
// walk cycle's stride are blended; the rest is the newest snapshot's.
#pragma once

#include "AutocraftNetApi.h"

#include "Types.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <unordered_map>
#include <vector>

namespace ac::net {

class AUTOCRAFTNET_API Interpolator {
public:
    /// How far in the past others are drawn, seconds.
    double delay = 0.1;
    /// The server's step, seconds a tick.
    double step = 1.0 / 60;
    /// Snapshots kept, seconds.
    double keep = 1.0;

    /// A snapshot of server tick `serverTick` arrived at local time `now`.
    void heard(int64_t serverTick, double now);
    /// The server tick to draw at local time `now` (fractional): the
    /// server's clock as estimated, less `delay`. Nil before any snapshot.
    std::optional<double> renderTick(double now) const;

    /// Where `units` stand after the snapshot of `serverTick` (a later part
    /// of the same snapshot overwrites its record).
    void record(int64_t serverTick, const std::vector<Unit>& units);
    /// Writes the blend at `tick` into `units`, except the unit `skip`.
    /// Before the oldest record: the oldest; after the newest: the newest
    /// (never ahead of what was sent).
    void apply(double tick, std::vector<Unit>& units, std::optional<int64_t> skip) const;

    void reset();
    size_t records() const { return frames_.size(); }

private:
    struct Pose {
        Vec2 position;
        double heading = 0;
        std::optional<double> aim;
        double stride = 0;
    };
    struct Frame {
        int64_t tick = 0;
        std::unordered_map<int64_t, Pose> poses;
    };
    std::deque<Frame> frames_;
    /// Server time minus local time, seconds (smoothed).
    std::optional<double> offset_;
};

} // namespace ac::net
