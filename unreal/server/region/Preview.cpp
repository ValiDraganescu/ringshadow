// The region preview (Preview.h).

#include "Preview.h"

#include "Font.h"
#include "NavGrid.h"
#include "TerrainField.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <thread>

int acWritePng(const char* path, int w, int h, const unsigned char* rgb); // StbImageWrite.cpp

namespace ac::regiontool {

namespace {

using region::Built;
using region::Edge;
using region::Problem;
using region::RegionFile;

struct Rgb { double r, g, b; };

Rgb mix(Rgb a, Rgb b, double t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
Rgb scaled(Rgb a, double k) { return {a.r * k, a.g * k, a.b * k}; }

// The palette.
constexpr Rgb background{24, 26, 30};
constexpr Rgb ink{225, 228, 232};
constexpr Rgb dim{140, 146, 156};
constexpr Rgb levelColour[3] = {{104, 112, 78}, {168, 150, 112}, {214, 204, 178}};
constexpr Rgb cliffColour{122, 50, 38};
constexpr Rgb rockColour{58, 58, 62};
constexpr Rgb plantColour{70, 150, 70};
constexpr Rgb oreColour{176, 110, 236};
constexpr Rgb wellColour{220, 232, 250};
constexpr Rgb padColour{250, 206, 60};
constexpr Rgb spawnColour{80, 235, 130};
constexpr Rgb openingColour{60, 220, 240};
constexpr Rgb problemColour{255, 64, 64};

constexpr int glyphScale = 2, advance = 6 * glyphScale, lineHeight = 9 * glyphScale;

/// UTF-8 to the font's ASCII: "…" as "...", "×" as "x", "·" and "–" as "-".
std::string ascii(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) { out += static_cast<char>(c); continue; }
        if (s.compare(i, 3, "\xE2\x80\xA6") == 0) { out += "..."; i += 2; continue; }
        if (s.compare(i, 3, "\xE2\x80\x93") == 0 || s.compare(i, 3, "\xE2\x80\x94") == 0) { out += "-"; i += 2; continue; }
        if (s.compare(i, 2, "\xC3\x97") == 0) { out += "x"; i += 1; continue; }
        if (s.compare(i, 2, "\xC2\xB7") == 0) { out += "-"; i += 1; continue; }
        // Any other character: one '?', past its continuation bytes.
        out += '?';
        while (i + 1 < s.size() && (static_cast<unsigned char>(s[i + 1]) & 0xC0) == 0x80) i++;
    }
    return out;
}

struct Canvas {
    Image& image;

    void blend(int x, int y, Rgb c, double a = 1) {
        if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
        uint8_t* p = &image.rgb[(static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x)) * 3];
        const double v[3] = {c.r, c.g, c.b};
        for (int k = 0; k < 3; k++)
            p[k] = static_cast<uint8_t>(std::clamp(std::lround(p[k] + (v[k] - p[k]) * a), 0L, 255L));
    }

    void rect(int x0, int y0, int x1, int y1, Rgb c, double a = 1) {
        for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) blend(x, y, c, a);
    }

    int text(int x, int y, const std::string& s, Rgb c, bool box = false) {
        const std::string t = ascii(s);
        const int w = static_cast<int>(t.size()) * advance;
        if (box) rect(x - 3, y - 3, x + w + 1, y + 7 * glyphScale + 3, background, 0.78);
        for (size_t i = 0; i < t.size(); i++) {
            const Glyph& g = glyph(t[i]);
            for (int row = 0; row < 7; row++)
                for (int col = 0; col < 5; col++)
                    if (g.rows[static_cast<size_t>(row)][static_cast<size_t>(col)] == '#')
                        rect(x + static_cast<int>(i) * advance + col * glyphScale, y + row * glyphScale,
                             x + static_cast<int>(i) * advance + (col + 1) * glyphScale, y + (row + 1) * glyphScale, c);
        }
        return w;
    }
};

} // namespace

Image preview(const RegionFile& file, const Built& built, const std::vector<Problem>& problems, int scale) {
    const MapDefinition& map = built.map;
    const TerrainField field(map);
    const Vec2 extent = built.extent();
    const int s = scale > 0 ? scale : std::clamp(static_cast<int>(1400 / extent.x), 3, 12);
    const int mw = static_cast<int>(extent.x) * s, md = static_cast<int>(extent.y) * s;
    const int left = 52, right = 52, top = 2 * lineHeight + 44;
    // The problems' lines, wrapped to the image's width.
    const int width = left + mw + right;
    const size_t perLine = static_cast<size_t>((width - 24) / advance);
    std::vector<std::pair<std::string, Rgb>> notes;
    if (problems.empty()) notes.push_back({"No problems.", spawnColour});
    for (size_t i = 0; i < problems.size(); i++) {
        std::string t = ascii(std::to_string(i + 1) + ". " + problems[i].text());
        while (!t.empty()) {
            size_t cut = std::min(t.size(), perLine);
            if (cut < t.size()) {
                const size_t space = t.rfind(' ', cut);
                if (space != std::string::npos && space > perLine / 2) cut = space;
            }
            notes.push_back({t.substr(0, cut), problemColour});
            t = t.substr(cut);
            while (!t.empty() && t[0] == ' ') t.erase(0, 1);
            if (!t.empty()) t = "    " + t;
        }
    }
    const int bottom = 30 + 2 * lineHeight + 12 + static_cast<int>(notes.size()) * lineHeight + 12;
    Image image;
    image.width = width;
    image.height = top + md + bottom;
    image.rgb.assign(static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * 3, 0);
    Canvas c{image};
    c.rect(0, 0, image.width, image.height, background);

    // Region position under a pixel of the map, and the pixel of a position.
    const auto at = [&](int px, int py) { return Vec2((px + 0.5) / s, (py + 0.5) / s); };
    const auto pixel = [&](Vec2 r) { return std::pair<int, int>{left + static_cast<int>(std::floor(r.x * s)), top + static_cast<int>(std::floor(r.y * s))}; };

    // The ground: one height a pixel, in rows on every core.
    std::vector<double> heights(static_cast<size_t>(mw + 2) * static_cast<size_t>(md + 2));
    const auto hAt = [&](int px, int py) -> double& {
        return heights[static_cast<size_t>(py + 1) * static_cast<size_t>(mw + 2) + static_cast<size_t>(px + 1)];
    };
    {
        const int threads = static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; t++) {
            pool.emplace_back([&, t] {
                for (int py = -1 + t; py <= md; py += threads)
                    for (int px = -1; px <= mw; px++) hAt(px, py) = field.height(built.ground(at(px, py)));
            });
        }
        for (std::thread& th : pool) th.join();
    }
    const double H = map.levelHeight;
    for (int py = 0; py < md; py++) {
        for (int px = 0; px < mw; px++) {
            const double h = hAt(px, py);
            const double dx = (hAt(px + 1, py) - hAt(px - 1, py)) * s / 2, dz = (hAt(px, py + 1) - hAt(px, py - 1)) * s / 2;
            const double band = std::clamp(h / H, 0.0, 2.0);
            const int level = static_cast<int>(std::clamp(std::lround(band), 0L, 2L));
            Rgb col = levelColour[level];
            // Within a level, a little lighter higher up.
            col = scaled(col, 1 + std::clamp(h / H - level, -0.5, 0.5) * 0.25);
            // Light from the north-west.
            const double nx = -dx, nz = -dz, ny = 1, len = std::sqrt(nx * nx + nz * nz + ny * ny);
            const double light = (nx * -0.55 + nz * -0.55 + ny * 0.63) / len;
            col = scaled(col, std::clamp(0.55 + 0.6 * light, 0.35, 1.25));
            if (std::sqrt(dx * dx + dz * dz) > NavGrid::maxSlope) col = mix(col, cliffColour, 0.8);
            c.blend(left + px, top + py, col);
        }
    }

    // Openings: the lane inside and a bar outside the edge.
    for (size_t e = 0; e < 4; e++) {
        const Edge edge = static_cast<Edge>(e);
        for (const region::Span& span : built.openings[e]) {
            const auto a = pixel(built.edgePoint(edge, span.from, 0)), b = pixel(built.edgePoint(edge, span.to, region::laneDepth));
            c.rect(std::min(a.first, b.first), std::min(a.second, b.second), std::max(a.first, b.first) + 1,
                   std::max(a.second, b.second) + 1, openingColour, 0.35);
            const auto o = pixel(built.edgePoint(edge, span.from, 0)), p = pixel(built.edgePoint(edge, span.to, 0));
            const int bar = 7;
            switch (edge) {
            case Edge::north: c.rect(o.first, top - bar - 1, p.first, top - 1, openingColour); break;
            case Edge::south: c.rect(o.first, top + md + 1, p.first, top + md + bar + 1, openingColour); break;
            case Edge::west: c.rect(left - bar - 1, o.second, left - 1, p.second, openingColour); break;
            case Edge::east: c.rect(left + mw + 1, o.second, left + mw + bar + 1, p.second, openingColour); break;
            }
        }
    }

    // Shapes in region cells.
    const auto disc = [&](Vec2 centre, double radius, Rgb col, double alpha) {
        const auto [cx, cy] = pixel(centre);
        const int r = static_cast<int>(std::ceil(radius * s)) + 1;
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++) {
                const Vec2 q = at(x - left, y - top);
                if (distance(q, centre) <= radius) c.blend(x, y, col, alpha);
            }
    };
    const auto ring = [&](Vec2 centre, double radius, double thick, Rgb col) {
        const auto [cx, cy] = pixel(centre);
        const int r = static_cast<int>(std::ceil((radius + thick) * s)) + 1;
        for (int y = cy - r; y <= cy + r; y++)
            for (int x = cx - r; x <= cx + r; x++) {
                const double d = distance(at(x - left, y - top), centre);
                if (std::fabs(d - radius) <= thick / 2) c.blend(x, y, col);
            }
    };
    const auto label = [&](Vec2 r, const std::string& t, Rgb col) {
        const auto [x, y] = pixel(r);
        c.text(x + 10, y - 7, t, col, true);
    };

    // Doodads: rock where they block, a dot where they are only plants.
    for (const Doodad& d : map.doodads) {
        const auto circles = NavGrid::circles(d);
        if (circles.empty()) { disc(built.regionAt(d.position), 0.25, plantColour, 1); continue; }
        for (const auto& [centre, radius] : circles) disc(built.regionAt(centre), radius, rockColour, 0.92);
    }
    // Ore deposits: 2 by 1, turned as `NavGrid::insidePatch` turns them.
    std::vector<OreSpec> ore = map.deposits;
    for (const BaseSite& b : map.bases) ore.insert(ore.end(), b.ore.begin(), b.ore.end());
    for (const OreSpec& o : ore) {
        const Vec2 r = built.regionAt(o.position);
        const double cs = std::cos(o.angle), sn = std::sin(o.angle);
        const auto [cx, cy] = pixel(r);
        const int span = 2 * s;
        for (int y = cy - span; y <= cy + span; y++)
            for (int x = cx - span; x <= cx + span; x++) {
                const Vec2 d = at(x - left, y - top) - r;
                const double lx = d.x * cs - d.y * sn, lz = d.x * sn + d.y * cs;
                if (std::fabs(lx) <= 1 && std::fabs(lz) <= 0.5)
                    c.blend(x, y, std::fabs(lx) > 0.85 || std::fabs(lz) > 0.36 ? scaled(oreColour, 0.55) : oreColour);
            }
    }
    // Wells: the Derrick's ground.
    std::vector<Vec2> wells;
    for (const WellSpec& w : map.wells) wells.push_back(w.position);
    for (const BaseSite& b : map.bases) wells.insert(wells.end(), b.wells.begin(), b.wells.end());
    for (Vec2 w : wells) {
        disc(built.regionAt(w), 1.5, wellColour, 0.9);
        ring(built.regionAt(w), 1.5, 0.25, scaled(wellColour, 0.4));
        disc(built.regionAt(w), 0.45, scaled(wellColour, 0.35), 1);
    }
    // Pads and spawn points, named.
    for (const region::Place& p : built.pads) {
        const Vec2 r = built.regionAt(p.at);
        ring(r, region::padRadius, 0.35, padColour);
        ring(r, region::padRadius * 0.45, 0.25, padColour);
        label(r + Vec2(region::padRadius, 0), "PAD " + p.name, padColour);
    }
    for (const region::Place& p : built.spawns) {
        const Vec2 r = built.regionAt(p.at);
        const auto [cx, cy] = pixel(r);
        const int k = std::max(6, static_cast<int>(1.2 * s));
        for (int y = -k; y <= k; y++)
            for (int x = -k; x <= k; x++)
                if (std::abs(x) + std::abs(y) <= k) c.blend(cx + x, cy + y, std::abs(x) + std::abs(y) > k - 2 ? background : spawnColour);
        label(r + Vec2(1.2, 0), p.name, spawnColour);
    }

    // The grid, labelled every 8 cells (16 when cells are small).
    const int step = s >= 6 ? 8 : 16;
    for (int x = 0; x <= static_cast<int>(extent.x); x += step) {
        const int px = left + x * s;
        c.rect(px, top, px + 1, top + md, background, x % (2 * step) == 0 ? 0.45 : 0.25);
        const std::string t = std::to_string(x);
        const int w = static_cast<int>(t.size()) * advance;
        c.text(px - w / 2, top - 7 * glyphScale - 12, t, dim);
        c.text(px - w / 2, top + md + 12, t, dim);
    }
    for (int z = 0; z <= static_cast<int>(extent.y); z += step) {
        const int py = top + z * s;
        c.rect(left, py, left + mw, py + 1, background, z % (2 * step) == 0 ? 0.45 : 0.25);
        const std::string t = std::to_string(z);
        const int w = static_cast<int>(t.size()) * advance;
        c.text(left - w - 12, py - 7, t, dim);
        c.text(left + mw + 12, py - 7, t, dim);
    }

    // The problems where they are.
    for (size_t i = 0; i < problems.size(); i++) {
        if (!problems[i].at) continue;
        const Vec2 r = *problems[i].at;
        ring(r, std::max(1.2, 9.0 / s), std::max(0.3, 2.5 / s), problemColour);
        const auto [x, y] = pixel(r);
        c.text(x + 12, y + 8, std::to_string(i + 1), problemColour, true);
    }

    // Title, legend, problems.
    char size[64];
    std::snprintf(size, sizeof size, "%dx%d", static_cast<int>(extent.x), static_cast<int>(extent.y));
    c.text(left, 8, file.id + " - " + file.name, ink);
    c.text(left, 8 + lineHeight, std::string(rawValue(file.size)) + " " + size + " cells, " + std::string(rawValue(file.style)) +
                                     (file.seed ? " seed " + std::to_string(*file.seed) : " (the game's map)") +
                                     ". North up; x east, z south.", dim);
    int y = top + md + 30 + 7 * glyphScale;
    int x = left;
    const auto swatch = [&](Rgb col, const std::string& t) {
        if (x + 22 + static_cast<int>(t.size() + 2) * advance > image.width - 12) { x = left; y += lineHeight; }
        c.rect(x, y, x + 14, y + 14, col);
        x += 20;
        x += c.text(x, y + 1, t, ink) + 2 * advance;
    };
    swatch(levelColour[0], "level 0");
    swatch(levelColour[1], "level 1");
    swatch(levelColour[2], "level 2");
    swatch(cliffColour, "cliff");
    swatch(rockColour, "rock");
    swatch(oreColour, "ore");
    swatch(wellColour, "MH well");
    swatch(padColour, "pad");
    swatch(spawnColour, "spawn");
    swatch(openingColour, "opening");
    swatch(problemColour, "problem");
    y += lineHeight + 12;
    for (const auto& [t, col] : notes) {
        c.text(left, y, t, col);
        y += lineHeight;
    }
    return image;
}

bool writePng(const Image& image, const std::string& path) {
    return acWritePng(path.c_str(), image.width, image.height, image.rgb.data()) != 0;
}

} // namespace ac::regiontool
