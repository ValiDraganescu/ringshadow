// Guns and practice (C2, docs/guns-and-practice.md sections 1, 3 and 4):
// rounds as real projectiles, the minigun's spin-up and heat, the manned
// corner gun with no range gate, the gatling gun's ballistic lead, the
// Target Dummy, blueprints learned once. Hand-built scenes of a few seconds.
#include "Brood.h"
#include "Defence.h"
#include "HallScene.h"
#include "Progression.h"
#include "Session.h"
#include "SimHelpers.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace ac;
using namespace simtest;

namespace {

/// Puts a round of player `owner`'s in flight from `from` at `velocity`,
/// fired now by `shooter`.
int64_t shoot(Simulation& sim, Vec3 from, Vec3 velocity, int64_t owner = 0, int64_t shooter = -1, double damage = 1) {
    const int64_t id = sim.state.nextRound++;
    sim.state.rounds.push_back(Round{id, shooter, owner, from, velocity, damage, sim.state.time});
    return id;
}

/// The events of `kind` in `events`.
template <class E> std::vector<E> all(const std::vector<GameEvent>& events) {
    std::vector<E> out;
    for (const auto& e : events) if (const E* x = e.as<E>()) out.push_back(*x);
    return out;
}

/// Hit points of building `id` (0 when gone).
double structureHp(const Simulation& sim, int64_t id) {
    const auto s = sim.state.structure(id);
    return s ? s->hp : 0;
}

int64_t fired(const std::vector<GameEvent>& events, int64_t shooter) {
    int64_t n = 0;
    for (const auto& f : all<GameEvent::Fired>(events)) n += f.shooter == shooter;
    return n;
}

/// The pitch that puts corner gun `gun`'s rounds (aimed along `aim`) on
/// `p`, z from the ground (no map: the ground is at 0).
double pitchOnto(int gun, double aim, Vec3 p) {
    double pitch = 0;
    for (int k = 0; k < 4; k++) {
        const Vec3 m = Defence::gunMuzzle(kAt, gun, aim, pitch);
        pitch = Rules::ballisticPitch(distance(Vec2(m.x, m.y), Vec2(p.x, p.y)), p.z - m.z).value_or(0);
    }
    return pitch;
}

/// A complete Target Dummy of `owner`'s at `p` (no map under it).
int64_t dummyAt(Simulation& sim, Vec2 p, int64_t owner = 0) {
    const int64_t id = sim.state.nextID++;
    Structure d(id, StructureKind::dummy, owner, p);
    d.hp = Rules::hp(StructureKind::dummy);
    sim.state.structures.push_back(d);
    sim.reindex();
    return id;
}

/// Blue's open ground on the home map and nothing on it; player 1 is the Brood.
struct Open {
    GameState s;
    Vec2 at;
};

Open open() {
    GameState s = GameState::new_(homeMap());
    Vec2 at;
    for (const auto& b : s.structures) if (b.owner == 0) { at = b.position + Vec2(0, 8); break; }
    s.units.clear();
    s.structures.clear();
    return Open{s, at};
}

} // namespace

TEST(round_fliesOnABallisticArcAndDies) {
    // The integrator the client shares: at 30 cells a round drops 0.55.
    const Vec3 v = Rules::roundVelocity(0, 0);
    EXPECT_NEAR(v.x, Rules::roundSpeed, 1e-12);
    const double t30 = 30 / Rules::roundSpeed;
    EXPECT_NEAR(Rules::roundAt(Vec3(0, 0, 0), v, t30).z, -0.5 * Rules::roundGravity * t30 * t30, 1e-12);
    EXPECT_NEAR(Rules::roundAt(Vec3(0, 0, 0), v, t30).z, -0.544, 0.001);
    // The ballistic solve lands where it was asked to.
    for (const auto& [ahead, rise] : std::vector<std::pair<double, double>>{{30, 0}, {80, -8}, {50, 12}, {150, 0}}) {
        const auto p = Rules::ballisticPitch(ahead, rise);
        ASSERT_TRUE(p.has_value());
        const Vec3 w = Rules::roundVelocity(0, *p);
        const Vec3 at = Rules::roundAt(Vec3(0, 0, 0), w, ahead / w.x);
        EXPECT_NEAR(at.z, rise, 1e-9);
    }
    // Past its reach (v²/g, about 826 cells level) there is no solve.
    EXPECT_TRUE(!Rules::ballisticPitch(900, 0));

    // In the sim, high in an empty sky: each step it is exactly on the
    // parabola from its muzzle, and at `roundLife` it is gone, with no Impact.
    GameState s = bare(1, 0, {}, {}, 1);
    Simulation sim(s, std::nullopt);
    const Vec3 from(10, 10, 200);
    const Vec3 up = Rules::roundVelocity(0.3, 0.2);
    const int64_t id = shoot(sim, from, up);
    const double born = sim.state.time;
    std::vector<GameEvent> events;
    for (int k = 0; k < 89; k++) {
        auto e = sim.step(1.0 / 30);
        events.insert(events.end(), e.begin(), e.end());
        ASSERT_TRUE(sim.state.rounds.size() == size_t(1));
        const Vec3 want = Rules::roundAt(from, up, sim.state.time - born);
        const Round& r = sim.state.rounds[0];
        EXPECT_NEAR(r.position.x, want.x, 1e-9);
        EXPECT_NEAR(r.position.y, want.y, 1e-9);
        EXPECT_NEAR(r.position.z, want.z, 1e-9);
        EXPECT_NEAR(r.velocity.z, up.z - Rules::roundGravity * (sim.state.time - born), 1e-9);
    }
    events = run(sim, 0.1);
    EXPECT_TRUE(sim.state.rounds.empty());
    EXPECT_TRUE(all<GameEvent::Impact>(events).empty());
    (void)id;
}

TEST(round_hitsTheFirstThingOnItsPath) {
    // A Shardling in front of a building of the Brood's: the bug takes it.
    {
        GameState s = bare(2, 0, {}, {}, 1);
        const int64_t wall = addStructure(s, Structure::Kind::garrison, Vec2(30, 10), 1);
        Simulation sim(s, std::nullopt);
        sim.broodSide = 1;
        const int64_t bug = bugAt(sim, Unit::Kind::shardling, Vec2(20, 10));
        const int64_t r = shoot(sim, Vec3(10, 10, 0.8), Vec3(Rules::roundSpeed, 0, 0));
        const auto events = run(sim, 0.5);
        const auto hits = all<GameEvent::Impact>(events);
        ASSERT_TRUE(hits.size() == size_t(1));
        EXPECT_EQ(hits[0].round, r);
        EXPECT_TRUE(hits[0].what == GameEvent::Impact::What::unit);
        EXPECT_EQ(hits[0].victim, bug);
        EXPECT_NEAR(hits[0].at.x, 20 - Rules::radius(Unit::Kind::shardling) - Rules::roundRadius, 0.01);
        EXPECT_EQ(hp(sim, bug), Rules::hp(Unit::Kind::shardling) - 1.0);
        EXPECT_EQ(structureHp(sim, wall), Rules::hp(Structure::Kind::garrison));
        EXPECT_TRUE(sim.state.rounds.empty());
        // Over the bug's head (its body 1.5 tall) it flies on to the building,
        // whose armour takes half of it.
        shoot(sim, Vec3(10, 10, 1.9), Vec3(Rules::roundSpeed, 0, 0));
        const auto next = all<GameEvent::Impact>(run(sim, 0.5));
        ASSERT_TRUE(next.size() == size_t(1));
        EXPECT_TRUE(next[0].what == GameEvent::Impact::What::structure && next[0].victim == wall);
        EXPECT_NEAR(next[0].at.x, 30 - Rules::radius(Structure::Kind::garrison) - Rules::roundRadius, 0.01);
        EXPECT_EQ(structureHp(sim, wall), Rules::hp(Structure::Kind::garrison) - 0.5);
    }
    // Into the ground: it strikes where the arc meets it (no map: z 0).
    {
        GameState s = bare(1, 0, {}, {}, 1);
        Simulation sim(s, std::nullopt);
        const Vec3 v = Rules::roundVelocity(0, -0.1);
        shoot(sim, Vec3(0, 0, 3), v);
        const auto hits = all<GameEvent::Impact>(run(sim, 0.5));
        ASSERT_TRUE(hits.size() == size_t(1));
        EXPECT_TRUE(hits[0].what == GameEvent::Impact::What::ground && hits[0].victim == 0);
        EXPECT_NEAR(hits[0].at.z, 0, 0.01);
        // Where the parabola from the muzzle crosses z 0.
        const double t = (v.z + std::sqrt(v.z * v.z + 2 * Rules::roundGravity * 3)) / Rules::roundGravity;
        EXPECT_NEAR(hits[0].at.x, v.x * t, 0.05);
    }
    // On the terrain of a real map: a round fired level into a rise strikes
    // the ground on it, the terrain's height there.
    {
        Open o = open();
        Simulation sim(o.s, homeMap());
        const TerrainField& f = *sim.field;
        Vec2 low = o.at;
        // Along +x from the base, to the first point a cell higher.
        std::optional<Vec2> rise;
        for (double d = 1; d < 60 && !rise; d += 0.5)
            if (f.height(low + Vec2(d, 0)) > f.height(low) + 1.2) rise = low + Vec2(d, 0);
        if (rise) {
            shoot(sim, Vec3(low.x, low.y, f.height(low) + 0.6), Vec3(Rules::roundSpeed, 0, 0));
            const auto hits = all<GameEvent::Impact>(run(sim, 1));
            ASSERT_TRUE(hits.size() == size_t(1));
            EXPECT_TRUE(hits[0].what == GameEvent::Impact::What::ground);
            EXPECT_NEAR(hits[0].at.z, f.height(Vec2(hits[0].at.x, hits[0].at.y)), 0.01);
            EXPECT_TRUE(hits[0].at.x <= rise->x + 0.01);
        }
    }
    // A friend in the way stops the round unhurt (no friendly fire); a
    // sheltered gunner is never struck.
    {
        GameState s = bare(2, 0, {}, {}, 1);
        const int64_t friend_ = addUnit(s, Unit::Kind::prospector, Vec2(20, 10), 0);
        Simulation sim(s, std::nullopt);
        shoot(sim, Vec3(10, 10, 0.8), Vec3(Rules::roundSpeed, 0, 0));
        const auto hits = all<GameEvent::Impact>(run(sim, 0.5));
        ASSERT_TRUE(hits.size() == size_t(1));
        EXPECT_TRUE(hits[0].what == GameEvent::Impact::What::unit && hits[0].victim == friend_);
        EXPECT_EQ(hp(sim, friend_), Rules::hp(Unit::Kind::prospector));
    }
}

TEST(round_damageLandsAtImpactNotAtFire) {
    GameState s = bare(2, 0, {}, {}, 1);
    Simulation sim(s, std::nullopt);
    sim.broodSide = 1;
    const int64_t bug = bugAt(sim, Unit::Kind::lodeback, Vec2(55, 10));
    // 45 cells at 90 a second: half a second on the way, 1.2 cells of drop.
    shoot(sim, Vec3(10, 10, 2), Vec3(Rules::roundSpeed, 0, 0), 0, -1, 3);
    run(sim, 0.4);
    EXPECT_EQ(hp(sim, bug), Rules::hp(Unit::Kind::lodeback));
    EXPECT_EQ(sim.state.rounds.size(), size_t(1));
    const auto events = run(sim, 0.2);
    // Its armour (1) off the round's 3.
    EXPECT_EQ(hp(sim, bug), Rules::hp(Unit::Kind::lodeback) - 2);
    EXPECT_EQ(all<GameEvent::Impact>(events).size(), size_t(1));
    // A manned gun's first round: Fired at once, the damage only on arrival.
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    const double rest = Defence::cornerAngle(0);
    // 36 cells: nearer, the ground is under the gun's lowest pitch (−20°
    // from the mount, 9 cells up).
    const Vec2 target = Defence::cornerGun(kAt, 0) + Vec2(std::cos(rest), std::sin(rest)) * 36;
    const int64_t far = bugAt(h.sim, Unit::Kind::lodeback, target);
    h.sim.pilot->heading = rest;
    h.sim.pilot->pitch = pitchOnto(0, rest, Vec3(target.x, target.y, 0.75));
    h.sim.pilot->hold = true;
    auto first = h.sim.step(1.0 / 30);
    EXPECT_EQ(fired(first, h.citadel), int64_t(1));
    h.sim.pilot->hold = false;
    first = run(h.sim, 0.2);
    EXPECT_EQ(hp(h.sim, far), Rules::hp(Unit::Kind::lodeback));
    first = run(h.sim, 0.4);
    EXPECT_TRUE(hp(h.sim, far) < Rules::hp(Unit::Kind::lodeback));
}

TEST(gun_spinsUpAndOverheats) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    // At the sky: nothing to hit, only the gun.
    h.sim.pilot->heading = Defence::cornerAngle(0);
    h.sim.pilot->pitch = Defence::gunPitchMax;
    h.sim.pilot->hold = true;
    auto gun = [&] { return (*structureOf(h.sim, h.citadel).guns)[0]; };
    // The barrels spin up over `gunSpinUp`: the first tenth fires fewer than
    // a tenth at full spin.
    const int64_t early = fired(run(h.sim, 0.1), h.citadel);
    run(h.sim, 0.4);
    EXPECT_EQ(gun().spin, 1.0);
    const int64_t full = fired(run(h.sim, 1.0), h.citadel);
    EXPECT_TRUE(early >= 1 && early < 3);
    EXPECT_TRUE(full >= 29 && full <= 31);
    EXPECT_TRUE(gun().heat > 0.2);
    EXPECT_TRUE(!gun().overheated);
    // Overheats after about 4.5 s of fire: it stops.
    double t = 1.5;
    while (!gun().overheated && t < 8) {
        h.sim.step(1.0 / 30);
        t += 1.0 / 30;
    }
    EXPECT_TRUE(gun().overheated);
    EXPECT_TRUE(t > 4.0 && t < 5.2);
    EXPECT_EQ(gun().heat, 1.0);
    // Held down, no round while it cools to `gunHeatResume`, about 3 s
    // (the step it is cool enough, it fires again).
    double cool = 0;
    int64_t during = 0;
    while (cool < 6) {
        const auto e = h.sim.step(1.0 / 30);
        cool += 1.0 / 30;
        if (!gun().overheated) break;
        during += fired(e, h.citadel);
    }
    EXPECT_EQ(during, int64_t(0));
    EXPECT_TRUE(cool > 2.8 && cool < 3.2);
    EXPECT_TRUE(gun().heat < Rules::gunHeatResume + 2 * Rules::gunHeatPerRound);
    // Then it fires again; the trigger up, the barrels run down.
    EXPECT_TRUE(fired(run(h.sim, 0.5), h.citadel) > 0);
    h.sim.pilot->hold = false;
    run(h.sim, 0.2);
    EXPECT_TRUE(gun().spin < 0.6);
    EXPECT_EQ(fired(run(h.sim, 0.5), h.citadel), int64_t(0));
    EXPECT_EQ(gun().spin, 0.0);
    // The cone grows round by round and fades.
    EXPECT_TRUE(gun().bloom < 0.0001);
}

TEST(gun_firesAlongItsAimWithNoRangeGate) {
    // The user's case (2026-10-18): a bug 35 cells off a corner gun, far
    // past the old 7.5, is hit along the gun's aim.
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    const double rest = Defence::cornerAngle(0);
    const Vec2 out(std::cos(rest), std::sin(rest));
    const Vec2 at = Defence::cornerGun(kAt, 0) + out * 35;
    const int64_t bug = bugAt(h.sim, Unit::Kind::lodeback, at);
    h.sim.pilot->heading = rest;
    h.sim.pilot->pitch = pitchOnto(0, rest, Vec3(at.x, at.y, 0.75));
    h.sim.pilot->hold = true;
    h.sim.pilot->crosshair = Pilot::Crosshair{Pilot::Crosshair::On{bug}};
    auto events = steps(h.sim, 1.5);
    EXPECT_TRUE(fired(events, h.citadel) > 20);
    EXPECT_EQ(shots(events, h.citadel), int64_t(0));
    for (const auto& e : events) EXPECT_TRUE(!e.is<GameEvent::Missed>());
    // Every round leaves along the aim, inside the cone.
    for (const auto& f : all<GameEvent::Fired>(events)) {
        const double yaw = std::atan2(f.velocity.y, f.velocity.x);
        EXPECT_TRUE(std::abs(std::remainder(yaw - rest, 2 * pi)) < 0.05);
        EXPECT_NEAR(std::sqrt(f.velocity.x * f.velocity.x + f.velocity.y * f.velocity.y + f.velocity.z * f.velocity.z), Rules::roundSpeed, 1e-9);
    }
    const auto impacts = all<GameEvent::Impact>(events);
    int64_t onBug = 0;
    for (const auto& i : impacts) onBug += i.victim == bug;
    EXPECT_TRUE(onBug > 10);
    EXPECT_TRUE(hp(h.sim, bug) < Rules::hp(Unit::Kind::lodeback) - 5);
    EXPECT_EQ((*structureOf(h.sim, h.citadel).guns)[0].target, std::optional<int64_t>(bug));
    bool told = false;
    for (const auto& b : h.sim.bugHits) told = told || (b.shooter == h.citadel && b.bug == bug);
    EXPECT_TRUE(told);
    // With the crosshair on it but the gun aimed away: the rounds go where
    // the gun points, not at the crosshair's foe.
    h.sim.pilot->heading = rest + 1.0;
    // (The rounds already on their way land first.)
    steps(h.sim, 0.6);
    const double had = hp(h.sim, bug);
    steps(h.sim, 1);
    EXPECT_EQ(hp(h.sim, bug), had);
    // The trigger up: nothing.
    h.sim.pilot->hold = false;
    h.sim.pilot->act = false;
    steps(h.sim, 0.3);
    EXPECT_EQ(fired(steps(h.sim, 0.5), h.citadel), int64_t(0));
}

TEST(gun_reachesTheFootOfItsWall) {
    // The manager's raid hole (2026-10-18): at −20° from the 9-cell mount a
    // corner gun met no ground nearer than about 26 cells, so a raid at the
    // walls was under every gun. On its bracket at 6.2 and down to −70°, its
    // rounds land within 4 cells of the wall's foot and strike a bug there.
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    const double rest = Defence::cornerAngle(0);
    h.sim.pilot->heading = rest;
    h.sim.pilot->pitch = Defence::gunPitchMin;
    h.sim.pilot->hold = true;
    const auto first = all<GameEvent::Impact>(steps(h.sim, 0.5));
    ASSERT_TRUE(!first.empty());
    // The wall's foot: the outer faces at 6 from the centre (no map: z 0).
    const auto& i = first.front();
    EXPECT_TRUE(i.what == GameEvent::Impact::What::ground);
    EXPECT_NEAR(i.at.z, 0, 0.01);
    const double out = ac::max(std::abs(i.at.x - kAt.x), std::abs(i.at.y - kAt.y)) - Rules::radius(StructureKind::citadel);
    EXPECT_TRUE(out > 0 && out < 4);
    // A bug there is hit.
    h.sim.pilot->hold = false;
    steps(h.sim, 1);
    const int64_t bug = bugAt(h.sim, Unit::Kind::lodeback, Vec2(i.at.x, i.at.y));
    h.sim.pilot->hold = true;
    const auto events = steps(h.sim, 1);
    int64_t onBug = 0;
    for (const auto& x : all<GameEvent::Impact>(events)) onBug += x.victim == bug;
    EXPECT_TRUE(onBug > 5);
    EXPECT_TRUE(hp(h.sim, bug) < Rules::hp(Unit::Kind::lodeback));
    // c2-netserver's blind strip: a bug 0.7 off the east wall's middle, at
    // (6.7, 1.5) from the centre, 129° round from the gun's rest: in its arc
    // and its pitch now, and hit.
    h.sim.pilot->hold = false;
    steps(h.sim, 1);
    const Vec2 mid = kAt + Vec2(6.7, 1.5);
    const int64_t side = bugAt(h.sim, Unit::Kind::lodeback, mid);
    const Vec2 mount = Defence::cornerGun(kAt, 0);
    const double toward = std::atan2(mid.y - mount.y, mid.x - mount.x);
    EXPECT_TRUE(std::abs(std::remainder(toward - rest, 2 * pi)) < Defence::gunArcHalf);
    h.sim.pilot->heading = toward;
    h.sim.pilot->pitch = pitchOnto(0, toward, Vec3(mid.x, mid.y, 0.75));
    EXPECT_TRUE(h.sim.pilot->pitch > Defence::gunPitchMin);
    h.sim.pilot->hold = true;
    const auto along = steps(h.sim, 1);
    int64_t onSide = 0;
    for (const auto& x : all<GameEvent::Impact>(along)) onSide += x.victim == side;
    EXPECT_TRUE(onSide > 5);
}

TEST(gatling_aiLeadsAndFiresRounds) {
    // A Lodeback walking across the gatling gun's line 15 cells out, its
    // walk scripted (+y at its full 3 cells a second): the gun leads it by
    // its speed across the line × the flight / the range, and hits it.
    Open o = open();
    const int64_t gun = addStructure(o.s, Structure::Kind::gatling, o.at, 0);
    Simulation sim(o.s, homeMap(), false);
    sim.broodSide = 1;
    const Vec2 from = o.at + Vec2(15, -4);
    const int64_t walker = bugAt(sim, Unit::Kind::lodeback, from);
    const double speed = Rules::speed(Unit::Kind::lodeback);
    double lead = 0, want = 0;
    int64_t n = 0;
    int64_t total = 0;
    for (int k = 0; k < 75; k++) {
        Unit& u = unitOf(sim, walker);
        u.position = from + Vec2(0, speed * k / 30.0);
        u.heading = pi / 2;
        u.moving = true;
        u.bug->pace = 1;
        const Vec2 at = u.position;
        const auto events = sim.step(1.0 / 30);
        total += fired(events, gun);
        for (const auto& f : all<GameEvent::Fired>(events)) {
            if (f.shooter != gun) continue;
            const Vec2 to = at - Vec2(f.muzzle.x, f.muzzle.y);
            const double d = length(to), bearing = std::atan2(to.y, to.x);
            lead += std::remainder(std::atan2(f.velocity.y, f.velocity.x) - bearing, 2 * pi);
            want += to.x * speed / d / Rules::roundSpeed;
            n++;
        }
    }
    EXPECT_TRUE(total > 20);
    ASSERT_TRUE(n > 20);
    // About 0.033 rad ahead, give or take the cone's scatter.
    EXPECT_TRUE(want / double(n) > 0.025);
    EXPECT_NEAR(lead / double(n), want / double(n), 0.006);
    EXPECT_TRUE(hp(sim, walker) < Rules::hp(Unit::Kind::lodeback));
    for (const auto& s : sim.state.structures)
        if (s.id == gun) EXPECT_TRUE(s.heat > 0 && s.spin > 0);
    // Out of its 20-cell reach: it holds fire.
    Open far = open();
    const int64_t gun2 = addStructure(far.s, Structure::Kind::gatling, far.at, 0);
    addUnit(far.s, Unit::Kind::ranger, far.at + Vec2(23, 0), 1);
    Simulation m(far.s, homeMap(), false);
    EXPECT_EQ(fired(run(m, 1), gun2), int64_t(0));
}

TEST(gatling_onALedgeHitsBugsBelow) {
    // c2-netserver's case (2026-10-18): a gatling gun up on a ledge fired
    // every round into its own lip at bugs below it. On the home map's
    // plateau: a bug on the low ground below whose line from the muzzle
    // runs under the plateau's edge is held fire at; one farther out below,
    // in the gun's view, is hit; and no round strikes the ground at the gun.
    Open o = open();
    Simulation probe(o.s, homeMap(), false);
    const TerrainField& f = *probe.field;
    auto hidden = [&](Vec2 p, Vec2 q) {
        const Vec3 m(p.x, p.y, f.height(p) + Defence::gatlingPivot);
        const Vec3 b(q.x, q.y, f.height(q) + 0.75);
        for (double t = 0.05; t < 0.95; t += 0.01) {
            const Vec3 x = m + (b - m) * t;
            if (x.z < f.height(Vec2(x.x, x.y)) - 0.2) return true;
        }
        return false;
    };
    // A gun on the plateau with a hidden spot below it, and a seen one.
    std::optional<std::pair<Vec2, Vec2>> pair;
    std::optional<Vec2> seen;
    for (double apart : {4.0, 5.0, 6.0}) {
        pair = cliffPair(homeMap(), f, *probe.navGrid(), apart, hidden);
        if (!pair) continue;
        seen = std::nullopt;
        for (int k = 0; k < 64 && !seen; k++) {
            const double a = 2 * pi * k / 64;
            for (double r = 9; r <= 17 && !seen; r += 1) {
                const Vec2 q = pair->first + Vec2(std::cos(a), std::sin(a)) * r;
                if (f.level(q) == 0 && probe.navGrid()->walkable(q) && f.height(pair->first) - f.height(q) > 1 && !hidden(pair->first, q)) seen = q;
            }
        }
        if (seen) break;
    }
    ASSERT_TRUE(pair.has_value() && seen.has_value());
    auto scene = [&](Vec2 bugAt_) {
        GameState s = o.s;
        addStructure(s, Structure::Kind::gatling, pair->first, 0);
        addUnit(s, Unit::Kind::lodeback, bugAt_, 1);
        s.units.back().bug = Bug{};
        s.units.back().bug->site = bugAt_;
        s.units.back().bug->coming = false;
        s.units.back().bug->wanderAt = 1e9;
        Simulation m(s, homeMap(), false);
        m.broodSide = 1;
        return m;
    };
    {
        Simulation m = scene(pair->second);
        const int64_t gun = m.state.structures.back().id, bug = m.state.units.back().id;
        const auto events = run(m, 2);
        EXPECT_EQ(fired(events, gun), int64_t(0));
        EXPECT_EQ(hp(m, bug), Rules::hp(Unit::Kind::lodeback));
    }
    {
        Simulation m = scene(*seen);
        const int64_t gun = m.state.structures.back().id, bug = m.state.units.back().id;
        const auto events = run(m, 2);
        EXPECT_TRUE(fired(events, gun) > 10);
        EXPECT_TRUE(hp(m, bug) < Rules::hp(Unit::Kind::lodeback));
        int64_t onBug = 0, atGun = 0;
        for (const auto& i : all<GameEvent::Impact>(events)) {
            onBug += i.victim == bug;
            atGun += i.what == GameEvent::Impact::What::ground && distance(Vec2(i.at.x, i.at.y), pair->first) < 3;
        }
        EXPECT_TRUE(onBug > 5);
        EXPECT_EQ(atGun, int64_t(0));
        // The barrels dip to the solve (the pose's cradle follows it).
        EXPECT_TRUE(m.state.structure(gun)->pitch < -0.02);
    }
}

TEST(dummy_isTargetableByItsOwnSideAndStandsAgain) {
    const CitadelHall on(true);
    Hall h = hallScene(kAt);
    manGun(h, 0);
    const double rest = Defence::cornerAngle(0);
    const Vec2 at = Defence::cornerGun(kAt, 0) + Vec2(std::cos(rest), std::sin(rest)) * 35;
    const int64_t d = dummyAt(h.sim, at);
    // Its own side may aim at it, unlike any other building of its own.
    EXPECT_TRUE(h.sim.targetable(structureOf(h.sim, d), 0));
    EXPECT_TRUE(h.sim.targetable(structureOf(h.sim, d), 1));
    EXPECT_TRUE(!h.sim.targetable(structureOf(h.sim, h.citadel), 0));
    EXPECT_TRUE(h.sim.targetable(structureOf(h.sim, h.citadel), 1));
    // A driven Ranger's crosshair on it is a sight.
    const Unit ranger(990, Unit::Kind::ranger, 0, at - Vec2(5, 5), 0, Unit::Task::idle);
    const auto sight = h.sim.crosshairSight(ranger, Pilot::Crosshair{Pilot::Crosshair::On{d}});
    EXPECT_TRUE(sight && sight->target == d);
    // The corner gun shoots it down: every round counted, no armour.
    structureOf(h.sim, d).hp = 20;
    h.sim.pilot->heading = rest;
    h.sim.pilot->pitch = pitchOnto(0, rest, Vec3(at.x, at.y, 0.9));
    h.sim.pilot->hold = true;
    std::vector<GameEvent> events;
    for (int k = 0; k < 90 && !structureOf(h.sim, d).downUntil; k++) {
        auto e = h.sim.step(1.0 / 30);
        events.insert(events.end(), e.begin(), e.end());
    }
    const auto down = all<GameEvent::DummyDown>(events);
    ASSERT_TRUE(down.size() == size_t(1));
    EXPECT_EQ(down[0].id, d);
    EXPECT_EQ(down[0].hits, int64_t(20));
    // Down, not destroyed: no longer a target, and rounds pass over it.
    ASSERT_TRUE(h.sim.state.structure(d).has_value());
    for (const auto& e : events) EXPECT_TRUE(!e.is<GameEvent::Destroyed>());
    EXPECT_TRUE(!h.sim.targetable(structureOf(h.sim, d), 0));
    events = steps(h.sim, 1);
    for (const auto& i : all<GameEvent::Impact>(events)) EXPECT_TRUE(i.victim != d);
    EXPECT_EQ(structureOf(h.sim, d).hits, int64_t(20));
    h.sim.pilot->hold = false;
    // It stands again `dummyDownTime` after it fell, whole, its count reset.
    steps(h.sim, Rules::dummyDownTime);
    const Structure& up = structureOf(h.sim, d);
    EXPECT_TRUE(!up.downUntil);
    EXPECT_EQ(up.hp, Rules::hp(StructureKind::dummy));
    EXPECT_EQ(up.hits, int64_t(0));
    EXPECT_TRUE(h.sim.targetable(up, 0));
    // The Brood leaves it be: a Shardling beside it picks nothing.
    const int64_t bug = bugAt(h.sim, Unit::Kind::shardling, at + Vec2(1.2, 0));
    EXPECT_TRUE(!h.sim.acquire(unitOf(h.sim, bug), false));
}

TEST(dummy_buildsForTenOreWithNoBlueprint) {
    EXPECT_EQ(Rules::cost(StructureKind::dummy), int64_t(10));
    EXPECT_EQ(Rules::hydrogenCost(StructureKind::dummy), int64_t(0));
    EXPECT_EQ(Progression::worldOre(StructureKind::dummy), int64_t(10));
    EXPECT_EQ(Progression::worldHydrogen(StructureKind::dummy), int64_t(0));
    EXPECT_EQ(Rules::buildTime(StructureKind::dummy), 2.0);
    EXPECT_EQ(Rules::radius(StructureKind::dummy), 0.5);
    EXPECT_EQ(Rules::hp(StructureKind::dummy), 150.0);
    EXPECT_EQ(Rules::height(StructureKind::dummy), 1.8);
    EXPECT_EQ(Rules::vision(StructureKind::dummy), 0.0);
    EXPECT_TRUE(Rules::freeBuild(StructureKind::dummy));
    EXPECT_TRUE(!Rules::freeBuild(StructureKind::derrick));
    EXPECT_TRUE(Progression::perRegion(StructureKind::dummy));
    EXPECT_EQ(Simulation::title(StructureKind::dummy), std::string("Target Dummy"));
    // A new pilot, no copy, level 1: it may; at twelve in the region, not.
    PilotProgress p;
    EXPECT_TRUE(p.knows(StructureKind::dummy));
    EXPECT_TRUE(!p.buildRefusal(StructureKind::dummy, 0));
    EXPECT_TRUE(!p.buildRefusal(StructureKind::dummy, 11));
    EXPECT_EQ(p.buildRefusal(StructureKind::dummy, 12), std::optional<std::string>("Target Dummy limit reached: 12"));
    EXPECT_TRUE(p.learnRefusal(StructureKind::dummy).has_value());
    // A Prospector puts one up for 10 ore, and it rises in 2 s.
    GameState s = bare(1, 0, {}, {}, 1);
    s.players[0].ore = 10;
    s.players[0].hydrogen = 0;
    const int64_t pro = addUnit(s, Unit::Kind::prospector, Vec2(20, 20), 0);
    Simulation sim(s, std::nullopt);
    ASSERT_TRUE(sim.take(pro));
    EXPECT_TRUE(!sim.pilotBuild(StructureKind::dummy, Vec2(22.5, 20.5)));
    EXPECT_EQ(sim.state.players[0].ore, int64_t(0));
    run(sim, 2.1);
    bool built = false;
    for (const auto& st : sim.state.structures) built = built || (st.kind == StructureKind::dummy && st.complete());
    EXPECT_TRUE(built);
}

TEST(blueprint_learnedOnceThenBuildsForOre) {
    PilotProgress p;
    p.xp = Progression::xp(Progression::blueprintFloor(StructureKind::derrick));
    ASSERT_TRUE(p.level() >= Progression::blueprintFloor(StructureKind::derrick));
    // No copy: buy one.
    EXPECT_EQ(p.buildRefusal(StructureKind::derrick, 0), std::optional<std::string>("No blueprint: buy one at a Citadel (H)"));
    EXPECT_EQ(p.learnRefusal(StructureKind::derrick), std::optional<std::string>("No Derrick blueprint to learn from"));
    EXPECT_TRUE(!p.learn(StructureKind::derrick));
    // A copy held: learn it first.
    p.giveBlueprint(StructureKind::derrick, 2);
    EXPECT_EQ(p.buildRefusal(StructureKind::derrick, 0), std::optional<std::string>("Learn it first (B)"));
    EXPECT_TRUE(!p.knows(StructureKind::derrick));
    // Learning uses one copy, once.
    EXPECT_TRUE(!p.learnRefusal(StructureKind::derrick));
    EXPECT_TRUE(p.learn(StructureKind::derrick));
    EXPECT_TRUE(p.knows(StructureKind::derrick));
    EXPECT_EQ(p.copies(StructureKind::derrick), int64_t(1));
    EXPECT_EQ(p.learned, std::set<StructureKind>{StructureKind::derrick});
    EXPECT_EQ(p.learnRefusal(StructureKind::derrick), std::optional<std::string>("You know the Derrick"));
    EXPECT_TRUE(!p.learn(StructureKind::derrick));
    EXPECT_EQ(p.copies(StructureKind::derrick), int64_t(1));
    // From then on it builds for its ore, with no copy, up to its limit.
    p.blueprints.clear();
    for (int64_t owned = 0; owned < p.limit(StructureKind::derrick); owned++) EXPECT_TRUE(!p.buildRefusal(StructureKind::derrick, owned));
    EXPECT_TRUE(p.buildRefusal(StructureKind::derrick, p.limit(StructureKind::derrick)).has_value());
    // The floor still holds for a learned kind.
    PilotProgress young;
    young.giveBlueprint(StructureKind::habDome);
    ASSERT_TRUE(young.learn(StructureKind::habDome));
    EXPECT_EQ(young.buildRefusal(StructureKind::habDome, 0), std::optional<std::string>("The Hab Dome opens at level 8"));
    // What was learned is saved with the pilot.
    const PilotProgress back = SessionStore::decode<PilotProgress>(SessionStore::encode(p)).value_or(PilotProgress{});
    EXPECT_EQ(back.learned, p.learned);
}

TEST(round_replayIsDeterministic) {
    auto scene = [] {
        const CitadelHall on(true);
        Hall h = hallScene(kAt);
        manGun(h, 0);
        const double rest = Defence::cornerAngle(0);
        for (int k = 0; k < 6; k++) {
            const Vec2 at = Defence::cornerGun(kAt, 0) + Vec2(std::cos(rest + 0.05 * (k - 3)), std::sin(rest + 0.05 * (k - 3))) * (30 + 3 * k);
            bugAt(h.sim, Unit::Kind::shardling, at);
        }
        h.sim.pilot->heading = rest;
        h.sim.pilot->pitch = pitchOnto(0, rest, Vec3(kAt.x + 30, kAt.y + 30, 0.75));
        h.sim.pilot->hold = true;
        std::vector<GameEvent> events;
        for (int k = 0; k < 75; k++) {
            if (k % 15 == 0) h.sim.pilot->heading = rest + 0.04 * double(k % 4 - 2);
            auto e = h.sim.step(1.0 / 30);
            events.insert(events.end(), e.begin(), e.end());
        }
        return std::pair{h.sim.state, events};
    };
    const auto [a, ea] = scene();
    const auto [b, eb] = scene();
    EXPECT_TRUE(!a.rounds.empty());
    EXPECT_TRUE(a == b);
    EXPECT_TRUE(ea == eb);
    EXPECT_TRUE(all<GameEvent::Impact>(ea).size() > 5);
}

/// The cost of the sweep: 6 guns at 30 rounds a second for 3 s keep about
/// 540 rounds in flight over a region of bugs. Only on request
/// (AUTOCRAFT_ROUNDS_BENCH=1): it prints the time a step's `stepRounds` takes.
TEST(rounds_benchSixGuns) {
    if (!std::getenv("AUTOCRAFT_ROUNDS_BENCH")) {
        std::printf("  skipped (AUTOCRAFT_ROUNDS_BENCH=1 runs it)\n");
        return;
    }
    Open o = open();
    Simulation sim(o.s, homeMap(), false);
    sim.broodSide = 1;
    for (int k = 0; k < 200; k++) {
        const double a = 0.7 * k, r = 6 + 0.15 * k;
        bugAt(sim, Unit::Kind::shardling, o.at + Vec2(std::cos(a), std::sin(a)) * r);
    }
    // 540 rounds fanned out level and slightly up, born over the last 3 s.
    for (int k = 0; k < 540; k++) {
        const double yaw = 0.0117 * k, pitch = 0.02 + 0.0003 * (k % 50);
        const Vec3 v = Rules::roundVelocity(yaw, pitch);
        const double age = 3.0 * (k % 90) / 90.0;
        Vec3 from(o.at.x, o.at.y, sim.field->height(o.at) + 9);
        sim.state.rounds.push_back(Round{sim.state.nextRound++, -1, 0, Rules::roundAt(from, v, age),
                                         Vec3(v.x, v.y, v.z - Rules::roundGravity * age), 1, sim.state.time - age});
    }
    std::vector<GameEvent> events;
    const std::vector<Round> before = sim.state.rounds;
    const auto t = std::chrono::steady_clock::now();
    const size_t n = sim.state.rounds.size();
    sim.stepRounds(1.0 / 30, events);
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count();
    std::printf("  stepRounds: %zu rounds, %zu units, %.0f us the first step (the ground's tiles built)\n", n, sim.state.units.size(), us);
    // Then, the tiles known: the next steps, each its own 540 rounds again.
    double sum = 0;
    for (int k = 0; k < 10; k++) {
        sim.state.rounds = before;
        std::vector<GameEvent> e;
        const auto tk = std::chrono::steady_clock::now();
        sim.stepRounds(1.0 / 30, e);
        sum += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - tk).count();
    }
    std::printf("  stepRounds: %.0f us a step after\n", sum / 10);
    // The same without the terrain (the ground flat at 0): what the sweep
    // of the bodies costs alone.
    Simulation flat = sim;
    flat.state.rounds = before;
    flat.field = std::nullopt;
    std::vector<GameEvent> more;
    const auto t2 = std::chrono::steady_clock::now();
    flat.stepRounds(1.0 / 30, more);
    std::printf("  without the terrain: %.0f us\n", std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t2).count());
    int64_t calls = 0;
    const auto t3 = std::chrono::steady_clock::now();
    double h = 0;
    for (const Round& r : before) for (int k = 0; k < 8; k++) { h += sim.field->height(Vec2(r.position.x + 0.3 * k, r.position.y)); calls++; }
    std::printf("  %lld terrain heights: %.0f us (%g)\n", (long long)calls, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t3).count(), h);
}
