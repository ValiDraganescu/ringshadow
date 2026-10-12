#include "Weather.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {
    double clamp01(double x) { return std::min(std::max(x, 0.0), 1.0); }
    double smoothstep(double a, double b, double x) {
        const double t = clamp01((x - a) / (b - a));
        return t * t * (3 - 2 * t);
    }
    double lerp(double a, double b, double t) { return a + (b - a) * t; }

    uint64_t splitmix(uint64_t x) {
        x += 0x9e3779b97f4a7c15ull;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
        return x ^ (x >> 31);
    }
    /// A uniform number in [0, 1) from a hash.
    double unitOf(uint64_t h) { return double(h >> 11) * (1.0 / 9007199254740992.0); }

    constexpr double CheckpointSeconds = 60;
    /// Strikes per second at full storminess (about one every 7 s).
    constexpr double StrikesPerSecond = 0.15;
    /// Strikes come from 0.6 km to 9 km away.
    constexpr double NearKm = 0.6, FarKm = 9;
}

Weather::Weather(uint64_t seed) : seed_(seed), spellRng_(splitmix(seed ^ 0x5745415448455221ull)) {
    checkpoints_.push_back(last_);
}

void Weather::hold(double state) { held_ = state < 0 ? -1 : std::min(state, 3.0); }

double Weather::coverOf(WeatherState s) { return targetsOf(s).cover; }
double Weather::rainOf(WeatherState s) { return targetsOf(s).rain; }
double Weather::gloomOf(WeatherState s) { return targetsOf(s).gloom; }
double Weather::stormOf(WeatherState s) { return targetsOf(s).storm; }
double Weather::windOf(WeatherState s) { return targetsOf(s).wind; }

Weather::Targets Weather::targetsOf(WeatherState s) {
    switch (s) {
    // Wind: 15 m/s clear moves a medium map's cloud shadows past about
    // five times in 20 min (AcDaylight.cpp, WeatherKm).
    case WeatherState::clear: return {0.35, 0.0, 0.0, 0.0, 15};
    case WeatherState::cloudy: return {0.70, 0.0, 0.15, 0.1, 17};
    case WeatherState::rain: return {0.96, 0.5, 0.6, 0.4, 20};
    case WeatherState::storm: return {0.99, 1.0, 0.9, 1.0, 26};
    }
    return {0.35, 0.0, 0.0, 0.0, 15};
}

void Weather::extendTo(double time) {
    while (spells_.empty() || spells_.back().end <= time) {
        auto unit = [this] { spellRng_ = splitmix(spellRng_); return unitOf(spellRng_); };
        WeatherSpell s;
        if (spells_.empty()) {
            s.state = WeatherState::clear;
            s.start = 0;
            s.end = FirstClearSeconds + unit() * 180;
        } else {
            const WeatherState prev = spells_.back().state;
            const double u = unit();
            switch (prev) {
            case WeatherState::clear: s.state = WeatherState::cloudy; break;
            case WeatherState::cloudy: s.state = u < 0.5 ? WeatherState::clear : WeatherState::rain; break;
            case WeatherState::rain: s.state = u < 0.55 ? WeatherState::cloudy : WeatherState::storm; break;
            case WeatherState::storm: s.state = WeatherState::rain; break;
            }
            double lo = 240, hi = 480;
            if (s.state == WeatherState::cloudy) { lo = 150; hi = 300; }
            if (s.state == WeatherState::rain) { lo = 120; hi = 240; }
            if (s.state == WeatherState::storm) { lo = 90; hi = 180; }
            s.start = spells_.back().end;
            s.end = s.start + lo + unit() * (hi - lo);
        }
        spells_.push_back(s);
    }
}

WeatherSpell Weather::spellAt(double time) {
    time = std::max(time, 0.0);
    extendTo(time);
    const auto it = std::upper_bound(spells_.begin(), spells_.end(), time,
                                     [](double t, const WeatherSpell& s) { return t < s.end; });
    return it == spells_.end() ? spells_.back() : *it;
}

Weather::Targets Weather::eased(double time) {
    if (held_ >= 0) {
        const int a = std::min(int(std::floor(held_)), 3), b = std::min(a + 1, 3);
        const Targets ta = targetsOf(WeatherState(a)), tb = targetsOf(WeatherState(b));
        const double f = held_ - a;
        return {lerp(ta.cover, tb.cover, f), lerp(ta.rain, tb.rain, f), lerp(ta.gloom, tb.gloom, f),
                lerp(ta.storm, tb.storm, f), lerp(ta.wind, tb.wind, f)};
    }
    time = std::max(time, 0.0);
    const WeatherSpell now = spellAt(time);
    const Targets to = targetsOf(now.state);
    if (now.start <= 0) return to;
    const Targets from = targetsOf(spellAt(now.start - 1e-6).state);
    const double x = (time - now.start) / ChangeSeconds;
    // Wetter: the cloud builds first, the rain follows. Drier: the rain
    // stops first, the cloud breaks after.
    const bool wetter = to.rain > from.rain || to.cover > from.cover;
    const double lead = smoothstep(0, 0.6, x), lag = smoothstep(0.4, 1, x), all = smoothstep(0, 1, x);
    const double sky = wetter ? lead : lag, rain = wetter ? lag : lead;
    return {lerp(from.cover, to.cover, sky), lerp(from.rain, to.rain, rain), lerp(from.gloom, to.gloom, sky),
            lerp(from.storm, to.storm, rain), lerp(from.wind, to.wind, all)};
}

void Weather::integrate(Checkpoint& c, double time) {
    while (c.time < time) {
        const double dt = std::min(1.0, time - c.time);
        const Targets t = eased(c.time + dt * 0.5);
        c.windKm += t.wind * dt * 0.001;
        // Wet over ~30 s of rain (sooner in a downpour); dry over 1.5-3 min
        // after it, slower under heavy cloud.
        // (A drizzle blends the two, so the ground never jumps.)
        const double raining = smoothstep(0, 0.05, t.rain);
        const double wet = (1 - c.wetness) * (1 - std::exp(-dt * (0.5 + t.rain) / 30));
        const double dry = c.wetness * (1 - std::exp(-dt / lerp(90, 180, t.gloom)));
        c.wetness = clamp01(c.wetness + raining * wet - (1 - raining) * dry);
        c.time += dt;
    }
}

Weather::Checkpoint Weather::integratedAt(double time) {
    time = std::max(time, 0.0);
    // The kept state is always on a whole second, stepped 1 s at a time
    // from 0, so the result never depends on the order of the questions;
    // the part of a second past it goes on a copy.
    const double whole = std::floor(time);
    if (whole < last_.time) {
        const size_t k = std::min(checkpoints_.size() - 1, size_t(whole / CheckpointSeconds));
        last_ = checkpoints_[k];
    }
    while (last_.time < whole) {
        integrate(last_, last_.time + 1);
        if (std::fmod(last_.time, CheckpointSeconds) == 0 && size_t(last_.time / CheckpointSeconds) == checkpoints_.size()) {
            checkpoints_.push_back(last_);
        }
    }
    Checkpoint c = last_;
    integrate(c, time);
    return c;
}

void Weather::strikesIn(int64_t slot0, int64_t slot1, std::vector<LightningStrike>& out) {
    for (int64_t k = std::max<int64_t>(slot0, 0); k < slot1; ++k) {
        const uint64_t h = splitmix(seed_ ^ splitmix(uint64_t(k) * 0x9e3779b97f4a7c15ull + 0x4c49474854ull));
        const double t = double(k) + unitOf(splitmix(h ^ 1));
        // Lightning only in the storm's heaviest part.
        const double rate = StrikesPerSecond * smoothstep(0.55, 0.9, eased(t).storm);
        if (rate <= 0 || unitOf(h) >= rate) continue;
        LightningStrike s;
        s.time = t;
        // More far strikes than near ones (uniform over an annulus).
        const double u = unitOf(splitmix(h ^ 2));
        s.distanceKm = std::sqrt(NearKm * NearKm + u * (FarKm * FarKm - NearKm * NearKm));
        s.azimuth = unitOf(splitmix(h ^ 3)) * 6.283185307179586;
        s.power = 0.5 + 0.5 * unitOf(splitmix(h ^ 4));
        out.push_back(s);
    }
}

std::vector<LightningStrike> Weather::strikes(double t0, double t1) {
    std::vector<LightningStrike> all, out;
    strikesIn(int64_t(std::floor(t0)), int64_t(std::ceil(t1)), all);
    for (const auto& s : all) if (s.time >= t0 && s.time < t1) out.push_back(s);
    return out;
}

std::vector<LightningStrike> Weather::thunder(double t0, double t1) {
    std::vector<LightningStrike> all, out;
    const double maxDelay = FarKm / 0.343;
    strikesIn(int64_t(std::floor(t0 - maxDelay)), int64_t(std::ceil(t1)), all);
    for (const auto& s : all) if (s.thunderTime() >= t0 && s.thunderTime() < t1) out.push_back(s);
    return out;
}

double Weather::flicker(double a) {
    if (a < 0 || a > 1) return 0;
    return std::exp(-a / 0.05) + 0.8 * std::exp(-std::max(0.0, a - 0.13) / 0.07) * (a >= 0.13 ? 1 : 0)
        + 0.25 * std::exp(-a / 0.4);
}

double Weather::flashAt(double time) {
    std::vector<LightningStrike> recent;
    strikesIn(int64_t(std::floor(time - 1)), int64_t(std::floor(time)) + 1, recent);
    double flash = 0;
    for (const auto& s : recent) {
        const double env = flicker(time - s.time);
        if (env <= 0) continue;
        const double near = lerp(1.0, 0.3, clamp01((s.distanceKm - NearKm) / (FarKm - NearKm)));
        flash += s.power * near * env;
    }
    return std::min(flash, 1.5);
}

WeatherSample Weather::sample(double time) {
    time = std::max(time, 0.0);
    const Targets t = eased(time);
    WeatherSample s;
    s.cover = t.cover;
    s.rain = t.rain;
    s.gloom = t.gloom;
    s.storm = t.storm;
    s.wind = t.wind;
    const Checkpoint c = integratedAt(time);
    s.windKm = c.windKm;
    s.wetness = held_ >= 0 ? clamp01(t.rain * 2.5) : c.wetness;
    s.flash = flashAt(time);
    if (held_ >= 0) {
        s.state = WeatherState(std::min(int(std::lround(held_)), 3));
        s.change = 1;
    } else {
        const WeatherSpell spell = spellAt(time);
        s.state = spell.state;
        s.change = clamp01((time - spell.start) / ChangeSeconds);
    }
    return s;
}

} // namespace ac
