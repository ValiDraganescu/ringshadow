// The Prospector's progression in the MMORPG mode (docs/prospector-progression.md):
// the XP curve to level 100, the five-branch skill tree, the actives and
// their slots, the respec price, and a pilot's progress with its refusals.
// MMO only and engine-free: the strategy game's leveling (Leveling.cpp)
// is apart and unchanged. The sim's side (the effects) and the server's
// (XP sources, requests) come later; this is the catalogue and its rules.
#pragma once

#include "Types.h"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ac {

/// A node of the tree. Append only: saves store the name, and the
/// catalogue's order is the tables' (branch by branch, row by row).
enum class Skill : uint16_t {
    // Deep Vein
    quickDrill, bigHaul, cleanCut, richVein, coreSampler, overcharge, deepBore, oreSled, quickRegrowth, motherlode,
    seismicCharge, densePack, stardustHeart,
    // Quicksilver
    rigMaster, bigTank, coldSeal, derrickTender, twinCanister, quickfill, pressureValve, quicksilverStride, wellspring,
    mercuryLungs, hydrogenBoost, fieldPump, silverTide,
    // Torchwright
    fastWelder, patchwork, prefab, reinforced, longArm, arcSurge, siteBoss, blueprintSavant, overbuild, hardenedWorks,
    emergencyWeld, efficientPlans, headStart,
    // Wayfinder
    lightFrame, surveyor, longSight, dustRunner, scannerPulse, cliffHop, jumpJets, padLink, homingBeacon, luckyStrike,
    sprint, shortHop, ringwalker,
    // Hardshell
    plating, hardHat, cuttingTorch, shield, selfMend, pulseMine, longTorch, bulwark, secondWind, rigArmor, aegis,
    torchRhythm, ironclad,
};

/// The five branches, left to right on the skill screen.
enum class Branch : uint8_t { deepVein, quicksilver, torchwright, wayfinder, hardshell };

/// What an every-Nth node counts.
enum class Work : uint8_t {
    /// An ore trip of the driven Prospector (Rich vein).
    oreTrip,
    /// A pump in a Derrick (Twin canister).
    pump,
    /// A strike of the cutting torch (Torch rhythm).
    strike,
};

/// A site kind a Scanner pulse shows (docs/prospector-progression.md,
/// "Blueprint sites").
enum class SiteKind : uint8_t { crate, wreck, cache, vault };

/// A number a skill changes that the strategy game's `Stat` lacks. Times
/// are "how long" (`.percent(-0.15)`: 15% sooner), chances and shares are
/// `.plus`. Behaviour comes in P2.
enum class SkillStat : uint8_t {
    /// What a load takes from a deposit, a pump from a well (`.percent`).
    depositYield, wellYield,
    /// How long a deposit the pilot empties takes to grow back (a time).
    regrowTime,
    /// What ore and MH unloaded count in the wallet, and in XP (`.percent`).
    unloadOre, unloadHydrogen,
    /// The chance an ore load carries triple (`.plus`).
    loadTriple,
    /// The chance an ore load turns up a survey chip (`.plus` on the base
    /// 0.5%, `.percent` of it).
    chipChance,
    /// Walking while carrying MH, and while carrying nothing (`.percent`).
    speedCarryingHydrogen, speedCarryingNothing,
    /// How far the torch welds and repairs from (`.plus`, cells).
    workReach,
    /// How long a flight between pads takes (a time).
    flightTime,
    /// The MH part of a building's cost (`.percent`).
    buildingHydrogenCost,
    /// The pilot's own buildings: their hp and the damage they take
    /// (`.percent`).
    ownBuildingHp, ownBuildingDamageTaken,
    /// Levels under its floor a blueprint opens (`.plus`).
    blueprintFloorCut,
    /// The share a building starts built when placed (`.plus`).
    builtOnPlacing,
};

/// The active key's burst with the channels the strategy game's `Burst`
/// lacks: for `seconds`, +`drill`, +`build`, +`repair` and +`speed` (1:
/// double), and `damageTaken` (-0.5: half), for `hydrogenCost` MH (the
/// cargo first, then the wallet). The cooldown is the node's.
struct ActiveBurst {
    double seconds = 0;
    double drill = 0;
    double build = 0;
    double repair = 0;
    double speed = 0;
    double damageTaken = 0;
    double hydrogenCost = 0;
    auto operator<=>(const ActiveBurst&) const = default;
};

/// One building block of a skill: one of the strategy game's `Effect`s
/// (on `Scope::Driven`), or a new block declared here as data. The new
/// blocks' behaviour comes in P2.
struct SkillEffect {
    /// The strategy game's block: `Stat` on `Scope::Driven`, `Shield`,
    /// `SelfMend`, `CliffJump`, `Grenade`, `Burst`.
    struct Core { Effect effect; auto operator<=>(const Core&) const = default; };
    /// A number the strategy game's `Stat` lacks.
    struct Stat { SkillStat stat; Change change; auto operator<=>(const Stat&) const = default; };
    /// A building limit raised (`.plus`): Overbuild.
    struct BuildingLimit { StructureKind kind; double plus; auto operator<=>(const BuildingLimit&) const = default; };
    /// Every `n`th time the driven Prospector does `work`, it counts double.
    struct EveryNth { Work work; int64_t n; auto operator<=>(const EveryNth&) const = default; };
    /// On a slot key: a burst with drill, build, repair, damage taken or an MH cost.
    struct Burst { ActiveBurst burst; auto operator<=>(const Burst&) const = default; };
    /// The shield gains `hp` and refills in `refill` seconds (Ironclad, on
    /// top of the Shield node).
    struct ShieldUp { double hp; double refill; auto operator<=>(const ShieldUp&) const = default; };
    // The one-offs.
    /// On a slot key: fills the cargo to full from a deposit within `radius` cells.
    struct SeismicCharge { double radius; auto operator<=>(const SeismicCharge&) const = default; };
    /// On a slot key: the next pump in a Derrick is instant.
    struct Quickfill { auto operator<=>(const Quickfill&) const = default; };
    /// On a slot key: a building of the pilot's within `radius` cells gets
    /// `share` of its hp back at once.
    struct EmergencyWeld { double radius; double share; auto operator<=>(const EmergencyWeld&) const = default; };
    /// The pilot's Derricks fill a `tank` of MH at `perMinute` by themselves;
    /// entering one collects it into the wallet, with no XP.
    struct DerrickTender { double tank; double perMinute; auto operator<=>(const DerrickTender&) const = default; };
    /// A well under the pilot's Derrick grows back `perMinute` MH.
    struct Wellspring { double perMinute; auto operator<=>(const Wellspring&) const = default; };
    /// Pump straight from a well without a Derrick, at `share` of a Derrick's rate.
    struct FieldPump { double share; auto operator<=>(const FieldPump&) const = default; };
    /// Call the Dropship from within `radius` cells of a Landing Pad's
    /// centre (4 without it).
    struct PadLink { double radius; auto operator<=>(const PadLink&) const = default; };
    /// F2: flights cost no MH, and the Dropship flies `faster` (0.3: 30%)
    /// faster.
    struct Ringwalker { double faster; auto operator<=>(const Ringwalker&) const = default; };
    /// Blueprint sites show on the HUD compass within `radius` cells.
    struct Surveyor { double radius; auto operator<=>(const Surveyor&) const = default; };
    /// Arriving in a region shows these kinds of its sites on the world map.
    struct ScannerPulse { std::vector<SiteKind> kinds; auto operator<=>(const ScannerPulse&) const = default; };
    /// After a death, back `sooner` of the wait sooner and keep `cargo` of the cargo.
    struct SecondWind { double sooner; double cargo; auto operator<=>(const SecondWind&) const = default; };

    AC_CASES(SkillEffect, Core, Stat, BuildingLimit, EveryNth, Burst, ShieldUp, SeismicCharge, Quickfill,
             EmergencyWeld, DerrickTender, Wellspring, FieldPump, PadLink, Ringwalker, Surveyor,
             ScannerPulse, SecondWind)
    auto operator<=>(const SkillEffect&) const = default;
};

/// A node that goes in a slot: its key starts it, then it waits `cooldown`
/// seconds (counted from the press).
struct Active {
    double cooldown = 0;
    auto operator<=>(const Active&) const = default;
};

/// One node of the tree.
struct SkillNode {
    Skill skill{};
    Branch branch{};
    /// 1 to 4, and 5 for the capstone.
    int64_t tier = 1;
    /// Points it takes to fill (1 for an active, a capstone, Cliff hop).
    int64_t ranks = 1;
    /// What one rank does; rank r applies it r times (`.percent` and
    /// `.plus` add up). Empty when `byRank` is used.
    std::vector<SkillEffect> effects;
    /// For the nodes whose ranks don't add up (every-Nth, shields, the
    /// site and travel nodes): row r − 1 is what rank r does, whole.
    std::vector<std::vector<SkillEffect>> byRank;
    /// Set for the nine actives.
    std::optional<Active> active;
    /// "Quick drill".
    std::string title;
    /// One plain line in the game's voice: "Drill 6% faster a rank."
    std::string text;
    bool operator==(const SkillNode&) const = default;
};

/// A branch's name and theme for the skill screen.
struct BranchInfo {
    Branch branch;
    /// "Deep Vein".
    std::string title;
    /// "The drill: faster drilling, bigger loads, richer strikes."
    std::string text;
    bool operator==(const BranchInfo&) const = default;
};

/// When a tier opens: at `level`, with `points` already in its branch.
struct Gate {
    int64_t level = 1;
    int64_t points = 0;
    bool operator==(const Gate&) const = default;
};

/// The catalogue and its numbers.
struct AUTOCRAFTCORE_API Progression {
    /// The highest level. XP keeps adding up past it, with no effect.
    static constexpr int64_t cap = 100;
    /// The capstone's tier.
    static constexpr int64_t capstone = 5;
    /// A point spent this recently can be taken back free (seconds).
    static constexpr double undoWindow = 60.0;
    /// The active slots: Q, C, G.
    static constexpr int64_t slotCount = 3;

    /// XP in all to reach `level`: `500 × (L − 1) + (L − 1) × L × (2L − 1) / 2`
    /// (0 for level 1; clamped to 1…cap).
    static double xp(int64_t toReach);
    /// The level `xp` in all reaches, 1…cap.
    static int64_t level(double xp);
    /// Skill points a level gives in all: `L − 1`.
    static int64_t points(int64_t level);
    /// When a tier (1…4, 5 the capstone) opens.
    static Gate gate(int64_t tier);
    /// MH for a respec at `level` once the free one is used: `0.4 × L²`,
    /// rounded to 10.
    static int64_t respecPrice(int64_t level);
    /// The level a slot (0 Q, 1 C, 2 G) opens at.
    static int64_t slotLevel(int64_t slot);
    /// "Q", "C", "G".
    static const char* slotKey(int64_t slot);

    /// Every node in a kind's tree, in `Skill` order (the retired ones left
    /// out): the Prospector's; empty for a kind with no tree yet, so a point
    /// there is refused (docs/kind-registry.md, "MMORPG: the pilot's kind").
    static const std::vector<SkillNode>& nodes(UnitKind kind);
    /// The Prospector's tree.
    static const std::vector<SkillNode>& nodes() { return nodes(UnitKind::prospector); }
    /// `s` is a node of `kind`'s tree (not retired).
    static bool inTree(UnitKind kind, Skill s);
    /// A skill's node, a retired one too (no ranks).
    static const SkillNode& node(Skill s);
    /// Gone from the tree; old saves give its points back (F2: Homing beacon).
    static bool retired(Skill s);
    static const BranchInfo& branch(Branch b);
    /// What `rank` of a node does (its rank row repeated, or its `byRank`
    /// row); empty at rank 0.
    static std::vector<SkillEffect> effects(Skill s, int64_t rank);
    /// Points to fill a branch, and the whole tree.
    static int64_t branchPoints(Branch b);
    static int64_t treePoints();
    /// What a node gives in all at `rank`, in a few words for the skill
    /// screen ("Drill +18%", "Every 4th ore trip carries double"). A
    /// one-rank node: its `text`. Empty at rank 0.
    static std::string rankText(Skill s, int64_t rank);

    /// The buildings a pilot builds from blueprints, in the build menu's
    /// order (docs/prospector-progression.md, "Buildings in the MMO"; the
    /// Garrison is held back).
    static const std::vector<StructureKind>& blueprintKinds();
    /// The level a building's blueprint opens at (its floor); 0: no
    /// blueprint (the Garrison).
    static int64_t blueprintFloor(StructureKind k);
    /// How many of a building a pilot may have across the world (0: none,
    /// the Garrison). Overbuild raises the Hab Dome, Derrick and Sentinel.
    /// The Landing Pad's is per region (F2: the server counts the region's).
    static int64_t buildingLimit(StructureKind k);
    /// Counted in a region, not across the realm: the Landing Pad (F2) and,
    /// since R3 (2026-10-15), the Citadel: one a region, each a full home.
    static bool perRegion(StructureKind k) { return k == StructureKind::landingPad || k == StructureKind::citadel; }
    /// Landing Pads a region may have (F2).
    static constexpr int64_t landingPadsPerRegion = 2;
    /// What a building costs in a world, ore and MH, before the skills
    /// (docs/prospector-progression.md, "Buildings in the MMO"): the
    /// Citadel, the Derrick and the Landing Pad ore only, the rest an MH part too. The
    /// Garrison keeps the strategy game's price.
    static int64_t worldOre(StructureKind k);
    static int64_t worldHydrogen(StructureKind k);
    /// An ore load's chance of turning up a survey chip, before the skills.
    static constexpr double chipChance = 0.005;

    /// The Citadel's tax (C1, docs/prospector-progression.md "The realm's
    /// tax"): the share of every load unloaded into a pilot's wallet, at the
    /// realm's Citadel or a Hab Dome, that goes to the realm wallet instead.
    static constexpr int64_t realmTaxPercent = 10;
    /// The realm's cut of a load of `load`: `realmTaxPercent` of it to the
    /// nearest whole unit (halves up), at least 1 and never all of it. A
    /// load of 1 pays nothing; 2 to 14 pay 1; 25 ore pays 3, 20 MH pays 2,
    /// 100 pays 10.
    static int64_t realmTax(int64_t load);

    /// The tree as text: every node, the curve, the gates, the slots and
    /// the respec price. `balance()` is the catalogue's.
    static std::string balance(const std::vector<SkillNode>& nodes);
    static std::string balance();
    /// FNV-1a 64 of the balance, 16 hex digits (as `Leveling::signature`):
    /// saved with each pilot; when it changes, the next respec is free.
    static const std::string& signature();
};

/// One point spent, for the undo: what and when (the server's clock, seconds).
struct SkillSpend {
    Skill skill{};
    double at = 0;
    bool operator==(const SkillSpend&) const = default;
};

/// A rig module (docs/prospector-progression.md, "Foundry and Lab"): the
/// Prospector has one slot of each. Append only: saves store the name.
enum class Module : uint8_t { drillHead, cargoPod, plate, torchTip, surveyLens };

/// Modules of one kind and mark waiting in the pilot's stores.
struct ModuleStock {
    Module module{};
    int64_t mark = 1;
    int64_t count = 0;
    bool operator==(const ModuleStock&) const = default;
};

/// A Foundry making, or a Lab researching, `module` at `mark`: done at
/// `done` on the world's clock (seconds).
struct ModuleJob {
    Module module{};
    int64_t mark = 1;
    double done = 0;
    bool operator==(const ModuleJob&) const = default;
};

/// The rig's numbers: marks, prices, times and what a module does.
struct AUTOCRAFTCORE_API Modules {
    /// Mk I to Mk III.
    static constexpr int64_t marks = 3;
    /// Every module, in the fitting screen's order.
    static const std::vector<Module>& all();
    /// "Drill head".
    static std::string title(Module m);
    /// "Mk II".
    static std::string markName(int64_t mark);
    /// What making one costs at the Foundry: 100 + 50, 300 + 200, 800 + 600.
    static int64_t ore(int64_t mark);
    static int64_t hydrogen(int64_t mark);
    /// The Foundry's time for one (world seconds): 60, 180, 480.
    static double makeTime(int64_t mark);
    /// What the Lab's research of a mark costs and takes: Mk I none, Mk II
    /// 300 MH and 10 minutes, Mk III 900 MH and 30.
    static int64_t researchHydrogen(int64_t mark);
    static double researchTime(int64_t mark);
    /// A mark's step: +8%, +16%, +25%.
    static double share(int64_t mark);
    /// What a fitted module of `mark` does on a `kind`, as the tree's blocks
    /// (0: none). The plate adds a share of the kind's own hp.
    static std::vector<SkillEffect> effects(Module m, int64_t mark, UnitKind kind = UnitKind::prospector);
    /// In a few words for the fitting screen: "Drill +16%".
    static std::string text(Module m, int64_t mark, UnitKind kind = UnitKind::prospector);
};

/// The pilot's rig (P7): its fitted modules, its stores, the marks the Lab
/// has opened, the Foundry's job and the Lab's. Where the pilot stands and
/// whether it owns a Foundry or a Lab are the server's to check.
struct AUTOCRAFTCORE_API Rig {
    /// The mark in each slot (none: empty).
    std::map<Module, int64_t> fitted;
    /// Made and not fitted, or taken off; by module, then mark.
    std::vector<ModuleStock> stores;
    /// The highest mark researched per module (Mk I needs none).
    std::map<Module, int64_t> researched;
    /// The Foundry's job and the Lab's (one at a time each).
    std::optional<ModuleJob> making;
    std::optional<ModuleJob> research;

    /// The mark fitted (0: none), in stores, researched (at least 1).
    int64_t fittedMark(Module m) const;
    int64_t stock(Module m, int64_t mark) const;
    int64_t researchedMark(Module m) const;
    /// Puts `n` into the stores; takes one out (false: none there).
    void store(Module m, int64_t mark, int64_t n = 1);
    bool take(Module m, int64_t mark);

    /// Why the Foundry can't start `m` at `mark` with this in the wallet, or nil.
    std::optional<std::string> makeRefusal(Module m, int64_t mark, int64_t ore, int64_t hydrogen) const;
    /// Why the Lab can't start researching `mark` of `m`, or nil.
    std::optional<std::string> researchRefusal(Module m, int64_t mark, int64_t hydrogen) const;
    /// Why `mark` of `m` can't be fitted from the stores (0: take it off), or nil.
    std::optional<std::string> fitRefusal(Module m, int64_t mark) const;
    /// Starts a job done at `now` plus its time (false, unchanged: refused).
    /// The server takes the price.
    bool make(Module m, int64_t mark, int64_t ore, int64_t hydrogen, double now);
    bool startResearch(Module m, int64_t mark, int64_t hydrogen, double now);
    /// Fits `mark` from the stores, the old one back into them (0: off).
    bool fit(Module m, int64_t mark);
    /// The jobs done by `now`: the module into the stores, the mark opened.
    /// What finished (made, researched).
    std::pair<std::optional<ModuleJob>, std::optional<ModuleJob>> finish(double now);
    /// What the fitted modules do on a `kind`, as the tree's blocks.
    std::vector<SkillEffect> effects(UnitKind kind = UnitKind::prospector) const;

    bool operator==(const Rig&) const = default;
};

/// The Citadel shop's consumables (C2, docs/prospector-progression.md "The
/// Citadel shop"): bought from the pilot's wallet, kept in stacks, used from
/// the field with keys 1-4. Append only: saves store the name.
enum class Item : uint8_t { repairKit, hydrogenCanister, shieldCharge, surveyFlare };

/// What a consumable is: its price, how many a pilot holds at most, its
/// cooldown after a use, and how long its effect lasts (0: at once).
struct ItemInfo {
    Item item = Item::repairKit;
    const char* title = "";
    /// What it does, in a few words for the shop.
    const char* text = "";
    int64_t ore = 0;
    int64_t hydrogen = 0;
    int64_t stack = 1;
    double cooldown = 0;
    double lasts = 0;
};

/// Ore and MH a purchase costs.
struct ShopPrice {
    int64_t ore = 0;
    int64_t hydrogen = 0;
    bool operator==(const ShopPrice&) const = default;
};

/// The Citadel shop (C2): blueprints and consumables, paid from the pilot's
/// own wallet (the tax does not apply).
struct AUTOCRAFTCORE_API Shop {
    /// A blueprint's price rises this much with each copy the pilot has
    /// bought of that kind before.
    static constexpr double blueprintGrowth = 1.5;
    /// The `bought`th copy's price (0: the first) of `k`'s blueprint: ore
    /// `(50 + 25 × floor) × 1.5^bought`, MH `5 × floor × 1.5^bought` (the
    /// Citadel, the Derrick and the Landing Pad ore only), each rounded to 10. The floor is
    /// the building's (`Progression::blueprintFloor`), not the pilot's.
    /// Nothing for a kind with no blueprint (the Garrison).
    static ShopPrice blueprintPrice(StructureKind k, int64_t bought);
    /// The four consumables, in key order (1-4).
    static const std::vector<ItemInfo>& items();
    static const ItemInfo& item(Item i);
    /// The repair kit gives back this share of the Prospector's full hp.
    static constexpr double repairShare = 0.5;
    /// The MH canister puts this much MH into the wallet.
    static constexpr int64_t canisterHydrogen = 25;
    /// The survey flare: sites within this many cells go on the pilot's
    /// map, and the map shows the deposits within it while it burns.
    static constexpr double flareRadius = 40;
};

/// A pilot's progression in a world: kept by the server, saved with the
/// pilot (`SessionStore::encode`).
struct AUTOCRAFTCORE_API PilotProgress {
    /// The kind the pilot drives, whose tree it climbs (saved as "kind",
    /// only when not the Prospector: older blobs read as one).
    UnitKind kind = UnitKind::prospector;
    /// Plain XP in all.
    double xp = 0.0;
    /// Points in each node learned (rank 1 and up).
    std::map<Skill, int64_t> ranks;
    /// The actives in Q, C and G.
    std::array<std::optional<Skill>, Progression::slotCount> slots{};
    /// Respecs made, the free first one included.
    int64_t respecs = 0;
    /// The next respec is free (the tree changed).
    bool freeRespec = false;
    /// Points spent in the last `Progression::undoWindow` seconds, oldest first.
    std::vector<SkillSpend> recent;
    /// Blueprint copies held (C2): building uses one up. A kind with none
    /// has no entry.
    std::map<StructureKind, int64_t> blueprints;
    /// Blueprint copies bought at the Citadel, per kind, ever: the next
    /// copy's price (`Shop::blueprintPrice`).
    std::map<StructureKind, int64_t> bought;
    /// Consumables held (C2). An item with none has no entry.
    std::map<Item, int64_t> items;
    /// Sites found, regions and pads visited, feats done (ids the server
    /// gives), and survey chips found.
    std::set<std::string> sites;
    std::set<std::string> regions;
    std::set<std::string> pads;
    std::set<std::string> feats;
    int64_t chips = 0;
    /// Sites the pilot knows of (seen, marked by a chip or a Scanner
    /// pulse), found or not: "<region id>/<site name>", as `sites`.
    std::set<std::string> known;
    /// The world's clock when its last survey chip came (at most one every
    /// 5 minutes).
    std::optional<double> chipAt;
    /// `Progression::signature` when the pilot last spent or respecced.
    std::string signature;
    /// Its rig of modules (P7). Saved apart from the rest (the world's save
    /// has a column for it, schema 7): the progress JSON leaves it out.
    Rig rig;

    int64_t level() const { return Progression::level(xp); }
    int64_t rank(Skill s) const;
    /// Points spent in all, in one branch, and in a branch's tiers below `tier`.
    int64_t spent() const;
    int64_t spent(Branch b) const;
    int64_t spentBelow(Branch b, int64_t tier) const;
    /// Points waiting to be spent.
    int64_t unspent() const;
    /// The slot an active is in, or nil.
    std::optional<int64_t> slotOf(Skill s) const;

    /// Why a point can't go into `s` now, or nil.
    std::optional<std::string> spendRefusal(Skill s) const;
    /// Why the last point in `s` can't be taken back at `now`, or nil.
    std::optional<std::string> undoRefusal(Skill s, double now) const;
    /// Why `s` can't go into `slot` (nil `s`: empty it), or nil.
    std::optional<std::string> slotRefusal(int64_t slot, std::optional<Skill> s) const;
    /// Why a respec can't be made with `hydrogen` MH in the wallet, or nil.
    /// (The place, the Citadel, is the server's to check.)
    std::optional<std::string> respecRefusal(int64_t hydrogen) const;

    /// The next respec is free: the first one, or the tree changed.
    bool respecIsFree() const;
    /// MH the next respec costs.
    int64_t respecCost() const;

    /// Spend a point in `s` at `now` (false, unchanged: refused).
    bool spend(Skill s, double now);
    /// Take back the last point in `s`, spent within the undo window.
    bool undo(Skill s, double now);
    /// Put `s` in `slot` (moving it from another), or empty the slot.
    bool slot(int64_t slot, std::optional<Skill> s);
    /// Take back every point and empty the slots; the MH it cost (0 when
    /// free), or nil when refused. The server takes the MH.
    std::optional<int64_t> respec(int64_t hydrogen);

    /// What the skills add up to for a `SkillStat` (`.plus` and `.percent`
    /// of every rank learned, and of the fitted modules).
    Boost skillBoost(SkillStat stat) const;
    /// The level a building opens at for this pilot: its floor less Blueprint
    /// savant, at least 1.
    int64_t floor(StructureKind k) const;
    /// How many of it the pilot may have (`Progression::buildingLimit` and
    /// Overbuild).
    int64_t limit(StructureKind k) const;
    /// An ore load's chance of a survey chip (Core sampler, Lucky strike).
    double chipChance() const;
    /// Why the pilot can't put up a `k` now, owning `owned` of them in the
    /// world: no blueprint copy, under its floor, or at its limit. Nil: it may.
    std::optional<std::string> buildRefusal(StructureKind k, int64_t owned) const;

    /// Blueprint copies of `k` held, and consumables of `i` held.
    int64_t copies(StructureKind k) const;
    int64_t held(Item i) const;
    /// `n` more copies of `k` (a site, a chip, a purchase).
    void giveBlueprint(StructureKind k, int64_t n = 1);
    /// One copy of `k` used up (a building placed). False: none held.
    bool useBlueprint(StructureKind k);
    /// What the next copy of `k` costs this pilot.
    ShopPrice blueprintPrice(StructureKind k) const;
    /// One consumable of `i` used up. False: none held.
    bool useItem(Item i);

    bool operator==(const PilotProgress&) const = default;
};
/// Its name before a pilot drove more than the Prospector.
using ProspectorProgress = PilotProgress;

template <> struct EnumInfo<Skill> {
    static constexpr std::array<std::string_view, 65> names{
        "quickDrill", "bigHaul", "cleanCut", "richVein", "coreSampler", "overcharge", "deepBore", "oreSled",
        "quickRegrowth", "motherlode", "seismicCharge", "densePack", "stardustHeart",
        "rigMaster", "bigTank", "coldSeal", "derrickTender", "twinCanister", "quickfill", "pressureValve",
        "quicksilverStride", "wellspring", "mercuryLungs", "hydrogenBoost", "fieldPump", "silverTide",
        "fastWelder", "patchwork", "prefab", "reinforced", "longArm", "arcSurge", "siteBoss", "blueprintSavant",
        "overbuild", "hardenedWorks", "emergencyWeld", "efficientPlans", "headStart",
        "lightFrame", "surveyor", "longSight", "dustRunner", "scannerPulse", "cliffHop", "jumpJets", "padLink",
        "homingBeacon", "luckyStrike", "sprint", "shortHop", "ringwalker",
        "plating", "hardHat", "cuttingTorch", "shield", "selfMend", "pulseMine", "longTorch", "bulwark",
        "secondWind", "rigArmor", "aegis", "torchRhythm", "ironclad"};
};
template <> struct EnumInfo<Branch> {
    static constexpr std::array<std::string_view, 5> names{"deepVein", "quicksilver", "torchwright", "wayfinder",
                                                           "hardshell"};
};
template <> struct EnumInfo<Work> {
    static constexpr std::array<std::string_view, 3> names{"oreTrip", "pump", "strike"};
};
template <> struct EnumInfo<SiteKind> {
    static constexpr std::array<std::string_view, 4> names{"crate", "wreck", "cache", "vault"};
};
template <> struct EnumInfo<Module> {
    static constexpr std::array<std::string_view, 5> names{"drillHead", "cargoPod", "plate", "torchTip", "surveyLens"};
};
template <> struct EnumInfo<Item> {
    static constexpr std::array<std::string_view, 4> names{"repairKit", "hydrogenCanister", "shieldCharge", "surveyFlare"};
};
template <> struct EnumInfo<SkillStat> {
    static constexpr std::array<std::string_view, 16> names{
        "depositYield", "wellYield", "regrowTime", "unloadOre", "unloadHydrogen", "loadTriple", "chipChance",
        "speedCarryingHydrogen", "speedCarryingNothing", "workReach", "flightTime", "buildingHydrogenCost",
        "ownBuildingHp", "ownBuildingDamageTaken", "blueprintFloorCut", "builtOnPlacing"};
};

} // namespace ac
