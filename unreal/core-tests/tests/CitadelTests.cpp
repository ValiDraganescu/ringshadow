// The Citadel's interior (C1, docs/citadel-interior.md): the hall's shape,
// the door, the shelter, the manned corner guns, from hand-built scenes of a
// few seconds.
#include "Brood.h"
#include "RegionFile.h"
#include "Defence.h"
#include "HallScene.h"
#include "SimHelpers.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

using namespace ac;
using namespace simtest;

namespace {

/// The MMO's start region, built.
const MapDefinition& startRegion() {
    static const MapDefinition m = [] {
        std::ifstream in(std::string(AC_REPO_ROOT) + "/unreal/server/regions/start.json");
        const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        const region::Parsed parsed = region::parse(json);
        return parsed.file ? region::build(*parsed.file).map : MapDefinition{};
    }();
    return m;
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
        // C2: the mounts on brackets over the corners, 0.2 outside the walls.
        EXPECT_EQ(Defence::cornerGun({0, 0}, 0), Vec2(6.2, 6.2));
        EXPECT_EQ(Defence::gunStation({10, 10}, 2), Vec2(6, 6));
        // The gun's eye rides the cradle: level, it stands 0.256 back along
        // the aim and 0.946 over the mount's top.
        const Vec3 eye = Defence::gunEye({0, 0}, 0, 0);
        EXPECT_NEAR(eye.x, 6.2 - 0.256, 1e-12);
        EXPECT_NEAR(eye.y, 6.2, 1e-12);
        EXPECT_NEAR(eye.z, Defence::mountHeight() + 0.946, 1e-12);
        EXPECT_EQ(Defence::mountHeight(), 9.0);
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
    // Not with someone in the doorway, from either side (C4: outside too).
    const Unit outside(999, Unit::Kind::prospector, 0, kAt + Vec2(0, 7.5), 0, Unit::Task::idle);
    EXPECT_EQ(h.sim.doorRefusal(structureOf(h.sim, h.citadel), false, outside), std::optional<std::string>("Someone is in the doorway"));
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
    const Unit far(998, Unit::Kind::prospector, 0, kAt + Vec2(0, 9.2), 0, Unit::Task::idle);
    EXPECT_EQ(h.sim.doorRefusal(structureOf(h.sim, h.citadel), true, far), std::optional<std::string>("Too far from the door"));
    EXPECT_EQ(h.sim.doorRefusal(structureOf(h.sim, h.citadel), false, far), std::optional<std::string>("Too far from the door"));
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
}

// C4: the door opens and closes from either side within `doorReach` (the
// foot of the ramp outside, the hall's front third inside), never from
// deep in the hall or far out on the apron.
TEST(door_opensAndClosesFromEitherSide) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 3));
    EXPECT_EQ(Rules::doorReach, 3.0);
    auto refusal = [&](Vec2 local, bool open) {
        const Unit u(997, Unit::Kind::prospector, 0, kAt + local, 0, Unit::Task::idle);
        return h.sim.doorRefusal(structureOf(h.sim, h.citadel), open, u);
    };
    for (const Vec2 at : {Vec2(0, 7.2), Vec2(0, 8.9), Vec2(-2.5, 7), Vec2(0, 3), Vec2(0, 2.1), Vec2(-2.5, 3.5)}) {
        EXPECT_TRUE(!refusal(at, true));
        EXPECT_TRUE(!refusal(at, false));
    }
    for (const Vec2 at : {Vec2(0, 9.2), Vec2(0, 1.8), Vec2(0, -2), Vec2(-4.9, 4)}) {
        EXPECT_EQ(refusal(at, true), std::optional<std::string>("Too far from the door"));
        EXPECT_EQ(refusal(at, false), std::optional<std::string>("Too far from the door"));
    }
    // The pilot walks it through: open, close, open from outside; in, close from inside.
    auto stand = [&](Vec2 local) { unitOf(h.sim, h.pilot).position = kAt + local; };
    auto settle = [&] { steps(h.sim, Rules::doorSeconds + 0.1); };
    stand(Vec2(0, 7.5));
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
    settle();
    EXPECT_TRUE(Simulation::doorOpen(structureOf(h.sim, h.citadel)));
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, false));
    settle();
    EXPECT_EQ(structureOf(h.sim, h.citadel).door->open, 0.0);
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
    settle();
    EXPECT_TRUE(Simulation::doorOpen(structureOf(h.sim, h.citadel)));
    stand(Vec2(0, 3));
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, false));
    settle();
    EXPECT_EQ(structureOf(h.sim, h.citadel).door->open, 0.0);
    EXPECT_TRUE(!h.sim.door(h.pilot, h.citadel, true));
    settle();
    EXPECT_TRUE(Simulation::doorOpen(structureOf(h.sim, h.citadel)));
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
    for (const auto& e : events) EXPECT_TRUE(!e.is<GameEvent::Missed>() && !e.is<GameEvent::Fired>());
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
    EXPECT_NEAR(Defence::gunPitchMin, -70.0 * pi / 180.0, 1e-12);
    // C2: 135° either side, along both its walls.
    EXPECT_NEAR(Defence::gunArcHalf, 135.0 * pi / 180.0, 1e-12);
    // C2: steeply down (−57°, under the old −20° stop) it follows the look.
    h.sim.pilot->pitch = -1.0;
    steps(h.sim, 0.1);
    EXPECT_EQ((*structureOf(h.sim, h.citadel).guns)[0].pitch, -1.0);
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

TEST(gun_eyeRidesTheCradle) {
    const CitadelHall on(true);
    // At level, the lowest and the highest pitch, at any yaw of the arc:
    // the eye stays over the mount's top and behind the barrels (0.40 back
    // from the trunnion along them, 0.55 over them).
    const Vec2 at{30, -10};
    for (int i = 0; i < 4; i++) {
        const Vec2 mount = Defence::cornerGun(at, i);
        for (const double off : {-Defence::gunArcHalf, 0.0, 0.7, Defence::gunArcHalf}) {
            const double aim = Defence::cornerAngle(i) + off;
            const Vec2 f(std::cos(aim), std::sin(aim));
            for (const double pitch : {0.0, Defence::gunPitchMin, Defence::gunPitchMax}) {
                const Vec3 e = Defence::gunEye(at, i, aim, pitch);
                EXPECT_TRUE(e.z > Defence::mountHeight());
                // On the line of the aim, seen from above.
                const Vec2 d = Vec2(e.x, e.y) - mount;
                EXPECT_NEAR(d.x * f.y - d.y * f.x, 0.0, 1e-12);
                // In the cradle's frame: behind the trunnion along the barrels, over them.
                const double along = dot(d, f) - Defence::trunnionAlong;
                const double up = e.z - Defence::mountHeight() - Defence::trunnionUp;
                const double back = along * std::cos(pitch) + up * std::sin(pitch);
                const double over = -along * std::sin(pitch) + up * std::cos(pitch);
                EXPECT_NEAR(back, Defence::eyeAlong, 1e-12);
                EXPECT_NEAR(over, Defence::eyeUp, 1e-12);
                EXPECT_TRUE(back < 0);
            }
        }
    }
}

TEST(citadel_bugsNeverTakeItBelowTheFloor) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 12));
    const double full = Rules::hp(StructureKind::citadel), floor = Rules::citadelBugFloor * full;
    // A Shardling biting the wall, and the Matriarch's slam: never below 10%.
    const int64_t bug = bugAt(h.sim, Unit::Kind::shardling, kAt + Vec2(7, 0));
    const Unit biter = unitOf(h.sim, bug);
    h.sim.hit(h.citadel, full * 0.5, false, &biter);
    EXPECT_EQ(structureOf(h.sim, h.citadel).hp, full * 0.5);
    h.sim.hit(h.citadel, full, false, &biter);
    EXPECT_EQ(structureOf(h.sim, h.citadel).hp, floor);
    const int64_t queen = bugAt(h.sim, Unit::Kind::matriarch, kAt + Vec2(0, -10));
    const Unit slam = unitOf(h.sim, queen);
    h.sim.hit(h.citadel, 400, false, &slam);
    EXPECT_EQ(structureOf(h.sim, h.citadel).hp, floor);
    // Steps of a real raid change nothing: it stands, at the floor.
    steps(h.sim, 2);
    EXPECT_TRUE(h.sim.state.structure(h.citadel).has_value());
    EXPECT_EQ(structureOf(h.sim, h.citadel).hp, floor);
    // One already lower takes nothing more from bugs.
    structureOf(h.sim, h.citadel).hp = floor / 2;
    h.sim.hit(h.citadel, 50, false, &biter);
    EXPECT_EQ(structureOf(h.sim, h.citadel).hp, floor / 2);
    // Anything but a bug still destroys it.
    const Unit raider(9999, Unit::Kind::ranger, 1, kAt + Vec2(8, 0), 0, Unit::Task::idle);
    h.sim.hit(h.citadel, full, false, &raider);
    EXPECT_TRUE(structureOf(h.sim, h.citadel).hp <= 0);
    // Another building of the pilots' falls to bugs as before.
    const int64_t dome = h.sim.state.nextID++;
    h.sim.state.structures.push_back(Structure(dome, StructureKind::habDome, 0, kAt + Vec2(14, 0)));
    h.sim.hit(dome, Rules::hp(StructureKind::habDome) + 1, false, &biter);
    EXPECT_TRUE(structureOf(h.sim, dome).hp <= 0);
}

TEST(brood_strikesTheCitadelFromEveryFace) {
    const CitadelHall on(true);
    // Shardlings raiding a 12x12 Citadel on open ground (the nav grid holds
    // them off its square) from +x, -x, +y, -y and two corners all reach a
    // face and bite, the pilot far off.
    const MapDefinition& map = startRegion();
    Simulation probe(bare(2, 0, {}, {}, 1), map);
    ASSERT_TRUE(probe.nav.has_value());
    // Open ground 6 cells round the Citadel's square.
    std::optional<Vec2> spot;
    for (double x = map.bounds.minX + 20; x < map.bounds.maxX - 20 && !spot; x += 2) {
        for (double z = map.bounds.minZ + 20; z < map.bounds.maxZ - 20 && !spot; z += 2) {
            bool open = true;
            for (double dx = -12; dx <= 12 && open; dx += 0.5)
                for (double dz = -12; dz <= 12 && open; dz += 0.5)
                    open = probe.nav->walkable(Vec2(x + dx, z + dz)) && !probe.nav->inRock(Vec2(x + dx, z + dz));
            if (open) spot = Vec2(x, z);
        }
    }
    ASSERT_TRUE(spot.has_value());
    const Vec2 at = *spot;
    Hall h = hallScene(at + Vec2(0, 15), map, at);
    std::vector<int64_t> bugs;
    for (const Vec2 from : {Vec2(11, 3), Vec2(-11, 8), Vec2(4, -11), Vec2(10, 10), Vec2(-10, -10), Vec2(3, 11)}) {
        const int64_t id = bugAt(h.sim, Unit::Kind::shardling, at + from);
        unitOf(h.sim, id).bug->raid = h.citadel;
        unitOf(h.sim, id).bug->siteRadius = 40;
        bugs.push_back(id);
    }
    std::map<int64_t, int64_t> bites;
    std::map<int64_t, int64_t> was;
    for (int k = 0; k < 20 * 30; k++) {
        h.sim.step(1.0 / 30);
        for (const int64_t id : bugs) {
            const auto b = h.sim.state.unit(id);
            if (!b) continue;
            if (b->bug->strikes != was[id]) bites[id]++;
            was[id] = b->bug->strikes;
        }
    }
    EXPECT_TRUE(structureOf(h.sim, h.citadel).hp < Rules::hp(StructureKind::citadel));
    for (const int64_t id : bugs) {
        EXPECT_TRUE(bites[id] >= 5);
        if (bites[id] < 5) {
            const Unit b = *h.sim.state.unit(id);
            std::printf("    bug %lld at (%.2f, %.2f) state %d, %lld bites\n", (long long)id, b.position.x - at.x,
                        b.position.y - at.y, int(b.bug->state), (long long)bites[id]);
        }
    }
}
