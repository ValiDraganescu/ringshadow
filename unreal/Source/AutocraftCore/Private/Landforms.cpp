// Landforms (docs/world-sculpting.md, "Landforms"): a region's land said in
// words — a butte here, an escarpment along there, the open-desert style —
// drawn into the file's plain layers (height edits, border islands, rocks)
// by `expand`. Every outline is bent by noise and bitten by erosion, so no
// edge runs straight (the rim and the hand-written ground too: Erosion.cpp);
// rocks gather where real ones would, at a cliff's foot and round a spire.
// The same seed draws the same land.
#include "RegionFile.h"

#include "Noise.h"
#include "ScreenConfig.h"
#include "TerrainField.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace ac::region {

namespace {

constexpr double pi = std::numbers::pi;

std::string kindName(LandformKind k) { return std::string(EnumInfo<LandformKind>::names[static_cast<size_t>(k)]); }

/// What a landform may not take: a point and how far round it, and why.
struct Rule {
    Vec2 at;
    double radius;
    std::string why;
    /// A passage's lane: gentle forms (a swell, a wash, boulders) may
    /// cross it.
    bool lane = false;
};

/// Each rule of the land (docs/world-sculpting.md, "Rules").
std::vector<Rule> rules(const RegionFile& f) {
    std::vector<Rule> out;
    for (const Place& p : f.pads) out.push_back({p.at, fieldOfFire, "the pad \"" + p.name + "\"'s field of fire"});
    for (const Place& p : f.spawns) out.push_back({p.at, 8, "the spawn point \"" + p.name + "\""});
    for (const Site& s : f.sites) out.push_back({s.at, 9, "the site \"" + s.name + "\""});
    for (const Deposit& d : f.deposits) out.push_back({d.at, 7, "a vein"});
    for (const WellSite& w : f.wells) out.push_back({w.at, 8, "a well"});
    if (f.nest) out.push_back({*f.nest, nestClear, "the nest"});
    for (const Prop& p : f.doodads)
        if (p.kind != Doodad::Kind::rock && p.kind != Doodad::Kind::boulder && p.kind != Doodad::Kind::rockSpire &&
            p.kind != Doodad::Kind::plant && p.kind != Doodad::Kind::bush && p.kind != Doodad::Kind::debris && p.kind != Doodad::Kind::deadTree)
            out.push_back({p.at, 6, "a landmark"});
    for (const Edit& e : f.height)
        if (e.op == HeightEdit::Kind::ramp)
            for (Vec2 q : e.line) out.push_back({q, 6, "a ramp"});
    // A passage's lane: open ground 16 cells in along its whole span.
    const Vec2 ext = f.extent();
    for (size_t e = 0; e < 4; e++)
        for (const Span& s : f.openings[e])
            for (double a = s.from; a <= s.to + 0.01; a += 3)
                for (const double d : {3.0, 10.0}) {
                    Vec2 q;
                    switch (static_cast<Edge>(e)) {
                    case Edge::north: q = Vec2(a, d); break;
                    case Edge::south: q = Vec2(a, ext.y - d); break;
                    case Edge::west: q = Vec2(d, a); break;
                    case Edge::east: q = Vec2(ext.x - d, a); break;
                    }
                    out.push_back({q, 7, "the " + std::string(edgeName(static_cast<Edge>(e))) + " passage's lane", true});
                }
    for (const Zone& z : f.zones) {
        for (Vec2 q : z.outline) out.push_back({q, 6, "the canyon zone \"" + z.name + "\""});
        for (const auto& path : z.paths)
            for (Vec2 q : path.points) out.push_back({q, 5, "the canyon zone \"" + z.name + "\"'s path"});
    }
    if (f.sculpt)
        for (const Sculpt::Keep& k : f.sculpt->keep) out.push_back({k.at, k.radius, "ground kept clear (sculpt.keep)"});
    return out;
}

/// A landform's reach, cells: how far from its middle (its line) it takes
/// ground.
double reach(const Landform& l) {
    const size_t s = static_cast<size_t>(l.size);
    switch (l.kind) {
    case LandformKind::butte: return std::array{3.0, 4.5, 6.0}[s] * 1.25 * 1.4 + 3.5;  // its apron's rough edge and slope
    case LandformKind::mesa: return std::array{6.0, 8.0, 11.0}[s] * 1.1 * 1.4 + 3.5;
    case LandformKind::spires: return std::array{5.0, 7.0, 10.0}[s] * 0.8 * 1.4 + 3.5;
    case LandformKind::shelf: return std::array{5.0, 8.0, 12.0}[s] + 2;
    case LandformKind::swell: return std::array{8.0, 12.0, 18.0}[s] + 4;
    case LandformKind::boulders: return std::array{3.0, 5.0, 8.0}[s];
    case LandformKind::escarpment: return 3;
    case LandformKind::wash: return std::array{2.0, 3.0, 4.5}[s];
    }
    return 4;
}

/// The points a landform is checked by: its middle, or its line every 2 cells.
std::vector<Vec2> probes(const Landform& l) {
    if (l.at) return {*l.at};
    std::vector<Vec2> out;
    for (size_t i = 0; i + 1 < l.along.size(); i++) {
        const Vec2 a = l.along[i], b = l.along[i + 1];
        const double n = std::max(1.0, std::ceil(distance(a, b) / 2));
        for (double k = 0; k < n; k++) out.push_back(a + (b - a) * (k / n));
    }
    out.push_back(l.along.back());
    return out;
}

/// The first rule `l` breaks (none: "").
std::string breaks(const Landform& l, const std::vector<Rule>& rs, const RegionFile& f) {
    const double r = reach(l);
    // A swell is gentle ground a machine drives over: only the places and
    // passages keep it off, not a pad's field of fire.
    // Low ground the Citadel's guns shoot over (a shelf is one level, the
    // guns stand nine cells up): only the base's own ground keeps it off.
    const bool gentle = l.kind == LandformKind::swell || l.kind == LandformKind::wash || l.kind == LandformKind::boulders ||
                        l.kind == LandformKind::shelf;
    // Boulders block a passage; a swell or a wash does not.
    const bool crosses = l.kind == LandformKind::swell || l.kind == LandformKind::wash;
    for (Vec2 p : probes(l)) {
        for (const Rule& rule : rs) {
            if (rule.lane && crosses) continue;
            const bool fire = rule.radius == fieldOfFire && rule.why.find("field of fire") != std::string::npos;
            // The pad's own ground: room for a Citadel (12 cells) and the base
            // round it, even for gentle forms; solid ones stay out of the
            // guns' whole field of fire.
            const double base = l.kind == LandformKind::swell ? 26.0 : 16.0;  // a swell rises over a base's sight lines
            const double need = fire ? (gentle ? base + r : rule.radius + r) : rule.radius + r;
            if (distance(p, rule.at) < need) return "in " + rule.why;
        }
        // The rock itself inside the land; its apron may run into a wall.
        const double body = l.kind == LandformKind::wash ? 0 : (l.kind == LandformKind::escarpment ? 2 : 0.6 * r);
        if (f.border && !(signedDistance(p, f.border->outline) < -body)) return "out of the land (past the border)";
    }
    return "";
}

/// An outline round `c`, `radius` across (stretched to `stretch` across
/// the other way, turned by `turn`), its edge bent by noise `rough` deep and
/// bitten in `bites` times by erosion: never a straight run.
std::vector<Vec2> blob(Vec2 c, double radius, double stretch, double turn, double rough, int bites, SeededRandom& rng, const Noise& noise,
                       int n = 30) {
    const double ox = rng.range(-50, 50), oy = rng.range(-50, 50);
    struct Bite { double at, width, depth; };
    std::vector<Bite> bs;
    for (int i = 0; i < bites; i++) bs.push_back({rng.range(0, 2 * pi), rng.range(0.18, 0.4), rng.range(0.2, 0.42)});
    std::vector<Vec2> out;
    for (int i = 0; i < n; i++) {
        const double a = 2 * pi * i / n;
        double r = radius * (1 + rough * noise.fbm(std::cos(a) * 1.4 + ox, std::sin(a) * 1.4 + oy, 3) +
                             0.35 * rough * noise.fbm(std::cos(a) * 4 + oy, std::sin(a) * 4 - ox, 2));
        for (const Bite& b : bs) {
            double d = std::fabs(a - b.at);
            d = std::min(d, 2 * pi - d);
            r *= 1 - b.depth * std::exp(-(d / b.width) * (d / b.width));
        }
        r = std::max(r, radius * 0.35);
        const double u = r * std::cos(a), v = r * std::sin(a) * stretch;
        out.push_back(c + Vec2(u * std::cos(turn) - v * std::sin(turn), u * std::sin(turn) + v * std::cos(turn)));
    }
    return out;
}

/// A line resampled every `step` cells and bent sideways by noise up to
/// `wander` cells (its ends held).
std::vector<Vec2> meander(const std::vector<Vec2>& line, double step, double wander, SeededRandom& rng, const Noise& noise) {
    std::vector<Vec2> pts;
    for (size_t i = 0; i + 1 < line.size(); i++) {
        const Vec2 a = line[i], b = line[i + 1];
        const double n = std::max(1.0, std::ceil(distance(a, b) / step));
        for (double k = 0; k < n; k++) pts.push_back(a + (b - a) * (k / n));
    }
    pts.push_back(line.back());
    const double o = rng.range(-50, 50);
    double run = 0;
    std::vector<Vec2> out;
    for (size_t i = 0; i < pts.size(); i++) {
        if (i > 0) run += distance(pts[i], pts[i - 1]);
        const Vec2 d = pts[std::min(i + 1, pts.size() - 1)] - pts[i > 0 ? i - 1 : 0];
        const double len = std::max(length(d), 1e-6);
        const Vec2 side(-d.y / len, d.x / len);
        const double t = pts.size() > 1 ? double(i) / double(pts.size() - 1) : 0;
        const double hold = std::sin(pi * t);
        out.push_back(pts[i] + side * (wander * hold * noise.fbm(run * 0.09 + o, o * 0.5, 3)));
    }
    return out;
}

Vec2 faceDir(std::optional<Edge> face) {
    switch (face.value_or(Edge::south)) {
    case Edge::north: return Vec2(0, -1);
    case Edge::south: return Vec2(0, 1);
    case Edge::east: return Vec2(1, 0);
    case Edge::west: return Vec2(-1, 0);
    }
    return Vec2(0, 1);
}

/// Rocks round a foot: `n` of them `from`…`to` cells out of the outline's
/// middle `c` (radius `r`), boulders and rocks.
void talus(RegionFile& out, Vec2 c, double r, int n, double from, double to, SeededRandom& rng) {
    for (int i = 0; i < n; i++) {
        const double a = rng.range(0, 2 * pi);
        const double d = r + rng.range(from, to);
        const bool big = rng.unit() < 0.3;
        out.doodads.push_back(Prop{big ? Doodad::Kind::boulder : Doodad::Kind::rock, c + Vec2(std::cos(a), std::sin(a)) * d,
                                   rng.range(0, 360), big ? rng.range(0.8, 1.7) : rng.range(0.5, 1.3)});
    }
}

/// A talus apron under free-standing rock: the ground round its foot
/// rising a level over a slope, so the rock stands on a skirt of its own
/// debris, not straight out of the flat.
void apron(RegionFile& out, Vec2 c, double r, SeededRandom& rng, const Noise& noise) {
    Edit e;
    e.op = HeightEdit::Kind::raise;
    e.area.points = blob(c, r, rng.range(0.75, 1.0), rng.range(0, pi), 0.3, 1, rng, noise, 24);
    e.level = 1;
    e.slope = 3.5;
    out.height.push_back(e);
}

void island(RegionFile& out, std::vector<Vec2> outline, int64_t level, IslandKind kind) {
    if (out.border) {
        out.border->islands.push_back(std::move(outline));
        out.border->islandLevels.resize(out.border->islands.size() - 1, 0);
        out.border->islandKinds.resize(out.border->islands.size() - 1, IslandKind::wall);
        out.border->islandLevels.push_back(level);
        out.border->islandKinds.push_back(kind);
        return;
    }
    // No border to hold islands: a cliff-sided raise instead.
    Edit e;
    e.op = HeightEdit::Kind::raise;
    e.area.points = std::move(outline);
    e.level = std::min(level, maxLevel);
    out.height.push_back(e);
}

/// Draws landform `l` into `out`.
void draw(RegionFile& out, const Landform& l, uint64_t seed) {
    SeededRandom rng(seed);
    const Noise noise(seed * 0x9e3779b97f4a7c15ull + 7);
    const size_t s = static_cast<size_t>(l.size);
    switch (l.kind) {
    case LandformKind::butte: {
        const double r = std::array{3.0, 4.5, 6.0}[s];
        const int64_t level = l.height ? l.height : std::array<int64_t, 3>{3, 4, 4}[s];
        apron(out, *l.at, r * 1.25, rng, noise);
        island(out, blob(*l.at, r, rng.range(0.7, 1.0), rng.range(0, pi), 0.22, int(rng.range(1, 3)), rng, noise), level, IslandKind::butte);
        talus(out, *l.at, r, int(r * 2.2), 0.6, 2.6, rng);
        break;
    }
    case LandformKind::mesa: {
        const double r = std::array{6.0, 8.0, 11.0}[s];
        const int64_t level = l.height ? l.height : std::array<int64_t, 3>{2, 2, 3}[s];
        apron(out, *l.at, r * 1.1, rng, noise);
        island(out, blob(*l.at, r, rng.range(0.5, 0.8), rng.range(0, pi), 0.28, int(rng.range(2, 4)), rng, noise), level, IslandKind::mesa);
        talus(out, *l.at, r * 0.8, int(r * 2), 0.8, 3.5, rng);
        break;
    }
    case LandformKind::spires: {
        const double spread = std::array{5.0, 7.0, 10.0}[s];
        const int count = int(l.count ? l.count : std::array<int64_t, 3>{3, 5, 7}[s]);
        const int64_t top = l.height ? l.height : 5;
        std::vector<Vec2> placed;
        for (int i = 0, tries = 0; i < count && tries < 400; tries++) {
            const double a = rng.range(0, 2 * pi), d = spread * std::sqrt(rng.unit());
            const Vec2 c = *l.at + Vec2(std::cos(a), std::sin(a)) * d;
            if (std::any_of(placed.begin(), placed.end(), [&](Vec2 q) { return distance(q, c) < 4.2; })) continue;
            placed.push_back(c);
            const double r = rng.range(1.5, 2.4);
            if (i == 0) apron(out, *l.at, spread * 0.8, rng, noise);
            island(out, blob(c, r, rng.range(0.75, 1.0), rng.range(0, pi), 0.18, 1, rng, noise, 12),
                   std::max<int64_t>(2, top - int64_t(rng.range(0, 2.99))), IslandKind::spire);
            talus(out, c, r, 3, 0.4, 1.8, rng);
            i++;
        }
        break;
    }
    case LandformKind::escarpment: {
        const double depth = std::array{8.0, 12.0, 18.0}[s];
        const Vec2 low = faceDir(l.face);
        const std::vector<Vec2> front = meander(l.along, 1.0, 2.5, rng, noise);
        // The face's own erosion: bays bitten in along it.
        std::vector<double> bite(front.size(), 0);
        for (int b = 0; b < int(front.size() / 10) + 1; b++) {
            const double at = rng.range(0, double(front.size() - 1)), w = rng.range(1.5, 4), d = rng.range(1, 3);
            for (size_t i = 0; i < front.size(); i++) bite[i] += d * std::exp(-((double(i) - at) / w) * ((double(i) - at) / w));
        }
        std::vector<Vec2> poly;
        for (size_t i = 0; i < front.size(); i++) poly.push_back(front[i] - low * bite[i]);
        // The back, tapering at each end so it dies out, not stops.
        for (size_t k = front.size(); k-- > 0;) {
            const double t = front.size() > 1 ? double(k) / double(front.size() - 1) : 0;
            // Round ends (a square root's shoulder), the back's own wander a
            // third of the depth and its small bays: no run stays straight.
            const double taper = std::sqrt(std::min(1.0, std::min(t, 1 - t) / 0.3));
            const double wander = 1 + 0.35 * noise.fbm(double(k) * 0.06 + 13, 9, 3);
            poly.push_back(front[k] - low * (2 + (depth - 2) * taper * wander + 1.6 * noise.fbm(double(k) * 0.3, 21, 2)) +
                           Vec2(-low.y, low.x) * (1.2 * noise.fbm(double(k) * 0.25, 33, 2)));
        }
        Edit e;
        e.op = HeightEdit::Kind::raise;
        e.area.points = poly;
        e.level = std::min<int64_t>(l.height ? l.height : 1, maxLevel);
        out.height.push_back(e);
        // A way up: a ramp where the face is least bitten, near its middle
        // (5 cells a level, its ends past the cliff on both levels).
        {
            size_t best = front.size() / 2;
            for (size_t i = front.size() / 3; i < 2 * front.size() / 3; i++)
                if (bite[i] < bite[best]) best = i;
            const Vec2 foot = front[best] - low * bite[best];
            Edit ramp;
            ramp.op = HeightEdit::Kind::ramp;
            ramp.line = {foot + low * (5.0 * double(e.level) + 1.0), foot - low * 4.0};
            ramp.width = 5;
            ramp.fromLevel = 0;
            ramp.level = e.level;
            out.height.push_back(ramp);
        }
        for (size_t i = 0; i < front.size(); i += 2)
            if (rng.unit() < 0.7) {
                const bool big = rng.unit() < 0.3;
                out.doodads.push_back(Prop{big ? Doodad::Kind::boulder : Doodad::Kind::rock, front[i] - low * bite[i] + low * rng.range(0.9, 2.6) +
                                                                                             Vec2(-low.y, low.x) * rng.range(-0.8, 0.8),
                                           rng.range(0, 360), big ? rng.range(0.8, 1.6) : rng.range(0.5, 1.2)});
            }
        break;
    }
    case LandformKind::shelf: {
        const double r = std::array{5.0, 8.0, 12.0}[s];
        Edit e;
        e.op = HeightEdit::Kind::raise;
        e.area.points = blob(*l.at, r, rng.range(0.32, 0.55), rng.range(0, pi), 0.2, int(rng.range(1, 3)), rng, noise);
        e.level = std::min<int64_t>(l.height ? l.height : 1, maxLevel);
        e.slope = 2.5;
        out.height.push_back(e);
        talus(out, *l.at, r * 0.45, int(r * 0.8), 0.5, 2.5, rng);
        break;
    }
    case LandformKind::swell: {
        const double r = std::array{8.0, 12.0, 18.0}[s];
        Edit e;
        e.op = HeightEdit::Kind::raise;
        e.area.points = blob(*l.at, r * 0.55, rng.range(0.55, 1.0), rng.range(0, pi), 0.3, 0, rng, noise, 20);
        e.level = std::min<int64_t>(l.height ? l.height : 1, maxLevel);
        e.slope = std::array{6.0, 8.0, 10.0}[s];
        out.height.push_back(e);
        break;
    }
    case LandformKind::wash: {
        Edit e;
        e.op = HeightEdit::Kind::cut;
        e.line = meander(l.along, 1.5, 3.0, rng, noise);
        e.width = std::array{2.0, 3.0, 4.5}[s];
        e.level = 0;
        e.slope = 1.5;
        out.height.push_back(e);
        break;
    }
    case LandformKind::boulders: {
        const double spread = std::array{3.0, 5.0, 8.0}[s];
        const int count = int(l.count ? l.count : std::array<int64_t, 3>{4, 8, 14}[s]);
        for (int i = 0; i < count; i++) {
            const double a = rng.range(0, 2 * pi), d = spread * std::sqrt(rng.unit());
            const double k = rng.unit();
            const Doodad::Kind kind = k < 0.08 ? Doodad::Kind::rockSpire : (k < 0.5 ? Doodad::Kind::boulder : Doodad::Kind::rock);
            out.doodads.push_back(Prop{kind, *l.at + Vec2(std::cos(a), std::sin(a)) * d, rng.range(0, 360),
                                       kind == Doodad::Kind::rockSpire ? rng.range(0.8, 1.6) : rng.range(0.5, 1.8)});
        }
        break;
    }
    }
}

/// A style's own forms: what, how big, how many (docs/world-sculpting.md).
struct Want {
    LandformKind kind;
    LandformSize size;
    int count;
};

std::vector<Want> styleForms(SculptStyle s) {
    using K = LandformKind;
    using Z = LandformSize;
    switch (s) {
    case SculptStyle::openDesert:
        return {{K::butte, Z::large, 1}, {K::butte, Z::medium, 2}, {K::mesa, Z::medium, 1}, {K::spires, Z::medium, 2},
                {K::shelf, Z::medium, 3}, {K::swell, Z::large, 3}, {K::swell, Z::medium, 3}, {K::boulders, Z::medium, 5},
                {K::wash, Z::medium, 1}};
    case SculptStyle::badlands:
        return {{K::shelf, Z::large, 4}, {K::shelf, Z::medium, 4}, {K::butte, Z::medium, 2}, {K::spires, Z::large, 2},
                {K::swell, Z::medium, 3}, {K::boulders, Z::large, 5}, {K::wash, Z::large, 2}};
    case SculptStyle::mesaCountry:
        return {{K::mesa, Z::large, 2}, {K::mesa, Z::medium, 2}, {K::butte, Z::large, 2}, {K::spires, Z::small, 2},
                {K::swell, Z::large, 3}, {K::boulders, Z::medium, 4}};
    case SculptStyle::none: return {};
    }
    return {};
}

/// The outlines already taken (the file's raises and islands), for the
/// style to keep its forms apart from.
struct Taken {
    std::vector<std::vector<Vec2>> polys;
    std::vector<std::pair<Vec2, double>> discs;
    bool clear(Vec2 p, double r) const {
        for (const auto& poly : polys)
            if (poly.size() >= 3 && signedDistance(p, poly) < r) return false;
        for (const auto& [c, cr] : discs)
            if (distance(p, c) < r + cr) return false;
        return true;
    }
};

} // namespace

std::vector<Problem> sculptProblems(const RegionFile& f) {
    std::vector<Problem> out;
    if (!f.sculpt) return out;
    const std::vector<Rule> rs = rules(f);
    for (size_t i = 0; i < f.sculpt->forms.size(); i++) {
        const Landform& l = f.sculpt->forms[i];
        const std::string why = breaks(l, rs, f);
        if (!why.empty())
            out.push_back(Problem{"sculpt.forms[" + std::to_string(i) + "]", "a " + kindName(l.kind) + " " + why,
                                  l.at ? *l.at : l.along.front()});
    }
    return out;
}

RegionFile expand(const RegionFile& f, std::vector<std::string>* notes) {
    if (!f.sculpt) return f;
    RegionFile out = f;
    out.sculpt.reset();
    const Sculpt& sc = *f.sculpt;
    const uint64_t base = fnv1a64(f.id) ^ (static_cast<uint64_t>(sc.seed) * 0x9e3779b97f4a7c15ull);
    // The file's own forms, as written.
    for (size_t i = 0; i < sc.forms.size(); i++) {
        const Landform& l = sc.forms[i];
        draw(out, l, base + 0x1000 * (i + 1) + static_cast<uint64_t>(l.seed));
    }
    // The style's own, where the rules and the land leave room.
    const std::vector<Rule> rs = rules(f);
    Taken taken;
    for (const Edit& e : f.height)
        if (e.op == HeightEdit::Kind::raise || e.op == HeightEdit::Kind::lower || e.op == HeightEdit::Kind::flatten)
            taken.polys.push_back(e.area.outline());
    if (f.border)
        for (const auto& is : f.border->islands) taken.polys.push_back(is);
    for (const Landform& l : sc.forms)
        if (l.at) taken.discs.push_back({*l.at, reach(l)});
        else
            for (Vec2 p : probes(l)) taken.discs.push_back({p, reach(l)});
    SeededRandom rng(base ^ 0x5eed);
    const Vec2 ext = f.extent();
    int n = 0;
    for (const Want& w : styleForms(sc.style)) {
        int placed = 0;
        for (int k = 0; k < w.count; k++) {
            bool done = false;
            for (int tries = 0; tries < 900 && !done; tries++) {
                Landform l;
                l.kind = w.kind;
                // No room at its size: a size smaller (after 300 tries each).
                l.size = static_cast<LandformSize>(std::max(0, static_cast<int>(w.size) - tries / 300));
                const double r = reach(l);
                if (w.kind == LandformKind::wash) {
                    // A bed across the land: two far points and a bend.
                    const Vec2 a(rng.range(8, ext.x - 8), rng.range(8, ext.y - 8)), b(rng.range(8, ext.x - 8), rng.range(8, ext.y - 8));
                    if (distance(a, b) < 0.45 * ext.x) continue;
                    const Vec2 mid = (a + b) * 0.5 + Vec2(rng.range(-10, 10), rng.range(-10, 10));
                    l.along = {a, mid, b};
                } else {
                    l.at = Vec2(rng.range(r, ext.x - r), rng.range(r, ext.y - r));
                }
                if (!breaks(l, rs, f).empty()) continue;
                bool room = true;
                for (Vec2 p : probes(l)) room = room && taken.clear(p, r + (w.kind == LandformKind::swell ? 0 : 6));
                if (!room) continue;
                draw(out, l, base + 0x777 * (++n));
                if (l.at) taken.discs.push_back({*l.at, r});
                else
                    for (Vec2 p : probes(l)) taken.discs.push_back({p, r});
                done = true;
                placed++;
            }
        }
        if (notes)
            notes->push_back(kindName(w.kind) + " (" + std::string(EnumInfo<LandformSize>::names[static_cast<size_t>(w.size)]) + "): " +
                             std::to_string(placed) + " of " + std::to_string(w.count) + (placed < w.count ? ", no room for the rest" : ""));
    }
    // Last, the rim and the hand-written ground worn: no edge runs straight.
    erode(out, f);
    return out;
}

} // namespace ac::region
