// The ringshadow sweep (Ringshadow.h).

#include "Ringshadow.h"

#include "Rules.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {

Vec3 cross(Vec3 a, Vec3 b) { return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }

double smooth(double e0, double e1, double x) {
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

} // namespace

Vec3 Ringshadow::giant() {
    const double b = giantBearing * pi / 180.0, e = giantElevation * pi / 180.0;
    return Vec3(std::sin(b) * std::cos(e), std::sin(e), -std::cos(b) * std::cos(e));
}

Vec3 Ringshadow::sun(int index) { return Rules::sunToward(hours[std::clamp(index, 0, 1)]); }

Vec2 Ringshadow::direction(int index) { return direction(sun(index), giant()); }

Vec2 Ringshadow::direction(Vec3 s, Vec3 g) {
    const Vec3 up(0, 1, 0);
    // The ring's great circle at the sun: its tangent toward the giant.
    const Vec3 t = normalize(g - s * dot(g, s));
    // The plane of the arc's edge where it crosses the sun, and its line on
    // the ground.
    const Vec3 n = cross(s, t);
    const Vec3 e = normalize(cross(n, up));
    // Across that line on the ground: up × e is n's level part. The sign:
    // the sky (make_sky_material.py, RING_HLSL) offsets the arc `RingOff`
    // along nrm = cross(G, S) in Unreal axes, which is n here (Unreal swaps
    // the sim's y and z, which turns every cross product round, and
    // s × t = s × g / |...|). AAcRingshadow sets RingOff from the camera's
    // place across the band: -RingHalf at the leading edge, +RingHalf at the
    // trailing one, so the eye further along the way the band moves sees
    // the arc further toward -n. Parallax: a far thing seen from a point
    // moved by d on the ground shifts in the sky by -d, so the way the band
    // moves has a positive share of n. As the band runs on, every eye's
    // RingOff grows: the arc slides toward +n, and its shadow with it.
    const Vec3 a = cross(up, e);
    return normalize(Vec2(a.x, a.z));
}

double Ringshadow::bearing(int index) {
    const Vec2 d = direction(index);
    const double b = std::atan2(d.x, -d.y) * 180.0 / pi;
    return b < 0 ? b + 360.0 : b;
}

std::optional<Ringshadow::Sweep> Ringshadow::at(double time, double day) {
    const Sweep s = next(time, day);
    if (s.start <= time && time < s.end) return s;
    return std::nullopt;
}

Ringshadow::Sweep Ringshadow::next(double time, double day) {
    // A sweep's start is its hour's first time in the world plus whole days:
    // the same number from every `time` (no rounding drift frame to frame).
    Sweep best{INFINITY, INFINITY};
    for (int i = 0; i < 2; ++i) {
        const double first = WorldClock::timeOf(hours[i], 0, day);
        const double k = std::max(0.0, std::ceil((time - duration - first) / day));
        double start = first + k * day;
        if (start + duration <= time) start += day;
        if (start < best.start) best = Sweep{start, start + duration, i};
    }
    return best;
}

std::optional<Ringshadow::Band> Ringshadow::band(double time, const GroundRect& bounds, double day) {
    const auto s = at(time, day);
    if (!s) return std::nullopt;
    const Vec2 dir = direction(s->index);
    double lo = INFINITY, hi = -INFINITY;
    for (const Vec2 c : {Vec2(bounds.minX, bounds.minZ), Vec2(bounds.maxX, bounds.minZ), Vec2(bounds.minX, bounds.maxZ),
                         Vec2(bounds.maxX, bounds.maxZ)}) {
        lo = std::min(lo, dot(c, dir));
        hi = std::max(hi, dot(c, dir));
    }
    const double f = (time - s->start) / duration;
    Band b;
    b.dir = dir;
    // From wholly short of the region (its trailing edge `margin + width`
    // back) to wholly past it.
    b.lead = lo - margin + f * (hi - lo + width + 2 * margin);
    b.trail = b.lead - width;
    return b;
}

double Ringshadow::cover(const Band& band, Vec2 p) {
    const double s = dot(p, band.dir);
    return smooth(band.trail, band.trail + band.soft, s) * (1.0 - smooth(band.lead - band.soft, band.lead, s));
}

double Ringshadow::cover(double time, Vec2 p, const GroundRect& bounds, double day) {
    const auto b = band(time, bounds, day);
    return b ? cover(*b, p) : 0.0;
}

int64_t Ringshadow::load(int64_t carried, double time, Vec2 p, const GroundRect& bounds, double day) {
    if (carried <= 0 || cover(time, p, bounds, day) < bonusCover) return carried;
    return int64_t(std::llround(double(carried) * oreBonus));
}

} // namespace ac
