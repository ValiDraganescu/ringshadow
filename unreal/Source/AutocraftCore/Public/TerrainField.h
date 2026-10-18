// Port of Sources/GameCore/TerrainField.swift.
#pragma once

#include "AutocraftCoreApi.h"

#include "SimdMath.h"
#include "Noise.h"
#include "Projection.h"
#include "Types.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace ac {

/// Continuous terrain derived from a map: height and ground-material weights
/// at any ground point. The renderer samples it into a mesh and a splat map;
/// the simulation samples it to stand units on the ground.
struct AUTOCRAFTCORE_API TerrainField {
    MapDefinition map;
    Noise noise;
    /// Half width of a cliff face, cells.
    double cliffHalf = 0.6;

    /// `(lo: Vec2, hi: Vec2)`.
    struct Bounds { Vec2 lo, hi; };
    /// Each plateau's polygon bounds: past them by `edgeReach` a plateau
    /// covers nothing, so its polygon need not be measured.
    std::vector<Bounds> plateauBounds;
    /// How far past its polygon a plateau's edge reaches: the cliff's half
    /// width plus the most the wobble moves it (fbm is within ±1).
    double edgeReach() const { return cliffHalf + 1.3 + 0.25 + 0.01; }
    /// Each height edit's reach (`MapDefinition::heightEdits`): past it the
    /// edit changes nothing.
    std::vector<Bounds> editBounds;
    /// A polygon's edges in runs of `run`, each with the box round its
    /// edges' ends: `signedDistance` bit for bit, without measuring the
    /// runs that can hold neither the nearest edge nor a crossing of the
    /// point's row.
    struct PolygonRuns {
        static constexpr size_t run = 8;
        static constexpr size_t bands = 32;
        /// The polygon's points when the runs were made (another polygon:
        /// measured whole).
        size_t points = 0;
        /// Run `k`: edges `k * run` up to `(k + 1) * run` (edge `i` from
        /// point `i` to the one before it, as `signedDistance` walks).
        std::vector<Bounds> boxes;
        /// The runs whose boxes reach into each of `bands` rows across the
        /// polygon's height, from `bandLo`, `bandStep` tall.
        std::vector<std::vector<uint16_t>> band;
        double bandLo = 0, bandStep = 1;
        size_t bandOf(double y) const;
        PolygonRuns() = default;
        explicit PolygonRuns(const std::vector<Vec2>& poly);
        /// `ac::signedDistance`, bit for bit where its size is under
        /// `cap` (further: some distance of at least `cap`, same sign).
        double signedDistance(Vec2 p, const std::vector<Vec2>& poly,
                              double cap = std::numeric_limits<double>::infinity()) const;
        /// `signedDistance`'s inside (its sign), from the runs that cross
        /// the point's row alone.
        bool inside(Vec2 p, const std::vector<Vec2>& poly) const;
    };
    /// The region's outline and islands (`MapDefinition::edges`) in runs.
    PolygonRuns outlineRuns;
    std::vector<PolygonRuns> islandRuns;

    explicit TerrainField(const MapDefinition& map_);

    /// `ac::landOutside` for the region's own outline and islands, bit for
    /// bit, by their runs (a walking grid asks it some 20 times a cell).
    double landOutside(Vec2 p) const;
    /// The same where it lies between 0 and `cap`; on the land some value
    /// of at most 0, past `cap` some value of at least `cap`.
    double landOutside(Vec2 p, double cap) const;

    /// Where noise is sampled for `p`: on a mirrored map the east half reads
    /// the west half's noise, so the two halves are one shape.
    Vec2 fold(Vec2 p) const {
        // A square-symmetric map (C++ only) reads the noise of the eighth
        // 0 <= z <= x: every start has the same ground.
        if (map.squareSymmetric == true) {
            const double x = std::fabs(p.x), z = std::fabs(p.y);
            return z > x ? Vec2(z, x) : Vec2(x, z);
        }
        if (!map.mirrorX || !(p.x > *map.mirrorX)) return p;
        const double m = *map.mirrorX;
        return Vec2(2 * m - p.x, p.y);
    }

    /// How far the noise moves every plateau's edge at `p` (the same for
    /// each plateau, so worked out once a point).
    double edgeWobble(Vec2 p) const;

    /// Plateau `k`'s coverage in [0, 1] (1 = on top), with the point's
    /// `wobble`. Exactly 0 well away from it, without measuring its polygon.
    double plateauCoverage(Vec2 p, size_t k, double wobble) const;

    /// `(height: Double, crag: Double)`.
    struct Cliff { double height, crag; };
    /// Height before ramps and base pads: the stacked plateaus plus craggy
    /// noise on the cliff faces.
    Cliff cliffHeight(Vec2 p) const;

    /// `(weight: Double, height: Double)`.
    struct RampBlend { double weight, height; };
    /// Ramp override: blend weight and the ramp's own height.
    RampBlend ramp(Vec2 p) const;

    /// Terrain height at a ground point.
    double height(Vec2 p) const;

    /// How much height edit `k` (`MapDefinition::heightEdits`) holds at `p`
    /// in [0, 1]: 1 inside its outline (a ridge: within half its width),
    /// falling to 0 over its slope past it (a cliff's: `2 * cliffHalf`).
    /// A ramp's is its own blend.
    double editCoverage(Vec2 p, size_t k) const;

    /// A cut's walls (Z1): how far out they step at `p`, a shelf half way
    /// up `cutShelfMin` to `cutShelfMax` cells wide in stretches (0: a
    /// sheer wall).
    double cutShelf(Vec2 p) const;
    static constexpr double cutShelfMin = 1.5;
    static constexpr double cutShelfMax = 2.5;

    /// `h`, the ground's height at `p` before the height edits, after them.
    double edited(Vec2 p, double h) const;

    /// How much a region's walls hold at `p` in [0, 1] (`MapDefinition::edges`:
    /// 1 past the outline by a cliff's width; within `RegionEdges::edgeBand`
    /// of an edge, 1 where the edge is walled and 0 in its gaps, so the two
    /// sides of a shared edge meet exactly), how near an edge it is (1 on an
    /// edge, 0 past the band) and how far past the outline (negative: inside
    /// it). 0 with no walls.
    /// `rise`: how far up its height the ground has climbed, which is
    /// `coverage` but over a sloped rim (`RegionEdges::slope`), where the
    /// ground climbs over the slope while the walking grid still ends a
    /// cliff's width past the outline. `island`: the island `p` is on, -1
    /// none.
    struct Wall { double coverage = 0, edge = 0, outside = 0, rise = 0; int64_t island = -1; };
    Wall wall(Vec2 p) const;
    /// `wall(p)` with its `outside` as `landOutside(p, wallReach())` gives
    /// it: all `walled`, `materials` and the walking grid read of it.
    Wall wallNear(Vec2 p) const;
    /// Past this far beyond the outline every smoothstep `walled` takes of
    /// the distance stands at 1 (the deepest terrace's shelf, 10.5 cells,
    /// plus a cliff's width, and a cell to spare).
    double wallReach() const { return 2.5 + 8.0 + max(2 * cliffHalf, map.edges ? map.edges->slope : 0.0) + 1.0; }
    /// The island `p` is inside, -1 none.
    int64_t islandAt(Vec2 p) const;
    /// `wall` with `outside` given.
    Wall wallAt(Vec2 p, double outside) const;
    /// On the region's land (inside its outline, out of its islands) and
    /// off every edge's band: there `wall(p).coverage` is exactly 0, known
    /// without measuring how far the outline is. False says nothing.
    bool openLand(Vec2 p) const;
    /// `h` after the region's walls: raised to their level (higher further
    /// from the outline) where they stand.
    double walled(Vec2 p, double h) const;

    /// How far a ground point lies outside the map's bounds, cells (0 on it).
    double outside(Vec2 p) const;

    /// Ground for the scenery past the map's edge, out of play: the map's
    /// own ground at the edge, rising over a dozen cells into rough highland
    /// and ridges further out, so the view never ends in the void.
    double borderHeight(Vec2 p) const;

    /// Discrete level under a point (for placement), ignoring cliff blending.
    int64_t level(Vec2 p) const;

    /// `SIMD4<Double>`: grass, highland, plating, scorch.
    struct Materials {
        double x = 0, y = 0, z = 0, w = 0;
        bool operator==(const Materials&) const = default;
    };
    /// Ground material weights: grass, highland, plating, scorch. Dirt fills
    /// whatever is left. Cliff rock is chosen by slope in the renderer.
    Materials materials(Vec2 p) const;

    /// The ground point under canvas point `c`: the first place the camera
    /// ray meets the terrain surface, so a click on a cliff top picks the
    /// plateau, not the low ground behind it.
    Vec2 pick(Vec2 c, const CanvasProjection& projection) const;

    /// The same for a ray from `o` along `direction` (a camera that has
    /// moved off the projection's focus); nil when it points at the sky.
    std::optional<Vec2> pick(Vec3 o, Vec3 direction) const;
};

inline double smoothstep(double e0, double e1, double x) {
    const double t = min(max((x - e0) / (e1 - e0), 0.0), 1.0);
    return t * t * (3 - 2 * t);
}

/// Signed distance from `p` to a polygon: negative inside.
double signedDistance(Vec2 p, const std::vector<Vec2>& poly);

/// How far `p` lies past a region's playable land (W4b): outside its
/// outline or inside one of its islands, cells (negative: on the land).
double landOutside(Vec2 p, const std::vector<Vec2>& outline, const std::vector<std::vector<Vec2>>& islands);

} // namespace ac
