// The ringshadow sweep (Ringshadow.h).

#include "Ringshadow.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {

double smooth(double e0, double e1, double x) {
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

} // namespace

Vec2 Ringshadow::direction() {
    const double b = bearing * 3.14159265358979323846 / 180.0;
    return Vec2(std::sin(b), -std::cos(b));
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
    for (const double h : hours) {
        const double first = WorldClock::timeOf(h, 0, day);
        const double k = std::max(0.0, std::ceil((time - duration - first) / day));
        double start = first + k * day;
        if (start + duration <= time) start += day;
        if (start < best.start) best = Sweep{start, start + duration};
    }
    return best;
}

std::optional<Ringshadow::Band> Ringshadow::band(double time, const GroundRect& bounds, double day) {
    const auto s = at(time, day);
    if (!s) return std::nullopt;
    const Vec2 dir = direction();
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
