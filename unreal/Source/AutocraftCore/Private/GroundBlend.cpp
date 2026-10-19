// Where ground palettes meet (GroundBlend.h). Every function here has its
// twin in M_Terrain's shader (Tools/Editor/make_terrain_material.py,
// `Lab::Gb*`): the same float operations in the same order, so the GPU's
// answer is the CPU's to a rounding.
#include "GroundBlend.h"

#include <cmath>

namespace ac::groundblend {

namespace {

uint32_t hash(int32_t x, int32_t y, uint32_t s) {
    uint32_t v = (uint32_t(x) * hashX) ^ (uint32_t(y) * hashY) ^ (s * hashS);
    v ^= v >> 16;
    v *= mixA;
    v ^= v >> 15;
    v *= mixB;
    v ^= v >> 16;
    return v;
}

float unit(uint32_t v) { return float(v >> 8) * (1.0f / 16777216.0f); }

float saturate(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/// Hermite between e0 and e1 (HLSL's smoothstep, spelt out: the shader uses
/// this one, not the intrinsic).
float smooth(float e0, float e1, float v) {
    const float t = saturate((v - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

uint32_t seed(std::string_view minus, std::string_view plus) {
    uint32_t h = 2166136261u;
    const auto eat = [&h](unsigned char c) {
        h ^= c;
        h *= 16777619u;
    };
    for (const char c : minus) eat(static_cast<unsigned char>(c));
    eat(0x1f);
    for (const char c : plus) eat(static_cast<unsigned char>(c));
    return (h ^ (h >> 24)) & 0xffffffu;
}

float value(float x, float y, uint32_t s) {
    const float fx = std::floor(x), fy = std::floor(y);
    const int32_t ix = int32_t(fx), iy = int32_t(fy);
    float tx = x - fx, ty = y - fy;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const float h00 = unit(hash(ix, iy, s)), h10 = unit(hash(ix + 1, iy, s));
    const float h01 = unit(hash(ix, iy + 1, s)), h11 = unit(hash(ix + 1, iy + 1, s));
    const float top = h00 + (h10 - h00) * tx;
    const float bottom = h01 + (h11 - h01) * tx;
    return top + (bottom - top) * ty;
}

float wander(float x, float y, uint32_t s) {
    // Each octave the last's lattice turned (0.8, 0.6), twice as fine, moved.
    const float px = x / wanderWave, py = y / wanderWave;
    const float qx = (0.8f * px - 0.6f * py) * 2.0f + 37.1f, qy = (0.6f * px + 0.8f * py) * 2.0f + 11.7f;
    const float rx = (0.8f * qx - 0.6f * qy) * 2.0f + 5.3f, ry = (0.6f * qx + 0.8f * qy) * 2.0f + 23.9f;
    const float n0 = value(px, py, s), n1 = value(qx, qy, s), n2 = value(rx, ry, s);
    return ((n0 - 0.5f) + (n1 - 0.5f) * 0.5f + (n2 - 0.5f) * 0.25f) * (2.0f / 1.75f);
}

float patch(float x, float y, uint32_t s) {
    const uint32_t ps = s ^ patchSalt;
    // Drift: the lattice warped by a coarser noise, then stretched along
    // the prevailing wind (0.8, 0.6), so the pockets and tongues lie in
    // streaks and curl rather than dot the band evenly.
    const float wx = value(x / warpWave, y / warpWave, ps + 1u) - 0.5f;
    const float wy = value(x / warpWave + 31.7f, y / warpWave + 17.3f, ps + 2u) - 0.5f;
    const float ux = x + wx * (2.0f * warpCells), uy = y + wy * (2.0f * warpCells);
    const float px = (0.8f * ux + 0.6f * uy) / (patchWave * patchStretch), py = (0.8f * uy - 0.6f * ux) / patchWave;
    const float qx = (0.8f * px - 0.6f * py) * 2.0f + 19.3f, qy = (0.6f * px + 0.8f * py) * 2.0f + 7.1f;
    const float raw = (value(px, py, ps) + value(qx, qy, ps) * 0.5f) * (1.0f / 1.5f);
    return saturate((raw - 0.5f) * patchContrast + 0.5f);
}

float plusOf(float d, float w, float m) {
    const float b = saturate((d + w * wanderCells) / bandCells + 0.5f);
    const float mm = patchSoft + (1.0f - 2.0f * patchSoft) * m;
    return smooth(mm - patchSoft, mm + patchSoft, b);
}

float line(float d, float x, float y, float t, float t0, uint32_t minus, uint32_t plus) {
    if (d <= -reach()) return 0.0f;
    if (d >= reach()) return 1.0f;
    const float k = smooth(-seamCells, seamCells, t - t0);
    float w = 0.0f, m = 0.0f;
    if (k < 1.0f) {
        w = wander(x, y, minus) * (1.0f - k);
        m = patch(x, y, minus) * (1.0f - k);
    }
    if (k > 0.0f) {
        w = w + wander(x, y, plus) * k;
        m = m + patch(x, y, plus) * k;
    }
    return plusOf(d, w, m);
}

Frame frame(float minX, float minY, float maxX, float maxY, const std::array<std::string_view, 9>& ids) {
    Frame f;
    f.minX = minX;
    f.minY = minY;
    f.maxX = maxX;
    f.maxY = maxY;
    const std::string_view o = ids[own], n = ids[north], e = ids[east], s = ids[south], w = ids[west];
    const std::string_view ne = ids[northEast], se = ids[southEast], sw = ids[southWest], nw = ids[northWest];
    f.has = {!n.empty(), !e.empty(), !s.empty(), !w.empty()};
    f.edge = {seed(n, o), seed(o, e), seed(o, s), seed(w, o)};
    f.beyondV = {seed(n, ne), seed(s, se), seed(sw, s), seed(nw, n)};
    f.beyondH = {seed(ne, e), seed(e, se), seed(w, sw), seed(nw, w)};
    return f;
}

std::array<float, 9> Field::weights() const {
    std::array<float, 9> w{};
    w[own] = ownWeight();
    w[side] += sideWeight();
    w[ahead] += aheadWeight();
    w[diagonal] += diagonalWeight();
    return w;
}

Field field(const Frame& f, float x, float y) {
    const float midX = (f.minX + f.maxX) * 0.5f, midY = (f.minY + f.maxY) * 0.5f;
    const bool e = x >= midX, s = y >= midY;
    const float x0 = e ? f.maxX : f.minX, y0 = s ? f.maxY : f.minY;
    // The vertical line at x0: its segments north and south of the corner at y0.
    const uint32_t vOwn = e ? f.edge[1] : f.edge[3];
    const uint32_t vBeyond = e ? (s ? f.beyondV[1] : f.beyondV[0]) : (s ? f.beyondV[2] : f.beyondV[3]);
    const float pv = line(x - x0, x, y, y, y0, s ? vOwn : vBeyond, s ? vBeyond : vOwn);
    // The horizontal line at y0: its segments west and east of the corner at x0.
    const uint32_t hOwn = s ? f.edge[2] : f.edge[0];
    const uint32_t hBeyond = s ? (e ? f.beyondH[1] : f.beyondH[2]) : (e ? f.beyondH[0] : f.beyondH[3]);
    const float ph = line(y - y0, x, y, x, x0, e ? hOwn : hBeyond, e ? hBeyond : hOwn);
    Field out;
    out.a = f.has[e ? 1 : 3] ? (e ? pv : 1.0f - pv) : 0.0f;
    out.b = f.has[s ? 2 : 0] ? (s ? ph : 1.0f - ph) : 0.0f;
    out.side = e ? east : west;
    out.ahead = s ? south : north;
    out.diagonal = e ? (s ? southEast : northEast) : (s ? southWest : northWest);
    return out;
}

int pick(const Frame& f, float x, float y, double u) {
    const Field fl = field(f, x, y);
    const double w[4] = {fl.ownWeight(), fl.sideWeight(), fl.aheadWeight(), fl.diagonalWeight()};
    const int slot[4] = {own, fl.side, fl.ahead, fl.diagonal};
    double sum = 0;
    for (int k = 0; k < 4; k++) {
        sum += w[k];
        if (u < sum) return slot[k];
    }
    for (int k = 3; k >= 0; k--)
        if (w[k] > 0) return slot[k];
    return own;
}

} // namespace ac::groundblend
