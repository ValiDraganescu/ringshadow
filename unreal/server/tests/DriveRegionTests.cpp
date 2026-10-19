// C4, the drive (docs/haul-and-ferry.md "The drive"): a vehicle climbs no
// steeper than its gear lets it (wheels 25°, tracks 35°), so every region
// must leave the Mule its ordinary routes: from its first spawn point (or
// pad, or opening) to every pad (the Dropship's) and every opening,
// wherever a Prospector on foot gets. The sites (crates, vaults) it only
// reports. The tests run from unreal/server.
#include "test.h"

#include "NavGrid.h"
#include "RegionFile.h"
#include "TerrainField.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ac;
using namespace ac::region;

TEST(DriveRegion_theMuleReachesWhatAWalkerReaches) {
    const uint8_t wheels = NavGrid::steepFor(UnitKind::mule);
    size_t regions = 0;
    for (const auto& entry : std::filesystem::directory_iterator("regions")) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream in(entry.path());
        std::stringstream text;
        text << in.rdbuf();
        const Parsed p = parse(text.str());
        ASSERT_TRUE(p.file.has_value());
        const Built b = build(*p.file);
        regions += 1;
        const TerrainField field(b.map);
        NavGrid nav(b.map, field);
        GameState s = GameState::new_(b.map);
        s.structures.clear();
        s.units.clear();
        nav.setDynamic(s);
        // How much of the open ground is too steep for wheels.
        size_t open = 0, steep = 0;
        for (size_t i = 0; i < nav.passable.size(); i++) {
            if (!(nav.passable[i] & 1)) continue;
            open += 1;
            if (nav.passable[i] & wheels) steep += 1;
        }
        std::vector<std::pair<std::string, Vec2>> places;
        for (const Place& q : b.pads) places.emplace_back("pad " + q.name, q.at);
        for (size_t e = 0; e < 4; e++) {
            for (const Span& span : b.openings[e]) {
                const double along = (span.from + span.to) / 2;
                places.emplace_back(std::string("opening ") + std::string(edgeName(static_cast<Edge>(e))),
                                    b.ground(b.edgePoint(static_cast<Edge>(e), along, laneDepth)));
            }
        }
        if (places.empty() && b.spawns.empty()) continue;
        const Vec2 from = !b.spawns.empty() ? b.spawns[0].at : places[0].second;
        const size_t routes = places.size();
        for (const Site& q : b.sites) places.emplace_back("site " + q.name, q.at);
        size_t walked = 0, rolled = 0, k = 0;
        for (const auto& [name, at] : places) {
            const bool site = k++ >= routes;
            // Where a walker gets (within a pad's plate, a site's reach).
            if (!nav.path(from, at, 2.0)) continue;
            const bool reached = nav.path(from, at, 2.0, false, wheels).has_value();
            if (!site) {
                walked += 1;
                if (reached) rolled += 1;
            }
            if (!reached) {
                std::printf("    %s: the Mule cannot reach %s (%.1f, %.1f)\n", entry.path().filename().c_str(), name.c_str(), at.x, at.y);
            }
        }
        std::printf("    %s: %zu of %zu pads and openings on wheels; %.1f%% of the open ground too steep for wheels\n",
                    entry.path().filename().c_str(), rolled, walked, open ? 100.0 * double(steep) / double(open) : 0.0);
        EXPECT_EQ(rolled, walked);
    }
    EXPECT_TRUE(regions > 0);
}
