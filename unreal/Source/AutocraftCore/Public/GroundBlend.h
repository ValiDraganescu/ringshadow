// Where ground palettes meet (docs/regions.md, "ground"; the world-building
// skill's GROUND-LOOK.md, "Where palettes meet"). Presentation only: the sim
// never reads it.
//
// Two regions' palettes meet across their shared edge in a band 34 cells
// wide whose middle wanders up to 10 cells off the edge line (low noise,
// wavelength 56 cells). Across the band the far palette shows as patches (a
// finer noise, 6 cells, thresholded by the place in the band): pockets of
// the neighbour's ground in this one's, tongues of this one in the
// neighbour's. Past the band (27 cells from the edge) nothing changes.
//
// One truth: the field is a function of the world-grid point and of the
// grid line's two regions' ids, so both regions' materials (each draws only
// its own ground) and the scenery plans of both get the same answer at a
// point. A corner is the two edges' fields together: across the nearer
// vertical line (`a`) and the nearer horizontal one (`b`), the palettes
// weigh own (1-a)(1-b), the side neighbour a(1-b), the one ahead (1-a)b and
// the diagonal one ab. Along a grid line the seeds of its two segments meet
// at the corner, cross-faded over 12 cells each way.
//
// M_Terrain has the same functions in HLSL (Tools/Editor/make_terrain_material.py,
// `Lab::Gb*`), written to do the same float operations in the same order;
// the table below is the one place the numbers live (the script reads it
// into the shader). The world's grid is a full rectangle of regions at
// least 2 × `reach()` cells across (a medium region is 144 × 96).
#pragma once

#include "AutocraftCoreApi.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace ac::groundblend {

// The numbers. Tools/Editor/make_terrain_material.py reads the lines
// between the markers (`inline constexpr <float|uint32_t> name = value;`)
// into the shader as `#define GB_name value`: keep them literal.
// groundblend-table begin
/// The boundary's furthest wander off the edge line, cells.
inline constexpr float wanderCells = 10.0f;
/// The wander's first octave's wavelength, cells (three octaves).
inline constexpr float wanderWave = 56.0f;
/// The band across the wandering line in which the patches mix, cells.
inline constexpr float bandCells = 34.0f;
/// The patches' first octave's wavelength, cells (two octaves).
inline constexpr float patchWave = 6.0f;
/// The patches stretched along the prevailing wind, times.
inline constexpr float patchStretch = 1.7f;
/// The warp that curls them: its wavelength and how far it moves them, cells.
inline constexpr float warpWave = 18.0f;
inline constexpr float warpCells = 5.0f;
/// How far the patch noise is stretched round its middle (its spread made
/// near even, so the patches' share follows the place in the band).
inline constexpr float patchContrast = 2.2f;
/// A patch's soft rim, in the band's 0-1.
inline constexpr float patchSoft = 0.1f;
/// Half the cross-fade of a grid line's two segments' seeds at a corner, cells.
inline constexpr float seamCells = 12.0f;
/// The lattice hash (an integer mix, the same on the CPU and the GPU).
inline constexpr uint32_t hashX = 0x8da6b343u;
inline constexpr uint32_t hashY = 0xd8163841u;
inline constexpr uint32_t hashS = 0xcb1ab31fu;
inline constexpr uint32_t mixA = 0x7feb352du;
inline constexpr uint32_t mixB = 0x846ca68bu;
/// The patches' seed stream, apart from the wander's.
inline constexpr uint32_t patchSalt = 0x68e31da4u;
// groundblend-table end

/// How far from an edge line the field reaches, cells: past it, 0 or 1.
constexpr float reach() { return wanderCells + bandCells * 0.5f; }

/// The seed of a grid line's segment between two regions: `minus` the one
/// west (a vertical line) or north (a horizontal one), `plus` the other; ""
/// for no region. 24 bits (a float holds it exactly: the shader's parameter).
AUTOCRAFTCORE_API uint32_t seed(std::string_view minus, std::string_view plus);

/// Smooth value noise on the integer lattice, 0-1.
AUTOCRAFTCORE_API float value(float x, float y, uint32_t seed);
/// The wander's noise at a point, -1 to 1 (three octaves).
AUTOCRAFTCORE_API float wander(float x, float y, uint32_t seed);
/// The patches' noise at a point, 0-1 (two octaves, stretched).
AUTOCRAFTCORE_API float patch(float x, float y, uint32_t seed);
/// The plus side's weight at `d` cells from the edge line toward it, with
/// the wander's and the patches' noise there (`w` -1 to 1, `m` 0-1).
AUTOCRAFTCORE_API float plusOf(float d, float w, float m);

/// One grid line's field at point (x, y): the plus side's weight (east of a
/// vertical line, south of a horizontal one). `d`: cells from the line
/// toward its plus side; `t`: the point's place along the line, `t0` the
/// corner there; `minus`/`plus`: the seeds of the segments before and after
/// the corner along the line (north/south, west/east).
AUTOCRAFTCORE_API float line(float d, float x, float y, float t, float t0, uint32_t minus, uint32_t plus);

/// A region's place on the grid and what is round it: the slots N, E, S, W
/// (the edges), NE, SE, SW, NW (the corners).
struct Frame {
    /// The region's rectangle, world-grid cells (x east, y south).
    float minX = 0, minY = 0, maxX = 0, maxY = 0;
    /// A region beyond each edge (N, E, S, W): none, no blend there.
    std::array<bool, 4> has{};
    /// The seeds across each edge (N, E, S, W): `seed(N, own)`,
    /// `seed(own, E)`, `seed(own, S)`, `seed(W, own)`.
    std::array<uint32_t, 4> edge{};
    /// At each corner (NE, SE, SW, NW), the seed of the vertical line's
    /// segment beyond it (NE: `seed(N, NE)`, SE: `seed(S, SE)`, SW:
    /// `seed(SW, S)`, NW: `seed(NW, N)`) and of the horizontal one's (NE:
    /// `seed(NE, E)`, SE: `seed(E, SE)`, SW: `seed(W, SW)`, NW: `seed(NW, W)`).
    std::array<uint32_t, 4> beyondV{}, beyondH{};
};

/// A frame from the ids round a region (own, then N, E, S, W, NE, SE, SW,
/// NW; "" none) and its rectangle.
AUTOCRAFTCORE_API Frame frame(float minX, float minY, float maxX, float maxY, const std::array<std::string_view, 9>& ids);

/// Slots: 0 own, 1-4 N, E, S, W, 5-8 NE, SE, SW, NW.
enum Slot : int { own = 0, north, east, south, west, northEast, southEast, southWest, northWest };

/// The field at a point in a frame: across the nearer vertical line (`a`,
/// toward the side away from the region) and the nearer horizontal one
/// (`b`), and the slots they name.
struct Field {
    float a = 0, b = 0;
    int side = east, ahead = south, diagonal = southEast;
    /// The four palettes' weights (they sum to 1).
    float ownWeight() const { return (1.0f - a) * (1.0f - b); }
    float sideWeight() const { return a * (1.0f - b); }
    float aheadWeight() const { return (1.0f - a) * b; }
    float diagonalWeight() const { return a * b; }
    /// Every slot's weight.
    std::array<float, 9> weights() const;
};
AUTOCRAFTCORE_API Field field(const Frame& f, float x, float y);

/// The slot a thing at (x, y) takes, `u` (0-1) its draw: each palette as
/// often as it weighs there (a pebble in a tongue of ash is ash's).
AUTOCRAFTCORE_API int pick(const Frame& f, float x, float y, double u);

} // namespace ac::groundblend
