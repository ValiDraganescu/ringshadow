// KSTR's next goal on the wire (L1, docs/contracts.md "What next"): the
// progress message carries the goal shown, whole, or none; its news says one
// was done.

#include "test.h"

#include "NetPacket.h"
#include "NetProtocol.h"

namespace {

using namespace ac;
using namespace ac::net;

} // namespace

TEST(progress_carries_the_next_goal) {
    ProgressMessage m;
    m.level = 10;
    Goal g;
    g.key = "foundryPlans";
    g.title = "The Foundry Hunt";
    g.pitch = "Foundry plans are out there, in the Fork wreck in Cold Fork.";
    g.why = "The Foundry makes rig modules.";
    g.hint = "FIND THE FOUNDRY PLANS  ·  THE GOLD COLUMN";
    g.marked = true;
    g.region = "cold-fork";
    g.at = Vec2(-40.25, 12.5);
    g.level = 10;
    g.xp = 270;
    g.hydrogen = 15;
    m.goal = g;
    m.news.push_back(ProgressMessage::News{ProgressMessage::NewsKind::goal, "sentinel", 270, 15, "Eyes on the Sky"});
    ProgressMessage back;
    ASSERT_TRUE(decode(encode(m), back));
    ASSERT_TRUE(back.goal.has_value());
    EXPECT_TRUE(*back.goal == g);
    EXPECT_TRUE(back.news == m.news);
    // None: on the first ten minutes, or every goal done.
    m.goal = std::nullopt;
    ASSERT_TRUE(decode(encode(m), back));
    EXPECT_TRUE(!back.goal.has_value());
    EXPECT_TRUE(kProtocolVersion >= uint16_t(25));
}
