// The act key's boxes (Act.h; C5, docs/acting.md): the gap between a body
// and a box, the call posts off their buildings, and the server's refusals
// never "too far" where the client's reach lets the pilot act.
#include "test.h"

#include "Act.h"
#include "HallScene.h"
#include "Mule.h"
#include "Shuttle.h"
#include "SimHelpers.h"

#include <cmath>
#include <functional>
#include <string>

using namespace ac;
using namespace simtest;

namespace {

/// A Prospector of blue's standing at `p` (not driven).
Unit walker(Vec2 p) {
    Unit u(997, Unit::Kind::prospector, 0, p, 0, Unit::Task::idle);
    u.hp = Rules::stats(Unit::Kind::prospector).hp;
    return u;
}

/// Every point of a grid round `centre` (`step` apart, `span` out) where
/// the body's edge is within the client's reach of `box`.
void inReach(Vec2 centre, const Block& box, const std::function<void(Vec2)>& at, double span = 5, double step = 0.25) {
    for (double x = -span; x <= span; x += step)
        for (double y = -span; y <= span; y += step) {
            const Vec2 p = centre + Vec2(x, y);
            if (Act::gap(walker(p), box) <= Act::reach) at(p);
        }
}

bool tooFar(const std::optional<std::string>& why) {
    return why && (why->rfind("Too far", 0) == 0 || why->rfind("Get to", 0) == 0);
}

} // namespace

TEST(act_gapBetweenBoxes) {
    const Block a{Vec2(0, 0), Vec2(1, 1), 0};
    EXPECT_NEAR(Act::gap(a, Block{Vec2(3, 0), Vec2(1, 1), 0}), 1.0, 1e-12);
    EXPECT_NEAR(Act::gap(a, Block{Vec2(3, 3), Vec2(1, 1), 0}), std::sqrt(2.0), 1e-12);
    // Turned a quarter, the same box: the same gap.
    EXPECT_NEAR(Act::gap(a, Block{Vec2(3, 0), Vec2(1, 1), pi / 2}), 1.0, 1e-12);
    // A diamond's corner toward the box's face.
    EXPECT_NEAR(Act::gap(a, Block{Vec2(3, 0), Vec2(1, 1), pi / 4}), 2 - std::sqrt(2.0), 1e-12);
    // Overlapping, or one inside the other: 0.
    EXPECT_EQ(Act::gap(a, Block{Vec2(1.5, 0.5), Vec2(1, 1), 0.3}), 0.0);
    EXPECT_EQ(Act::gap(a, Block{Vec2(0, 0), Vec2(0.1, 0.1), 0}), 0.0);
    // A cross: no corner inside the other, still overlapping.
    EXPECT_EQ(Act::gap(Block{Vec2(0, 0), Vec2(3, 0.2), 0}, Block{Vec2(0, 0), Vec2(0.2, 3), 0}), 0.0);
    // A Prospector's body: its circle's square; a Mule's: its box along its heading.
    const Unit p = walker(Vec2(10, 10));
    EXPECT_NEAR(Act::gap(p, Block{Vec2(12, 10), Vec2(0.5, 0.5), 0}), 2 - 0.5 - Rules::radius(UnitKind::prospector), 1e-12);
    Unit m(5, UnitKind::mule, 0, Vec2(0, 0), pi / 2, Unit::Task::idle);
    EXPECT_NEAR(Act::gap(m, Block{Vec2(0, 3), Vec2(0.5, 0.5), 0}), 3 - 0.5 - Mule::length / 2, 1e-12);
    EXPECT_NEAR(Act::gap(m, Block{Vec2(3, 0), Vec2(0.5, 0.5), 0}), 3 - 0.5 - Mule::width / 2, 1e-12);
}

TEST(act_callPostsStandOffTheirBuildings) {
    for (const StructureKind k : {StructureKind::landingPad, StructureKind::foundry}) {
        const Structure s(-1, k, 0, Vec2(40, 40));
        const Block post = k == StructureKind::landingPad ? Act::padPost(s) : Act::foundryPost(s);
        const Footprint& f = Rules::footprint(k);
        // Off the building's slab, at its front (+y), a short reach from it.
        for (const Slab& w : f.walls) EXPECT_TRUE(Act::gap(Block{s.position + (w.min + w.max) * 0.5, (w.max - w.min) * 0.5, 0}, post) > 0.05);
        EXPECT_TRUE(post.centre.y > s.position.y + Rules::radius(k));
        EXPECT_TRUE(post.z1 > 1.0 && post.z0 > 0.5);
    }
    // The post stands solid to a pilot's walk.
    {
        GameState g = bare(2, 0, {}, {}, 1);
        addStructure(g, StructureKind::foundry, Vec2(40, 40), 0);
        const int64_t p = addUnit(g, Unit::Kind::prospector, Vec2(40, 46), 0);
        Simulation sim(g, std::nullopt);
        const Block foot = *Act::postFoot(sim.state.structures.front());
        const Unit& u = sim.state.units[size_t(*sim.unitIndex(p))];
        EXPECT_TRUE(!sim.pilotCanStand(u, foot.centre));
        EXPECT_TRUE(sim.pilotCanStand(u, foot.centre + Vec2(0, 0.7)));
    }
    // The Foundry's: clear of the Mule rolling out of its door.
    const Structure foundry(-1, StructureKind::foundry, 0, Vec2(40, 40));
    Unit m(5, UnitKind::mule, 0, Mule::door(foundry.position), Mule::doorHeading, Unit::Task::idle);
    EXPECT_TRUE(Act::gap(m, Act::foundryPost(foundry)) > 0.1);
}

/// Wherever the client lets the pilot act (the body's edge within
/// `Act::reach` of the box), the server's refusal is never about the reach.
TEST(act_clientReachIsNeverRefusedForDistance) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt + Vec2(0, 9));
    Structure& c = structureOf(h.sim, h.citadel);
    int door = 0, gun = 0;
    // The door: its leaf and both panels, from either side.
    for (const Block& box : {*Act::doorLeaf(c), *Act::doorPanel(c, false), *Act::doorPanel(c, true)}) {
        inReach(box.centre, box, [&](Vec2 p) {
            const Unit u = walker(p);
            EXPECT_TRUE(!tooFar(h.sim.doorRefusal(c, true, u)));
            door++;
        });
    }
    // The gun consoles, from the hall.
    for (int k = 0; k < 4; k++) {
        const Block box = Act::console(c, k);
        inReach(box.centre, box, [&](Vec2 p) {
            if (!Rules::footprint(c.kind).interior->contains(p - c.position, -0.4)) return;
            Hall t = h;
            unitOf(t.sim, t.pilot).position = p;
            const auto why = t.sim.man(t.pilot, t.citadel, k);
            EXPECT_TRUE(!tooFar(why));
            gun++;
        });
    }
    EXPECT_TRUE(door > 50 && gun > 20);
    // The Mule's perch and hopper.
    const Unit mule = h.sim.freshUnit(UnitKind::mule, h.sim.state.nextID, 0, kAt + Vec2(0, 18), 0.7, Unit::Task::idle);
    int mules = 0;
    for (const bool perch : {true, false}) {
        const Block box = perch ? Act::perch(mule) : Act::hopper(mule);
        inReach(box.centre, box, [&](Vec2 p) {
            Unit u = walker(p);
            u.carrying = 10;
            EXPECT_TRUE(!tooFar(perch ? h.sim.muleBoardRefusal(u, mule) : h.sim.muleFillRefusal(u, mule)));
            mules++;
        });
    }
    EXPECT_TRUE(mules > 50);
    // Out past the server's reach of every box of the door: too far.
    const Unit far = walker(c.position + Vec2(0, Rules::footprint(c.kind).door->max.y + Act::serverReach + 0.5));
    EXPECT_EQ(h.sim.doorRefusal(c, true, far), std::optional<std::string>("Too far from the door"));
}

TEST(act_shipButtonsInReachAreNeverRefusedForDistance) {
    GameState s = bare(2, 0, {}, {}, 1);
    Simulation sim(s, std::nullopt);
    const int64_t id = sim.callShuttle(Vec2(50, 50), 0.6);
    Shuttle& ship = *sim.shuttle(id);
    ship.stage = Shuttle::Stage::ground;
    ship.ramp = Door{0, false};
    int n = 0;
    for (const bool ramp : {true, false}) {
        const Block box = ramp ? Act::rampPost(ship) : Act::takeOff(ship);
        inReach(box.centre, box, [&](Vec2 p) {
            if (!ShuttleBay::inBay(ShuttleBay::local(ship, p))) return;
            const Unit u = walker(p);
            EXPECT_TRUE(!tooFar(ramp ? sim.rampRefusal(ship, u, true) : sim.takeOffRefusal(ship, u)));
            n++;
        });
    }
    EXPECT_TRUE(n > 20);
}
