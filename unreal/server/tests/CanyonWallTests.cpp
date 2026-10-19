// Free placement in canyons (Z4, docs/canyon.md "Building"): pilots build
// anywhere standable in a zone, packed as close as the footprints let them,
// and the Brood's walkers route round the buildings while a way is open and
// bite through what walls their path off. A line of Gatling Guns across the
// Drowned Cut's Salt arm (and one across its gorge, the basin's other way
// in), sealed and with a gap; and a wave's straggler that once held
// The Fork's wave out for 19 minutes. Isolated scenes of a minute or two;
// run from unreal/server (as `make test` does).

#include "test.h"

#include "Brood.h"
#include "NavGrid.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;
using State = WorldMap::CanyonState;

constexpr PilotId me = 91;
const std::string key = "z4z4z4z4z4z4z4z4z4z4z4z4z4z4z4z4";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.frontier = 0;
    p.firstStep = 0;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(std::lround(seconds * 60));
    for (int i = 0; i < n; i++) w.step();
}

/// The pilot seated, taken to `region` at region cell `cell`.
size_t seatIn(WorldMap& w, const std::string& region, Vec2 cell) {
    EXPECT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    w.step();
    const size_t r = *w.regionNamed(region);
    EXPECT_TRUE(w.devTransfer(me, r, w.region(r).places()->ground(cell)));
    w.step();
    return r;
}

/// A line of Gatling Guns, their footprints touching, square to the way
/// from region cell `fromCell` to `toCell` at `t` along it: across the floor
/// there and 4 cells into the walls on both sides (the walk's grid steps on
/// slopes the floor's `walkable` run leaves out); `gap` cells of floor left
/// open in its middle. Unfinished, so they neither shoot nor draw the
/// Brood's anger: only the walk is tested. The line's middle, the way
/// across it, its guns.
struct Line {
    Vec2 middle, across;
    std::vector<int64_t> guns;
};

Line wall(WorldMap& w, size_t r, double gap, Vec2 fromCell = Vec2(82, 28), Vec2 toCell = Vec2(92, 31), double t = 0.2) {
    Simulation& sim = w.region(r).simulation();
    const region::Built& places = *w.region(r).places();
    const Vec2 a = places.ground(fromCell), b = places.ground(toCell);
    const Vec2 along = normalize(b - a), side(-along.y, along.x);
    const Vec2 mid = a + (b - a) * t;
    double lo = 0, hi = 0;
    while (lo > -12 && sim.nav->walkable(mid + side * (lo - 0.5))) lo -= 0.5;
    while (hi < 12 && sim.nav->walkable(mid + side * (hi + 0.5))) hi += 0.5;
    const double r_ = Rules::radius(StructureKind::gatling), reachIn = 4;
    Line line{mid, along, {}};
    // The gap in the floor's middle, its edges whole cells from `lo` (C5:
    // with 5-cell guns, guns laid from one side put it on a slope); guns out
    // from it both ways into the walls.
    const double from = lo + std::round(((hi - lo) - gap) / 2), to = from + gap;
    std::vector<double> offs;
    for (double e = from; e > lo - reachIn; e -= 2 * r_) offs.push_back(e - r_);
    for (double e = to; e < hi + reachIn; e += 2 * r_) offs.push_back(e + r_);
    for (const double off : offs) {
        Structure s(sim.state.nextID++, StructureKind::gatling, Pilot::player, mid + side * off, Rules::buildTime(StructureKind::gatling));
        s.hp = Rules::hp(StructureKind::gatling);
        sim.state.structures.push_back(s);
        line.guns.push_back(s.id);
    }
    sim.reindex();
    sim.refreshNav();
    return line;
}

/// The gorge: the basin's other way in. From the Salt tunnel's mouth the
/// ground runs out round the mesa's foot and up the gorge from the south
/// (the canyon is open there), so a line across the Salt arm alone is walked
/// round, as it should be. The gorge's line closes that way.
Line gorge(WorldMap& w, size_t r) { return wall(w, r, 0, Vec2(106, 45), Vec2(106, 49), 0.5); }

/// How far past the line, toward the basin, a walker stands (negative: on
/// the tunnel's side).
double past(const Line& l, Vec2 p) { return dot(p - l.middle, l.across); }

/// The Salt tunnel's mouth (where the walkers come out) and the basin.
int64_t mouthCell(WorldMap& w, size_t r) {
    const Simulation& sim = w.region(r).simulation();
    return *sim.nav->index(*sim.nav->nearestFree(w.region(r).places()->ground(Vec2(74.5, 27.1))));
}
Vec2 basin(WorldMap& w, size_t r) { return w.region(r).places()->ground(Vec2(102.5, 36.6)); }

} // namespace

/// No gap: the arm and the gorge walled off. The walkers come to the arm's
/// line and bite through it; none is past it while it stands.
TEST(Z4_wallOffTheArmAndTheWalkersBiteThrough) {
    WorldMap w(first(), params());
    const size_t r = seatIn(w, "brine-pans", Vec2(100, 60));
    const Line l = wall(w, r, 0);
    gorge(w, r);
    Simulation& sim = w.region(r).simulation();
    EXPECT_TRUE(!sim.nav->reachable(mouthCell(w, r), basin(w, r), Brood::pathReach, false));
    const auto g = w.sendAlongPath(r, "the-drowned-cut", 0, UnitKind::shardling, 4);
    ASSERT_TRUE(g.has_value());
    bool bitten = false, through = false;
    for (int s = 0; s < 90 * 4 && !bitten; s++) {
        stepFor(w, 0.25);
        for (const int64_t id : l.guns)
            if (const auto b = sim.state.structure(id); b && b->hp < Rules::hp(StructureKind::gatling)) bitten = true;
        for (const Unit& u : sim.state.units)
            if (u.bug && u.bug->group == *g && u.hp > 0 && past(l, u.position) > 1.5) through = true;
    }
    EXPECT_TRUE(bitten);
    EXPECT_TRUE(!through);
    int biting = 0;
    for (const Unit& u : sim.state.units)
        if (u.bug && u.bug->group == *g && u.bug->breach) biting++;
    std::printf("  %d of 4 biting the line\n", biting);
    EXPECT_TRUE(biting >= 1);
}

/// A gap in the arm's line, the gorge closed: the walkers file through the
/// gap to the basin and leave the line alone. Two cells: buildings snap to
/// whole cells, and the walk keeps `NavGrid::clearance` (0.35) off every
/// footprint, so a one-cell gap is a wall to them (they bite through it).
TEST(Z4_aGapAndTheWalkersFileThrough) {
    WorldMap w(first(), params());
    const size_t r = seatIn(w, "brine-pans", Vec2(100, 60));
    {
        WorldMap one(first(), params());
        const size_t r1 = seatIn(one, "brine-pans", Vec2(100, 60));
        wall(one, r1, 1.0);
        gorge(one, r1);
        EXPECT_TRUE(!one.region(r1).simulation().nav->reachable(mouthCell(one, r1), basin(one, r1), Brood::pathReach, false));
    }
    const Line l = wall(w, r, 2.0);
    gorge(w, r);
    Simulation& sim = w.region(r).simulation();
    EXPECT_TRUE(sim.nav->reachable(mouthCell(w, r), basin(w, r), Brood::pathReach, false));
    const auto g = w.sendAlongPath(r, "the-drowned-cut", 0, UnitKind::shardling, 4);
    ASSERT_TRUE(g.has_value());
    int through = 0;
    for (int s = 0; s < 90 * 4 && through < 4; s++) {
        stepFor(w, 0.25);
        through = 0;
        for (const Unit& u : sim.state.units)
            if (u.bug && u.bug->group == *g && u.hp > 0 && past(l, u.position) > 3) through++;
    }
    std::printf("  %d of 4 through the gap\n", through);
    EXPECT_EQ(through, 4);
    for (const int64_t id : l.guns) {
        const auto b = sim.state.structure(id);
        ASSERT_TRUE(b.has_value());
        EXPECT_NEAR(b->hp, Rules::hp(StructureKind::gatling), 1e-9);
    }
}

/// Packed: Gatling Guns side by side on The Fork's floor, footprints
/// touching, all taken by the ordinary rules (no spacing, no build spots).
/// C5: a 5x5 gun nearly fills an arm's floor (cut 5 to 6 wide), so the
/// first goes where the arm's floor takes one, the next touching it along
/// whichever axis the floor runs.
TEST(Z4_packedOnTheFloor) {
    WorldMap w(first(), params());
    const size_t r = seatIn(w, "cold-fork", Vec2(100, 52));
    Simulation& sim = w.region(r).simulation();
    const region::Zone& z = w.region(r).places()->zones[0];
    const double side = 2 * Rules::radius(StructureKind::gatling);
    auto put = [&](Vec2 at) {
        Structure s(sim.state.nextID++, StructureKind::gatling, Pilot::player, at);
        s.hp = Rules::hp(StructureKind::gatling);
        sim.state.structures.push_back(s);
        sim.reindex();
        sim.refreshNav();
    };
    // Along the North arm's floor from its end: the first spot that takes
    // one with a neighbour touching it.
    int placed = 0;
    std::optional<Vec2> first_;
    const auto& line = z.paths[0].points;
    for (size_t k = line.size() - 1; k > 0 && !first_; k--)
        for (double t = 0; t < 1 && !first_; t += 0.1) {
            const auto at = sim.snap(StructureKind::gatling, line[k] + (line[k - 1] - line[k]) * t);
            if (!at || sim.placementRefusal(StructureKind::gatling, *at)) continue;
            for (const Vec2 off : {Vec2(side, 0), Vec2(-side, 0), Vec2(0, side), Vec2(0, -side)}) {
                if (first_) break;
                const auto next = sim.snap(StructureKind::gatling, *at + off);
                if (!next || sim.placementRefusal(StructureKind::gatling, *next)) continue;
                put(*at);
                if (!sim.placementRefusal(StructureKind::gatling, *next)) {
                    put(*next);
                    placed = 2;
                    first_ = at;
                } else {
                    sim.state.structures.pop_back();
                    sim.reindex();
                    sim.refreshNav();
                }
            }
        }
    std::printf("  %d placed touching\n", placed);
    ASSERT_TRUE(first_.has_value());
    EXPECT_EQ(placed, 2);
    // One more on top of the first is refused (the footprint rule, no other).
    EXPECT_TRUE(sim.placementRefusal(StructureKind::gatling, *first_).has_value());
}

/// B2's straggler: a wave's Lodeback, hit, fled with its sac toward the
/// region's nearest edge, behind The Fork's wall, and stood where it could
/// get no nearer until the wave's time ran out. Now a wave's bugs leave by
/// their own tunnel, and one that can get no nearer goes where it stands:
/// every wave of a full cycle resolves within two minutes.
TEST(Z4_aWaveAlwaysResolves) {
    WorldMap w(first(), params());
    const size_t r = seatIn(w, "cold-fork", Vec2(100, 52));
    std::printf("  %s\n", w.devCanyon("canyon turrets 12 5 0").c_str());
    double out = -1, longest = 0;
    int64_t waves = 0;
    for (int i = 0; i < 60 * 60 * 45; i++) {
        w.step();
        const WorldMap::Canyon& c = *w.canyon(r, "the-fork");
        if (c.out && out < 0) out = w.clock();
        if (!c.out && out >= 0) {
            longest = std::max(longest, w.clock() - out);
            waves++;
            out = -1;
        }
        if (c.state == State::held || c.state == State::failed) break;
    }
    std::printf("  %lld waves, the longest out %.0f s\n", (long long)waves, longest);
    EXPECT_TRUE(waves >= 8);
    EXPECT_TRUE(longest < 120);
}
