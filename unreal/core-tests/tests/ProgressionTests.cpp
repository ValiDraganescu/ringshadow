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

// F2: Wayfinder lost Homing beacon (3 points, 12 nodes); its skill stays,
// retired, with no ranks.
TEST(progression_treeHolds182Points37ABranch) {
    EXPECT_EQ(Progression::nodes().size(), size_t(64));
    EXPECT_EQ(Progression::treePoints(), int64_t(182));
    for (Branch b : allCases<Branch>()) {
        const bool wayfinder = b == Branch::wayfinder;
        EXPECT_EQ(Progression::branchPoints(b), int64_t(wayfinder ? 34 : 37));
        int64_t count = 0, capstones = 0;
        for (const auto& n : Progression::nodes())
            if (n.branch == b) {
                count += 1;
                if (n.tier == Progression::capstone) capstones += 1;
            }
        EXPECT_EQ(count, int64_t(wayfinder ? 12 : 13));
        EXPECT_EQ(capstones, int64_t(1));
    }
    for (Skill s : allCases<Skill>()) {
        const SkillNode& n = Progression::node(s);
        EXPECT_TRUE(n.skill == s);
        EXPECT_TRUE(!n.title.empty() && !n.text.empty());
        if (Progression::retired(s)) {
            EXPECT_EQ(n.ranks, int64_t(0));
            continue;
        }
        EXPECT_TRUE(n.byRank.empty() ? !n.effects.empty() : int64_t(n.byRank.size()) == n.ranks);
    }
    EXPECT_TRUE(Progression::retired(Skill::homingBeacon));
    EXPECT_TRUE(ProspectorProgress{}.spendRefusal(Skill::homingBeacon).has_value());
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
    p.giveBlueprint(StructureKind::derrick);
    p.regions.insert("rust-hollow");
    ASSERT_TRUE(p.respec(0).has_value());
    EXPECT_TRUE(!p.slots[0].has_value());
    EXPECT_TRUE(p.ranks.empty());
    EXPECT_EQ(p.copies(StructureKind::derrick), int64_t(1));
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
    p.blueprints = {{StructureKind::citadel, 1}, {StructureKind::derrick, 3}};
    p.bought = {{StructureKind::derrick, 2}};
    p.items = {{Item::repairKit, 4}, {Item::surveyFlare, 1}};
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
    // Before C2 the blueprints were the kinds found, a list: one copy each.
    const auto found = SessionStore::decode<ProspectorProgress>(R"({"xp": 10, "blueprints": ["citadel", "habDome"]})");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->copies(StructureKind::citadel), int64_t(1));
    EXPECT_EQ(found->copies(StructureKind::habDome), int64_t(1));
    EXPECT_EQ(found->copies(StructureKind::derrick), int64_t(0));
    EXPECT_TRUE(found->bought.empty() && found->items.empty());
}

// The skill screen's words: what a node gives in all at a rank, and every
// rank of every node says something.
TEST(progression_rankText) {
    EXPECT_TRUE(Progression::rankText(Skill::quickDrill, 0).empty());
    EXPECT_TRUE(Progression::rankText(Skill::quickDrill, 3) == "Drill +18%");
    EXPECT_TRUE(Progression::rankText(Skill::bigHaul, 2) == "+6 ore a trip");
    EXPECT_TRUE(Progression::rankText(Skill::densePack, 3) == "Ore loads +30%");
    EXPECT_TRUE(Progression::rankText(Skill::richVein, 2) == "Every 4th ore trip carries double");
    EXPECT_TRUE(Progression::rankText(Skill::longArm, 3) == "Weld and repair from 1.5 cells farther");
    EXPECT_TRUE(Progression::rankText(Skill::bulwark, 2) == "Damage taken −10%");
    EXPECT_TRUE(Progression::rankText(Skill::overbuild, 2) == "+2 Hab Dome, Derrick and Sentinel allowed");
    EXPECT_TRUE(Progression::rankText(Skill::padLink, 2) == "Call the Dropship from within 12 cells of a Landing Pad");
    EXPECT_TRUE(Progression::rankText(Skill::ringwalker, 1) == "Flights cost no MH, from any Landing Pad, and the Dropship comes 30% faster.");
    EXPECT_TRUE(Progression::rankText(Skill::scannerPulse, 3) == "Arriving marks a region's wrecks, caches and vaults");
    EXPECT_TRUE(Progression::rankText(Skill::overcharge, 1) == Progression::node(Skill::overcharge).text);
    for (const SkillNode& n : Progression::nodes())
        for (int64_t r = 1; r <= n.ranks; r++) EXPECT_TRUE(!Progression::rankText(n.skill, r).empty());
}

// The blueprints: ten buildings, the floors of the doc's table, rising
// (D2: the gatling gun at 5, the Sentinel down to 10; F2: the Landing Pad
// at 1 with the Citadel).
TEST(progression_blueprintFloors) {
    const auto& k = Progression::blueprintKinds();
    ASSERT_TRUE(k.size() == size_t(10));
    EXPECT_TRUE(k.front() == StructureKind::citadel);
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::landingPad), int64_t(1));
    EXPECT_EQ(Progression::buildingLimit(StructureKind::landingPad), int64_t(2));
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::derrick), int64_t(3));
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::gatling), int64_t(5));
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::sentinel), int64_t(10));
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::garrison), int64_t(0));
    for (size_t i = 1; i < k.size(); i++)
        EXPECT_TRUE(Progression::blueprintFloor(k[i]) >= Progression::blueprintFloor(k[i - 1]));
    EXPECT_TRUE(Progression::blueprintFloor(k[2]) > Progression::blueprintFloor(k[1]));
}

// The Citadel shop (C2): a blueprint's price grows with its floor and with
// every copy bought; the Citadel and the Derrick cost ore only.
TEST(progression_blueprintPriceRises) {
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::citadel, 0) == (ShopPrice{80, 0}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::derrick, 0) == (ShopPrice{130, 0}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::habDome, 0) == (ShopPrice{250, 40}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::habDome, 1) == (ShopPrice{380, 60}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::habDome, 4) == (ShopPrice{1270, 200}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::sentinel, 0) == (ShopPrice{300, 50}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::gatling, 0) == (ShopPrice{180, 30}));
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::garrison, 0) == ShopPrice{});
    // F2: the Landing Pad as cheap as the Citadel's, ore only.
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::landingPad, 0) == (ShopPrice{80, 0}));
    for (const StructureKind k : Progression::blueprintKinds())
        for (int64_t n = 0; n < 8; n++) {
            const ShopPrice a = Shop::blueprintPrice(k, n), b = Shop::blueprintPrice(k, n + 1);
            EXPECT_TRUE(b.ore > a.ore && b.hydrogen >= a.hydrogen);
        }
    // Up the floors, dearer.
    const auto& kinds = Progression::blueprintKinds();
    for (size_t i = 1; i < kinds.size(); i++)
        EXPECT_TRUE(Shop::blueprintPrice(kinds[i], 0).ore >= Shop::blueprintPrice(kinds[i - 1], 0).ore);
    ProspectorProgress p;
    p.bought[StructureKind::habDome] = 1;
    EXPECT_TRUE(p.blueprintPrice(StructureKind::habDome) == Shop::blueprintPrice(StructureKind::habDome, 1));
}

// Copies and stacks: given, used up one at a time, gone at none.
TEST(progression_blueprintCopiesAndItems) {
    ProspectorProgress p = atLevel(10);
    EXPECT_TRUE(p.buildRefusal(StructureKind::citadel, 0) == std::string("No blueprint: buy one at a Citadel (H)"));
    p.giveBlueprint(StructureKind::citadel, 2);
    EXPECT_EQ(p.copies(StructureKind::citadel), int64_t(2));
    EXPECT_TRUE(!p.buildRefusal(StructureKind::citadel, 0));
    EXPECT_TRUE(p.useBlueprint(StructureKind::citadel));
    EXPECT_TRUE(p.useBlueprint(StructureKind::citadel));
    EXPECT_TRUE(!p.useBlueprint(StructureKind::citadel));
    EXPECT_TRUE(p.blueprints.empty());
    p.items[Item::shieldCharge] = 1;
    EXPECT_EQ(p.held(Item::shieldCharge), int64_t(1));
    EXPECT_TRUE(p.useItem(Item::shieldCharge));
    EXPECT_TRUE(!p.useItem(Item::shieldCharge));
    EXPECT_TRUE(p.items.empty());
    // Four items, in key order, each with a price and a stack.
    ASSERT_TRUE(Shop::items().size() == 4);
    for (size_t i = 0; i < Shop::items().size(); i++) {
        const ItemInfo& x = Shop::items()[i];
        EXPECT_TRUE(size_t(x.item) == i && x.ore + x.hydrogen > 0 && x.stack > 0 && x.cooldown > 0);
        EXPECT_TRUE(&Shop::item(x.item) == &x);
    }
}
