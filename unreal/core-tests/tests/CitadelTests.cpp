// The Citadel's interior (C1, docs/citadel-interior.md): the hall's shape,
// the door, the shelter, the manned corner guns, from hand-built scenes of a
// few seconds.
#include "Defence.h"
#include "SimHelpers.h"

using namespace ac;
using namespace simtest;

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
        EXPECT_EQ(Defence::cornerGun({0, 0}, 0), Vec2(5.5, 5.5));
        EXPECT_EQ(Defence::gunStation({10, 10}, 2), Vec2(6, 6));
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
