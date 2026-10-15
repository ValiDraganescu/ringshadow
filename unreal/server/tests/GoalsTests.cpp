// KSTR's next goal (L1, docs/contracts.md "What next"; WorldGoals.cpp): after
// the first ten minutes one goal at a time, chosen by level and what the
// pilot has done; goals met before they come up skipped unpaid; the one shown
// paid a little when done; a wait for a level; kept through the save; an old
// save's pilot picking up at the first goal it hasn't met. Isolated scenes of
// a few seconds on The First Reach; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId me = 62;
const std::string key = "a1a2a3a4a5a6a7a8a9aaabacadaeaf00";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// A new pilot at level `level`, past the first ten minutes (`step` 0) or on
/// its step `step`.
WorldParams params(int64_t level, int step = 0) {
    WorldParams p;
    p.contractSeed = 77;
    p.firstStep = step;
    p.devXp = Progression::xp(level);
    return p;
}

/// A second of world time: the goals are checked once a second.
void second(WorldMap& w, int seconds = 1) {
    for (int i = 0; i < seconds * 60 + 2; i++) w.step();
}

std::optional<net::Goal> goal(WorldMap& w) {
    const auto m = w.progressMessage(me);
    return m ? m->goal : std::nullopt;
}

std::string goalKey(WorldMap& w) { return goal(w) ? goal(w)->key : std::string("none"); }

int paid(const std::vector<net::ProgressMessage::News>& news, std::string* id = nullptr) {
    int n = 0;
    for (const auto& x : news)
        if (x.kind == NewsKind::goal) {
            n += 1;
            if (id) *id = x.id;
        }
    return n;
}

std::optional<WorldSave> saved(WorldMap& w, const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string p = "build/save-tests/" + name + ".sqlite";
    std::remove(p.c_str());
    std::remove((p + "-wal").c_str());
    std::remove((p + "-shm").c_str());
    std::string why;
    {
        auto store = WorldStore::open(p, "first", &why);
        if (!store || !store->write(w.save(), &why)) return std::nullopt;
    }
    auto store = WorldStore::open(p, "first", &why);
    return store ? store->load(&why) : std::nullopt;
}

} // namespace

// The chain is in order and its keys are its own; the pay is a little.
TEST(goals_chain_and_pay) {
    const auto& chain = WorldGoals::chain();
    EXPECT_TRUE(chain.size() >= size_t(14));
    EXPECT_TRUE(std::string(chain.front().key) == "derrick");
    EXPECT_TRUE(std::string(chain.back().key) == "spacedock");
    // N4: "Bring Her Down" at level 20, between the Foundry's and the Lab's.
    ASSERT_TRUE(WorldGoals::find("matriarch") != nullptr);
    EXPECT_EQ(WorldGoals::find("matriarch")->level, int64_t(20));
    EXPECT_TRUE(std::string(WorldGoals::find("matriarch")->title) == "Bring Her Down");
    for (const auto& e : chain) {
        EXPECT_TRUE(WorldGoals::find(e.key) == &e);
        EXPECT_TRUE(*e.title && *e.pitch && *e.why && *e.hint);
    }
    // 3 minutes of play: 240 XP and 10 MH at level 4, more at 40.
    EXPECT_NEAR(WorldGoals::xp(4), 240, 1e-9);
    EXPECT_EQ(WorldGoals::hydrogen(4), int64_t(10));
    EXPECT_TRUE(WorldGoals::xp(40) > WorldGoals::xp(4));
    EXPECT_TRUE(WorldGoals::xp(4) < WorldContracts::xp(net::Contract::Kind::deliverOre, 4));
}

// None while the first ten minutes run; the first goal as soon as they end.
TEST(goals_start_after_the_first_ten_minutes) {
    WorldMap w(first(), params(4, 6));
    ASSERT_TRUE(w.join(me));
    second(w, 2);
    EXPECT_TRUE(!goal(w).has_value());
    const double before = w.progress(me)->xp;
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill).has_value());
    second(w, 2);
    std::printf("  the chain over: %.0f XP (was %.0f), level %lld\n", w.progress(me)->xp, before, (long long)w.progress(me)->level());
    // No Derrick of its own (the dev step skipped 5): the first goal.
    ASSERT_TRUE(goal(w).has_value());
    EXPECT_TRUE(goal(w)->key == "derrick");
    EXPECT_TRUE(goal(w)->title == "Pump Some Silver");
    EXPECT_TRUE(!goal(w)->pitch.empty() && !goal(w)->why.empty() && !goal(w)->hint.empty());
    // Its marker: the nearest well with MH, here.
    EXPECT_TRUE(goal(w)->marked);
    EXPECT_TRUE(goal(w)->region == "start");
    EXPECT_NEAR(goal(w)->xp, WorldGoals::xp(4), 1e-9);
}

// The one shown, done: paid once (news, XP, MH), the next one at once.
TEST(goals_completion_pays_and_the_next_comes) {
    WorldMap w(first(), params(4));
    ASSERT_TRUE(w.join(me));
    second(w);
    EXPECT_TRUE(goalKey(w) == "derrick");
    EXPECT_TRUE(w.devGoal("goal pass derrick").find("goal skills") != std::string::npos);
    // Three points waiting at level 4: the hint counts them.
    EXPECT_TRUE(goal(w)->hint.find("3 LEFT") != std::string::npos);
    w.takeNews(me);
    const double xp = w.progress(me)->xp;
    const int64_t mh = w.wallet(me)->hydrogen;
    for (int i = 0; i < 3; i++) ASSERT_TRUE(!w.spend(me, Skill::quickDrill).has_value());
    second(w);
    std::string id;
    EXPECT_EQ(paid(w.takeNews(me), &id), 1);
    EXPECT_TRUE(id == "skills");
    EXPECT_NEAR(w.progress(me)->xp - xp, WorldGoals::xp(4), 1e-6);
    EXPECT_EQ(w.wallet(me)->hydrogen - mh, WorldGoals::hydrogen(4));
    EXPECT_TRUE(goalKey(w) == "mission");
    // The Citadel's goals point at the realm's Citadel: none yet, no marker.
    EXPECT_TRUE(!goal(w)->marked);
    second(w, 2);
    EXPECT_EQ(paid(w.takeNews(me)), 0);
}

// Goals met before they come up pass unpaid; skill points spent at once leave
// "Spend your points" for the next level's point.
TEST(goals_skip_what_is_met) {
    WorldMap w(first(), params(4));
    ASSERT_TRUE(w.join(me));
    for (int i = 0; i < 3; i++) ASSERT_TRUE(!w.spend(me, Skill::quickDrill).has_value());
    second(w);
    w.devGoal("goal pass derrick");
    // No points: "skills" is not offered; the mission is.
    EXPECT_TRUE(goalKey(w) == "mission");
    w.devGoal("goal pass mission");
    EXPECT_TRUE(goalKey(w) == "shop");
    // Level 5 (a point): the gatling and the mercenary open, but the points
    // come first in the chain.
    w.devGoal("goal level 5");
    // `goal level` passes the goals under level 5: the gatling is next.
    EXPECT_TRUE(goalKey(w) == "gatling");
    EXPECT_EQ(paid(w.takeNews(me)), 0);
}

// Chosen by level: a level-10 pilot past the earlier goals gets its first
// active slot; the Foundry hunt marks the plans' site on its map.
TEST(goals_chosen_by_level) {
    WorldMap w(first(), params(4));
    ASSERT_TRUE(w.join(me));
    second(w);
    EXPECT_TRUE(w.devGoal("goal level 10").find("goal active, level 10") != std::string::npos);
    EXPECT_TRUE(goal(w)->title == "Something for Q");
    EXPECT_TRUE(goal(w)->hint == "K  SLOT AN ACTIVE ON Q");
    w.devGoal("goal pass active");
    EXPECT_TRUE(goalKey(w) == "sentinel");
    w.devGoal("goal pass sentinel");
    EXPECT_TRUE(goalKey(w) == "foundryPlans");
    ASSERT_TRUE(goal(w)->marked);
    const std::string site = goal(w)->region + "/" + "Fork wreck";
    EXPECT_TRUE(goal(w)->region == "cold-fork");
    EXPECT_TRUE(goal(w)->pitch.find("Fork wreck") != std::string::npos);
    EXPECT_TRUE(w.progress(me)->known.count(site) == 1);
    // The Foundry's own goal waits for level 15.
    w.devGoal("goal pass foundryPlans");
    ASSERT_TRUE(goal(w).has_value());
    EXPECT_TRUE(goal(w)->key == "level");
    EXPECT_EQ(goal(w)->level, int64_t(15));
    EXPECT_TRUE(goal(w)->hint == "REACH LEVEL 15  ·  YOU ARE 10");
    EXPECT_NEAR(goal(w)->xp, 0, 1e-9);
}

// A wait pays nothing: at the level the next goal comes.
TEST(goals_wait_for_a_level) {
    WorldMap w(first(), params(4));
    ASSERT_TRUE(w.join(me));
    for (int i = 0; i < 3; i++) ASSERT_TRUE(!w.spend(me, Skill::quickDrill).has_value());
    second(w);
    for (const char* k : {"derrick", "mission", "shop"}) w.devGoal(std::string("goal pass ") + k);
    EXPECT_TRUE(goalKey(w) == "level");
    EXPECT_EQ(goal(w)->level, int64_t(5));
    EXPECT_TRUE(goal(w)->pitch.find("Guard Your Ore") != std::string::npos);
    w.takeNews(me);
    w.earn(me, Progression::xp(5) - w.progress(me)->xp);
    second(w);
    EXPECT_EQ(paid(w.takeNews(me)), 0);
    // Level 5's point first, then the gatling.
    EXPECT_TRUE(goalKey(w) == "skills");
}

// Kept through the save: the goal shown, the passed and the done.
TEST(goals_kept_through_the_save) {
    WorldMap w(first(), params(4));
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    second(w);
    w.devGoal("goal pass derrick");
    EXPECT_TRUE(goalKey(w) == "skills");
    auto back = saved(w, "goals");
    ASSERT_TRUE(back.has_value());
    ASSERT_TRUE(back->pilots.size() == size_t(1));
    EXPECT_TRUE(back->pilots[0].goals.find("\"goal\":\"skills\"") != std::string::npos);
    EXPECT_TRUE(back->pilots[0].goals.find("derrick") != std::string::npos);
    WorldMap again(first(), params(4));
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(goalKey(again) == "skills");
    second(again);
    EXPECT_TRUE(goalKey(again) == "skills");
    EXPECT_EQ(paid(again.takeNews(me)), 0);
}

// An old save (no goals column): the pilot picks up at the first goal it
// hasn't met, those it has passed unpaid (a supply bought before: no shop
// goal).
TEST(goals_old_save_picks_up_at_the_right_goal) {
    WorldParams p = params(4);
    p.devRig = true;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    second(w);
    ASSERT_TRUE(!w.buy(me, std::nullopt, Item::repairKit).has_value());
    auto back = saved(w, "goals-old");
    ASSERT_TRUE(back.has_value());
    back->pilots[0].goals = "";
    WorldMap again(first(), p);
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    again.takeNews(me);
    second(again);
    // The dev rig has no Derrick: the first goal.
    EXPECT_TRUE(goalKey(again) == "derrick");
    again.devGoal("goal pass derrick");
    again.devGoal("goal pass skills");
    EXPECT_TRUE(goalKey(again) == "mission");
    again.devGoal("goal pass mission");
    // The repair kit held: the shop is met, passed on the way.
    EXPECT_TRUE(goalKey(again) == "gatling");
    EXPECT_EQ(paid(again.takeNews(me)), 0);
}

// A finished Foundry leads to the rig screen: order a Drill head Mk I (its
// price from the rules), then fit it where you unload; a finished Lab leads
// to researching Mk II. Each paid when done, the next at once.
TEST(goals_the_foundry_and_the_lab_lead_to_the_rig_screen) {
    WorldParams p = params(4);
    p.devRig = true;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    second(w);
    // The dev rig's Foundry and Lab, with an empty rig.
    ProspectorProgress g = *w.progress(me);
    g.rig = Rig{};
    w.setProgress(me, g);
    w.devGoal("goal level 15");
    ASSERT_TRUE(goalKey(w) == "module");
    std::printf("  %s\n", goal(w)->pitch.c_str());
    EXPECT_TRUE(goal(w)->pitch.find("Your Foundry is up: J, order a Drill head Mk I (100 ore + 50 MH, 60 s)") != std::string::npos);
    EXPECT_TRUE(goal(w)->hint == "J  RIG SCREEN  ·  ORDER A DRILL HEAD MK I");
    w.takeNews(me);
    ASSERT_TRUE(!w.make(me, Module::drillHead, 1).has_value());
    second(w);
    std::string id;
    EXPECT_EQ(paid(w.takeNews(me), &id), 1);
    EXPECT_TRUE(id == "module");
    // Ordered: fit it when it comes, at the Citadel (marked).
    ASSERT_TRUE(goalKey(w) == "fit");
    std::printf("  %s\n", goal(w)->pitch.c_str());
    EXPECT_TRUE(goal(w)->pitch.find("Your Drill head is in the stores") != std::string::npos);
    EXPECT_TRUE(goal(w)->marked);
    second(w, 62);
    EXPECT_EQ(w.progress(me)->rig.stock(Module::drillHead, 1), int64_t(1));
    EXPECT_TRUE(goalKey(w) == "fit");
    g = *w.progress(me);
    ASSERT_TRUE(g.rig.fit(Module::drillHead, 1));
    w.setProgress(me, g);
    w.takeNews(me);
    second(w);
    EXPECT_EQ(paid(w.takeNews(me), &id), 1);
    EXPECT_TRUE(id == "fit");
    // N4: at level 20 the chain leads to her nest and THE HUNT first.
    ASSERT_TRUE(goalKey(w) == "matriarch");
    std::printf("  %s\n", goal(w)->pitch.c_str());
    EXPECT_TRUE(goal(w)->title == "Bring Her Down");
    EXPECT_TRUE(goal(w)->pitch.find("She's in Slag Pockets") != std::string::npos);
    EXPECT_TRUE(goal(w)->pitch.find("Call THE HUNT on the Citadel's REALM tab") != std::string::npos);
    EXPECT_TRUE(goal(w)->hint == "H  AT A CITADEL  ·  REALM  ·  CALL THE HUNT  ·  SLAG POCKETS");
    EXPECT_TRUE(goal(w)->marked);
    w.devGoal("goal pass matriarch");
    // The Lab is up (the dev rig's): passed unpaid on the way; research Mk II.
    ASSERT_TRUE(goalKey(w) == "research");
    std::printf("  %s\n", goal(w)->pitch.c_str());
    EXPECT_TRUE(goal(w)->pitch.find("Your Lab is up: J, research Mk II of the Drill head (300 MH, 10 min)") != std::string::npos);
    ASSERT_TRUE(!w.research(me, Module::drillHead, 2).has_value());
    second(w);
    EXPECT_EQ(paid(w.takeNews(me), &id), 1);
    EXPECT_TRUE(id == "research");
}
