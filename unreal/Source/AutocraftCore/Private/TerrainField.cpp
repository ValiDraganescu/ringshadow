// Port of Sources/GameCore/TerrainField.swift.
#include "TerrainField.h"

#include "RegionFile.h"
#include "ScreenConfig.h"

#include <cmath>
#include <string>
#include <limits>

namespace ac {

TerrainField::TerrainField(const MapDefinition& map_) : map(map_), noise(map_.seed) {
    const double inf = std::numeric_limits<double>::infinity();
    plateauBounds.reserve(map.plateaus.size());
    for (const Plateau& p : map.plateaus) {
        Vec2 lo = Vec2::repeating(inf), hi = Vec2::repeating(-inf);
        for (Vec2 q : p.polygon) lo = min(lo, q);
        for (Vec2 q : p.polygon) hi = max(hi, q);
        plateauBounds.push_back(Bounds{lo, hi});
    }
    editBounds.reserve(map.heightEdits.size());
    for (const HeightEdit& e : map.heightEdits) {
        Vec2 lo = Vec2::repeating(inf), hi = Vec2::repeating(-inf);
        for (Vec2 q : e.points) lo = min(lo, q);
        for (Vec2 q : e.points) hi = max(hi, q);
        // A ramp's blend reaches a third of its length past each end.
        const double along = e.kind == HeightEdit::Kind::ramp && e.points.size() == 2
                                 ? 0.4 * distance(e.points[0], e.points[1]) : 0.0;
        // A cut's walls step out by a shelf at most (`cutShelf`).
        const double shelf = e.kind == HeightEdit::Kind::cut ? cutShelfMax + 2 * cliffHalf : 0.0;
        const double reach = e.width / 2 * 1.25 + max(e.slope, 2 * cliffHalf) + along + shelf + 0.01;
        editBounds.push_back(Bounds{lo - Vec2::repeating(reach), hi + Vec2::repeating(reach)});
    }
}

namespace {

/// Distance from `p` to a line through `points`.
double lineDistance(Vec2 p, const std::vector<Vec2>& points) {
    if (points.size() == 1) return distance(p, points[0]);
    double d = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i + 1 < points.size(); i++) {
        const Vec2 a = points[i], e = points[i + 1] - a;
        const double len2 = dot(e, e);
        const double t = len2 > 0 ? min(max(dot(p - a, e) / len2, 0.0), 1.0) : 0.0;
        d = min(d, length(p - a - e * t));
    }
    return d;
}

} // namespace

double TerrainField::editCoverage(Vec2 p, size_t k) const {
    const HeightEdit& e = map.heightEdits[k];
    const Bounds& b = editBounds[k];
    if (p.x < b.lo.x || p.x > b.hi.x || p.y < b.lo.y || p.y > b.hi.y || e.points.empty()) return 0;
    if (e.kind == HeightEdit::Kind::ramp) {
        if (e.points.size() != 2) return 0;
        // As `ramp`.
        const Vec2 axis = e.points[1] - e.points[0];
        const double len = length(axis);
        if (!(len > 0)) return 0;
        const Vec2 dir = axis / len, perp = Vec2(-dir.y, dir.x);
        const double u = dot(p - e.points[0], dir) / len;
        const double v = std::fabs(dot(p - e.points[0], perp)) / (e.width / 2);
        return (1 - smoothstep(0.8, 1.2, v)) * smoothstep(-0.35, -0.02, u) * (1 - smoothstep(1.02, 1.35, u));
    }
    const bool line = e.kind == HeightEdit::Kind::ridge || e.kind == HeightEdit::Kind::cut;
    const double out = line ? lineDistance(p, e.points) - e.width / 2 : signedDistance(p, e.points);
    const double edge = e.slope > 0 ? e.slope : 2 * cliffHalf;
    // A cut's: out to its walls' tops, past a shelf where they step.
    if (e.kind == HeightEdit::Kind::cut && !(e.slope > 0)) return smoothstep(edge, 0.0, out - cutShelf(p));
    return smoothstep(edge, 0.0, out);
}

double TerrainField::cutShelf(Vec2 p) const {
    // As the region's walls' terraces (`walled`), on noise of its own.
    const Vec2 q = fold(p);
    const double step = noise.fbm(q.x * 0.06 - 41, q.y * 0.06 + 13);
    return smoothstep(0.14, 0.24, step) * (cutShelfMin + (cutShelfMax - cutShelfMin) * smoothstep(0.24, 0.5, step));
}

double TerrainField::edited(Vec2 p, double h) const {
    for (size_t k = 0; k < map.heightEdits.size(); k++) {
        const double t = editCoverage(p, k);
        if (!(t > 0)) continue;
        const HeightEdit& e = map.heightEdits[k];
        const double target = static_cast<double>(e.level) * map.levelHeight;
        switch (e.kind) {
        case HeightEdit::Kind::raise:
        case HeightEdit::Kind::ridge: h += max(target - h, 0.0) * t; break;
        case HeightEdit::Kind::lower: h -= max(h - target, 0.0) * t; break;
        case HeightEdit::Kind::cut: {
            // The floor within its width; where it is two levels deep or more
            // and the walls step, a shelf half way up, then the rest.
            const double depth = max(h - target, 0.0);
            const double shelf = !(e.slope > 0) && depth >= 1.5 * map.levelHeight ? cutShelf(p) : 0.0;
            const double out = lineDistance(p, e.points) - e.width / 2;
            const double edge = e.slope > 0 ? e.slope : 2 * cliffHalf;
            const double inner = smoothstep(edge, 0.0, out);
            const double outer = shelf > 0 ? t : inner;
            h -= depth * 0.5 * (inner + outer);
            // Craggy noise on each face, as on a plateau's, where it cuts.
            if (!(e.slope > 0)) {
                const Vec2 q = fold(p);
                h += 4 * (inner * (1 - inner) + outer * (1 - outer)) * smoothstep(0.0, map.levelHeight, depth) *
                     noise.fbm(q.x * 0.9 + 3.1, q.y * 0.9) * 0.45;
            }
            continue;
        }
        case HeightEdit::Kind::flatten: h += (target - h) * t; break;
        case HeightEdit::Kind::ramp: {
            const Vec2 axis = e.points[1] - e.points[0];
            const double s = min(max(dot(p - e.points[0], axis) / dot(axis, axis), 0.0), 1.0);
            const double low = static_cast<double>(e.fromLevel) * map.levelHeight;
            h += (low + (target - low) * (s * s * (3 - 2 * s)) - h) * t;
            break;
        }
        }
        // Craggy noise on a cliff's face, as on a plateau's.
        if (e.slope > 0 || e.kind == HeightEdit::Kind::flatten || e.kind == HeightEdit::Kind::ramp) continue;
        const Vec2 q = fold(p);
        h += 4 * t * (1 - t) * noise.fbm(q.x * 0.9 + 3.1, q.y * 0.9) * 0.45;
    }
    return h;
}

/// How far the noise moves every plateau's edge at `p` (the same for
/// each plateau, so worked out once a point).
double TerrainField::edgeWobble(Vec2 p) const {
    const Vec2 q = fold(p);
    return noise.fbm(q.x * 0.16, q.y * 0.16) * 1.3 + noise.fbm(q.x * 0.7, q.y * 0.7) * 0.25;
}

/// Plateau `k`'s coverage in [0, 1] (1 = on top), with the point's
/// `wobble`. Exactly 0 well away from it, without measuring its polygon.
double TerrainField::plateauCoverage(Vec2 p, size_t k, double wobble) const {
    const Bounds& b = plateauBounds[k];
    const double out = length(max(max(b.lo - p, p - b.hi), Vec2::zero));
    if (!(out < edgeReach())) return 0;
    const double sd = signedDistance(p, map.plateaus[k].polygon) + wobble;
    return smoothstep(cliffHalf, -cliffHalf, sd);
}

/// Height before ramps and base pads: the stacked plateaus plus craggy
/// noise on the cliff faces.
TerrainField::Cliff TerrainField::cliffHeight(Vec2 p) const {
    double h = 0.0, crag = 0.0;
    const double wobble = edgeWobble(p);
    for (size_t k = 0; k < map.plateaus.size(); k++) {
        const Plateau& plateau = map.plateaus[k];
        const double t = plateauCoverage(p, k, wobble);
        h = max(h, static_cast<double>(plateau.level) * map.levelHeight * t);
        crag = max(crag, 4 * t * (1 - t));
    }
    return Cliff{h, crag};
}

/// Ramp override: blend weight and the ramp's own height.
TerrainField::RampBlend TerrainField::ramp(Vec2 p) const {
    RampBlend best{0.0, 0.0};
    for (const Ramp& r : map.ramps) {
        const Vec2 axis = r.high - r.low;
        const double len = length(axis);
        if (!(len > 0)) continue;
        const Vec2 dir = axis / len;
        const Vec2 perp = Vec2(-dir.y, dir.x);
        const double u = dot(p - r.low, dir) / len;
        const double v = std::fabs(dot(p - r.low, perp)) / (r.width / 2);
        const double w = (1 - smoothstep(0.8, 1.2, v)) * smoothstep(-0.35, -0.02, u) * (1 - smoothstep(1.02, 1.35, u));
        if (w > best.weight) {
            const double hl = static_cast<double>(r.lowLevel) * map.levelHeight,
                         hh = static_cast<double>(r.highLevel) * map.levelHeight;
            const double s = min(max(u, 0.0), 1.0);
            best = RampBlend{w, hl + (hh - hl) * (s * s * (3 - 2 * s))};
        }
    }
    return best;
}

/// Terrain height at a ground point.
double TerrainField::height(Vec2 p) const {
    // The playground is level everywhere: anything goes down anywhere.
    if (map.playground == true) return 0;
    const Cliff c = cliffHeight(p);
    const double base = c.height, crag = c.crag;
    const Vec2 q = fold(p);
    double h = base + crag * noise.fbm(q.x * 0.9 + 3.1, q.y * 0.9) * 0.45;
    const RampBlend r = ramp(p);
    if (r.weight > 0) h = h + (r.height - h) * r.weight;
    // Gentle rolling ground everywhere, flattened under base sites.
    double roll = noise.fbm(q.x * 0.07, q.y * 0.07) * 0.22 + noise.fbm(q.x * 0.45, q.y * 0.45) * 0.05;
    for (const BaseSite& b : map.bases) {
        const double d = distance(p, b.center);
        const double flat = 1 - smoothstep(5.0, 8.5, d);
        if (flat > 0) {
            const double target = static_cast<double>(b.level) * map.levelHeight;
            h = h + (target - h) * flat * (1 - crag);
            roll *= 1 - flat;
        }
    }
    const double out = map.heightEdits.empty() ? h + roll : edited(p, h + roll);
    return map.edges && !map.edges->outline.empty() ? walled(p, out) : out;
}

TerrainField::Wall TerrainField::wall(Vec2 p) const {
    if (!map.edges || map.edges->outline.empty()) return Wall{};
    const RegionEdges& e = *map.edges;
    // A cliff's width past the outline the wall stands full.
    const double outside = landOutside(p, e.outline, e.islands);
    double cover = smoothstep(0.0, 2 * cliffHalf, outside);
    // Near an edge the gaps alone say: the same on both sides of it.
    double nearEdge = 0;
    for (int64_t k = 0; k < 4; k++) {
        const double d = max(RegionEdges::depth(map.bounds, k, p), 0.0);
        const double w = 1 - smoothstep(0.0, RegionEdges::edgeBand, d);
        if (!(w > 0)) continue;
        const double at = RegionEdges::along(map.bounds, k, p);
        double walled = 1;
        for (const RegionEdges::Gap& g : e.gaps) {
            if (g.edge != k) continue;
            // Open over the whole gap and `gapMargin` past each end, where
            // its cliff faces stand.
            const double a = g.from - RegionEdges::gapMargin, b = g.to + RegionEdges::gapMargin;
            const double open = smoothstep(a - 2 * cliffHalf, a, at) * (1 - smoothstep(b, b + 2 * cliffHalf, at));
            walled = min(walled, 1 - open);
        }
        cover += (walled - cover) * w;
        nearEdge = max(nearEdge, w);
    }
    return Wall{cover, nearEdge, outside};
}

namespace {

/// The walls' top along a shared edge over its level, levels (W4b): a
/// profile both sides compute alike, from where along the edge and the
/// edge's gaps alone (the same spans on both sides), so the two meet
/// exactly without a flat band; none near the corners, where four meet.
double seamRise(const RegionEdges& e, const GroundRect& b, int64_t edge, double at) {
    std::string key = edge % 2 == 0 ? "ns" : "ew";
    for (const RegionEdges::Gap& g : e.gaps)
        if (g.edge == edge) key += "|" + std::to_string(std::lround(g.from * 4)) + "-" + std::to_string(std::lround(g.to * 4));
    const uint64_t seed = fnv1a64(key);
    const auto lattice = [seed](int64_t i, uint64_t salt) {
        uint64_t x = seed ^ (static_cast<uint64_t>(i) * 0x9e3779b97f4a7c15ull) ^ salt;
        x ^= x >> 31; x *= 0xbf58476d1ce4e5b9ull; x ^= x >> 29;
        return static_cast<double>(x & 0xffff) / 65535.0;
    };
    const auto octave = [&](double spacing, uint64_t salt) {
        const double u = at / spacing, f = u - std::floor(u);
        const int64_t i = static_cast<int64_t>(std::floor(u));
        const double t = f * f * (3 - 2 * f);
        return lattice(i, salt) + (lattice(i + 1, salt) - lattice(i, salt)) * t;
    };
    const double lo = edge % 2 == 0 ? b.minX : b.minZ, hi = edge % 2 == 0 ? b.maxX : b.maxZ;
    const double ends = smoothstep(RegionEdges::edgeBand + 1, RegionEdges::edgeBand + 7, min(at - lo, hi - at));
    return ends * (0.15 + 1.1 * octave(11, 1) + 0.35 * octave(3.7, 2));
}

} // namespace

double TerrainField::walled(Vec2 p, double h) const {
    const Wall w = wall(p);
    if (!(w.coverage > 0)) return h;
    const RegionEdges& e = *map.edges;
    // The wall's top: its level, up to most of a level more along the way
    // (a broken skyline), rising another level or two further out, rough;
    // near an edge, the edge's own profile, the same on both sides of it.
    const Vec2 q = fold(p);
    const double free = 1 - w.edge;
    const double away = smoothstep(0.3, 7, w.outside);
    const double skyline = (0.5 + 0.5 * noise.fbm(q.x * 0.11 + 31, q.y * 0.11 - 17)) * 0.85;
    const double ridge = away * max(0.7 + 0.8 * noise.fbm(q.x * 0.07 - 7, q.y * 0.07 + 11), 0.0);
    const double rough = 0.18 * (0.5 + 0.5 * noise.fbm(q.x * 0.6 + 5, q.y * 0.6 - 3)) * away;
    double seam = 0, weights = 0;
    for (int64_t k = 0; k < 4; k++) {
        const double d = max(RegionEdges::depth(map.bounds, k, p), 0.0);
        const double wk = 1 - smoothstep(0.0, RegionEdges::edgeBand, d);
        if (!(wk > 0)) continue;
        seam += wk * seamRise(e, map.bounds, k, RegionEdges::along(map.bounds, k, p));
        weights += wk;
    }
    if (weights > 0) seam /= weights;
    const double top = (static_cast<double>(e.level) + free * (skyline + ridge + rough) + w.edge * seam) * map.levelHeight;
    // Terraces (W4b): along stretches of the wall its face steps, a shelf
    // half way up 2.5 to 6 cells wide (never a sliver), then the rest of
    // the climb.
    const double step = noise.fbm(q.x * 0.045 + 19, q.y * 0.045 - 29);
    const double shelf = 2.5 + 8.0 * max(step, 0.0);
    const double terrace = free * smoothstep(0.04, 0.2, step);
    const double second = smoothstep(shelf, shelf + 2 * cliffHalf, w.outside);
    h += max(top - h, 0.0) * w.coverage * (1 - 0.5 * terrace * (1 - second));
    // Craggy noise on its faces, more than a plain cliff's.
    h += 4 * w.coverage * (1 - w.coverage) * free * noise.fbm(q.x * 0.9 + 3.1, q.y * 0.9) * 1.1;
    h += 4 * terrace * second * (1 - second) * noise.fbm(q.x * 0.9 - 8.3, q.y * 0.9 + 2) * 0.8;
    return h;
}

/// How far a ground point lies outside the map's bounds, cells (0 on it).
double TerrainField::outside(Vec2 p) const {
    const GroundRect& b = map.bounds;
    return length(max(max(Vec2(b.minX, b.minZ) - p, p - Vec2(b.maxX, b.maxZ)), Vec2::zero));
}

/// Ground for the scenery past the map's edge, out of play: the map's
/// own ground at the edge, rising over a dozen cells into rough highland
/// and ridges further out, so the view never ends in the void.
double TerrainField::borderHeight(Vec2 p) const {
    const double d = outside(p);
    const double h = height(p);
    if (!(d > 0)) return h;
    const Vec2 q = fold(p);
    const double hills = smoothstep(0.5, 8, d) * (2.8 + 2.2 * noise.fbm(q.x * 0.06 + 17, q.y * 0.06 - 5));
    const double ridges = smoothstep(6, 26, d) * (4 + 5 * max(noise.fbm(q.x * 0.03 - 9, q.y * 0.03 + 23), -0.4));
    const double crags = smoothstep(1.5, 8, d) * (noise.fbm(q.x * 0.3 + 5, q.y * 0.3 + 2) * 1.3
                                                  + noise.fbm(q.x * 0.9 - 3, q.y * 0.9 + 8) * 0.35);
    return h + hills + ridges + crags;
}

/// Discrete level under a point (for placement), ignoring cliff blending.
int64_t TerrainField::level(Vec2 p) const {
    int64_t l = 0;
    for (size_t k = 0; k < map.plateaus.size(); k++) {
        const Plateau& plateau = map.plateaus[k];
        // Off its polygon's box (with room for rounding) a point is outside
        // it, so its signed distance is not negative: skip measuring it.
        if (k < plateauBounds.size()) {
            const Bounds& b = plateauBounds[k];
            if (p.x < b.lo.x - 1e-6 || p.x > b.hi.x + 1e-6 || p.y < b.lo.y - 1e-6 || p.y > b.hi.y + 1e-6) continue;
        }
        if (!(signedDistance(p, plateau.polygon) < 0)) continue;
        l = std::max(l, plateau.level);
    }
    // The height edits whose outline (a ridge's width) holds `p`.
    for (size_t k = 0; k < map.heightEdits.size(); k++) {
        const HeightEdit& e = map.heightEdits[k];
        if (e.kind == HeightEdit::Kind::ramp || e.points.empty()) continue;
        const Bounds& b = editBounds[k];
        if (p.x < b.lo.x || p.x > b.hi.x || p.y < b.lo.y || p.y > b.hi.y) continue;
        const bool inside = e.kind == HeightEdit::Kind::ridge || e.kind == HeightEdit::Kind::cut
                                ? lineDistance(p, e.points) <= e.width / 2
                                : signedDistance(p, e.points) < 0;
        if (!inside) continue;
        switch (e.kind) {
        case HeightEdit::Kind::raise:
        case HeightEdit::Kind::ridge: l = std::max(l, e.level); break;
        case HeightEdit::Kind::lower:
        case HeightEdit::Kind::cut: l = std::min(l, e.level); break;
        case HeightEdit::Kind::flatten: l = e.level; break;
        case HeightEdit::Kind::ramp: break;
        }
    }
    return l;
}

/// Ground material weights: grass, highland, plating, scorch. Dirt fills
/// whatever is left. Cliff rock is chosen by slope in the renderer.
TerrainField::Materials TerrainField::materials(Vec2 p) const {
    double grass = 0.0, highland = 0.0, plating = 0.0, scorch = 0.0, cutFloor = 0.0;
    const double wobble = edgeWobble(p);
    for (size_t k = 0; k < map.plateaus.size(); k++) {
        if (!(map.plateaus[k].level > 0)) continue;
        highland = max(highland, plateauCoverage(p, k, wobble));
    }
    // Crafted high ground is highland; ground brought down to the low
    // ground loses it.
    for (size_t k = 0; k < map.heightEdits.size(); k++) {
        const HeightEdit& e = map.heightEdits[k];
        if (e.kind == HeightEdit::Kind::ramp) continue;
        const double t = editCoverage(p, k);
        if (!(t > 0)) continue;
        if (e.level > 0 && (e.kind == HeightEdit::Kind::raise || e.kind == HeightEdit::Kind::ridge)) highland = max(highland, t);
        else if (e.level == 0 && e.kind != HeightEdit::Kind::raise && e.kind != HeightEdit::Kind::ridge) highland *= 1 - t;
        // A canyon's floor (Z1) a darker tone than the ground above it.
        if (e.kind == HeightEdit::Kind::cut) {
            const double floor = smoothstep(2 * cliffHalf, -0.5, lineDistance(p, e.points) - e.width / 2);
            cutFloor = max(cutFloor, floor);
        }
    }
    if (cutFloor > 0) {
        const Vec2 qf = fold(p);
        scorch = max(scorch, cutFloor * (0.42 + 0.18 * noise.fbm(qf.x * 0.3 - 13, qf.y * 0.3 + 5)));
    }
    const Vec2 q = fold(p);
    highland *= 0.55 + 0.45 * smoothstep(-0.3, 0.3, noise.fbm(q.x * 0.11 + 40, q.y * 0.11));
    for (const GroundPatch& patch : map.patches) {
        const double d = distance(p, patch.center);
        const double edge = noise.fbm(q.x * 0.35 + 11, q.y * 0.35 - 7) * patch.radius * 0.45;
        const double w = smoothstep(patch.radius, patch.radius * 0.55, d + edge);
        switch (patch.kind) {
        case GroundPatch::Kind::grass: grass = max(grass, w); break;
        case GroundPatch::Kind::gravel: highland = max(highland, w * 0.8); break;
        case GroundPatch::Kind::scorch: scorch = max(scorch, w); break;
        }
    }
    // A region's walls (W4): bare rock-strewn highland, darker in patches,
    // no grass (the same along a shared edge on both sides).
    if (const Wall w = wall(p); w.coverage > 0) {
        const double patchy = 0.5 + 0.5 * noise.fbm(q.x * 0.21 - 13, q.y * 0.21 + 5);
        highland = highland + (1 - highland) * w.coverage;
        grass *= 1 - w.coverage;
        scorch = max(scorch, w.coverage * (0.3 + 0.25 * (patchy * (1 - w.edge) + 0.5 * w.edge)));
    }
    // Grass thins out on cliff faces and ramps.
    const RampBlend r = ramp(p);
    grass *= 1 - r.weight * 0.7;
    // A crafted region's pads: a round plate as wide as a flight's reach.
    for (const Vec2& c : map.plates) {
        const Vec2 d = p - c;
        const double sdf = length(d) - region::padReach;
        if (!(sdf < 4)) continue;
        plating = max(plating, smoothstep(0.12, -0.12, sdf));
        scorch = max(scorch, smoothstep(region::padReach + 2.5, region::padReach, length(d)) * 0.35 * (1 - plating));
    }
    // Foundation plate under each player's first Citadel (not in a region).
    for (int64_t b : map.plates.empty() ? map.starts : std::vector<int64_t>{}) {
        if (!(b >= 0 && b < static_cast<int64_t>(map.bases.size()))) continue;
        const Vec2 d = p - map.bases[static_cast<size_t>(b)].center;
        if (!(std::fabs(d.x) < 8) || !(std::fabs(d.y) < 8)) continue;
        // Rounded square, 7 cells across.
        const Vec2 sq = Vec2(std::fabs(d.x), std::fabs(d.y)) - Vec2(3.0, 3.0);
        const double sdf = length(max(sq, Vec2::zero)) + min(max(sq.x, sq.y), 0.0) - 0.5;
        plating = max(plating, smoothstep(0.12, -0.12, sdf));
        // Dust around the plate from constant traffic.
        scorch = max(scorch, smoothstep(6.5, 3.6, length(d)) * 0.35 * (1 - plating));
    }
    return Materials{grass, highland, plating, scorch};
}

/// Signed distance from `p` to a polygon: negative inside.
double landOutside(Vec2 p, const std::vector<Vec2>& outline, const std::vector<std::vector<Vec2>>& islands) {
    double d = signedDistance(p, outline);
    for (const std::vector<Vec2>& island : islands) d = max(d, -signedDistance(p, island));
    return d;
}

double signedDistance(Vec2 p, const std::vector<Vec2>& poly) {
    if (!(poly.size() > 2)) return std::numeric_limits<double>::infinity();
    double d = length_squared(p - poly[0]);
    bool inside = false;
    size_t j = poly.size() - 1;
    for (size_t i = 0; i < poly.size(); i++) {
        const Vec2 a = poly[i], b = poly[j];
        const Vec2 e = b - a, w = p - a;
        const double t = min(max(dot(w, e) / dot(e, e), 0.0), 1.0);
        d = min(d, length_squared(w - e * t));
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) {
            inside = !inside;
        }
        j = i;
    }
    return inside ? -std::sqrt(d) : std::sqrt(d);
}

/// The ground point under canvas point `c`: the first place the camera
/// ray meets the terrain surface, so a click on a cliff top picks the
/// plateau, not the low ground behind it.
Vec2 TerrainField::pick(Vec2 c, const CanvasProjection& projection) const {
    if (auto g = pick(projection.cameraPosition(), projection.ray(c))) return *g;
    return projection.ground(c);
}

/// The same for a ray from `o` along `direction` (a camera that has
/// moved off the projection's focus); nil when it points at the sky.
std::optional<Vec2> TerrainField::pick(Vec3 o, Vec3 direction) const {
    const Vec3 d = normalize(direction);
    const double top = map.levelHeight * 3;
    if (!(d.y < 0)) return std::nullopt;
    // Start where the ray drops below anything the terrain can reach.
    double t = max((top - o.y) / d.y, 0.0);
    const double step = 0.1;
    auto above = [&](double at) {
        const Vec3 q = o + d * at;
        return q.y > height(Vec2(q.x, q.z));
    };
    const double limit = (-2 * top - o.y) / d.y;
    while (t < limit && above(t + step)) t += step;
    double lo = t, hi = t + step;
    for (int i = 0; i < 20; i++) {
        const double mid = (lo + hi) / 2;
        if (above(mid)) lo = mid; else hi = mid;
    }
    const Vec3 q = o + d * hi;
    return Vec2(q.x, q.z);
}

} // namespace ac
