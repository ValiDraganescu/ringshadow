// The region seen at an angle (View.h).

#include "View.h"

#include "Font.h"
#include "NavGrid.h"
#include "TerrainField.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>

namespace ac::regiontool {

namespace {

using region::Built;
using region::RegionFile;

constexpr double degrees = 3.14159265358979323846 / 180;

struct V3 {
    double x = 0, y = 0, z = 0;
    V3 operator+(V3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(V3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator*(double k) const { return {x * k, y * k, z * k}; }
};
double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3 unit(V3 a) { return a * (1 / std::sqrt(dot(a, a))); }

struct Rgb { double r, g, b; };
Rgb mix(Rgb a, Rgb b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Rgb scaled(Rgb a, double k) { return {a.r * k, a.g * k, a.b * k}; }

/// What a grid sample is, for its colour.
enum Material : uint8_t { ground, scenery, rock, landmark, ore, well, pad, spawn, site };

/// The ground's palette (docs/regions.md, "ground"), roughly.
Rgb groundColour(region::Ground g) {
    switch (g) {
    case region::Ground::plain: return {150, 116, 82};
    case region::Ground::rust: return {160, 88, 60};
    case region::Ground::salt: return {206, 200, 186};
    case region::Ground::ash: return {148, 146, 142};
    case region::Ground::glass: return {74, 78, 86};
    case region::Ground::slag: return {62, 56, 52};
    case region::Ground::frost: return {138, 150, 164};
    case region::Ground::ochre: return {188, 144, 80};
    }
    return {150, 116, 82};
}

constexpr Rgb wallColour{122, 118, 114};
constexpr Rgb islandColour{146, 112, 90};
constexpr Rgb rockColour{124, 116, 108};
constexpr Rgb landmarkColour{120, 92, 150};
constexpr Rgb oreColour{176, 110, 236};
constexpr Rgb wellColour{226, 236, 250};
constexpr Rgb padColour{250, 206, 60};
constexpr Rgb spawnColour{80, 235, 130};
constexpr Rgb siteColour{232, 168, 36};
constexpr Rgb skyHigh{122, 150, 188};
constexpr Rgb skyLow{204, 208, 210};
constexpr Rgb ink{235, 236, 238};

/// A height grid over a rectangle of ground, a sample every `step` cells.
struct Grid {
    double x0 = 0, z0 = 0, step = 1;
    int w = 0, d = 0;
    std::vector<float> h;
    /// The fine grid's: what each sample is (nearest), and, read smooth,
    /// the bare ground's slope, the walls' coverage and the islands' share.
    std::vector<uint8_t> m;
    std::vector<float> slope, wall, isle;

    bool covers(double x, double z) const {
        return x >= x0 && z >= z0 && x <= x0 + (w - 1) * step && z <= z0 + (d - 1) * step;
    }
    float& at(int i, int k) { return h[static_cast<size_t>(k) * static_cast<size_t>(w) + static_cast<size_t>(i)]; }
    float at(int i, int k) const { return h[static_cast<size_t>(k) * static_cast<size_t>(w) + static_cast<size_t>(i)]; }
    double smooth(const std::vector<float>& g, double x, double z) const {
        const double u = std::clamp((x - x0) / step, 0.0, w - 1.001), v = std::clamp((z - z0) / step, 0.0, d - 1.001);
        const int i = static_cast<int>(u), k = static_cast<int>(v);
        const double fu = u - i, fv = v - k;
        const size_t n = static_cast<size_t>(k) * static_cast<size_t>(w) + static_cast<size_t>(i), row = static_cast<size_t>(w);
        const double a = g[n] + (g[n + 1] - g[n]) * fu;
        const double b = g[n + row] + (g[n + row + 1] - g[n + row]) * fu;
        return a + (b - a) * fv;
    }
    double height(double x, double z) const { return smooth(h, x, z); }
    uint8_t material(double x, double z) const {
        const int i = std::clamp(static_cast<int>(std::lround((x - x0) / step)), 0, w - 1);
        const int k = std::clamp(static_cast<int>(std::lround((z - z0) / step)), 0, d - 1);
        return m[static_cast<size_t>(k) * static_cast<size_t>(w) + static_cast<size_t>(i)];
    }
};

template <class F> void rows(int count, F f) {
    const int threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&, t] { for (int r = t; r < count; r += threads) f(r); });
    for (std::thread& th : pool) th.join();
}

/// The ground: a fine grid over the region and a band round it, a coarse
/// one over the scenery further out.
struct Scene {
    Grid fine, coarse;
    std::vector<float> bare;
    double lo = 0, hi = 0;

    double height(double x, double z) const {
        return fine.covers(x, z) ? fine.height(x, z) : coarse.height(x, z);
    }
    bool inside(double x, double z) const { return coarse.covers(x, z); }
    uint8_t material(double x, double z) const {
        return fine.covers(x, z) ? fine.material(x, z) : static_cast<uint8_t>(scenery);
    }
    V3 normal(double x, double z, double e) const {
        const double dx = height(x + e, z) - height(x - e, z), dz = height(x, z + e) - height(x, z - e);
        return unit({-dx, 2 * e, -dz});
    }
};

void fill(Grid& g, const TerrainField& field, const GroundRect& b, bool materials) {
    g.h.assign(static_cast<size_t>(g.w) * static_cast<size_t>(g.d), 0);
    if (materials) {
        g.m.assign(g.h.size(), ground);
        g.wall.assign(g.h.size(), 0);
        g.isle.assign(g.h.size(), 0);
        g.slope.assign(g.h.size(), 0);
    }
    const bool walls = field.map.edges && !field.map.edges->outline.empty();
    rows(g.d, [&](int k) {
        for (int i = 0; i < g.w; i++) {
            const Vec2 p(g.x0 + i * g.step, g.z0 + k * g.step);
            g.at(i, k) = static_cast<float>(field.borderHeight(p));
            if (!materials) continue;
            const size_t n = static_cast<size_t>(k) * static_cast<size_t>(g.w) + static_cast<size_t>(i);
            if (p.x < b.minX || p.x > b.maxX || p.y < b.minZ || p.y > b.maxZ) g.m[n] = scenery;
            else if (walls) {
                const TerrainField::Wall w = field.wall(p);
                g.wall[n] = static_cast<float>(w.coverage);
                g.isle[n] = w.island >= 0 ? 1.0f : 0.0f;
            }
        }
    });
}

/// `bare`: a pad left unpainted (the one a pilot's eye stands on).
Scene scene(const Built& built, std::optional<Vec2> bare) {
    const MapDefinition& map = built.map;
    const TerrainField field(map);
    const GroundRect& b = map.bounds;
    Scene s;
    const double margin = 48, far = 220;
    s.fine.step = 1.0 / 3;
    s.fine.x0 = b.minX - margin, s.fine.z0 = b.minZ - margin;
    s.fine.w = static_cast<int>((b.maxX - b.minX + 2 * margin) / s.fine.step) + 1;
    s.fine.d = static_cast<int>((b.maxZ - b.minZ + 2 * margin) / s.fine.step) + 1;
    fill(s.fine, field, b, true);
    s.coarse.step = 2;
    s.coarse.x0 = b.minX - far, s.coarse.z0 = b.minZ - far;
    s.coarse.w = static_cast<int>((b.maxX - b.minX + 2 * far) / s.coarse.step) + 1;
    s.coarse.d = static_cast<int>((b.maxZ - b.minZ + 2 * far) / s.coarse.step) + 1;
    fill(s.coarse, field, b, false);

    // Cliffs: the walking grid's slope on the bare ground, before the marks.
    Grid& f = s.fine;
    const int e = static_cast<int>(std::lround(0.5 / f.step));
    rows(f.d, [&](int k) {
        for (int i = e; i < f.w - e; i++) {
            if (k < e || k >= f.d - e) continue;
            const double dx = f.at(i + e, k) - f.at(i - e, k), dz = f.at(i, k + e) - f.at(i, k - e);
            f.slope[static_cast<size_t>(k) * static_cast<size_t>(f.w) + static_cast<size_t>(i)] =
                static_cast<float>(std::sqrt(dx * dx + dz * dz));
        }
    });
    // The bare ground's height, for the level tint.
    s.bare = f.h;

    // Marks: raise or paint the fine grid round a point.
    const auto around = [&](Vec2 c, double r, auto paint) {
        const int i0 = std::max(0, static_cast<int>((c.x - r - f.x0) / f.step)), i1 = std::min(f.w - 1, static_cast<int>((c.x + r - f.x0) / f.step) + 1);
        const int k0 = std::max(0, static_cast<int>((c.y - r - f.z0) / f.step)), k1 = std::min(f.d - 1, static_cast<int>((c.y + r - f.z0) / f.step) + 1);
        for (int k = k0; k <= k1; k++)
            for (int i = i0; i <= i1; i++) {
                const Vec2 q(f.x0 + i * f.step, f.z0 + k * f.step);
                paint(i, k, q - c);
            }
    };
    const auto mat = [&](int i, int k) -> uint8_t& { return f.m[static_cast<size_t>(k) * static_cast<size_t>(f.w) + static_cast<size_t>(i)]; };
    // Rocks: a dome on each solid circle (a spire a tall cone); other
    // solid doodads (landmarks, crates, towers) a lower dome in violet.
    for (const Doodad& d : map.doodads) {
        if (d.kind == Doodad::Kind::broodTunnel || d.kind == Doodad::Kind::buildSpot) continue;
        const bool stone = d.kind == Doodad::Kind::rock || d.kind == Doodad::Kind::boulder || d.kind == Doodad::Kind::rockSpire;
        const bool spire = d.kind == Doodad::Kind::rockSpire;
        auto circles = NavGrid::circles(d);
        // A spire: one tall cone on its widest circle.
        if (spire && !circles.empty()) circles.resize(1);
        for (const auto& [centre, radius] : circles) {
            const double r = std::max(radius, 0.3);
            const double tall = spire ? 3.0 * d.scale : (stone ? 0.9 : 0.8) * r + 0.3;
            around(centre, r, [&](int i, int k, Vec2 o) {
                const double t = length(o) / r;
                if (t > 1) return;
                const double rise = spire ? tall * std::pow(1 - t, 1.4) : tall * std::sqrt(1 - t * t);
                // On the ground under it, so a rock on a cliff's edge
                // follows the face down rather than hanging over it.
                const float h = static_cast<float>(s.bare[static_cast<size_t>(k) * static_cast<size_t>(f.w) + static_cast<size_t>(i)] + rise);
                if (h > f.at(i, k)) {
                    f.at(i, k) = h;
                    mat(i, k) = stone ? rock : landmark;
                }
            });
        }
    }
    // Ore: a low violet mound, 2 by 1, turned like the deposit.
    std::vector<OreSpec> deposits = map.deposits;
    for (const BaseSite& bs : map.bases) deposits.insert(deposits.end(), bs.ore.begin(), bs.ore.end());
    for (const OreSpec& o : deposits) {
        const double cs = std::cos(o.angle), sn = std::sin(o.angle);
        const double base = s.height(o.position.x, o.position.y);
        around(o.position, 1.2, [&](int i, int k, Vec2 v) {
            const double lx = v.x * cs - v.y * sn, lz = v.x * sn + v.y * cs;
            const double t = std::max(std::fabs(lx), 2 * std::fabs(lz));
            if (t > 1) return;
            f.at(i, k) = std::max(f.at(i, k), static_cast<float>(base + 0.55 * std::sqrt(1 - t * t)));
            mat(i, k) = ore;
        });
    }
    // Wells: a silver disc, a little raised.
    std::vector<Vec2> wells;
    for (const WellSpec& w : map.wells) wells.push_back(w.position);
    for (const BaseSite& bs : map.bases) wells.insert(wells.end(), bs.wells.begin(), bs.wells.end());
    for (Vec2 w : wells) {
        const double base = s.height(w.x, w.y);
        around(w, 1.5, [&](int i, int k, Vec2 v) {
            const double t = length(v) / 1.5;
            if (t > 1) return;
            f.at(i, k) = static_cast<float>(base + (t < 0.35 ? 0.9 : 0.25));
            mat(i, k) = well;
        });
    }
    // Pads: two yellow rings painted; spawn points a green disc; sites a
    // gold square.
    for (const region::Place& p : built.pads)
        if (!bare || distance(*bare, p.at) > region::padRadius + 1) around(p.at, region::padRadius + 0.3, [&](int i, int k, Vec2 v) {
            const double r = length(v);
            if (std::fabs(r - region::padRadius) < 0.3 || std::fabs(r - region::padRadius * 0.45) < 0.25) mat(i, k) = pad;
        });
    for (const region::Place& p : built.spawns)
        around(p.at, 1.2, [&](int i, int k, Vec2 v) { if (std::fabs(v.x) + std::fabs(v.y) <= 1.2) mat(i, k) = spawn; });
    for (const region::Site& t : built.sites)
        around(t.at, 1.1, [&](int i, int k, Vec2) { mat(i, k) = site; });

    s.lo = 1e9, s.hi = -1e9;
    for (float h : s.coarse.h) s.lo = std::min(s.lo, double(h)), s.hi = std::max(s.hi, double(h));
    for (float h : f.h) s.lo = std::min(s.lo, double(h)), s.hi = std::max(s.hi, double(h));
    return s;
}

struct Camera {
    V3 eye, forward, right, up;
    double tanX = 1, tanY = 1;

    void look(V3 f) {
        forward = unit(f);
        right = unit(cross(forward, {0, 1, 0}));
        up = cross(right, forward);
    }
    /// Screen position in [-1, 1] (y up) and the depth along `forward`.
    bool project(V3 p, double& sx, double& sy, double& depth) const {
        const V3 v = p - eye;
        depth = dot(v, forward);
        if (depth <= 0.01) return false;
        sx = dot(v, right) / (depth * tanX);
        sy = dot(v, up) / (depth * tanY);
        return true;
    }
};

/// A horizontal unit vector for a compass bearing (x east, z south).
V3 bearing(double azimuth) { return {std::sin(azimuth * degrees), 0, -std::cos(azimuth * degrees)}; }

std::string compass(double a) {
    static const char* names[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    a = std::fmod(std::fmod(a, 360) + 360, 360);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s (%.0f deg)", names[static_cast<int>(std::lround(a / 45)) % 8], a);
    return buf;
}

struct Painter {
    Image& image;
    void put(int x, int y, Rgb c, double a = 1) {
        if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
        uint8_t* p = &image.rgb[(static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 3];
        const double v[3] = {c.r, c.g, c.b};
        for (int k = 0; k < 3; k++) p[k] = static_cast<uint8_t>(std::clamp(std::lround(p[k] + (v[k] - p[k]) * a), 0L, 255L));
    }
    void rect(int x0, int y0, int x1, int y1, Rgb c, double a = 1) {
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) put(x, y, c, a);
    }
    void text(int x, int y, const std::string& t, Rgb c, int scale = 2) {
        const int advance = 6 * scale;
        rect(x - 4, y - 4, x + static_cast<int>(t.size()) * advance + 3, y + 7 * scale + 4, {20, 22, 26}, 0.7);
        for (size_t i = 0; i < t.size(); i++) {
            const Glyph& g = glyph(t[i]);
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (g.rows[static_cast<size_t>(row)][static_cast<size_t>(col)] == '#')
                        rect(x + static_cast<int>(i) * advance + col * scale, y + row * scale,
                             x + static_cast<int>(i) * advance + (col + 1) * scale, y + (row + 1) * scale, c);
        }
    }
};

bool number(const std::string& s, double& out) {
    char* end = nullptr;
    out = std::strtod(s.c_str(), &end);
    return !s.empty() && *end == '\0' && std::isfinite(out);
}

} // namespace

std::optional<ViewOptions> viewOptions(const std::vector<std::string>& args, std::string& error) {
    ViewOptions o;
    for (size_t i = 2; i < args.size(); i++) {
        const std::string& a = args[i];
        if (a == "--eye") { o.eye = true; continue; }
        if (a == "-o") { i++; continue; }
        if (i + 1 >= args.size()) { error = a + " needs a value"; return std::nullopt; }
        const std::string& v = args[++i];
        double x = 0;
        if (a == "--from") {
            if (v == "n") o.azimuth = 0;
            else if (v == "e") o.azimuth = 90;
            else if (v == "s") o.azimuth = 180;
            else if (v == "w") o.azimuth = 270;
            else if (number(v, x)) o.azimuth = x;
            else { error = "--from takes n, e, s, w or degrees"; return std::nullopt; }
        } else if (a == "--pitch" && number(v, x) && x > -89 && x < 89.5) o.pitch = x;
        else if (a == "--dist" && number(v, x) && x > 0) o.distance = x;
        else if (a == "--fov" && number(v, x) && x >= 5 && x <= 150) o.fov = x;
        else if (a == "--at") {
            const size_t comma = v.find(',');
            double z = 0;
            if (comma == std::string::npos || !number(v.substr(0, comma), x) || !number(v.substr(comma + 1), z)) {
                error = "--at takes X,Z (region cells)";
                return std::nullopt;
            }
            o.at = Vec2(x, z);
        } else if (a == "--size") {
            const size_t by = v.find('x');
            double h = 0;
            if (by == std::string::npos || !number(v.substr(0, by), x) || !number(v.substr(by + 1), h) || x < 64 || h < 64 ||
                x > 8192 || h > 8192) {
                error = "--size takes WxH (64 to 8192)";
                return std::nullopt;
            }
            o.width = static_cast<int>(x), o.height = static_cast<int>(h);
        } else {
            error = "bad option " + a + " " + v;
            return std::nullopt;
        }
    }
    return o;
}

Image view(const RegionFile& file, const Built& built, const ViewOptions& o) {
    const MapDefinition& map = built.map;
    const GroundRect& b = map.bounds;
    std::optional<Vec2> stand;
    if (o.eye) stand = o.at ? built.ground(*o.at) : !built.pads.empty() ? built.pads[0].at
                                                   : !built.spawns.empty() ? built.spawns[0].at
                                                                           : Vec2((b.minX + b.maxX) / 2, (b.minZ + b.maxZ) / 2);
    const Scene sc = scene(built, stand);
    const double H = map.levelHeight;

    Camera cam;
    const double fov = o.fov.value_or(o.eye ? 75 : 40);
    cam.tanX = std::tan(fov * degrees / 2);
    cam.tanY = cam.tanX * o.height / o.width;
    double reach = 0;  // how far a typical view looks, for the haze
    if (o.eye) {
        const Vec2 at = *stand;
        const double pitch = o.pitch.value_or(2);
        cam.eye = {at.x, sc.height(at.x, at.y) + 1.3, at.y};
        const V3 h = bearing(o.azimuth);
        cam.look({h.x * std::cos(pitch * degrees), -std::sin(pitch * degrees), h.z * std::cos(pitch * degrees)});
        reach = 140;
    } else {
        const Vec2 t = o.at ? built.ground(*o.at) : Vec2((b.minX + b.maxX) / 2, (b.minZ + b.maxZ) / 2);
        const double pitch = o.pitch.value_or(35);
        const V3 h = bearing(o.azimuth);
        cam.look({-h.x * std::cos(pitch * degrees), -std::sin(pitch * degrees), -h.z * std::cos(pitch * degrees)});
        const V3 target{t.x, o.at ? sc.height(t.x, t.y) : H * 0.8, t.y};
        double dist = 0;
        if (o.distance) dist = *o.distance;
        else {
            // The nearest distance at which the bounds, ground to wall top,
            // fit the picture.
            const double top = H * 4.5;
            const V3 corners[8] = {{b.minX, 0, b.minZ}, {b.maxX, 0, b.minZ}, {b.minX, 0, b.maxZ}, {b.maxX, 0, b.maxZ},
                                   {b.minX, top, b.minZ}, {b.maxX, top, b.minZ}, {b.minX, top, b.maxZ}, {b.maxX, top, b.maxZ}};
            double lo = 1, hi = 20000;
            for (int it = 0; it < 60; it++) {
                const double mid = (lo + hi) / 2;
                cam.eye = target - cam.forward * mid;
                bool fits = true;
                for (const V3& c : corners) {
                    double sx = 0, sy = 0, depth = 0;
                    fits = fits && cam.project(c, sx, sy, depth) && std::fabs(sx) <= 0.94 && std::fabs(sy) <= 0.9;
                }
                (fits ? hi : lo) = mid;
            }
            dist = hi;
        }
        cam.eye = target - cam.forward * dist;
        reach = dist;
    }

    const V3 sun = unit({std::sin(300 * degrees) * std::cos(35 * degrees), std::sin(35 * degrees),
                         -std::cos(300 * degrees) * std::cos(35 * degrees)});
    const Rgb soil = groundColour(file.ground);
    const double hazeStart = o.eye ? 20 : reach * 0.7, hazeScale = o.eye ? 260 : reach * 2.2;
    const double farthest = o.eye ? 700 : reach * 4 + 400;

    Image image;
    image.width = o.width, image.height = o.height;
    image.rgb.assign(static_cast<size_t>(o.width) * static_cast<size_t>(o.height) * 3, 0);
    std::vector<float> depthBuffer(static_cast<size_t>(o.width) * static_cast<size_t>(o.height), std::numeric_limits<float>::infinity());

    const auto skyAt = [&](V3 d) {
        const double t = std::clamp(d.y * 2.2, 0.0, 1.0);
        Rgb c = mix(skyLow, skyHigh, std::sqrt(t));
        const double glow = std::pow(std::max(0.0, dot(d, sun)), 24);
        return mix(c, Rgb{255, 240, 214}, glow * 0.6);
    };
    /// The sun's share at a lit point: 0 in full shadow, soft at the edge.
    const auto sunlight = [&](V3 p) {
        double light = 1, t = 0.15;
        while (t < 160) {
            const V3 q = p + sun * t;
            if (!sc.inside(q.x, q.z) || q.y > sc.hi) break;
            const double gap = q.y - sc.height(q.x, q.z);
            light = std::min(light, 10 * gap / t);
            if (light < 0) return 0.0;
            t += std::clamp(gap * 0.6, 0.12 + t * 0.01, 3.0);
        }
        return std::clamp(light, 0.0, 1.0);
    };

    rows(o.height, [&](int py) {
        for (int px = 0; px < o.width; px++) {
            const double sx = (2 * (px + 0.5) / o.width - 1) * cam.tanX, sy = (1 - 2 * (py + 0.5) / o.height) * cam.tanY;
            const V3 d = unit(cam.forward + cam.right * sx + cam.up * sy);
            Rgb col = skyAt(d);
            // March: steps grow with distance and with the gap to the ground.
            double t = 0.05, prev = t;
            bool hit = false;
            // Start where the ray first comes under the scene's top.
            if (cam.eye.y > sc.hi && d.y < 0) t = std::max(t, (cam.eye.y - sc.hi) / -d.y);
            while (t < farthest) {
                const V3 p = cam.eye + d * t;
                if (!sc.inside(p.x, p.z)) break;
                if (p.y > sc.hi && d.y >= 0) break;
                const double gap = p.y - sc.height(p.x, p.z);
                if (gap < 0) { hit = true; break; }
                prev = t;
                t += std::max(0.02 + t * 0.0015, gap * 0.35);
            }
            if (hit) {
                double a = prev, z = t;
                for (int k = 0; k < 10; k++) {
                    const double m = (a + z) / 2;
                    const V3 p = cam.eye + d * m;
                    (p.y - sc.height(p.x, p.z) < 0 ? z : a) = m;
                }
                t = z;
                const V3 p = cam.eye + d * t;
                const bool near = sc.fine.covers(p.x, p.z);
                const V3 n = sc.normal(p.x, p.z, near ? 0.33 : 2);
                const uint8_t m = sc.material(p.x, p.z);
                Rgb base = soil;
                if (near && m != scenery) {
                    // The level a little lighter each step up; cliffs (the
                    // walking grid's slope), walls and islands blended in.
                    const double level = std::clamp(sc.fine.smooth(sc.bare, p.x, p.z) / H, 0.0, 4.0);
                    base = scaled(soil, 0.84 + 0.09 * level);
                    const auto step01 = [](double a0, double a1, double v) {
                        const double q = std::clamp((v - a0) / (a1 - a0), 0.0, 1.0);
                        return q * q * (3 - 2 * q);
                    };
                    const double steep = step01(NavGrid::maxSlope * 0.8, NavGrid::maxSlope * 1.2, sc.fine.smooth(sc.fine.slope, p.x, p.z));
                    base = mix(base, mix(scaled(soil, 0.62), Rgb{104, 92, 82}, 0.45), steep);
                    const double walled = step01(0.3, 0.7, sc.fine.smooth(sc.fine.wall, p.x, p.z));
                    base = mix(base, mix(wallColour, islandColour, sc.fine.smooth(sc.fine.isle, p.x, p.z)), walled);
                } else base = mix(scaled(soil, 0.85), wallColour, 0.55);
                switch (m) {
                case rock: base = rockColour; break;
                case landmark: base = landmarkColour; break;
                case ore: base = oreColour; break;
                case well: base = wellColour; break;
                case pad: base = padColour; break;
                case spawn: base = spawnColour; break;
                case site: base = siteColour; break;
                default: break;
                }
                const bool flatMark = m == pad || m == spawn || m == site;
                const double lambert = std::max(0.0, dot(n, sun));
                const double shade = sunlight(p + n * 0.05);
                const double sky = 0.5 + 0.5 * n.y;
                double light = 0.40 * sky + 0.85 * lambert * shade;
                if (flatMark) light = std::max(light, 0.75);
                col = scaled(base, light);
                // The preview's grid, every 8 cells, faint, on the region.
                if (!o.eye && near && m != scenery) {
                    const double foot = std::max(0.08, t * 2 * cam.tanX / o.width);
                    const auto line = [&](double v) { const double f = std::fmod(v + 8000, 8.0); return std::min(f, 8 - f) < foot; };
                    if (line(p.x - b.minX) || line(p.z - b.minZ)) col = scaled(col, 0.8);
                }
                const double haze = 1 - std::exp(-std::max(0.0, t - hazeStart) / hazeScale);
                col = mix(col, skyLow, std::clamp(haze, 0.0, 0.85));
                depthBuffer[static_cast<size_t>(py) * static_cast<size_t>(o.width) + static_cast<size_t>(px)] = static_cast<float>(t);
            }
            uint8_t* out = &image.rgb[(static_cast<size_t>(py) * static_cast<size_t>(o.width) + static_cast<size_t>(px)) * 3];
            out[0] = static_cast<uint8_t>(std::clamp(std::lround(col.r), 0L, 255L));
            out[1] = static_cast<uint8_t>(std::clamp(std::lround(col.g), 0L, 255L));
            out[2] = static_cast<uint8_t>(std::clamp(std::lround(col.b), 0L, 255L));
        }
    });

    // Names over pads and spawn points where they show.
    Painter paint{image};
    const auto name = [&](Vec2 g, double over, const std::string& t, Rgb c) {
        const V3 p{g.x, sc.height(g.x, g.y) + over, g.y};
        double sx = 0, sy = 0, depth = 0;
        if (!cam.project(p, sx, sy, depth) || std::fabs(sx) > 1 || std::fabs(sy) > 1) return;
        const int x = static_cast<int>((sx + 1) / 2 * o.width), y = static_cast<int>((1 - sy) / 2 * o.height);
        const V3 v = p - cam.eye;
        const double along = std::sqrt(dot(v, v));
        // Hidden behind the ground: the ground at that pixel is nearer.
        const int cx = std::clamp(x, 0, o.width - 1), cy = std::clamp(y, 0, o.height - 1);
        if (depthBuffer[static_cast<size_t>(cy) * static_cast<size_t>(o.width) + static_cast<size_t>(cx)] < along - 2) return;
        paint.rect(x - 1, y - 14, x + 1, y, c);
        paint.text(x + 4, y - 26, t, c);
    };
    for (const region::Place& p : built.pads) name(p.at, 0.3, "PAD " + p.name, padColour);
    for (const region::Place& p : built.spawns) name(p.at, 0.3, p.name, spawnColour);

    char caption[256];
    if (o.eye)
        std::snprintf(caption, sizeof caption, "%s - eye at %.0f,%.0f looking %s, fov %.0f", file.id.c_str(),
                      built.regionAt(Vec2(cam.eye.x, cam.eye.z)).x, built.regionAt(Vec2(cam.eye.x, cam.eye.z)).y,
                      compass(o.azimuth).c_str(), fov);
    else
        std::snprintf(caption, sizeof caption, "%s - from %s, pitch %.0f, fov %.0f", file.id.c_str(), compass(o.azimuth).c_str(),
                      o.pitch.value_or(35), fov);
    paint.text(10, 10, caption, ink);
    return image;
}

} // namespace ac::regiontool
