// A pad flight's MH price (F1, docs/prospector-progression.md "MH sinks"):
// a base and a share a grid step, and the refusal when the wallet is short.
#include "test.h"

#include "Flight.h"

using namespace ac;

TEST(Flight_priceIsABaseAndAShareAStep) {
    EXPECT_EQ(Flight::steps(1, 1, 1, 1), int64_t(0));
    EXPECT_EQ(Flight::steps(1, 1, 2, 1), int64_t(1));
    EXPECT_EQ(Flight::steps(0, 0, 2, 1), int64_t(3));
    EXPECT_EQ(Flight::steps(2, 1, 0, 0), int64_t(3));
    EXPECT_EQ(Flight::hydrogen(0), int64_t(10));
    EXPECT_EQ(Flight::hydrogen(1), int64_t(15));
    EXPECT_EQ(Flight::hydrogen(4), int64_t(30));
}

TEST(Flight_refusedWhenTheWalletIsShort) {
    EXPECT_TRUE(!Flight::refusal(15, 15).has_value());
    EXPECT_TRUE(!Flight::refusal(15, 400).has_value());
    EXPECT_EQ(Flight::refusal(15, 4).value_or(""), std::string("Not enough MH to fly: 15 MH, you have 4"));
    EXPECT_EQ(Flight::refusal(10, -3).value_or(""), std::string("Not enough MH to fly: 10 MH, you have 0"));
}

/// F2: half from a Spacedock (rounded up), nothing with Ringwalker.
TEST(Flight_spacedockHalvesRingwalkerFree) {
    EXPECT_EQ(Flight::price(0, false, false), int64_t(10));
    EXPECT_EQ(Flight::price(3, false, false), int64_t(25));
    EXPECT_EQ(Flight::price(3, true, false), int64_t(13));
    EXPECT_EQ(Flight::price(2, true, false), int64_t(10));
    EXPECT_EQ(Flight::price(3, false, true), int64_t(0));
    EXPECT_EQ(Flight::price(3, true, true), int64_t(0));
}

/// F2: the Dropship's flight runs 12 to 25 s with the step out, its stages
/// in order; the dash grows with the grid steps up to its most; Short hop
/// cuts the dash only; Ringwalker makes every stage 30% faster.
TEST(Ferry_stagesAndTimes) {
    const Ferry::Plan near = Ferry::plan(0);
    EXPECT_NEAR(near.boardAt(), 3.0, 1e-12);
    EXPECT_NEAR(near.liftAt(), 4.5, 1e-12);
    EXPECT_NEAR(near.turnsAt(), 7.0, 1e-12);
    EXPECT_NEAR(near.dashAt(), 10.0, 1e-12);
    EXPECT_NEAR(near.approachAt(), 12.0, 1e-12);
    EXPECT_NEAR(near.total(), 16.0, 1e-12);
    EXPECT_NEAR(Ferry::plan(2).dash, 4.0, 1e-12);
    EXPECT_NEAR(Ferry::plan(9).dash, Ferry::dashMost, 1e-12);
    for (int64_t steps = 0; steps <= 12; steps++)
        for (const double hop : {0.0, -0.2, -0.6})
            for (const double faster : {0.0, 0.3}) {
                const Ferry::Plan p = Ferry::plan(steps, hop, faster);
                const double all = p.total() + Ferry::stepOut;
                EXPECT_TRUE(all >= 12 && all <= 25);
            }
    const Ferry::Plan hop = Ferry::plan(2, -0.6);
    EXPECT_NEAR(hop.dash, 1.6, 1e-12);
    EXPECT_NEAR(hop.lift, Ferry::lift, 1e-12);
    const Ferry::Plan quick = Ferry::plan(2, 0, 0.3);
    EXPECT_NEAR(quick.total() * 1.3, Ferry::plan(2).total(), 1e-9);
}
