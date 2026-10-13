// The Prospector's MMO progression (docs/prospector-progression.md, "Tests",
// core): the curve, points, the gates, ranks, slots, respec and undo, the
// tree's size and its signature, and the progress's JSON.
#include "test.h"

#include "Leveling.h"
#include "Progression.h"
#include "Session.h"

#include <algorithm>
#include <array>

using namespace ac;

namespace {

/// A pilot with just enough XP for `level`.
ProspectorProgress atLevel(int64_t level) {
    ProspectorProgress p;
    p.xp = Progression::xp(level);
    return p;
}

/// Put `count` points into `b`'s tiers below `tier`, filling nodes in order.
void fillBelow(ProspectorProgress& p, Branch b, int64_t tier, int64_t count) {
    for (const auto& n : Progression::nodes()) {
        if (count == 0) return;
        if (n.branch != b || n.tier >= tier) continue;
        const int64_t take = std::min(count, n.ranks - p.rank(n.skill));
        if (take > 0) p.ranks[n.skill] += take;
        count -= take;
    }
}

/// The first node of `b` at `tier`.
Skill firstAt(Branch b, int64_t tier) {
    for (const auto& n : Progression::nodes())
        if (n.branch == b && n.tier == tier) return n.skill;
    return Skill::quickDrill;
}

} // namespace

TEST(progression_curveTotals) {
    EXPECT_EQ(Progression::xp(1), 0.0);
    EXPECT_EQ(Progression::xp(2), 503.0);
    EXPECT_EQ(Progression::xp(10), 5355.0);
    EXPECT_EQ(Progression::xp(50), 145775.0);
    EXPECT_EQ(Progression::xp(100), 1034550.0);
    // Each step is 500 + 3 × L².
    for (int64_t l = 1; l < Progression::cap; l++)
        EXPECT_EQ(Progression::xp(l + 1) - Progression::xp(l), 500.0 + 3.0 * double(l * l));
}

TEST(progression_levelAtTheEdges) {
    EXPECT_EQ(Progression::level(0), int64_t(1));
    EXPECT_EQ(Progression::level(502), int64_t(1));
    EXPECT_EQ(Progression::level(503), int64_t(2));
    EXPECT_EQ(Progression::level(5354), int64_t(9));
    EXPECT_EQ(Progression::level(5355), int64_t(10));
    EXPECT_EQ(Progression::level(1034549), int64_t(99));
    EXPECT_EQ(Progression::level(1034550), int64_t(100));
    EXPECT_EQ(Progression::level(1e9), int64_t(100));
}

TEST(progression_pointsAreLevelLessOne) {
    EXPECT_EQ(Progression::points(1), int64_t(0));
    EXPECT_EQ(Progression::points(10), int64_t(9));
    EXPECT_EQ(Progression::points(100), int64_t(99));
    EXPECT_EQ(atLevel(60).unspent(), int64_t(59));
}

TEST(progression_treeHolds185Points37ABranch) {
    EXPECT_EQ(Progression::nodes().size(), size_t(65));
    EXPECT_EQ(Progression::treePoints(), int64_t(185));
    for (Branch b : allCases<Branch>()) {
        EXPECT_EQ(Progression::branchPoints(b), int64_t(37));
        int64_t count = 0, capstones = 0;
        for (const auto& n : Progression::nodes())
            if (n.branch == b) {
                count += 1;
                if (n.tier == Progression::capstone) capstones += 1;
            }
        EXPECT_EQ(count, int64_t(13));
        EXPECT_EQ(capstones, int64_t(1));
    }
    for (Skill s : allCases<Skill>()) {
        const SkillNode& n = Progression::node(s);
        EXPECT_TRUE(n.skill == s);
        EXPECT_TRUE(!n.title.empty() && !n.text.empty());
        EXPECT_TRUE(n.byRank.empty() ? !n.effects.empty() : int64_t(n.byRank.size()) == n.ranks);
    }
    EXPECT_TRUE(Progression::branch(Branch::hardshell).text.find("for the frontier") != std::string::npos);
}

TEST(progression_nineActivesWithTheirCooldowns) {
    const std::vector<std::pair<Skill, double>> want{
        {Skill::overcharge, 60}, {Skill::quickfill, 45},    {Skill::arcSurge, 60},
        {Skill::pulseMine, 14},  {Skill::seismicCharge, 120}, {Skill::hydrogenBoost, 20},
        {Skill::emergencyWeld, 120}, {Skill::sprint, 30},   {Skill::aegis, 45}};
    int64_t actives = 0;
    for (const auto& n : Progression::nodes())
        if (n.active) actives += 1;
    EXPECT_EQ(actives, int64_t(9));
    for (const auto& [s, cooldown] : want) {
        const SkillNode& n = Progression::node(s);
        ASSERT_TRUE(n.active.has_value());
        EXPECT_EQ(n.active->cooldown, cooldown);
        EXPECT_EQ(n.ranks, int64_t(1));
        EXPECT_EQ(Progression::gate(n.tier).level, n.tier == 2 ? int64_t(10) : int64_t(45));
    }
    // The strategy game's blocks carry their own cooldown: it agrees.
    const auto sprint = Progression::node(Skill::sprint).effects[0].as<SkillEffect::Core>();
    ASSERT_TRUE(sprint != nullptr);
    EXPECT_EQ(sprint->effect.as<Effect::Burst>()->burst.cooldown, 30.0);
    const auto mine = Progression::node(Skill::pulseMine).effects[0].as<SkillEffect::Core>();
    ASSERT_TRUE(mine != nullptr);
    EXPECT_EQ(mine->effect.as<Effect::Grenade>()->grenade.cooldown, 14.0);
}

TEST(progression_effectsByRank) {
    // Quick drill's rank row three times: +18% drill.
    const auto drill = Progression::effects(Skill::quickDrill, 3);
    EXPECT_EQ(drill.size(), size_t(3));
    Boost boost;
    for (const auto& e : drill) boost.add(e.as<SkillEffect::Core>()->effect.as<Effect::Stat>()->change);
    EXPECT_NEAR(boost.rate(), 1.18, 1e-12);
    // Rich vein rank 2: every 4th trip.
    const auto vein = Progression::effects(Skill::richVein, 2);
    ASSERT_TRUE(vein.size() == 1);
    EXPECT_EQ(vein[0].as<SkillEffect::EveryNth>()->n, int64_t(4));
    EXPECT_TRUE(Progression::effects(Skill::richVein, 0).empty());
}

TEST(progression_eachGateRefusesThenAllows) {
    for (Branch b : allCases<Branch>())
        for (int64_t tier = 1; tier <= Progression::capstone; tier++) {
            const Gate g = Progression::gate(tier);
            const Skill s = firstAt(b, tier);
            // A level short, with the points in the branch.
            ProspectorProgress low = atLevel(g.level - 1);
            fillBelow(low, b, tier, g.points);
            low.xp = Progression::xp(std::max(g.level - 1, int64_t(1)));
            const bool hasPoint = low.unspent() > 0;
            EXPECT_TRUE(low.spendRefusal(s).has_value());
            if (hasPoint) EXPECT_TRUE(low.spendRefusal(s)->find("at level " + std::to_string(g.level)) != std::string::npos);
            // At the level, a point short in the branch.
            if (g.points > 0) {
                ProspectorProgress few = atLevel(g.level);
                fillBelow(few, b, tier, g.points - 1);
                EXPECT_TRUE(few.spendRefusal(s) == "Needs " + std::to_string(g.points) + " points in " +
                                                       Progression::branch(b).title);
                EXPECT_TRUE(!few.spend(s, 0));
            }
            // Both: allowed.
            ProspectorProgress ok = atLevel(g.level);
            fillBelow(ok, b, tier, g.points);
            EXPECT_TRUE(!ok.spendRefusal(s).has_value());
            EXPECT_TRUE(ok.spend(s, 0));
            EXPECT_EQ(ok.rank(s), int64_t(1));
        }
}

TEST(progression_ranksStopAtTheMax) {
    ProspectorProgress p = atLevel(20);
    for (int i = 0; i < 5; i++) EXPECT_TRUE(p.spend(Skill::quickDrill, 0));
    EXPECT_TRUE(p.spendRefusal(Skill::quickDrill) == std::string("Full: 5 of 5"));
    EXPECT_TRUE(!p.spend(Skill::quickDrill, 0));
    EXPECT_EQ(p.rank(Skill::quickDrill), int64_t(5));
}

TEST(progression_noPointNoSpend) {
    ProspectorProgress p = atLevel(2);
    EXPECT_TRUE(p.spend(Skill::plating, 0));
    EXPECT_TRUE(p.spendRefusal(Skill::plating) == std::string("No point to spend"));
    EXPECT_TRUE(atLevel(1).spendRefusal(Skill::plating) == std::string("Tier 1 opens at level 2"));
}

TEST(progression_respecPrices) {
    EXPECT_EQ(Progression::respecPrice(10), int64_t(40));
    EXPECT_EQ(Progression::respecPrice(20), int64_t(160));
    EXPECT_EQ(Progression::respecPrice(34), int64_t(460));
    EXPECT_EQ(Progression::respecPrice(50), int64_t(1000));
    EXPECT_EQ(Progression::respecPrice(70), int64_t(1960));
    EXPECT_EQ(Progression::respecPrice(100), int64_t(4000));
}

TEST(progression_respecFirstFreeThenPriced) {
    ProspectorProgress p = atLevel(50);
    EXPECT_TRUE(p.respecRefusal(0) == std::string("No points spent"));
    EXPECT_TRUE(p.spend(Skill::rigMaster, 0));
    ASSERT_TRUE(p.slot(0, std::nullopt));
    EXPECT_EQ(p.respecCost(), int64_t(0));
    EXPECT_EQ(p.respec(0), std::optional<int64_t>(0));
    EXPECT_EQ(p.spent(), int64_t(0));
    EXPECT_EQ(p.respecs, int64_t(1));
    // The second costs 0.4 × 50², and the wallet must hold it.
    EXPECT_TRUE(p.spend(Skill::rigMaster, 0));
    EXPECT_EQ(p.respecCost(), int64_t(1000));
    EXPECT_TRUE(p.respecRefusal(999) == std::string("Needs 1000 MH"));
    EXPECT_TRUE(!p.respec(999).has_value());
    EXPECT_EQ(p.respec(1000), std::optional<int64_t>(1000));
    // A tree change makes the next one free.
    EXPECT_TRUE(p.spend(Skill::rigMaster, 0));
    p.signature = "0000000000000000";
    EXPECT_EQ(p.respecCost(), int64_t(0));
    EXPECT_EQ(p.respec(0), std::optional<int64_t>(0));
    EXPECT_TRUE(p.spend(Skill::rigMaster, 0));
    p.freeRespec = true;
    EXPECT_EQ(p.respec(0), std::optional<int64_t>(0));
    EXPECT_TRUE(!p.freeRespec);
}

TEST(progression_respecKeepsFindsAndEmptiesSlots) {
    ProspectorProgress p = atLevel(30);
    fillBelow(p, Branch::deepVein, 2, 5);
    ASSERT_TRUE(p.spend(Skill::overcharge, 0));
    ASSERT_TRUE(p.slot(0, Skill::overcharge));
    p.blueprints.insert(StructureKind::derrick);
    p.regions.insert("rust-hollow");
    ASSERT_TRUE(p.respec(0).has_value());
    EXPECT_TRUE(!p.slots[0].has_value());
    EXPECT_TRUE(p.ranks.empty());
    EXPECT_TRUE(p.blueprints.count(StructureKind::derrick) == 1);
    EXPECT_TRUE(p.regions.count("rust-hollow") == 1);
    EXPECT_EQ(p.xp, Progression::xp(30));
}

TEST(progression_slotsOpenAt10_25_45) {
    for (int64_t slot = 0; slot < Progression::slotCount; slot++) {
        const int64_t open = Progression::slotLevel(slot);
        const std::array<int64_t, 3> want{10, 25, 45};
        EXPECT_EQ(open, want[size_t(slot)]);
        ProspectorProgress before = atLevel(open - 1);
        EXPECT_TRUE(before.slotRefusal(slot, std::nullopt) ==
                    "Slot " + std::string(Progression::slotKey(slot)) + " opens at level " + std::to_string(open));
        ProspectorProgress after = atLevel(open);
        fillBelow(after, Branch::hardshell, 2, 5);
        ASSERT_TRUE(after.spend(Skill::pulseMine, 0));
        EXPECT_TRUE(!after.slotRefusal(slot, Skill::pulseMine).has_value());
        EXPECT_TRUE(after.slot(slot, Skill::pulseMine));
    }
    EXPECT_TRUE(atLevel(100).slotRefusal(3, std::nullopt) == std::string("No such slot"));
}

TEST(progression_slotTakesLearnedActivesAndMovesThem) {
    ProspectorProgress p = atLevel(45);
    EXPECT_TRUE(p.slotRefusal(0, Skill::sprint) == std::string("Sprint is not learned"));
    ASSERT_TRUE(p.spend(Skill::lightFrame, 0));
    EXPECT_TRUE(p.slotRefusal(0, Skill::lightFrame) == std::string("Light frame is not an active"));
    fillBelow(p, Branch::wayfinder, 4, 20);
    ASSERT_TRUE(p.spend(Skill::sprint, 0));
    EXPECT_TRUE(p.slot(0, Skill::sprint));
    EXPECT_TRUE(p.slot(2, Skill::sprint));
    EXPECT_TRUE(!p.slots[0].has_value());
    EXPECT_EQ(p.slotOf(Skill::sprint), std::optional<int64_t>(2));
    EXPECT_TRUE(p.slot(2, std::nullopt));
    EXPECT_TRUE(!p.slotOf(Skill::sprint).has_value());
}

TEST(progression_undoWithinSixtySeconds) {
    ProspectorProgress p = atLevel(10);
    ASSERT_TRUE(p.spend(Skill::quickDrill, 100));
    EXPECT_TRUE(!p.undoRefusal(Skill::quickDrill, 160).has_value());
    EXPECT_TRUE(p.undo(Skill::quickDrill, 159));
    EXPECT_EQ(p.rank(Skill::quickDrill), int64_t(0));
    EXPECT_EQ(p.unspent(), int64_t(9));
    // After 60 s, it stays.
    ASSERT_TRUE(p.spend(Skill::quickDrill, 200));
    EXPECT_TRUE(p.undoRefusal(Skill::quickDrill, 261) == std::string("No point spent here in the last minute"));
    EXPECT_TRUE(!p.undo(Skill::quickDrill, 261));
    EXPECT_EQ(p.rank(Skill::quickDrill), int64_t(1));
    // Never spent: nothing to undo.
    EXPECT_TRUE(p.undoRefusal(Skill::bigHaul, 200).has_value());
}

TEST(progression_undoKeepsDeeperTiersOpen) {
    ProspectorProgress p = atLevel(10);
    for (int i = 0; i < 5; i++) ASSERT_TRUE(p.spend(Skill::quickDrill, 0));
    ASSERT_TRUE(p.spend(Skill::richVein, 0));
    // Taking a Quick drill point back would leave Rich vein under its gate.
    EXPECT_TRUE(p.undoRefusal(Skill::quickDrill, 30) == std::string("Holds up Rich vein"));
    EXPECT_TRUE(p.undo(Skill::richVein, 30));
    EXPECT_TRUE(p.undo(Skill::quickDrill, 30));
}

TEST(progression_undoEmptiesTheSlotOfAnUnlearnedActive) {
    ProspectorProgress p = atLevel(10);
    fillBelow(p, Branch::torchwright, 2, 5);
    ASSERT_TRUE(p.spend(Skill::arcSurge, 0));
    ASSERT_TRUE(p.slot(0, Skill::arcSurge));
    EXPECT_TRUE(p.undo(Skill::arcSurge, 10));
    EXPECT_TRUE(!p.slots[0].has_value());
}

TEST(progression_signatureFollowsTheTable) {
    EXPECT_EQ(Progression::signature().size(), size_t(16));
    EXPECT_TRUE(Progression::balance(Progression::nodes()) == Progression::balance());
    auto changed = Progression::nodes();
    changed[size_t(Skill::quickDrill)].effects = {
        SkillEffect::Core{Effect::Stat{Stat::Drill{}, Scope::Driven{}, Change::Percent{0.07}}}};
    EXPECT_TRUE(Leveling::fnv1a(Progression::balance(changed)) != Progression::signature());
    auto renamed = Progression::nodes();
    renamed[size_t(Skill::aegis)].ranks = 2;
    EXPECT_TRUE(Leveling::fnv1a(Progression::balance(renamed)) != Progression::signature());
    // Spending stamps the signature.
    ProspectorProgress p = atLevel(2);
    ASSERT_TRUE(p.spend(Skill::plating, 0));
    EXPECT_TRUE(p.signature == Progression::signature());
}

TEST(progression_progressJsonRoundTrip) {
    ProspectorProgress p = atLevel(30);
    fillBelow(p, Branch::deepVein, 2, 5);
    ASSERT_TRUE(p.spend(Skill::overcharge, 12.5));
    ASSERT_TRUE(p.slot(1, Skill::overcharge));
    p.respecs = 2;
    p.blueprints = {StructureKind::citadel, StructureKind::derrick};
    p.sites = {"prospect-landing/lander-wreck"};
    p.regions = {"rust-hollow"};
    p.pads = {"rust-hollow/pad"};
    p.feats = {"citadel"};
    p.chips = 3;
    const std::string text = SessionStore::encode(p);
    const auto back = SessionStore::decode<ProspectorProgress>(text);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == p);
    // Tolerant: an unknown skill and an unknown slot are dropped, missing fields default.
    const auto old = SessionStore::decode<ProspectorProgress>(
        R"({"xp": 600, "ranks": {"quickDrill": 2, "gone": 1}, "slots": ["gone", null, "overcharge"]})");
    ASSERT_TRUE(old.has_value());
    EXPECT_EQ(old->xp, 600.0);
    EXPECT_EQ(old->rank(Skill::quickDrill), int64_t(2));
    EXPECT_EQ(old->ranks.size(), size_t(1));
    EXPECT_TRUE(!old->slots[0].has_value());
    EXPECT_TRUE(old->slots[2] == Skill::overcharge);
    EXPECT_EQ(old->respecs, int64_t(0));
}
