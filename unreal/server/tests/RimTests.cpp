// No straight edge (docs/world-sculpting.md, "No straight edge"): every
// region of the First Reach as `expand` draws it: no cliff, cliff-walled cut
// or rim runs straight for `region::straightRun` cells, no three of the
// walls' rocks stand in a row, and the canyon zones' worn walls still leave
// every path on open floor. Reads the world files; run from unreal/server
// (as `make test` does).

#include "test.h"

#include "NavGrid.h"
#include "TerrainField.h"
#include "WorldMap.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

} // namespace

/// N = 8 cells, the critic's own rule (`straightRun`): an edge that keeps
/// within a quarter cell of one line for 8 cells reads as drawn by hand.
TEST(Rim_noStraightRunInAnyRegion) {
    for (const WorldRegion& r : first().regions) {
        const auto runs = region::straightRuns(region::expand(r.file));
        for (const region::StraightRun& s : runs)
            std::printf("  %s: %s from (%.1f, %.1f) to (%.1f, %.1f)\n", r.file.id.c_str(), s.what.c_str(), s.from.x, s.from.y, s.to.x,
                        s.to.y);
        EXPECT_TRUE(runs.empty());
    }
}

/// The measure finds what it should: a hand-drawn square cliff and a
/// straight rim both count, a passage's strip along the edge does not.
TEST(Rim_theMeasureSeesAStraightCliffAndRim) {
    region::RegionFile f = first().regions[0].file;
    f.sculpt.reset();
    f.height.clear();
    region::Edit square;
    square.op = HeightEdit::Kind::raise;
    square.area.points = {Vec2(40, 40), Vec2(52, 40), Vec2(52, 52), Vec2(40, 52)};
    square.level = 2;
    f.height.push_back(square);
    f.border->islands.clear();
    f.border->outline = {Vec2(3, 3), Vec2(141, 3), Vec2(141, 93), Vec2(3, 93)};
    const auto runs = region::straightRuns(f);
    int cliff = 0, rim = 0;
    for (const region::StraightRun& s : runs) (s.what == "border.outline" ? rim : cliff)++;
    EXPECT_EQ(cliff, 4);
    EXPECT_EQ(rim, 4);
    // A strip along the north edge: only its inner side shows (the other
    // three lie on the region's edge), beside the square's four.
    f.border->outline = {Vec2(0, 3), Vec2(144, 3), Vec2(144, 0), Vec2(0, 0)};
    EXPECT_EQ(region::straightRuns(f).size(), size_t(5));
}

/// The walls' rocks and the canyons' fallen blocks: past the land, in
/// clusters, never three in a row.
TEST(Rim_noThreeWallRocksInARow) {
    for (const WorldRegion& r : first().regions) {
        const MapDefinition& m = r.built.map;
        if (!m.edges) continue;
        const TerrainField field(m);
        std::vector<Vec2> rocks;
        for (const Doodad& d : m.doodads)
            if ((d.kind == Doodad::Kind::rock || d.kind == Doodad::Kind::boulder || d.kind == Doodad::Kind::rockSpire) &&
                field.landOutside(d.position) > 2.0)
                rocks.push_back(d.position);
        int rows = 0;
        for (size_t a = 0; a < rocks.size(); a++)
            for (size_t b = a + 1; b < rocks.size(); b++)
                for (size_t c = b + 1; c < rocks.size(); c++)
                    if (region::row(rocks[a], rocks[b], rocks[c]) || region::row(rocks[b], rocks[a], rocks[c]) ||
                        region::row(rocks[a], rocks[c], rocks[b])) {
                        rows++;
                        const Vec2 pa = r.built.regionAt(rocks[a]), pb = r.built.regionAt(rocks[b]), pc = r.built.regionAt(rocks[c]);
                        std::printf("  row: (%.1f, %.1f) (%.1f, %.1f) (%.1f, %.1f)\n", pa.x, pa.y, pb.x, pb.y, pc.x, pc.y);
                    }
        std::printf("  %s: %zu wall rocks, %d rows\n", r.file.id.c_str(), rocks.size(), rows);
        EXPECT_TRUE(rocks.size() >= 15);
        EXPECT_EQ(rows, 0);
    }
}

/// The canyon zones' worn walls leave every walkers' path on open floor,
/// from its tunnel to the prize, and the walk from each mouth to the prize
/// short.
TEST(Rim_canyonPathsStayOnTheFloor) {
    int zones = 0;
    for (const WorldRegion& r : first().regions) {
        if (r.built.zones.empty()) continue;
        const MapDefinition& m = r.built.map;
        const TerrainField field(m);
        NavGrid nav(m, field);
        GameState ground = GameState::new_(m);
        ground.structures.clear();
        ground.units.clear();
        nav.setDynamic(ground);
        for (const region::Zone& z : r.built.zones) {
            zones++;
            for (const region::Zone::Path& p : z.paths) {
                // A roost's path is the flyers' own, over the walls.
                const auto mouth = std::find_if(z.entrances.begin(), z.entrances.end(), [&](const region::Zone::Entrance& e) { return e.name == p.from; });
                if (mouth != z.entrances.end() && mouth->roost) continue;
                const Vec2 a = p.points.front(), b = p.points.back();
                const auto from = nav.nearestFree(a), to = nav.nearestFree(b);
                ASSERT_TRUE(from && to);
                EXPECT_TRUE(distance(*from, a) < 2.5 && distance(*to, b) < 2.5);
                const auto way = nav.path(*from, *to, 1.0);
                ASSERT_TRUE(way.has_value());
                double len = distance(*from, way->front()), along = 0;
                for (size_t i = 0; i + 1 < way->size(); i++) len += distance((*way)[i], (*way)[i + 1]);
                for (size_t i = 0; i + 1 < p.points.size(); i++) along += distance(p.points[i], p.points[i + 1]);
                std::printf("  %s %s from %s: walked %.0f cells, the path %.0f\n", r.file.id.c_str(), z.id.c_str(), p.from.c_str(), len, along);
                // The floor still runs the way the path says, not round the canyon.
                EXPECT_TRUE(len < along * 1.4 + 4);
            }
        }
    }
    EXPECT_EQ(zones, 3);
}
