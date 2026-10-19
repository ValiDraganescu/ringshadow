// ac::Wipers (docs/weather.md, "Rain on the visor and the wipers"): the
// modes on the rain rate, their cadence, the blade's sweep, the park when
// the rain stops, the early sweep on a covered glass, the last-wipe field
// and the water's history.
#include "test.h"

#include "Wipers.h"

#include <cmath>
#include <vector>

using namespace ac;

namespace {

constexpr double Dt = 1.0 / 60;

/// Runs `w` at `rate` from `from` for `seconds`; the starts of the sweeps
/// begun on the way.
std::vector<double> run(Wipers& w, double& t, double rate, double seconds) {
    std::vector<double> starts;
    int64_t seen = w.latest().number;
    const double end = t + seconds;
    for (; t < end - 1e-9; t += Dt) {
        w.update(t, rate);
        if (w.latest().number != seen) {
            seen = w.latest().number;
            starts.push_back(w.latest().start);
        }
    }
    return starts;
}

} // namespace

TEST(wipers_modesOnTheRainRate) {
    EXPECT_TRUE(Wipers::modeFor(0) == WiperMode::off);
    EXPECT_TRUE(Wipers::modeFor(0.149) == WiperMode::off);
    EXPECT_TRUE(Wipers::modeFor(0.15) == WiperMode::intermittent);
    EXPECT_TRUE(Wipers::modeFor(0.449) == WiperMode::intermittent);
    EXPECT_TRUE(Wipers::modeFor(0.45) == WiperMode::steady);
    EXPECT_TRUE(Wipers::modeFor(0.799) == WiperMode::steady);
    EXPECT_TRUE(Wipers::modeFor(0.8) == WiperMode::fast);
    EXPECT_TRUE(Wipers::modeFor(1) == WiperMode::fast);
}

/// Intermittent every 5 s, steady every 1.8 s, both at the slow speed;
/// fast without a pause, out and back in 0.8 s.
TEST(wipers_cadenceOfEachMode) {
    struct Case { double rate, every, each; };
    for (const Case c : {Case{0.3, 5.0, 0.7}, Case{0.6, 1.8, 0.7}, Case{1.0, 0.8, 0.4}}) {
        Wipers w;
        double t = 100;
        const std::vector<double> starts = run(w, t, c.rate, 30);
        EXPECT_TRUE(starts.size() >= 5);
        EXPECT_NEAR(starts.front(), 100, 1e-9);
        for (size_t k = 1; k < starts.size(); ++k) EXPECT_NEAR(starts[k] - starts[k - 1], c.every, Dt);
        EXPECT_NEAR(w.latest().each, c.each, 1e-9);
        EXPECT_NEAR(static_cast<double>(starts.size()), std::floor(30 / c.every) + 1, 1.0);
    }
    // Held off: no sweep in a storm, however covered the glass.
    Wipers held;
    held.enable(false);
    double th = 0;
    EXPECT_EQ(run(held, th, 1.0, 30).size(), size_t(0));
    EXPECT_TRUE(held.mode() == WiperMode::off && held.coverage() > 0.9);
    held.enable(true);
    EXPECT_EQ(run(held, th, 1.0, 1).size(), size_t(2));
    // Off: no sweep in a dry minute.
    Wipers dry;
    double t = 0;
    EXPECT_EQ(run(dry, t, 0, 60).size(), size_t(0));
    EXPECT_TRUE(dry.parked());
    EXPECT_NEAR(dry.angle(), Wipers::ParkAngle, 1e-12);
}

/// Out from the park to the far stop, back to the park; eased at the
/// stops, straight up half way; `reachTime` is the ease's inverse.
TEST(wipers_sweepAngle) {
    const double each = 0.7;
    EXPECT_NEAR(Wipers::sweepAngle(-0.1, each), Wipers::ParkAngle, 1e-12);
    EXPECT_NEAR(Wipers::sweepAngle(0, each), Wipers::ParkAngle, 1e-12);
    EXPECT_NEAR(Wipers::sweepAngle(each / 2, each), 0, 1e-9);
    EXPECT_NEAR(Wipers::sweepAngle(each, each), Wipers::FarAngle, 1e-9);
    EXPECT_NEAR(Wipers::sweepAngle(1.5 * each, each), 0, 1e-9);
    EXPECT_NEAR(Wipers::sweepAngle(2 * each, each), Wipers::ParkAngle, 1e-12);
    // Slow at the stops: the first tenth of the time covers under a tenth of the arc.
    const double arc = Wipers::FarAngle - Wipers::ParkAngle;
    EXPECT_TRUE(Wipers::sweepAngle(0.1 * each, each) - Wipers::ParkAngle < 0.1 * arc);
    // Out and back pass each angle at mirrored times.
    for (double a = -1.5; a <= 1.5; a += 0.25) {
        const double u = Wipers::reachTime(a);
        EXPECT_NEAR(Wipers::sweepAngle(u * each, each), a, 1e-9);
        EXPECT_NEAR(Wipers::sweepAngle((2 - u) * each, each), a, 1e-9);
    }
    EXPECT_NEAR(Wipers::reachTime(Wipers::ParkAngle), 0, 1e-12);
    EXPECT_NEAR(Wipers::reachTime(Wipers::FarAngle), 1, 1e-12);
}

/// When the rain stops (or the glass is sheltered: the fed rate 0), two
/// last sweeps back to back, the second one dry, then the park.
TEST(wipers_parkWhenTheRainStops) {
    Wipers w;
    double t = 0;
    run(w, t, 1.0, 10);
    EXPECT_TRUE(w.mode() == WiperMode::fast);
    // Let the running sweep finish its pass.
    const double stop = t;
    const std::vector<double> finals = run(w, t, 0.0, 10);
    EXPECT_EQ(finals.size(), size_t(Wipers::FinalSweeps));
    EXPECT_TRUE(w.mode() == WiperMode::off);
    EXPECT_TRUE(w.parked());
    EXPECT_NEAR(w.angle(), Wipers::ParkAngle, 1e-12);
    EXPECT_EQ(w.finalsLeft(), 0);
    if (finals.size() == 2) {
        EXPECT_TRUE(finals[0] - stop < 0.8 + Dt);
        EXPECT_NEAR(finals[1] - finals[0], 2 * Wipers::SlowEach, Dt);
        EXPECT_NEAR(w.latest().each, Wipers::SlowEach, 1e-9);
        EXPECT_TRUE(w.latest().dry);
        EXPECT_TRUE(!w.previous().dry);
    }
    // Parked for good while it stays dry.
    EXPECT_EQ(run(w, t, 0.0, 60).size(), size_t(0));
    // The rain coming back during the last sweeps cancels the park.
    Wipers again;
    double t2 = 0;
    run(again, t2, 0.6, 5);
    run(again, t2, 0.0, 0.5);
    run(again, t2, 0.6, 10);
    EXPECT_TRUE(again.mode() == WiperMode::steady);
    EXPECT_EQ(again.finalsLeft(), 0);
}

/// A wet sweep is not dry: the squeak is only for a nearly dry glass.
TEST(wipers_wetSweepsAreNotDry) {
    Wipers w;
    double t = 0;
    run(w, t, 0.5, 20);
    EXPECT_TRUE(!w.latest().dry);
}

/// With the wiper off (a drizzle), the water builds until the glass is
/// half covered, and then the blade sweeps once on its own.
TEST(wipers_sweepEarlyOnCoverage) {
    Wipers w;
    double t = 0;
    const double rate = 0.12;
    EXPECT_TRUE(Wipers::modeFor(rate) == WiperMode::off);
    EXPECT_EQ(run(w, t, rate, 40).size(), size_t(0));
    EXPECT_TRUE(w.coverage() < Wipers::SweepEarlyAt);
    double before = 0;
    std::vector<double> starts;
    while (starts.empty() && t < 200) {
        before = w.coverage();
        starts = run(w, t, rate, Dt);
    }
    EXPECT_EQ(starts.size(), size_t(1));
    EXPECT_TRUE(before >= Wipers::SweepEarlyAt - 0.01);
    EXPECT_TRUE(t > 45 && t < 80);
    // The sweep cleared the middle of the glass.
    run(w, t, rate, 2);
    EXPECT_TRUE(w.coverage() < 0.05);
    // Intermittent: the next one waits for the cadence, not the cover.
    Wipers i;
    double ti = 0;
    const std::vector<double> s = run(i, ti, 0.3, 12);
    EXPECT_EQ(s.size(), size_t(3));
}

/// The last wipe of a spot: the latest crossing of its angle at or before
/// now; mid sweep, the angles not reached yet keep the sweep before.
TEST(wipers_lastWipeField) {
    Wipers w;
    EXPECT_TRUE(w.lastWipe(0) == Wipers::Never);
    double t = 10;
    w.update(t, 0.6);
    const WiperSweep first = w.latest();
    EXPECT_NEAR(first.start, 10, 1e-9);
    // A bit into the out pass: the left is wiped, the right not yet.
    t += 0.3;
    w.update(t, 0.6);
    EXPECT_TRUE(w.moving() == 1);
    const double blade = w.angle();
    EXPECT_NEAR(w.lastWipe(blade - 0.2), first.start + first.each * Wipers::reachTime(blade - 0.2), 1e-9);
    EXPECT_TRUE(w.lastWipe(blade + 0.2) == Wipers::Never);
    // After the sweep: the back pass, later on the left (it comes back last).
    run(w, t, 0.6, 1.2);
    const double l = w.lastWipe(-1.0), r = w.lastWipe(1.0);
    EXPECT_NEAR(l, first.start + first.each * (2 - Wipers::reachTime(-1.0)), 1e-9);
    EXPECT_TRUE(l > r);
    // Into the next sweep's out pass: the right still the first sweep's.
    run(w, t, 0.6, 1.8 - 1.5 + 0.2);
    EXPECT_EQ(w.latest().number, int64_t(1));
    EXPECT_NEAR(w.lastWipe(1.4), r + 0, 1.0);
    EXPECT_NEAR(w.lastWipe(1.4), first.start + first.each * (2 - Wipers::reachTime(1.4)), 1e-9);
    EXPECT_TRUE(w.lastWipe(-1.4) >= w.latest().start);
}

/// The water's history: what landed in the last few seconds, heavier rain
/// landing more; monotone in the age, toward the whole.
TEST(wipers_waterHistory) {
    Wipers w;
    double t = 0;
    run(w, t, 0.5, 60);
    const double per = 0.5 * Wipers::landing(0.5);
    EXPECT_NEAR(w.water(4), per * 4, per * 4 * 0.03);
    EXPECT_NEAR(w.water(0.5), per * 0.5, per * 0.5 * 0.05);
    const auto h = w.history();
    for (size_t k = 1; k < h.size(); ++k) EXPECT_TRUE(h[k] > h[k - 1]);
    EXPECT_TRUE(w.all() >= h.back());
    EXPECT_TRUE(w.water(1e9) <= w.all() + 1e-9);
    // Swept every 1.8 s: no more than that much rain since the blade passed.
    EXPECT_TRUE(w.waterSinceWipe(0) < per * 1.8);
    // Never wiped (the wiper off, a drizzle): the whole lands there.
    Wipers off;
    double to = 0;
    run(off, to, 0.1, 20);
    EXPECT_NEAR(off.waterSinceWipe(0), off.all(), 1e-9);
    // A storm lands three times its rate.
    EXPECT_NEAR(Wipers::landing(1.0), 3.0, 1e-12);
    // Rain 0.5 half covers the glass in 8 s; a storm in 2.
    EXPECT_NEAR(Wipers::coverOf(0.5 * Wipers::landing(0.5) * 8), 0.5, 0.01);
    EXPECT_NEAR(Wipers::coverOf(Wipers::landing(1.0) * 2), 0.5, 0.01);
    // Dry weather: nothing lands, the old water dries.
    const double wet = w.all();
    run(w, t, 0, 60);
    EXPECT_NEAR(w.water(4), 0, 1e-12);
    EXPECT_TRUE(w.all() < wet * 0.75 && w.all() > wet * 0.6);
    // Water landed since a wipe: what fell after the blade passed, however
    // long ago (two final sweeps in rain, then a dry minute: still none).
    Wipers p;
    double tp = 0;
    run(p, tp, 1.0, 10);
    EXPECT_TRUE(p.waterSinceWipe(0) < Wipers::landing(1.0) * 0.9);
    run(p, tp, 0.0, 70);
    EXPECT_NEAR(p.waterSinceWipe(0), 0, 1e-6);
    EXPECT_NEAR(p.waterSinceWipe(1.2), 0, 1e-6);
    EXPECT_TRUE(p.all() > 10);
    // Rain again: on the parked glass it builds as it lands.
    run(p, tp, 0.1, 20);
    const double dried = 0.1 * Wipers::landing(0.1) * Wipers::DrySeconds * (1 - std::exp(-20 / Wipers::DrySeconds));
    EXPECT_NEAR(p.waterSinceWipe(0), dried, 0.01);
    // Time never runs backwards.
    const double now = w.now();
    w.update(now - 5, 1.0);
    EXPECT_NEAR(w.now(), now, 1e-12);
}
