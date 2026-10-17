// Port of Sources/GameCore/Leveling.swift: `Leveling`, the effect and record
// types' methods, the perk table (`perkRows`: Swift's `Perk.rows` and `built`
// in one) with the `catalogue` and `offers` read from it, the balance text
// and its signature.
#include "Leveling.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <variant>
#include <limits>

namespace ac {

/// XP in all to reach `level`: `30 × L × (L − 1)` (0 for level 1).
double Leveling::xp(int64_t toReach) {
    const double l = static_cast<double>(max(int64_t(1), min(toReach, cap)));
    return 30 * l * (l - 1);
}

/// The level `xp` in all reaches.
int64_t Leveling::level(double xp) {
    int64_t l = 1;
    while (l < cap && xp >= Leveling::xp(l + 1)) l += 1;
    return l;
}

/// A unit's worth for XP: its ore plus its MH.
double Leveling::worth(UnitKind k) { return static_cast<double>(Rules::cost(k) + Rules::hydrogenCost(k)); }
double Leveling::worth(StructureKind k) { return static_cast<double>(Rules::cost(k) + Rules::hydrogenCost(k)); }

/// Level 1: the driven unit is a hero (`Rules.hero*`). Picks build on
/// top of these.
std::optional<Change> Leveling::hero(const Stat& stat) {
    if (stat.is<Stat::Speed>()) return Change::Times{Rules::heroSpeed};
    if (stat.is<Stat::Damage>()) return Change::Times{Rules::heroDamage};
    if (stat.is<Stat::Range>()) return Change::Times{Rules::heroRange};
    if (stat.is<Stat::Build>() || stat.is<Stat::Repair>()) return Change::Times{Rules::heroWork};
    if (stat.is<Stat::OreCarry>()) return Change::AtLeast{static_cast<double>(Rules::heroCarry)};
    if (stat.is<Stat::HydrogenCarry>()) return Change::AtLeast{static_cast<double>(Rules::heroHydrogenCarry)};
    return std::nullopt;
}

/// FNV-1a 64 of `text`'s UTF-8, as 16 hex digits.
std::string Leveling::fnv1a(const std::string& text) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (unsigned char b : text) h = (h ^ static_cast<uint64_t>(b)) * 0x100000001b3ull;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016" PRIx64, h);
    return buf;
}

// MARK: - Effects

bool covers(Filter f, UnitKind k) {
    switch (f) {
    case Filter::machines: return Leveling::machines.contains(k);
    case Filter::bio: return Leveling::bio.contains(k);
    case Filter::all: return true;
    case Filter::air: return Rules::stats(k).air;
    }
    return true;
}

void Boost::add(const Change& c) {
    if (auto p = c.as<Change::Percent>()) percent += p->x;
    else if (auto t = c.as<Change::Times>()) times = max(times, t->x);
    else if (auto a = c.as<Change::AtLeast>()) atLeast = max(atLeast ? *atLeast : a->x, a->x);
    else if (auto q = c.as<Change::Plus>()) plus += q->x;
}

double Boost::apply(double base) const {
    return max(base * times, atLeast ? *atLeast : -std::numeric_limits<double>::infinity()) * (1 + percent) + plus;
}

/// A whole number (a cost, a load) under the changes, rounded.
int64_t Boost::apply(int64_t base) const { return static_cast<int64_t>(rounded(apply(static_cast<double>(base)))); }

// MARK: - The record

int64_t PilotRecord::level(UnitKind k) const {
    auto it = kinds.find(k);
    return it != kinds.end() ? it->second.level() : 1;
}

double PilotRecord::xp(UnitKind k) const {
    auto it = kinds.find(k);
    return it != kinds.end() ? it->second.xp : 0;
}

/// The picks made for a kind, lowest level first.
std::vector<Perk> PilotRecord::picks(UnitKind k) const {
    auto it = kinds.find(k);
    return it != kinds.end() ? it->second.picks : std::vector<Perk>{};
}

/// The kinds driven this game, in `Unit.Kind` order (the command map's
/// list).
std::vector<UnitKind> PilotRecord::driven() const {
    std::vector<UnitKind> out;
    for (UnitKind k : allCases<UnitKind>()) if (kinds.count(k)) out.push_back(k);
    return out;
}

/// Two standings added up (the enemies' together).
Standing operator+(const Standing& a, const Standing& b) {
    Standing s = a;
    s.army += b.army;
    s.workers += b.workers;
    s.ore += b.ore;
    s.hydrogen += b.hydrogen;
    s.supplyUsed += b.supplyUsed;
    s.supplyCap += b.supplyCap;
    s.buildings += b.buildings;
    s.mined += b.mined;
    return s;
}

/// How player `p` stands: its army's list price (every unit but Prospectors),
/// its Prospectors, bank, supply, finished buildings and ore mined.
Standing GameState::standing(int64_t p) const {
    Standing s;
    if (p < 0 || p >= static_cast<int64_t>(players.size())) return s;
    for (const auto& u : units) {
        if (!(u.owner == p && u.hp > 0)) continue;
        if (u.kind == UnitKind::prospector) s.workers += 1; else s.army += Rules::cost(u.kind) + Rules::hydrogenCost(u.kind);
    }
    int64_t built = 0;
    for (const auto& b : structures) if (b.owner == p && b.complete() && b.hp > 0) built += 1;
    s.buildings = built;
    const Player& pl = players[static_cast<size_t>(p)];
    s.ore = pl.ore;
    s.hydrogen = pl.hydrogen;
    s.supplyUsed = pl.supplyUsed;
    s.supplyCap = pl.supplyCap;
    s.mined = pl.totalMined;
    return s;
}


// MARK: - The catalogue

namespace {

constexpr double inf = std::numeric_limits<double>::infinity();

} // namespace

/// The doc's table, one row per pick: its kind, title, effect text and
/// what it does. A kind's rows run in level order, level 2's two first:
/// the level and the slot come from the row's place.
const std::vector<PerkRow>& perkRows() {
    static const std::vector<PerkRow> table = {
        {Perk::prospectorQuickDrill, UnitKind::prospector, "Quick drill", "Nearby Prospectors drill 10% faster.", {Effect::Stat{Stat::Drill{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::prospectorLightFrame, UnitKind::prospector, "Light frame", "Every Prospector walks 50% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.5}}}},
        {Perk::prospectorRigMaster, UnitKind::prospector, "Rig master", "Every Prospector pumps MH 20% faster.", {Effect::Stat{Stat::Hydrogen{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::prospectorPlating, UnitKind::prospector, "Plating", "+20 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{20.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::prospectorBigHaul, UnitKind::prospector, "Big haul", "Carries 35 ore or 28 MH a trip; nearby Prospectors carry 7 or 6.", {Effect::Stat{Stat::OreCarry{}, Scope::Driven{}, Change::AtLeast{35.0}}, Effect::Stat{Stat::HydrogenCarry{}, Scope::Driven{}, Change::AtLeast{28.0}}, Effect::Stat{Stat::OreCarry{}, Scope::Nearby{}, Change::AtLeast{7.0}}, Effect::Stat{Stat::HydrogenCarry{}, Scope::Nearby{}, Change::AtLeast{6.0}}}},
        {Perk::prospectorFastWelder, UnitKind::prospector, "Fast welder", "Builds and repairs at ×3, up from ×2.", {Effect::Stat{Stat::Build{}, Scope::Driven{}, Change::Times{3.0}}, Effect::Stat{Stat::Repair{}, Scope::Driven{}, Change::Times{3.0}}}},
        {Perk::prospectorFieldWelder, UnitKind::prospector, "Field welder", "Repairs machines: a full repair takes half their training time.", {Effect::MachineRepair{0.5}}},
        {Perk::prospectorGroupRepair, UnitKind::prospector, "Group repair", "Friendly machines within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::machines}}}},
        {Perk::prospectorForeman, UnitKind::prospector, "Foreman", "Nearby Prospectors drill 50% faster.", {Effect::Stat{Stat::Drill{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::prospectorSiteBoss, UnitKind::prospector, "Site boss", "Every Prospector builds 30% faster.", {Effect::Stat{Stat::Build{}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::prospectorRichVein, UnitKind::prospector, "Rich vein", "Every third ore trip carries double.", {Effect::EveryThird{Action::oreTrip}}},
        {Perk::prospectorCuttingTorch, UnitKind::prospector, "Cutting torch", "Its weapon does double damage.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{1.0}}}},
        {Perk::prospectorOvertime, UnitKind::prospector, "Overtime", "Every Prospector drills 10% faster.", {Effect::Stat{Stat::Drill{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::prospectorPrefab, UnitKind::prospector, "Prefab", "Buildings it places cost 10% less.", {Effect::Stat{Stat::BuildingCost{}, Scope::Driven{}, Change::Percent{-0.1}}}},
        {Perk::prospectorCliffHop, UnitKind::prospector, "Cliff hop", "Jumps up and down cliffs like a Comet.", {Effect::CliffJump{}}},
        {Perk::prospectorSupplyChief, UnitKind::prospector, "Supply chief", "The team's Hab Domes give 10 supply, up from 8.", {Effect::Stat{Stat::SupplyPerHabDome{}, Scope::Team{}, Change::AtLeast{10.0}}}},
        {Perk::prospectorUnionCrew, UnitKind::prospector, "Union crew", "Nearby Prospectors carry 10 ore or 8 MH, up from 5 and 4.", {Effect::Stat{Stat::OreCarry{}, Scope::Nearby{}, Change::AtLeast{10.0}}, Effect::Stat{Stat::HydrogenCarry{}, Scope::Nearby{}, Change::AtLeast{8.0}}}},
        {Perk::prospectorShield, UnitKind::prospector, "Shield", "A 20 hp shield takes hits first; it refills 2 a second after 7 s without a hit.", {Effect::Shield{Shield{20.0, 7.0, 10.0}}}},

        // "The driven one +40%, and nearby +15%": nearby includes the
        // driven one, so its own row is the other 25%.
        {Perk::rangerDrillSergeant, UnitKind::ranger, "Drill sergeant", "Nearby Rangers fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::rangerLightKit, UnitKind::ranger, "Light kit", "Every Ranger walks 20% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::rangerCombatDrill, UnitKind::ranger, "Combat drill", "Every Ranger gets +10 hp.", {Effect::Stat{Stat::Hp{}, Scope::Every{}, Change::Plus{10.0}}}},
        {Perk::rangerFlakVest, UnitKind::ranger, "Flak vest", "+20 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{20.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::rangerHollowPoints, UnitKind::ranger, "Hollow points", "+40% damage, and nearby Rangers +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::rangerLongBarrel, UnitKind::ranger, "Long barrel", "+2 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{2.0}}}},
        {Perk::rangerSurge, UnitKind::ranger, "Surge", "The ability key gives 8 s of +50% fire rate and speed, for 10 hp, every 15 s.", {Effect::Burst{Burst{8.0, 15.0, 0.5, 0.5, 10.0}}}},
        {Perk::rangerFieldMedic, UnitKind::ranger, "Field medic", "Friendly bio units within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::bio}}}},
        {Perk::rangerSquadLeader, UnitKind::ranger, "Squad leader", "Nearby Rangers fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::rangerQuartermaster, UnitKind::ranger, "Quartermaster", "The team's Garrisons train Rangers 30% faster.", {Effect::Stat{Stat::Training{UnitKind::ranger}, Scope::Team{}, Change::Percent{0.3}}}},
        {Perk::rangerBurstFire, UnitKind::ranger, "Burst fire", "Every third shot does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::rangerArmorPiercing, UnitKind::ranger, "Armor-piercing", "Shots ignore armor and do +5 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{5.0}}}},
        {Perk::rangerEspritDeCorps, UnitKind::ranger, "Esprit de corps", "Every Ranger fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::rangerRecruitment, UnitKind::ranger, "Recruitment", "Rangers cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::ranger}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::rangerJumpPack, UnitKind::ranger, "Jump pack", "Jumps up and down cliffs like a Comet.", {Effect::CliffJump{}}},
        {Perk::rangerBastionDrill, UnitKind::ranger, "Bastion drill", "The team's Bastions hold 6, up from 4.", {Effect::Stat{Stat::BastionSize{}, Scope::Team{}, Change::AtLeast{6.0}}}},
        {Perk::rangerBandOfBrothers, UnitKind::ranger, "Band of brothers", "Nearby Rangers do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::rangerShield, UnitKind::ranger, "Shield", "A 20 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{20.0, 7.0, 10.0}}}},

        {Perk::cometPackTactics, UnitKind::comet, "Pack tactics", "Nearby Comets fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::cometFleetFoot, UnitKind::comet, "Fleet foot", "Every Comet runs 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::cometNanomeds, UnitKind::comet, "Nanomeds", "Every Comet heals out of combat 20% faster.", {Effect::Stat{Stat::Regen{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::cometPaddedSuit, UnitKind::comet, "Padded suit", "+25 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{25.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::cometTwinMagnums, UnitKind::comet, "Twin magnums", "+40% damage, and nearby Comets +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::cometLongJump, UnitKind::comet, "Long jump", "Jumps reach 50% farther.", {Effect::Stat{Stat::JumpReach{}, Scope::Driven{}, Change::Percent{0.5}}}},
        {Perk::cometPulseMine, UnitKind::comet, "Pulse mine", "The ability key throws a grenade at the crosshair: 20 damage within 1.5 cells, every 14 s.", {Effect::Grenade{Grenade{20.0, 1.5, 14.0}}}},
        {Perk::cometSpotter, UnitKind::comet, "Spotter", "Sees 50% farther, for the whole team.", {Effect::Stat{Stat::Sight{}, Scope::Driven{}, Change::Percent{0.5}}}},
        {Perk::cometRaidParty, UnitKind::comet, "Raid party", "Nearby Comets fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::cometDemolition, UnitKind::comet, "Demolition", "Every Comet does 30% more damage to buildings.", {Effect::Stat{Stat::DamageVs{TargetClass::building}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::cometDoubleTap, UnitKind::comet, "Double tap", "Every third volley does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::cometLightKiller, UnitKind::comet, "Light killer", "+5 more against light, so +10 in all.", {Effect::Stat{Stat::DamageVs{TargetClass::light}, Scope::Driven{}, Change::Plus{5.0}}}},
        {Perk::cometHitAndRun, UnitKind::comet, "Hit and run", "Every Comet fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::cometCheapKit, UnitKind::comet, "Cheap kit", "Comets cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::comet}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::cometBooster, UnitKind::comet, "Booster", "3 s at +50% speed after each jump.", {Effect::AfterJump{Rush{3.0, 0.5, 0.0}}}},
        {Perk::cometBounty, UnitKind::comet, "Bounty", "Each of its kills gives the team 10% of the victim's cost.", {Effect::Stat{Stat::Bounty{}, Scope::Driven{}, Change::Plus{0.1}}}},
        {Perk::cometDeathSquad, UnitKind::comet, "Death squad", "Nearby Comets do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::cometShield, UnitKind::comet, "Shield", "A 25 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{25.0, 7.0, 10.0}}}},

        {Perk::fireflyHotBurners, UnitKind::firefly, "Hot burners", "Nearby Fireflies fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::fireflyTurbo, UnitKind::firefly, "Turbo", "Every Firefly drives 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::fireflyFuelLine, UnitKind::firefly, "Fuel line", "Every Firefly's flame reaches 20% farther.", {Effect::Stat{Stat::FlameReach{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::fireflyArmorPlates, UnitKind::firefly, "Armor plates", "+40 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{40.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::fireflyNapalm, UnitKind::firefly, "Napalm", "+40% damage, and nearby Fireflies +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::fireflyWideNozzle, UnitKind::firefly, "Wide nozzle", "Its flame is 50% wider.", {Effect::Stat{Stat::FlameWidth{}, Scope::Driven{}, Change::Percent{0.5}}}},
        {Perk::fireflyBurn, UnitKind::firefly, "Burn", "What its flame hits burns for 3 s, 4 damage a second.", {Effect::Burn{4.0, 3.0}}},
        {Perk::fireflyMechanic, UnitKind::firefly, "Mechanic", "Friendly machines within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::machines}}}},
        {Perk::fireflyPack, UnitKind::firefly, "Pack", "Nearby Fireflies fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::fireflySpread, UnitKind::firefly, "Spread", "Every Firefly's flame is 30% wider.", {Effect::Stat{Stat::FlameWidth{}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::fireflyFlashpoint, UnitKind::firefly, "Flashpoint", "Every third flame does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::fireflyHotCore, UnitKind::firefly, "Hot core", "Its flame ignores armor and does +8 against bio.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::bio}, Scope::Driven{}, Change::Plus{8.0}}}},
        {Perk::fireflyOverdrive, UnitKind::firefly, "Overdrive", "Every Firefly fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::fireflyCheapChassis, UnitKind::firefly, "Cheap chassis", "Fireflies cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::firefly}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::fireflyBoost, UnitKind::firefly, "Boost", "The ability key gives 2 s at double speed, every 15 s.", {Effect::Burst{Burst{2.0, 15.0, 1.0, 0.0, 0.0}}}},
        {Perk::fireflyScrap, UnitKind::firefly, "Scrap", "Every Firefly the team loses refunds 25% of its cost.", {Effect::Stat{Stat::Refund{UnitKind::firefly}, Scope::Team{}, Change::Plus{0.25}}}},
        {Perk::fireflyFirestorm, UnitKind::firefly, "Firestorm", "Nearby Fireflies do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::fireflyShield, UnitKind::firefly, "Shield", "A 40 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{40.0, 7.0, 10.0}}}},

        {Perk::juggernautShellDrill, UnitKind::juggernaut, "Shell drill", "Nearby Juggernauts fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::juggernautLongStride, UnitKind::juggernaut, "Long stride", "Every Juggernaut walks 20% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::juggernautShockRounds, UnitKind::juggernaut, "Shock rounds", "Every Juggernaut's slow lasts 20% longer.", {Effect::Stat{Stat::SlowTime{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::juggernautHeavyPlate, UnitKind::juggernaut, "Heavy plate", "+55 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{55.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::juggernautBreacher, UnitKind::juggernaut, "Breacher", "+40% damage, and nearby Juggernauts +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::juggernautDeepSlow, UnitKind::juggernaut, "Deep slow", "Its slow drops a target to a third of its speed, not half.", {Effect::Stat{Stat::Slow{}, Scope::Driven{}, Change::AtLeast{2.0 / 3}}}},
        {Perk::juggernautSurge, UnitKind::juggernaut, "Surge", "The ability key gives 8 s of +50% fire rate and speed, for 20 hp, every 15 s.", {Effect::Burst{Burst{8.0, 15.0, 0.5, 0.5, 20.0}}}},
        {Perk::juggernautBulwark, UnitKind::juggernaut, "Bulwark", "Friendly units within 6 cells take 10% less damage.", {Effect::Stat{Stat::DamageTaken{}, Scope::Around{Filter::all}, Change::Percent{-0.1}}}},
        {Perk::juggernautSiegeBreakers, UnitKind::juggernaut, "Siege breakers", "Nearby Juggernauts fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::juggernautBehemoth, UnitKind::juggernaut, "Behemoth", "Every Juggernaut gets +40 hp.", {Effect::Stat{Stat::Hp{}, Scope::Every{}, Change::Plus{40.0}}}},
        {Perk::juggernautHeavyVolley, UnitKind::juggernaut, "Heavy volley", "Every third grenade does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::juggernautFragment, UnitKind::juggernaut, "Fragment", "Its grenades splash 1 cell around for half damage.", {Effect::Fragment{1.0, 0.5}}},
        {Perk::juggernautShockTroops, UnitKind::juggernaut, "Shock troops", "Every Juggernaut fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::juggernautFieldKit, UnitKind::juggernaut, "Field kit", "Juggernauts cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::juggernaut}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::juggernautJumpPack, UnitKind::juggernaut, "Jump pack", "Jumps up and down cliffs like a Comet.", {Effect::CliffJump{}}},
        // "Finish 25% sooner": a time of 3/4, so a rate of 4/3.
        {Perk::juggernautFieldResearch, UnitKind::juggernaut, "Field research", "Garrison upgrades cost 25% less and finish 25% sooner.", {Effect::Stat{Stat::UpgradeCost{StructureKind::garrison}, Scope::Team{}, Change::Percent{-0.25}}, Effect::Stat{Stat::Research{StructureKind::garrison}, Scope::Team{}, Change::Percent{1.0 / 3}}}},
        {Perk::juggernautWreckingCrew, UnitKind::juggernaut, "Wrecking crew", "Nearby Juggernauts do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::juggernautShield, UnitKind::juggernaut, "Shield", "A 55 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{55.0, 7.0, 10.0}}}},

        {Perk::dropshipTriage, UnitKind::dropship, "Triage", "Nearby Dropships heal 10% faster.", {Effect::Stat{Stat::Heal{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::dropshipBoosters, UnitKind::dropship, "Boosters", "Every Dropship flies 20% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::dropshipLifeline, UnitKind::dropship, "Lifeline", "Every Dropship regains energy 20% faster.", {Effect::Stat{Stat::Energy{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::dropshipArmorPlates, UnitKind::dropship, "Armor plates", "+65 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{65.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::dropshipSurgeon, UnitKind::dropship, "Surgeon", "Heals 40% faster, and nearby Dropships 15% faster.", {Effect::Stat{Stat::Heal{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Heal{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::dropshipCargoBay, UnitKind::dropship, "Cargo bay", "Carries 12 slots, up from 8.", {Effect::Stat{Stat::Cargo{}, Scope::Driven{}, Change::AtLeast{12.0}}}},
        {Perk::dropshipNaniteBeam, UnitKind::dropship, "Nanite beam", "Heals machines too, at the same rate.", {Effect::NaniteBeam{}}},
        {Perk::dropshipHealingField, UnitKind::dropship, "Healing field", "Friendly bio units within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::bio}}}},
        {Perk::dropshipMedicalTeam, UnitKind::dropship, "Medical team", "Nearby Dropships heal 25% faster.", {Effect::Stat{Stat::Heal{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        // "30% less energy": an energy point heals 10/7 as much.
        {Perk::dropshipEfficient, UnitKind::dropship, "Efficient", "Every Dropship spends 30% less energy healing.", {Effect::Stat{Stat::HealEnergy{}, Scope::Every{}, Change::Times{10.0 / 7}}}},
        {Perk::dropshipDoubleBeam, UnitKind::dropship, "Double beam", "Heals two units at once.", {Effect::DoubleBeam{}}},
        {Perk::dropshipCombatDrop, UnitKind::dropship, "Combat drop", "Units it drops get 4 s of +30% speed and fire rate.", {Effect::DropRush{Rush{4.0, 0.3, 0.3}}}},
        {Perk::dropshipFieldHospital, UnitKind::dropship, "Field hospital", "Every Dropship heals 10% faster.", {Effect::Stat{Stat::Heal{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::dropshipCheapHull, UnitKind::dropship, "Cheap hull", "Dropships cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::dropship}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::dropshipCruise, UnitKind::dropship, "Cruise", "Flies 50% faster while not healing.", {Effect::Cruise{0.5}}},
        {Perk::dropshipSpacedockRefit, UnitKind::dropship, "Spacedock refit", "The team's Spacedocks build Dropships 25% faster.", {Effect::Stat{Stat::Production{StructureKind::spacedock}, Scope::Team{}, Change::Percent{0.25}}}},
        {Perk::dropshipMedicalCorps, UnitKind::dropship, "Medical corps", "Nearby Dropships heal 50% faster.", {Effect::Stat{Stat::Heal{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::dropshipShield, UnitKind::dropship, "Shield", "A 65 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{65.0, 7.0, 10.0}}}},

        {Perk::longbowSpotter, UnitKind::longbow, "Spotter", "Nearby Longbows fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::longbowTreads, UnitKind::longbow, "Treads", "Every Longbow drives 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::longbowQuickAnchor, UnitKind::longbow, "Quick anchor", "Every Longbow anchors and unanchors 20% faster.", {Effect::Stat{Stat::Anchor{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::longbowReactiveArmor, UnitKind::longbow, "Reactive armor", "+80 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{80.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::longbowShapedCharge, UnitKind::longbow, "Shaped charge", "+40% damage, and nearby Longbows +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::longbowLongGun, UnitKind::longbow, "Long gun", "+2 range anchored.", {Effect::Stat{Stat::AnchorRange{}, Scope::Driven{}, Change::Plus{2.0}}}},
        {Perk::longbowCrewMechanic, UnitKind::longbow, "Crew mechanic", "Mends itself 3 hp a second after 5 s without a hit.", {Effect::SelfMend{3.0, 5.0}}},
        {Perk::longbowMechanic, UnitKind::longbow, "Mechanic", "Friendly machines within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::machines}}}},
        {Perk::longbowBattery, UnitKind::longbow, "Battery", "Nearby Longbows fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::longbowHeavyShells, UnitKind::longbow, "Heavy shells", "Every Longbow's splash reaches 30% wider.", {Effect::Stat{Stat::Splash{}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::longbowDoubleShot, UnitKind::longbow, "Double shot", "Every third shell does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::longbowAPShells, UnitKind::longbow, "AP shells", "Its shells ignore armor and do +10 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{10.0}}}},
        {Perk::longbowDrilledCrews, UnitKind::longbow, "Drilled crews", "Every Longbow fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::longbowCheapHull, UnitKind::longbow, "Cheap hull", "Longbows cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::longbow}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::longbowHullDown, UnitKind::longbow, "Hull down", "Anchors and unanchors at once.", {Effect::Stat{Stat::Anchor{}, Scope::Driven{}, Change::Times{inf}}}},
        {Perk::longbowHeavyIndustry, UnitKind::longbow, "Heavy industry", "The team's Foundries build 20% faster.", {Effect::Stat{Stat::Production{StructureKind::foundry}, Scope::Team{}, Change::Percent{0.2}}}},
        {Perk::longbowArtilleryPark, UnitKind::longbow, "Artillery park", "Nearby Longbows do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::longbowShield, UnitKind::longbow, "Shield", "An 80 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{80.0, 7.0, 10.0}}}},

        {Perk::kestrelWingman, UnitKind::kestrel, "Wingman", "Nearby Kestrels fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::kestrelAfterburners, UnitKind::kestrel, "Afterburners", "Every Kestrel flies 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::kestrelKeenOptics, UnitKind::kestrel, "Keen optics", "Every Kestrel sees 20% farther.", {Effect::Stat{Stat::Sight{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::kestrelArmorPlates, UnitKind::kestrel, "Armor plates", "+60 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{60.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::kestrelHellfire, UnitKind::kestrel, "Hellfire", "+40% damage, and nearby Kestrels +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::kestrelLongRails, UnitKind::kestrel, "Long rails", "+1 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::kestrelFieldRepairs, UnitKind::kestrel, "Field repairs", "Mends itself 3 hp a second after 5 s without a hit.", {Effect::SelfMend{3.0, 5.0}}},
        {Perk::kestrelMechanic, UnitKind::kestrel, "Mechanic", "Friendly machines within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::machines}}}},
        {Perk::kestrelSquadron, UnitKind::kestrel, "Squadron", "Nearby Kestrels fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::kestrelWorkerHunter, UnitKind::kestrel, "Worker hunter", "Every Kestrel does 30% more damage to light units.", {Effect::Stat{Stat::DamageVs{TargetClass::light}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::kestrelDoubleSalvo, UnitKind::kestrel, "Double salvo", "Every third salvo does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::kestrelAPRockets, UnitKind::kestrel, "AP rockets", "Its rockets ignore armor and do +4 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{4.0}}}},
        {Perk::kestrelFlightDrill, UnitKind::kestrel, "Flight drill", "Every Kestrel fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::kestrelCheapAirframe, UnitKind::kestrel, "Cheap airframe", "Kestrels cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::kestrel}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::kestrelAfterburn, UnitKind::kestrel, "Afterburn", "The ability key gives 2 s at double speed, every 15 s.", {Effect::Burst{Burst{2.0, 15.0, 1.0, 0.0, 0.0}}}},
        {Perk::kestrelBountyHunter, UnitKind::kestrel, "Bounty hunter", "Each of its kills gives the team 10% of the victim's cost.", {Effect::Stat{Stat::Bounty{}, Scope::Driven{}, Change::Plus{0.1}}}},
        {Perk::kestrelStrikeWing, UnitKind::kestrel, "Strike wing", "Nearby Kestrels do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::kestrelShield, UnitKind::kestrel, "Shield", "A 60 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{60.0, 7.0, 10.0}}}},

        {Perk::hailstormLoaders, UnitKind::hailstorm, "Loaders", "Nearby Hailstorms fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::hailstormTracks, UnitKind::hailstorm, "Tracks", "Every Hailstorm drives 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::hailstormWideBurst, UnitKind::hailstorm, "Wide burst", "Every Hailstorm's flak splashes 20% wider.", {Effect::Stat{Stat::Splash{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::hailstormArmorPlates, UnitKind::hailstorm, "Armor plates", "+60 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{60.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::hailstormProximityFuse, UnitKind::hailstorm, "Proximity fuse", "+40% damage, and nearby Hailstorms +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::hailstormLongBarrels, UnitKind::hailstorm, "Long barrels", "+1 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::hailstormCrewMechanic, UnitKind::hailstorm, "Crew mechanic", "Mends itself 3 hp a second after 5 s without a hit.", {Effect::SelfMend{3.0, 5.0}}},
        {Perk::hailstormMechanic, UnitKind::hailstorm, "Mechanic", "Friendly machines within 6 cells mend 1 hp a second.", {Effect::Aura{Aura{1.0, Leveling::radius, Filter::machines}}}},
        {Perk::hailstormFlakBattery, UnitKind::hailstorm, "Flak battery", "Nearby Hailstorms fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::hailstormSkyWatch, UnitKind::hailstorm, "Sky watch", "Every Hailstorm sees 30% farther.", {Effect::Stat{Stat::Sight{}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::hailstormDoubleBurst, UnitKind::hailstorm, "Double burst", "Every third burst does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::hailstormAPFlak, UnitKind::hailstorm, "AP flak", "Its flak ignores armor and does +4 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{4.0}}}},
        {Perk::hailstormDrilledGunners, UnitKind::hailstorm, "Drilled gunners", "Every Hailstorm fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::hailstormCheapChassis, UnitKind::hailstorm, "Cheap chassis", "Hailstorms cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::hailstorm}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::hailstormWalkerLegs, UnitKind::hailstorm, "Walker legs", "Climbs up and down cliffs like a Comet.", {Effect::CliffJump{}}},
        {Perk::hailstormHeavyIndustry, UnitKind::hailstorm, "Heavy industry", "The team's Foundries build 20% faster.", {Effect::Stat{Stat::Production{StructureKind::foundry}, Scope::Team{}, Change::Percent{0.2}}}},
        {Perk::hailstormFlakWall, UnitKind::hailstorm, "Flak wall", "Nearby Hailstorms do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::hailstormShield, UnitKind::hailstorm, "Shield", "A 60 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{60.0, 7.0, 10.0}}}},

        {Perk::peregrineWingmen, UnitKind::peregrine, "Wingmen", "Nearby Peregrines fire 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.1}}}},
        {Perk::peregrineThrusters, UnitKind::peregrine, "Thrusters", "Every Peregrine flies 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::peregrineLongRangeRadar, UnitKind::peregrine, "Long-range radar", "Every Peregrine sees 20% farther.", {Effect::Stat{Stat::Sight{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::peregrineArmorPlates, UnitKind::peregrine, "Armor plates", "+40 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{40.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::peregrineSeekerHeads, UnitKind::peregrine, "Seeker heads", "+25% damage, and nearby Peregrines +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::peregrineLongRails, UnitKind::peregrine, "Long rails", "+1 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::peregrineFieldRepairs, UnitKind::peregrine, "Field repairs", "Mends itself 3 hp a second after 5 s without a hit.", {Effect::SelfMend{3.0, 5.0}}},
        {Perk::peregrineEscort, UnitKind::peregrine, "Escort", "Friendly flyers within 6 cells take 10% less damage.", {Effect::Stat{Stat::DamageTaken{}, Scope::Around{Filter::air}, Change::Percent{-0.1}}}},
        {Perk::peregrinePackHunters, UnitKind::peregrine, "Pack hunters", "Nearby Peregrines fire 25% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Nearby{}, Change::Percent{0.25}}}},
        {Perk::peregrineGunshipKiller, UnitKind::peregrine, "Gunship killer", "Every Peregrine does 30% more damage to armored units.", {Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::peregrineDoubleVolley, UnitKind::peregrine, "Double volley", "Every third volley does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::peregrineAPMissiles, UnitKind::peregrine, "AP missiles", "Its missiles ignore armor and do +4 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{4.0}}}},
        {Perk::peregrineFlightSchool, UnitKind::peregrine, "Flight school", "Every Peregrine fires 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::peregrineCheapAirframe, UnitKind::peregrine, "Cheap airframe", "Peregrines cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::peregrine}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::peregrineAfterburn, UnitKind::peregrine, "Afterburn", "The ability key gives 2 s at double speed, every 15 s.", {Effect::Burst{Burst{2.0, 15.0, 1.0, 0.0, 0.0}}}},
        {Perk::peregrineBountyHunter, UnitKind::peregrine, "Bounty hunter", "Each of its kills gives the team 10% of the victim's cost.", {Effect::Stat{Stat::Bounty{}, Scope::Driven{}, Change::Plus{0.1}}}},
        {Perk::peregrineAirSuperiority, UnitKind::peregrine, "Air superiority", "Nearby Peregrines do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::peregrineShield, UnitKind::peregrine, "Shield", "A 40 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{40.0, 7.0, 10.0}}}},

        {Perk::atlasAutoloader, UnitKind::atlas, "Autoloader", "Fires 15% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Driven{}, Change::Percent{0.15}}}},
        {Perk::atlasServoLegs, UnitKind::atlas, "Servo legs", "Every Atlas walks 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::atlasRangefinder, UnitKind::atlas, "Rangefinder", "Every Atlas sees 20% farther.", {Effect::Stat{Stat::Sight{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::atlasArmorPlates, UnitKind::atlas, "Armor plates", "+100 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{100.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::atlasHeavyShells, UnitKind::atlas, "Heavy shells", "+25% damage.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}}},
        {Perk::atlasLongBarrels, UnitKind::atlas, "Long barrels", "+1 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::atlasCrewMechanic, UnitKind::atlas, "Crew mechanic", "Mends itself 5 hp a second after 5 s without a hit.", {Effect::SelfMend{5.0, 5.0}}},
        {Perk::atlasMechanic, UnitKind::atlas, "Mechanic", "Friendly machines within 6 cells mend 2 hp a second.", {Effect::Aura{Aura{2.0, Leveling::radius, Filter::machines}}}},
        {Perk::atlasBulwark, UnitKind::atlas, "Bulwark", "Friendly units within 6 cells take 10% less damage.", {Effect::Stat{Stat::DamageTaken{}, Scope::Around{Filter::all}, Change::Percent{-0.1}}}},
        {Perk::atlasWideShells, UnitKind::atlas, "Wide shells", "Every Atlas's splash reaches 25% wider.", {Effect::Stat{Stat::Splash{}, Scope::Every{}, Change::Percent{0.25}}}},
        {Perk::atlasDoubleVolley, UnitKind::atlas, "Double volley", "Every third volley does double damage.", {Effect::EveryThird{Action::attack}}},
        {Perk::atlasAPShells, UnitKind::atlas, "AP shells", "Its shells ignore armor and do +6 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{6.0}}}},
        {Perk::atlasAftershock, UnitKind::atlas, "Aftershock", "Its Quake stomp does 50% more damage and reaches 1 cell farther.", {Effect::Stat{Stat::Stomp{}, Scope::Driven{}, Change::Percent{0.5}}, Effect::Stat{Stat::Stomp{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::atlasCheapFrame, UnitKind::atlas, "Cheap frame", "Atlases cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::atlas}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::atlasHeavyIndustry, UnitKind::atlas, "Heavy industry", "The team's Foundries build 20% faster.", {Effect::Stat{Stat::Production{StructureKind::foundry}, Scope::Team{}, Change::Percent{0.2}}}},
        {Perk::atlasFlakMount, UnitKind::atlas, "Flak mount", "Its cannons also fire at flyers, at half damage.", {Effect::HitsAir{0.5}}},
        {Perk::atlasWarMarch, UnitKind::atlas, "War march", "Friendly units within 6 cells move 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Around{Filter::all}, Change::Percent{0.15}}}},
        {Perk::atlasShield, UnitKind::atlas, "Shield", "A 150 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{150.0, 7.0, 10.0}}}},

        {Perk::scorpionQuickDig, UnitKind::scorpion, "Quick dig", "Every Scorpion buries and digs out 50% faster.", {Effect::Stat{Stat::BuryTime{}, Scope::Every{}, Change::Percent{0.5}}}},
        {Perk::scorpionLightFrame, UnitKind::scorpion, "Light frame", "Every Scorpion crawls 15% faster.", {Effect::Stat{Stat::Speed{}, Scope::Every{}, Change::Percent{0.15}}}},
        {Perk::scorpionSeismicSense, UnitKind::scorpion, "Seismic sense", "Every Scorpion sees 20% farther.", {Effect::Stat{Stat::Sight{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::scorpionArmorPlates, UnitKind::scorpion, "Armor plates", "+40 hp and +1 armor.", {Effect::Stat{Stat::Hp{}, Scope::Driven{}, Change::Plus{40.0}}, Effect::Stat{Stat::Armor{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::scorpionHeavyCharge, UnitKind::scorpion, "Heavy charge", "+25% damage, and nearby Scorpions +15%.", {Effect::Stat{Stat::Damage{}, Scope::Driven{}, Change::Percent{0.25}}, Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.15}}}},
        {Perk::scorpionLongTail, UnitKind::scorpion, "Long tail", "+1 range.", {Effect::Stat{Stat::Range{}, Scope::Driven{}, Change::Plus{1.0}}}},
        {Perk::scorpionFieldRepairs, UnitKind::scorpion, "Field repairs", "Mends itself 3 hp a second after 5 s without a hit.", {Effect::SelfMend{3.0, 5.0}}},
        {Perk::scorpionDeepBurrow, UnitKind::scorpion, "Deep burrow", "Buried, enemies see it only within 1 cell.", {Effect::Stat{Stat::RevealRange{}, Scope::Driven{}, Change::Plus{-1.0}}}},
        {Perk::scorpionQuickReload, UnitKind::scorpion, "Quick reload", "Every Scorpion reloads 20% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.2}}}},
        {Perk::scorpionWorkerHunter, UnitKind::scorpion, "Worker hunter", "Every Scorpion does 30% more damage to light units.", {Effect::Stat{Stat::DamageVs{TargetClass::light}, Scope::Every{}, Change::Percent{0.3}}}},
        {Perk::scorpionShapedCharge, UnitKind::scorpion, "Shaped charge", "Its sting ignores armor and does +10 against armored.", {Effect::Stat{Stat::IgnoresArmor{}, Scope::Driven{}, Change::Plus{1.0}}, Effect::Stat{Stat::DamageVs{TargetClass::armored}, Scope::Driven{}, Change::Plus{10.0}}}},
        {Perk::scorpionTwinSting, UnitKind::scorpion, "Twin sting", "Its strike hits a second target within 2 cells of the first.", {Effect::SecondTarget{2.0}}},
        {Perk::scorpionMinefieldDrill, UnitKind::scorpion, "Minefield drill", "Every Scorpion reloads 10% faster.", {Effect::Stat{Stat::FireRate{}, Scope::Every{}, Change::Percent{0.1}}}},
        {Perk::scorpionCheapShell, UnitKind::scorpion, "Cheap shell", "Scorpions cost 10% less.", {Effect::Stat{Stat::UnitCost{UnitKind::scorpion}, Scope::Team{}, Change::Percent{-0.1}}}},
        {Perk::scorpionSapper, UnitKind::scorpion, "Sapper", "Its sting also hits buildings, at half damage.", {Effect::HitsBuildings{0.5}}},
        {Perk::scorpionBountyHunter, UnitKind::scorpion, "Bounty hunter", "Each of its kills gives the team 10% of the victim's cost.", {Effect::Stat{Stat::Bounty{}, Scope::Driven{}, Change::Plus{0.1}}}},
        {Perk::scorpionNest, UnitKind::scorpion, "Nest", "Nearby Scorpions do 50% more damage.", {Effect::Stat{Stat::Damage{}, Scope::Nearby{}, Change::Percent{0.5}}}},
        {Perk::scorpionShield, UnitKind::scorpion, "Shield", "A 40 hp shield takes hits first; it refills over 10 s after 7 s without a hit.", {Effect::Shield{Shield{40.0, 7.0, 10.0}}}},
    };
    return table;
}

namespace {

/// Each pick's row, indexed by the perk.
const std::vector<const PerkRow*>& byPerk() {
    static const std::vector<const PerkRow*> out = [] {
        std::vector<const PerkRow*> b(EnumInfo<Perk>::names.size());
        for (const PerkRow& r : perkRows()) b[static_cast<size_t>(r.perk)] = &r;
        return b;
    }();
    return out;
}

/// `Perk.catalogue`, indexed by the perk.
const std::vector<PerkInfo>& catalogue() {
    static const std::vector<PerkInfo> out = [] {
        std::vector<PerkInfo> c(EnumInfo<Perk>::names.size());
        std::map<UnitKind, int64_t> seen;
        for (const PerkRow& r : perkRows()) {
            const int64_t n = seen[r.kind];
            seen[r.kind] = n + 1;
            c[static_cast<size_t>(r.perk)] = PerkInfo{r.perk, r.kind, 2 + n / 2, n % 2 == 0 ? PerkSlot::one : PerkSlot::other,
                                                      r.title, r.text};
        }
        return c;
    }();
    return out;
}

/// `Perk.offers`: each kind's pairs, level 2's first.
const std::map<UnitKind, std::vector<std::pair<Perk, Perk>>>& offers() {
    static const std::map<UnitKind, std::vector<std::pair<Perk, Perk>>> out = [] {
        std::map<UnitKind, std::vector<std::pair<Perk, Perk>>> o;
        for (UnitKind kind : allCases<UnitKind>()) {
            std::vector<Perk> mine;
            for (const PerkRow& r : perkRows()) if (r.kind == kind) mine.push_back(r.perk);
            std::vector<std::pair<Perk, Perk>> pairs;
            for (int64_t i = 0; i < static_cast<int64_t>(mine.size()) - 1; i += 2)
                pairs.emplace_back(mine[static_cast<size_t>(i)], mine[static_cast<size_t>(i + 1)]);
            o[kind] = pairs;
        }
        return o;
    }();
    return out;
}

} // namespace

const PerkInfo& info(Perk p) { return catalogue()[static_cast<size_t>(p)]; }
UnitKind kind(Perk p) { return info(p).kind; }
int64_t level(Perk p) { return info(p).level; }
const std::string& title(Perk p) { return info(p).title; }
const std::string& effect(Perk p) { return info(p).effect; }
/// What it does (its row's effects).
const std::vector<Effect>& effects(Perk p) { return byPerk()[static_cast<size_t>(p)]->effects; }

/// The two picks a kind offers at `level` (2…10), the same every game.
std::optional<std::pair<Perk, Perk>> perkOffer(UnitKind kind, int64_t level) {
    auto it = offers().find(kind);
    if (it == offers().end()) return std::nullopt;
    const auto& pair = it->second;
    if (level - 2 < 0 || level - 2 >= static_cast<int64_t>(pair.size())) return std::nullopt;
    return pair[static_cast<size_t>(level - 2)];
}

// MARK: - The balance

/// The balance as text: every pick (its info and what it does), the
/// XP curve and the numbers above, and `Rules.hero*`.
std::string Leveling::balance() {
    std::string text;
    for (Perk p : allCases<Perk>()) {
        const PerkInfo& i = info(p);
        text += std::string(rawValue(p)) + "|" + std::string(rawValue(i.kind)) + "|" + std::to_string(i.level) + "|" +
                std::string(rawValue(i.slot)) + "|" + i.title + "|" + i.effect + "|" + describe(effects(p)) + "\n";
    }
    for (int64_t l = 2; l <= cap; l++) text += "xp " + std::to_string(l) + " " + swiftDescription(xp(l)) + "\n";
    text += "leveling " + std::to_string(cap) + " " + swiftDescription(radius) + " " + swiftDescription(oreXP) + " " +
            swiftDescription(hydrogenXP) + " " + swiftDescription(buildingXP) + " " + swiftDescription(mendXP) + " " +
            swiftDescription(carryDistance) + " " + swiftDescription(carryXP) + " " + swiftDescription(healing) + "\n";
    text += "hero " + swiftDescription(Rules::heroDamage) + " " + swiftDescription(Rules::heroRange) + " " +
            swiftDescription(Rules::heroSpeed) + " " + std::to_string(Rules::heroCarry) + " " +
            std::to_string(Rules::heroHydrogenCarry) + " " + swiftDescription(Rules::heroWork) + "\n";
    return text;
}

/// A hash of the balance, 16 hex digits: games tracked under one
/// signature were played with the same picks and numbers. FNV-1a, the
/// same on every launch (Swift's `Hasher` is seeded per launch).
const std::string& Leveling::signature() {
    static const std::string s = fnv1a(balance());
    return s;
}

// MARK: - Swift's descriptions

std::string swiftDescription(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v < 0 ? "-inf" : "inf";
    if (v == 0) return std::signbit(v) ? "-0.0" : "0.0";
    // The shortest digits that read back the same.
    char buf[64];
    for (int precision = 0; precision <= 17; precision++) {
        std::snprintf(buf, sizeof buf, "%.*e", precision, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    // buf: [-]d[.ddd]e±XX
    std::string s(buf);
    const size_t e = s.find('e');
    std::string mantissa = s.substr(0, e);
    const int exponent = std::atoi(s.c_str() + e + 1);
    const bool negative = mantissa[0] == '-';
    if (negative) mantissa.erase(0, 1);
    std::string digits;
    for (char c : mantissa) if (c != '.') digits += c;
    std::string out;
    if (exponent < -4 || exponent >= 16) {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        char ex[16];
        std::snprintf(ex, sizeof ex, "e%c%02d", exponent < 0 ? '-' : '+', exponent < 0 ? -exponent : exponent);
        out += ex;
    } else if (exponent < 0) {
        out = "0." + std::string(static_cast<size_t>(-exponent - 1), '0') + digits;
    } else {
        const size_t whole = static_cast<size_t>(exponent) + 1;
        if (digits.size() <= whole) out = digits + std::string(whole - digits.size(), '0') + ".0";
        else out = digits.substr(0, whole) + "." + digits.substr(whole);
    }
    return negative ? "-" + out : out;
}

namespace {

std::string g(std::string_view type, std::string_view name) { return "GameCore." + std::string(type) + "." + std::string(name); }

/// `GameCore.Stat`'s case names, in its order.
constexpr std::array<std::string_view, 42> statNames{
    "speed", "drill", "hydrogen", "oreCarry", "hydrogenCarry", "build", "repair", "hp", "armor", "damage", "fireRate",
    "range", "damageVs", "ignoresArmor", "damageTaken", "buildingCost", "sight", "flameReach", "flameWidth", "splash",
    "anchor", "anchorRange", "slowTime", "slow", "heal", "healEnergy", "energy", "regen", "jumpReach", "cargo", "bounty",
    "training", "production", "unitCost", "upgradeCost", "research", "supplyPerHabDome", "bastionSize", "refund",
    "stomp", "buryTime", "revealRange"};
static_assert(statNames.size() == std::variant_size_v<Stat::Value>);

std::string d(double v) { return swiftDescription(v); }
std::string unitKind(UnitKind k) { return g("Unit.Kind", rawValue(k)); }
std::string structureKind(StructureKind k) { return g("Structure.Kind", rawValue(k)); }
std::string rush(const Rush& r) {
    return "GameCore.Rush(seconds: " + d(r.seconds) + ", speed: " + d(r.speed) + ", fireRate: " + d(r.fireRate) + ")";
}

} // namespace

std::string describe(const Stat& s) {
    std::string out = g("Stat", statNames[s.index()]);
    if (auto v = s.as<Stat::DamageVs>()) out += "(" + g("TargetClass", rawValue(v->targetClass)) + ")";
    else if (auto t = s.as<Stat::Training>()) out += "(" + unitKind(t->kind) + ")";
    else if (auto p = s.as<Stat::Production>()) out += "(" + structureKind(p->kind) + ")";
    else if (auto u = s.as<Stat::UnitCost>()) out += "(" + unitKind(u->kind) + ")";
    else if (auto c = s.as<Stat::UpgradeCost>()) out += "(" + structureKind(c->kind) + ")";
    else if (auto r = s.as<Stat::Research>()) out += "(" + structureKind(r->kind) + ")";
    else if (auto f = s.as<Stat::Refund>()) out += "(" + unitKind(f->kind) + ")";
    return out;
}

std::string describe(const Scope& s) {
    if (s.is<Scope::Driven>()) return g("Scope", "driven");
    if (s.is<Scope::Nearby>()) return g("Scope", "nearby");
    if (s.is<Scope::Every>()) return g("Scope", "every");
    if (auto a = s.as<Scope::Around>()) return g("Scope", "around") + "(" + g("Filter", rawValue(a->filter)) + ")";
    return g("Scope", "team");
}

std::string describe(const Change& c) {
    if (auto p = c.as<Change::Percent>()) return g("Change", "percent") + "(" + d(p->x) + ")";
    if (auto t = c.as<Change::Times>()) return g("Change", "times") + "(" + d(t->x) + ")";
    if (auto a = c.as<Change::AtLeast>()) return g("Change", "atLeast") + "(" + d(a->x) + ")";
    const auto* q = c.as<Change::Plus>();
    return g("Change", "plus") + "(" + d(q ? q->x : 0) + ")";
}

std::string describe(const Effect& e) {
    if (auto s = e.as<Effect::Stat>())
        return g("Effect", "stat") + "(" + describe(s->stat) + ", " + describe(s->scope) + ", " + describe(s->change) + ")";
    if (auto t = e.as<Effect::EveryThird>()) return g("Effect", "everyThird") + "(" + g("Action", rawValue(t->action)) + ")";
    if (auto a = e.as<Effect::Aura>())
        return g("Effect", "aura") + "(GameCore.Aura(hpPerSecond: " + d(a->aura.hpPerSecond) + ", radius: " + d(a->aura.radius) +
               ", filter: " + g("Filter", rawValue(a->aura.filter)) + "))";
    if (auto b = e.as<Effect::Burst>())
        return g("Effect", "burst") + "(GameCore.Burst(seconds: " + d(b->burst.seconds) + ", cooldown: " + d(b->burst.cooldown) +
               ", speed: " + d(b->burst.speed) + ", fireRate: " + d(b->burst.fireRate) + ", hpCost: " + d(b->burst.hpCost) + "))";
    if (auto s = e.as<Effect::Shield>())
        return g("Effect", "shield") + "(GameCore.Shield(hp: " + d(s->shield.hp) + ", delay: " + d(s->shield.delay) +
               ", refill: " + d(s->shield.refill) + "))";
    if (auto m = e.as<Effect::SelfMend>())
        return g("Effect", "selfMend") + "(hpPerSecond: " + d(m->hpPerSecond) + ", delay: " + d(m->delay) + ")";
    if (e.is<Effect::CliffJump>()) return g("Effect", "cliffJump");
    if (auto r = e.as<Effect::MachineRepair>()) return g("Effect", "machineRepair") + "(share: " + d(r->share) + ")";
    if (auto n = e.as<Effect::Grenade>())
        return g("Effect", "grenade") + "(GameCore.Grenade(damage: " + d(n->grenade.damage) + ", radius: " + d(n->grenade.radius) +
               ", cooldown: " + d(n->grenade.cooldown) + "))";
    if (auto b = e.as<Effect::Burn>()) return g("Effect", "burn") + "(perSecond: " + d(b->perSecond) + ", seconds: " + d(b->seconds) + ")";
    if (auto f = e.as<Effect::Fragment>()) return g("Effect", "fragment") + "(radius: " + d(f->radius) + ", share: " + d(f->share) + ")";
    if (e.is<Effect::DoubleBeam>()) return g("Effect", "doubleBeam");
    if (e.is<Effect::NaniteBeam>()) return g("Effect", "naniteBeam");
    if (auto r = e.as<Effect::DropRush>()) return g("Effect", "dropRush") + "(" + rush(r->rush) + ")";
    if (auto c = e.as<Effect::Cruise>()) return g("Effect", "cruise") + "(speed: " + d(c->speed) + ")";
    if (auto h = e.as<Effect::HitsAir>()) return g("Effect", "hitsAir") + "(share: " + d(h->share) + ")";
    if (auto t = e.as<Effect::SecondTarget>()) return g("Effect", "secondTarget") + "(radius: " + d(t->radius) + ")";
    if (auto b = e.as<Effect::HitsBuildings>()) return g("Effect", "hitsBuildings") + "(share: " + d(b->share) + ")";
    const auto* j = e.as<Effect::AfterJump>();
    return g("Effect", "afterJump") + "(" + rush(j ? j->rush : Rush{}) + ")";
}

std::string describe(const std::vector<Effect>& effects) {
    std::string out = "[";
    for (size_t i = 0; i < effects.size(); i++) {
        if (i > 0) out += ", ";
        out += describe(effects[i]);
    }
    return out + "]";
}

// MARK: - The record's picks

bool PilotRecord::has(Perk perk) const {
    const auto mine = picks(kind(perk));
    return std::find(mine.begin(), mine.end(), perk) != mine.end();
}

/// The choices waiting for a kind, lowest level first. Only the first
/// can be picked from now.
std::vector<PilotRecord::Offer> PilotRecord::pending(UnitKind k) const {
    const int64_t from = static_cast<int64_t>(picks(k).size()) + 2, to = level(k);
    if (!(from <= to)) return {};
    std::vector<Offer> out;
    for (int64_t l = from; l <= to; l++)
        if (auto o = perkOffer(k, l)) out.push_back(Offer{l, o->first, o->second});
    return out;
}

/// Why `perk` cannot be picked now, or nil: it must be one of the two
/// on offer at the kind's lowest pending level.
std::optional<std::string> PilotRecord::refusal(Perk perk) const {
    const auto waiting = pending(kind(perk));
    if (waiting.empty()) return std::string("No pick waiting");
    const auto& first = waiting.front();
    return first.one == perk || first.other == perk ? std::nullopt : std::optional<std::string>("Not on offer");
}

} // namespace ac
