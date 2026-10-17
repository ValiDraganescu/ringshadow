// The Prospector's progression in the MMORPG mode (docs/prospector-progression.md):
// the curve, the gates, the 64 nodes (65 skills, Homing beacon retired), the balance and its signature, and
// `PilotProgress`'s rules.
#include "Progression.h"

#include "Leveling.h"
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ac {

// MARK: - The curve, the gates, the slots

/// XP in all to reach `level`: `500 × (L − 1) + (L − 1) × L × (2L − 1) / 2`.
double Progression::xp(int64_t toReach) {
    const double l = static_cast<double>(std::clamp(toReach, int64_t(1), cap));
    return 500 * (l - 1) + (l - 1) * l * (2 * l - 1) / 2;
}

int64_t Progression::level(double xp) {
    int64_t l = 1;
    while (l < cap && xp >= Progression::xp(l + 1)) l += 1;
    return l;
}

int64_t Progression::points(int64_t level) { return std::clamp(level, int64_t(1), cap) - 1; }

Gate Progression::gate(int64_t tier) {
    switch (tier) {
    case 1: return {2, 0};
    case 2: return {10, 5};
    case 3: return {25, 12};
    case 4: return {45, 20};
    default: return {70, 28};
    }
}

int64_t Progression::respecPrice(int64_t level) {
    const double l = static_cast<double>(std::clamp(level, int64_t(1), cap));
    return std::llround(0.4 * l * l / 10) * 10;
}

int64_t Progression::realmTax(int64_t load) {
    if (load < 2) return 0;
    const int64_t cut = (load * realmTaxPercent + 50) / 100;
    return std::clamp(cut, int64_t(1), load - 1);
}

int64_t Progression::slotLevel(int64_t slot) {
    static constexpr std::array<int64_t, slotCount> levels{10, 25, 45};
    return slot >= 0 && slot < slotCount ? levels[static_cast<size_t>(slot)] : cap + 1;
}

const char* Progression::slotKey(int64_t slot) {
    static constexpr std::array<const char*, slotCount> keys{"Q", "C", "G"};
    return slot >= 0 && slot < slotCount ? keys[static_cast<size_t>(slot)] : "?";
}

// MARK: - The catalogue

namespace {

SkillEffect driven(Stat stat, Change change) { return SkillEffect::Core{Effect::Stat{stat, Scope::Driven{}, change}}; }
SkillEffect skillStat(SkillStat stat, Change change) { return SkillEffect::Stat{stat, change}; }
Change pct(double x) { return Change::Percent{x}; }
Change plus(double x) { return Change::Plus{x}; }

/// A node whose ranks add up.
SkillNode node(Skill s, Branch b, int64_t tier, int64_t ranks, std::vector<SkillEffect> effects, std::string title,
               std::string text) {
    SkillNode n;
    n.skill = s;
    n.branch = b;
    n.tier = tier;
    n.ranks = ranks;
    n.effects = std::move(effects);
    n.title = std::move(title);
    n.text = std::move(text);
    return n;
}

/// A node with a row for each rank.
SkillNode ranked(Skill s, Branch b, int64_t tier, std::vector<std::vector<SkillEffect>> rows, std::string title,
                 std::string text) {
    SkillNode n = node(s, b, tier, static_cast<int64_t>(rows.size()), {}, std::move(title), std::move(text));
    n.byRank = std::move(rows);
    return n;
}

/// An active: one rank, on a slot key.
SkillNode active(Skill s, Branch b, int64_t tier, double cooldown, SkillEffect effect, std::string title,
                 std::string text) {
    SkillNode n = node(s, b, tier, 1, {std::move(effect)}, std::move(title), std::move(text));
    n.active = Active{cooldown};
    return n;
}

std::vector<SkillNode> table() {
    using S = Skill;
    std::vector<SkillNode> t;

    // Deep Vein (the drill)
    {
        const Branch b = Branch::deepVein;
        t.push_back(node(S::quickDrill, b, 1, 5, {driven(Stat::Drill{}, pct(0.06))}, "Quick drill",
                         "Drill 6% faster a rank."));
        t.push_back(node(S::bigHaul, b, 1, 3, {driven(Stat::OreCarry{}, plus(3))}, "Big haul",
                         "Carry 3 more ore a trip, a rank."));
        t.push_back(node(S::cleanCut, b, 1, 3, {skillStat(SkillStat::depositYield, pct(-0.05))}, "Clean cut",
                         "A load takes 5% less from the deposit, a rank."));
        t.push_back(ranked(S::richVein, b, 2,
                           {{SkillEffect::EveryNth{Work::oreTrip, 5}}, {SkillEffect::EveryNth{Work::oreTrip, 4}},
                            {SkillEffect::EveryNth{Work::oreTrip, 3}}},
                           "Rich vein", "Every 5th ore trip carries double; every 4th, then every 3rd."));
        t.push_back(node(S::coreSampler, b, 2, 3, {skillStat(SkillStat::chipChance, plus(0.01))}, "Core sampler",
                         "1% more chance a load turns up a survey chip, a rank."));
        t.push_back(active(S::overcharge, b, 2, 60, SkillEffect::Burst{ActiveBurst{.seconds = 10, .drill = 1.0}},
                           "Overcharge", "Drill twice as fast for 10 s. Ready again in 60 s."));
        t.push_back(node(S::deepBore, b, 3, 3, {driven(Stat::Drill{}, pct(0.08))}, "Deep bore",
                         "Drill 8% faster a rank."));
        t.push_back(node(S::oreSled, b, 3, 3, {driven(Stat::OreCarry{}, plus(5))}, "Ore sled",
                         "Carry 5 more ore a trip, a rank."));
        t.push_back(node(S::quickRegrowth, b, 3, 3, {skillStat(SkillStat::regrowTime, pct(-0.15))}, "Quick regrowth",
                         "Deposits you empty grow back 15% sooner, a rank."));
        t.push_back(node(S::motherlode, b, 4, 5, {skillStat(SkillStat::loadTriple, plus(0.02))}, "Motherlode",
                         "2% chance a load carries triple, a rank."));
        t.push_back(active(S::seismicCharge, b, 4, 120, SkillEffect::SeismicCharge{3}, "Seismic charge",
                           "Fill the cargo from a deposit within 3 cells. Ready again in 120 s."));
        t.push_back(node(S::densePack, b, 4, 3, {driven(Stat::OreCarry{}, pct(0.10))}, "Dense pack",
                         "Ore loads 10% bigger, a rank."));
        t.push_back(node(S::stardustHeart, b, Progression::capstone, 1, {skillStat(SkillStat::unloadOre, pct(0.2))},
                         "Stardust heart", "Ore you unload counts 20% more, in the wallet and in XP."));
    }

    // Quicksilver (the well)
    {
        const Branch b = Branch::quicksilver;
        t.push_back(node(S::rigMaster, b, 1, 5, {driven(Stat::Hydrogen{}, pct(0.08))}, "Rig master",
                         "Pump 8% faster a rank."));
        t.push_back(node(S::bigTank, b, 1, 3, {driven(Stat::HydrogenCarry{}, plus(3))}, "Big tank",
                         "Carry 3 more MH a trip, a rank."));
        t.push_back(node(S::coldSeal, b, 1, 3, {skillStat(SkillStat::wellYield, pct(-0.05))}, "Cold seal",
                         "A pump takes 5% less from the well, a rank."));
        t.push_back(ranked(S::derrickTender, b, 2,
                           {{SkillEffect::DerrickTender{100, 2}}, {SkillEffect::DerrickTender{100, 4}},
                            {SkillEffect::DerrickTender{100, 6}}},
                           "Derrick tender",
                           "Your Derricks fill a 100 MH tank by themselves, 2 MH a minute a rank. Step in to collect."));
        t.push_back(ranked(S::twinCanister, b, 2,
                           {{SkillEffect::EveryNth{Work::pump, 5}}, {SkillEffect::EveryNth{Work::pump, 4}},
                            {SkillEffect::EveryNth{Work::pump, 3}}},
                           "Twin canister", "Every 5th pump carries double; every 4th, then every 3rd."));
        t.push_back(active(S::quickfill, b, 2, 45, SkillEffect::Quickfill{}, "Quickfill",
                           "Your next pump in a Derrick is instant. Ready again in 45 s."));
        t.push_back(node(S::pressureValve, b, 3, 3, {driven(Stat::Hydrogen{}, pct(0.10))}, "Pressure valve",
                         "Pump 10% faster a rank."));
        t.push_back(node(S::quicksilverStride, b, 3, 3, {skillStat(SkillStat::speedCarryingHydrogen, pct(0.05))},
                         "Quicksilver stride", "Walk 5% faster while carrying MH, a rank."));
        t.push_back(ranked(S::wellspring, b, 3,
                           {{SkillEffect::Wellspring{5}}, {SkillEffect::Wellspring{10}}, {SkillEffect::Wellspring{15}}},
                           "Wellspring", "A well under your Derrick grows back 5 MH a minute, a rank."));
        t.push_back(node(S::mercuryLungs, b, 4, 5, {driven(Stat::HydrogenCarry{}, plus(4))}, "Mercury lungs",
                         "Carry 4 more MH a trip, a rank."));
        t.push_back(active(S::hydrogenBoost, b, 4, 20,
                           SkillEffect::Burst{ActiveBurst{.seconds = 6, .speed = 0.8, .hydrogenCost = 10}},
                           "Hydrogen boost", "Burn 10 MH to walk 80% faster for 6 s. Ready again in 20 s."));
        t.push_back(ranked(S::fieldPump, b, 4,
                           {{SkillEffect::FieldPump{0.4}}, {SkillEffect::FieldPump{0.6}}, {SkillEffect::FieldPump{0.8}}},
                           "Field pump", "Pump straight from a well, no Derrick: 40% of its rate, then 60%, then 80%."));
        t.push_back(node(S::silverTide, b, Progression::capstone, 1, {skillStat(SkillStat::unloadHydrogen, pct(0.2))},
                         "Silver tide", "MH you unload counts 20% more, in the wallet and in XP."));
    }

    // Torchwright (the torch)
    {
        const Branch b = Branch::torchwright;
        t.push_back(node(S::fastWelder, b, 1, 5, {driven(Stat::Build{}, pct(0.10))}, "Fast welder",
                         "Build 10% faster a rank."));
        t.push_back(node(S::patchwork, b, 1, 3, {driven(Stat::Repair{}, pct(0.15))}, "Patchwork",
                         "Repair 15% faster a rank."));
        t.push_back(node(S::prefab, b, 1, 3, {driven(Stat::BuildingCost{}, pct(-0.04))}, "Prefab",
                         "Buildings cost 4% less, ore and MH, a rank."));
        t.push_back(node(S::reinforced, b, 2, 3, {skillStat(SkillStat::ownBuildingHp, pct(0.10))}, "Reinforced",
                         "Your buildings get 10% more hp, a rank."));
        t.push_back(node(S::longArm, b, 2, 3, {skillStat(SkillStat::workReach, plus(0.5))}, "Long arm",
                         "Weld and repair from half a cell farther, a rank."));
        t.push_back(active(S::arcSurge, b, 2, 60,
                           SkillEffect::Burst{ActiveBurst{.seconds = 8, .build = 2.0, .repair = 2.0}}, "Arc surge",
                           "Build and repair three times as fast for 8 s. Ready again in 60 s."));
        t.push_back(node(S::siteBoss, b, 3, 3, {driven(Stat::Build{}, pct(0.10))}, "Site boss",
                         "Build 10% faster a rank."));
        t.push_back(node(S::blueprintSavant, b, 3, 3, {skillStat(SkillStat::blueprintFloorCut, plus(2))},
                         "Blueprint savant", "Blueprints open 2 levels below their floor, a rank."));
        t.push_back(node(S::overbuild, b, 3, 3,
                         {SkillEffect::BuildingLimit{StructureKind::habDome, 1},
                          SkillEffect::BuildingLimit{StructureKind::derrick, 1},
                          SkillEffect::BuildingLimit{StructureKind::sentinel, 1}},
                         "Overbuild", "One more Hab Dome, Derrick and Sentinel allowed, a rank."));
        t.push_back(node(S::hardenedWorks, b, 4, 5, {skillStat(SkillStat::ownBuildingDamageTaken, pct(-0.04))},
                         "Hardened works", "Your buildings take 4% less damage, a rank."));
        t.push_back(active(S::emergencyWeld, b, 4, 120, SkillEffect::EmergencyWeld{3, 0.3}, "Emergency weld",
                           "A building of yours within 3 cells gets 30% of its hp back at once. Ready again in 120 s."));
        t.push_back(node(S::efficientPlans, b, 4, 3, {skillStat(SkillStat::buildingHydrogenCost, pct(-0.10))},
                         "Efficient plans", "Buildings cost 10% less MH, a rank."));
        t.push_back(node(S::headStart, b, Progression::capstone, 1, {skillStat(SkillStat::builtOnPlacing, plus(0.25))},
                         "Head start", "A building you place starts a quarter built."));
    }

    // Wayfinder (the road)
    {
        const Branch b = Branch::wayfinder;
        t.push_back(node(S::lightFrame, b, 1, 5, {driven(Stat::Speed{}, pct(0.05))}, "Light frame",
                         "Walk 5% faster a rank."));
        t.push_back(ranked(S::surveyor, b, 1,
                           {{SkillEffect::Surveyor{15}}, {SkillEffect::Surveyor{25}}, {SkillEffect::Surveyor{35}}},
                           "Surveyor", "Blueprint sites show on the compass within 15 cells; then 25, then 35."));
        t.push_back(node(S::longSight, b, 1, 3, {driven(Stat::Sight{}, pct(0.10))}, "Long sight",
                         "See 10% farther, a rank."));
        t.push_back(node(S::dustRunner, b, 2, 3, {skillStat(SkillStat::speedCarryingNothing, pct(0.05))},
                         "Dust runner", "Walk 5% faster while carrying nothing, a rank."));
        t.push_back(ranked(S::scannerPulse, b, 2,
                           {{SkillEffect::ScannerPulse{{SiteKind::wreck}}},
                            {SkillEffect::ScannerPulse{{SiteKind::wreck, SiteKind::cache}}},
                            {SkillEffect::ScannerPulse{{SiteKind::wreck, SiteKind::cache, SiteKind::vault}}}},
                           "Scanner pulse", "Arriving in a region marks its wrecks on the map; then caches, then vaults."));
        t.push_back(node(S::cliffHop, b, 2, 1, {SkillEffect::Core{Effect::CliffJump{}}}, "Cliff hop",
                         "Jump up and down cliffs, for 2 MH a jump."));
        t.push_back(node(S::jumpJets, b, 3, 3, {driven(Stat::JumpReach{}, plus(1))}, "Jump jets",
                         "Jump a cell farther, a rank. At rank 3 a jump burns 1 MH."));
        t.push_back(ranked(S::padLink, b, 3,
                           {{SkillEffect::PadLink{8}}, {SkillEffect::PadLink{12}}, {SkillEffect::PadLink{16}}},
                           "Pad link", "Call the Dropship from within 8 cells of a Landing Pad, up from 4; then 12, then 16."));
        // F2: Homing beacon is gone (the Dropship lands only on Landing
        // Pads). Its place stays, with no ranks, for the table's order and
        // old saves (`retired`); `nodes()` leaves it out.
        t.push_back(node(S::homingBeacon, b, 3, 0, {}, "Homing beacon", "Retired: the Dropship lands only on Landing Pads."));
        t.push_back(node(S::luckyStrike, b, 4, 5, {skillStat(SkillStat::chipChance, pct(0.10))}, "Lucky strike",
                         "Survey chips come 10% more often, a rank."));
        t.push_back(active(S::sprint, b, 4, 30,
                           SkillEffect::Core{Effect::Burst{Burst{.seconds = 6, .cooldown = 30, .speed = 0.6}}},
                           "Sprint", "Walk 60% faster for 6 s. Ready again in 30 s."));
        t.push_back(node(S::shortHop, b, 4, 3, {skillStat(SkillStat::flightTime, pct(-0.20))}, "Short hop",
                         "The Dropship's dash across the sky is 20% shorter, a rank."));
        t.push_back(node(S::ringwalker, b, Progression::capstone, 1, {SkillEffect::Ringwalker{0.3}}, "Ringwalker",
                         "Flights cost no MH, from any Landing Pad, and the Dropship comes 30% faster."));
    }

    // Hardshell (the plate)
    {
        const Branch b = Branch::hardshell;
        t.push_back(node(S::plating, b, 1, 5, {driven(Stat::Hp{}, plus(6))}, "Plating", "6 more hp a rank."));
        t.push_back(node(S::hardHat, b, 1, 3, {driven(Stat::Armor{}, plus(1))}, "Hard hat", "1 more armor a rank."));
        t.push_back(node(S::cuttingTorch, b, 1, 3, {driven(Stat::Damage{}, pct(0.20))}, "Cutting torch",
                         "The torch does 20% more damage, a rank."));
        t.push_back(ranked(S::shield, b, 2,
                           {{SkillEffect::Core{Effect::Shield{Shield{.hp = 10}}}},
                            {SkillEffect::Core{Effect::Shield{Shield{.hp = 20}}}},
                            {SkillEffect::Core{Effect::Shield{Shield{.hp = 30}}}}},
                           "Shield", "A 10 hp shield, then 20, then 30. It refills over 10 s after 7 s without a hit."));
        t.push_back(ranked(S::selfMend, b, 2,
                           {{SkillEffect::Core{Effect::SelfMend{1, 6}}}, {SkillEffect::Core{Effect::SelfMend{2, 6}}},
                            {SkillEffect::Core{Effect::SelfMend{3, 6}}}},
                           "Self mend", "Mend 1 hp a second after 6 s without a hit; then 2, then 3."));
        t.push_back(active(S::pulseMine, b, 2, 14,
                           SkillEffect::Core{Effect::Grenade{Grenade{.damage = 20, .radius = 1.5, .cooldown = 14}}},
                           "Pulse mine", "A charge at the crosshair: 20 damage within 1.5 cells. Ready again in 14 s."));
        t.push_back(node(S::longTorch, b, 3, 3, {driven(Stat::Range{}, plus(0.5))}, "Long torch",
                         "The torch reaches half a cell farther, a rank."));
        t.push_back(node(S::bulwark, b, 3, 3, {driven(Stat::DamageTaken{}, pct(-0.05))}, "Bulwark",
                         "Take 5% less damage, a rank."));
        t.push_back(ranked(S::secondWind, b, 3,
                           {{SkillEffect::SecondWind{0.25, 0.25}}, {SkillEffect::SecondWind{0.5, 0.5}},
                            {SkillEffect::SecondWind{0.75, 0.75}}},
                           "Second wind", "After a death, back 25% sooner and keep 25% of the cargo, a rank."));
        t.push_back(node(S::rigArmor, b, 4, 5, {driven(Stat::Hp{}, plus(10))}, "Rig armor", "10 more hp a rank."));
        t.push_back(active(S::aegis, b, 4, 45, SkillEffect::Burst{ActiveBurst{.seconds = 6, .damageTaken = -0.5}},
                           "Aegis", "Take half damage for 6 s. Ready again in 45 s."));
        t.push_back(ranked(S::torchRhythm, b, 4,
                           {{SkillEffect::EveryNth{Work::strike, 5}}, {SkillEffect::EveryNth{Work::strike, 4}},
                            {SkillEffect::EveryNth{Work::strike, 3}}},
                           "Torch rhythm", "Every 5th strike does double damage; every 4th, then every 3rd."));
        t.push_back(node(S::ironclad, b, Progression::capstone, 1, {SkillEffect::ShieldUp{40, 5}}, "Ironclad",
                         "The shield gains 40 hp and refills in 5 s, not 10."));
    }
    return t;
}

} // namespace

namespace {
/// Every skill's node, the retired ones too, in `Skill` order.
const std::vector<SkillNode>& everyNode() {
    static const std::vector<SkillNode> n = [] {
        auto t = table();
        std::sort(t.begin(), t.end(), [](const SkillNode& a, const SkillNode& b) { return a.skill < b.skill; });
        return t;
    }();
    return n;
}
} // namespace

const std::vector<SkillNode>& Progression::nodes(UnitKind kind) {
    static const std::vector<SkillNode> n = [] {
        std::vector<SkillNode> t;
        for (const SkillNode& x : everyNode())
            if (!retired(x.skill)) t.push_back(x);
        return t;
    }();
    static const std::vector<SkillNode> none;
    return kind == UnitKind::prospector ? n : none;
}

bool Progression::inTree(UnitKind kind, Skill s) {
    const auto& t = nodes(kind);
    return std::any_of(t.begin(), t.end(), [&](const SkillNode& n) { return n.skill == s; });
}

const SkillNode& Progression::node(Skill s) { return everyNode()[static_cast<size_t>(s)]; }

bool Progression::retired(Skill s) { return s == Skill::homingBeacon; }

const BranchInfo& Progression::branch(Branch b) {
    static const std::array<BranchInfo, 5> info{{
        {Branch::deepVein, "Deep Vein", "The drill: faster drilling, bigger loads, richer strikes."},
        {Branch::quicksilver, "Quicksilver", "The well: faster pumping, bigger tanks, Derricks that tend themselves."},
        {Branch::torchwright, "Torchwright", "The torch: building, repairing, and blueprints."},
        {Branch::wayfinder, "Wayfinder", "The road: moving, surveying, finding the sites."},
        {Branch::hardshell, "Hardshell",
         "The plate, for the frontier: staying alive, and the cutting torch as a weapon."},
    }};
    return info[static_cast<size_t>(b)];
}

std::vector<SkillEffect> Progression::effects(Skill s, int64_t rank) {
    const SkillNode& n = node(s);
    rank = std::clamp(rank, int64_t(0), n.ranks);
    if (rank == 0) return {};
    if (!n.byRank.empty()) return n.byRank[static_cast<size_t>(rank - 1)];
    std::vector<SkillEffect> out;
    for (int64_t r = 0; r < rank; r++) out.insert(out.end(), n.effects.begin(), n.effects.end());
    return out;
}

int64_t Progression::branchPoints(Branch b) {
    int64_t sum = 0;
    for (const auto& n : nodes())
        if (n.branch == b) sum += n.ranks;
    return sum;
}

int64_t Progression::treePoints() {
    int64_t sum = 0;
    for (const auto& n : nodes()) sum += n.ranks;
    return sum;
}

// MARK: - The skill screen's words

namespace {

/// "6%" from 0.06; "0.5" from 0.5; "12" from 12.
std::string percent(double x) {
    const double v = std::round(std::abs(x) * 1000) / 10;
    std::string s = swiftDescription(v);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
    return s + "%";
}
std::string number(double x) {
    std::string s = swiftDescription(std::round(std::abs(x) * 100) / 100);
    if (s.size() > 2 && s.compare(s.size() - 2, 2, ".0") == 0) s.resize(s.size() - 2);
    return s;
}
std::string signedPercent(double x) { return (x < 0 ? "−" : "+") + percent(x); }
std::string ordinal(int64_t n) {
    const int64_t t = n % 100;
    const char* suffix = (t >= 11 && t <= 13) ? "th" : n % 10 == 1 ? "st" : n % 10 == 2 ? "nd" : n % 10 == 3 ? "rd" : "th";
    return std::to_string(n) + suffix;
}
std::string list(const std::vector<std::string>& words) {
    std::string out;
    for (size_t i = 0; i < words.size(); i++) {
        if (i > 0) out += i + 1 == words.size() ? " and " : ", ";
        out += words[i];
    }
    return out;
}
std::string plural(SiteKind k) {
    switch (k) {
    case SiteKind::crate: return "crates";
    case SiteKind::wreck: return "wrecks";
    case SiteKind::cache: return "caches";
    case SiteKind::vault: return "vaults";
    }
    return "sites";
}

/// A stat of the strategy game's, changed by `c` in all.
std::string coreWords(const Stat& stat, const Change& c) {
    const auto* p = c.as<Change::Percent>();
    const auto* q = c.as<Change::Plus>();
    const double x = p ? p->x : q ? q->x : 0;
    if (stat.is<Stat::Drill>()) return "Drill " + signedPercent(x);
    if (stat.is<Stat::OreCarry>()) return p ? "Ore loads " + signedPercent(x) : "+" + number(x) + " ore a trip";
    if (stat.is<Stat::Hydrogen>()) return "Pump " + signedPercent(x);
    if (stat.is<Stat::HydrogenCarry>()) return "+" + number(x) + " MH a trip";
    if (stat.is<Stat::Build>()) return "Build " + signedPercent(x);
    if (stat.is<Stat::Repair>()) return "Repair " + signedPercent(x);
    if (stat.is<Stat::BuildingCost>()) return "Buildings cost " + percent(x) + " less";
    if (stat.is<Stat::Speed>()) return "Walk " + signedPercent(x);
    if (stat.is<Stat::Sight>()) return "Sight " + signedPercent(x);
    if (stat.is<Stat::JumpReach>()) return "Jump " + number(x) + (x == 1 ? " cell" : " cells") + " farther";
    if (stat.is<Stat::Hp>()) return "+" + number(x) + " hp";
    if (stat.is<Stat::Armor>()) return "+" + number(x) + " armor";
    if (stat.is<Stat::Damage>()) return "Torch damage " + signedPercent(x);
    if (stat.is<Stat::Range>()) return "Torch reach +" + number(x) + (x == 1 ? " cell" : " cells");
    if (stat.is<Stat::DamageTaken>()) return "Damage taken " + signedPercent(x);
    return p ? signedPercent(x) : "+" + number(x);
}

std::string skillWords(SkillStat stat, double x) {
    switch (stat) {
    case SkillStat::depositYield: return "A load takes " + percent(x) + " less from the deposit";
    case SkillStat::wellYield: return "A pump takes " + percent(x) + " less from the well";
    case SkillStat::regrowTime: return "Deposits you empty grow back " + percent(x) + " sooner";
    case SkillStat::unloadOre: return "Ore unloaded counts " + signedPercent(x);
    case SkillStat::unloadHydrogen: return "MH unloaded counts " + signedPercent(x);
    case SkillStat::loadTriple: return percent(x) + " chance a load carries triple";
    case SkillStat::chipChance: return "+" + percent(x) + " chance of a survey chip";
    case SkillStat::speedCarryingHydrogen: return "Walk " + signedPercent(x) + " carrying MH";
    case SkillStat::speedCarryingNothing: return "Walk " + signedPercent(x) + " carrying nothing";
    case SkillStat::workReach: return "Weld and repair from " + number(x) + (x == 1 ? " cell" : " cells") + " farther";
    case SkillStat::flightTime: return "Flights take " + percent(x) + " less time";
    case SkillStat::buildingHydrogenCost: return "Buildings cost " + percent(x) + " less MH";
    case SkillStat::ownBuildingHp: return "Your buildings " + signedPercent(x) + " hp";
    case SkillStat::ownBuildingDamageTaken: return "Your buildings take " + percent(x) + " less damage";
    case SkillStat::blueprintFloorCut: return "Blueprints open " + number(x) + " levels below their floor";
    case SkillStat::builtOnPlacing: return "Buildings start " + percent(x) + " built";
    }
    return "";
}

std::string effectWords(const SkillEffect& e) {
    if (auto n = e.as<SkillEffect::EveryNth>()) {
        const std::string what = n->work == Work::oreTrip ? "ore trip carries double"
                                 : n->work == Work::pump  ? "pump carries double"
                                                          : "strike does double damage";
        return "Every " + ordinal(n->n) + " " + what;
    }
    if (auto t = e.as<SkillEffect::DerrickTender>())
        return "Derricks fill a " + number(t->tank) + " MH tank, " + number(t->perMinute) + " MH a minute";
    if (auto w = e.as<SkillEffect::Wellspring>()) return "Wells under your Derricks grow back " + number(w->perMinute) + " MH a minute";
    if (auto f = e.as<SkillEffect::FieldPump>()) return "Pump straight from a well at " + percent(f->share) + " of a Derrick's rate";
    if (auto p = e.as<SkillEffect::PadLink>()) return "Call the Dropship from within " + number(p->radius) + " cells of a Landing Pad";
    if (auto r = e.as<SkillEffect::Ringwalker>())
        return "Flights cost no MH, and the Dropship comes " + percent(r->faster) + " faster";
    if (auto s = e.as<SkillEffect::Surveyor>()) return "Sites show on the compass within " + number(s->radius) + " cells";
    if (auto p = e.as<SkillEffect::ScannerPulse>()) {
        std::vector<std::string> w;
        for (SiteKind k : p->kinds) w.push_back(plural(k));
        return "Arriving marks a region's " + list(w);
    }
    if (auto w = e.as<SkillEffect::SecondWind>())
        return "Back " + percent(w->sooner) + " sooner, keep " + percent(w->cargo) + " of the cargo";
    if (auto c = e.as<SkillEffect::Core>()) {
        if (auto s = c->effect.as<Effect::Shield>()) return "A " + number(s->shield.hp) + " hp shield";
        if (auto m = c->effect.as<Effect::SelfMend>()) return "Mend " + number(m->hpPerSecond) + " hp a second";
    }
    return "";
}

} // namespace

std::string Progression::rankText(Skill s, int64_t rank) {
    const SkillNode& n = node(s);
    rank = std::clamp(rank, int64_t(0), n.ranks);
    if (rank == 0) return {};
    if (n.ranks == 1) return n.text;
    if (!n.byRank.empty()) {
        std::vector<std::string> w;
        for (const SkillEffect& e : n.byRank[static_cast<size_t>(rank - 1)]) w.push_back(effectWords(e));
        return list(w);
    }
    // Ranks that add up: one rank's effects, times `rank`.
    const double r = static_cast<double>(rank);
    std::vector<std::string> w;
    std::vector<std::string> limits;
    for (const SkillEffect& e : n.effects) {
        if (auto c = e.as<SkillEffect::Core>()) {
            if (auto st = c->effect.as<Effect::Stat>()) {
                Change total = st->change;
                if (auto p = total.as<Change::Percent>()) total = Change::Percent{p->x * r};
                else if (auto q = total.as<Change::Plus>()) total = Change::Plus{q->x * r};
                w.push_back(coreWords(st->stat, total));
            }
        } else if (auto st = e.as<SkillEffect::Stat>()) {
            const auto* p = st->change.as<Change::Percent>();
            const auto* q = st->change.as<Change::Plus>();
            w.push_back(skillWords(st->stat, (p ? p->x : q ? q->x : 0) * r));
        } else if (auto l = e.as<SkillEffect::BuildingLimit>()) {
            limits.push_back(Simulation::title(l->kind));
        }
    }
    if (!limits.empty()) w.push_back("+" + number(r) + " " + list(limits) + " allowed");
    return list(w);
}

const std::vector<StructureKind>& Progression::blueprintKinds() {
    // By floor (D2 brought the gatling gun in at 5 and the Sentinel down to 10).
    // F2: the Landing Pad at 1, beside the Citadel.
    static const std::vector<StructureKind> k{StructureKind::citadel,  StructureKind::landingPad, StructureKind::derrick, StructureKind::gatling,
                                              StructureKind::habDome,  StructureKind::sentinel, StructureKind::foundry,
                                              StructureKind::lab,      StructureKind::spacedock, StructureKind::bastion};
    return k;
}

int64_t Progression::blueprintFloor(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 1;
    case StructureKind::landingPad: return 1;
    case StructureKind::derrick: return 3;
    case StructureKind::gatling: return 5;
    case StructureKind::habDome: return 8;
    case StructureKind::foundry: return 15;
    case StructureKind::lab: return 25;
    case StructureKind::spacedock: return 35;
    case StructureKind::bastion: return 45;
    case StructureKind::sentinel: return 10;
    default: return 0;
    }
}

int64_t Progression::buildingLimit(StructureKind k) {
    switch (k) {
    // R3: one a region (`perRegion`).
    case StructureKind::citadel: return 1;
    case StructureKind::derrick: return 4;
    case StructureKind::habDome: return 3;
    case StructureKind::foundry: return 1;
    case StructureKind::lab: return 1;
    case StructureKind::spacedock: return 1;
    case StructureKind::bastion: return 2;
    case StructureKind::sentinel: return 4;
    case StructureKind::gatling: return 6;
    // F2: per region, not per realm (the server counts the region's).
    case StructureKind::landingPad: return Progression::landingPadsPerRegion;
    default: return 0;
    }
}

int64_t Progression::worldOre(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: return 400;
    case StructureKind::derrick: return 75;
    case StructureKind::habDome: return 150;
    case StructureKind::foundry: return 300;
    case StructureKind::lab: return 200;
    case StructureKind::spacedock: return 400;
    case StructureKind::bastion: return 250;
    case StructureKind::sentinel: return 150;
    case StructureKind::gatling: return 125;
    case StructureKind::landingPad: return 60;
    default: return Rules::cost(k);
    }
}

int64_t Progression::worldHydrogen(StructureKind k) {
    switch (k) {
    case StructureKind::citadel: case StructureKind::derrick: case StructureKind::landingPad: return 0;
    case StructureKind::habDome: return 50;
    case StructureKind::foundry: return 150;
    case StructureKind::lab: return 200;
    case StructureKind::spacedock: return 300;
    case StructureKind::bastion: return 100;
    case StructureKind::sentinel: return 100;
    case StructureKind::gatling: return 25;
    default: return Rules::hydrogenCost(k);
    }
}

// MARK: - The balance

namespace {

std::string d(double v) { return swiftDescription(v); }

std::string describe(const SkillEffect& e) {
    if (auto c = e.as<SkillEffect::Core>()) return ac::describe(c->effect);
    if (auto s = e.as<SkillEffect::Stat>())
        return "stat(" + std::string(rawValue(s->stat)) + ", " + ac::describe(s->change) + ")";
    if (auto l = e.as<SkillEffect::BuildingLimit>())
        return "buildingLimit(" + std::string(rawValue(l->kind)) + ", " + d(l->plus) + ")";
    if (auto n = e.as<SkillEffect::EveryNth>())
        return "everyNth(" + std::string(rawValue(n->work)) + ", " + std::to_string(n->n) + ")";
    if (auto b = e.as<SkillEffect::Burst>()) {
        const ActiveBurst& x = b->burst;
        return "burst(" + d(x.seconds) + ", drill " + d(x.drill) + ", build " + d(x.build) + ", repair " +
               d(x.repair) + ", speed " + d(x.speed) + ", damageTaken " + d(x.damageTaken) + ", hydrogen " +
               d(x.hydrogenCost) + ")";
    }
    if (auto s = e.as<SkillEffect::ShieldUp>()) return "shieldUp(" + d(s->hp) + ", " + d(s->refill) + ")";
    if (auto s = e.as<SkillEffect::SeismicCharge>()) return "seismicCharge(" + d(s->radius) + ")";
    if (e.is<SkillEffect::Quickfill>()) return "quickfill";
    if (auto w = e.as<SkillEffect::EmergencyWeld>()) return "emergencyWeld(" + d(w->radius) + ", " + d(w->share) + ")";
    if (auto t = e.as<SkillEffect::DerrickTender>()) return "derrickTender(" + d(t->tank) + ", " + d(t->perMinute) + ")";
    if (auto w = e.as<SkillEffect::Wellspring>()) return "wellspring(" + d(w->perMinute) + ")";
    if (auto f = e.as<SkillEffect::FieldPump>()) return "fieldPump(" + d(f->share) + ")";
    if (auto p = e.as<SkillEffect::PadLink>()) return "padLink(" + d(p->radius) + ")";
    if (auto r = e.as<SkillEffect::Ringwalker>()) return "ringwalker(" + d(r->faster) + ")";
    if (auto s = e.as<SkillEffect::Surveyor>()) return "surveyor(" + d(s->radius) + ")";
    if (auto p = e.as<SkillEffect::ScannerPulse>()) {
        std::string s = "scannerPulse(";
        for (size_t i = 0; i < p->kinds.size(); i++) s += (i ? ", " : "") + std::string(rawValue(p->kinds[i]));
        return s + ")";
    }
    if (auto w = e.as<SkillEffect::SecondWind>()) return "secondWind(" + d(w->sooner) + ", " + d(w->cargo) + ")";
    return "?";
}

std::string describe(const std::vector<SkillEffect>& effects) {
    std::string out = "[";
    for (size_t i = 0; i < effects.size(); i++) out += (i ? ", " : "") + describe(effects[i]);
    return out + "]";
}

} // namespace

std::string Progression::balance(const std::vector<SkillNode>& nodes) {
    std::string text;
    for (const auto& n : nodes) {
        text += std::string(rawValue(n.skill)) + "|" + std::string(rawValue(n.branch)) + "|" + std::to_string(n.tier) +
                "|" + std::to_string(n.ranks) + "|" + n.title + "|" + n.text + "|" + describe(n.effects);
        for (const auto& row : n.byRank) text += "|" + describe(row);
        if (n.active) text += "|active " + d(n.active->cooldown);
        text += "\n";
    }
    for (Branch b : allCases<Branch>()) text += "branch " + branch(b).title + "|" + branch(b).text + "\n";
    for (int64_t l = 2; l <= cap; l++) text += "xp " + std::to_string(l) + " " + d(xp(l)) + "\n";
    for (int64_t t = 1; t <= capstone; t++)
        text += "gate " + std::to_string(t) + " " + std::to_string(gate(t).level) + " " +
                std::to_string(gate(t).points) + "\n";
    for (int64_t s = 0; s < slotCount; s++) text += "slot " + std::string(slotKey(s)) + " " + std::to_string(slotLevel(s)) + "\n";
    text += "respec " + d(0.4) + " " + d(undoWindow) + "\n";
    return text;
}

std::string Progression::balance() { return balance(nodes()); }

const std::string& Progression::signature() {
    static const std::string s = Leveling::fnv1a(balance());
    return s;
}

// MARK: - A pilot's progress

int64_t PilotProgress::rank(Skill s) const {
    auto it = ranks.find(s);
    return it == ranks.end() ? 0 : it->second;
}

int64_t PilotProgress::spent() const {
    int64_t sum = 0;
    for (const auto& [s, r] : ranks) sum += r;
    return sum;
}

int64_t PilotProgress::spent(Branch b) const {
    int64_t sum = 0;
    for (const auto& [s, r] : ranks)
        if (Progression::node(s).branch == b) sum += r;
    return sum;
}

int64_t PilotProgress::spentBelow(Branch b, int64_t tier) const {
    int64_t sum = 0;
    for (const auto& [s, r] : ranks) {
        const SkillNode& n = Progression::node(s);
        if (n.branch == b && n.tier < tier) sum += r;
    }
    return sum;
}

int64_t PilotProgress::unspent() const { return Progression::points(level()) - spent(); }

std::optional<int64_t> PilotProgress::slotOf(Skill s) const {
    for (int64_t i = 0; i < Progression::slotCount; i++)
        if (slots[static_cast<size_t>(i)] == s) return i;
    return std::nullopt;
}

/// The gate counts the points in the branch's tiers below the node's, so
/// a point in a tier never opens that tier (the doc's "points already in
/// that branch" before the first point there).
std::optional<std::string> PilotProgress::spendRefusal(Skill s) const {
    const SkillNode& n = Progression::node(s);
    if (Progression::retired(s)) return std::string("Retired: ") + n.text;
    if (!Progression::inTree(kind, s)) return std::string("Not in this pilot's tree");
    if (rank(s) >= n.ranks)
        return "Full: " + std::to_string(n.ranks) + " of " + std::to_string(n.ranks);
    const Gate g = Progression::gate(n.tier);
    if (level() < g.level)
        return (n.tier == Progression::capstone ? std::string("Capstones open") : "Tier " + std::to_string(n.tier) + " opens") +
               " at level " + std::to_string(g.level);
    if (spentBelow(n.branch, n.tier) < g.points)
        return "Needs " + std::to_string(g.points) + " points in " + Progression::branch(n.branch).title;
    if (unspent() <= 0) return std::string("No point to spend");
    return std::nullopt;
}

std::optional<std::string> PilotProgress::undoRefusal(Skill s, double now) const {
    const bool fresh = std::any_of(recent.begin(), recent.end(), [&](const SkillSpend& x) {
        return x.skill == s && now - x.at <= Progression::undoWindow;
    });
    if (rank(s) == 0 || !fresh) return std::string("No point spent here in the last minute");
    // Without that point, every deeper node in the branch must still pass its gate.
    PilotProgress after = *this;
    after.ranks[s] -= 1;
    const Branch b = Progression::node(s).branch;
    for (const auto& [other, r] : after.ranks) {
        const SkillNode& n = Progression::node(other);
        if (r > 0 && n.branch == b && n.tier > Progression::node(s).tier &&
            after.spentBelow(b, n.tier) < Progression::gate(n.tier).points)
            return "Holds up " + n.title;
    }
    return std::nullopt;
}

std::optional<std::string> PilotProgress::slotRefusal(int64_t slot, std::optional<Skill> s) const {
    if (slot < 0 || slot >= Progression::slotCount) return std::string("No such slot");
    if (level() < Progression::slotLevel(slot))
        return "Slot " + std::string(Progression::slotKey(slot)) + " opens at level " +
               std::to_string(Progression::slotLevel(slot));
    if (!s) return std::nullopt;
    if (!Progression::node(*s).active) return Progression::node(*s).title + " is not an active";
    if (rank(*s) == 0) return Progression::node(*s).title + " is not learned";
    return std::nullopt;
}

bool PilotProgress::respecIsFree() const {
    return respecs == 0 || freeRespec || (!signature.empty() && signature != Progression::signature());
}

int64_t PilotProgress::respecCost() const { return respecIsFree() ? 0 : Progression::respecPrice(level()); }

std::optional<std::string> PilotProgress::respecRefusal(int64_t hydrogen) const {
    if (spent() == 0) return std::string("No points spent");
    const int64_t cost = respecCost();
    if (hydrogen < cost) return "Needs " + std::to_string(cost) + " MH";
    return std::nullopt;
}

Boost PilotProgress::skillBoost(SkillStat stat) const {
    Boost b;
    for (const auto& [s, rank] : ranks)
        for (const SkillEffect& e : Progression::effects(s, rank))
            if (const auto* x = e.as<SkillEffect::Stat>(); x && x->stat == stat) b.add(x->change);
    for (const SkillEffect& e : rig.effects(kind))
        if (const auto* x = e.as<SkillEffect::Stat>(); x && x->stat == stat) b.add(x->change);
    return b;
}

int64_t PilotProgress::floor(StructureKind k) const {
    const int64_t cut = int64_t(std::lround(skillBoost(SkillStat::blueprintFloorCut).plus));
    return std::max<int64_t>(1, Progression::blueprintFloor(k) - cut);
}

int64_t PilotProgress::limit(StructureKind k) const {
    double plus = 0;
    for (const auto& [s, rank] : ranks)
        for (const SkillEffect& e : Progression::effects(s, rank))
            if (const auto* l = e.as<SkillEffect::BuildingLimit>(); l && l->kind == k) plus += l->plus;
    const int64_t base = Progression::buildingLimit(k);
    return base > 0 ? base + int64_t(std::lround(plus)) : 0;
}

double PilotProgress::chipChance() const {
    const Boost b = skillBoost(SkillStat::chipChance);
    return (Progression::chipChance + b.plus) * (1 + b.percent);
}

std::optional<std::string> PilotProgress::buildRefusal(StructureKind k, int64_t owned) const {
    const std::string name = Simulation::title(k);
    if (Progression::blueprintFloor(k) <= 0) return "No " + name + " blueprint in this world";
    if (copies(k) <= 0) return std::string("No blueprint: buy one at a Citadel (H)");
    if (level() < floor(k)) return "The " + name + " opens at level " + std::to_string(floor(k));
    if (owned >= limit(k)) {
        if (k == StructureKind::landingPad) return "Landing Pad limit reached: " + std::to_string(limit(k)) + " in a region";
        if (k == StructureKind::citadel) return std::string("This region has its Citadel: one a region");
        const bool more = k == StructureKind::habDome || k == StructureKind::derrick || k == StructureKind::sentinel;
        return name + " limit reached: " + std::to_string(limit(k)) + (more ? " (Overbuild allows more)" : "");
    }
    return std::nullopt;
}

int64_t PilotProgress::copies(StructureKind k) const {
    const auto it = blueprints.find(k);
    return it == blueprints.end() ? 0 : it->second;
}

int64_t PilotProgress::held(Item i) const {
    const auto it = items.find(i);
    return it == items.end() ? 0 : it->second;
}

void PilotProgress::giveBlueprint(StructureKind k, int64_t n) {
    if (n > 0) blueprints[k] += n;
}

bool PilotProgress::useBlueprint(StructureKind k) {
    const auto it = blueprints.find(k);
    if (it == blueprints.end() || it->second <= 0) return false;
    if (--it->second <= 0) blueprints.erase(it);
    return true;
}

ShopPrice PilotProgress::blueprintPrice(StructureKind k) const {
    const auto it = bought.find(k);
    return Shop::blueprintPrice(k, it == bought.end() ? 0 : it->second);
}

bool PilotProgress::useItem(Item i) {
    const auto it = items.find(i);
    if (it == items.end() || it->second <= 0) return false;
    if (--it->second <= 0) items.erase(it);
    return true;
}

// MARK: - The Citadel shop (C2)

ShopPrice Shop::blueprintPrice(StructureKind k, int64_t bought) {
    const int64_t floor = Progression::blueprintFloor(k);
    if (floor <= 0) return {};
    const double grow = std::pow(blueprintGrowth, double(std::max<int64_t>(0, bought)));
    auto ten = [](double x) { return int64_t(std::llround(x / 10.0)) * 10; };
    const bool oreOnly = k == StructureKind::citadel || k == StructureKind::derrick || k == StructureKind::landingPad;
    return {std::max<int64_t>(10, ten(double(50 + 25 * floor) * grow)), oreOnly ? 0 : ten(double(5 * floor) * grow)};
}

const std::vector<ItemInfo>& Shop::items() {
    static const std::vector<ItemInfo> list{
        {Item::repairKit, "Repair kit", "Mends half the Prospector's hp at once", 120, 10, 5, 30, 0},
        {Item::hydrogenCanister, "MH canister", "25 MH into the wallet, for jets and flights", 150, 0, 3, 60, 0},
        {Item::shieldCharge, "Shield charge", "No damage taken for 8 s", 100, 30, 3, 45, 8},
        {Item::surveyFlare, "Survey flare", "Sites and deposits near on the map, 60 s", 80, 15, 5, 20, 60},
    };
    return list;
}

const ItemInfo& Shop::item(Item i) {
    for (const ItemInfo& x : items())
        if (x.item == i) return x;
    return items().front();
}

bool PilotProgress::spend(Skill s, double now) {
    if (spendRefusal(s)) return false;
    ranks[s] += 1;
    std::erase_if(recent, [&](const SkillSpend& x) { return now - x.at > Progression::undoWindow; });
    recent.push_back({s, now});
    signature = Progression::signature();
    return true;
}

bool PilotProgress::undo(Skill s, double now) {
    if (undoRefusal(s, now)) return false;
    if (--ranks[s] == 0) {
        ranks.erase(s);
        if (auto i = slotOf(s)) slots[static_cast<size_t>(*i)].reset();
    }
    for (auto it = recent.rbegin(); it != recent.rend(); ++it)
        if (it->skill == s) {
            recent.erase(std::next(it).base());
            break;
        }
    return true;
}

bool PilotProgress::slot(int64_t index, std::optional<Skill> s) {
    if (slotRefusal(index, s)) return false;
    if (s)
        if (auto from = slotOf(*s)) slots[static_cast<size_t>(*from)].reset();
    slots[static_cast<size_t>(index)] = s;
    return true;
}

std::optional<int64_t> PilotProgress::respec(int64_t hydrogen) {
    if (respecRefusal(hydrogen)) return std::nullopt;
    const int64_t cost = respecCost();
    ranks.clear();
    slots = {};
    recent.clear();
    respecs += 1;
    freeRespec = false;
    signature = Progression::signature();
    return cost;
}

// MARK: - The rig (P7)

const std::vector<Module>& Modules::all() {
    static const std::vector<Module> list{Module::drillHead, Module::cargoPod, Module::plate, Module::torchTip,
                                          Module::surveyLens};
    return list;
}

std::string Modules::title(Module m) {
    switch (m) {
    case Module::drillHead: return "Drill head";
    case Module::cargoPod: return "Cargo pod";
    case Module::plate: return "Plate";
    case Module::torchTip: return "Torch tip";
    case Module::surveyLens: return "Survey lens";
    }
    return "";
}

std::string Modules::markName(int64_t mark) {
    static const char* names[] = {"Mk I", "Mk II", "Mk III"};
    return mark >= 1 && mark <= marks ? names[mark - 1] : "";
}

namespace {

/// Row `mark` − 1 of a three-mark table (0 out of range).
template <class T> T row(const std::array<T, 3>& table, int64_t mark) {
    return mark >= 1 && mark <= Modules::marks ? table[size_t(mark - 1)] : T{};
}

} // namespace

int64_t Modules::ore(int64_t mark) { return row<int64_t>({100, 300, 800}, mark); }
int64_t Modules::hydrogen(int64_t mark) { return row<int64_t>({50, 200, 600}, mark); }
double Modules::makeTime(int64_t mark) { return row<double>({60, 180, 480}, mark); }
int64_t Modules::researchHydrogen(int64_t mark) { return row<int64_t>({0, 300, 900}, mark); }
double Modules::researchTime(int64_t mark) { return row<double>({0, 600, 1800}, mark); }
double Modules::share(int64_t mark) { return row<double>({0.08, 0.16, 0.25}, mark); }

std::vector<SkillEffect> Modules::effects(Module m, int64_t mark, UnitKind kind) {
    const double s = share(mark);
    if (!(s > 0)) return {};
    auto driven = [](Stat stat, Change change) { return SkillEffect{SkillEffect::Core{Effect::Stat{stat, Scope::Driven{}, change}}}; };
    switch (m) {
    case Module::drillHead: return {driven(Stat::Drill{}, Change::Percent{s})};
    case Module::cargoPod: return {driven(Stat::OreCarry{}, Change::Percent{s}), driven(Stat::HydrogenCarry{}, Change::Percent{s})};
    // The sim adds hp as `.plus`: the share of the kind's own.
    case Module::plate: return {driven(Stat::Hp{}, Change::Plus{std::round(s * Rules::hp(kind))})};
    case Module::torchTip:
        return {driven(Stat::Damage{}, Change::Percent{s}), driven(Stat::Range{}, Change::Plus{0.25 * double(mark)})};
    case Module::surveyLens:
        return {driven(Stat::Sight{}, Change::Percent{s}),
                SkillEffect{SkillEffect::Stat{SkillStat::chipChance, Change::Percent{s}}}};
    }
    return {};
}

std::string Modules::text(Module m, int64_t mark, UnitKind kind) {
    const double s = share(mark);
    if (!(s > 0)) return "";
    const std::string pct = "+" + std::to_string(int(std::lround(s * 100))) + "%";
    switch (m) {
    case Module::drillHead: return "Drill " + pct;
    case Module::cargoPod: return "Ore and MH a trip " + pct;
    case Module::plate: return "Hp +" + std::to_string(int(std::lround(s * Rules::hp(kind))));
    case Module::torchTip: {
        char range[16];
        std::snprintf(range, sizeof range, "%.2g", 0.25 * double(mark));
        return "Torch damage " + pct + ", range +" + range + " cells";
    }
    case Module::surveyLens: return "Sight " + pct + ", survey chips " + pct;
    }
    return "";
}

int64_t Rig::fittedMark(Module m) const {
    const auto it = fitted.find(m);
    return it == fitted.end() ? 0 : it->second;
}

int64_t Rig::stock(Module m, int64_t mark) const {
    for (const ModuleStock& s : stores)
        if (s.module == m && s.mark == mark) return s.count;
    return 0;
}

int64_t Rig::researchedMark(Module m) const {
    const auto it = researched.find(m);
    return std::max<int64_t>(1, it == researched.end() ? 1 : it->second);
}

void Rig::store(Module m, int64_t mark, int64_t n) {
    if (n <= 0) return;
    for (ModuleStock& s : stores)
        if (s.module == m && s.mark == mark) {
            s.count += n;
            return;
        }
    stores.push_back({m, mark, n});
    std::sort(stores.begin(), stores.end(), [](const ModuleStock& a, const ModuleStock& b) {
        return std::pair(a.module, a.mark) < std::pair(b.module, b.mark);
    });
}

bool Rig::take(Module m, int64_t mark) {
    for (auto it = stores.begin(); it != stores.end(); ++it)
        if (it->module == m && it->mark == mark && it->count > 0) {
            if (--it->count == 0) stores.erase(it);
            return true;
        }
    return false;
}

std::optional<std::string> Rig::makeRefusal(Module m, int64_t mark, int64_t ore, int64_t hydrogen) const {
    if (mark < 1 || mark > Modules::marks) return std::string("No such mark");
    const std::string name = Modules::title(m) + " " + Modules::markName(mark);
    if (making) return "The Foundry is busy: " + Modules::title(making->module) + " " + Modules::markName(making->mark);
    if (researchedMark(m) < mark) return name + " needs research at the Lab first";
    if (ore < Modules::ore(mark)) return std::string("Not enough ore");
    if (hydrogen < Modules::hydrogen(mark)) return std::string("Not enough MH");
    return std::nullopt;
}

std::optional<std::string> Rig::researchRefusal(Module m, int64_t mark, int64_t hydrogen) const {
    if (mark < 2 || mark > Modules::marks) return std::string("Only Mk II and Mk III are researched");
    const std::string name = Modules::title(m) + " " + Modules::markName(mark);
    if (researchedMark(m) >= mark) return name + " is researched already";
    if (research) return "The Lab is busy: " + Modules::title(research->module) + " " + Modules::markName(research->mark);
    if (researchedMark(m) < mark - 1) return name + " needs " + Modules::markName(mark - 1) + " researched first";
    if (hydrogen < Modules::researchHydrogen(mark)) return std::string("Not enough MH");
    return std::nullopt;
}

std::optional<std::string> Rig::fitRefusal(Module m, int64_t mark) const {
    if (mark < 0 || mark > Modules::marks) return std::string("No such mark");
    if (mark == 0) {
        if (fittedMark(m) == 0) return "No " + Modules::title(m) + " fitted";
        return std::nullopt;
    }
    if (fittedMark(m) == mark) return Modules::title(m) + " " + Modules::markName(mark) + " is fitted already";
    if (stock(m, mark) <= 0) return "No " + Modules::title(m) + " " + Modules::markName(mark) + " in your stores";
    return std::nullopt;
}

bool Rig::make(Module m, int64_t mark, int64_t ore, int64_t hydrogen, double now) {
    if (makeRefusal(m, mark, ore, hydrogen)) return false;
    making = ModuleJob{m, mark, now + Modules::makeTime(mark)};
    return true;
}

bool Rig::startResearch(Module m, int64_t mark, int64_t hydrogen, double now) {
    if (researchRefusal(m, mark, hydrogen)) return false;
    research = ModuleJob{m, mark, now + Modules::researchTime(mark)};
    return true;
}

bool Rig::fit(Module m, int64_t mark) {
    if (fitRefusal(m, mark)) return false;
    if (mark > 0) take(m, mark);
    if (const int64_t old = fittedMark(m)) store(m, old);
    if (mark > 0) fitted[m] = mark;
    else fitted.erase(m);
    return true;
}

std::pair<std::optional<ModuleJob>, std::optional<ModuleJob>> Rig::finish(double now) {
    std::pair<std::optional<ModuleJob>, std::optional<ModuleJob>> out;
    if (making && making->done <= now + 1e-9) {
        store(making->module, making->mark);
        out.first = making;
        making.reset();
    }
    if (research && research->done <= now + 1e-9) {
        researched[research->module] = std::max(researchedMark(research->module), research->mark);
        out.second = research;
        research.reset();
    }
    return out;
}

std::vector<SkillEffect> Rig::effects(UnitKind kind) const {
    std::vector<SkillEffect> out;
    for (const auto& [m, mark] : fitted) {
        const auto e = Modules::effects(m, mark, kind);
        out.insert(out.end(), e.begin(), e.end());
    }
    return out;
}

} // namespace ac
