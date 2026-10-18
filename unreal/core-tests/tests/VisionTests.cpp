// Port of VisionTests.swift: the fog of war, the same for every player:
// what each side sees, what it remembers, and that its units and its AI act
// only on that. Hand-built positions and a few seconds of simulation each,
// never a game.
#include "VisionHelpers.h"

#include "NavGrid.h"
#include "TerrainField.h"
#include "Vision.h"

#include <algorithm>
#include <cmath>

using namespace ac;
using visiontest::add;
using visiontest::homeMap;

namespace {

/// Both Citadels, nothing else, at minute 10.
GameState field() {
    GameState s = GameState::new_(homeMap());
    s.units.clear();
    s.time = 600;
    return s;
}

std::vector<GameEvent> run(Simulation& sim, double seconds) {
    std::vector<GameEvent> events;
    for (int k = 0; k < static_cast<int>(seconds * 30); ++k) {
        for (auto& e : sim.step(1.0 / 30)) events.push_back(e);
    }
    return events;
}

Vec2 citadel(const GameState& s, int64_t owner) {
    for (const auto& b : s.structures) if (b.owner == owner) return b.position;
    return Vec2::zero;
}

struct Cliff { Vec2 low, high; bool found; };

/// A walkable spot under a cliff and one on top of it, `gap` apart
/// across the cliff, well away from any ramp and both Citadels.
Cliff cliff(const MapDefinition& m, double gap = 6) {
    const Simulation sim(GameState::new_(m), m);
    const NavGrid& nav = *sim.nav;
    const Vision& v = *sim.vision;
    for (const auto& plateau : m.plateaus) {
        if (!(plateau.level > 0)) continue;
        const auto& poly = plateau.polygon;
        for (size_t i = 0; i < poly.size(); ++i) {
            const Vec2 a = poly[i], b = poly[(i + 1) % poly.size()];
            for (int j = 0; j <= 6; ++j) {
                const double t = 0.2 + static_cast<double>(j) * 0.1;
                const Vec2 e = a + (b - a) * t;
                Vec2 n = normalize(Vec2(-(b - a).y, (b - a).x));
                if (signedDistance(e + n, poly) < 0) n = -n;
                const Vec2 low = e + n * gap / 2, high = e - n * gap / 2;
                if (!(nav.walkable(low) && nav.walkable(high))) continue;
                const auto li = v.index(low), hi = v.index(high);
                if (!(li && hi && v.tier[static_cast<size_t>(*hi)] > v.tier[static_cast<size_t>(*li)])) continue;
                if (!std::all_of(m.ramps.begin(), m.ramps.end(),
                                 [&](const Ramp& r) { return distance(r.low, e) > 8 && distance(r.high, e) > 8; })) continue;
                if (!std::all_of(sim.state.structures.begin(), sim.state.structures.end(),
                                 [&](const Structure& s) { return distance(s.position, e) > 14; })) continue;
                return Cliff{low, high, true};
            }
        }
    }
    return Cliff{Vec2::zero, Vec2::zero, false};
}

std::vector<int64_t> shooters(const std::vector<GameEvent>& events) {
    std::vector<int64_t> out;
    for (const auto& e : events) if (const auto* s = e.as<GameEvent::Shot>()) out.push_back(s->unit);
    return out;
}

bool contains(const std::vector<int64_t>& v, int64_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

} // namespace

/// High ground hides what stands on it: a Ranger below the cliff does
/// not see an enemy Ranger 6 cells off on top, and never shoots it;
/// the one on top sees and shoots the one below.
TEST(vision_highGroundHidesWhatIsOnIt) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Cliff c = cliff(m);
    ASSERT_TRUE(c.found);
    const int64_t below = add(s, Unit::Kind::ranger, c.low, 0);
    const int64_t above = add(s, Unit::Kind::ranger, c.high, 1);
    Simulation sim(s, m);
    const auto events = run(sim, 1.5);
    EXPECT_TRUE(!sim.sees(0, above)); // the cliff hides the Ranger on top
    EXPECT_TRUE(sim.sees(1, below));  // the Ranger on top sees down
    const auto shots = shooters(events);
    EXPECT_TRUE(!contains(shots, below)); // nothing to shoot at that it can see
    EXPECT_TRUE(contains(shots, above));
}

/// The Ranger below, out with its army (attacking somewhere else) and
/// shot by what it cannot see, goes for where the shots come from
/// instead of walking on.
TEST(vision_shotFromTheFogItGoesForTheShooter) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Cliff c = cliff(m);
    ASSERT_TRUE(c.found);
    const int64_t below = add(s, Unit::Kind::ranger, c.low, 0);
    s.units.back().task = Unit::Task::attackMove;
    s.players[0].attack = c.low + normalize(c.low - c.high) * 30;
    add(s, Unit::Kind::ranger, c.high, 1);
    Simulation sim(s, m);
    run(sim, 1.5);
    const auto u = sim.state.unit(below);
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(u->shotFrom == c.high); // it knows where the shots came from
    EXPECT_TRUE(u->task == Unit::Task::attacking); // and heads there
}

/// The air sees every level: a Dropship over the low ground sees the
/// Ranger on top of the cliff.
TEST(vision_theAirSeesUpCliffs) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Cliff c = cliff(m);
    ASSERT_TRUE(c.found);
    add(s, Unit::Kind::dropship, c.low, 0);
    const int64_t above = add(s, Unit::Kind::ranger, c.high, 1);
    Simulation sim(s, m);
    run(sim, 0.2);
    EXPECT_TRUE(sim.sees(0, above));
}

/// Out of sight is unknown: a red Ranger 20 cells from anything blue is
/// not in blue's view of the game, nor its AI's; blue sees its own and
/// knows only red's start Citadel.
TEST(vision_whatASideDoesNotSeeItDoesNotKnow) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Vec2 blue = citadel(s, 0), red = citadel(s, 1);
    const int64_t lone = add(s, Unit::Kind::ranger, red + normalize(blue - red) * 6, 1);
    Simulation sim(s, m);
    run(sim, 0.2);
    const GameState known = sim.known(0);
    EXPECT_TRUE(!known.unit(lone).has_value());
    EXPECT_TRUE(!sim.seen(0).state.unit(lone).has_value()); // nor its AI's
    std::vector<Structure::Kind> red_;
    for (const auto& b : known.structures) if (b.owner == 1) red_.push_back(b.kind);
    EXPECT_TRUE(red_ == std::vector<Structure::Kind>{Structure::Kind::citadel}); // only where red starts
    EXPECT_TRUE(sim.known(1).unit(lone).has_value()); // red knows its own
}

/// A building seen is remembered as it was: out of sight it stays in
/// the side's view (destroyed meanwhile, too) until its ground is seen
/// again; then it is forgotten.
TEST(vision_buildingsAreRememberedUntilSeenGone) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Vec2 blue = citadel(s, 0), red = citadel(s, 1);
    const Vec2 at = blue + normalize(red - blue) * 18;
    const int64_t habDome = add(s, Structure::Kind::habDome, at, 1);
    const int64_t scout = add(s, Unit::Kind::ranger, at + Vec2(0, 4), 0);
    Simulation sim(s, m);
    run(sim, 0.2);
    EXPECT_TRUE(sim.sees(0, habDome));
    // The scout walks away; the Hab Dome dies out of sight.
    auto& us = sim.state.units;
    us.erase(std::remove_if(us.begin(), us.end(), [&](const Unit& u) { return u.id == scout; }), us.end());
    run(sim, 0.2);
    EXPECT_TRUE(!sim.sees(0, habDome));
    auto& bs = sim.state.structures;
    bs.erase(std::remove_if(bs.begin(), bs.end(), [&](const Structure& b) { return b.id == habDome; }), bs.end());
    run(sim, 0.2);
    EXPECT_TRUE(sim.known(0).structure(habDome).has_value()); // remembered as last seen
    EXPECT_TRUE(sim.unseen(0).count(habDome) > 0);
    // Back to look: gone.
    add(sim.state, Unit::Kind::ranger, at + Vec2(0, 4), 0);
    run(sim, 0.2);
    EXPECT_TRUE(!sim.known(0).structure(habDome).has_value()); // seen empty, forgotten
}

/// A unit lost from sight is kept in mind for `Rules.memory` seconds,
/// where it was last seen, unless that spot is seen empty.
TEST(vision_unitsAreRememberedForAWhile) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Vec2 blue = citadel(s, 0), red = citadel(s, 1);
    const Vec2 at = blue + normalize(red - blue) * 18;
    const int64_t foe = add(s, Unit::Kind::ranger, at, 1);
    const int64_t scout = add(s, Unit::Kind::ranger, at + Vec2(0, 3), 0);
    Simulation sim(s, m);
    run(sim, 0.2);
    auto& us = sim.state.units;
    us.erase(std::remove_if(us.begin(), us.end(), [&](const Unit& u) { return u.id == scout; }), us.end());
    run(sim, 0.2);
    const auto remembered = sim.known(0).unit(foe);
    EXPECT_TRUE(remembered && remembered->position == at); // in mind where last seen
    EXPECT_TRUE(!sim.shown(0).unit(foe).has_value()); // but not shown on screen
    sim.state.time += Rules::memory + 1;
    run(sim, 0.2);
    EXPECT_TRUE(!sim.known(0).unit(foe).has_value()); // forgotten after a while
}

/// A ground unit at the watchtower sees far round it.
TEST(vision_watchtowerSeesFar) {
    GameState s = field();
    const MapDefinition& m = homeMap();
    const Doodad* towerDoodad = nullptr;
    for (const auto& d : m.doodads) if (d.kind == Doodad::Kind::tower) { towerDoodad = &d; break; }
    ASSERT_TRUE(towerDoodad != nullptr);
    const Vec2 tower = towerDoodad->position;
    const Simulation sim0(s, m);
    ASSERT_TRUE(sim0.nav.has_value());
    const NavGrid& nav = *sim0.nav;
    std::optional<Vec2> spot;
    for (int k = 0; k < 21; ++k) {
        const double a = static_cast<double>(k) * 0.3;
        const Vec2 p = tower + Vec2(std::cos(a), std::sin(a)) * 2.2;
        if (nav.walkable(p)) { spot = p; break; }
    }
    ASSERT_TRUE(spot.has_value());
    std::optional<Vec2> far;
    for (const Vec2 d : {Vec2(15, 0), Vec2(-15, 0), Vec2(0, 15), Vec2(0, -15)}) {
        const Vec2 p = tower + d;
        if (nav.walkable(p) && sim0.vision->tierAt(p) <= sim0.vision->tierAt(tower)) { far = p; break; }
    }
    ASSERT_TRUE(far.has_value());
    add(s, Unit::Kind::ranger, *spot, 0);
    const int64_t foe = add(s, Unit::Kind::ranger, *far, 1);
    Simulation sim(s, m);
    run(sim, 0.2);
    EXPECT_TRUE(sim.sees(0, foe)); // the tower's sight reaches 15 cells off
}

/// On the mirrored map each side's start looks the same: blue's sight
/// is red's mirrored, cell for cell.
TEST(vision_sightIsAMirror) {
    const MapDefinition& m = homeMap();
    ASSERT_TRUE(m.mirrorX.has_value());
    const double axis = *m.mirrorX;
    const Simulation sim(GameState::new_(m), m);
    ASSERT_TRUE(sim.vision.has_value());
    const Vision& v = *sim.vision;
    const auto sa = sim.sight(0), sb = sim.sight(1);
    ASSERT_TRUE(sa && sb);
    const auto& a = sa->seen;
    const auto& b = sb->seen;
    int64_t mismatches = 0, seen = 0;
    for (int64_t z = 0; z < v.height; ++z) {
        for (int64_t x = 0; x < v.width; ++x) {
            if (!a[static_cast<size_t>(z * v.width + x)]) continue;
            seen += 1;
            const Vec2 p = v.origin + Vec2(static_cast<double>(x) + 0.5, static_cast<double>(z) + 0.5) * Vision::cell;
            const auto j = v.index(Vec2(2 * axis - p.x, p.y));
            if (!j) { mismatches += 1; continue; }
            if (!b[static_cast<size_t>(*j)]) mismatches += 1;
        }
    }
    EXPECT_TRUE(seen > 100);
    EXPECT_EQ(mismatches, int64_t(0));
    EXPECT_EQ(std::count(a.begin(), a.end(), uint8_t(1)), std::count(b.begin(), b.end(), uint8_t(1)));
}

namespace {

/// `Vision::stamp` as it was before docs/perf.md "Server" round 3: every
/// cell of the circle's box tested on its own.
void stampPerCell(const Vision& v, Vec2 c, double r, std::optional<int8_t> level, std::vector<uint8_t>& seen,
                  std::vector<uint8_t>& near, std::vector<uint64_t>& explored,
                  std::optional<std::pair<double, double>> cone = std::nullopt) {
    const double cell = Vision::cell;
    const int64_t x0 = max<int64_t>(0, static_cast<int64_t>(std::floor((c.x - r - v.origin.x) / cell)));
    const int64_t x1 = min<int64_t>(v.width - 1, static_cast<int64_t>(std::floor((c.x + r - v.origin.x) / cell)));
    const int64_t z0 = max<int64_t>(0, static_cast<int64_t>(std::floor((c.y - r - v.origin.y) / cell)));
    const int64_t z1 = min<int64_t>(v.height - 1, static_cast<int64_t>(std::floor((c.y + r - v.origin.y) / cell)));
    if (!(x0 <= x1 && z0 <= z1)) return;
    const double r2 = r * r;
    const int8_t top = level ? *level : std::numeric_limits<int8_t>::max();
    const Vec2 ahead = cone ? Vec2(std::cos(cone->first), std::sin(cone->first)) : Vec2::zero;
    const double edge = cone ? std::cos(cone->second) : 0.0;
    for (int64_t z = z0; z <= z1; ++z) {
        const double dz = v.origin.y + (static_cast<double>(z) + 0.5) * cell - c.y;
        const double dz2 = dz * dz;
        if (!(dz2 <= r2)) continue;
        for (int64_t x = x0; x <= x1; ++x) {
            const double dx = v.origin.x + (static_cast<double>(x) + 0.5) * cell - c.x;
            if (!(dx * dx + dz2 <= r2)) continue;
            if (cone && dx * ahead.x + dz * ahead.y < edge * std::sqrt(dx * dx + dz2)) continue;
            const size_t i = static_cast<size_t>(z * v.width + x);
            near[i] = 1;
            if (v.tier[i] <= top && !seen[i]) {
                seen[i] = 1;
                explored[i >> 6] |= uint64_t(1) << static_cast<uint64_t>(i & 63);
            }
        }
    }
}

} // namespace

// The stamp fills each row's span of the circle at once (a headlamp's wedge
// skips the root behind the lamp); it marks what the cell-by-cell test
// marked, byte for byte, for circles and wedges of every size, in and off
// the map, on every level and in the air, stamped over each other.
TEST(vision_stampSpansAsEveryCell) {
    const Simulation sim(GameState::new_(homeMap()), homeMap());
    const Vision& v = *sim.vision;
    const size_t cells = static_cast<size_t>(v.width * v.height);
    SeededRandom dice(20261018);
    int64_t marked = 0;
    for (int round = 0; round < 40; ++round) {
        std::vector<uint8_t> seenA(cells, 0), nearA(cells, 0), seenB(cells, 0), nearB(cells, 0);
        std::vector<uint64_t> exA((cells + 63) / 64, 0), exB((cells + 63) / 64, 0);
        for (int k = 0; k < 60; ++k) {
            const double w = static_cast<double>(v.width) * Vision::cell, h = static_cast<double>(v.height) * Vision::cell;
            const Vec2 c = v.origin + Vec2(dice.range(-0.2 * w, 1.2 * w), dice.range(-0.2 * h, 1.2 * h));
            // Round radii and cell-aligned centres too: the ties.
            double r = k % 7 == 0 ? std::round(dice.range(0, 30)) * 0.5 : dice.range(0.01, k % 5 == 0 ? 200.0 : 25.0);
            const Vec2 at = k % 4 == 0 ? Vec2(std::round(c.x * 4) / 4, std::round(c.y * 4) / 4) : c;
            const std::optional<int8_t> level =
                k % 3 == 0 ? std::nullopt : std::optional<int8_t>(static_cast<int8_t>(dice.random(-1, 4)));
            // Headlamp wedges too, narrow and wider than a half turn.
            const auto cone = k % 6 == 1 ? std::optional<std::pair<double, double>>(
                                               std::make_pair(dice.range(-7, 7), k % 12 == 1 ? 0.3927 : dice.range(0.05, 3.0)))
                                         : std::nullopt;
            v.stamp(at, r, level, seenA, nearA, exA, cone);
            stampPerCell(v, at, r, level, seenB, nearB, exB, cone);
        }
        EXPECT_TRUE(seenA == seenB);
        EXPECT_TRUE(nearA == nearB);
        EXPECT_TRUE(exA == exB);
        marked += std::count(nearB.begin(), nearB.end(), uint8_t(1));
    }
    EXPECT_TRUE(marked > 1000);
}

// docs/perf.md "Server" round 22: on the ground the stamp compares eight
// levels at a time; for levels of every byte value (the map's are a few,
// but a level is any int8) and viewers on every level, it marks what the
// cell-by-cell test marks.
TEST(vision_stampComparesLevelsOfEveryByte) {
    const Simulation sim(GameState::new_(homeMap()), homeMap());
    Vision v = *sim.vision;
    SeededRandom dice(22);
    for (int8_t& t : v.tier) t = static_cast<int8_t>(dice.random(-128, 127));
    const size_t cells = static_cast<size_t>(v.width * v.height);
    int64_t seen = 0;
    for (int round = 0; round < 20; ++round) {
        std::vector<uint8_t> seenA(cells, 0), nearA(cells, 0), seenB(cells, 0), nearB(cells, 0);
        std::vector<uint64_t> exA((cells + 63) / 64, 0), exB((cells + 63) / 64, 0);
        for (int k = 0; k < 40; ++k) {
            const double w = static_cast<double>(v.width) * Vision::cell, h = static_cast<double>(v.height) * Vision::cell;
            const Vec2 c = v.origin + Vec2(dice.range(0, w), dice.range(0, h));
            const int8_t level = k % 5 == 0 ? (k % 10 == 0 ? int8_t(-128) : int8_t(127)) : static_cast<int8_t>(dice.random(-128, 127));
            const double r = dice.range(0.3, 30);
            v.stamp(c, r, level, seenA, nearA, exA);
            stampPerCell(v, c, r, level, seenB, nearB, exB);
        }
        EXPECT_TRUE(seenA == seenB);
        EXPECT_TRUE(nearA == nearB);
        EXPECT_TRUE(exA == exB);
        seen += std::count(seenB.begin(), seenB.end(), uint8_t(1));
    }
    EXPECT_TRUE(seen > 1000);
}

// A look fills the last look's arrays again (`Vision::sightInto`): into a
// used Sight, what a new one gets, for both sides of a game in progress.
TEST(vision_sightIntoAUsedSightIsAFreshOne) {
    GameState s = field();
    add(s, Unit::Kind::prospector, citadel(s, 0) + Vec2(6, 0), 0);
    add(s, Unit::Kind::ranger, citadel(s, 1) - Vec2(9, 2), 1);
    const Simulation sim(s, homeMap());
    const Vision& v = *sim.vision;
    for (int64_t p = 0; p < 2; ++p) {
        std::vector<uint64_t> a, b;
        const Sight fresh = v.sight(p, s, a);
        Sight used;
        used.seen.assign(fresh.seen.size(), 1);
        used.near.assign(fresh.near.size() / 2, 1);
        used.ids = {7, 8, 9};
        v.sightInto(used, p, s, b, {}, {}, {}, {}, 0.5);
        EXPECT_TRUE(used.seen == fresh.seen);
        EXPECT_TRUE(used.near == fresh.near);
        EXPECT_TRUE(used.ids == fresh.ids);
        EXPECT_TRUE(a == b);
    }
}

// docs/perf.md "Server" round 7: by day a pilot's daylight takes in the whole
// region, and once a circle on every level has, a look stamps nothing more.
// `coversGrid` says so exactly when the stamp marks every cell (radii right
// at the farthest corner's distance, from centres in and off the map), and
// after such a circle any stamp, on any level, in a cone or not, changes
// nothing.
TEST(vision_aCircleOverTheWholeGridLeavesNothingToStamp) {
    const Simulation sim(GameState::new_(homeMap()), homeMap());
    const Vision& v = *sim.vision;
    const size_t cells = static_cast<size_t>(v.width * v.height);
    SeededRandom dice(7);
    const double w = static_cast<double>(v.width) * Vision::cell, h = static_cast<double>(v.height) * Vision::cell;
    int64_t covering = 0, short_ = 0;
    for (int k = 0; k < 120; ++k) {
        const Vec2 c = v.origin + Vec2(dice.range(-0.3 * w, 1.3 * w), dice.range(-0.3 * h, 1.3 * h));
        // The farthest corner cell's centre, and a hair either side.
        double far = 0;
        for (const double x : {v.origin.x + 0.5 * Vision::cell, v.origin.x + w - 0.5 * Vision::cell})
            for (const double y : {v.origin.y + 0.5 * Vision::cell, v.origin.y + h - 0.5 * Vision::cell})
                far = std::max(far, distance(c, Vec2(x, y)));
        const double r = far * (1 + (k % 3 - 1) * 1e-15) + (k % 5 == 0 ? -1e-3 : 0);
        std::vector<uint8_t> seen(cells, 0), near(cells, 0);
        std::vector<uint64_t> ex((cells + 63) / 64, 0);
        v.stamp(c, r, std::nullopt, seen, near, ex);
        const bool all = std::count(near.begin(), near.end(), uint8_t(1)) == static_cast<std::ptrdiff_t>(cells);
        EXPECT_EQ(v.coversGrid(c, r), all);
        (all ? covering : short_) += 1;
        if (!all) continue;
        const auto seen0 = seen, near0 = near;
        const auto ex0 = ex;
        for (int j = 0; j < 20; ++j) {
            const Vec2 at = v.origin + Vec2(dice.range(0, w), dice.range(0, h));
            const std::optional<int8_t> level =
                j % 3 == 0 ? std::nullopt : std::optional<int8_t>(static_cast<int8_t>(dice.random(-1, 4)));
            const auto cone = j % 4 == 0 ? std::optional<std::pair<double, double>>(std::make_pair(dice.range(0, 6.28), 0.6))
                                         : std::nullopt;
            v.stamp(at, dice.range(0.5, 60), level, seen, near, ex, cone);
        }
        EXPECT_TRUE(seen == seen0 && near == near0 && ex == ex0);
    }
    EXPECT_TRUE(covering > 10 && short_ > 10);
}

// docs/perf.md "Server" round 15: when the widest viewer, on every level,
// takes in the whole grid (by day, a world region's daylight), a sight is
// written at once: what stamping that circle on cleared arrays leaves, into
// a used Sight, over explored bits already set (stray ones past the last
// cell too).
TEST(vision_daylightOverTheWholeGridIsWrittenAtOnce) {
    GameState s = field();
    add(s, Unit::Kind::prospector, citadel(s, 0) + Vec2(6, 0), 0);
    add(s, Unit::Kind::ranger, citadel(s, 1) - Vec2(9, 2), 1);
    const Simulation sim(s, homeMap());
    const Vision& v = *sim.vision;
    const size_t cells = static_cast<size_t>(v.width * v.height), words = (cells + 63) / 64;
    const double w = static_cast<double>(v.width) * Vision::cell, h = static_cast<double>(v.height) * Vision::cell;
    const Vec2 centre = v.origin + Vec2(0.37 * w, 0.61 * h);
    const double r = std::hypot(w, h);
    ASSERT_TRUE(v.coversGrid(centre, r));
    SeededRandom dice(15);
    for (int round = 0; round < 3; ++round) {
        std::vector<uint64_t> got(words, 0);
        for (auto& x : got)
            if (round > 0) x = dice.next();
        std::vector<uint64_t> want = got;
        Sight used;
        used.seen.assign(cells / 3, 9);
        used.near.assign(cells + 5, 1);
        used.ids = {42};
        v.sightInto(used, 0, s, got, {}, {}, {}, {{centre, r}}, 0.5);
        std::vector<uint8_t> seen(cells, 0), near(cells, 0);
        v.stamp(centre, r, std::nullopt, seen, near, want);
        EXPECT_TRUE(used.seen == seen);
        EXPECT_TRUE(used.near == near);
        EXPECT_TRUE(got == want);
        // What it sees of the other side, from the same arrays.
        std::vector<uint64_t> again(words, 0);
        EXPECT_TRUE(v.sight(0, s, again, {}, {}, {}, {{centre, r}}, 0.5).ids == used.ids);
    }
}
