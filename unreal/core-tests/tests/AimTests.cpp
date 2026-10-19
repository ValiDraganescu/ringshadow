// Every weapon aims at the target's body at its height now
// (docs/guns-and-practice.md, "Range and line of sight"): a Gloamwing
// hovering, one held in its dive, a Kestrel at its hover. The core's one
// truth is `Unit::bodyHeight` (`Simulation::aimHeight`); the Sentinel's pods
// pitch onto it, the gatling's and the corner guns' rounds strike it there,
// the units' shots land on it. Hand-built scenes with no map (ground at 0).
#include "Brood.h"
#include "Defence.h"
#include "HallScene.h"
#include "SimHelpers.h"

#include <cmath>

using namespace ac;
using namespace simtest;

namespace {

template <class E> std::vector<E> all(const std::vector<GameEvent>& events) {
    std::vector<E> out;
    for (const auto& e : events) if (const E* x = e.as<E>()) out.push_back(*x);
    return out;
}

/// How a test holds its Gloamwing: hovering, or in its dive (counted as
/// ground), at `height` cells.
struct Flight {
    const char* name;
    bool dive;
    double height;
};

constexpr Flight kHover{"hovering", false, Brood::hoverHeight};
constexpr Flight kDive{"diving", true, Brood::diveHeight};
/// On its way down: still high, but already in its dive.
constexpr Flight kDiving{"halfway down", true, 1.6};

/// Hit points a test's Gloamwing starts with: it lives through the scene.
constexpr double kDeep = 5000;

/// A Gloamwing of the Brood's at `p` with `kDeep` hp.
int64_t wingAt(Simulation& sim, Vec2 p) {
    const int64_t id = bugAt(sim, Unit::Kind::gloamwing, p);
    unitOf(sim, id).hp = kDeep;
    return id;
}

/// Holds bug `id` as `f` says, still at its spot (before each step: the
/// buildings and the rounds step before the units, so they see it so).
void hold(Simulation& sim, int64_t id, const Flight& f) {
    Unit& u = unitOf(sim, id);
    u.position = u.bug->site;
    u.moving = false;
    u.bug->height = f.height;
    u.bug->diveFrom = f.dive ? std::optional<double>(sim.state.time) : std::nullopt;
    u.cooldown = 10;
}

/// Steps `seconds` holding bug `id`; the events.
std::vector<GameEvent> flyFor(Simulation& sim, int64_t id, const Flight& f, double seconds) {
    std::vector<GameEvent> out;
    for (int k = 0; k < int(std::lround(seconds * 30)); k++) {
        hold(sim, id, f);
        const auto e = sim.step(1.0 / 30);
        out.insert(out.end(), e.begin(), e.end());
    }
    hold(sim, id, f);
    return out;
}

/// A bare field with no map, the Brood as player 1.
Simulation field(GameState s) {
    Simulation sim(s, std::nullopt);
    sim.broodSide = 1;
    return sim;
}

} // namespace

TEST(aim_bodyHeightIsWhereTheBodyIsNow) {
    // A flyer's at its height whether or not it counts as in the air; the
    // rest at half their height.
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t kestrel = addUnit(s, Unit::Kind::kestrel, Vec2(20, 20), 0);
    const int64_t ranger = addUnit(s, Unit::Kind::ranger, Vec2(24, 20), 0);
    const int64_t matriarch = addUnit(s, Unit::Kind::matriarch, Vec2(40, 20), 1);
    Simulation sim = field(s);
    const int64_t wing = wingAt(sim, Vec2(30, 20));
    for (const Flight& f : {kHover, kDive, kDiving}) {
        hold(sim, wing, f);
        const Unit& u = unitOf(sim, wing);
        EXPECT_EQ(u.airborne(), !f.dive);
        EXPECT_NEAR(u.bodyHeight(), f.height, 1e-12);
        EXPECT_NEAR(sim.aimHeight(wing), f.height, 1e-12);
    }
    EXPECT_NEAR(sim.aimHeight(kestrel), Rules::stats(Unit::Kind::kestrel).hover, 1e-12);
    EXPECT_NEAR(sim.aimHeight(ranger), 0.75, 1e-12);
    EXPECT_NEAR(sim.aimHeight(matriarch), Rules::stats(Unit::Kind::matriarch).height / 2, 1e-12);
}

TEST(aim_sentinelPitchesOntoAFlyersBody) {
    // A Gloamwing hovering 5 cells off a Sentinel: the pods pitch onto its
    // body (not its ground point), and the missiles hit. The Sentinel hits
    // only the air: in its dive (at torch height, `Brood::low`) it holds
    // fire and its pods go back to rest.
    GameState s = bare(2, 0, {}, {}, 1);
    const int64_t sentinel = addStructure(s, Structure::Kind::sentinel, Vec2(30, 30), 0);
    Simulation sim = field(s);
    const int64_t wing = wingAt(sim, Vec2(35, 30));
    flyFor(sim, wing, kHover, 1);
    const double want = Defence::sentinelPitch(5, Brood::hoverHeight);
    EXPECT_TRUE(want > 0.25);
    const auto events = flyFor(sim, wing, kHover, 2);
    EXPECT_NEAR(sim.state.structure(sentinel)->pitch, want, 1e-9);
    int64_t shots = 0;
    for (const auto& x : all<GameEvent::Shot>(events)) shots += x.unit == sentinel && x.target == wing;
    EXPECT_TRUE(shots >= 2);
    EXPECT_TRUE(hp(sim, wing) < kDeep);
    // Higher or lower, the pods follow its height each step.
    for (const double h : {1.2, 3.4}) {
        flyFor(sim, wing, Flight{"held", false, h}, 1);
        EXPECT_NEAR(sim.state.structure(sentinel)->pitch, Defence::sentinelPitch(5, h), 1e-9);
    }
    // Diving: no target, back to rest.
    const double had = hp(sim, wing);
    const auto low = flyFor(sim, wing, kDive, 1);
    EXPECT_TRUE(all<GameEvent::Shot>(low).empty());
    EXPECT_EQ(hp(sim, wing), had);
    EXPECT_NEAR(sim.state.structure(sentinel)->pitch, Defence::sentinelRest, 1e-9);
}

TEST(aim_gatlingStrikesAFlyersBodyAtItsHeight) {
    // A gatling gun's rounds at a Gloamwing hovering 4 cells off (its air
    // reach), halfway down in its dive, and low in it 8 cells off: they
    // leave pitched for its height and strike it there.
    for (const auto& [f, off] : std::vector<std::pair<Flight, double>>{{kHover, 4}, {kDiving, 4}, {kDive, 8}}) {
        GameState s = bare(2, 0, {}, {}, 1);
        const int64_t gun = addStructure(s, Structure::Kind::gatling, Vec2(30, 30), 0);
        Simulation sim = field(s);
        const int64_t wing = wingAt(sim, Vec2(30 + off, 30));
        const auto events = flyFor(sim, wing, f, 1.5);
        int64_t fired = 0, onWing = 0;
        for (const auto& x : all<GameEvent::Fired>(events)) fired += x.shooter == gun;
        for (const auto& x : all<GameEvent::Impact>(events)) {
            if (x.victim != wing) continue;
            onWing++;
            // Within its body, about its height (half a body up or down).
            EXPECT_TRUE(std::abs(x.at.z - f.height) < 0.75 + 1e-9);
        }
        EXPECT_TRUE(fired > 10);
        EXPECT_TRUE(onWing > 5);
        EXPECT_TRUE(hp(sim, wing) < kDeep);
        // The barrels' pitch is the ballistic solve onto its body.
        const auto p = sim.gatlingPitch(*sim.state.structure(gun), Vec2(30 + off, 30), f.height, 0.5);
        ASSERT_TRUE(p.has_value());
        EXPECT_NEAR(sim.state.structure(gun)->pitch, *p, 1e-9);
    }
}

TEST(aim_cornerGunStrikesAFlyersBody) {
    // A pilot on a corner gun aims at a Gloamwing's body (hovering, then in
    // its dive): its rounds strike it; aimed at its ground point under it
    // they go under it.
    const CitadelHall on(true);
    for (const Flight& f : {kHover, kDive}) {
        Hall h = hallScene(kAt);
        manGun(h, 0);
        const double rest = Defence::cornerAngle(0);
        const Vec2 at = Defence::cornerGun(kAt, 0) + Vec2(std::cos(rest), std::sin(rest)) * 14;
        const int64_t wing = wingAt(h.sim, at);
        hold(h.sim, wing, f);
        h.sim.pilot->heading = rest;
        h.sim.pilot->pitch = 0;
        for (int k = 0; k < 4; k++) {
            const Vec3 m = Defence::gunMuzzle(kAt, 0, rest, h.sim.pilot->pitch);
            h.sim.pilot->pitch = Rules::ballisticPitch(distance(Vec2(m.x, m.y), at), h.sim.aimHeight(wing) - m.z).value_or(0);
        }
        h.sim.pilot->hold = true;
        const auto events = flyFor(h.sim, wing, f, 1);
        int64_t onWing = 0;
        for (const auto& x : all<GameEvent::Impact>(events)) onWing += x.victim == wing;
        EXPECT_TRUE(onWing > 5);
    }
}

TEST(aim_unitsShotsLandOnAFlyer) {
    // The units that shoot, at a Gloamwing 3 cells off: those that hit the
    // air while it hovers, those that hit the ground while it dives. Their
    // shots strike at once, by the target's id.
    struct Row { Unit::Kind kind; Flight f; };
    const std::vector<Row> rows = {
        {Unit::Kind::ranger, kHover},     {Unit::Kind::kestrel, kHover},  {Unit::Kind::hailstorm, kHover},
        {Unit::Kind::peregrine, kHover},  {Unit::Kind::ranger, kDive},    {Unit::Kind::juggernaut, kDive},
        {Unit::Kind::kestrel, kDive},     {Unit::Kind::atlas, kDive},     {Unit::Kind::longbow, kDive},
    };
    for (const Row& r : rows) {
        GameState s = bare(2, 0, {}, {}, 1);
        const int64_t shooter = addUnit(s, r.kind, Vec2(30, 30), 0);
        Simulation sim = field(s);
        const int64_t wing = wingAt(sim, Vec2(33, 30));
        const auto events = flyFor(sim, wing, r.f, 3);
        int64_t shots = 0;
        for (const auto& x : all<GameEvent::Shot>(events)) shots += x.unit == shooter && x.target == wing;
        if (shots == 0 || !(hp(sim, wing) < kDeep)) {
            std::printf("    %.*s at a Gloamwing %s: %lld shots, hp %.1f\n", int(Rules::stats(r.kind).title.size()), Rules::stats(r.kind).title.data(), r.f.name, (long long)shots, hp(sim, wing));
        }
        EXPECT_TRUE(shots > 0);
        EXPECT_TRUE(hp(sim, wing) < kDeep);
    }
}
