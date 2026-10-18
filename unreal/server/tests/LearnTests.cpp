// Blueprints learned once (C2, docs/guns-and-practice.md section 4): a copy
// held is learned with the Learn request and used up; a known kind is not
// learned again. Isolated scenes on The First Reach. Run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldMap.h"

#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 93;
const std::string keyA = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

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
    return p;
}

} // namespace

// Two Derrick copies held: the first Learn uses one up and the Derrick is
// known; a second Learn is refused and leaves the other copy; a kind with
// no copy is refused. The client is told what is learned.
TEST(learn_consumesACopyOnce) {
    WorldMap w(first(), params());
    w.seat(a, keyA);
    ProspectorProgress g;
    g.xp = Progression::xp(10);
    g.giveBlueprint(StructureKind::derrick, 2);
    w.setProgress(a, g);
    w.step();
    EXPECT_TRUE(!w.progress(a)->knows(StructureKind::derrick));
    EXPECT_TRUE(!w.learn(a, StructureKind::derrick).has_value());
    EXPECT_TRUE(w.progress(a)->knows(StructureKind::derrick));
    EXPECT_EQ(w.progress(a)->copies(StructureKind::derrick), int64_t(1));
    const auto again = w.learn(a, StructureKind::derrick);
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(*again == "You know the " + Simulation::title(StructureKind::derrick));
    EXPECT_EQ(w.progress(a)->copies(StructureKind::derrick), int64_t(1));
    // No copy: refused, nothing learned.
    const auto none = w.learn(a, StructureKind::habDome);
    ASSERT_TRUE(none.has_value());
    EXPECT_TRUE(!w.progress(a)->knows(StructureKind::habDome));
    // A free kind is known without a copy.
    EXPECT_TRUE(w.learn(a, StructureKind::dummy).has_value());
    const auto m = *w.progressMessage(a);
    EXPECT_TRUE(m.learned == std::set<StructureKind>{StructureKind::derrick});
    EXPECT_EQ(m.blueprints.at(StructureKind::derrick), int64_t(1));
}
