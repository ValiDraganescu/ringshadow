// C4, the drive (docs/haul-and-ferry.md "The drive"): wheels turn only as
// they roll, on arcs no tighter than their turning radius, backing up the
// other way; tracks turn on the spot; a vehicle climbs no steeper than its
// gear lets it, driven or on the AI's paths. Hand-built scenes on the
// playground's flat ground, and on a slope raised on it.
#include "SimHelpers.h"

#include "Drive.h"
#include "WindowMaps.h"

#include <cmath>

using namespace ac;
using namespace simtest;

namespace {

/// Blue's `kind` at `at` facing `heading`, driven, on `map` (the playground).
struct Drive {
    Simulation sim;
    int64_t me;
};

Drive drive(UnitKind kind, Vec2 at, double heading, const MapDefinition& map = WindowMaps::playground()) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t id = addUnit(s, kind, at, 0, Unit::Task::idle, heading);
    Simulation sim(s, map, false);
    EXPECT_TRUE(sim.take(id));
    sim.pilot->heading = heading;
    return Drive{std::move(sim), id};
}

Unit& unitOf(Simulation& sim, int64_t id) { return sim.state.units[size_t(*sim.unitIndex(id))]; }

/// Steps of 1/30 s with the keys `walk` held.
void hold(Simulation& sim, Vec2 walk, double seconds) {
    sim.pilot->walk = walk;
    run(sim, seconds);
}

/// The playground's ground, not held level, with its east raised one level
/// (2.4 cells) over a slope `slope` cells wide from x = 0 (the ground's
/// smoothstep: steepest at x = -slope / 2, 1.5 × 2.4 / slope, on the
/// gentle roll of the ground everywhere).
MapDefinition slopeMap(double slope) {
    MapDefinition m = WindowMaps::playground();
    m.playground = std::nullopt;
    m.name += " slope " + std::to_string(slope);
    HeightEdit e;
    e.kind = HeightEdit::Kind::raise;
    e.points = {Vec2(0, -20), Vec2(30, -20), Vec2(30, 20), Vec2(0, 20)};
    e.level = 1;
    e.slope = slope;
    m.heightEdits.push_back(e);
    return m;
}

} // namespace

/// The running gear of every kind, as the models show it: wheels, tracks,
/// legs or the air; only wheels have a turning radius, only vehicles a
/// climb limit.
TEST(drive_runningGearOfEveryKind) {
    using K = UnitKind;
    for (K k : allCases<K>()) {
        const UnitStats& st = Rules::stats(k);
        const RunningGear want = k == K::firefly || k == K::hailstorm || k == K::mule ? RunningGear::wheels
                                 : k == K::longbow                                       ? RunningGear::tracks
                                 : st.air                                                ? RunningGear::air
                                                                                         : RunningGear::legs;
        EXPECT_TRUE(st.gear == want);
        EXPECT_EQ(st.turnRadius > 0, want == RunningGear::wheels);
        EXPECT_EQ(Rules::climb(k).has_value(), want == RunningGear::wheels || want == RunningGear::tracks);
        EXPECT_EQ(Contacts::of(k).has_value(), Rules::climb(k).has_value());
        // Every vehicle is driven as one (W/S, A/D), not walked.
        if (want == RunningGear::wheels || want == RunningGear::tracks) EXPECT_TRUE(Pilot::steers(k));
    }
    EXPECT_EQ(Pilot::turnRadius(K::mule), Rules::muleTurnRadius);
    EXPECT_NEAR(Rules::muleTurnRadius, 1.5 * 3.0, 1e-12);
    EXPECT_TRUE(*Rules::climb(K::mule) < *Rules::climb(K::longbow));
}

/// A wheeled kind does not turn where it stands: A/D alone leave its hull
/// and place as they were.
TEST(drive_wheelsDoNotTurnAtAStandstill) {
    Drive d = drive(UnitKind::mule, Vec2(-10, 0), 0.3);
    hold(d.sim, Vec2(0, 1), 1.0);
    hold(d.sim, Vec2(0, -1), 1.0);
    EXPECT_EQ(unitOf(d.sim, d.me).heading, 0.3);
    EXPECT_EQ(unitOf(d.sim, d.me).position, Vec2(-10, 0));
}

/// Full lock ahead, the Mule drives a circle of its turning radius: its
/// heading turns by the distance rolled over the radius, its middle stays
/// on the circle, and the heading follows the arc's tangent.
TEST(drive_wheelsTurnOnTheirRadius) {
    const double h0 = 0;
    Drive d = drive(UnitKind::mule, Vec2(-10, -5), h0);
    const Vec2 right(-std::sin(h0), std::cos(h0));
    const Vec2 centre = Vec2(-10, -5) + right * Rules::muleTurnRadius;
    double rolled = 0;
    for (int k = 0; k < 90; k++) {
        const Unit before = unitOf(d.sim, d.me);
        hold(d.sim, Vec2(1, 1), 1.0 / 30);
        const Unit& u = unitOf(d.sim, d.me);
        rolled += distance(before.position, u.position);
        // No turn tighter than the radius, step by step.
        EXPECT_TRUE(std::abs(std::remainder(u.heading - before.heading, 2 * pi)) <= distance(before.position, u.position) / Rules::muleTurnRadius + 1e-9);
    }
    const Unit& u = unitOf(d.sim, d.me);
    EXPECT_NEAR(rolled, 3 * Rules::speed(UnitKind::mule), 1e-4);  // (chords of the arc)
    EXPECT_NEAR(std::remainder(u.heading - h0, 2 * pi), rolled / Rules::muleTurnRadius, 1e-3);
    EXPECT_NEAR(distance(u.position, centre), Rules::muleTurnRadius, 1e-3);
    // Tangent to the circle: the heading is square to the way to its centre.
    const Vec2 in = normalize(centre - u.position);
    EXPECT_NEAR(dot(in, Vec2(std::cos(u.heading), std::sin(u.heading))), 0, 1e-3);
}

/// Backing up with D held, the nose swings left (a car's geometry): the
/// same arc's turn the other way, at the back pace.
TEST(drive_reversingTurnsTheOtherWay) {
    Drive d = drive(UnitKind::mule, Vec2(-10, -5), 0);
    hold(d.sim, Vec2(-1, 1), 2.0);
    const Unit& u = unitOf(d.sim, d.me);
    const double back = 2 * Rules::speed(UnitKind::mule) * Pilot::backPace;
    EXPECT_TRUE(u.position.x < -10);
    EXPECT_NEAR(std::remainder(u.heading, 2 * pi), -back / Rules::muleTurnRadius, 1e-3);
    // Straight back with no steer: no turn at all.
    Drive s = drive(UnitKind::mule, Vec2(-10, -5), 0.5);
    hold(s.sim, Vec2(-1, 0), 1.0);
    EXPECT_EQ(unitOf(s.sim, s.me).heading, 0.5);
}

/// Tracks still turn on the spot: a Longbow pivots on D where it stands.
TEST(drive_tracksPivotInPlace) {
    Drive d = drive(UnitKind::longbow, Vec2(-10, 0), 0);
    hold(d.sim, Vec2(0, 1), 0.5);
    const Unit& u = unitOf(d.sim, d.me);
    EXPECT_NEAR(distance(u.position, Vec2(-10, 0)), 0, 1e-9);
    EXPECT_TRUE(u.heading > 0.5);
}

/// The AI's way to a point behind a wheeled kind: arcs and a three-point
/// turn, never a turn on the spot; it gets there. A Firefly with room, and
/// the Mule with the point inside its turning circle.
TEST(drive_aiWheelsReachATargetBehindWithoutPivoting) {
    for (const auto& [kind, target] : {std::pair{UnitKind::firefly, Vec2(-8, 1)}, std::pair{UnitKind::mule, Vec2(-3, 1)}}) {
        GameState s = bare(2, 0, {}, {}, 1);
        const int64_t id = addUnit(s, kind, Vec2(0, 0), 0, Unit::Task::idle, 0);
        const MapDefinition m = WindowMaps::playground();
        Simulation sim(s, m, false);
        bool arrived = false, backed = false;
        for (int k = 0; k < 30 * 60 && !arrived; k++) {
            Unit u = unitOf(sim, id);
            const Unit before = u;
            arrived = sim.travel(u, target, 0.3, 1.0 / 30);
            const double moved = distance(before.position, u.position);
            // (The arc a step rolls is a hair longer than its chord.)
            EXPECT_TRUE(std::abs(std::remainder(u.heading - before.heading, 2 * pi)) <= 1.001 * moved / Pilot::turnRadius(kind) + 1e-9);
            if (dot(u.position - before.position, Vec2(std::cos(before.heading), std::sin(before.heading))) < -1e-6) backed = true;
            unitOf(sim, id) = u;
        }
        EXPECT_TRUE(arrived);
        EXPECT_TRUE(distance(unitOf(sim, id).position, target) < 0.35);
        // Inside its tightest circle the Mule backs up on the way.
        if (kind == UnitKind::mule) EXPECT_TRUE(backed);
    }
}

/// The body over a plane: its pitch and roll are the plane's slopes along
/// and across its heading, whichever way it faces; on flat ground, level.
TEST(drive_tiltFromAPlane) {
    const double a = 0.3, b = -0.2;
    auto plane = [&](Vec2 p) { return a * p.x + b * p.y + 1; };
    const Contacts c = *Contacts::of(UnitKind::mule);
    const GroundTilt east = groundTilt(plane, Vec2(3, 4), 0, c);
    EXPECT_NEAR(east.pitch, std::atan(a), 1e-12);
    EXPECT_NEAR(east.roll, std::atan(b), 1e-12);  // its right is +y
    EXPECT_NEAR(east.lift, 0, 1e-12);
    const GroundTilt north = groundTilt(plane, Vec2(3, 4), pi / 2, c);
    EXPECT_NEAR(north.pitch, std::atan(b), 1e-12);
    EXPECT_NEAR(north.roll, std::atan(-a), 1e-12);  // its right is -x
    const GroundTilt flat = groundTilt([](Vec2) { return 2.0; }, Vec2(0, 0), 1.0, c);
    EXPECT_TRUE(flat == GroundTilt{});
    // Astride a crest the middle sits lower than the ridge under it.
    const GroundTilt crest = groundTilt([](Vec2 p) { return -std::abs(p.x); }, Vec2(0, 0), 0, c);
    EXPECT_NEAR(crest.pitch, 0, 1e-12);
    EXPECT_TRUE(crest.lift < -0.5);
}

/// Up a gentle slope the Mule climbs; up one steeper than 25° it is
/// refused (it stands, the driver told so), though a Longbow's tracks
/// (35°) take it; down it always goes, and across.
TEST(drive_tooSteepToClimb) {
    // Gentle: 1.5 × 2.4 / 12 = 0.3 at its steepest, 17°.
    {
        Drive d = drive(UnitKind::mule, Vec2(-16, 0), 0, slopeMap(12));
        hold(d.sim, Vec2(1, 0), 16.0);
        EXPECT_TRUE(unitOf(d.sim, d.me).position.x > 1);
        EXPECT_TRUE(!d.sim.pilotSteep);
    }
    // Steep: 1.5 × 2.4 / 5 = 0.72 at its steepest, 36° (the grid's cliffs
    // start past 0.9), 34° under the Mule's wheelbase: too steep for wheels.
    const MapDefinition steep = slopeMap(5);
    const TerrainField field(steep);
    double most = 0;
    for (double x = -6; x <= 1; x += 0.05) most = max(most, (field.height(Vec2(x + 0.05, 0)) - field.height(Vec2(x - 0.05, 0))) / 0.1);
    EXPECT_TRUE(most > std::tan(Rules::wheelsClimb) && most < NavGrid::maxSlope);
    {
        Drive d = drive(UnitKind::mule, Vec2(-9, 0), 0, steep);
        hold(d.sim, Vec2(1, 0), 10.0);
        const Unit& u = unitOf(d.sim, d.me);
        EXPECT_TRUE(u.position.x < -1);
        EXPECT_TRUE(d.sim.pilotSteep && d.sim.state.time - *d.sim.pilotSteep < 0.1);
        // Where it stopped the ground ahead is too steep for it.
        EXPECT_TRUE(d.sim.groundPitch(u, u.position + Vec2(0.05, 0), Vec2(1, 0)) > std::tan(Rules::wheelsClimb) - 0.05);
        // Down it goes.
        const double x = u.position.x;
        hold(d.sim, Vec2(-1, 0), 1.0);
        EXPECT_TRUE(unitOf(d.sim, d.me).position.x < x - 0.5);
    }
    // A slope between the two limits: 1.5 × 2.4 / 6 = 0.6, 31°.
    {
        Drive w = drive(UnitKind::mule, Vec2(-10, 0), 0, slopeMap(6));
        hold(w.sim, Vec2(1, 0), 12.0);
        EXPECT_TRUE(unitOf(w.sim, w.me).position.x < -1);
        Drive t = drive(UnitKind::longbow, Vec2(-10, 0), 0, slopeMap(6));
        hold(t.sim, Vec2(1, 0), 6.0);
        EXPECT_TRUE(unitOf(t.sim, t.me).position.x > 1);
        EXPECT_TRUE(!t.sim.pilotSteep);
    }
    // Down from the top, and across the face: never refused.
    {
        Drive d = drive(UnitKind::mule, Vec2(3, 0), pi, steep);
        hold(d.sim, Vec2(1, 0), 8.0);
        EXPECT_TRUE(unitOf(d.sim, d.me).position.x < -6);
        EXPECT_TRUE(!d.sim.pilotSteep);
        Drive a = drive(UnitKind::mule, Vec2(-2.5, -10), pi / 2, steep);
        hold(a.sim, Vec2(1, 0), 6.0);
        EXPECT_TRUE(unitOf(a.sim, a.me).position.y > -4);
        EXPECT_TRUE(!a.sim.pilotSteep);
    }
    // Legs climb what they can stand on.
    {
        Drive d = drive(UnitKind::prospector, Vec2(-9, 0), 0, steep);
        hold(d.sim, Vec2(1, 0), 4.0);
        EXPECT_TRUE(unitOf(d.sim, d.me).position.x > 1);
    }
}

/// The AI's paths leave ground too steep for the gear out: every way onto
/// the raised ground climbs its face, which a walker takes and wheels do
/// not (34° over 1.5 cells); up a gentle face (17°) wheels go too, on cells none of which
/// is too steep for them.
TEST(drive_aiPathsKeepOffTooSteepGround) {
    const Vec2 from(-12, 0), to(6, 0);
    const uint8_t wheels = NavGrid::steepFor(UnitKind::mule);
    EXPECT_TRUE(wheels == NavGrid::steepWheels && NavGrid::steepFor(UnitKind::longbow) == NavGrid::steepTracks);
    EXPECT_TRUE(NavGrid::steepFor(UnitKind::prospector) == 0 && NavGrid::steepFor(UnitKind::kestrel) == 0);
    {
        const MapDefinition m = slopeMap(5);
        const NavGrid nav(m, TerrainField(m));
        EXPECT_TRUE(nav.path(from, to, 0.3).has_value());
        EXPECT_TRUE(!nav.path(from, to, 0.3, false, wheels).has_value());
    }
    {
        const MapDefinition m = slopeMap(12);
        const NavGrid nav(m, TerrainField(m));
        const auto way = nav.path(from, to, 0.3, false, wheels);
        ASSERT_TRUE(way.has_value());
        Vec2 at = from;
        for (Vec2 p : *way) {
            for (int k = 1; k <= 20; k++) {
                const auto i = nav.index(at + (p - at) * (double(k) / 20));
                ASSERT_TRUE(i.has_value());
                EXPECT_TRUE(!(nav.passable[size_t(*i)] & wheels));
            }
            at = p;
        }
    }
}
