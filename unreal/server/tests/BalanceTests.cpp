// The economy's curves pinned (B2, docs/balance.md): the realm's tax, the
// pace model's rate, blueprint copies and consumables, mercenaries, mission
// and canyon pay, turret marks and the frontier pilots' booked mining. Every
// number here is in docs/balance.md's table; a change to one breaks a test
// here first, then goes into that table with its before and after. Beside the
// exact values, the rules each curve keeps against the pace model
// (docs/prospector-progression.md). Pure functions only: no world stepped.

#include "test.h"

#include "Brood.h"
#include "Defence.h"
#include "Progression.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>

namespace {

using namespace ac;
using namespace ac::server;
using Tier = net::CitadelMission::Tier;

/// What a pilot keeps an hour at `level` by the pace model, ore (1 XP an
/// ore; the realm's 10% off).
double keptAnHour(int64_t level) { return WorldContracts::perMinute(level) * 60 * (1 - Progression::realmTaxPercent / 100.0); }

} // namespace

/// 10% of a load to the realm, rounded to the nearest, at least 1, never
/// the whole load; nothing on a load under 2.
TEST(balance_tax) {
    EXPECT_EQ(Progression::realmTaxPercent, int64_t(10));
    const std::pair<int64_t, int64_t> cuts[] = {{1, 0},  {2, 1},  {5, 1},  {9, 1},  {10, 1}, {14, 1},  {15, 2},
                                                 {20, 2}, {25, 3}, {34, 3}, {35, 4}, {50, 5}, {100, 10}};
    for (const auto& [load, tax] : cuts) EXPECT_EQ(Progression::realmTax(load), tax);
    // A driven Prospector's load (25, 25 + Big haul's 3s, + Ore sled's 5s)
    // is taxed 7 to 12%.
    for (int64_t load = 25; load <= 25 + 9 + 15; load++) {
        const double share = double(Progression::realmTax(load)) / double(load);
        EXPECT_TRUE(share >= 0.07 && share <= 0.12);
    }
}

/// The pace model: 4,500 XP an hour at level 1, 2% more a level.
TEST(balance_pace) {
    const std::pair<int64_t, double> pace[] = {{1, 75}, {5, 81}, {10, 88.5}, {20, 103.5}, {50, 148.5}, {100, 223.5}};
    for (const auto& [level, perMinute] : pace) EXPECT_NEAR(WorldContracts::perMinute(level), perMinute, 1e-9);
    EXPECT_NEAR(keptAnHour(1), 4050, 1e-9);
}

/// A blueprint copy: (50 + 25 × floor) ore and 5 × floor MH (none for the
/// Citadel, Derrick and Landing Pad), × 1.5 a copy bought, rounded to 10.
TEST(balance_blueprint_copies) {
    struct Row { StructureKind kind; int64_t floor; ShopPrice price[5]; };
    const Row rows[] = {
        {StructureKind::citadel, 1, {{80, 0}, {110, 0}, {170, 0}, {250, 0}, {380, 0}}},
        {StructureKind::derrick, 3, {{130, 0}, {190, 0}, {280, 0}, {420, 0}, {630, 0}}},
        {StructureKind::gatling, 5, {{180, 30}, {260, 40}, {390, 60}, {590, 80}, {890, 130}}},
        {StructureKind::habDome, 8, {{250, 40}, {380, 60}, {560, 90}, {840, 140}, {1270, 200}}},
        {StructureKind::sentinel, 10, {{300, 50}, {450, 80}, {680, 110}, {1010, 170}, {1520, 250}}},
        {StructureKind::foundry, 15, {{430, 80}, {640, 110}, {960, 170}, {1430, 250}, {2150, 380}}},
        {StructureKind::lab, 25, {{680, 130}, {1010, 190}, {1520, 280}, {2280, 420}, {3420, 630}}},
        {StructureKind::spacedock, 35, {{930, 180}, {1390, 260}, {2080, 390}, {3120, 590}, {4680, 890}}},
        {StructureKind::bastion, 45, {{1180, 230}, {1760, 340}, {2640, 510}, {3970, 760}, {5950, 1140}}},
    };
    for (const Row& r : rows) {
        EXPECT_EQ(Progression::blueprintFloor(r.kind), r.floor);
        for (int64_t n = 0; n < 5; n++) EXPECT_TRUE(Shop::blueprintPrice(r.kind, n) == r.price[n]);
        // The first copy at its floor: 1 minute of what a pilot keeps there
        // (the Citadel) to 13 (the Bastion), MH at 2 ore (its XP's worth):
        // the price rises with the floor, the income 2% a level.
        const ShopPrice p = Shop::blueprintPrice(r.kind, 0);
        EXPECT_TRUE(double(p.ore + 2 * p.hydrogen) <= keptAnHour(r.floor) / 4);
    }
}

/// The shop's consumables: price, stack, cooldown; each under 3 minutes of
/// what a level-1 pilot keeps (MH at 2 ore).
TEST(balance_consumables) {
    struct Row { Item item; int64_t ore, hydrogen, stack; double cooldown; };
    const Row rows[] = {{Item::repairKit, 120, 10, 5, 30},
                        {Item::hydrogenCanister, 150, 0, 3, 60},
                        {Item::shieldCharge, 100, 30, 3, 45},
                        {Item::surveyFlare, 80, 15, 5, 20}};
    ASSERT_TRUE(Shop::items().size() == std::size(rows));
    for (size_t i = 0; i < std::size(rows); i++) {
        const ItemInfo& it = Shop::items()[i];
        EXPECT_TRUE(it.item == rows[i].item);
        EXPECT_EQ(it.ore, rows[i].ore);
        EXPECT_EQ(it.hydrogen, rows[i].hydrogen);
        EXPECT_EQ(it.stack, rows[i].stack);
        EXPECT_NEAR(it.cooldown, rows[i].cooldown, 1e-9);
        EXPECT_TRUE(double(it.ore + 2 * it.hydrogen) <= keptAnHour(1) / 20);
    }
}

/// Mercenaries: 1, 2, 3 from levels 5, 15, 30; a fee of 500 ore and 50 MH
/// doubling with each held; a wage of 5 + level/5 MH a world day; the pace
/// model's hour of ore over a real day (1/24 of the pilot's rate).
TEST(balance_mercenaries) {
    const int64_t fees[][3] = {{500, 50, 5}, {1000, 100, 15}, {2000, 200, 30}};
    for (int64_t h = 0; h < 3; h++) {
        EXPECT_EQ(WorldMercs::feeOre(h), fees[h][0]);
        EXPECT_EQ(WorldMercs::feeHydrogen(h), fees[h][1]);
        EXPECT_EQ(WorldMercs::nextLevel(h), fees[h][2]);
        EXPECT_EQ(WorldMercs::limit(fees[h][2]), h + 1);
        EXPECT_EQ(WorldMercs::limit(fees[h][2] - 1), h);
    }
    const std::tuple<int64_t, int64_t, double> rows[] = {{1, 5, 187.5},   {5, 6, 202.5},   {10, 7, 221.25}, {15, 8, 240},
                                                         {20, 9, 258.75}, {30, 11, 296.25}, {50, 15, 371.25}, {100, 25, 558.75}};
    const double daysAnHour = 3600 / WorldClock::dayLength;
    for (const auto& [level, wage, perHour] : rows) {
        EXPECT_EQ(WorldMercs::wage(level), wage);
        EXPECT_NEAR(WorldMercs::rate(level) * 3600, perHour, 1e-9);
        // Never out-earns the pilot: all it may hold make at most an eighth.
        EXPECT_TRUE(double(WorldMercs::limit(level)) * WorldMercs::rate(level) * 3600 <= WorldContracts::perMinute(level) * 60 / 8);
        // Its wage (MH at 2 ore) is under a third of what it brings in.
        EXPECT_TRUE(double(wage) * daysAnHour * 2 < perHour * 0.9 / 3);
    }
    // Each place pays its fee back within a real day of work.
    for (int64_t h = 0; h < 3; h++) {
        const int64_t level = WorldMercs::nextLevel(h);
        const double net = WorldMercs::rate(level) * 3600 * 0.9 - double(WorldMercs::wage(level)) * daysAnHour * 2;
        const double fee = double(WorldMercs::feeOre(h) + 2 * WorldMercs::feeHydrogen(h));
        EXPECT_TRUE(fee / net < 24);
    }
}

/// Mission pay by tier (B2): 3, 8, 15 minutes of play as XP (about three
/// times the time each takes); 75, 225, 500 ore rising 2% a level (to 25);
/// 10 + 2L, 30 + 4L, 60 + 8L MH (to 5). Before B2: 8, 20, 40 minutes and
/// 150, 450, 1,000 ore.
TEST(balance_mission_pay) {
    EXPECT_NEAR(WorldMissions::minutes(Tier::easy), 3, 1e-9);
    EXPECT_NEAR(WorldMissions::minutes(Tier::hard), 8, 1e-9);
    EXPECT_NEAR(WorldMissions::minutes(Tier::perilous), 15, 1e-9);
    struct Row { int64_t level; Tier tier; double xp; int64_t ore, hydrogen, haulOre, haulHydrogen; };
    const Row rows[] = {
        {1, Tier::easy, 225, 75, 10, 200, 40},        {1, Tier::hard, 600, 225, 35, 800, 150},
        {1, Tier::perilous, 1125, 500, 70, 800, 150}, {5, Tier::easy, 245, 75, 20, 200, 40},
        {5, Tier::hard, 650, 250, 50, 850, 160},      {5, Tier::perilous, 1215, 550, 100, 850, 160},
        {10, Tier::easy, 265, 100, 30, 250, 50},      {10, Tier::hard, 710, 275, 70, 950, 180},
        {10, Tier::perilous, 1330, 600, 140, 950, 180}, {20, Tier::easy, 310, 100, 50, 300, 60},
        {20, Tier::hard, 830, 300, 110, 1100, 210},   {20, Tier::perilous, 1550, 700, 220, 1100, 210},
    };
    for (const Row& r : rows) {
        EXPECT_NEAR(WorldMissions::xp(r.tier, r.level), r.xp, 1e-9);
        EXPECT_EQ(WorldMissions::ore(r.tier, r.level), r.ore);
        EXPECT_EQ(WorldMissions::hydrogen(r.tier, r.level), r.hydrogen);
        EXPECT_EQ(WorldMissions::haulOre(r.tier, r.level), r.haulOre);
        EXPECT_EQ(WorldMissions::haulHydrogen(r.tier, r.level), r.haulHydrogen);
    }
    // Each tier pays more than the one below, at every level.
    for (int64_t level = 1; level <= 100; level++) {
        EXPECT_TRUE(WorldMissions::xp(Tier::easy, level) < WorldMissions::xp(Tier::hard, level));
        EXPECT_TRUE(WorldMissions::xp(Tier::hard, level) < WorldMissions::xp(Tier::perilous, level));
        EXPECT_TRUE(WorldMissions::ore(Tier::easy, level) < WorldMissions::ore(Tier::hard, level));
        EXPECT_TRUE(WorldMissions::hydrogen(Tier::hard, level) < WorldMissions::hydrogen(Tier::perilous, level));
    }
}

/// Canyon pay (B2): 80 XP, 60 ore and 16 MH times the wave (0.4 of Z2's
/// 200, 150, 40); the last wave twice that; × 1.3 in a level-3 region (Z3);
/// each fighter at least a tenth.
TEST(balance_wave_pay) {
    for (int64_t n = 1; n <= 10; n++) {
        EXPECT_NEAR(WorldCanyon::xp(n), 80.0 * double(n), 1e-9);
        EXPECT_EQ(WorldCanyon::ore(n), 60 * n);
        EXPECT_EQ(WorldCanyon::hydrogen(n), 16 * n);
    }
    EXPECT_NEAR(WorldCanyon::payScale(2), 1.0, 1e-9);
    EXPECT_NEAR(WorldCanyon::payScale(3), 1.3, 1e-9);
    EXPECT_NEAR(WorldCanyon::payScale(4), 1.3, 1e-9);
    // Every zone has ten waves.
    for (const char* zone : {"the-fork", "the-drowned-cut", "the-roost"}) EXPECT_EQ(WorldCanyon::waves(zone).size(), size_t(10));
    EXPECT_EQ(WorldCanyon::count(), int64_t(10));
    EXPECT_NEAR(WorldCanyon::lastBonus, 2, 1e-9);
    EXPECT_NEAR(WorldCanyon::floorShare, 0.1, 1e-9);
}

/// Turret marks: a Gatling Gun's 100 + 50, 300 + 200, 800 + 500 for each
/// stat (rate, damage, range, air reach); an AA tower's half as much again
/// (no air reach: it is air only); the Citadel's from the realm wallet, 250,
/// 500 + 50, 1,000 + 150.
TEST(balance_turret_marks) {
    const ShopPrice gatling[] = {{100, 50}, {300, 200}, {800, 500}};
    const ShopPrice tower[] = {{150, 75}, {450, 300}, {1200, 750}};
    const ShopPrice citadel[] = {{250, 0}, {500, 50}, {1000, 150}};
    for (const DefenceStat s : {DefenceStat::rate, DefenceStat::damage, DefenceStat::range, DefenceStat::air}) {
        EXPECT_TRUE(Defence::takes(StructureKind::gatling, s));
        EXPECT_EQ(Defence::takes(StructureKind::sentinel, s), s != DefenceStat::air);
        for (int64_t m = 1; m <= 3; m++) {
            EXPECT_TRUE(Defence::turretPrice(StructureKind::gatling, s, m) == gatling[m - 1]);
            if (s != DefenceStat::air) EXPECT_TRUE(Defence::turretPrice(StructureKind::sentinel, s, m) == tower[m - 1]);
            EXPECT_TRUE(Defence::citadelPrice(s, m) == citadel[m - 1]);
        }
    }
    // Against a turret's kill value: a gun's builder takes half the kill XP
    // of what it kills (`WorldMap::defenceXpShare`). One mark of one stat
    // pays back within a held cycle's kills (waves 1 to 10, all of them the
    // builder's guns'); a stat's three marks within two cycles.
    for (const char* zone : {"the-fork", "the-drowned-cut", "the-roost"}) {
        double cycle = 0;
        for (const WorldCanyon::Wave& wv : WorldCanyon::waves(zone))
            for (const WorldCanyon::Group& g : wv.groups)
                cycle += double(g.count) * Brood::xp(g.kind) + double(g.escort) * Brood::xp(UnitKind::shardling);
        const double builder = cycle * WorldMap::defenceXpShare;
        std::printf("  %s: a held cycle's kills %.0f XP, %.0f to the guns' builder\n", zone, cycle, builder);
        EXPECT_TRUE(100 + 2 * 50 <= builder);
        EXPECT_TRUE(100 + 300 + 800 + 2 * (50 + 200 + 500) <= 2 * builder);
    }
}

/// A frontier pilot's booked mining: an unskilled Prospector's load of 5
/// over its drill and a 12-cell walk each way, 0.41 ore a second; a quarter
/// of the pace model's level-1 hour.
TEST(balance_frontier_booked) {
    EXPECT_NEAR(WorldFrontier::bookRate(), 5 / (6 + 2 * 12 / 3.94), 1e-9);
    EXPECT_NEAR(WorldFrontier::bookRate() * 3600, 1488.7, 0.1);
    EXPECT_TRUE(WorldFrontier::bookRate() * 3600 < WorldContracts::perMinute(1) * 60 / 2);
    // What five of them give the realm an hour, at 10% (at most): under one
    // driven pilot's tax.
    EXPECT_TRUE(5 * WorldFrontier::bookRate() * 3600 * 0.1 < WorldContracts::perMinute(1) * 60 * 0.2);
}
