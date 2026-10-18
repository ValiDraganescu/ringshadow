// KSTR contracts (WorldMap.h `WorldContracts`, docs/contracts.md): three
// short jobs per pilot from the frontier radio, drawn by the server's dice
// per pilot and world day, counted from the work the server already sees
// (unloads, ore loads, places reached, buildings finished), paid in XP and
// MH, replaced at the world's dawn when done or abandoned.

#include "WorldMap.h"

#include "Pilot.h"
#include "Rules.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <cmath>

namespace ac::server {

using Contract = net::Contract;
using Kind = net::Contract::Kind;
using State = net::Contract::State;
using json = nlohmann::json;

namespace {

double scale(int64_t level) { return 1 + 0.02 * double(std::max<int64_t>(1, level) - 1); }

int64_t roundTo(double x, int64_t step) { return std::max<int64_t>(step, int64_t(std::llround(x / double(step))) * step); }

uint64_t mix(uint64_t x) {
    // splitmix64's finaliser.
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

uint64_t hashText(const std::string& s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : s) h = (h ^ uint8_t(c)) * 0x100000001b3ULL;
    return h;
}

/// The radio's lines: a title and a pitch, `{n}` the amount, `{where}` the
/// region's name, `{what}` the place or the building.
struct Line {
    const char* title;
    const char* pitch;
};

const std::vector<Line>& lines(Kind kind, bool pad = false) {
    static const std::vector<Line> ore{
        {"Stardust for the Stacks", "KSTR, morning shift. The furnaces want {n} ore at your Citadel. Mind the dust."},
        {"Full Bins", "Somebody has to fill the bins, and today it's you: {n} ore, home before dark if you can."},
        {"Ore Run", "Plain work, honest pay. Haul {n} ore to your Citadel and the station sends something back."},
    };
    static const std::vector<Line> hydrogen{
        {"Quicksilver Order", "We're low on the silver stuff. {n} MH to your Citadel keeps the lights on out here."},
        {"Tank and Back", "Fill up at your Derrick, bring {n} MH home. Don't spill it. It's expensive."},
        {"Cold Fuel", "The night freighters are thirsty: {n} MH, delivered. KSTR thanks you, and so do they."},
    };
    static const std::vector<Line> drill{
        {"Survey Cores", "Geology wants samples from {where}. Drill {n} loads there and tell them it's rock."},
        {"Prove the Vein", "Word is {where} runs rich. Drill {n} loads and prove the rumour, or bury it."},
        {"Out Past the Fence", "{where} hasn't heard a drill in a while. Give it {n} loads' worth of noise."},
    };
    static const std::vector<Line> site{
        {"Something in {where}", "A signal pings from the {what} in {where}. Go and stand on it. We'll wait."},
        {"Lost and Found", "The {what}, out in {where}. Nobody's been. Be the first, then tell us about it."},
    };
    static const std::vector<Line> padLines{
        {"Postcard from {where}", "Stand on the {what} pad in {where}. The view's on us."},
        {"Check the Pad", "The {what} pad in {where} hasn't answered in days. Go over and wave."},
    };
    static const std::vector<Line> build{
        {"Put Something Up", "The Reach could use a {what}. You have the plans; we have faith. Mostly."},
        {"Raise a {what}", "A {what} of your own, welded and finished. The frontier goes up one bolt at a time."},
    };
    static const std::vector<Line> sweep{
        {"Under the Shadow", "When the ringshadow passes, keep drilling. {n} loads in the dark and the giant owes you."},
    };
    switch (kind) {
    case Kind::deliverOre: return ore;
    case Kind::deliverHydrogen: return hydrogen;
    case Kind::drill: return drill;
    case Kind::reach: return pad ? padLines : site;
    case Kind::build: return build;
    case Kind::sweep:
    case Kind::skill: return sweep;
    }
    return ore;
}

std::string fill(std::string text, int64_t n, const std::string& where, const std::string& what) {
    auto put = [&](const std::string& key, const std::string& value) {
        for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size()))
            text.replace(at, key.size(), value);
    };
    put("{n}", std::to_string(n));
    put("{where}", where);
    put("{what}", what);
    return text;
}

/// The first ten minutes' radio lines, step 1…6 (docs/contracts.md, "First
/// ten minutes"): `{what}` the site.
const Line firstLines[WorldContracts::firstSteps] = {
    {"Touchdown", "KSTR to the new pilot: welcome down. There's a crate by the pad with your name on it, more or less. Go and open it."},
    {"A Roof over Your Head", "Those are Citadel plans. Put one up near the pad. Nights out here are long, and the dust gets into everything."},
    {"First Haul", "Find the ore, drill a load, bring it home to your Citadel. That's the whole job. Some pilots do it for years."},
    {"The Wreck on the Rise", "See the {what} up by the wells? A lander that didn't. Hold on its hatch; whatever's inside is yours now."},
    {"Liquid Gold", "Derrick plans, nice. Build one on a well and pump it once. Metallic hydrogen flies you and pays you. Don't spill it."},
    {"Make It Yours", "You've earned a skill point. Spend it on yourself: nobody out here stays the pilot they landed as. KSTR out, for now."},
};

/// Step 2 in a realm that has its Citadel already (R1): reach it instead.
const Line realmCitadelLine = {
    "The Realm's Citadel",
    "Those are Citadel plans, but this world has one: {what} put it up, and it serves every pilot here. Walk up to it and say hello."};

std::string capitalised(std::string s) {
    if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = char(s[0] - 'a' + 'A');
    return s;
}

} // namespace

// MARK: - The numbers

double WorldContracts::perMinute(int64_t level) { return 4500.0 / 60.0 * scale(level); }

double WorldContracts::minutes(Kind kind) {
    switch (kind) {
    case Kind::deliverOre: return 8;
    case Kind::deliverHydrogen: return 10;
    case Kind::drill: return 8;
    case Kind::reach: return 12;
    case Kind::build: return 10;
    case Kind::sweep: return 10;
    case Kind::skill: return 2;
    }
    return 8;
}

double WorldContracts::xp(Kind kind, int64_t level) { return double(roundTo(perMinute(level) * minutes(kind), 5)); }

int64_t WorldContracts::hydrogen(int64_t level) { return roundTo(10 + 2 * double(level), 5); }

int64_t WorldContracts::ore(int64_t level) { return roundTo(300 * scale(level), 50); }

int64_t WorldContracts::hydrogenNeed(int64_t level) { return roundTo(60 * scale(level), 10); }

int64_t WorldContracts::loads(int64_t level) { return 6 + std::max<int64_t>(1, level) / 20; }

double WorldContracts::firstXp(int step) {
    // With what the work pays by itself (the Citadel's blueprint 50, its
    // welding 200 and feat 150, a load 5, the Derrick's blueprint 150): 150,
    // 650 (level 2), 855, 1,155 (level 3, the Derrick's floor); the Derrick
    // (welding 37, feat 100) and step 5: about 1,590 (level 4); 1,690.
    static const double xp[firstSteps] = {100, 150, 200, 150, 300, 100};
    return step >= 1 && step <= firstSteps ? xp[step - 1] : 0;
}

int64_t WorldContracts::firstHydrogen(int step) {
    static const int64_t mh[firstSteps] = {5, 5, 10, 10, 15, 10};
    return step >= 1 && step <= firstSteps ? mh[step - 1] : 0;
}

// MARK: - The world's days

int64_t WorldMap::contractDay(double time) const {
    const double dawn = WorldClock::timeOf(Rules::sunrise, 0, params_.dayLength);
    return int64_t(std::floor((time - dawn) / params_.dayLength));
}

double WorldMap::nextDawn(double time) const {
    const double dawn = WorldClock::timeOf(Rules::sunrise, 0, params_.dayLength);
    return dawn + double(contractDay(time) + 1) * params_.dayLength;
}

bool WorldMap::underSweep(PilotId) const {
    // E1: the driven unit's `Ringshadow::cover` at the world clock, at least
    // `Ringshadow::bonusCover`. Until then no sweep contract is drawn
    // (`WorldParams::sweepContracts`).
    return false;
}

// MARK: - Drawing

void WorldMap::drawContracts(PilotId pilot, PilotState& p, int64_t day) {
    const uint64_t seed = params_.contractSeed ? params_.contractSeed
                                               : uint64_t(WorldClock::weatherSeed(world_->file.id)) ^ 0xc0177ac75ULL;
    const std::string who = p.key.empty() ? "pilot-" + std::to_string(pilot) : p.key;
    std::mt19937_64 dice(mix(seed ^ mix(hashText(who)) ^ mix(uint64_t(day) * 0x2545f4914f6cdd1dULL)));
    p.contracts.resize(WorldContracts::slots);
    for (size_t i = 0; i < p.contracts.size(); i++) {
        Contract& c = p.contracts[i];
        if (c.id != 0 && c.state == State::open) continue;
        std::vector<Kind> taken;
        for (size_t k = 0; k < p.contracts.size(); k++)
            if (k != i && p.contracts[k].id != 0 && p.contracts[k].state == State::open) taken.push_back(p.contracts[k].kind);
        c = drawContract(pilot, p, dice, taken);
        c.id = p.nextContract++;
    }
    p.contractDay = day;
}

Contract WorldMap::drawContract(PilotId, const PilotState& p, std::mt19937_64& dice, const std::vector<Kind>& taken) {
    const int64_t level = p.progress.level();
    auto roll = [&](size_t n) { return n ? size_t(std::uniform_int_distribution<size_t>(0, n - 1)(dice)) : size_t(0); };
    // Where it can drill: regions with ore left, another one than its own
    // if there is one.
    std::vector<size_t> drill, drillAway;
    for (size_t r = 0; r < regions_.size(); r++) {
        const auto& patches = regions_[r]->simulation().state.patches;
        if (std::none_of(patches.begin(), patches.end(), [](const OreDeposit& d) { return d.remaining > 0; })) continue;
        drill.push_back(r);
        if (r != p.region) drillAway.push_back(r);
    }
    // Places to reach: sites it hasn't opened that it can walk to (a vault
    // only Cliff hop reaches: with Cliff hop), pads in other regions.
    std::vector<std::pair<size_t, size_t>> sites, pads;
    if (siteReach_.size() != regions_.size()) siteReach_.resize(regions_.size());
    const bool hop = p.progress.rank(Skill::cliffHop) > 0;
    for (size_t r = 0; r < world_->regions.size(); r++) {
        const region::Built& built = world_->regions[r].built;
        if (!built.sites.empty() && !siteReach_[r]) siteReach_[r] = region::siteReach(built);
        for (size_t i = 0; i < built.sites.size(); i++) {
            const region::SiteReach how = (*siteReach_[r])[i];
            const bool can = how == region::SiteReach::walk || how == region::SiteReach::ramp ||
                             (hop && how == region::SiteReach::cliffHop);
            if (can && !p.progress.sites.count(siteId(r, i)) && !siteSwallowed(r, i)) sites.push_back({r, i});
        }
        if (r != p.region)
            for (size_t i = 0; i < built.pads.size(); i++) pads.push_back({r, i});
    }
    // What it can build: a blueprint it has, at its floor, under its limit.
    std::vector<StructureKind> builds;
    for (const StructureKind k : Progression::blueprintKinds()) {
        const bool byHand = std::find(PilotBuild::worldKinds.begin(), PilotBuild::worldKinds.end(), k) != PilotBuild::worldKinds.end();
        if (byHand && !p.progress.buildRefusal(k, limitCount(k, p.region))) builds.push_back(k);
    }

    struct Option {
        Kind kind;
        int weight;
    };
    std::vector<Option> options;
    if (realmFinished(StructureKind::citadel) || realmFinished(StructureKind::habDome)) {
        options.push_back({Kind::deliverOre, 3});
        if (realmFinished(StructureKind::derrick)) options.push_back({Kind::deliverHydrogen, 2});
    }
    if (!drill.empty()) options.push_back({Kind::drill, 3});
    if (!sites.empty() || !pads.empty()) options.push_back({Kind::reach, 3});
    if (!builds.empty()) options.push_back({Kind::build, 2});
    if (params_.sweepContracts && !drill.empty()) options.push_back({Kind::sweep, 2});
    // A kind not already open in another slot, while there is one.
    std::vector<Option> fresh;
    for (const Option& o : options)
        if (std::find(taken.begin(), taken.end(), o.kind) == taken.end()) fresh.push_back(o);
    const std::vector<Option>& from = fresh.empty() ? options : fresh;
    int total = 0;
    for (const Option& o : from) total += o.weight;
    int pick = int(roll(size_t(std::max(1, total))));
    Kind kind = from.empty() ? Kind::reach : from.front().kind;
    for (const Option& o : from) {
        if (pick < o.weight) {
            kind = o.kind;
            break;
        }
        pick -= o.weight;
    }

    Contract c;
    c.kind = kind;
    c.xp = WorldContracts::xp(kind, level);
    c.hydrogen = WorldContracts::hydrogen(level);
    std::string where, what;
    bool pad = false;
    switch (kind) {
    case Kind::deliverOre: c.need = WorldContracts::ore(level); break;
    case Kind::deliverHydrogen: c.need = WorldContracts::hydrogenNeed(level); break;
    case Kind::drill:
    case Kind::sweep: {
        c.need = WorldContracts::loads(level);
        if (kind == Kind::sweep) break;
        const auto& in = drillAway.empty() ? drill : drillAway;
        const size_t r = in[roll(in.size())];
        c.target = c.region = world_->regions[r].file.id;
        where = world_->regions[r].file.name;
        break;
    }
    case Kind::reach: {
        c.need = 1;
        // Two in three: a site, while there are any.
        pad = sites.empty() || (!pads.empty() && roll(3) == 0);
        const auto [r, i] = pad ? pads[roll(pads.size())] : sites[roll(sites.size())];
        const WorldRegion& g = world_->regions[r];
        c.region = g.file.id;
        where = g.file.name;
        if (pad) {
            c.target = g.file.id + "/pad/" + g.built.pads[i].name;
            what = capitalised(g.built.pads[i].name);
        } else {
            c.target = siteId(r, i);
            what = g.built.sites[i].name;
        }
        break;
    }
    case Kind::build: {
        c.need = 1;
        const StructureKind k = builds[roll(builds.size())];
        c.target = std::string(rawValue(k));
        what = Simulation::title(k);
        break;
    }
    case Kind::skill: c.need = 1; break;
    }
    const auto& choices = lines(kind, pad);
    const Line& line = choices[roll(choices.size())];
    c.title = fill(line.title, c.need, where, what);
    c.pitch = fill(line.pitch, c.need, where, what);
    return c;
}

// MARK: - Each step

void WorldMap::contracts() {
    if (!params_.contracts) return;
    const int64_t day = contractDay(clock_);
    for (auto& [pilot, p] : pilots_) {
        if (p.firstStep > 0) firstTen(pilot, p);
        else if (!p.contractDay || *p.contractDay != day) drawContracts(pilot, p, day);
        if (p.flight) continue;
        // A place reached: standing at the site, or on the pad's plate.
        Region& here = *regions_[p.region];
        const auto unit = here.driving(pilot);
        const auto u = unit ? here.simulation().state.unit(*unit) : std::nullopt;
        if (!u) continue;
        const WorldRegion& g = world_->regions[p.region];
        std::vector<std::string> reached;
        for (const Contract& c : p.contracts) {
            // The first ten minutes' sites count when opened (`firstTen`).
            if (c.state != State::open || c.kind != Kind::reach || c.step || c.region != g.file.id) continue;
            const std::string rest = c.target.substr(c.region.size() + 1);
            if (rest.rfind("pad/", 0) == 0) {
                for (const region::Place& pad : g.built.pads)
                    if (pad.name == rest.substr(4) && distance(u->position, pad.at) <= params_.padReach) reached.push_back(c.target);
            } else {
                for (const region::Site& s : g.built.sites)
                    if (s.name == rest && distance(u->position, s.at) <= WorldSites::reach) reached.push_back(c.target);
            }
        }
        for (const std::string& t : reached) contractWork(pilot, p, Kind::reach, t, 1);
    }
}

void WorldMap::contractWork(PilotId pilot, PilotState& p, Kind kind, const std::string& target, int64_t amount) {
    if (amount <= 0) return;
    for (Contract& c : p.contracts) {
        if (c.state != State::open || c.kind != kind || c.target != target) continue;
        c.have = std::min(c.need, c.have + amount);
        if (c.have < c.need) continue;
        payContract(pilot, p, c);
    }
}

void WorldMap::payContract(PilotId pilot, PilotState& p, Contract& c) {
    // Done: paid now, replaced at dawn (a first-ten-minutes step: by the
    // next step at once).
    c.state = State::done;
    p.news.push_back(net::ProgressMessage::News{net::ProgressMessage::NewsKind::contract, std::to_string(c.id), c.xp, c.hydrogen, c.title});
    const int64_t level = p.progress.level();
    earn(p, c.xp);
    if (!p.flight) bankOut(p, p.region);
    p.wallet.hydrogen += c.hydrogen;
    if (!p.flight) bankIn(p, p.region);
    if (p.progress.level() != level) pushProgress(pilot);
}

// MARK: - The first ten minutes

std::optional<size_t> WorldMap::startSite(StructureKind kind) const {
    if (!world_) return std::nullopt;
    const auto& sites = world_->regions[start_].built.sites;
    for (size_t i = 0; i < sites.size(); i++)
        if (sites[i].blueprint == kind && !siteSwallowed(start_, i)) return i;
    return std::nullopt;
}

Contract WorldMap::firstContract(const PilotState&, int step) const {
    Contract c;
    c.step = uint8_t(step);
    c.xp = WorldContracts::firstXp(step);
    c.hydrogen = WorldContracts::firstHydrogen(step);
    std::string what;
    auto site = [&](StructureKind kind) {
        c.kind = Kind::reach;
        if (const auto i = startSite(kind)) {
            c.target = siteId(start_, *i);
            c.region = world_->regions[start_].file.id;
            what = world_->regions[start_].built.sites[*i].name;
            if (!what.empty() && what[0] >= 'A' && what[0] <= 'Z') what[0] = char(what[0] - 'A' + 'a');
        }
    };
    switch (step) {
    case 1: site(StructureKind::citadel); break;
    case 2:
        // One Citadel a realm (R1): a pilot landing where one stands
        // reaches it; else it builds the realm's.
        if (const auto at = realmCitadel()) {
            c.kind = Kind::reach;
            c.region = world_->regions[at->first].file.id;
            c.target = c.region + "/Citadel";
            what = builder(at->first, at->second);
            if (what.empty()) what = "another pilot";
        } else {
            c.kind = Kind::build;
            c.target = std::string(rawValue(StructureKind::citadel));
        }
        break;
    case 3:
        c.kind = Kind::deliverOre;
        c.need = Rules::carry;
        break;
    case 4: site(StructureKind::derrick); break;
    case 5:
        // Built (1), then pumped once (2).
        c.kind = Kind::build;
        c.target = std::string(rawValue(StructureKind::derrick));
        c.need = 2;
        break;
    default: c.kind = Kind::skill; break;
    }
    const Line& line = step == 2 && c.kind == Kind::reach ? realmCitadelLine
                                                          : firstLines[std::clamp(step, 1, WorldContracts::firstSteps) - 1];
    c.title = fill(line.title, c.need, "", what);
    c.pitch = fill(line.pitch, c.need, "", what);
    return c;
}

void WorldMap::firstTen(PilotId pilot, PilotState& p) {
    // A step whose work is done already passes at once, to the next.
    for (int guard = 0; p.firstStep > 0 && guard <= WorldContracts::firstSteps; guard++) {
        const auto it = std::find_if(p.contracts.begin(), p.contracts.end(), [&](const Contract& c) { return c.step == p.firstStep; });
        if (it == p.contracts.end()) {
            Contract c = firstContract(p, p.firstStep);
            c.id = p.nextContract++;
            // The radio says where: its site goes on the pilot's map (its
            // column, W2).
            if (c.kind == Kind::reach && !c.target.empty() && p.progress.known.insert(c.target).second) pushProgress(pilot);
            // The step just paid stays under it (PAID), its title for the news.
            std::vector<Contract> list{c};
            for (const Contract& k : p.contracts)
                if (k.step && k.step + 1 == p.firstStep && k.state == State::done) list.push_back(k);
            p.contracts = std::move(list);
            continue;
        }
        Contract& c = *it;
        // Step 2 as the realm stands now: a Citadel another pilot put up
        // since it was told turns "build one" into "reach it".
        if (c.state == State::open && c.step == 2 && !builtFinished(p, StructureKind::citadel)) {
            Contract now = firstContract(p, 2);
            if (now.kind != c.kind || now.target != c.target) {
                now.id = c.id;
                c = now;
            }
        }
        if (c.state == State::open) {
            auto opened = [&](StructureKind kind) {
                return p.progress.blueprints.count(kind) || (!c.target.empty() && p.progress.sites.count(c.target));
            };
            bool now = false;
            switch (c.step) {
            case 1: now = opened(StructureKind::citadel); break;
            case 2:
                now = builtFinished(p, StructureKind::citadel) ||
                      (c.kind == Kind::reach && atRealm(pilot, p, {StructureKind::citadel}));
                break;
            case 4: now = opened(StructureKind::derrick); break;
            case 5:
                if (realmFinished(StructureKind::derrick)) c.have = std::max<int64_t>(c.have, 1);
                break;
            case 6: now = p.progress.spent() > 0; break;
            default: break;
            }
            if (now) c.have = c.need;
            if (c.have < c.need) return;
            payContract(pilot, p, c);
        }
        // Done: the next step, or after the last the daily three at once.
        if (++p.firstStep > WorldContracts::firstSteps) {
            p.firstStep = 0;
            p.contractDay = std::nullopt;
            drawContracts(pilot, p, contractDay(clock_));
            return;
        }
    }
}

void WorldMap::pumped(PilotId, PilotState& p) {
    goalDid(p, "pumped");
    if (p.firstStep != 5) return;
    for (Contract& c : p.contracts)
        if (c.step == 5 && c.state == State::open && c.have >= 1) c.have = c.need;
}

// MARK: - Asked by the pilot, told to it

std::vector<Contract> WorldMap::contracts(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() || !world_ ? std::vector<Contract>{} : it->second.contracts;
}

void WorldMap::setContracts(PilotId pilot, const std::vector<Contract>& list) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    p->contracts = list;
    p->contractDay = contractDay(clock_);
    for (const Contract& c : list) p->nextContract = std::max(p->nextContract, c.id + 1);
}

std::optional<std::string> WorldMap::abandon(PilotId pilot, uint32_t contract) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    for (Contract& c : p->contracts) {
        if (c.id != contract) continue;
        if (c.step) return std::string("The radio's first jobs can't be dropped");
        if (c.state == State::done) return std::string("That contract is done already");
        if (c.state == State::abandoned) return std::string("That contract is dropped already");
        c.state = State::abandoned;
        return std::nullopt;
    }
    return std::string("No such contract");
}

// MARK: - Saved

std::string WorldMap::encodeContracts(const PilotState& p) {
    if (!p.contractDay && p.contracts.empty() && !p.firstStep) return "";
    json list = json::array();
    for (const Contract& c : p.contracts)
        list.push_back(json{{"id", c.id},
                            {"kind", int(c.kind)},
                            {"state", int(c.state)},
                            {"title", c.title},
                            {"pitch", c.pitch},
                            {"target", c.target},
                            {"region", c.region},
                            {"need", c.need},
                            {"have", c.have},
                            {"xp", c.xp},
                            {"hydrogen", c.hydrogen},
                            {"step", int(c.step)}});
    json root{{"next", p.nextContract}, {"list", list}};
    // On the first ten minutes (absent: done with it, or an old save).
    if (p.firstStep) root["first"] = p.firstStep;
    if (p.contractDay) root["day"] = *p.contractDay;
    return root.dump();
}

bool WorldMap::decodeContracts(const std::string& text, PilotState& p) {
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("list") || !root["list"].is_array()) return false;
    std::vector<Contract> out;
    for (const json& j : root["list"]) {
        if (!j.is_object()) return false;
        Contract c;
        c.id = j.value("id", uint32_t(0));
        const int kind = j.value("kind", -1), state = j.value("state", -1);
        if (kind < 0 || kind > int(Kind::skill) || state < 0 || state > int(State::abandoned)) return false;
        c.kind = Kind(kind);
        c.state = State(state);
        c.title = j.value("title", std::string());
        c.pitch = j.value("pitch", std::string());
        c.target = j.value("target", std::string());
        c.region = j.value("region", std::string());
        c.need = j.value("need", int64_t(1));
        c.have = j.value("have", int64_t(0));
        c.xp = j.value("xp", 0.0);
        c.hydrogen = j.value("hydrogen", int64_t(0));
        c.step = uint8_t(std::clamp(j.value("step", 0), 0, WorldContracts::firstSteps));
        out.push_back(std::move(c));
    }
    p.contracts = std::move(out);
    p.nextContract = root.value("next", uint32_t(1));
    p.firstStep = std::clamp(root.value("first", 0), 0, WorldContracts::firstSteps);
    p.contractDay = root.contains("day") && root["day"].is_number_integer() ? std::optional<int64_t>(root["day"].get<int64_t>())
                                                                             : std::nullopt;
    return true;
}

} // namespace ac::server
