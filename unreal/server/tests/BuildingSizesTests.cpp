// C5, the MMO's building sizes (docs/building-sizes.md): every kind 2.4
// times the strategy game's, as the Citadel grew in C1. On each region's
// ground, every 4 cells, how many spots take each kind now against the
// strategy game's size (the C1 doc's count for the Citadel), and the
// Derricks on the wells clear of ore. The tests run from unreal/server.
#include "test.h"

#include "NavGrid.h"
#include "RegionFile.h"
#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::region;

namespace {

/// The kinds a pilot places on open ground in the MMO (the Derrick goes on
/// a well, the Target Dummy keeps its size).
const std::vector<StructureKind> kinds{StructureKind::citadel, StructureKind::habDome,  StructureKind::garrison,
                                       StructureKind::bastion, StructureKind::foundry,  StructureKind::spacedock,
                                       StructureKind::lab,     StructureKind::sentinel, StructureKind::gatling,
                                       StructureKind::landingPad};

/// Spots every 4 cells where `kind` places (the core's rule, off the pads'
/// plates as the server keeps it) with the hall `big` or not.
int spots(const Built& b, StructureKind kind, bool big) {
    Rules::setCitadelHall(big);
    GameState s = GameState::new_(b.map);
    s.structures.clear();
    s.units.clear();
    const Simulation sim(s, b.map, false);
    int n = 0;
    for (double x = b.map.bounds.minX + 4; x < b.map.bounds.maxX - 4; x += 4)
        for (double y = b.map.bounds.minZ + 4; y < b.map.bounds.maxZ - 4; y += 4) {
            const auto at = sim.snap(kind, Vec2(x, y));
            if (at && !sim.placementRefusal(kind, *at) && !touchesPadPlate(b.pads, *at, Rules::radius(kind))) n += 1;
        }
    Rules::setCitadelHall(true);
    return n;
}

} // namespace

TEST(BuildingSizes_theSizesOfC5) {
    // With the hall the MMO's: 2.4 times the side to the nearest cell, 2.4
    // times the height; the strategy game's without.
    const std::map<StructureKind, std::pair<double, double>> table{
        {StructureKind::habDome, {2.5, 2.88}},  {StructureKind::garrison, {3.5, 6.0}}, {StructureKind::bastion, {3.5, 3.36}},
        {StructureKind::derrick, {3.5, 6.0}},   {StructureKind::foundry, {3.5, 5.76}}, {StructureKind::spacedock, {3.5, 7.68}},
        {StructureKind::lab, {2.5, 4.56}},
        {StructureKind::sentinel, {std::round(2 * Rules::sentinelScale) / 2, 1.7 * Rules::sentinelScale}},
        {StructureKind::gatling, {std::round(2 * Rules::gatlingScale) / 2, 1.3 * Rules::gatlingScale}},
        {StructureKind::citadel, {6.0, 12.3}},  {StructureKind::dummy, {0.5, 1.8}}};
    for (const auto& [k, want] : table) {
        EXPECT_NEAR(Rules::radius(k), want.first, 1e-12);
        EXPECT_NEAR(Rules::height(k), want.second, 1e-9);
        EXPECT_NEAR(Rules::footprint(k).radius, want.first, 1e-12);
    }
    // The Landing Pad holds the Dropship landed aligned and centred on it,
    // with its margin: the smallest odd square that does; a region's pad
    // plate as big.
    const double pad = Rules::radius(StructureKind::landingPad);
    const double need = std::max((ShuttleBay::hullFront - ShuttleBay::hullRear) / 2, ShuttleBay::hullHalfWide) + ShuttleBay::padMargin;
    EXPECT_NEAR(pad, ShuttleBay::padHalfSide(), 1e-12);
    EXPECT_TRUE(pad >= need && pad - 1 < need);
    EXPECT_NEAR(region::padReach, pad, 1e-12);
    EXPECT_NEAR(region::padRadius, pad, 1e-12);
    EXPECT_NEAR(Rules::height(StructureKind::landingPad), 0.18, 1e-12);
    // The Lab stays flush on its host's +x side and +y edge.
    EXPECT_NEAR(Rules::addonOffset().x, 6.0, 1e-12);
    EXPECT_NEAR(Rules::addonOffset().y, 1.0, 1e-12);
    Rules::setCitadelHall(false);
    EXPECT_NEAR(Rules::radius(StructureKind::foundry), 1.5, 1e-12);
    EXPECT_NEAR(Rules::radius(StructureKind::habDome), 1.0, 1e-12);
    EXPECT_NEAR(Rules::radius(StructureKind::landingPad), 1.5, 1e-12);
    EXPECT_NEAR(Rules::height(StructureKind::spacedock), 3.2, 1e-12);
    EXPECT_NEAR(Rules::footprint(StructureKind::foundry).radius, 1.5, 1e-12);
    EXPECT_NEAR(Rules::addonOffset().x, 2.5, 1e-12);
    EXPECT_NEAR(Rules::addonOffset().y, 0.5, 1e-12);
    Rules::setCitadelHall(true);
}

TEST(BuildingSizes_spotsOnEveryRegion) {
    // The First Reach's regions (cinder-steps, a generated example, is in
    // no world).
    static const server::LoadedWorld world = [] {
        server::WorldLoad l = server::loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    size_t regions = 0;
    for (const server::WorldRegion& g : world.regions) {
        const Built& b = g.built;
        const std::string name = g.file.id;
        regions += 1;
        std::string line;
        for (const StructureKind k : kinds) {
            const int small = spots(b, k, false), big = spots(b, k, true);
            char cell[64];
            std::snprintf(cell, sizeof cell, " %s %d/%d", std::string(EnumInfo<StructureKind>::names[size_t(k)]).c_str(), big, small);
            line += cell;
            // Every region with open ground still takes every kind.
            if (small > 0) EXPECT_TRUE(big > 0);
        }
        // The Derrick goes on a well whatever is round it: none of the 7x7
        // ones stands on ore (the canyons' prize rings 6 out, C5); how many
        // on a rock (the walk goes round).
        GameState s = GameState::new_(b.map);
        s.structures.clear();
        s.units.clear();
        const Simulation sim(s, b.map, false);
        int wells = 0, onOre = 0, onRock = 0;
        const double r = Rules::radius(StructureKind::derrick);
        for (const WellSpec& w : b.map.wells) {
            wells += 1;
            bool ore = false, rock = false;
            // A mound (2x1, turned) whose middle or ends fall in the square.
            for (const OreDeposit& d : sim.state.patches)
                for (const double along : {-1.0, 0.0, 1.0}) {
                    const Vec2 q = d.position + Vec2(std::cos(d.angle), -std::sin(d.angle)) * along;
                    if (std::abs(q.x - w.position.x) < r && std::abs(q.y - w.position.y) < r) ore = true;
                }
            for (const Doodad& o : b.map.doodads)
                for (const NavGrid::Circle& c : NavGrid::circles(o))
                    if (std::abs(c.first.x - w.position.x) < r + c.second && std::abs(c.first.y - w.position.y) < r + c.second) rock = true;
            onOre += ore;
            onRock += rock;
        }
        std::printf("    %s:%s; derrick wells %d (%d on ore, %d on rock)\n", name.c_str(), line.c_str(), wells, onOre, onRock);
        EXPECT_EQ(onOre, 0);
    }
    EXPECT_TRUE(regions > 0);
}
