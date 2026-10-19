#include "Wipers.h"

#include <algorithm>
#include <cmath>

namespace ac {

namespace {
    constexpr double Pi = 3.14159265358979323846;
}

WiperMode Wipers::modeFor(const double rate) {
    if (rate >= FastFrom) return WiperMode::fast;
    if (rate >= SteadyFrom) return WiperMode::steady;
    if (rate >= IntermittentFrom) return WiperMode::intermittent;
    return WiperMode::off;
}

double Wipers::cadenceOf(const WiperMode mode) {
    switch (mode) {
    case WiperMode::intermittent: return IntermittentEvery;
    case WiperMode::steady: return SteadyEvery;
    case WiperMode::fast: return 2 * FastEach;
    case WiperMode::off: break;
    }
    return 0;
}

double Wipers::eachOf(const WiperMode mode) {
    return mode == WiperMode::fast ? FastEach : SlowEach;
}

double Wipers::sweepAngle(const double t, const double each) {
    if (!(each > 0) || t <= 0 || t >= 2 * each) return ParkAngle;
    const double p = t <= each ? t / each : 2 - t / each;
    const double reach = (1 - std::cos(Pi * p)) / 2;
    return ParkAngle + (FarAngle - ParkAngle) * reach;
}

double Wipers::reachTime(const double a) {
    const double r = std::clamp((a - ParkAngle) / (FarAngle - ParkAngle), 0.0, 1.0);
    return std::acos(1 - 2 * r) / Pi;
}

double Wipers::landing(const double rate) {
    return 1 + HeavyLands * rate * rate;
}

double Wipers::coverOf(const double water) {
    return 1 - std::exp(-std::max(water, 0.0) / CoverWater);
}

void Wipers::update(double now, double rate) {
    rate = std::clamp(rate, 0.0, 1.0);
    if (!started_) {
        started_ = true;
        now_ = sliceStart_ = now;
    }
    now = std::max(now, now_);
    const double before = now_;

    // The water: each slice of time's landing, dried as it ages.
    double t = now_;
    if (now - t > Slices * SliceSeconds) {
        // A long gap: what the slices held is older than they reach now.
        all_ *= std::exp(-(now - t - Slices * SliceSeconds) / DrySeconds);
        slices_.fill(0);
        t = now - Slices * SliceSeconds;
        sliceStart_ = t;
    }
    const double lands = rate * landing(rate);
    while (t < now) {
        const double sliceEnd = sliceStart_ + SliceSeconds;
        const double step = std::min(now, sliceEnd) - t;
        const double dry = std::exp(-step / DrySeconds);
        for (double& s : slices_) s *= dry;
        all_ = all_ * dry + lands * step;
        slices_[0] += lands * step;
        t += step;
        if (t >= sliceEnd) {
            std::rotate(slices_.rbegin(), slices_.rbegin() + 1, slices_.rend());
            slices_[0] = 0;
            sliceStart_ = sliceEnd;
        }
    }
    now_ = now;
    rate_ = rate;
    for (WiperSweep& s : sweeps_) {
        if (s.number >= 0 && s.allAtEnd < 0 && now_ >= s.end()) {
            // Ended within this step: what landed since its end is a share of the step's.
            s.allAtEnd = all_ - lands * (now_ - s.end());
        }
    }

    // The mode, and the last sweeps when it turns off.
    const WiperMode want = enabled_ ? modeFor(rate) : WiperMode::off;
    if (want != WiperMode::off) finals_ = 0;
    else if (mode_ != WiperMode::off) finals_ = FinalSweeps;
    mode_ = want;

    if (!enabled_) finals_ = 0;
    if (enabled_ && !sweeping()) {
        double next = std::numeric_limits<double>::infinity();
        const WiperSweep& last = sweeps_[0];
        if (finals_ > 0) {
            next = last.number < 0 ? now : last.end();
        } else if (mode_ != WiperMode::off) {
            next = last.number < 0 ? now : std::max(last.start + cadenceOf(mode_), last.end());
        }
        if (coverage() >= SweepEarlyAt) next = std::min(next, now);
        if (next <= now) {
            const double each = finals_ > 0 || mode_ == WiperMode::off ? SlowEach : eachOf(mode_);
            const double at = std::clamp(next, before, now);
            if (finals_ > 0) --finals_;
            startSweep(at, each);
        }
    }
    if (sweeps_[0].number >= 0) {
        passes_ = 2 * sweeps_[0].number + (now_ >= sweeps_[0].start + sweeps_[0].each ? 2 : 1);
    }
}

void Wipers::startSweep(const double at, const double each) {
    WiperSweep s;
    s.number = count_++;
    s.start = at;
    s.each = each;
    s.dry = rate_ < DryRain && water(1.0) < DryRecent && coverage() < DryCover;
    s.allAtStart = all_ - rate_ * landing(rate_) * (now_ - at);
    sweeps_[1] = sweeps_[0];
    sweeps_[0] = s;
}

bool Wipers::sweeping() const {
    return sweeps_[0].number >= 0 && now_ < sweeps_[0].end();
}

double Wipers::angle() const {
    return sweeping() ? sweepAngle(now_ - sweeps_[0].start, sweeps_[0].each) : ParkAngle;
}

int Wipers::moving() const {
    if (!sweeping()) return 0;
    return now_ - sweeps_[0].start < sweeps_[0].each ? 1 : -1;
}

double Wipers::lastWipe(const double a) const {
    const double u = reachTime(a);
    for (const WiperSweep& s : sweeps_) {
        if (s.number < 0) break;
        const double back = s.start + s.each * (2 - u);
        if (back <= now_) return back;
        const double out = s.start + s.each * u;
        if (out <= now_) return out;
    }
    return Never;
}

double Wipers::waterSinceStart(const WiperSweep& s) const {
    if (s.number < 0) return all_;
    return std::max(all_ - s.allAtStart * std::exp(-(now_ - s.start) / DrySeconds), 0.0);
}

double Wipers::waterSinceEnd(const WiperSweep& s) const {
    if (s.number < 0) return all_;
    if (s.allAtEnd < 0) return 0;
    return std::max(all_ - s.allAtEnd * std::exp(-(now_ - s.end()) / DrySeconds), 0.0);
}

double Wipers::waterSinceWipe(const double a) const {
    const double at = lastWipe(a);
    if (at == Never) return all_;
    for (const WiperSweep& s : sweeps_) {
        if (s.number < 0 || at < s.start) continue;
        const double ws = waterSinceStart(s), we = waterSinceEnd(s);
        const double span = std::min(s.end(), now_) - s.start;
        return span > 0 ? ws + (we - ws) * (at - s.start) / span : ws;
    }
    return all_;
}

double Wipers::water(const double age) const {
    if (!(age > 0)) return 0;
    // The newest slice runs from `sliceStart_` to now; each older one a
    // whole `SliceSeconds`. A slice is taken in part by its share of time.
    double sum = 0, from = 0;
    for (int k = 0; k < Slices; ++k) {
        const double len = k == 0 ? now_ - sliceStart_ : SliceSeconds;
        if (len <= 0) continue;
        if (from + len >= age) return sum + slices_[k] * (age - from) / len;
        sum += slices_[k];
        from += len;
    }
    return sum;
}

std::array<double, Wipers::HistoryAges.size()> Wipers::history() const {
    std::array<double, HistoryAges.size()> h{};
    for (size_t k = 0; k < h.size(); ++k) h[k] = water(HistoryAges[k]);
    return h;
}

} // namespace ac
