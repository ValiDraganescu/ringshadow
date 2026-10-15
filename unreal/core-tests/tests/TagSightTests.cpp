// A name tag only over a unit the pilot sees (P1's tag fix, TagSight.h):
// out of the pilot's sight (the night's circle) no tag; a wall of ground or
// a building's body between the eye and the head, no tag; in the open, in
// sight, a tag.

#include "test.h"

#include "TagSight.h"

namespace {

using namespace ac;

double flat(Vec2) { return 0; }

} // namespace

TEST(TagSight_inTheOpenAndInSight) {
    EXPECT_TRUE(TagSight::seen(Vec2(10, 0), 2.4, Vec2(-4, 0), 3, std::make_pair(Vec2(0, 0), 20.0), flat, {}));
    // No sight given (the RTS view): any distance.
    EXPECT_TRUE(TagSight::seen(Vec2(200, 0), 2.4, Vec2(-4, 0), 3, std::nullopt, flat, {}));
}

TEST(TagSight_outOfThePilotsSight) {
    // At night the pilot sees 8 cells: a unit 12 out has no tag.
    EXPECT_TRUE(!TagSight::seen(Vec2(12, 0), 2.4, Vec2(-4, 0), 3, std::make_pair(Vec2(0, 0), 8.0), flat, {}));
    EXPECT_TRUE(TagSight::seen(Vec2(7, 0), 2.4, Vec2(-4, 0), 3, std::make_pair(Vec2(0, 0), 8.0), flat, {}));
}

TEST(TagSight_behindARidge) {
    // A ridge 4 cells high between x = 4 and 6.
    auto ridge = [](Vec2 p) { return p.x > 4 && p.x < 6 ? 4.0 : 0.0; };
    EXPECT_TRUE(!TagSight::seen(Vec2(10, 0), 2.4, Vec2(-4, 0), 3, std::nullopt, ridge, {}));
    // From high above it, over the ridge: seen.
    EXPECT_TRUE(TagSight::seen(Vec2(10, 0), 2.4, Vec2(-4, 0), 30, std::nullopt, ridge, {}));
}

TEST(TagSight_behindABuilding) {
    // A Citadel's body, 3 cells round (4, 0), 6 cells high: the unit behind it hidden.
    const std::vector<TagSight::Block> citadel{{Vec2(4, 0), 3, 0, 6}};
    EXPECT_TRUE(!TagSight::seen(Vec2(10, 0), 2.4, Vec2(-4, 0), 3, std::nullopt, flat, citadel));
    // Beside it: seen.
    EXPECT_TRUE(TagSight::seen(Vec2(10, 8), 2.4, Vec2(-4, 8), 3, std::nullopt, flat, citadel));
}
