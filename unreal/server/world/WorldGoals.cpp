// KSTR's next goal (L1, WorldMap.h `WorldGoals`, docs/contracts.md "What
// next"): after the first ten minutes the radio keeps one goal in front of
// each pilot, from a fixed chain, chosen by its level and what it has done.
// A goal met before it comes up is skipped; the one shown pays a little when
// done; a goal over the pilot's level makes the next one wait for it. It
// never blocks anything: the daily contracts and the missions go on beside it.

#include "WorldMap.h"

#include "Pilot.h"
#include "Rules.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac::server {

using json = nlohmann::json;
using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

namespace {

std::string filled(std::string text, const std::string& key, const std::string& value) {
    for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size())) text.replace(at, key.size(), value);
    return text;
}

constexpr const char* waitPrefix = "level:";

} // namespace

// MARK: - The chain

const std::vector<WorldGoals::Entry>& WorldGoals::chain() {
    static const std::vector<Entry> all{
        {"derrick", 3, "Pump Some Silver",
         "No tank pumped yet? Put a Derrick on a well and fill it once. MH flies you, builds for you and pays the help.",
         "MH is the second currency: every building past the Derrick, the flights and the mercenaries cost it.",
         "B  BUILD A DERRICK ON A WELL  ·  CLICK IT TO PUMP"},
        {"skills", 1, "Spend Your Points",
         "You're sitting on {n} skill points. KSTR can't spend them for you, and they don't earn interest.",
         "Each level gives a point. The tree turns them into a faster drill, a bigger hold, a tougher rig.",
         "K  SPEND YOUR SKILL POINTS  ·  {n} LEFT"},
        {"mission", 1, "Sign Up at a Citadel",
         "Every Citadel's board has work with real pay. Walk up to your nearest, press H, take something off MISSIONS.",
         "Missions pay more than the radio: ore, MH, blueprint copies, supplies. The Journal (L) keeps them.",
         "H  AT A CITADEL  ·  MISSIONS  ·  TAKE ONE"},
        {"shop", 1, "Stock Up",
         "Every Citadel sells kits, canisters, shield charges and flares. Buy one before you need one, not after.",
         "Supplies get you out of trouble in the field: a repair, a tank of MH, a shield, a flare that shows the ore.",
         "H  AT A CITADEL  ·  SHOP  ·  BUY A SUPPLY"},
        {"gatling", 5, "Guard Your Ore",
         "The bugs have found your veins. Gatling plans are in any Citadel's shop: set one by the ore you work.",
         "The Brood mines unguarded veins and bites buildings. A gatling gun shoots ground bugs and low flyers.",
         "B  BUILD A GATLING GUN BY YOUR ORE"},
        {"merc", 5, "Hire a Hand",
         "Mercenaries mine while you sleep, for a fee and a daily wage. Any Citadel's MERCS tab has names.",
         "A mercenary works a field at a 24th of your rate, around the clock, into your wallet.",
         "H  AT A CITADEL  ·  MERCS  ·  HIRE ONE"},
        {"dome", 8, "A Roof Away from Home",
         "Hab Dome plans open at 8. Raise one in another region: a depot to unload at, a bed to wake up in.",
         "A Hab Dome is a field depot and a respawn point, so far ore is worth the walk.",
         "B  BUILD A HAB DOME IN ANOTHER REGION"},
        {"active", 10, "Something for Q",
         "Level 10, and your first active slot. Put a skill with a button on Q; the tree marks which ones.",
         "Actives are what you press in a fight or a rush: a surge, a weld, a dash. More slots open at 25 and 45.",
         "K  SLOT AN ACTIVE ON Q"},
        {"sentinel", 10, "Eyes on the Sky",
         "Gloamwings come in over the walls. Sentinel plans open at 10: an AA tower by your buildings keeps the sky clear.",
         "The Sentinel fires at flyers only. Beside a gatling gun, little gets through.",
         "B  BUILD A SENTINEL  ·  THE AA TOWER"},
        {"foundryPlans", 10, "The Foundry Hunt",
         "Foundry plans are out there, in the {what}. They open at 15: find them before then and you're ready.",
         "The Foundry makes rig modules: drill heads, cargo pods, plating. Its plans wait at a site, or in the shop.",
         "FIND THE FOUNDRY PLANS  ·  THE GOLD COLUMN"},
        {"foundry", 15, "Light the Foundry",
         "Plans in hand and level 15: put the Foundry up. The realm gets one, so put it where it counts.",
         "Without a Foundry the rig stays as it landed. With one, every module is a step past what skills do.",
         "B  BUILD THE FOUNDRY"},
        {"module", 15, "Order a Module",
         "Your Foundry is up: J, order a Drill head Mk I ({price}). It drills faster for good.",
         "Modules lift the rig past what the tree alone does. The Foundry makes them; the Lab opens the higher marks.",
         "J  RIG SCREEN  ·  ORDER A DRILL HEAD MK I"},
        {"fit", 15, "Fit It",
         "Your {what} is in the stores, or will be soon: stand where you unload, J, and fit it.",
         "A module in the stores does nothing. Fitted, it works on every load from then on.",
         "AT A CITADEL OR HAB DOME  ·  J  ·  FIT IT"},
        {"matriarch", 20, "Bring Her Down",
         "She's in {what}, in her nest, the horde round her. Call THE HUNT on the Citadel's REALM tab: the realm's defenders and guards gather at her ring.",
         "One call a day for the whole realm. Her rare blueprints, ore and MH are shared by the damage done; she's back three days after.",
         "H  AT A CITADEL  ·  REALM  ·  CALL THE HUNT  ·  {what}"},
        {"lab", 25, "Raise the Lab",
         "Level 25 and the Lab's plans are open. It researches the marks the Foundry can't make yet.",
         "The Lab opens Mk II and up for every module.",
         "B  BUILD THE LAB"},
        {"research", 25, "Research Mk II",
         "Your Lab is up: J, research Mk II of the Drill head ({price}). Then the Foundry can make it.",
         "Mk II doubles a module's step; Mk III comes after it.",
         "J  RIG SCREEN  ·  RESEARCH MK II"},
        {"spacedock", 35, "A Dock for Ships",
         "Level 35: Spacedock plans. The realm's ships start here, and so does the long way out.",
         "The Spacedock is the realm's yard: the biggest build on the frontier, one a realm.",
         "B  BUILD THE SPACEDOCK"},
    };
    return all;
}

const WorldGoals::Entry* WorldGoals::find(const std::string& key) {
    for (const Entry& e : chain())
        if (key == e.key) return &e;
    return nullptr;
}

double WorldGoals::xp(int64_t level) {
    const double x = WorldContracts::perMinute(level) * 3;
    return double(std::max<int64_t>(5, int64_t(std::llround(x / 5.0)) * 5));
}

int64_t WorldGoals::hydrogen(int64_t level) {
    return std::max<int64_t>(5, int64_t(std::llround(double(5 + std::max<int64_t>(1, level)) / 5.0)) * 5);
}

// MARK: - Met

std::optional<std::pair<size_t, size_t>> WorldMap::worldSite(StructureKind kind) const {
    if (!world_) return std::nullopt;
    for (size_t r = 0; r < world_->regions.size(); r++) {
        const auto& sites = world_->regions[r].built.sites;
        for (size_t i = 0; i < sites.size(); i++)
            if (sites[i].blueprint == kind) return std::pair{r, i};
    }
    return std::nullopt;
}

bool WorldMap::goalMet(const PilotState& p, const WorldGoals::Entry& e) const {
    const std::string key = e.key;
    const ProspectorProgress& g = p.progress;
    // The realm's count of a kind, finished or not: at the pilot's limit
    // the goal can't be done, so it counts as met (never blocks).
    auto full = [&](StructureKind k) {
        int64_t n = 0;
        for (const auto& r : regions_)
            for (const Structure& b : r->simulation().state.structures)
                if (b.kind == k && realm(b)) n += 1;
        return n >= g.limit(k);
    };
    auto builtOrFull = [&](StructureKind k) { return builtFinished(p, k) || full(k); };
    if (key == "derrick") return p.goalsDid.count("pumped") || builtFinished(p, StructureKind::derrick);
    if (key == "skills") return g.unspent() <= 0;
    if (key == "mission") return p.goalsDid.count("mission") || !p.journal.empty();
    if (key == "shop") return p.goalsDid.count("shop") || !g.items.empty() || !g.bought.empty();
    if (key == "gatling") return builtOrFull(StructureKind::gatling);
    if (key == "merc") return p.goalsDid.count("merc") || !p.mercs.empty();
    if (key == "dome") {
        for (size_t r = 0; r < regions_.size(); r++) {
            if (r == start_) continue;
            for (const Structure& b : regions_[r]->simulation().state.structures)
                if (b.kind == StructureKind::habDome && b.complete() && built(p, r, b.id)) return true;
        }
        return full(StructureKind::habDome);
    }
    if (key == "active") return std::any_of(g.slots.begin(), g.slots.end(), [](const auto& s) { return s.has_value(); });
    if (key == "sentinel") return builtOrFull(StructureKind::sentinel);
    if (key == "foundryPlans") {
        if (g.blueprints.count(StructureKind::foundry) || realmFinished(StructureKind::foundry)) return true;
        const auto site = worldSite(StructureKind::foundry);
        return site && g.sites.count(siteId(site->first, site->second));
    }
    if (key == "foundry") return realmFinished(StructureKind::foundry);
    if (key == "module") return p.goalsDid.count("made") || g.rig.making || !g.rig.stores.empty() || !g.rig.fitted.empty();
    if (key == "fit") return !g.rig.fitted.empty();
    if (key == "lab") return realmFinished(StructureKind::lab);
    if (key == "research") {
        if (g.rig.research) return true;
        for (const Module m : Modules::all())
            if (g.rig.researchedMark(m) >= 2) return true;
        return false;
    }
    if (key == "spacedock") return realmFinished(StructureKind::spacedock);
    if (key == "matriarch") return p.goalsDid.count("matriarch") > 0 || !nestRegion();
    return true;
}

// MARK: - Each step

void WorldMap::goals() {
    if (!params_.contracts || !world_ || clock_ - goalsAt_ < WorldGoals::every) return;
    goalsAt_ = clock_;
    for (auto& [pilot, p] : pilots_) goalStep(pilot, p);
}

void WorldMap::goalStep(PilotId pilot, PilotState& p) {
    // On the first ten minutes: none yet (the chain's step is the goal).
    if (p.firstStep > 0) return;
    const std::string was = p.goal;
    const int64_t level = p.progress.level();
    // The one shown, done: paid, passed.
    if (const WorldGoals::Entry* e = WorldGoals::find(p.goal); e && goalMet(p, *e)) {
        const double xp = WorldGoals::xp(level);
        const int64_t mh = WorldGoals::hydrogen(level);
        p.goalsPassed.insert(p.goal);
        p.news.push_back(News{NewsKind::goal, p.goal, xp, mh, e->title});
        earn(p, xp);
        if (!p.flight) bankOut(p, p.region);
        p.wallet.hydrogen += mh;
        if (!p.flight) bankIn(p, p.region);
        p.goal.clear();
    }
    // The next: the first in the chain not passed and open at this level;
    // those met already are passed on the way, unpaid. None open: a wait
    // for the lowest level among the rest.
    std::string next;
    int64_t wait = 0;
    for (const WorldGoals::Entry& e : WorldGoals::chain()) {
        if (p.goalsPassed.count(e.key)) continue;
        if (e.key == p.goal) {
            // Still open (its level is the pilot's).
            next = e.key;
            break;
        }
        // No points waiting: nothing to spend yet (it comes with a level).
        if (std::string(e.key) == "skills" && goalMet(p, e)) continue;
        if (goalMet(p, e)) {
            p.goalsPassed.insert(e.key);
            continue;
        }
        if (e.level <= level) {
            next = e.key;
            break;
        }
        if (!wait || e.level < wait) wait = e.level;
    }
    if (next.empty() && wait) next = waitPrefix + std::to_string(wait);
    p.goal = next;
    // The Foundry hunt: its site goes on the pilot's map (its column).
    if (p.goal == "foundryPlans" && was != p.goal)
        if (const auto site = worldSite(StructureKind::foundry)) p.progress.known.insert(siteId(site->first, site->second));
    if (p.goal != was || !p.news.empty()) pushProgress(pilot);
}

std::optional<net::Goal> WorldMap::goalMessage(PilotId pilot, const PilotState& p) const {
    if (p.goal.empty() || !world_) return std::nullopt;
    net::Goal out;
    out.key = p.goal;
    const int64_t level = p.progress.level();
    if (p.goal.rfind(waitPrefix, 0) == 0) {
        // A wait: what the level opens.
        out.key = "level";
        out.level = std::max<int64_t>(1, std::atoll(p.goal.c_str() + std::string(waitPrefix).size()));
        std::string opens;
        for (const WorldGoals::Entry& e : WorldGoals::chain())
            if (opens.empty() && e.level == out.level && !p.goalsPassed.count(e.key)) opens = e.title;
        out.title = "Reach Level " + std::to_string(out.level);
        out.pitch = "Keep at it: level " + std::to_string(out.level) + " opens the next job" +
                    (opens.empty() ? std::string(".") : ", " + opens + ". Ore, contracts and missions all count.");
        out.why = "Every load, building and contract earns XP. The radio has more for you at level " + std::to_string(out.level) + ".";
        out.hint = "REACH LEVEL " + std::to_string(out.level) + "  ·  YOU ARE " + std::to_string(level);
        return out;
    }
    const WorldGoals::Entry* e = WorldGoals::find(p.goal);
    if (!e) return std::nullopt;
    out.level = e->level;
    out.xp = WorldGoals::xp(level);
    out.hydrogen = WorldGoals::hydrogen(level);
    std::string what, price;
    auto mark = [&](size_t r, Vec2 at) {
        out.marked = true;
        out.region = world_->regions[r].file.id;
        out.at = at;
    };
    // The Citadel's goals point at the nearest Citadel (R3: one a region):
    // the pilot's region's, else the realm's first.
    if (p.goal == "mission" || p.goal == "shop" || p.goal == "merc") {
        bool here = false;
        if (!p.flight)
            for (const Structure& s : regions_[p.region]->simulation().state.structures)
                if (!here && s.kind == StructureKind::citadel && s.complete() && realm(s)) {
                    mark(p.region, s.position);
                    here = true;
                }
        if (!here)
            if (const auto c = realmCitadel())
                for (const Structure& s : regions_[c->first]->simulation().state.structures)
                    if (s.id == c->second) mark(c->first, s.position);
    } else if (p.goal == "derrick" && !p.flight) {
        // The nearest well with MH in the pilot's region.
        const Region& here = *regions_[p.region];
        const auto unit = p.unit ? here.simulation().state.unit(*p.unit) : std::nullopt;
        const Vec2 from = unit ? unit->position : Vec2(0, 0);
        std::optional<Vec2> best;
        if (const auto& wells = here.simulation().state.wells)
            for (const Well& w : *wells)
                if (w.remaining > 0 && (!best || distance(w.position, from) < distance(*best, from))) best = w.position;
        if (best) mark(p.region, *best);
    } else if (p.goal == "foundryPlans") {
        if (const auto site = worldSite(StructureKind::foundry)) {
            const auto& s = world_->regions[site->first].built.sites[site->second];
            what = s.name + " in " + world_->regions[site->first].file.name;
            mark(site->first, s.at);
        } else {
            what = "Citadel's shop";
        }
    } else if (p.goal == "module" || p.goal == "research") {
        // The price from the rules (Mk I at the Foundry, Mk II at the Lab).
        const auto secs = [](double t) {
            return t >= 120 ? std::to_string(int64_t(std::llround(t / 60))) + " min" : std::to_string(int64_t(std::llround(t))) + " s";
        };
        price = p.goal == "module" ? std::to_string(Modules::ore(1)) + " ore + " + std::to_string(Modules::hydrogen(1)) + " MH, " +
                                         secs(Modules::makeTime(1))
                                   : std::to_string(Modules::researchHydrogen(2)) + " MH, " + secs(Modules::researchTime(2));
    } else if (p.goal == "fit") {
        // What waits: the stores' first, else the Foundry's job; marked at
        // the nearest place to fit it (the pilot's region's Citadel).
        const Rig& rig = p.progress.rig;
        what = !rig.stores.empty() ? Modules::title(rig.stores.front().module)
               : rig.making       ? Modules::title(rig.making->module)
                                  : std::string("module");
        if (!p.flight)
            for (const Structure& s : regions_[p.region]->simulation().state.structures)
                if (!out.marked && s.kind == StructureKind::citadel && s.complete() && realm(s)) mark(p.region, s.position);
    } else if (p.goal == "matriarch") {
        if (const auto r = nestRegion()) {
            what = world_->regions[*r].file.name;
            if (const auto at = nestAt(*r)) mark(*r, *at);
        }
    }
    (void)pilot;
    const std::string n = std::to_string(p.progress.unspent());
    out.title = e->title;
    out.pitch = filled(filled(filled(e->pitch, "{n}", n), "{what}", what), "{price}", price);
    out.why = e->why;
    std::string hint = filled(filled(e->hint, "{n}", n), "{what}", what);
    for (char& c : hint) c = char(std::toupper(uint8_t(c)));
    out.hint = hint;
    return out;
}

// MARK: - Saved

std::string WorldMap::encodeGoals(const PilotState& p) {
    if (p.goal.empty() && p.goalsPassed.empty() && p.goalsDid.empty()) return "";
    json root{{"goal", p.goal}, {"passed", p.goalsPassed}, {"did", p.goalsDid}};
    return root.dump();
}

bool WorldMap::decodeGoals(const std::string& text, PilotState& p) {
    p.goal.clear();
    p.goalsPassed.clear();
    p.goalsDid.clear();
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) return false;
    p.goal = root.value("goal", std::string());
    for (const char* name : {"passed", "did"}) {
        if (!root.contains(name)) continue;
        if (!root[name].is_array()) return false;
        for (const json& k : root[name]) {
            if (!k.is_string()) return false;
            (std::string(name) == "passed" ? p.goalsPassed : p.goalsDid).insert(k.get<std::string>());
        }
    }
    return true;
}

// MARK: - Dev and shots

std::string WorldMap::devGoal(const std::string& line) {
    std::istringstream in(line);
    std::string word, what, arg;
    in >> word >> what >> arg;
    if (!world_ || pilots_.empty()) return "dev: goal: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (what == "pass" && WorldGoals::find(arg)) {
        p.goalsPassed.insert(arg);
        if (p.goal == arg) p.goal.clear();
    } else if (what == "level") {
        const int64_t to = std::clamp<int64_t>(std::atoll(arg.c_str()), 1, Progression::cap);
        if (p.progress.xp < Progression::xp(to)) earn(p, Progression::xp(to) - p.progress.xp);
        for (const WorldGoals::Entry& e : WorldGoals::chain())
            if (e.level < to) p.goalsPassed.insert(e.key);
        if (WorldGoals::find(p.goal) && p.goalsPassed.count(p.goal)) p.goal.clear();
    } else if (!what.empty()) {
        return "dev: goal '" + line + "' not done (not `goal`, `goal pass KEY`, `goal level N`)";
    }
    goalStep(pilot, p);
    pushProgress(pilot);
    return "dev: goal " + (p.goal.empty() ? std::string("none") : p.goal) + ", level " + std::to_string(p.progress.level());
}

} // namespace ac::server
