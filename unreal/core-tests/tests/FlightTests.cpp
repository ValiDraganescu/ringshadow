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
