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

} // namespace ac
