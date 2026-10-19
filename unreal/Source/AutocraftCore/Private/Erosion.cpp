// Erosion (docs/world-sculpting.md, "No straight edge"): the last step of
// `expand`. A sculpted region's rim and its hand-written ground are worn the
// way the landforms are, so no edge a player sees runs straight: the rim's
// outline wanders in and out along its run; a canyon plateau's outer faces
// wander and fall away as a rough slope (the rims' rule), with fallen blocks
// at their foot; its lanes' walls bend, step out in offset segments and have
// notches bitten into their tops; any other hand-written raise wanders. The
// same file wears the same way.
#include "RegionFile.h"

#include "Noise.h"
#include "ScreenConfig.h"
#include "TerrainField.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>

namespace ac::region {

namespace {

constexpr double pi = std::numbers::pi;
/// How far from a canyon's path its lane's wall may step out, cells (it
/// stands 2.5 out): the rims' guns are placed and aimed by the wall's line.
constexpr double laneReach = 2.8;

double lineDist(Vec2 p, const std::vector<Vec2>& line) {
    double d = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i + 1 < line.size(); i++) {
        const Vec2 a = line[i], e = line[i + 1] - a;
        const double len2 = dot(e, e);
        const double t = len2 > 0 ? std::clamp(dot(p - a, e) / len2, 0.0, 1.0) : 0.0;
        d = std::min(d, distance(p, a + e * t));
    }
    if (line.size() == 1) d = distance(p, line[0]);
    return d;
}

/// `line` with a point every `step` cells or less, its corners kept.
std::vector<Vec2> resample(const std::vector<Vec2>& line, double step, bool closed) {
    std::vector<Vec2> out;
    const size_t n = line.size();
    for (size_t i = 0; i < (closed ? n : n - 1); i++) {
        const Vec2 a = line[i], b = line[(i + 1) % n];
        const double k = std::max(1.0, std::ceil(distance(a, b) / step));
        for (double j = 0; j < k; j++) out.push_back(a + (b - a) * (j / k));
    }
    if (!closed) out.push_back(line.back());
    return out;
}

double area(const std::vector<Vec2>& poly) {
    double a = 0;
    for (size_t i = 0; i < poly.size(); i++) {
        const Vec2 p = poly[i], q = poly[(i + 1) % poly.size()];
        a += p.x * q.y - q.x * p.y;
    }
    return a / 2;
}

/// Each point's way into the polygon, from its neighbours `k` either side.
std::vector<Vec2> inward(const std::vector<Vec2>& pts, size_t k) {
    const double s = area(pts) > 0 ? 1 : -1;
    const size_t n = pts.size();
    std::vector<Vec2> out;
    for (size_t i = 0; i < n; i++) {
        const Vec2 t = pts[(i + k) % n] - pts[(i + n - k) % n];
        const double len = std::max(length(t), 1e-9);
        out.push_back(Vec2(-t.y, t.x) * (s / len));
    }
    return out;
}

bool segmentsCross(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const auto side = [](Vec2 p, Vec2 q, Vec2 r) { return (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x); };
    const double d1 = side(c, d, a), d2 = side(c, d, b), d3 = side(a, b, c), d4 = side(a, b, d);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

/// No two of the polygon's edges cross.
bool simple(const std::vector<Vec2>& poly) {
    const size_t n = poly.size();
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 2; j < n; j++) {
            if (i == 0 && j == n - 1) continue;
            if (segmentsCross(poly[i], poly[(i + 1) % n], poly[j], poly[(j + 1) % n])) return false;
        }
    return true;
}

/// Points closer than `gap` to the one before dropped.
std::vector<Vec2> thin(const std::vector<Vec2>& pts, double gap) {
    std::vector<Vec2> out;
    for (Vec2 p : pts)
        if (out.empty() || distance(out.back(), p) >= gap) out.push_back(p);
    while (out.size() > 3 && distance(out.back(), out.front()) < gap) out.pop_back();
    return out;
}

/// A wander along a run, cells: two waves (one `long_` cells, the other a
/// third of that) and noise, so no 8 cells of it lie straight.
struct Wander {
    double long_, short_, p1, p2, o;
    Wander(SeededRandom& rng, double lo, double hi)
        : long_(rng.range(lo, hi)), short_(rng.range(4.5, 6.5)), p1(rng.range(0, 2 * pi)), p2(rng.range(0, 2 * pi)), o(rng.range(-50, 50)) {}
    double at(double s, const Noise& noise) const {
        return std::sin(2 * pi * s / long_ + p1) + 0.4 * std::sin(2 * pi * s / short_ + p2) + 0.3 * noise.fbm(s * 0.12 + o, o * 0.7, 2);
    }
};

/// Ground a worn edge may not move onto: a point and how near.
struct Keep {
    Vec2 at;
    double radius;
};

std::vector<Keep> keeps(const RegionFile& f, const RegionFile& out) {
    std::vector<Keep> k;
    for (const Place& p : f.pads) k.push_back({p.at, 16});
    for (const Place& p : f.spawns) k.push_back({p.at, 3.5});
    for (const Site& s : f.sites) k.push_back({s.at, 4.5});
    for (const Deposit& d : f.deposits) k.push_back({d.at, 3.5});
    for (const WellSite& w : f.wells) k.push_back({w.at, 4});
    if (f.nest) k.push_back({*f.nest, 9});
    for (const Prop& p : f.doodads)
        if (p.kind != Doodad::Kind::rock && p.kind != Doodad::Kind::boulder && p.kind != Doodad::Kind::rockSpire)
            k.push_back({p.at, 3.5});
    for (const Edit& e : out.height)
        if (e.op == HeightEdit::Kind::ramp && e.line.size() == 2)
            for (const Vec2 q : resample(e.line, 1, false)) k.push_back({q, e.width / 2 + 3});
    for (const Zone& z : f.zones) {
        for (const Zone::Entrance& e : z.entrances) k.push_back({e.at, 5});
        for (const Zone::Path& p : z.paths)
            for (const Vec2 q : resample(p.points, 1, false)) k.push_back({q, 4});
    }
    return k;
}

bool clearOf(Vec2 p, const std::vector<Keep>& ks) {
    for (const Keep& k : ks)
        if (distance(p, k.at) < k.radius) return false;
    return true;
}

double depthIn(Vec2 p, Vec2 ext) { return std::min({p.x, p.y, ext.x - p.x, ext.y - p.y}); }

/// The edge from `a` to `b` lies along the region's edge (a passage's
/// strip, a polygon drawn past the bounds): nothing of it shows.
bool alongEdge(Vec2 a, Vec2 b, Vec2 ext) {
    return depthIn(a, ext) < 0.75 && depthIn(b, ext) < 0.75 && depthIn((a + b) * 0.5, ext) < 0.75;
}

/// 0 in a passage's lane (its span and 3 cells past each end, 20 cells
/// in), rising to 1 over 6 cells past it: a passage keeps its shape.
double laneFactor(Vec2 p, const RegionFile& f) {
    const Vec2 ext = f.extent();
    double out = 1;
    for (size_t e = 0; e < 4; e++)
        for (const Span& s : f.openings[e]) {
            const double along = e % 2 == 0 ? p.x : p.y;
            const double depth = e == 0 ? p.y : e == 1 ? ext.x - p.x : e == 2 ? ext.y - p.y : p.x;
            if (depth > 20) continue;
            const double gap = std::max({s.from - 3 - along, along - (s.to + 3), 0.0});
            out = std::min(out, smoothstep(0.0, 6.0, gap));
        }
    return out;
}

/// The worn outline: each point moved along `in` (into the polygon) by
/// `want` as far as `allowed` lets it (tried at the full move, then less),
/// softened, the whole retried smaller when an edge would cross another.
std::vector<Vec2> wear(const std::vector<Vec2>& pts, const std::vector<Vec2>& in, std::vector<double> want,
                       const std::function<bool(size_t, Vec2, double)>& allowed) {
    const size_t n = pts.size();
    for (double scale : {1.0, 0.7, 0.45}) {
        std::vector<double> w(n);
        for (size_t i = 0; i < n; i++) {
            const double full = want[i] * scale;
            w[i] = 0;
            for (double f = 1; f > 0.05; f -= 0.1)
                if (allowed(i, pts[i] + in[i] * (full * f), full * f)) {
                    w[i] = full * f;
                    break;
                }
            // Out as far as it may go and no further: the wave folds back
            // in, so it never runs flat along the limit.
            const double back = w[i] - 0.6 * (full - w[i]);
            if (full < 0 && w[i] > full && allowed(i, pts[i] + in[i] * back, back)) w[i] = back;
        }
        // Soften (a mean over 2 either side), never past what each point allows.
        std::vector<double> soft(n);
        for (size_t i = 0; i < n; i++) {
            double sum = 0;
            for (int k = -2; k <= 2; k++) sum += w[(i + n + static_cast<size_t>(k + 2) - 2) % n];
            const double m = sum / 5;
            soft[i] = (m > 0) == (w[i] > 0) ? (std::fabs(m) < std::fabs(w[i]) ? m : w[i]) : 0.5 * w[i];
        }
        std::vector<Vec2> out;
        for (size_t i = 0; i < n; i++) out.push_back(pts[i] + in[i] * soft[i]);
        out = thin(out, 0.35);
        if (out.size() >= 3 && simple(out)) return out;
    }
    return pts;
}

/// The region's rim: its outline wanders up to 1.8 cells out and 3.6 in along its
/// run (wavelength 12-18 cells), kept 2 cells inside the region, off the
/// passages, the places and the canyon zones' ways.
void wearRim(RegionFile& out, const RegionFile& f, const std::vector<Keep>& ks, uint64_t seed) {
    if (!out.border || out.border->outline.size() < 3) return;
    const std::vector<Vec2> outline = out.border->outline;
    const Vec2 ext = f.extent();
    const std::vector<Vec2> pts = resample(outline, 2.0, true);
    const std::vector<Vec2> in = inward(pts, 2);
    SeededRandom rng(seed);
    const Noise noise(seed + 11);
    const Wander wander(rng, 12, 18);
    std::vector<double> want(pts.size());
    double s = 0;
    for (size_t i = 0; i < pts.size(); i++) {
        if (i > 0) s += distance(pts[i], pts[i - 1]);
        const double fade = depthIn(pts[i], ext) < 0.75 ? 0.0 : laneFactor(pts[i], f);
        want[i] = fade * (0.0 + 1.6 * wander.at(s, noise));
    }
    out.border->outline = wear(pts, in, want, [&](size_t i, Vec2 q, double w) {
        if (depthIn(q, ext) < 1.6) return false;
        const double sd = signedDistance(q, outline);
        // Moved in: still as deep in the land as it moved (no neck crossed),
        // clear of what the land holds; moved out: as far outside.
        if (w > 0) return sd <= -0.6 * w && (clearOf(q, ks) || !clearOf(pts[i], ks));
        return sd >= 0.6 * -w;
    });
}

/// The level the ground under `p` stands at by the file's other raises.
int64_t underLevel(Vec2 p, const RegionFile& f, size_t self) {
    int64_t l = 0;
    for (size_t k = 0; k < f.height.size(); k++) {
        const Edit& e = f.height[k];
        if (k == self || e.op != HeightEdit::Kind::raise || e.area.points.empty()) continue;
        if (signedDistance(p, e.area.outline()) < 0) l = std::max(l, e.level);
    }
    return l;
}

/// A canyon's hand-written ground (Z1's raises and cuts) and any other
/// hand-written raise, worn. A plateau that holds a zone's paths is the
/// canyon's mass: its outer faces only close the canyon off from the open
/// land, so they take the rims' rule (a rough slope, 3 to 5 cells) and
/// wander in up to 3.5 cells; fallen blocks lie at their foot. Its lanes
/// (the cuts) keep their cliffs: they wander 1.2 cells either side, a cell
/// wider so the floor keeps its width, and step out in offset segments,
/// with notches bitten into their tops where they are two levels deep.
void wearHandDrawn(RegionFile& out, const RegionFile& f, const std::vector<Keep>& ks, uint64_t seed) {
    const Vec2 ext = f.extent();
    SeededRandom rng(seed);
    const Noise noise(seed + 23);
    std::vector<Edit> after;
    std::vector<Prop> blocks;
    std::vector<Vec2> placed;
    // The cuts, as written, for the plateau's faces to keep clear of.
    std::vector<const Edit*> cuts;
    for (const Edit& e : f.height)
        if (e.op == HeightEdit::Kind::cut && e.line.size() >= 2) cuts.push_back(&e);
    const auto clearOfCuts = [&](Vec2 q, double margin) {
        for (const Edit* c : cuts)
            if (lineDist(q, c->line) < c->width / 2 + margin) return false;
        return true;
    };
    for (size_t k = 0; k < f.height.size(); k++) {
        Edit& e = out.height[k];
        const bool area = (e.op == HeightEdit::Kind::raise || e.op == HeightEdit::Kind::lower) &&
                          (e.area.shape == Area::Shape::polygon || e.area.shape == Area::Shape::rect);
        if (area) {
            const std::vector<Vec2> poly = e.area.outline();
            bool plateau = false;
            for (const Zone& z : f.zones)
                for (const Zone::Path& p : z.paths)
                    for (const Vec2 q : p.points) plateau = plateau || (e.slope < 2 && signedDistance(q, poly) < 0);
            const std::vector<Vec2> pts = resample(poly, 2.0, true);
            const std::vector<Vec2> in = inward(pts, 2);
            // The rims' rule for a plateau: a slope 2.5 cells a level the
            // face falls (the least it falls anywhere), 3 to 5 cells, its
            // face set back by half of it so the slope's foot stays near
            // where the cliff stood.
            double slope = 0;
            if (plateau) {
                int64_t rise = e.level;
                for (size_t i = 0; i < pts.size(); i += 3) {
                    const Vec2 foot = pts[i] - in[i] * 1.5;
                    if (depthIn(foot, ext) > 3) rise = std::min(rise, e.level - underLevel(foot, f, k));
                }
                slope = std::clamp(2.5 * static_cast<double>(std::max<int64_t>(rise, 1)), 3.0, 5.0);
            }
            const Wander wander(rng, 10, 20);
            std::vector<double> want(pts.size());
            double s = 0;
            for (size_t i = 0; i < pts.size(); i++) {
                if (i > 0) s += distance(pts[i], pts[i - 1]);
                // Under the rim (at the region's edge) nothing shows.
                const double fade = depthIn(pts[i], ext) < 3 ? 0.0 : laneFactor(pts[i], f);
                // In a pad's field of fire the slope's foot stays where the
                // cliff stood (the guns see as far as before), elsewhere half
                // way.
                double back = 0.5;
                for (const Place& p : f.pads)
                    if (distance(pts[i], p.at) < fieldOfFire + slope) back = 1.1;
                want[i] = fade * (1.8 + back * slope + 1.7 * wander.at(s, noise));
            }
            const std::vector<Vec2> worn = wear(pts, in, want, [&](size_t i, Vec2 q, double w) {
                const double sd = signedDistance(q, poly);
                if (w > 0 && !(sd <= -0.6 * w)) return false;
                if (w < 0 && !(sd >= 0.6 * -w)) return false;
                if (!clearOf(q, ks) && clearOf(pts[i], ks)) return false;
                // A ramp up the face still meets the top: its high end
                // stays 2 cells in.
                for (const Edit& r : f.height)
                    if (r.op == HeightEdit::Kind::ramp && r.line.size() == 2) {
                        const Vec2 high = r.level >= r.fromLevel ? r.line[1] : r.line[0];
                        if (signedDistance(high, poly) < 0 && distance(q, high) < r.width / 2 + 2 + std::max(w, 0.0)) return false;
                    }
                // A plateau's top keeps its rims: 4 cells past every cut's
                // wall, 9.5 from every path (the guns' ground).
                if (plateau) {
                    for (const Edit* c : cuts)
                        if (lineDist(q, c->line) < c->width / 2 + 4 && lineDist(q, c->line) < lineDist(pts[i], c->line)) return false;
                    for (const Zone& z : f.zones)
                        for (const Zone::Path& p : z.paths)
                            if (lineDist(q, p.points) < 9.5 && lineDist(q, p.points) < lineDist(pts[i], p.points)) return false;
                }
                return true;
            });
            e.area.shape = Area::Shape::polygon;
            e.area.points = worn;
            if (!plateau) continue;
            const std::vector<Vec2> win = inward(worn, 2);
            e.slope = slope;
            // A cut that ran out through the old face runs on through the
            // slope, so its mouth stays open.
            for (const Edit* c : cuts)
                for (const bool head : {true, false}) {
                    const Vec2 end = head ? c->line.front() : c->line.back();
                    const Vec2 prev = head ? c->line[1] : c->line[c->line.size() - 2];
                    const double sd = signedDistance(end, poly);
                    if (!(sd + c->width / 2 > 0) || sd > e.slope + 1) continue;
                    // Not past the region's edge: the rim stands there.
                    double run = e.slope + 1.5 - std::min(sd, 0.0);
                    while (run > 0 && depthIn(end + normalize(end - prev) * run, ext) < 1) run -= 0.5;
                    if (run < 1) continue;
                    Edit through = *c;
                    through.line = {end, end + normalize(end - prev) * run};
                    after.push_back(through);
                }
            // Fallen blocks: a cluster every 9-16 cells along the foot, never
            // three in a row.
            double next = rng.range(3, 9), run = 0;
            for (size_t i = 1; i < worn.size(); i++) {
                run += distance(worn[i], worn[i - 1]);
                if (run < next) continue;
                next = run + rng.range(9, 16);
                const Vec2 c = worn[i] - win[i] * (e.slope + rng.range(0.4, 2.2));
                if (depthIn(c, ext) < 4 || !clearOf(c, ks) || !clearOfCuts(c, 2.5) || laneFactor(c, f) < 1) continue;
                const int count = 1 + int(rng.range(0, 2.99));
                for (int b = 0; b < count; b++) {
                    const double a = rng.range(0, 2 * pi);
                    const Vec2 at = c + Vec2(std::cos(a), std::sin(a)) * (b == 0 ? 0.0 : rng.range(1.0, 2.2));
                    if (!clearOf(at, ks) || !clearOfCuts(at, 2.5) || inRow(placed, at)) continue;
                    placed.push_back(at);
                    blocks.push_back(Prop{b == 0 ? Doodad::Kind::boulder : Doodad::Kind::rock, at, rng.range(0, 360),
                                          b == 0 ? rng.range(1.4, 2.4) : rng.range(0.6, 1.4)});
                }
            }
            continue;
        }
        if (e.op != HeightEdit::Kind::cut || e.slope >= 2 || e.line.size() < 2 || e.width < 4) continue;
        double len = 0;
        for (size_t i = 0; i + 1 < e.line.size(); i++) len += distance(e.line[i], e.line[i + 1]);
        if (len < 6) continue;
        // A lane the walkers walk keeps its line and width: the rims' guns
        // are placed and aimed by it (a straight run of it is bent at the
        // end, as anything straight is). Another cut's wall bends, held at
        // both ends (a tunnel's mouth, the basin).
        bool walked = false;
        for (const Zone& z : f.zones)
            for (const Zone::Path& p : z.paths)
                for (const Vec2 q : p.points) walked = walked || lineDist(q, e.line) < e.width / 2 + 1;
        const std::vector<Vec2> line = resample(e.line, 1.0, false);
        const Wander wander(rng, 11, 17);
        std::vector<Vec2> bent;
        double s = 0;
        for (size_t i = 0; i < line.size(); i++) {
            if (i > 0) s += distance(line[i], line[i - 1]);
            const Vec2 d = line[std::min(i + 1, line.size() - 1)] - line[i > 0 ? i - 1 : 0];
            const Vec2 side = Vec2(-d.y, d.x) / std::max(length(d), 1e-9);
            const double hold = smoothstep(0.0, 3.0, s) * smoothstep(0.0, 3.0, len - s);
            bent.push_back(line[i] + side * (walked ? 0.0 : 1.2 * hold * wander.at(s, noise)));
        }
        const double half = e.width / 2;
        // How far a point lies from the zones' walkers' paths.
        const auto pathDist = [&](Vec2 q) {
            double d = std::numeric_limits<double>::infinity();
            for (const Zone& z : f.zones)
                for (const Zone::Path& p : z.paths) d = std::min(d, lineDist(q, p.points));
            return d;
        };
        e.line = walked ? e.line : thin(bent, 0.5);
        // Offset segments: every 7-12 cells a stretch of wall 3-6 cells
        // long steps 0.8-1.8 cells out, on one side or the other.
        const int64_t top = [&] {
            int64_t l = 0;
            for (const Vec2 q : line) l = std::max(l, underLevel(q, f, k));
            return l;
        }();
        double at = rng.range(2, 6);
        bool left = rng.unit() < 0.5;
        while (at + 3 < len - 2) {
            const double piece = rng.range(3, 6);
            double out_ = rng.range(0.8, 1.8);
            const auto point = [&](double t) {
                double acc = 0;
                for (size_t i = 0; i + 1 < bent.size(); i++) {
                    const double l = distance(bent[i], bent[i + 1]);
                    if (acc + l >= t) {
                        const Vec2 d = normalize(bent[i + 1] - bent[i]);
                        return std::pair{bent[i] + d * (t - acc), Vec2(-d.y, d.x)};
                    }
                    acc += l;
                }
                const Vec2 d = normalize(bent.back() - bent[bent.size() - 2]);
                return std::pair{bent.back(), Vec2(-d.y, d.x)};
            };
            const auto [a, na] = point(at);
            const auto [b, nb] = point(std::min(at + piece, len - 2));
            const double sign = left ? 1.0 : -1.0;
            // Beside a walkers' path the wall steps out no further than
            // `laneReach` from it: past that is the rim the guns stand on.
            for (const auto& [p, n] : {std::pair{a, na}, std::pair{b, nb}}) {
                const double d = pathDist(p + n * sign * (half + 0.5 + out_));
                if (pathDist(p) < laneReach && d > laneReach) out_ -= d - laneReach;
            }
            Edit bay = e;
            bay.width = 3;
            bay.line = {a + na * sign * (half + 0.5 + out_ - 1.5), b + nb * sign * (half + 0.5 + out_ - 1.5)};
            // Never over a ramp: carved after it, a bay would bite its side.
            const auto offRamps = [&](const Edit& c) {
                for (const Edit& r : f.height)
                    if (r.op == HeightEdit::Kind::ramp && r.line.size() == 2)
                        for (const Vec2 q : resample(c.line, 0.5, false))
                            if (lineDist(q, r.line) < r.width / 2 + c.width / 2 + 2) return false;
                return true;
            };
            if (out_ >= 0.4 && offRamps(bay)) after.push_back(bay);
            // A notch in the top beside it, where the wall is two levels or
            // more, away from the rims over the paths.
            if (top - e.level >= 2 && rng.unit() < 0.6) {
                const auto [c, nc] = point(std::min(at + piece + rng.range(1.5, 3.5), len - 2));
                Edit notch = e;
                notch.width = rng.range(1.6, 2.4);
                notch.level = e.level + (top - e.level) / 2;
                notch.line = {c - nc * sign * (half + 0.2), c - nc * sign * (half + 0.2 + rng.range(2.0, 3.4))};
                if (pathDist(notch.line[1]) > 8 && offRamps(notch)) after.push_back(notch);
            }
            at += piece + rng.range(7, 12) - 3;
            left = rng.unit() < 0.7 ? !left : left;
        }
    }
    // The cut-throughs, bays and notches carve after the file's own edits,
    // before the landforms' (a wash over a canyon floor still shows).
    out.height.insert(out.height.begin() + static_cast<std::ptrdiff_t>(f.height.size()), after.begin(), after.end());
    for (const Prop& p : blocks) out.doodads.push_back(p);
}

} // namespace

bool row(Vec2 a, Vec2 m, Vec2 b) {
    const Vec2 e = b - a;
    const double len = length(e);
    if (!(len > 0) || len > rowReach) return false;
    const double t = dot(m - a, e) / (len * len);
    return t >= 1.0 / 3 && t <= 2.0 / 3 && distance(m, a + e * t) <= rowTolerance;
}

bool inRow(const std::vector<Vec2>& rocks, Vec2 p) {
    std::vector<Vec2> near;
    for (const Vec2 q : rocks)
        if (distance(p, q) <= rowReach) near.push_back(q);
    for (size_t i = 0; i < near.size(); i++)
        for (size_t j = i + 1; j < near.size(); j++)
            if (row(near[i], p, near[j]) || row(p, near[i], near[j]) || row(near[i], near[j], p)) return true;
    return false;
}

namespace {

/// A straight run of a list of points: from point `from`, `steps` points on
/// (round the end of a closed outline).
struct Run {
    size_t from, steps;
};

/// The straight runs of `pts`, split where `cut(i)` (the edge from point
/// `i` to the next is not counted): from each start, the furthest point
/// whose chain keeps within `straightTolerance` of the chord, kept when that
/// is `straightRun` cells or more.
std::vector<Run> runsOf(const std::vector<Vec2>& pts, bool closed, const std::function<bool(size_t)>& cut) {
    std::vector<Run> out;
    const size_t n = pts.size();
    if (n < 2) return out;
    // A closed outline is read from where an edge is cut, else from its
    // sharpest corner, so no run is split by where its list starts.
    size_t start = 0;
    if (closed) {
        bool found = false;
        for (size_t i = 0; i < n && !found; i++)
            if (cut(i)) start = (i + 1) % n, found = true;
        double sharpest = 2;
        for (size_t i = 0; i < n && !found; i++) {
            const Vec2 a = normalize(pts[i] - pts[(i + n - 1) % n]), b = normalize(pts[(i + 1) % n] - pts[i]);
            if (dot(a, b) < sharpest) sharpest = dot(a, b), start = i;
        }
    }
    const size_t edges = closed ? n : n - 1;
    const auto at = [&](size_t k) { return pts[(start + k) % n]; };
    size_t k = 0;
    while (k < edges) {
        if (cut((start + k) % n)) {
            k++;
            continue;
        }
        size_t best = k;
        for (size_t j = k + 1; j <= edges; j++) {
            if (cut((start + j - 1) % n)) break;
            bool straight = true;
            for (size_t m = k + 1; m < j && straight; m++) straight = lineDist(at(m), {at(k), at(j)}) <= straightTolerance;
            if (!straight) break;
            best = j;
        }
        if (best > k && distance(at(k), at(best)) >= straightRun) {
            out.push_back({(start + k) % n, best - k});
            k = best;
        } else {
            k++;
        }
    }
    return out;
}

/// Each straight run of `pts` bent: its points replaced by a wave across
/// its chord every cell, 9 cells long and 0.8 cells deep (only into the
/// polygon, `inwardOnly`), held at its ends; or (`bow`) by one bow 0.5 cells
/// deep at its middle, to either side in turn, for a lane whose floor must
/// keep its way.
void breakRuns(std::vector<Vec2>& pts, bool closed, bool inwardOnly, const std::function<bool(size_t)>& cut, bool bow = false) {
    for (int pass = 0; pass < 5; pass++) {
        const std::vector<Run> runs = runsOf(pts, closed, cut);
        if (runs.empty()) return;
        const size_t n = pts.size();
        const double sign = closed && area(pts) < 0 ? -1 : 1;
        std::vector<int> runAt(n, -1);
        for (size_t r = 0; r < runs.size(); r++) runAt[runs[r].from] = static_cast<int>(r);
        // Read from a point no run passes over.
        std::vector<bool> inside(n, false);
        for (const Run& r : runs)
            for (size_t m = 1; m < r.steps; m++) inside[(r.from + m) % n] = true;
        size_t first = 0;
        while (first < n && inside[first]) first++;
        std::vector<Vec2> out;
        for (size_t k = 0; k < (closed ? n : n);) {
            const size_t i = (first + k) % n;
            out.push_back(pts[i]);
            if (runAt[i] < 0) {
                k++;
                continue;
            }
            const Run& r = runs[static_cast<size_t>(runAt[i])];
            const Vec2 a = pts[i], b = pts[(i + r.steps) % n];
            const double len = distance(a, b);
            const Vec2 d = (b - a) / len, left(-d.y, d.x);
            for (double t = 1; t < len - 0.5; t += 1) {
                const double hold = std::min({1.0, t / 2, (len - t) / 2});
                const double wave = inwardOnly ? 0.5 - 0.5 * std::cos(2 * pi * t / 9) : std::sin(2 * pi * t / 9);
                const double u = t / len;
                out.push_back(a + d * t + left * (bow ? (pass % 2 == 0 ? -0.5 : 0.5) * 4 * u * (1 - u) : sign * 0.8 * hold * wave));
            }
            k += r.steps;
        }
        if (!closed && out.back() != pts.back()) out.push_back(pts.back());
        pts = out;
    }
}

bool cliffArea(const Edit& e) {
    return (e.op == HeightEdit::Kind::raise || e.op == HeightEdit::Kind::lower || e.op == HeightEdit::Kind::flatten) && e.slope < 2 &&
           (e.area.shape == Area::Shape::polygon || e.area.shape == Area::Shape::rect);
}

/// A cut or ridge with cliff walls (a wash's gentle bed is not one).
bool cliffLine(const Edit& e) { return (e.op == HeightEdit::Kind::cut || e.op == HeightEdit::Kind::ridge) && !(e.slope > 0); }

} // namespace

std::vector<StraightRun> straightRuns(const RegionFile& f) {
    std::vector<StraightRun> out;
    const Vec2 ext = f.extent();
    const auto scan = [&](const std::vector<Vec2>& pts, bool closed, const std::string& what) {
        // Past the region's edge (under the rim, a passage's strip along the
        // edge) nothing shows.
        const auto cut = [&](size_t i) { return alongEdge(pts[i], pts[(i + 1) % pts.size()], ext); };
        for (const Run& r : runsOf(pts, closed, cut)) out.push_back({what, pts[r.from], pts[(r.from + r.steps) % pts.size()]});
    };
    for (size_t k = 0; k < f.height.size(); k++) {
        const Edit& e = f.height[k];
        const std::string what = "height[" + std::to_string(k) + "]";
        if (cliffArea(e)) scan(e.area.outline(), true, what);
        if (cliffLine(e)) scan(e.line, false, what);
    }
    if (f.border) {
        for (size_t k = 0; k < f.border->islands.size(); k++) scan(f.border->islands[k], true, "border.islands[" + std::to_string(k) + "]");
        scan(f.border->outline, true, "border.outline");
    }
    return out;
}

void erode(RegionFile& out, const RegionFile& f) {
    const uint64_t seed = fnv1a64(f.id + "|erode");
    const std::vector<Keep> ks = keeps(f, out);
    wearHandDrawn(out, f, ks, seed ^ 0xc4a7);
    wearRim(out, f, ks, seed ^ 0x51e7);
    // Whatever still runs straight (a landform's long side, a stretch of rim
    // held still by what it keeps clear of) is bent.
    const Vec2 ext = f.extent();
    for (Edit& e : out.height) {
        if (cliffArea(e)) {
            std::vector<Vec2> pts = e.area.outline();
            breakRuns(pts, true, false, [&](size_t i) { return alongEdge(pts[i], pts[(i + 1) % pts.size()], ext); });
            e.area.shape = Area::Shape::polygon;
            e.area.points = pts;
        }
        if (cliffLine(e)) {
            bool walked = false;
            for (const Zone& z : f.zones)
                for (const Zone::Path& p : z.paths)
                    for (const Vec2 q : p.points) walked = walked || lineDist(q, e.line) < e.width / 2 + 1;
            breakRuns(e.line, false, false, [](size_t) { return false; }, walked);
        }
    }
    if (out.border) {
        for (std::vector<Vec2>& island : out.border->islands) breakRuns(island, true, true, [](size_t) { return false; });
        std::vector<Vec2>& o = out.border->outline;
        breakRuns(o, true, true, [&](size_t i) {
            return alongEdge(o[i], o[(i + 1) % o.size()], ext) || laneFactor(o[i], f) < 0.01;
        });
    }
}

} // namespace ac::region
