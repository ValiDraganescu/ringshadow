// The region seen at an angle (`autocraft-region view`, docs/regions.md,
// "Seeing a region"): a software-rendered view of the region's ground, for
// an agent to judge its shapes the way a player meets them. The ground's
// heights ray-marched from a camera, lit by a low sun with cast shadows and
// haze; cliffs, the border's walls, free-standing rock and the scenery past
// the bounds in their own rock colours; rocks as bumps; ore, wells, pads,
// spawn points and sites as coloured marks, pads and spawns named.
#pragma once

#include "Preview.h"
#include "RegionFile.h"

#include <optional>
#include <string>
#include <vector>

namespace ac::regiontool {

struct ViewOptions {
    /// Compass degrees the camera looks from (overview: it stands that way
    /// from the target) or toward (`eye`): 0 north, 90 east, 180 south.
    double azimuth = 180;
    /// Degrees below the horizon the camera looks (nil: 35 for the
    /// overview, 2 for the eye).
    std::optional<double> pitch;
    /// What it looks at (overview) or stands on (eye), region cells (nil:
    /// the region's middle; the eye: the first pad).
    std::optional<Vec2> at;
    /// Cells from the target (overview; nil: framing the whole region).
    std::optional<double> distance;
    /// Horizontal field of view, degrees (nil: 40 overview, 75 eye).
    std::optional<double> fov;
    int width = 1280, height = 720;
    /// A pilot's eye: 1.3 cells over the ground at `at`.
    bool eye = false;
};

/// The options after `view FILE` (`--from`, `--pitch`, `--at`, `--dist`,
/// `--fov`, `--size`, `--eye`); nil with `error` set when one is wrong.
std::optional<ViewOptions> viewOptions(const std::vector<std::string>& args, std::string& error);

Image view(const region::RegionFile& file, const region::Built& built, const ViewOptions& options);

} // namespace ac::regiontool
