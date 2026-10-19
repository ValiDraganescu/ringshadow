// C4, the drive (docs/haul-and-ferry.md "The drive"): how a vehicle sits on
// the ground under its wheels or tracks. The simulation's units stand on a
// point; the game draws a vehicle pitched and rolled to the ground under its
// contacts (`AcPose`), and `Simulation::climbRefused` measures the same
// pitch along the way it goes.
#pragma once

#include "Rules.h"
#include "SimdMath.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ac {

/// A vehicle's body on the ground: pitch (nose up, radians), roll (its
/// right side up, radians), and how far its middle sits above the ground
/// right under it (cells: above 0 over a dip, below 0 on a crest), from
/// the ground under its four contacts.
struct GroundTilt {
    double pitch = 0, roll = 0, lift = 0;
    bool operator==(const GroundTilt&) const = default;
};

/// Where a ground kind touches the ground, in its own frame (x ahead, y to
/// its right): half its `wheelbase` ahead and behind, 0.8 of its radius
/// (half its width) to each side.
/// Nil for legs and the air, which stand level.
struct Contacts {
    double halfLength = 0, halfWidth = 0;
    static std::optional<Contacts> of(UnitKind k) {
        const UnitStats& st = Rules::stats(k);
        if (st.gear != RunningGear::wheels && st.gear != RunningGear::tracks) return std::nullopt;
        return Contacts{0.5 * std::max(st.wheelbase, 2 * st.radius), 0.8 * st.radius};
    }
};

/// The body over the ground `height(Vec2)` at `at`, facing `heading`: the
/// plane through its four contacts' ground (front pair against the back
/// pair for the pitch, right pair against the left for the roll), so a
/// crest or a ditch under its middle reads as the wheels feel it.
template <class Height>
GroundTilt groundTilt(const Height& height, Vec2 at, double heading, const Contacts& c) {
    const Vec2 ahead(std::cos(heading), std::sin(heading)), right(-std::sin(heading), std::cos(heading));
    const Vec2 f = ahead * c.halfLength, r = right * c.halfWidth;
    const double fl = height(at + f - r), fr = height(at + f + r), bl = height(at - f - r), br = height(at - f + r);
    GroundTilt t;
    t.pitch = std::atan2(0.5 * (fl + fr - bl - br), 2 * c.halfLength);
    t.roll = std::atan2(0.5 * (fr + br - fl - bl), 2 * c.halfWidth);
    t.lift = 0.25 * (fl + fr + bl + br) - height(at);
    return t;
}

/// How a car-like vehicle at `at` facing `heading` (its tightest arc
/// `radius`) makes for `target`: `way` 1 ahead or -1 back, `steer` -1 (full
/// left) to 1 (full right), its arc's curvature `steer / radius`. Ahead, it
/// steers onto the arc through the target (full lock when that is tighter,
/// or the target is behind); a target inside its tightest circle on that
/// side, which no forward arc reaches, it backs away from on the other
/// lock, the nose swinging toward it (a three-point turn), until the
/// target is outside. `Simulation::roll` (the AI) drives by it.
struct SteerTo {
    double way = 1, steer = 0;
    bool operator==(const SteerTo&) const = default;
};
inline SteerTo steerTo(Vec2 at, double heading, Vec2 target, double radius) {
    const Vec2 d = target - at;
    const double dist = length(d);
    if (!(dist > 1e-9) || !(radius > 0)) return SteerTo{};
    const Vec2 ahead(std::cos(heading), std::sin(heading)), right(-std::sin(heading), std::cos(heading));
    const double along = dot(d, ahead), side = dot(d, right);
    // The side it turns to: the target's (dead behind: the right).
    const double toward = side < 0 ? -1.0 : 1.0;
    if (distance(target, at + right * (toward * radius)) < radius) return SteerTo{-1, -toward};
    if (along <= 0) return SteerTo{1, toward};
    // The arc tangent to its heading through the target.
    return SteerTo{1, std::clamp(2 * side * radius / (dist * dist), -1.0, 1.0)};
}

} // namespace ac
