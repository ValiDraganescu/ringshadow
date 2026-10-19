// The ringshadow sweep (Ringshadow.h): two a world day at their hours, the
// band the shadow of the ring's arc crossing the sun, its soft edges, the
// bonus.
#include "test.h"

#include "Ringshadow.h"
#include "Rules.h"

#include <cmath>

using namespace ac;

namespace {

/// A medium region (144 × 96 cells).
const GroundRect medium{0, 0, 144, 96};

constexpr double deg = 3.14159265358979323846 / 180.0;

/// A sky direction from a compass bearing and an elevation, degrees (sim
/// axes: x east, y up, z south).
Vec3 sky(double bearing, double elevation) {
    return Vec3(std::sin(bearing * deg) * std::cos(elevation * deg), std::sin(elevation * deg),
                -std::cos(bearing * deg) * std::cos(elevation * deg));
}

double bearingOf(Vec3 v) {
    const double b = std::atan2(v.x, -v.z) / deg;
    return b < 0 ? b + 360 : b;
}

double elevationOf(Vec3 v) { return std::asin(v.y) / deg; }

/// The sky's own normal of the ring's arc, as M_AcSky works it out
/// (make_sky_material.py, RING_HLSL: `nrm = cross(G, S)`, Unreal axes x, z, y),
/// back in sim axes.
Vec3 skyNormal(Vec3 s, Vec3 g) {
    const Vec3 G(g.x, g.z, g.y), S(s.x, s.z, s.y);
    const Vec3 n(G.y * S.z - G.z * S.y, G.z * S.x - G.x * S.z, G.x * S.y - G.y * S.x);
    return normalize(Vec3(n.x, n.z, n.y));
}

/// AAcRingshadow's offset of the arc from the sun for an eye at `p`
/// (degrees along the sky's normal): -RingHalf at the band's leading edge,
/// +RingHalf at its trailing one.
double ringOff(const Ringshadow::Band& b, Vec2 p) {
    const double across = (dot(p, b.dir) - b.trail) / (b.lead - b.trail);
    return (0.5 - across) * 2.0 * 6.0;
}

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

TEST(RingshadowSunAndGiantStandWhereTheSkyDrawsThem) {
    // The sun's path is the core's (Rules::sunToward): at the sweeps' hours
    // it stands here (Autocraft.Ringshadow.Sky checks AAcDaylight and
    // FAcGasGiant against the same numbers).
    const Vec3 s0 = Ringshadow::sun(0), s1 = Ringshadow::sun(1), g = Ringshadow::giant();
    EXPECT_NEAR(bearingOf(s0), 301.0, 0.5);
    EXPECT_NEAR(elevationOf(s0), 52.9, 0.5);
    EXPECT_NEAR(bearingOf(s1), 20.2, 0.5);
    EXPECT_NEAR(elevationOf(s1), 27.2, 0.5);
    EXPECT_NEAR(bearingOf(g), 280.0, 1e-9);
    EXPECT_NEAR(elevationOf(g), 12.0, 1e-9);
    // The same sun as the height the sight closes in by.
    for (double h = 0; h < 24; h += 0.25) EXPECT_NEAR(elevationOf(Rules::sunToward(h)), Rules::sunHeight(h), 1e-9);
}

TEST(RingshadowDirectionIsTheArcsShadow) {
    // The sun due south 60° up, the giant due north low: the ring's great
    // circle is the meridian, so the arc crosses the sun north to south and
    // its edges' shadows lie north-south on the ground. The band moves
    // across them, west here (the arc slides west over the sun: n = s × g
    // points west), not north or south.
    const Vec2 d = Ringshadow::direction(sky(180, 60), sky(0, 12));
    EXPECT_NEAR(std::hypot(d.x, d.y), 1.0, 1e-12);
    EXPECT_NEAR(d.x, -1.0, 1e-9);
    EXPECT_NEAR(d.y, 0.0, 1e-9);
    // The giant on the other side of the sun along the meridian turns the
    // circle round: east.
    EXPECT_NEAR(Ringshadow::direction(sky(180, 60), sky(180, 10)).x, 1.0, 1e-9);
    // So the band's edges, across its way, lie along the meridian, the arc's
    // own line on the ground. The same with the arc running east-west
    // through the zenith (the giant low in the east, the sun high in the
    // east): the edges lie east-west and the band moves north or south.
    const Vec2 ns = Ringshadow::direction(sky(90, 70), sky(90, 12));
    EXPECT_NEAR(ns.x, 0.0, 1e-9);
    EXPECT_NEAR(std::fabs(ns.y), 1.0, 1e-9);
    // The two sweeps: the sun has moved, and so has the band.
    EXPECT_NEAR(Ringshadow::bearing(0), 186.1, 0.5);
    EXPECT_NEAR(Ringshadow::bearing(1), 169.2, 0.5);
    EXPECT_TRUE(std::fabs(Ringshadow::bearing(0) - Ringshadow::bearing(1)) > 10);
    EXPECT_NEAR(std::hypot(Ringshadow::direction(1).x, Ringshadow::direction(1).y), 1.0, 1e-12);
}

TEST(RingshadowArcSlidesTheWayTheBandMoves) {
    // The band's edge on the ground runs along e = (s × t) × up: across it
    // the band moves in the plane of the sky's normal of the arc.
    for (int i = 0; i < 2; ++i) {
        const Vec3 s = Ringshadow::sun(i), g = Ringshadow::giant();
        const Vec3 n = skyNormal(s, g);
        const Vec2 d = Ringshadow::direction(i);
        const Vec2 level = normalize(Vec2(n.x, n.z));
        EXPECT_NEAR(dot(d, level), 1.0, 1e-9);
        // The sky's drawing of the arc's offset against the parallax of a
        // far ring: an eye further along the band's way sees the arc further
        // toward -n (RingOff smaller), as a far thing seen from a point moved
        // by d shifts by -d · n.
        const auto sw = Ringshadow::next(WorldClock::timeOf(Ringshadow::hours[i], 0));
        EXPECT_EQ(sw.index, i);
        const auto b = Ringshadow::band(sw.start + 60, medium);
        ASSERT_TRUE(b.has_value());
        const Vec2 mid = b->dir * (0.5 * (b->lead + b->trail));
        const double shift = ringOff(*b, mid + d * 10) - ringOff(*b, mid);
        EXPECT_TRUE(shift < 0);
        EXPECT_TRUE(dot(Vec3(d.x, 0, d.y), n) > 0);
        // As time runs on, an eye's arc slides toward +n: the way its shadow goes.
        const auto later = Ringshadow::band(sw.start + 61, medium);
        EXPECT_TRUE(ringOff(*later, mid) > ringOff(*b, mid));
        // Along the edge line the arc stands still (no parallax across n).
        const Vec2 e(-d.y, d.x);
        EXPECT_NEAR(ringOff(*b, mid + e * 50), ringOff(*b, mid), 1e-9);
    }
}

TEST(RingshadowBandCrossesTheRegion) {
    // Both sweeps move southward (186° and 169°): the north side goes dark
    // first and comes out first.
    const Vec2 north(72, 4), south(72, 92);
    double t0 = 0;
    for (int i = 0; i < 2; ++i) {
        const auto s = Ringshadow::next(t0);
        t0 = s.end;
        EXPECT_EQ(s.index, i);
        // At the start nothing of the region is in it; at the end nothing either.
        for (double x = 0; x <= 144; x += 8)
            for (double z = 0; z <= 96; z += 8) {
                EXPECT_NEAR(Ringshadow::cover(s.start, Vec2(x, z), medium), 0.0, 1e-9);
                EXPECT_NEAR(Ringshadow::cover(s.end - 1e-6, Vec2(x, z), medium), 0.0, 1e-9);
            }
        double northIn = -1, southIn = -1, northOut = -1, dark = 0;
        for (double t = s.start; t < s.end; t += 0.5) {
            const double n = Ringshadow::cover(t, north, medium), so = Ringshadow::cover(t, south, medium);
            if (northIn < 0 && n > 0.5) northIn = t;
            if (southIn < 0 && so > 0.5) southIn = t;
            if (northIn > 0 && northOut < 0 && n < 0.5) northOut = t;
            if (n >= 1) dark += 0.5;
        }
        EXPECT_TRUE(northIn > s.start && southIn > northIn);
        EXPECT_TRUE(northOut > northIn);
        // A point is in the shadow (past half) for about 40 s, 33 of them at full dark.
        EXPECT_TRUE(northOut - northIn > 36 && northOut - northIn < 45);
        EXPECT_TRUE(dark > 29 && dark < 38);
        EXPECT_NEAR(Ringshadow::cover(0.5 * (northIn + northOut), north, medium), 1.0, 1e-9);
    }
}

TEST(RingshadowCoverIsSymmetricAboutTheBandsMiddle) {
    for (int i = 0; i < 2; ++i) {
        const auto s = Ringshadow::next(WorldClock::timeOf(Ringshadow::hours[i], 0));
        const auto b = Ringshadow::band(s.start + 55, medium);
        ASSERT_TRUE(b.has_value());
        // The middle line runs along the edge e; the cover is the same on
        // both sides of it and the same all along it.
        const Vec2 e(-b->dir.y, b->dir.x);
        const Vec2 mid = b->dir * (0.5 * (b->lead + b->trail));
        for (double k = -80; k <= 80; k += 20)
            for (double x = 0; x <= 70; x += 2.5) {
                const double c = Ringshadow::cover(*b, mid + e * k + b->dir * x);
                EXPECT_NEAR(c, Ringshadow::cover(*b, mid + e * k - b->dir * x), 1e-9);
                EXPECT_NEAR(c, Ringshadow::cover(*b, mid + b->dir * x), 1e-9);
            }
    }
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
    const Vec2 north(72, 4);
    double under = -1;
    for (double t = s.start; t < s.end && under < 0; t += 0.5)
        if (Ringshadow::cover(t, north, medium) >= 1.0) under = t;
    ASSERT_TRUE(under > 0);
    EXPECT_EQ(Ringshadow::load(25, under, north, medium), 38);
    EXPECT_EQ(Ringshadow::load(25, s.start - 10, north, medium), 25);
    EXPECT_EQ(Ringshadow::load(25, under, Vec2(72, 92), medium), 25);
    EXPECT_EQ(Ringshadow::load(0, under, north, medium), 0);
}
