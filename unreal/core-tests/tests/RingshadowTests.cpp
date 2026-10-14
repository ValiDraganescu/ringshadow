// The ringshadow sweep (Ringshadow.h): two a world day at their hours, the
// band crossing a region out of the giant's side, its soft edges, the bonus.
#include "test.h"

#include "Ringshadow.h"

#include <cmath>

using namespace ac;

namespace {

/// A medium region (144 × 96 cells).
const GroundRect medium{0, 0, 144, 96};

} // namespace

TEST(RingshadowSweepsTwiceADayAtItsHours) {
    int starts = 0;
    bool was = false;
    double on = 0;
    for (double t = 0; t < WorldClock::dayLength; t += 1) {
        const auto s = Ringshadow::at(t);
        if (s && !was) {
            starts += 1;
            const double h = WorldClock::hourAt(s->start);
            EXPECT_TRUE(std::fabs(h - Ringshadow::hours[0]) < 1e-6 || std::fabs(h - Ringshadow::hours[1]) < 1e-6);
            EXPECT_NEAR(s->end - s->start, Ringshadow::duration, 1e-9);
        }
        if (s) on += 1;
        was = s.has_value();
    }
    EXPECT_EQ(starts, 2);
    EXPECT_NEAR(on, 2 * Ringshadow::duration, 2.0);
    // A new world sees the first one 8 minutes in, at 11:30.
    const auto first = Ringshadow::next(0);
    EXPECT_NEAR(first.start, WorldClock::timeOf(11.5, 0), 1e-9);
    EXPECT_TRUE(first.start > 400 && first.start < 600);
    // The same the next day, a day later.
    EXPECT_NEAR(Ringshadow::next(WorldClock::dayLength).start, first.start + WorldClock::dayLength, 1e-6);
}

TEST(RingshadowNextIsTheRunningOneOrTheComing) {
    const auto first = Ringshadow::next(0);
    // The very same start from every moment until it ends (the client says
    // "in 60" once a sweep, keyed by it).
    for (double t = first.start - 300; t < first.end; t += 0.37) EXPECT_TRUE(Ringshadow::next(t) == first);
    EXPECT_TRUE(Ringshadow::at(first.start + 30).has_value());
    EXPECT_TRUE(!Ringshadow::at(first.start - 1).has_value());
    EXPECT_TRUE(!Ringshadow::at(first.end).has_value());
    const auto second = Ringshadow::next(first.end);
    EXPECT_TRUE(second.start > first.end);
    EXPECT_NEAR(WorldClock::hourAt(second.start), 16.0, 1e-6);
    // The held hour of a dev run does not stop it: it runs on the clock.
    EXPECT_TRUE(!Ringshadow::band(first.start - 1, medium).has_value());
}

TEST(RingshadowBandComesFromTheGiantsSide) {
    // It moves east and a little south (bearing 100°), away from the giant at 280°.
    const Vec2 d = Ringshadow::direction();
    EXPECT_NEAR(std::hypot(d.x, d.y), 1.0, 1e-12);
    EXPECT_TRUE(d.x > 0.98);
    EXPECT_TRUE(d.y > 0.1 && d.y < 0.2);
    const auto s = Ringshadow::next(0);
    const Vec2 west(4, 48), east(140, 48);
    // At the start nothing of the region is in it; at the end nothing either.
    for (double x = 0; x <= 144; x += 8)
        for (double z = 0; z <= 96; z += 8) {
            EXPECT_NEAR(Ringshadow::cover(s.start, Vec2(x, z), medium), 0.0, 1e-9);
            EXPECT_NEAR(Ringshadow::cover(s.end - 1e-6, Vec2(x, z), medium), 0.0, 1e-9);
        }
    // The west side goes dark first and comes out first.
    double westIn = -1, eastIn = -1, westOut = -1;
    for (double t = s.start; t < s.end; t += 0.5) {
        const double w = Ringshadow::cover(t, west, medium), e = Ringshadow::cover(t, east, medium);
        if (westIn < 0 && w > 0.5) westIn = t;
        if (eastIn < 0 && e > 0.5) eastIn = t;
        if (westIn > 0 && westOut < 0 && w < 0.5) westOut = t;
    }
    EXPECT_TRUE(westIn > s.start && eastIn > westIn);
    EXPECT_TRUE(westOut > westIn);
    // A point is in the shadow (past half) for about 35 s; full dark in the middle.
    EXPECT_TRUE(westOut - westIn > 30 && westOut - westIn < 45);
    EXPECT_NEAR(Ringshadow::cover(0.5 * (westIn + westOut), west, medium), 1.0, 1e-9);
}

TEST(RingshadowEdgesAreSoft) {
    const auto s = Ringshadow::next(0);
    const auto b = Ringshadow::band(s.start + 50, medium);
    ASSERT_TRUE(b.has_value());
    EXPECT_NEAR(b->lead - b->trail, Ringshadow::width, 1e-9);
    // Across the leading edge the cover falls from 1 to 0 over `soft` cells, smoothly.
    const Vec2 o = b->dir * b->lead;
    double last = 1;
    for (double k = -Ringshadow::soft; k <= 0; k += 1) {
        const double c = Ringshadow::cover(*b, o + b->dir * k);
        EXPECT_TRUE(c <= last + 1e-12);
        EXPECT_TRUE(last - c < 0.2);
        last = c;
    }
    EXPECT_NEAR(Ringshadow::cover(*b, o), 0.0, 1e-12);
    EXPECT_NEAR(Ringshadow::cover(*b, o - b->dir * Ringshadow::soft), 1.0, 1e-12);
}

TEST(RingshadowLoadPaysHalfAgainUnderTheBand) {
    const auto s = Ringshadow::next(0);
    const Vec2 west(4, 48);
    double under = -1;
    for (double t = s.start; t < s.end && under < 0; t += 0.5)
        if (Ringshadow::cover(t, west, medium) >= 1.0) under = t;
    ASSERT_TRUE(under > 0);
    EXPECT_EQ(Ringshadow::load(25, under, west, medium), 38);
    EXPECT_EQ(Ringshadow::load(25, s.start - 10, west, medium), 25);
    EXPECT_EQ(Ringshadow::load(25, under, Vec2(140, 48), medium), 25);
    EXPECT_EQ(Ringshadow::load(0, under, west, medium), 0);
}
