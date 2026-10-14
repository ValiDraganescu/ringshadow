// Starfall (Starfall.h).

#include "Starfall.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {

constexpr double kPi = 3.14159265358979323846;

/// 0 … 1 from 53 bits of `x`.
double unit(uint64_t x) { return double(x >> 11) * (1.0 / 9007199254740992.0); }

} // namespace

uint64_t Starfall::mix(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

std::vector<double> Starfall::impacts(uint64_t seed, int64_t day, double dayLength) {
    uint64_t dice = mix(seed ^ mix(uint64_t(day) * 0x51a2f411ULL + 0x5fa11ULL));
    const int64_t count = fewest + int64_t(dice % uint64_t(most - fewest + 1));
    const double share = dayLength / double(count);
    std::vector<double> out;
    for (int64_t k = 0; k < count; k++) {
        dice = mix(dice);
        out.push_back(double(day) * dayLength + share * (double(k) + 0.15 + 0.7 * unit(dice)));
    }
    return out;
}

double Starfall::next(uint64_t seed, double time, double dayLength) {
    for (int64_t day = int64_t(std::floor(time / dayLength));; day++)
        for (const double t : impacts(seed, day, dayLength))
            if (t > time) return t;
}

double Starfall::bearingFrom(uint64_t dice) { return giantBearing + spread * (2 * unit(mix(dice)) - 1); }

Vec2 Starfall::toward(double bearing) {
    const double b = bearing * kPi / 180.0;
    return Vec2(std::sin(b), -std::cos(b));
}

double Starfall::bearing(Vec2 from, Vec2 to) {
    const Vec2 d = to - from;
    const double b = std::atan2(d.x, -d.y) * 180.0 / kPi;
    return b < 0 ? b + 360 : b;
}

Vec3 Starfall::head(double before, double bearing) {
    const double along = speed * std::max(0.0, before);
    const Vec2 back = toward(bearing) * (along * std::cos(descent * kPi / 180.0));
    return Vec3(back.x, along * std::sin(descent * kPi / 180.0), back.y);
}

} // namespace ac
