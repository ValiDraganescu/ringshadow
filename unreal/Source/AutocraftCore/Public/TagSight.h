// A name tag over a unit (the HUD's "VESPER · LV 14", "MERC · FLINT"; P1)
// is drawn only where the pilot sees that unit: within the pilot's sight
// (its reach by the hour: the night's small circle, the day's whole
// region), and with nothing in the way between the eye and the unit's head,
// neither the ground (a ridge, a canyon's wall) nor a building's body. All
// in cells: ground points and heights over the ground's zero. Engine-free.
#pragma once

#include "Types.h"

#include <cmath>
#include <functional>
#include <optional>
#include <vector>

namespace ac {

struct TagSight {
    /// A building in the way: a box `half` cells round `at` (its footprint),
    /// from the ground at `base` up to `top`.
    struct Block {
        Vec2 at{0, 0};
        double half = 1;
        double base = 0;
        double top = 2;
    };
    /// The ray is sampled every `step` cells; the last `near` cells before
    /// the head and the first before the eye are not tested (the unit's own
    /// body, the camera's boom).
    static constexpr double step = 0.25;
    static constexpr double near = 0.8;

    /// The unit at ground point `unit`, its head `head` cells over the
    /// ground there (`ground(unit) + head`), seen from `eye` (a ground point)
    /// at `eyeHeight`. `sight`: the pilot's sight (its unit's place and
    /// reach; nil: no limit, the RTS view). `ground`: the ground's height.
    static bool seen(Vec2 unit, double head, Vec2 eye, double eyeHeight, std::optional<std::pair<Vec2, double>> sight,
                     const std::function<double(Vec2)>& ground, const std::vector<Block>& blocks) {
        if (sight && distance(unit, sight->first) > sight->second) return false;
        const double top = ground(unit) + head;
        const double length = distance(eye, unit);
        if (length < 1e-6) return true;
        for (double s = near; s < length - near; s += step) {
            const double t = s / length;
            const Vec2 p = eye + (unit - eye) * t;
            const double h = eyeHeight + (top - eyeHeight) * t;
            if (ground(p) > h) return false;
            for (const Block& b : blocks)
                if (std::abs(p.x - b.at.x) <= b.half && std::abs(p.y - b.at.y) <= b.half && h >= b.base && h <= b.top) return false;
        }
        return true;
    }
};

} // namespace ac
