// The Citadel's interior (C1, docs/citadel-interior.md): the hall's shape,
// the door, the shelter, the manned corner guns, from hand-built scenes of a
// few seconds.
#include "Brood.h"
#include "RegionFile.h"
#include "Defence.h"
#include "SimHelpers.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

using namespace ac;
using namespace simtest;

namespace {

/// A complete Citadel of blue's at `at` (the hall on), with no map under it,
/// blue's Prospector at `pilotAt` driven by hand; player 1 is the Brood.
struct Hall {
    Simulation sim;
    int64_t citadel;
    int64_t pilot;
};

constexpr Vec2 kAt{50, 50};

Hall hallScene(Vec2 pilotAt, std::optional<MapDefinition> map = std::nullopt, Vec2 at = kAt) {
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t c = addStructure(s, Structure::Kind::citadel, at, 0);
    const int64_t p = addUnit(s, Unit::Kind::prospector, pilotAt, 0);
    Simulation sim(s, map);
    sim.broodSide = 1;
    EXPECT_TRUE(sim.take(p));
    sim.step(1.0 / 30);
    return Hall{sim, c, p};
}

Unit& unitOf(Simulation& sim, int64_t id) { return sim.state.units[size_t(*sim.unitIndex(id))]; }

Structure& structureOf(Simulation& sim, int64_t id) {
    for (Structure& s : sim.state.structures) if (s.id == id) return s;
    return sim.state.structures.front();
}

/// A bug of the Brood's at `p` that stays where it is.
int64_t bugAt(Simulation& sim, Unit::Kind kind, Vec2 p) {
    const int64_t id = sim.state.nextID++;
    Unit u(id, kind, 1, p, 0, Unit::Task::idle);
    u.bug = Bug{};
    u.bug->site = p;
    u.bug->coming = false;
    u.bug->wanderAt = 1e9;
    sim.state.units.push_back(u);
    sim.reindex();
    return id;
}

/// Steps of 1/30 s with the pilot's keys as they are.
std::vector<GameEvent> steps(Simulation& sim, double seconds) { return run(sim, seconds); }

/// Arms the Citadel and puts the pilot on gun `gun` (it walks to the
/// station first: it stands there).
void manGun(Hall& h, int gun) {
    structureOf(h.sim, h.citadel).marks = TurretMarks{};
    h.sim.step(1.0 / 30);
    unitOf(h.sim, h.pilot).position = Defence::gunStation(kAt, gun);
    EXPECT_TRUE(!h.sim.man(h.pilot, h.citadel, gun));
}

} // namespace

TEST(footprint_citadelHallIsHollow) {
    {
        const CitadelHall hall(true);
        EXPECT_EQ(Rules::radius(StructureKind::citadel), 6.0);
        const Footprint& f = Rules::footprint(StructureKind::citadel);
        EXPECT_EQ(f.radius, 6.0);
        EXPECT_EQ(f.walls.size(), size_t(6));
        ASSERT_TRUE(f.door && f.interior && f.roof);
        // The hall is open, the walls are not.
        EXPECT_TRUE(!f.blocks({0, 0}, false, 0.3));
        EXPECT_TRUE(!f.blocks({4.4, -4.4}, false, 0.3));
        EXPECT_TRUE(f.blocks({5.5, 0}, false, 0));
        EXPECT_TRUE(f.blocks({0, -5.5}, true, 0));
        EXPECT_TRUE(f.blocks({-5.5, 3}, true, 0));
        // Grown by a pilot's reach, the hall's edge blocks too.
        EXPECT_TRUE(f.blocks({4.8, 0}, false, 0.3));
        // The doorway: shut, it blocks; open, it lets through, but not its jambs.
        EXPECT_TRUE(f.blocks({0, 5.5}, false, 0));
        EXPECT_TRUE(!f.blocks({0, 5.5}, true, 0.3));
        EXPECT_TRUE(f.blocks({1.4, 5.5}, true, 0.3));
        EXPECT_TRUE(f.blocks({-2, 5.5}, true, 0));
        // Inside is the hall only: the doorway and the walls are outside.
        EXPECT_TRUE(f.inside({0, 0}));
        EXPECT_TRUE(f.inside({-4.9, 4.9}));
        EXPECT_TRUE(!f.inside({0, 5.5}));
        EXPECT_TRUE(!f.inside({5.5, 0}));
        EXPECT_TRUE(!f.inside({0, 7}));
        // The heights: the walls to the deck, the hall 6, the roof 6 to 7.
        EXPECT_EQ(f.interior->z1, 6.0);
        EXPECT_EQ(f.roof->z0, 6.0);
        EXPECT_EQ(f.roof->z1, 7.0);
        EXPECT_EQ(f.door->z1, 3.0);
        EXPECT_EQ(Defence::cornerGun({0, 0}, 0), Vec2(5.5, 5.5));
        EXPECT_EQ(Defence::gunStation({10, 10}, 2), Vec2(6, 6));
        // The gun's eye over the corner tower's mount, a little back along the aim.
        const Vec3 eye = Defence::gunEye({0, 0}, 0, 0);
        EXPECT_NEAR(eye.x, 5.1, 1e-12);
        EXPECT_NEAR(eye.y, 5.5, 1e-12);
        EXPECT_NEAR(eye.z, 10.2, 1e-12);
        EXPECT_EQ(Rules::height(StructureKind::citadel), 12.3);
        // Every other kind is its whole square, no hall.
        const Footprint& d = Rules::footprint(StructureKind::derrick);
        EXPECT_TRUE(d.blocks({0, 0}, true, 0) && !d.door && !d.inside({0, 0}));
    }
    {
        // The strategy game's Citadel: solid 5x5, no hall.
        const CitadelHall hall(false);
        const Footprint& f = Rules::footprint(StructureKind::citadel);
        EXPECT_EQ(f.radius, 2.5);
        EXPECT_TRUE(f.blocks({0, 0}, true, 0));
        EXPECT_TRUE(!f.door && !f.inside({0, 0}));
        EXPECT_EQ(Defence::cornerGun({0, 0}, 0), Vec2(2, 2));
    }
}

TEST(gun_aimClampsAtTheArcsHardStops) {
    // Gun 0 rests at 45 degrees; its arc is 110 either side.
    const double rest = Defence::cornerAngle(0);
    EXPECT_NEAR(Defence::clampAim(0, rest + 0.5), rest + 0.5, 1e-12);
    EXPECT_NEAR(Defence::clampAim(0, rest + 2.5), rest + Defence::gunArcHalf, 1e-12);
    EXPECT_NEAR(Defence::clampAim(0, rest - 2.5), rest - Defence::gunArcHalf, 1e-12);
    // Straight back at the hull: a stop, never through.
    const double back = Defence::clampAim(0, rest + pi - 0.01);
    EXPECT_NEAR(back, std::remainder(rest + Defence::gunArcHalf, 2 * pi), 1e-12);
}

TEST(pilot_walksInThroughTheOpenDoorOnly) {
    const CitadelHall on(true);
    // Outside the +y face, in front of the doorway, facing in.
    Hall h = hallScene(kAt + Vec2(0, 8));
    h.sim.pilot->heading = -pi / 2;
    h.sim.pilot->walk = Vec2(1, 0);
    steps(h.sim, 2);
    // The shut door stops it at the +y face.
    const Unit& at = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(at.position.y > kAt.y + 6);
    EXPECT_TRUE(!h.sim.sheltered(at));
    // Opened from outside, it walks in once the door is open enough.
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
    steps(h.sim, 1);
    EXPECT_TRUE(!Simulation::doorOpen(structureOf(h.sim, h.citadel)));
    EXPECT_TRUE(unitOf(h.sim, h.pilot).position.y > kAt.y + 6);
    steps(h.sim, 3);
    EXPECT_TRUE(Simulation::doorOpen(structureOf(h.sim, h.citadel)));
    EXPECT_TRUE(unitOf(h.sim, h.pilot).position.y < kAt.y + 5);
    EXPECT_TRUE(h.sim.sheltered(unitOf(h.sim, h.pilot)));
    // Never through a wall: beside the doorway it stops at the face, open or not.
    Hall w = hallScene(kAt + Vec2(4, 8));
    structureOf(w.sim, w.citadel).door = Door{1, true};
    w.sim.pilot->heading = -pi / 2;
    w.sim.pilot->walk = Vec2(1, 0);
    steps(w.sim, 3);
    EXPECT_TRUE(unitOf(w.sim, w.pilot).position.y > kAt.y + 6);
    // From inside it walks the hall but not out through the -x wall.
    Hall in = hallScene(kAt);
    in.sim.pilot->heading = pi;
    in.sim.pilot->walk = Vec2(1, 0);
    steps(in.sim, 3);
    const double x = unitOf(in.sim, in.pilot).position.x;
    EXPECT_TRUE(x < kAt.x - 4 && x > kAt.x - 5);
    // The strategy game's Citadel is solid.
    const CitadelHall off(false);
    Hall solid = hallScene(kAt + Vec2(0, 6));
    structureOf(solid.sim, solid.citadel).door = std::nullopt;
    solid.sim.pilot->heading = -pi / 2;
    solid.sim.pilot->walk = Vec2(1, 0);
    steps(solid.sim, 3);
    EXPECT_TRUE(unitOf(solid.sim, solid.pilot).position.y > kAt.y + 2.5);
    EXPECT_TRUE(!structureOf(solid.sim, solid.citadel).door);
}

TEST(pilot_isPushedNowhereInsideTheHall) {
    const CitadelHall on(true);
    const MapDefinition& map = homeMap();
    // Blue's start, where its Citadel stands: the open ground there.
    const Vec2 at = map.bases.front().center;
    Hall h = hallScene(at + Vec2(-3, 2), map, at);
    ASSERT_TRUE(h.sim.nav.has_value());
    // The grid blocks the whole square: no path for anyone else goes in.
    EXPECT_TRUE(!h.sim.nav->walkable(at));
    // A building going up elsewhere rebuilds the grid; the pilot in the
    // hall stays where it stands, and so does one in the doorway.
    const Vec2 stood = unitOf(h.sim, h.pilot).position;
    h.sim.state.structures.push_back(Structure(h.sim.state.nextID++, Structure::Kind::habDome, 0, at + Vec2(14, 0)));
    steps(h.sim, 0.5);
    EXPECT_EQ(unitOf(h.sim, h.pilot).position, stood);
    structureOf(h.sim, h.citadel).door = Door{1, true};
    unitOf(h.sim, h.pilot).position = at + Vec2(0, 5.5);
    h.sim.state.structures.push_back(Structure(h.sim.state.nextID++, Structure::Kind::habDome, 0, at + Vec2(-14, 0)));
    steps(h.sim, 0.5);
    EXPECT_EQ(unitOf(h.sim, h.pilot).position, at + Vec2(0, 5.5));
    // Handed back, it is not killed as trapped in there.
    unitOf(h.sim, h.pilot).position = at;
    EXPECT_TRUE(h.sim.take(std::nullopt));
    unitOf(h.sim, h.pilot).task = Unit::Task::idle;
    steps(h.sim, Simulation::trappedTime + 2);
    EXPECT_TRUE(hp(h.sim, h.pilot) > 0);
}

TEST(brood_neverEntersAndNeverTargetsASheltered) {
    const CitadelHall on(true);
    const MapDefinition& map = homeMap();
    const Vec2 at = map.bases.front().center;
    Hall h = hallScene(at + Vec2(2, -2), map, at);
    ASSERT_TRUE(h.sim.nav.has_value());
    // A* never ends inside the square.
    const auto path = h.sim.nav->path(at + Vec2(0, 12), at, 0);
    if (path && !path->empty()) EXPECT_TRUE(!NavGrid::insideStructure(path->back(), structureOf(h.sim, h.citadel), 0));
    // Sheltered: no target, and a hit does nothing.
    const Unit& u = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(h.sim.sheltered(u));
    EXPECT_TRUE(!h.sim.target(h.pilot));
    h.sim.hit(h.pilot, 30);
    EXPECT_EQ(hp(h.sim, h.pilot), Rules::hp(Unit::Kind::prospector));
    // A Matriarch's slam takes the Citadel, never the pilot inside.
    const int64_t queen = bugAt(h.sim, Unit::Kind::matriarch, at + Vec2(0, -9.5));
    unitOf(h.sim, queen).heading = pi / 2;
    unitOf(h.sim, h.pilot).position = at + Vec2(0, -4.5);
    const auto struck = h.sim.slamStrikes(unitOf(h.sim, queen));
    EXPECT_TRUE(std::find(struck.begin(), struck.end(), h.pilot) == struck.end());
    EXPECT_TRUE(std::find(struck.begin(), struck.end(), h.citadel) != struck.end());
    // Shardlings at the wall bite the Citadel, never the pilot behind it.
    std::erase_if(h.sim.state.units, [&](const Unit& v) { return v.id == queen; });
    h.sim.reindex();
    unitOf(h.sim, h.pilot).position = at + Vec2(4.5, 0);
    bugAt(h.sim, Unit::Kind::shardling, at + Vec2(6.6, 0));
    bugAt(h.sim, Unit::Kind::shardling, at + Vec2(6.6, 1));
    steps(h.sim, 4);
    EXPECT_EQ(hp(h.sim, h.pilot), Rules::hp(Unit::Kind::prospector));
    // Out in the open the same pilot is a target.
    unitOf(h.sim, h.pilot).position = at + Vec2(0, 8);
    EXPECT_TRUE(!h.sim.sheltered(unitOf(h.sim, h.pilot)));
    EXPECT_TRUE(h.sim.target(h.pilot).has_value());
}

TEST(door_closesOnlyWhenTheDoorwayIsClear) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 3));
    // A second unit of blue's, in the doorway.
    const int64_t other = h.sim.state.nextID++;
    h.sim.state.units.push_back(Unit(other, Unit::Kind::ranger, 0, kAt + Vec2(0.5, 5.5), 0, Unit::Task::idle));
    h.sim.reindex();
    structureOf(h.sim, h.citadel).door = Door{1, true};
    // Not from outside; not with someone in the doorway.
    const Unit outside(999, Unit::Kind::prospector, 0, kAt + Vec2(0, 7.5), 0, Unit::Task::idle);
    EXPECT_EQ(h.sim.doorRefusal(structureOf(h.sim, h.citadel), false, outside), std::optional<std::string>("Close it from inside"));
    EXPECT_EQ(h.sim.door(h.pilot, h.citadel, false), std::optional<std::string>("Someone is in the doorway"));
    // One grown by its radius into the doorway still counts.
    unitOf(h.sim, other).position = kAt + Vec2(0, 6.3);
    EXPECT_EQ(h.sim.door(h.pilot, h.citadel, false), std::optional<std::string>("Someone is in the doorway"));
    // Clear: it closes, in `doorSeconds`.
    unitOf(h.sim, other).position = kAt + Vec2(0, 9);
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, false));
    steps(h.sim, 1);
    EXPECT_TRUE(!Simulation::doorOpen(structureOf(h.sim, h.citadel)));
    steps(h.sim, Rules::doorSeconds);
    EXPECT_EQ(structureOf(h.sim, h.citadel).door->open, 0.0);
    // Opening: from either side within reach, not from afar.
    EXPECT_TRUE(!h.sim.doorRefusal(structureOf(h.sim, h.citadel), true, outside));
    const Unit far(998, Unit::Kind::prospector, 0, kAt + Vec2(0, 9), 0, Unit::Task::idle);
    EXPECT_EQ(h.sim.doorRefusal(structureOf(h.sim, h.citadel), true, far), std::optional<std::string>("Too far from the door"));
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
}

TEST(gun_restsWithoutAGunnerAndNeverFires) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    Structure& s = structureOf(h.sim, h.citadel);
    s.marks = TurretMarks{.rate = 3, .damage = 3, .range = 3};
    h.sim.turretBonus = 3;
    const int64_t ground = bugAt(h.sim, Unit::Kind::shardling, kAt + Vec2(9, 9));
    const int64_t air = bugAt(h.sim, Unit::Kind::gloamwing, kAt + Vec2(-9, 9));
    unitOf(h.sim, air).bug->height = Brood::hoverHeight;
    h.sim.step(1.0 / 30);
    (*structureOf(h.sim, h.citadel).guns)[0].aim = 1.0;
    (*structureOf(h.sim, h.citadel).guns)[0].pitch = 0.5;
    const auto events = steps(h.sim, 4);
    EXPECT_EQ(shots(events, h.citadel), int64_t(0));
    for (const auto& e : events) EXPECT_TRUE(!e.is<GameEvent::Missed>());
    EXPECT_EQ(hp(h.sim, ground), Rules::hp(Unit::Kind::shardling));
    EXPECT_EQ(hp(h.sim, air), Rules::hp(Unit::Kind::gloamwing));
    for (int i = 0; i < 4; i++) {
        const CornerGun& g = (*structureOf(h.sim, h.citadel).guns)[size_t(i)];
        EXPECT_NEAR(g.aim, Defence::cornerAngle(i), 1e-9);
        EXPECT_EQ(g.pitch, 0.0);
        EXPECT_TRUE(!g.gunner && !g.target);
    }
}

TEST(gun_aimClampedToTheArcAndPitch) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    // Refused out of the hall, away from the station, or on a taken gun.
    structureOf(h.sim, h.citadel).marks = TurretMarks{};
    h.sim.step(1.0 / 30);
    EXPECT_EQ(h.sim.man(h.pilot, h.citadel, 0), std::optional<std::string>("Too far from the gun's station"));
    unitOf(h.sim, h.pilot).position = kAt + Vec2(4, 8);
    EXPECT_EQ(h.sim.man(h.pilot, h.citadel, 0), std::optional<std::string>("Get inside the Citadel first"));
    manGun(h, 0);
    const Unit& u = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(u.task == Unit::Task::atGun && u.structure == h.citadel);
    EXPECT_EQ(u.position, Defence::gunStation(kAt, 0));
    EXPECT_EQ((*structureOf(h.sim, h.citadel).guns)[0].gunner, std::optional<int64_t>(h.pilot));
    // Walk keys do nothing at a gun.
    h.sim.pilot->walk = Vec2(1, 0);
    // Looking back over the hull: the gun stops at its arc's edge.
    const double rest = Defence::cornerAngle(0);
    h.sim.pilot->heading = rest + pi - 0.2;
    h.sim.pilot->pitch = 1.4;
    steps(h.sim, 0.5);
    CornerGun g = (*structureOf(h.sim, h.citadel).guns)[0];
    EXPECT_NEAR(g.aim, std::remainder(rest + Defence::gunArcHalf, 2 * pi), 1e-9);
    EXPECT_EQ(g.pitch, Defence::gunPitchMax);
    EXPECT_EQ(unitOf(h.sim, h.pilot).position, Defence::gunStation(kAt, 0));
    h.sim.pilot->heading = rest - pi + 0.2;
    h.sim.pilot->pitch = -1.4;
    steps(h.sim, 0.1);
    g = (*structureOf(h.sim, h.citadel).guns)[0];
    EXPECT_NEAR(g.aim, std::remainder(rest - Defence::gunArcHalf, 2 * pi), 1e-9);
    EXPECT_EQ(g.pitch, Defence::gunPitchMin);
    // Within the arc it follows the look exactly.
    h.sim.pilot->heading = rest + 0.7;
    h.sim.pilot->pitch = 0.2;
    steps(h.sim, 0.1);
    g = (*structureOf(h.sim, h.citadel).guns)[0];
    EXPECT_NEAR(g.aim, rest + 0.7, 1e-9);
    EXPECT_EQ(g.pitch, 0.2);
    // A second pilot can't take the same gun; disengaged, the gun is free
    // and turns back to rest.
    const int64_t second = h.sim.state.nextID++;
    h.sim.state.units.push_back(Unit(second, Unit::Kind::prospector, 0, Defence::gunStation(kAt, 0), 0, Unit::Task::idle));
    h.sim.reindex();
    EXPECT_EQ(h.sim.man(second, h.citadel, 0), std::optional<std::string>("Someone is at that gun"));
    h.sim.unman(h.pilot);
    EXPECT_TRUE(unitOf(h.sim, h.pilot).task != Unit::Task::atGun);
    steps(h.sim, 1);
    g = (*structureOf(h.sim, h.citadel).guns)[0];
    EXPECT_TRUE(!g.gunner);
    EXPECT_NEAR(g.aim, rest, 1e-9);
    // A gunner that dies frees its gun.
    manGun(h, 1);
    unitOf(h.sim, h.pilot).hp = 0;
    steps(h.sim, 0.1);
    EXPECT_TRUE(!(*structureOf(h.sim, h.citadel).guns)[1].gunner);
}

TEST(gun_firesAtTheCrosshairFoeInRange) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    const Vec2 mount = Defence::cornerGun(kAt, 0);
    const double rest = Defence::cornerAngle(0);
    const Vec2 out(std::cos(rest), std::sin(rest));
    const int64_t near = bugAt(h.sim, Unit::Kind::shardling, mount + out * 5);
    const int64_t far = bugAt(h.sim, Unit::Kind::shardling, mount + out * 14);
    // On the crosshair, in range, along the aim: the trigger held fires.
    h.sim.pilot->heading = rest;
    h.sim.pilot->hold = true;
    h.sim.pilot->crosshair = Pilot::Crosshair{Pilot::Crosshair::On{near}};
    auto events = steps(h.sim, 0.5);
    EXPECT_TRUE(shots(events, h.citadel) >= 2);
    EXPECT_TRUE(hp(h.sim, near) < Rules::hp(Unit::Kind::shardling));
    EXPECT_EQ((*structureOf(h.sim, h.citadel).guns)[0].target, std::optional<int64_t>(near));
    bool told = false;
    for (const auto& b : h.sim.bugHits) told = told || (b.shooter == h.citadel && b.bug == near);
    EXPECT_TRUE(told);
    // Out of range: misses along the aim, from the mount.
    h.sim.pilot->crosshair = Pilot::Crosshair{Pilot::Crosshair::On{far}};
    events = steps(h.sim, 0.5);
    EXPECT_EQ(shots(events, h.citadel), int64_t(0));
    int64_t missed = 0;
    for (const auto& e : events) {
        if (auto m = e.as<GameEvent::Missed>(); m && m->unit == h.citadel) {
            missed++;
            EXPECT_NEAR(std::atan2(m->at.y - mount.y, m->at.x - mount.x), rest, 1e-9);
        }
    }
    EXPECT_TRUE(missed >= 2);
    EXPECT_EQ(hp(h.sim, far), Rules::hp(Unit::Kind::shardling));
    // In range but well off the aim: a miss too.
    const double had = hp(h.sim, near);
    h.sim.pilot->crosshair = Pilot::Crosshair{Pilot::Crosshair::On{near}};
    h.sim.pilot->heading = rest + 1.0;
    events = steps(h.sim, 0.5);
    EXPECT_EQ(shots(events, h.citadel), int64_t(0));
    EXPECT_EQ(hp(h.sim, near), had);
    // The trigger up: nothing.
    h.sim.pilot->heading = rest;
    h.sim.pilot->hold = false;
    h.sim.pilot->act = false;
    events = steps(h.sim, 0.5);
    EXPECT_EQ(shots(events, h.citadel), int64_t(0));
}

TEST(placement_citadelPlacesOnTodaysGround) {
    // The MMO's start region, every 4 cells, wherever the old 5x5 Citadel
    // placed: the 12x12 one is refused for the slope only where the ground
    // under it rises more than 0.2 over 4.5 cells scaled to its 11.5 (the
    // same slope as the 5x5's limit), and it places on spots a flat 0.2
    // would have refused. Something under the bigger square (a rock, a
    // cliff, a deposit) still refuses it.
    std::ifstream in(std::string(AC_REPO_ROOT) + "/unreal/server/regions/start.json");
    ASSERT_TRUE(in.good());
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const region::Parsed parsed = region::parse(json);
    ASSERT_TRUE(parsed.file.has_value());
    const MapDefinition map = region::build(*parsed.file).map;
    Simulation sim(bare(2, 0, {}, {}, 1), map);
    ASSERT_TRUE(sim.field.has_value());
    const double tolerance = 0.2 * 11.5 / 4.5;
    auto spread = [&](Vec2 p) {
        double lo = 1e9, hi = -1e9;
        for (double dx = -5.75; dx <= 5.75; dx += 0.5) {
            for (double dz = -5.75; dz <= 5.75; dz += 0.5) {
                const double h = sim.field->height(p + Vec2(dx, dz));
                lo = std::min(lo, h);
                hi = std::max(hi, h);
            }
        }
        return hi - lo;
    };
    int64_t small = 0, placed = 0, gentle = 0;
    for (double x = map.bounds.minX + 8; x < map.bounds.maxX - 8; x += 4) {
        for (double z = map.bounds.minZ + 8; z < map.bounds.maxZ - 8; z += 4) {
            std::optional<std::string> was, now;
            {
                const CitadelHall off(false);
                was = sim.placementRefusal(Structure::Kind::citadel, Vec2(x, z));
            }
            if (was) continue;
            small++;
            {
                const CitadelHall on(true);
                now = sim.placementRefusal(Structure::Kind::citadel, Vec2(x, z));
            }
            if (now == std::optional<std::string>("Can't build there")) continue;
            const double d = spread(Vec2(x, z));
            EXPECT_EQ(now == std::optional<std::string>("The ground is not level"), d > tolerance);
            placed += !now;
            gentle += !now && d > 0.2;
        }
    }
    EXPECT_TRUE(small > 100);
    EXPECT_TRUE(placed > 100);
    EXPECT_TRUE(gentle > 0);
}

TEST(pilot_walksOverADoodadSwallowedByTheCitadel) {
    const CitadelHall on(true);
    const MapDefinition& map = homeMap();
    // A solid doodad of the map's, a complete Citadel grown over it (as a
    // saved world's), the pilot in the hall where the rock stands.
    std::optional<Doodad> rock;
    for (const Doodad& d : map.doodads) {
        if (!NavGrid::circles(d).empty()) { rock = d; break; }
    }
    ASSERT_TRUE(rock.has_value());
    const Vec2 at = rock->position + Vec2(2, 0);
    Hall h = hallScene(at + Vec2(-3, 0), map, at);
    ASSERT_TRUE(h.sim.nav.has_value());
    const Unit& u = unitOf(h.sim, h.pilot);
    EXPECT_TRUE(h.sim.swallowedByCitadel(rock->position));
    EXPECT_TRUE(h.sim.nav->inRock(rock->position) || !h.sim.nav->standable(rock->position));
    EXPECT_TRUE(h.sim.pilotCanStand(u, rock->position));
    // Without the Citadel the rock is there.
    std::erase_if(h.sim.state.structures, [&](const Structure& st) { return st.id == h.citadel; });
    EXPECT_TRUE(!h.sim.swallowedByCitadel(rock->position));
    EXPECT_TRUE(!h.sim.pilotCanStand(u, rock->position));
}

TEST(pilot_unloadsInTheHallAndAtItsWalls) {
    const CitadelHall on(true);
    // A load dropped off in the hall, and against the outer wall (within
    // `pilotDepositReach` of the Citadel's edge), as at the 5x5's.
    for (const Vec2 at : {kAt + Vec2(0, 3), kAt + Vec2(-6.6, 0)}) {
        Hall h = hallScene(at);
        unitOf(h.sim, h.pilot).carrying = 5;
        const int64_t ore = h.sim.state.players[0].ore;
        steps(h.sim, 0.2);
        EXPECT_EQ(unitOf(h.sim, h.pilot).carrying, int64_t(0));
        EXPECT_TRUE(h.sim.state.players[0].ore > ore);
    }
    // A Prospector trained there starts outside, half a cell past the walls.
    Hall h = hallScene(kAt + Vec2(0, 9));
    const Unit fresh = h.sim.spawnProspector(structureOf(h.sim, h.citadel));
    EXPECT_NEAR(distance(fresh.position, kAt), 6.5, 1e-9);
    EXPECT_TRUE(!h.sim.sheltered(fresh));
}
