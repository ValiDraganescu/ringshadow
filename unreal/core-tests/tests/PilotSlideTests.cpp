// A driven unit walking into what blocks it slides along it
// (`Simulation::pilotSlide`, docs/shooter-feel.md "Third person
// (2026-10-18)"): along a rock's circle, round a building's corner, never
// into either; the Citadel hall's consoles block it, the gunner's station
// does not. Isolated scenes, a few seconds of walking each.

#include "test.h"
#include "CitadelHall.h"
#include "SimHelpers.h"

#include "Act.h"
#include "Defence.h"
#include "Simulation.h"
#include "WindowMaps.h"

#include <cmath>
#include <optional>

namespace {

using namespace ac;

struct Walker {
    Simulation sim;
    int64_t id;
};

/// A small open map, blue alone, its first Prospector driven and stood on
/// open ground with nothing within 6 cells.
std::optional<Walker> openGround() {
    const MapDefinition map = WindowMaps::build(MapChoice{MapStyle::openField, MapSize::small});
    Simulation sim(GameState::new_(map), map);
    int64_t id = -1;
    for (const Unit& u : sim.state.units) {
        if (u.owner == 0 && u.kind == Unit::Kind::prospector) { id = u.id; break; }
    }
    if (id < 0 || !sim.take(id)) return std::nullopt;
    Unit& u = sim.state.units[size_t(*sim.unitIndex(id))];
    auto open = [&](Vec2 p) {
        for (int k = 0; k < 24; k++) {
            for (const double r : {0.0, 2.0, 4.0, 6.0}) {
                if (!sim.pilotCanStand(u, p + Vec2(std::cos(k * pi / 12), std::sin(k * pi / 12)) * r)) return false;
            }
        }
        return true;
    };
    for (double r = 8; r < 60; r += 2) {
        for (int k = 0; k < 24; k++) {
            const Vec2 p = u.position + Vec2(std::cos(k * pi / 12), std::sin(k * pi / 12)) * r;
            if (open(p)) {
                u.position = p;
                return Walker{sim, id};
            }
        }
    }
    return std::nullopt;
}

Unit& unitOf(Simulation& sim, int64_t id) { return sim.state.units[size_t(*sim.unitIndex(id))]; }

/// One step's length walking ahead on open ground, at 30 steps a second.
double fullStep(Simulation& sim, int64_t id) {
    Unit copy = unitOf(sim, id);
    const Vec2 from = copy.position;
    sim.pilotWalk(copy, Vec2(1, 0), 1.0 / 30);
    return distance(from, copy.position);
}

} // namespace

/// Walking ahead into a rock 0.3 cells off its centre line, across the
/// world's axes (45°): it slides round the rock's circle, keeping most of
/// its pace along the way it walks, never within the circle grown by the
/// grid's clearance, and comes out past it.
TEST(pilot_slidesAlongARock) {
    auto w = openGround();
    ASSERT_TRUE(w.has_value());
    Simulation& sim = w->sim;
    const Vec2 dir = normalize(Vec2(1, 1)), side(-dir.y, dir.x);
    const Vec2 start = unitOf(sim, w->id).position;
    Doodad rock;
    rock.kind = Doodad::Kind::rock;
    rock.position = start + dir * 3 + side * 0.3;
    sim.placeDoodad(rock);
    const double keep = NavGrid::footprint(Doodad::Kind::rock)[0].second + NavGrid::clearance;
    Unit& u = unitOf(sim, w->id);
    u.heading = std::atan2(dir.y, dir.x);
    const double step = fullStep(sim, w->id);
    ASSERT_TRUE(step > 0.05);
    int contactSteps = 0;
    double contactAlong = 0;
    double nearest = 1e9;
    const int n = static_cast<int>(6.0 / step);  // 6 cells' worth of steps
    for (int k = 0; k < n; k++) {
        const Vec2 from = u.position;
        sim.pilotWalk(u, Vec2(1, 0), 1.0 / 30);
        const double gap = distance(u.position, rock.position);
        nearest = std::min(nearest, gap);
        if (gap < keep + 0.25) {
            contactSteps += 1;
            contactAlong += dot(u.position - from, dir);
        }
    }
    EXPECT_TRUE(nearest >= keep - 1e-9);    // never into the rock
    EXPECT_TRUE(contactSteps >= 3);         // it did meet it
    // Along the rock it kept most of its pace in the way it walks.
    EXPECT_TRUE(contactAlong / contactSteps > 0.6 * step);
    // And it is past the rock, not stuck behind it: over the whole walk
    // it went at least 80% of the way it would have on open ground.
    EXPECT_TRUE(dot(u.position - rock.position, dir) > 1.5);
    EXPECT_TRUE(dot(u.position - start, dir) > 0.8 * n * step);
}

/// Walking along a building's face, leaning 30° into it, toward its
/// corner: it slides along the face, rounds the corner and walks on the
/// way it leans, never into the building; in a concave corner it stands.
TEST(pilot_roundsACorner) {
    auto w = openGround();
    ASSERT_TRUE(w.has_value());
    Simulation& sim = w->sim;
    Unit& u = unitOf(sim, w->id);
    // A Hab Dome (2x2) with its -y face 0.32 off the walker's line.
    const double r = Rules::radius(Unit::Kind::prospector) * 0.8;
    const Vec2 dome = u.position + Vec2(3, 1 + r + 0.02);
    sim.state.structures.push_back(Structure(sim.state.nextID++, Structure::Kind::habDome, 0, dome));
    const Structure& hab = sim.state.structures.back();
    ASSERT_TRUE(hab.complete());
    u.heading = pi / 6;  // +x, leaning toward +y (into the face)
    const double step = fullStep(sim, w->id);
    double along = 0;
    int alongFace = 0;
    for (int k = 0; k < 90; k++) {
        const Vec2 from = u.position;
        sim.pilotWalk(u, Vec2(1, 0), 1.0 / 30);
        EXPECT_TRUE(!NavGrid::insideStructure(u.position, hab, r - 1e-9));
        if (u.position.x > dome.x - 1 && u.position.x < dome.x + 1) {
            alongFace += 1;
            along += u.position.x - from.x;
        }
    }
    ASSERT_TRUE(alongFace > 0);
    // Along the face at the pace of the step's share along it (cos 30°).
    EXPECT_TRUE(along / alongFace > 0.8 * std::cos(pi / 6) * step);
    // Round the corner and on past it, up beside the dome's +x face.
    EXPECT_TRUE(u.position.x > dome.x + 1 + r);
    EXPECT_TRUE(u.position.y > dome.y - 1);

    // Into the corner of two buildings side by side (a concave corner):
    // it stands still at it, no step back.
    auto w2 = openGround();
    ASSERT_TRUE(w2.has_value());
    Simulation& s2 = w2->sim;
    Unit& v = unitOf(s2, w2->id);
    const Vec2 a = v.position + Vec2(3, 0), b = a + Vec2(-2, 2);
    s2.state.structures.push_back(Structure(s2.state.nextID++, Structure::Kind::habDome, 0, a));
    s2.state.structures.push_back(Structure(s2.state.nextID++, Structure::Kind::habDome, 0, b));
    // Into the inside corner at (a.x - 1, a.y + 1), along the diagonal.
    v.heading = std::atan2(1, 1);
    v.position = a + Vec2(-1, 1) - normalize(Vec2(1, 1)) * 2;
    Vec2 last = v.position;
    double back = 0;
    for (int k = 0; k < 60; k++) {
        s2.pilotWalk(v, Vec2(1, 0), 1.0 / 30);
        back = std::min(back, dot(v.position - last, normalize(Vec2(1, 1))));
        last = v.position;
    }
    EXPECT_TRUE(back >= -1e-12);
    for (const Structure& st : s2.state.structures) EXPECT_TRUE(!NavGrid::insideStructure(v.position, st, r - 1e-9));
}

/// The hall's gun consoles block the walk (grown by the walker's radius
/// as walls are); every gunner's station is free, and walking from it
/// into its console stops at the console.
TEST(pilot_standsAtEveryStationButNotInTheConsole) {
    const simtest::CitadelHall on(true);
    const Vec2 at(50, 50);
    GameState s = simtest::bare(2, 0, {}, {}, 1);
    simtest::addStructure(s, Structure::Kind::citadel, at, 0);
    const int64_t p = simtest::addUnit(s, Unit::Kind::prospector, at, 0);
    Simulation sim(s, std::nullopt);
    ASSERT_TRUE(sim.take(p));
    const Footprint& f = Rules::footprint(Structure::Kind::citadel);
    int consoles = 0;
    for (const Block& b : f.furniture) consoles += b.z0 <= 0.1 && b.half.x > 0.3;
    EXPECT_EQ(consoles, 4);
    const double r = Rules::radius(Unit::Kind::prospector) * 0.8;
    for (int k = 0; k < 4; k++) {
        const Vec2 station = Defence::gunStation(at, k);
        const Vec2 corner = normalize(Defence::cornerGun(at, k) - at);
        Unit& u = unitOf(sim, p);
        EXPECT_TRUE(sim.pilotCanStand(u, station));
        EXPECT_TRUE(sim.pilotCanStand(u, station + corner * 0.15));
        EXPECT_TRUE(!sim.pilotCanStand(u, station + corner * 0.3));
        EXPECT_TRUE(!sim.pilotCanStand(u, station + corner * 0.9));  // in the console
        // From the station into the console: it stops short, never in it.
        u.position = station;
        u.heading = std::atan2(corner.y, corner.x);
        for (int j = 0; j < 30; j++) {
            sim.pilotWalk(u, Vec2(1, 0), 1.0 / 30);
            for (const Block& b : f.furniture) {
                if (b.z0 <= 0.1) EXPECT_TRUE(!b.contains(u.position - at, r - 1e-9));
            }
        }
        EXPECT_TRUE(distance(u.position, station) < 0.25);
        // Still within reach of the console to man the gun.
        EXPECT_TRUE(Act::gap(u, Act::console(Structure(-1, Structure::Kind::citadel, 0, at), k)) <= Act::reach);
    }
}

/// Handed back up against a rock (where the AI's grid has no ground), the
/// unit steps onto the nearest free ground, a short step, so the AI walks
/// on from there.
TEST(pilot_handedBackBesideARockStepsOntoTheGrid) {
    auto w = openGround();
    ASSERT_TRUE(w.has_value());
    Simulation& sim = w->sim;
    const Vec2 start = unitOf(sim, w->id).position;
    Doodad rock;
    rock.kind = Doodad::Kind::rock;
    rock.position = start + Vec2(3, 0);
    sim.placeDoodad(rock);
    // A spot by the rock where the walk lets it stand and the grid has no ground.
    Unit& u = unitOf(sim, w->id);
    const double keep = NavGrid::footprint(Doodad::Kind::rock)[0].second + NavGrid::clearance;
    std::optional<Vec2> spot;
    for (int k = 0; k < 64 && !spot; k++) {
        const Vec2 p = rock.position + Vec2(std::cos(k * pi / 32), std::sin(k * pi / 32)) * (keep + 0.02);
        if (sim.pilotCanStand(u, p) && !sim.nav->walkable(p)) spot = p;
    }
    ASSERT_TRUE(spot.has_value());
    u.position = *spot;
    const Vec2 at = u.position;
    sim.take(std::nullopt);
    const Unit& v = unitOf(sim, w->id);
    EXPECT_TRUE(sim.nav->walkable(v.position));
    EXPECT_TRUE(distance(v.position, at) < 0.8);
}
