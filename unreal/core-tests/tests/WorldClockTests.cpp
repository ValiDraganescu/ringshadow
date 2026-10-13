// The MMORPG world's clock (WorldClock.h): its long day, its light share,
// the start in the morning, a held hour, the weather's seed.
#include "test.h"

#include "Rules.h"
#include "WorldClock.h"

#include <cmath>

using namespace ac;

TEST(WorldClockStartsInTheMorningAndRunsADay) {
    EXPECT_NEAR(WorldClock::hourAt(0), WorldClock::startHour, 1e-9);
    // A whole day later it is the same hour; the hour always moves forward.
    EXPECT_NEAR(WorldClock::hourAt(WorldClock::dayLength), WorldClock::startHour, 1e-9);
    double last = WorldClock::hourAt(0);
    int wraps = 0;
    for (double t = 1; t <= WorldClock::dayLength; t += 1) {
        const double h = WorldClock::hourAt(t);
        if (h < last) wraps += 1;
        else EXPECT_TRUE(h - last < 0.1);
        last = h;
    }
    EXPECT_EQ(wraps, 1);
}

TEST(WorldClockLightTakesThreeQuartersOfTheDay) {
    // Sunrise to sunset: lightShare of the day (30 of 40 minutes).
    const double rise = WorldClock::timeOf(Rules::sunrise, 0);
    const double set = WorldClock::timeOf(Rules::sunset, rise);
    EXPECT_NEAR(set - rise, WorldClock::lightShare * WorldClock::dayLength, 1e-6);
    // And it is dark (sight at its night share) for most of the rest.
    int dark = 0, n = 0;
    for (double t = 0; t < WorldClock::dayLength; t += 1, n++)
        if (Rules::dark(WorldClock::hourAt(t)) > 0.95) dark += 1;
    EXPECT_TRUE(double(dark) / n > 0.18);
    EXPECT_TRUE(double(dark) / n < 0.25);
}

TEST(WorldClockTimeOfIsHourAtsInverse) {
    for (double h : {0.0, 5.5, 8.0, 12.0, 18.5, 18.9, 22.0, 23.9}) {
        const double t = WorldClock::timeOf(h, 100);
        EXPECT_TRUE(t >= 100 && t < 100 + WorldClock::dayLength);
        EXPECT_NEAR(WorldClock::hourAt(t), h, 1e-6);
    }
}

TEST(WorldClockHeldHour) {
    EXPECT_NEAR(WorldClock::hourAt(1234, WorldClock::dayLength, 22.0), 22.0, 1e-12);
    // Negative: the clock runs.
    EXPECT_NEAR(WorldClock::hourAt(0, WorldClock::dayLength, -1.0), WorldClock::startHour, 1e-9);
}

TEST(WorldClockWeatherSeedIsTheWorlds) {
    EXPECT_EQ(WorldClock::weatherSeed("first"), WorldClock::weatherSeed("first"));
    EXPECT_TRUE(WorldClock::weatherSeed("first") != WorldClock::weatherSeed("second"));
}
