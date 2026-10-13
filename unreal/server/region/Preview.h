// The region preview (`autocraft-region preview`, docs/regions.md): the
// region from above for an agent to look at. Height shading, cliffs, rocks,
// ore deposits, MH wells, pads, spawn points, edge openings, the problems
// `validate` found, and a cell grid labelled on all four sides.
#pragma once

#include "RegionFile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ac::regiontool {

struct Image {
    int width = 0, height = 0;
    /// RGB, rows top first.
    std::vector<uint8_t> rgb;
};

/// `scale`: pixels a cell; 0 picks one that makes the region about 1400
/// pixels wide.
Image preview(const region::RegionFile& file, const region::Built& built, const std::vector<region::Problem>& problems,
              int scale = 0);

/// PNG; false when the file cannot be written.
bool writePng(const Image& image, const std::string& path);

} // namespace ac::regiontool
