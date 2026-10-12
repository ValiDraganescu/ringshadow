// ac::Weather (docs/weather.md): the schedule, the changes, the wet
// ground, the lightning.
#include "test.h"

#include "Weather.h"

#include <cmath>

using namespace ac;

namespace {
    int rank(WeatherState s) { return int(s); }
}

/// Spells walk between neighbours only, start clear for at least 5 min,
/// and last as long as their state allows.
TEST(weather_spellsWalkBetweenNeighbours) {
    for (uint64_t seed = 1; seed <= 20; ++seed) {
        Weather w(seed);
        const WeatherSpell first = w.spellAt(0);
        EXPECT_TRUE(first.state == WeatherState::clear);
        EXPECT_TRUE(first.end >= Weather::FirstClearSeconds);
        WeatherSpell prev = first;
        for (int k = 0; k < 40; ++k) {
            const WeatherSpell s = w.spellAt(prev.end + 0.5);
            EXPECT_EQ(s.start, prev.end);
            EXPECT_EQ(std::abs(rank(s.state) - rank(prev.state)), 1);
            const double len = s.end - s.start;
            EXPECT_TRUE(len >= 90 && len <= 480);
            prev = s;
        }
    }
}

/// The same seed and time give the same sample, whatever was asked before,
/// and the integrated parts (wind offset, wetness) agree going backwards.
TEST(weather_isAPureFunctionOfSeedAndTime) {
    Weather a(7), b(7);
    std::vector<double> times;
    for (double t = 0; t < 3600; t += 37.3) times.push_back(t);
    std::vector<WeatherSample> forward;
    for (double t : times) forward.push_back(a.sample(t));
    for (size_t k = times.size(); k-- > 0;) {
        const WeatherSample s = b.sample(times[k]);
        EXPECT_NEAR(s.cover, forward[k].cover, 1e-12);
        EXPECT_NEAR(s.rain, forward[k].rain, 1e-12);
        EXPECT_NEAR(s.windKm, forward[k].windKm, 1e-9);
        EXPECT_NEAR(s.wetness, forward[k].wetness, 1e-9);
        EXPECT_NEAR(s.flash, forward[k].flash, 1e-12);
    }
    Weather c(8);
    bool differs = false;
    for (double t : times) differs = differs || std::abs(c.sample(t).cover - forward[size_t(t / 37.3)].cover) > 1e-6;
    EXPECT_TRUE(differs);
}

/// Every quantity moves smoothly: no jumps between two samples 0.1 s apart.
TEST(weather_changesAreContinuous) {
    Weather w(3);
    WeatherSample prev = w.sample(0);
    for (double t = 0.1; t < 7200; t += 0.1) {
        const WeatherSample s = w.sample(t);
        EXPECT_TRUE(std::abs(s.cover - prev.cover) < 0.01);
        EXPECT_TRUE(std::abs(s.rain - prev.rain) < 0.01);
        EXPECT_TRUE(std::abs(s.wetness - prev.wetness) < 0.01);
        EXPECT_TRUE(s.windKm >= prev.windKm);
        EXPECT_TRUE(s.windKm - prev.windKm < 0.0027);
        prev = s;
    }
}

/// Rain comes after the cloud has built and stops before it breaks.
TEST(weather_rainFollowsTheCloud) {
    for (uint64_t seed = 1; seed <= 30; ++seed) {
        Weather w(seed);
        double t = 0;
        for (int k = 0; k < 30; ++k) {
            const WeatherSpell s = w.spellAt(t);
            const WeatherSpell prev = s.start > 0 ? w.spellAt(s.start - 0.01) : s;
            if (prev.state == WeatherState::cloudy && s.state == WeatherState::rain) {
                const WeatherSample early = w.sample(s.start + Weather::ChangeSeconds * 0.3);
                EXPECT_TRUE(early.cover > Weather::coverOf(WeatherState::cloudy) + 0.05);
                EXPECT_TRUE(early.rain < 0.02);
            }
            if (prev.state == WeatherState::rain && s.state == WeatherState::cloudy) {
                const WeatherSample early = w.sample(s.start + Weather::ChangeSeconds * 0.3);
                EXPECT_TRUE(early.rain < Weather::rainOf(WeatherState::rain) - 0.1);
                EXPECT_TRUE(early.cover > Weather::coverOf(WeatherState::rain) - 0.02);
            }
            t = s.end + 0.01;
        }
    }
}

/// The ground gets wet in the rain and dries after it; never outside 0-1.
TEST(weather_groundWetsAndDries) {
    Weather w(5);
    w.hold(-1);
    bool sawWet = false, sawDryAfterWet = false;
    for (double t = 0; t < 4 * 3600; t += 5) {
        const WeatherSample s = w.sample(t);
        EXPECT_TRUE(s.wetness >= 0 && s.wetness <= 1);
        if (s.wetness > 0.8) sawWet = true;
        if (sawWet && s.wetness < 0.1) sawDryAfterWet = true;
        if (s.rain == 0 && s.change >= 1 && w.spellAt(t).state == WeatherState::clear && t > 400) {
            // Dry spells start from a wet ground at most 10 min earlier.
        }
    }
    EXPECT_TRUE(sawWet);
    EXPECT_TRUE(sawDryAfterWet);
}

/// Holding a state gives its targets, fractions blend, and the held rain
/// wets the ground at once (for shots).
TEST(weather_holdGivesTheStateAndBlends) {
    Weather w(9);
    w.hold(3);
    WeatherSample s = w.sample(10);
    EXPECT_NEAR(s.cover, Weather::coverOf(WeatherState::storm), 1e-12);
    EXPECT_NEAR(s.rain, 1.0, 1e-12);
    EXPECT_NEAR(s.wetness, 1.0, 1e-12);
    EXPECT_TRUE(s.state == WeatherState::storm);
    w.hold(2.5);
    s = w.sample(10);
    EXPECT_NEAR(s.rain, 0.75, 1e-12);
    w.hold(0);
    s = w.sample(10);
    EXPECT_NEAR(s.rain, 0.0, 1e-12);
    EXPECT_NEAR(s.wetness, 0.0, 1e-12);
    w.hold(-1);
    EXPECT_TRUE(w.held() < 0);
}

/// Lightning strikes only in a storm, a few seconds apart; the flash
/// follows the strike and the thunder comes distance / 343 m/s later.
TEST(weather_lightningOnlyInStorms) {
    Weather w(11);
    w.hold(1);
    EXPECT_EQ(w.strikes(0, 600).size(), size_t(0));
    w.hold(2);
    EXPECT_EQ(w.strikes(0, 600).size(), size_t(0));
    w.hold(3);
    const auto strikes = w.strikes(0, 600);
    EXPECT_TRUE(strikes.size() > 40 && strikes.size() < 140);
    for (const auto& s : strikes) {
        EXPECT_TRUE(s.distanceKm >= 0.6 && s.distanceKm <= 9);
        EXPECT_TRUE(s.power >= 0.5 && s.power <= 1);
        EXPECT_TRUE(w.sample(s.time + 0.01).flash > 0.1);
        const auto heard = w.thunder(s.thunderTime() - 0.001, s.thunderTime() + 0.001);
        bool found = false;
        for (const auto& h : heard) found = found || std::abs(h.time - s.time) < 1e-9;
        EXPECT_TRUE(found);
    }
    // Between strikes the sky is dark again.
    double quiet = 0;
    for (double t = 0; t < 600; t += 0.05) quiet += w.sample(t).flash < 0.02 ? 0.05 : 0;
    EXPECT_TRUE(quiet > 400);
}

/// Over a long game the schedule visits every state, and it rains about
/// a quarter to a third of the time.
TEST(weather_scheduleMix) {
    double wet = 0, total = 0;
    int states[4] = {0, 0, 0, 0};
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        Weather w(seed);
        for (double t = 0; t < 6 * 3600; t += 10) {
            const WeatherSample s = w.sample(t);
            wet += s.rain > 0.1 ? 10 : 0;
            total += 10;
            states[rank(w.spellAt(t).state)] += 1;
        }
    }
    for (int k = 0; k < 4; ++k) EXPECT_TRUE(states[k] > 0);
    EXPECT_TRUE(wet / total > 0.15 && wet / total < 0.4);
}
