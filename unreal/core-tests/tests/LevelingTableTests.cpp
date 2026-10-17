// The perk table (`perkRows`, Leveling.cpp) against the `Perk` enum and its
// names: the facts that used to hold across four parallel lists. Counts come
// from `allCases` and `Leveling::perksPerKind`. (The balance golden is in
// GoldenLevelingTests.cpp.)
#include "test.h"

#include "Leveling.h"

#include <vector>

using namespace ac;

namespace {

bool playerKind(UnitKind k) { return Leveling::machines.contains(k) || Leveling::bio.contains(k); }

std::vector<const PerkRow*> rowsOf(UnitKind k) {
    std::vector<const PerkRow*> out;
    for (const PerkRow& r : perkRows())
        if (r.kind == k) out.push_back(&r);
    return out;
}

} // namespace

/// A name for every perk, and one row for every perk.
TEST(LevelingTable_everyPerkHasANameAndOneRow) {
    EXPECT_EQ(EnumInfo<Perk>::names.size(), allCases<Perk>().size());
    EXPECT_EQ(perkRows().size(), allCases<Perk>().size());
    std::vector<int> seen(allCases<Perk>().size(), 0);
    for (const PerkRow& r : perkRows()) seen[static_cast<size_t>(r.perk)] += 1;
    for (Perk p : allCases<Perk>()) EXPECT_EQ(seen[static_cast<size_t>(p)], 1);
}

/// `kind`, `title`, `effect` and `effects` read the perk's row.
TEST(LevelingTable_theCatalogueReadsTheRow) {
    for (const PerkRow& r : perkRows()) {
        EXPECT_TRUE(kind(r.perk) == r.kind);
        EXPECT_EQ(title(r.perk), std::string(r.title));
        EXPECT_EQ(effect(r.perk), std::string(r.text));
        EXPECT_TRUE(&effects(r.perk) == &r.effects);
    }
}

/// Each player kind has `perksPerKind` rows, two at each level from 2 to
/// the cap in the table's order; the Brood's bugs have none.
TEST(LevelingTable_everyPlayerKindHasItsRowsInLevelOrder) {
    size_t players = 0;
    for (UnitKind k : allCases<UnitKind>()) {
        const auto mine = rowsOf(k);
        if (!playerKind(k)) {
            EXPECT_EQ(mine.size(), size_t(0));
            continue;
        }
        players += 1;
        ASSERT_TRUE(mine.size() == size_t(Leveling::perksPerKind));
        for (size_t i = 0; i < mine.size(); i++) {
            const PerkInfo& in = info(mine[i]->perk);
            EXPECT_EQ(in.level, int64_t(2 + i / 2));
            EXPECT_TRUE(in.slot == (i % 2 == 0 ? PerkSlot::one : PerkSlot::other));
        }
        for (int64_t l = 2; l <= Leveling::cap; l++) {
            const auto o = perkOffer(k, l);
            ASSERT_TRUE(o.has_value());
            EXPECT_TRUE(o->first == mine[size_t(2 * (l - 2))]->perk);
            EXPECT_TRUE(o->second == mine[size_t(2 * (l - 2) + 1)]->perk);
        }
        EXPECT_TRUE(!perkOffer(k, Leveling::cap + 1).has_value());
    }
    EXPECT_EQ(perkRows().size(), players * size_t(Leveling::perksPerKind));
}

/// A perk's name starts with its kind's: "prospectorQuickDrill".
TEST(LevelingTable_aPerksNameStartsWithItsKind) {
    for (const PerkRow& r : perkRows()) {
        const std::string_view name = rawValue(r.perk), k = rawValue(r.kind);
        EXPECT_TRUE(name.size() > k.size() && name.substr(0, k.size()) == k);
    }
}

/// Every pick does something, and has a title and a line of text.
TEST(LevelingTable_everyRowIsFilledIn) {
    for (const PerkRow& r : perkRows()) {
        EXPECT_TRUE(!r.effects.empty());
        EXPECT_TRUE(r.title != nullptr && *r.title != '\0');
        EXPECT_TRUE(r.text != nullptr && *r.text != '\0');
    }
}
