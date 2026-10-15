// Frontier pilots (WorldMap.h `WorldFrontier`, P1; docs/frontier.md): the
// pilots the server runs so a world with one human reads as inhabited. Each
// is a pilot of the world by the pilot's rules: its own wallet, XP and
// level, skill points spent by its habit's build, blueprint copies bought at
// the realm's Citadel at the shop's price, the world's building prices and
// limits, the Citadel's tax on every unload, the Dropship's fare. None of
// them come before the first human finished the opening chain, and none
// touch the chain's places while a human is on it.
//
// Live where a human can see them (in its region or next to it): their
// Prospectors are units of the realm the server steers with the core's own
// orders (to a deposit, home with a load, into a Derrick, an errand to a
// point, a hunt). Elsewhere their work is booked on the world's clock: a
// walk is a straight line at a Prospector's speed, a load comes at an
// unskilled Prospector's pace, a building rises as its welding would.

#include "WorldMap.h"

#include "Brood.h"
#include "NavGrid.h"
#include "Session.h"

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
using Habit = WorldFrontier::Habit;
using Doing = WorldFrontier::Doing;
using Act = FrontierPlan::Act;

// MARK: - The rules

Habit WorldFrontier::habitOf(size_t i) {
    static const Habit order[] = {Habit::miner, Habit::builder, Habit::defender, Habit::wanderer, Habit::miner, Habit::builder};
    return order[i % 6];
}

const std::vector<std::string>& WorldFrontier::callsigns() {
    static const std::vector<std::string> n = {"Vesper", "Kestrel", "Corvid", "Tamsin", "Brindle", "Sorrel",
                                               "Moraine", "Talus",  "Lumen",  "Calder", "Drover",  "Larkspur"};
    return n;
}

int64_t WorldFrontier::levelAt(int64_t human, size_t i) {
    static const int64_t spread[] = {9, 3, 6, 1, 12, 4};
    return std::clamp<int64_t>(std::max<int64_t>(human, 1) + spread[i % 6], 2, Progression::cap);
}

double WorldFrontier::bookRate() {
    return double(Rules::carry) / (Rules::miningTime + 2 * bookWalk / Rules::prospectorSpeed);
}

namespace {

const char* kindWord(StructureKind k) {
    switch (k) {
    case StructureKind::habDome: return "a Hab Dome";
    case StructureKind::derrick: return "a Derrick";
    case StructureKind::gatling: return "a Gatling Gun";
    case StructureKind::landingPad: return "a Landing Pad";
    default: return "a building";
    }
}

/// The radio's lines, by occasion ({name}: its callsign, {where}: a place).
const std::vector<std::string>& lines(const std::string& kind) {
    static const std::map<std::string, std::vector<std::string>> all = {
        {"hello",
         {"{name} on the frontier band. Down at {where}, ready to work.", "This is {name}, fresh off the Dropship at {where}. Save me a seam.",
          "{name} here. Landed at {where}; the ring looks bigger from the ground."}},
        {"raid",
         {"{name}: swarm moving on {where}. Guns up, pilots.", "Bugs inbound at {where}. {name} sees them, keep your heads down.",
          "{name}: something's chewing toward {where}. Here they come."}},
        {"cheer",
         {"{name}: that wave is broken at {where}! Good shooting, all.", "Wave down at {where}. {name} owes somebody a drink.",
          "{name}: {where} holds. The Brood can count its dead."}},
        {"help",
         {"{name}, taking a beating near {where}. Anyone close?", "Mayday from {name} at {where}: bugs all over me.",
          "{name}: hull's going at {where}. Could use a hand."}},
        {"chatter",
         {"{name}: the ore runs sweet in {where} today.", "{name}: anyone else hear the ring hum at dusk?",
          "{name}: Citadel's prices climbed again. Figures.", "{name}: quiet out by {where}. Too quiet.",
          "{name}: storm light over {where}. Pretty, if you don't look at the sky too long."}},
    };
    static const std::vector<std::string> none;
    const auto it = all.find(kind);
    return it == all.end() ? none : it->second;
}

std::string fill(std::string text, const std::string& name, const std::string& where) {
    for (const auto& [key, value] : {std::pair<std::string, std::string>{"{name}", name}, {"{where}", where}}) {
        for (size_t at = text.find(key); at != std::string::npos; at = text.find(key, at + value.size())) text.replace(at, key.size(), value);
    }
    return text;
}

bool ground(const Unit& u) { return !u.stats().air; }

region::Edge opposite(region::Edge e) {
    switch (e) {
    case region::Edge::north: return region::Edge::south;
    case region::Edge::south: return region::Edge::north;
    case region::Edge::east: return region::Edge::west;
    case region::Edge::west: return region::Edge::east;
    }
    return e;
}

} // namespace

// MARK: - Who may come, and where they keep off

bool WorldMap::frontierOpen() const {
    if (!world_) return false;
    auto done = [](const PilotState& p) { return !p.key.empty() && p.firstStep == 0; };
    for (const auto& [id, p] : pilots_)
        if (done(p)) return true;
    for (const auto& [key, p] : away_)
        if (done(p)) return true;
    return false;
}

bool WorldMap::chainRunning() const {
    for (const auto& [id, p] : pilots_)
        if (!p.key.empty() && p.firstStep > 0) return true;
    return false;
}

std::vector<Vec2> WorldMap::chainPlaces(size_t r) const {
    std::vector<Vec2> out;
    if (!world_ || r != start_) return out;
    const region::Built& b = world_->regions[r].built;
    if (world_->startPad < b.pads.size()) out.push_back(b.pads[world_->startPad].at);
    for (const StructureKind k : {StructureKind::citadel, StructureKind::derrick})
        if (const auto i = startSite(k)) out.push_back(b.sites[*i].at);
    const GameState& st = regions_[r]->simulation().state;
    const Vec2 pad = out.empty() ? Vec2(0, 0) : out[0];
    std::optional<Vec2> deposit;
    for (size_t k = 0; k < std::min(st.patches.size(), r < ownPatches_.size() ? ownPatches_[r] : st.patches.size()); k++)
        if (!deposit || distance(st.patches[k].position, pad) < distance(*deposit, pad)) deposit = st.patches[k].position;
    if (deposit) out.push_back(*deposit);
    if (const auto i = startSite(StructureKind::derrick); i && st.wells) {
        std::optional<Vec2> well;
        for (const auto& w : *st.wells)
            if (!well || distance(w.position, b.sites[*i].at) < distance(*well, b.sites[*i].at)) well = w.position;
        if (well) out.push_back(*well);
    }
    return out;
}

bool WorldMap::keptOff(size_t r, Vec2 at) const {
    if (!chainRunning()) return false;
    for (const Vec2 p : chainPlaces(r))
        if (distance(p, at) < WorldFrontier::keepOff) return true;
    return false;
}

bool WorldMap::frontierLive(const FrontierPilot& f) const {
    return f.arrived && !f.travelTo && clock_ >= f.downUntil && pilotNear(f.region);
}

WorldMap::FrontierPilot* WorldMap::frontierPilot(uint32_t id) {
    for (FrontierPilot& f : frontier_)
        if (f.id == id) return &f;
    return nullptr;
}

const WorldMap::FrontierPilot* WorldMap::frontierByKey(const std::string& key) const {
    for (const FrontierPilot& f : frontier_)
        if (f.key == key) return &f;
    return nullptr;
}

// MARK: - The roster

void WorldMap::frontierRoster() {
    if (!world_ || !frontier_.empty() || params_.frontier <= 0 || !frontierOpen()) return;
    frontierDice_.seed(params_.frontierSeed ? params_.frontierSeed : uint64_t(WorldClock::weatherSeed(world_->file.id)) ^ 0xf207713eULL);
    // The first human's level: theirs come near it.
    int64_t human = 1;
    for (const auto& [id, p] : pilots_)
        if (!p.key.empty()) human = std::max(human, p.progress.level());
    for (const auto& [key, p] : away_) human = std::max(human, p.progress.level());
    std::vector<std::string> names = WorldFrontier::callsigns();
    std::shuffle(names.begin(), names.end(), frontierDice_);
    const size_t n = size_t(std::clamp<int64_t>(params_.frontier, 0, 6));
    for (size_t i = 0; i < n; i++) {
        FrontierPilot f;
        f.id = uint32_t(i + 1);
        f.callsign = names[i % names.size()];
        f.key = "frontier:" + f.callsign;
        std::transform(f.key.begin(), f.key.end(), f.key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        f.habit = WorldFrontier::habitOf(i);
        f.paint = WorldFrontier::paints[i % WorldFrontier::paints.size()];
        f.progress.xp = Progression::xp(WorldFrontier::levelAt(human, i));
        // A new pilot's wallet, as any pilot's.
        f.wallet = Wallet{params_.startOre, params_.startHydrogen + 40};
        f.comes = clock_ + WorldFrontier::arriveAfter + WorldFrontier::arriveEvery * double(i);
        f.region = start_;
        frontierSkills(f);
        frontier_.push_back(std::move(f));
    }
}

void WorldMap::frontierSkills(FrontierPilot& f) {
    // Its habit's branch first, in the tree's order, then any other.
    static const Branch home[] = {Branch::deepVein, Branch::torchwright, Branch::hardshell, Branch::wayfinder};
    const Branch mine = home[size_t(f.habit) % 4];
    for (int guard = 0; f.progress.unspent() > 0 && guard < 200; guard++) {
        bool spent = false;
        for (const bool own : {true, false}) {
            for (const SkillNode& n : Progression::nodes()) {
                if ((n.branch == mine) != own || Progression::retired(n.skill)) continue;
                if (!f.progress.spendRefusal(n.skill) && f.progress.spend(n.skill, clock_)) {
                    spent = true;
                    break;
                }
            }
            if (spent) break;
        }
        if (!spent) break;
    }
    f.progress.recent.clear();
}

void WorldMap::frontierEarn(FrontierPilot& f, double xp) {
    if (!(xp > 0)) return;
    const int64_t was = f.progress.level();
    f.progress.xp += xp;
    if (f.progress.level() != was) frontierSkills(f);
}

// MARK: - Where it is, what is near

Unit* WorldMap::frontierUnitOf(const FrontierPilot& f) {
    if (!f.unit) return nullptr;
    Simulation& sim = regions_[f.region]->simulation();
    const auto i = sim.unitIndex(*f.unit);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

const Unit* WorldMap::frontierUnitOf(const FrontierPilot& f) const {
    if (!f.unit) return nullptr;
    const Simulation& sim = regions_[f.region]->simulation();
    const auto i = sim.unitIndex(*f.unit);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

Vec2 WorldMap::frontierAt(const FrontierPilot& f) const {
    const Unit* u = frontierUnitOf(f);
    return u ? u->position : f.at;
}

std::optional<int64_t> WorldMap::frontierDepot(size_t r, Vec2 at) const {
    std::optional<int64_t> best;
    double d = 1e18;
    for (const Structure& b : regions_[r]->simulation().state.structures) {
        if ((b.kind != StructureKind::citadel && b.kind != StructureKind::habDome) || !b.complete() || !realm(b)) continue;
        if (distance(b.position, at) < d) {
            d = distance(b.position, at);
            best = b.id;
        }
    }
    return best;
}

bool WorldMap::frontierBeside(const FrontierPilot& f, int64_t building) const {
    const auto b = regions_[f.region]->simulation().state.structure(building);
    if (!b || !b->complete() || !realm(*b)) return false;
    return NavGrid::insideStructure(frontierAt(f), *b, Simulation::pilotDepositReach + 0.6);
}

std::optional<std::pair<size_t, Vec2>> WorldMap::frontierStepToward(size_t from, size_t to, Vec2 at) const {
    if (from == to) return std::nullopt;
    // Breadth first over the regions joined by openings on both sides.
    const size_t n = regions_.size();
    std::vector<int64_t> prev(n, -1);
    std::vector<size_t> queue{from};
    prev[from] = int64_t(from);
    for (size_t k = 0; k < queue.size() && prev[to] < 0; k++)
        for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west}) {
            const auto nb = neighbour(queue[k], e);
            if (!nb || prev[*nb] >= 0) continue;
            if (world_->regions[queue[k]].built.openings[size_t(e)].empty() ||
                world_->regions[*nb].built.openings[size_t(opposite(e))].empty())
                continue;
            prev[*nb] = int64_t(queue[k]);
            queue.push_back(*nb);
        }
    if (prev[to] < 0) return std::nullopt;
    size_t next = to;
    while (size_t(prev[next]) != from) next = size_t(prev[next]);
    // The opening toward it nearest `at`: its middle, at the edge.
    for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west}) {
        if (neighbour(from, e) != next) continue;
        const region::Built& b = world_->regions[from].built;
        const region::Built& t = world_->regions[next].built;
        std::optional<Vec2> best;
        for (const region::Span& s : b.openings[size_t(e)]) {
            const double mid = (s.from + s.to) / 2;
            bool there = false;
            for (const region::Span& o : t.openings[size_t(opposite(e))]) there = there || (mid >= o.from && mid <= o.to);
            if (!there) continue;
            const Vec2 out = b.ground(b.edgePoint(e, mid, 1.0));
            if (!best || distance(out, at) < distance(*best, at)) best = out;
        }
        if (best) return std::pair<size_t, Vec2>{next, *best};
    }
    return std::nullopt;
}

std::optional<Vec2> WorldMap::raidedField(size_t r) const {
    const auto& f = fields(r);
    for (size_t k = 0; k < f.size(); k++) {
        const auto it = fieldRaided_.find({r, int64_t(k)});
        if (it == fieldRaided_.end() || clock_ - it->second > WorldFrontier::raidMemory) continue;
        const Vec2 mid = fieldAt(r, int64_t(k));
        bool guarded = false;
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if ((b.kind == StructureKind::gatling || b.kind == StructureKind::sentinel) && realm(b) &&
                distance(b.position, mid) < WorldFrontier::turretField)
                guarded = true;
        if (!guarded && fieldOre(r, int64_t(k)) > 0) return mid;
    }
    return std::nullopt;
}

std::optional<int64_t> WorldMap::frontierFoe(const FrontierPilot& f) const {
    const Simulation& sim = regions_[f.region]->simulation();
    if (!sim.broodSide) return std::nullopt;
    const Vec2 at = frontierAt(f);
    std::vector<Vec2> guard{at};
    for (const Structure& b : sim.state.structures)
        if (realm(b)) guard.push_back(b.position);
    std::optional<int64_t> best;
    double d = 1e18;
    for (const Unit& u : sim.state.units) {
        if (u.owner != *sim.broodSide || u.hp <= 0 || !ground(u) || u.kind == UnitKind::matriarch) continue;
        bool near = false;
        for (const Vec2 g : guard) near = near || distance(g, u.position) < WorldFrontier::guard;
        if (near && distance(at, u.position) < d) {
            d = distance(at, u.position);
            best = u.id;
        }
    }
    return best;
}

const WorldMap::Canyon* WorldMap::frontierCanyon(const FrontierPilot& f) const {
    const Canyon* best = nullptr;
    for (const Canyon& c : canyons_) {
        if (!c.out || c.state == CanyonState::failed || c.state == CanyonState::held) continue;
        bool human = false;
        for (const auto& [id, p] : pilots_) human = human || (!p.flight && p.region == c.region && !p.key.empty());
        if (!human) continue;
        if (!best || c.region == f.region) best = &c;
    }
    return best;
}

// MARK: - The decisions

std::optional<FrontierPlan> WorldMap::minePlan(const FrontierPilot& f) const {
    const size_t r = f.region;
    const Vec2 at = frontierAt(f);
    const GameState& st = regions_[r]->simulation().state;
    if (f.cargo > 0) {
        const auto d = frontierDepot(r, at);
        if (!d) return std::nullopt;
        FrontierPlan p;
        p.act = Act::haul;
        p.target = *d;
        p.at = st.structure(*d)->position;
        p.why = "home with a load";
        return p;
    }
    // Every few trips a tank of MH, at a Derrick of the realm here.
    if (f.trips >= WorldFrontier::pumpEvery && frontierDepot(r, at))
        for (const Structure& b : st.structures)
            if (b.kind == StructureKind::derrick && b.complete() && realm(b) && !keptOff(r, b.position)) {
                FrontierPlan p;
                p.act = Act::pump;
                p.target = b.id;
                p.at = b.position;
                p.why = "a tank of MH";
                return p;
            }
    std::optional<size_t> best;
    for (size_t k = 0; k < std::min(st.patches.size(), r < ownPatches_.size() ? ownPatches_[r] : st.patches.size()); k++) {
        const OreDeposit& d = st.patches[k];
        if (d.remaining <= 0 || keptOff(r, d.position) || !frontierDepot(r, d.position)) continue;
        if (!best || distance(d.position, at) < distance(st.patches[*best].position, at)) best = k;
    }
    if (!best) return std::nullopt;
    FrontierPlan p;
    p.act = Act::mine;
    p.target = int64_t(*best);
    p.at = st.patches[*best].position;
    p.why = "drilling";
    return p;
}

std::optional<std::string> WorldMap::frontierCap(size_t r, StructureKind kind, Vec2 at) const {
    int64_t mine = 0, here = 0, all = 0;
    for (size_t i = 0; i < regions_.size(); i++)
        for (const Structure& b : regions_[i]->simulation().state.structures) {
            const auto by = builtBy_[i].find(b.id);
            if (by == builtBy_[i].end() || !frontierKey(by->second)) continue;
            all += 1;
            if (b.kind == kind) {
                mine += 1;
                if (i == r) here += 1;
            }
        }
    if (all >= WorldFrontier::buildingsInAll) return std::string("The frontier has built enough");
    switch (kind) {
    case StructureKind::habDome:
        if (mine >= WorldFrontier::habDomes) return std::string("A Hab Dome of the frontier's stands already");
        break;
    case StructureKind::derrick:
        if (mine >= WorldFrontier::derricks) return std::string("The frontier's Derricks are up");
        break;
    case StructureKind::gatling:
        if (mine >= WorldFrontier::gatlings) return std::string("The frontier's Gatling Guns are up");
        break;
    case StructureKind::landingPad:
        if (here >= WorldFrontier::landingPads) return std::string("A Landing Pad of the frontier's stands here");
        break;
    default: return std::string("The frontier builds no ") + Simulation::title(kind);
    }
    // One always left for a human under the realm's limit.
    if (limitCount(kind, r) + 2 > Progression::buildingLimit(kind)) return std::string("Left for the realm's pilots");
    const Simulation& sim = regions_[r]->simulation();
    for (const Structure& b : sim.state.structures) {
        if (!realm(b)) continue;
        if (distance(b.position, at) < WorldFrontier::apart + Rules::radius(b.kind) + Rules::radius(kind)) return std::string("Too near a building");
        if (kind == StructureKind::gatling && (b.kind == StructureKind::gatling || b.kind == StructureKind::sentinel) &&
            distance(b.position, at) < WorldFrontier::turretField)
            return std::string("A turret guards this field");
    }
    if (keptOff(r, at)) return std::string("Kept off the opening chain's places");
    return std::nullopt;
}

std::optional<Vec2> WorldMap::frontierSpot(const FrontierPilot& f, StructureKind kind, Vec2 near) const {
    const size_t r = f.region;
    const Simulation& sim = regions_[r]->simulation();
    const auto& pads = world_->regions[r].built.pads;
    auto fits = [&](Vec2 q) -> std::optional<Vec2> {
        const auto spot = sim.snap(kind, q);
        if (!spot) return std::nullopt;
        if (sim.placementRefusal(kind, *spot)) return std::nullopt;
        if (region::touchesPadPlate(pads, *spot, Rules::radius(kind))) return std::nullopt;
        if (frontierCap(r, kind, *spot)) return std::nullopt;
        return spot;
    };
    if (kind == StructureKind::derrick) return fits(near);
    for (double rad = 4; rad <= 16; rad += 2)
        for (int k = 0; k < 16; k++) {
            const double a = k * 6.283185307179586 / 16;
            if (const auto s = fits(near + Vec2(std::cos(a), std::sin(a)) * rad)) return s;
        }
    return std::nullopt;
}

std::optional<FrontierPlan> WorldMap::buildPlan(const FrontierPilot& f, StructureKind kind, Vec2 near, const std::string& why) const {
    const size_t r = f.region;
    if (f.progress.level() < f.progress.floor(kind)) return std::nullopt;
    if (!frontierSpot(f, kind, near)) return std::nullopt;
    const int64_t ore = Progression::worldOre(kind), mh = Progression::worldHydrogen(kind);
    FrontierPlan p;
    p.kind = kind;
    p.why = why;
    if (f.progress.copies(kind) <= 0) {
        // Its blueprint first, at the realm's Citadel here, when the wallet
        // holds both it and the building.
        const ShopPrice price = f.progress.blueprintPrice(kind);
        if (price.ore + ore + WorldFrontier::reserveOre > f.wallet.ore ||
            price.hydrogen + mh + WorldFrontier::reserveHydrogen > f.wallet.hydrogen)
            return std::nullopt;
        const auto citadel = realmCitadel();
        if (!citadel || citadel->first != r) return std::nullopt;
        p.act = Act::buy;
        p.target = citadel->second;
        p.at = regions_[r]->simulation().state.structure(citadel->second)->position;
        return p;
    }
    if (ore > f.wallet.ore || mh > f.wallet.hydrogen) return std::nullopt;
    p.act = Act::build;
    p.at = *frontierSpot(f, kind, near);
    return p;
}

FrontierPlan WorldMap::minerPlan(const FrontierPilot& f) const {
    const size_t r = f.region;
    const Vec2 at = frontierAt(f);
    const GameState& st = regions_[r]->simulation().state;
    if (f.cargo == 0) {
        // A Derrick on a well by its field; a Hab Dome by a field far from
        // every depot.
        if (st.wells) {
            std::optional<Vec2> well;
            for (const auto& w : *st.wells) {
                if (w.remaining <= 0 || distance(w.position, at) > 24) continue;
                bool taken = false;
                for (const Structure& b : st.structures) taken = taken || (b.kind == StructureKind::derrick && distance(b.position, w.position) < 2);
                if (!taken && (!well || distance(w.position, at) < distance(*well, at))) well = w.position;
            }
            if (well)
                if (auto p = buildPlan(f, StructureKind::derrick, *well, "a Derrick on the well by its field")) return *p;
        }
        if (const auto m = minePlan(f)) {
            const auto d = frontierDepot(r, m->at);
            if (d && distance(st.structure(*d)->position, m->at) > WorldFrontier::depotFar)
                if (auto p = buildPlan(f, StructureKind::habDome, m->at, "a Hab Dome by its field")) return *p;
        }
    }
    if (auto m = minePlan(f)) return *m;
    // Nothing to mine here: to the realm's Citadel's region.
    FrontierPlan p;
    if (const auto c = realmCitadel(); c && c->first != r) {
        p.act = Act::walk;
        p.region = c->first;
        p.why = "to the Citadel's fields";
    } else {
        p.why = "no field with a depot";
    }
    return p;
}

FrontierPlan WorldMap::builderPlan(const FrontierPilot& f) const {
    const size_t r = f.region;
    const GameState& st = regions_[r]->simulation().state;
    if (f.cargo == 0) {
        if (const auto field = raidedField(r))
            if (auto p = buildPlan(f, StructureKind::gatling, *field, "a Gatling Gun by a raided field")) return *p;
        // A Landing Pad where the realm has a Citadel or a canyon, and none.
        const bool worth = depot(r, frontierAt(f)).has_value() || !world_->regions[r].built.zones.empty();
        if (worth && limitCount(StructureKind::landingPad, r) == 0) {
            Vec2 near = frontierAt(f);
            if (const auto c = realmCitadel(); c && c->first == r) near = st.structure(c->second)->position;
            if (auto p = buildPlan(f, StructureKind::landingPad, near, "a Landing Pad for the Dropship")) return *p;
        }
        if (const auto m = minePlan(f)) {
            const auto d = frontierDepot(r, m->at);
            if (d && distance(st.structure(*d)->position, m->at) > WorldFrontier::depotFar)
                if (auto p = buildPlan(f, StructureKind::habDome, m->at, "a Hab Dome where the haul is long")) return *p;
        }
    }
    if (auto m = minePlan(f)) return *m;
    FrontierPlan p;
    if (const auto c = realmCitadel(); c && c->first != r) {
        p.act = Act::walk;
        p.region = c->first;
        p.why = "to the realm's heart";
    }
    return p;
}

FrontierPlan WorldMap::defenderPlan(const FrontierPilot& f) const {
    const size_t r = f.region;
    const Simulation& sim = regions_[r]->simulation();
    const GameState& st = sim.state;
    const Vec2 at = frontierAt(f);
    FrontierPlan p;
    // A wave a human holds: there, and at its bugs.
    if (const Canyon* c = frontierCanyon(f)) {
        if (c->region != r) {
            p.act = Act::walk;
            p.region = c->region;
            p.why = "to " + c->name + ": a wave is out";
            return p;
        }
        std::optional<int64_t> best;
        double d = 1e18;
        for (const int64_t b : c->bugs) {
            const auto u = st.unit(b);
            if (!u || u->hp <= 0 || !ground(*u) || u->kind == UnitKind::matriarch) continue;
            if (distance(u->position, at) < d) {
                d = distance(u->position, at);
                best = b;
            }
        }
        if (best) {
            p.act = Act::fight;
            p.target = *best;
            p.at = st.unit(*best)->position;
            p.why = "holding " + c->name;
            return p;
        }
        p.act = Act::patrol;
        p.at = c->middle;
        p.why = "on the prize at " + c->name;
        return p;
    }
    if (const auto foe = frontierFoe(f)) {
        p.act = Act::fight;
        p.target = *foe;
        p.at = st.unit(*foe)->position;
        p.why = "bugs at the realm's buildings";
        return p;
    }
    if (f.cargo > 0)
        if (auto m = minePlan(f)) return *m;
    // A building of the realm hurt: mend it.
    for (const Structure& b : st.structures)
        if (realm(b) && b.complete() && b.hp < Rules::hp(b.kind) * 0.9 && !keptOff(r, b.position)) {
            p.act = Act::repair;
            p.target = b.id;
            p.at = b.position;
            p.why = "mending " + Simulation::title(b.kind);
            return p;
        }
    // On patrol: from one building of the realm here to the next, a minute
    // each; half the day it drills by them for its keep.
    std::vector<const Structure*> beat;
    for (const Structure& b : st.structures)
        if (realm(b) && b.complete() && !keptOff(r, b.position)) beat.push_back(&b);
    const int64_t minute = int64_t(std::floor(clock_ / 60));
    if (!beat.empty() && minute % 2 == 0) {
        const Structure& b = *beat[size_t(minute / 2) % beat.size()];
        const double a = double(f.id) * 2.1 + double(minute) * 0.7;
        p.act = Act::patrol;
        p.at = b.position + Vec2(std::cos(a), std::sin(a)) * (Rules::radius(b.kind) + 4);
        p.why = "patrolling by the " + Simulation::title(b.kind);
        return p;
    }
    if (auto m = minePlan(f)) return *m;
    if (const auto c = realmCitadel(); c && c->first != r) {
        p.act = Act::walk;
        p.region = c->first;
        p.why = "to the realm's buildings";
    }
    return p;
}

FrontierPlan WorldMap::wandererPlan(const FrontierPilot& f) const {
    const size_t r = f.region;
    const Vec2 at = frontierAt(f);
    FrontierPlan p;
    if (f.cargo > 0)
        if (auto m = minePlan(f)) return *m;
    // A mission taken: to its region, then its middle.
    if (f.missionRegion) {
        p.act = *f.missionRegion == r ? Act::mission : Act::walk;
        p.region = *f.missionRegion;
        p.at = world_->regions[*f.missionRegion].built.ground(world_->regions[*f.missionRegion].built.extent() * 0.5);
        p.why = "a Citadel survey of " + world_->regions[*f.missionRegion].file.name;
        return p;
    }
    // A site here it has not opened, at its level.
    const region::Built& b = world_->regions[r].built;
    std::optional<size_t> site;
    for (size_t i = 0; i < b.sites.size(); i++) {
        if (f.progress.sites.count(siteId(r, i)) || keptOff(r, b.sites[i].at)) continue;
        if (b.sites[i].kind == SiteKind::vault && f.progress.level() < f.progress.floor(b.sites[i].blueprint)) continue;
        if (!site || distance(b.sites[i].at, at) < distance(b.sites[*site].at, at)) site = i;
    }
    if (site) {
        p.act = Act::open;
        p.target = int64_t(*site);
        p.at = b.sites[*site].at;
        p.why = "opening the " + b.sites[*site].name;
        return p;
    }
    // A Citadel survey from the board: a region next door, taken at the
    // realm's Citadel.
    const auto citadel = realmCitadel();
    std::vector<size_t> next;
    for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west})
        if (const auto nb = neighbour(r, e); nb && frontierStepToward(r, *nb, at)) next.push_back(*nb);
    if (citadel && citadel->first == r && !next.empty()) {
        // The least seen of them.
        std::sort(next.begin(), next.end(), [&](size_t x, size_t y) {
            const bool sx = f.progress.regions.count(world_->regions[x].file.id), sy = f.progress.regions.count(world_->regions[y].file.id);
            return sx != sy ? !sx : x < y;
        });
        p.act = Act::mission;
        p.region = next[0];
        p.target = citadel->second;
        p.at = regions_[r]->simulation().state.structure(citadel->second)->position;
        p.why = "to the Citadel's board";
        return p;
    }
    // On to a region it has not seen, by Dropship when both ends have a pad
    // and the wallet the fare; else on foot.
    std::optional<size_t> go;
    for (size_t k = 0; k < regions_.size(); k++)
        if (k != r && !f.progress.regions.count(world_->regions[k].file.id) && frontierStepToward(r, k, at)) {
            go = k;
            break;
        }
    if (!go && citadel && citadel->first != r) go = citadel->first;
    if (!go && !next.empty()) go = next[size_t(f.id + uint32_t(clock_ / 600)) % next.size()];
    if (!go) return p;
    p.region = *go;
    p.act = Act::walk;
    p.why = "on to " + world_->regions[*go].file.name;
    bool padHere = false, padThere = false;
    for (const LandingPad& l : landingPads()) {
        padHere = padHere || l.region == r;
        padThere = padThere || l.region == *go;
    }
    const int64_t steps = std::abs(world_->regions[r].column - world_->regions[*go].column) + std::abs(world_->regions[r].row - world_->regions[*go].row);
    if (padHere && padThere && steps >= 2 && f.wallet.hydrogen >= ::ac::Flight::price(steps, false, false) + WorldFrontier::reserveHydrogen) {
        p.act = Act::fly;
        p.why = "flying to " + world_->regions[*go].file.name;
    }
    return p;
}

FrontierPlan WorldMap::frontierDecide(const FrontierPilot& f) const {
    if (!f.arrived || f.travelTo || clock_ < f.downUntil) return {};
    switch (f.habit) {
    case Habit::miner: return minerPlan(f);
    case Habit::builder: return builderPlan(f);
    case Habit::defender: return defenderPlan(f);
    case Habit::wanderer: return wandererPlan(f);
    }
    return {};
}

// MARK: - The rules it acts by

std::optional<std::string> WorldMap::frontierBuy(uint32_t id, StructureKind kind) {
    FrontierPilot* f = frontierPilot(id);
    if (!f || !f->arrived || f->travelTo) return "Not here";
    if (Progression::blueprintFloor(kind) <= 0) return "No " + Simulation::title(kind) + " blueprint in this world";
    const auto citadel = realmCitadel();
    if (!citadel || citadel->first != f->region || !frontierBeside(*f, citadel->second))
        return std::string("Buy at the realm's Citadel: stand where you unload");
    const ShopPrice price = f->progress.blueprintPrice(kind);
    if (price.ore > f->wallet.ore) return "Not enough ore: " + std::to_string(price.ore) + ", you have " + std::to_string(f->wallet.ore);
    if (price.hydrogen > f->wallet.hydrogen)
        return "Not enough MH: " + std::to_string(price.hydrogen) + ", you have " + std::to_string(f->wallet.hydrogen);
    f->wallet.ore -= price.ore;
    f->wallet.hydrogen -= price.hydrogen;
    f->progress.giveBlueprint(kind);
    f->progress.bought[kind] += 1;
    return std::nullopt;
}

std::optional<std::string> WorldMap::frontierBuild(uint32_t id, StructureKind kind, Vec2 at) {
    FrontierPilot* f = frontierPilot(id);
    if (!f || !f->arrived || f->travelTo || clock_ < f->downUntil) return "Not here";
    const size_t r = f->region;
    // As a pilot: its blueprint, its floor, the realm's limit.
    if (auto why = f->progress.buildRefusal(kind, limitCount(kind, r))) return why;
    Simulation& sim = regions_[r]->simulation();
    const auto spot = sim.snap(kind, at);
    if (!spot) return std::string("No spot there");
    if (region::touchesPadPlate(world_->regions[r].built.pads, *spot, Rules::radius(kind))) return std::string(region::padPlateRefusal);
    if (auto why = frontierCap(r, kind, *spot)) return why;
    // Within reach of its Prospector, as a pilot's hand-build.
    if (distance(frontierAt(*f), *spot) > Region::buildReach(kind) + Region::buildSlack) return std::string("Too far from the spot");
    if (auto why = sim.placementRefusal(kind, *spot)) return why;
    const int64_t ore = Progression::worldOre(kind), mh = Progression::worldHydrogen(kind);
    if (ore > f->wallet.ore) return "Not enough ore: " + std::to_string(ore) + ", you have " + std::to_string(f->wallet.ore);
    if (mh > f->wallet.hydrogen) return "Not enough MH: " + std::to_string(mh) + ", you have " + std::to_string(f->wallet.hydrogen);
    f->wallet.ore -= ore;
    f->wallet.hydrogen -= mh;
    f->progress.useBlueprint(kind);
    Structure s(sim.state.nextID, kind, Pilot::player, *spot, Rules::buildTime(kind), f->unit.value_or(-1));
    sim.state.nextID += 1;
    s.hp = Rules::hp(kind) * 0.1;
    sim.state.structures.push_back(s);
    sim.refreshNav();
    builtBy_[r][s.id] = f->key;
    f->building = s.id;
    if (Unit* u = frontierUnitOf(*f)) {
        // Off to weld it, as the core's Prospector finishing a building.
        for (auto& d : sim.state.patches)
            if (d.miner == u->id) d.miner = std::nullopt;
        u->mission = std::nullopt;
        u->task = Unit::Task::toBuild;
        u->order = std::nullopt;
        u->structure = s.id;
    }
    return std::nullopt;
}

std::optional<std::string> WorldMap::frontierFly(uint32_t id, size_t to) {
    FrontierPilot* f = frontierPilot(id);
    if (!f || !f->arrived || f->travelTo || clock_ < f->downUntil) return "Not here";
    if (to >= regions_.size() || to == f->region) return std::string("No such region");
    // From a Landing Pad of the realm it stands on, to one in `to`.
    std::optional<LandingPad> from, there;
    for (const LandingPad& l : landingPads()) {
        if (l.region == f->region && distance(l.at, frontierAt(*f)) <= params_.padReach) from = l;
        if (l.region == to && !there) there = l;
    }
    if (!from) return std::string("Call the Dropship at a Landing Pad");
    if (!there) return "No Landing Pad in " + world_->regions[to].file.name;
    const int64_t steps = std::abs(world_->regions[f->region].column - world_->regions[to].column) +
                          std::abs(world_->regions[f->region].row - world_->regions[to].row);
    const int64_t price = ::ac::Flight::price(steps, from->kind == StructureKind::spacedock, false);
    if (auto why = ::ac::Flight::refusal(price, f->wallet.hydrogen)) return why;
    f->wallet.hydrogen -= price;
    // Aboard: out of the region until the touch down.
    frontierUnit(*f, false);
    f->travelTo = to;
    f->travelAt = there->at;
    f->aboard = true;
    f->until = clock_ + Ferry::plan(steps).total();
    f->doing = Doing::flying;
    return std::nullopt;
}

std::optional<std::string> WorldMap::frontierMission(uint32_t id, size_t to) {
    FrontierPilot* f = frontierPilot(id);
    if (!f || !f->arrived) return "Not here";
    if (f->missionRegion) return std::string("A mission is taken already");
    const auto citadel = realmCitadel();
    if (!citadel || citadel->first != f->region || !frontierBeside(*f, citadel->second))
        return std::string("Take missions at the realm's Citadel: stand where you unload");
    if (to >= regions_.size()) return std::string("No such region");
    const int64_t steps = std::abs(world_->regions[start_].column - world_->regions[to].column) +
                          std::abs(world_->regions[start_].row - world_->regions[to].row);
    f->missionRegion = to;
    f->missionTier = WorldMissions::tierAt(steps);
    return std::nullopt;
}

// MARK: - Live and booked

void WorldMap::frontierUnit(FrontierPilot& f, bool live) {
    if (!live) {
        if (!f.unit) return;
        if (const Unit* u = frontierUnitOf(f)) {
            f.at = u->position;
            f.cargo = u->carrying;
            f.hydrogen = u->hydrogen == true;
        }
        Simulation& sim = regions_[f.region]->simulation();
        for (auto& d : sim.state.patches)
            if (d.miner == *f.unit) d.miner = std::nullopt;
        // A building it welded goes on rising by the book.
        regions_[f.region]->release(*f.unit);
        f.unit = std::nullopt;
        return;
    }
    if (f.unit && frontierUnitOf(f)) return;
    Simulation& sim = regions_[f.region]->simulation();
    Unit v = sim.freshUnit(UnitKind::prospector, 0, Pilot::player, f.at, 0);
    v.position = openGround(sim, v, f.at, 6);
    v.carrying = f.cargo;
    v.hydrogen = f.cargo > 0 && f.hydrogen ? std::optional<bool>(true) : std::nullopt;
    v.timer = 1;
    if (f.hp > 0) v.hp = std::min(v.hp, f.hp);
    f.unit = regions_[f.region]->admit(v);
    f.hp = v.hp;
    if (f.building)
        if (Unit* u = frontierUnitOf(f)) {
            u->task = Unit::Task::toBuild;
            u->structure = *f.building;
        }
}

void WorldMap::frontierEnter(FrontierPilot& f, size_t to, Vec2 at) {
    frontierUnit(f, false);
    f.region = to;
    f.at = at;
    f.travelTo = std::nullopt;
    f.aboard = false;
    f.building = std::nullopt;
    f.opening = std::nullopt;
    f.progress.regions.insert(world_->regions[to].file.id);
    f.thought = -1e9;
}

void WorldMap::frontierUnload(FrontierPilot& f, int64_t amount, bool hydrogen) {
    if (amount <= 0) return;
    // The Citadel's tax (C1), as on any pilot's unload; its XP (WorldXp).
    const int64_t tax = Progression::realmTax(amount);
    (hydrogen ? realm_.hydrogen : realm_.ore) += tax;
    (hydrogen ? f.taxed.hydrogen : f.taxed.ore) += tax;
    (hydrogen ? f.wallet.hydrogen : f.wallet.ore) += amount - tax;
    frontierEarn(f, double(amount) * (hydrogen ? WorldXp::hydrogen : WorldXp::ore));
    if (hydrogen) f.trips = 0;
    else f.trips += 1;
}

void WorldMap::frontierHit(size_t r, const Simulation::BugHit& h) {
    for (FrontierPilot& f : frontier_) {
        if (!f.arrived || f.region != r) continue;
        bool mine = f.unit == h.shooter;
        if (!mine && r < builtBy_.size())
            if (const auto by = builtBy_[r].find(h.shooter); by != builtBy_[r].end()) mine = by->second == f.key;
        if (!mine) continue;
        const double share = h.hp / std::max(1.0, Rules::hp(h.kind));
        frontierEarn(f, Brood::xp(h.kind) * share * (f.unit == h.shooter ? 1.0 : defenceXpShare));
        if (h.kill) f.kills += 1;
        for (Canyon& c : canyons_)
            if (c.region == r && c.bugs.count(h.bug) && h.hp > 0) c.frontierDamage[f.id] += h.hp;
    }
}

void WorldMap::frontierEvents(FrontierPilot& f) {
    Region& reg = *regions_[f.region];
    Simulation& sim = reg.simulation();
    if (f.unit) {
        for (const GameEvent& e : reg.lastEvents()) {
            const auto* d = e.as<GameEvent::Deposited>();
            if (!d || d->unit != *f.unit || d->amount <= 0) continue;
            // Out of the region's bank (the human's wallet there), into its own.
            Player& side = sim.state.players[0];
            (d->hydrogen ? side.hydrogen : side.ore) -= d->amount;
            frontierUnload(f, d->amount, d->hydrogen);
            f.cargo = 0;
        }
        const Unit* u = frontierUnitOf(f);
        if (!u) {
            // It fell: back at a depot after the respawn delay, its load lost.
            f.unit = std::nullopt;
            f.cargo = 0;
            f.building = std::nullopt;
            f.downUntil = clock_ + params_.respawnDelay;
            f.doing = Doing::down;
            f.hp = 0;
            frontierRadio(f, "help", world_->regions[f.region].file.name);
            if (const auto d = frontierDepot(f.region, f.at)) f.at = sim.state.structure(*d)->position;
            return;
        }
        f.at = u->position;
        f.cargo = u->carrying;
        f.hydrogen = u->hydrogen == true;
        // Badly hurt: a cry on the radio, now and then.
        if (u->hp < f.hp && u->hp < Rules::hp(UnitKind::prospector) * WorldFrontier::flee && clock_ - f.lastCry > 120) {
            f.lastCry = clock_;
            frontierRadio(f, "help", world_->regions[f.region].file.name);
        }
        f.hp = u->hp;
    }
    // Its building finished: a deed the world hears.
    if (f.building) {
        const auto s = sim.state.structure(*f.building);
        if (!s) {
            f.building = std::nullopt;
        } else if (s->complete()) {
            f.building = std::nullopt;
            f.built += 1;
            frontierEarn(f, WorldXp::feat(WorldXp::built(s->kind)) * (f.progress.feats.insert(WorldXp::built(s->kind)).second ? 1 : 0));
            frontierDeed(f, f.callsign + " built " + kindWord(s->kind) + " in " + world_->regions[f.region].file.name);
        }
    }
}

void WorldMap::frontierBookWork(FrontierPilot& f, double dt) {
    // Booked welding: its building rises as the core's welding would.
    if (f.building) {
        Simulation& sim = regions_[f.region]->simulation();
        for (Structure& s : sim.state.structures) {
            if (s.id != *f.building || !s.buildLeft) continue;
            const double put = std::min(dt, *s.buildLeft);
            s.buildLeft = *s.buildLeft - put;
            s.hp = std::min(Rules::hp(s.kind), s.hp + 0.9 * Rules::hp(s.kind) * put / Rules::buildTime(s.kind));
            if (*s.buildLeft <= 1e-9) s.buildLeft = std::nullopt;
        }
    }
}

void WorldMap::frontierAct(FrontierPilot& f, double dt) {
    const size_t r = f.region;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    Unit* u = frontierUnitOf(f);
    const Vec2 pos = frontierAt(f);
    const FrontierPlan& p = f.plan;
    auto clearDrill = [&] {
        if (!u) return;
        for (auto& d : st.patches)
            if (d.miner == u->id) d.miner = std::nullopt;
    };
    // To `to` within `stop`: true when there. Live, an errand of the core's
    // to that point; booked, a straight walk at a Prospector's speed.
    auto moveTo = [&](Vec2 to, double stop) {
        if (distance(pos, to) <= stop) {
            if (u && u->task == Unit::Task::errand) {
                u->task = Unit::Task::idle;
                u->mission = std::nullopt;
                u->timer = 1;
            }
            return true;
        }
        if (u) {
            if (u->task != Unit::Task::errand || !u->mission || !u->mission->is<Mission::Raid>() ||
                distance(u->mission->as<Mission::Raid>()->at, to) > 0.5) {
                clearDrill();
                u->patch = std::nullopt;
                u->task = Unit::Task::errand;
                u->mission = Mission{Mission::Raid{to}};
            }
        } else {
            const double step = std::min(distance(pos, to), Rules::prospectorSpeed * dt);
            f.at = pos + normalize(to - pos) * step;
        }
        return false;
    };
    auto rest = [&] {
        if (u && (u->task == Unit::Task::errand || u->task == Unit::Task::idle)) {
            u->task = Unit::Task::idle;
            u->mission = std::nullopt;
            u->timer = 1;
        }
    };
    // A building it welds goes on (live: the core welds while it stands by).
    if (f.building) {
        const auto s = st.structure(*f.building);
        if (s && s->buildLeft) {
            f.doing = Doing::building;
            if (u && u->task != Unit::Task::building && u->task != Unit::Task::toBuild) {
                u->mission = std::nullopt;
                u->task = Unit::Task::toBuild;
                u->structure = *f.building;
            }
            if (!u) frontierBookWork(f, dt);
            return;
        }
    }
    switch (p.act) {
    case Act::rest:
        f.doing = Doing::resting;
        rest();
        break;
    case Act::mine: {
        f.doing = Doing::mining;
        if (u) {
            const bool busy = u->task == Unit::Task::toPatch || u->task == Unit::Task::mining || u->task == Unit::Task::waiting ||
                              u->task == Unit::Task::toBase || u->task == Unit::Task::depositing;
            if (u->task == Unit::Task::toBase || u->task == Unit::Task::depositing) f.doing = Doing::hauling;
            if (!busy || (u->task != Unit::Task::toBase && u->task != Unit::Task::depositing && u->patch != p.target)) {
                clearDrill();
                u->mission = std::nullopt;
                u->patch = p.target;
                u->task = Unit::Task::toPatch;
                u->timer = 0;
            }
            break;
        }
        // Booked: on the deposit, loads at an unskilled Prospector's pace.
        if (!moveTo(p.at, Rules::mineRange + 0.5)) break;
        if (!frontierDepot(r, p.at)) break;
        f.owed += WorldFrontier::bookRate() * dt;
        OreDeposit& d = st.patches[size_t(p.target)];
        while (f.owed >= double(Rules::carry) && d.remaining > 0) {
            const int64_t take = std::min<int64_t>(Rules::carry, d.remaining);
            d.remaining -= take;
            if (d.remaining <= 0) d.depletedAt = st.time;
            f.owed -= double(take);
            frontierUnload(f, take, false);
        }
        break;
    }
    case Act::haul: {
        f.doing = Doing::hauling;
        const auto b = st.structure(p.target);
        if (!b) break;
        if (u && b->kind == StructureKind::citadel) {
            // The core's own haul home, and its unload.
            if (u->task != Unit::Task::toBase && u->task != Unit::Task::depositing) {
                u->mission = std::nullopt;
                u->task = Unit::Task::toBase;
                u->goal = b->position;
            }
            break;
        }
        // A Hab Dome (the core's Prospector hauls to Citadels only), or booked.
        const Vec2 out = normalize(pos - b->position);
        if (!moveTo(b->position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(b->kind) + 0.6), 0.8)) break;
        if (!frontierBeside(f, b->id)) break;
        frontierUnload(f, f.cargo, f.hydrogen);
        f.cargo = 0;
        f.hydrogen = false;
        if (u) {
            u->carrying = 0;
            u->hydrogen = std::nullopt;
        }
        break;
    }
    case Act::pump: {
        f.doing = Doing::pumping;
        if (u) {
            if (u->task != Unit::Task::toDerrick && u->task != Unit::Task::inDerrick && u->task != Unit::Task::toBase &&
                u->task != Unit::Task::depositing) {
                clearDrill();
                u->mission = std::nullopt;
                u->task = Unit::Task::toDerrick;
                u->structure = p.target;
            }
            break;
        }
        // Booked: a tank's worth from the Derrick's well.
        if (!moveTo(p.at, Rules::radius(StructureKind::derrick) + 1)) break;
        f.owed += WorldFrontier::bookRate() * dt;
        if (f.owed < double(Rules::carry) || !st.wells) break;
        for (auto& w : *st.wells)
            if (distance(w.position, p.at) < 3 && w.remaining > 0) {
                const int64_t take = std::min<int64_t>(Rules::carry, w.remaining);
                w.remaining -= take;
                f.owed -= double(take);
                frontierUnload(f, take, true);
                break;
            }
        break;
    }
    case Act::buy: {
        f.doing = Doing::buying;
        const auto b = st.structure(p.target);
        if (!b) break;
        const Vec2 out = normalize(pos - b->position);
        if (!moveTo(b->position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(b->kind) + 0.6), 0.8)) break;
        if (!frontierBuy(f.id, p.kind)) f.thought = -1e9;
        break;
    }
    case Act::build: {
        f.doing = Doing::building;
        if (!moveTo(p.at, Region::buildReach(p.kind))) break;
        if (const auto why = frontierBuild(f.id, p.kind, p.at)) {
            f.plan = {};
            break;
        }
        break;
    }
    case Act::repair:
        f.doing = Doing::repairing;
        if (u) {
            if (u->task != Unit::Task::repairing) {
                clearDrill();
                u->mission = std::nullopt;
                u->task = Unit::Task::repairing;
                u->structure = p.target;
            }
        } else if (moveTo(p.at, 4)) {
            for (Structure& s : st.structures)
                if (s.id == p.target) s.hp = std::min(Rules::hp(s.kind), s.hp + Rules::hp(s.kind) / Rules::buildTime(s.kind) * dt);
        }
        break;
    case Act::fight:
        f.doing = Doing::fighting;
        if (u) {
            const auto bug = st.unit(p.target);
            if (!bug) break;
            clearDrill();
            u->patch = std::nullopt;
            u->task = Unit::Task::errand;
            u->mission = Mission{Mission::Hunt{bug->position}};
        }
        break;
    case Act::patrol:
        f.doing = Doing::patrolling;
        if (moveTo(p.at, 1.5)) rest();
        break;
    case Act::open: {
        f.doing = Doing::opening;
        const region::Built& b = world_->regions[r].built;
        if (p.target < 0 || size_t(p.target) >= b.sites.size()) break;
        const region::Site& s = b.sites[size_t(p.target)];
        if (!moveTo(s.at, WorldSites::reach - 0.3)) {
            f.opening = std::nullopt;
            break;
        }
        if (f.opening != size_t(p.target)) {
            f.opening = size_t(p.target);
            f.openSince = clock_;
        }
        if (clock_ - f.openSince + 1e-9 < WorldSites::needs(s.kind)) break;
        // Opened: its blueprint copy, its XP, as a pilot's find.
        f.opening = std::nullopt;
        if (!f.progress.sites.insert(siteId(r, size_t(p.target))).second) break;
        f.progress.giveBlueprint(s.blueprint);
        frontierEarn(f, WorldXp::blueprintPerFloor * double(Progression::blueprintFloor(s.blueprint)));
        frontierDeed(f, f.callsign + " opened the " + s.name + " in " + world_->regions[r].file.name);
        break;
    }
    case Act::walk: {
        f.doing = Doing::walking;
        const auto step = frontierStepToward(r, p.region, pos);
        if (!step) break;
        if (!moveTo(step->second, WorldFrontier::edgeReach)) break;
        // Through the opening: into the next region at the opening's far side.
        const region::Built& here = world_->regions[r].built;
        const region::Built& there = world_->regions[step->first].built;
        const Vec2 local = here.regionAt(step->second);
        Vec2 in = local;
        for (const region::Edge e : {region::Edge::north, region::Edge::east, region::Edge::south, region::Edge::west})
            if (neighbour(r, e) == step->first) {
                const double along = e == region::Edge::north || e == region::Edge::south ? local.x : local.y;
                in = there.edgePoint(opposite(e), along, params_.arriveDepth + 1);
            }
        frontierUnit(f, false);
        f.travelTo = step->first;
        f.travelAt = there.ground(in);
        f.until = clock_ + 1.0;
        f.aboard = false;
        break;
    }
    case Act::fly: {
        f.doing = Doing::walking;
        std::optional<LandingPad> from;
        for (const LandingPad& l : landingPads())
            if (l.region == r && (!from || distance(l.at, pos) < distance(from->at, pos))) from = l;
        if (!from) break;
        if (!moveTo(from->at, params_.padReach - 0.5)) break;
        frontierFly(f.id, p.region);
        break;
    }
    case Act::mission: {
        if (!f.missionRegion) {
            // To the board at the Citadel.
            f.doing = Doing::buying;
            const auto b = st.structure(p.target);
            if (!b) break;
            const Vec2 out = normalize(pos - b->position);
            if (!moveTo(b->position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(b->kind) + 0.6), 0.8)) break;
            frontierMission(f.id, p.region);
            break;
        }
        f.doing = Doing::walking;
        if (!moveTo(p.at, 4)) break;
        // The survey done: paid as a Citadel mission of its tier.
        const int64_t level = f.progress.level();
        f.wallet.ore += WorldMissions::ore(f.missionTier, level);
        f.wallet.hydrogen += WorldMissions::hydrogen(f.missionTier, level);
        frontierEarn(f, WorldMissions::xp(f.missionTier, level));
        frontierDeed(f, f.callsign + " finished a Citadel survey of " + world_->regions[r].file.name);
        f.missionRegion = std::nullopt;
        break;
    }
    }
}

void WorldMap::frontierThink(FrontierPilot& f, double dt) {
    // Its plan again every think, or at once when the last one is done.
    if (clock_ - f.thought >= WorldFrontier::think - 1e-9) {
        f.thought = clock_;
        f.plan = frontierDecide(f);
    }
    frontierAct(f, dt);
}

// MARK: - Each step

void WorldMap::frontierStep(double dt) {
    if (!world_) return;
    frontierRoster();
    if (frontier_.empty()) return;
    // The fields the Brood lingered at (a builder's Gatling Gun goes there).
    for (size_t r = 0; r < brood_.size() && r < regions_.size(); r++)
        for (const BroodGroup& g : brood_[r].groups) {
            if (g.poi != Poi::deposit || g.nest) continue;
            const auto& fl = fields(r);
            for (size_t k = 0; k < fl.size(); k++)
                if (std::find(fl[k].begin(), fl[k].end(), size_t(g.key)) != fl[k].end()) fieldRaided_[{r, int64_t(k)}] = clock_;
        }
    for (FrontierPilot& f : frontier_) {
        if (!f.arrived) {
            if (clock_ < f.comes) continue;
            // It comes by Dropship: to the realm's Citadel's region (a
            // wanderer anywhere), at a Landing Pad there, else the region's
            // pad; never the start pad while a human is on the chain.
            size_t r = start_;
            if (const auto c = realmCitadel()) r = c->first;
            if (f.habit == Habit::wanderer) r = size_t(frontierDice_() % regions_.size());
            std::optional<Vec2> at;
            for (const LandingPad& l : landingPads())
                if (l.region == r && !keptOff(r, l.at)) at = l.at;
            for (const region::Place& pad : world_->regions[r].built.pads)
                if (!at && !keptOff(r, pad.at)) at = pad.at;
            if (!at) {
                // Next door, where the chain is not.
                for (const region::Edge e : {region::Edge::east, region::Edge::west, region::Edge::north, region::Edge::south})
                    if (const auto nb = neighbour(r, e); nb && !at && !world_->regions[*nb].built.pads.empty()) {
                        r = *nb;
                        at = world_->regions[r].built.pads[0].at;
                    }
            }
            if (!at) continue;
            f.arrived = true;
            const double a = double(f.id) * 1.9;
            frontierEnter(f, r, *at + Vec2(std::cos(a), std::sin(a)) * 3.0);
            f.doing = Doing::coming;
            const std::string where = world_->regions[r].file.name;
            frontierRadio(f, "hello", where, true);
            for (auto& [id, p] : pilots_)
                p.news.push_back(News{NewsKind::frontier, "come", 0, int64_t(f.id),
                                      f.callsign + " landed at " + where + "  ·  " + net::title(f.habit) + ", level " +
                                          std::to_string(f.progress.level())});
            continue;
        }
        // On its way: there at `until`.
        if (f.travelTo) {
            f.doing = f.aboard ? Doing::flying : Doing::walking;
            if (clock_ < f.until) continue;
            const bool flew = f.aboard;
            frontierEnter(f, *f.travelTo, f.travelAt);
            if (flew) f.doing = Doing::coming;
        }
        if (clock_ < f.downUntil) {
            f.doing = Doing::down;
            continue;
        }
        if (f.doing == Doing::down) {
            // Back on its feet at the depot it was taken to.
            f.doing = Doing::resting;
            f.hp = 0;
        }
        frontierUnit(f, frontierLive(f));
        frontierEvents(f);
        if (clock_ < f.downUntil) continue;
        frontierThink(f, dt);
    }
    frontierChatter();
    sky_.frontier = skyFrontier();
}

void WorldMap::frontierIdle(double seconds) {
    if (!world_ || frontier_.empty()) return;
    // Booked, in steps of a think, as the step's work would be.
    double left = seconds;
    while (left > 1e-9) {
        const double dt = std::min(left, WorldFrontier::think);
        left -= dt;
        for (FrontierPilot& f : frontier_) {
            if (!f.arrived || f.travelTo || clock_ < f.downUntil) continue;
            frontierUnit(f, false);
            f.thought = -1e9;
            frontierThink(f, dt);
            frontierEvents(f);
        }
    }
    sky_.frontier = skyFrontier();
}

// MARK: - The radio and the deeds

void WorldMap::frontierRadio(const FrontierPilot& f, const std::string& kind, const std::string& what, bool force) {
    if (!force && clock_ - radioAt_ < WorldFrontier::radioEvery) return;
    const auto& pool = lines(kind);
    if (pool.empty()) return;
    radioAt_ = clock_;
    const std::string line = fill(pool[size_t(frontierDice_() % pool.size())], f.callsign, what);
    radioLog_.push_back(line);
    for (auto& [id, p] : pilots_) p.news.push_back(News{NewsKind::frontier, "radio", 0, int64_t(f.id), line});
}

void WorldMap::frontierDeed(const FrontierPilot& f, const std::string& text) {
    radioLog_.push_back(text);
    for (auto& [id, p] : pilots_) p.news.push_back(News{NewsKind::frontier, "deed", 0, int64_t(f.id), text});
}

void WorldMap::frontierChatter() {
    const FrontierPilot* any = nullptr;
    for (const FrontierPilot& f : frontier_)
        if (f.arrived && !f.travelTo && clock_ >= f.downUntil) {
            any = &f;
            break;
        }
    if (!any || pilots_.empty()) return;
    // A raid told: the frontier pilot nearest it warns.
    for (const net::Sky::Raid& x : raids_) {
        if (!raidsHeard_.insert(x.id).second) continue;
        const FrontierPilot* by = any;
        for (const FrontierPilot& f : frontier_)
            if (f.arrived && f.region == size_t(x.region)) by = &f;
        frontierRadio(*by, "raid", world_->regions[size_t(x.region)].file.name);
    }
    // A wave beaten: a cheer from one who fought it, or anyone.
    for (const Canyon& c : canyons_) {
        if (!c.out) continue;
        wavesHeard_.erase({c.region, c.wave});
    }
    for (const Canyon& c : canyons_)
        if (!c.out && c.wave > 1 && c.state == CanyonState::running && wavesHeard_.insert({c.region, c.wave - 1}).second &&
            clock_ - c.next < WorldCanyon::breakFor - 1) {
            const FrontierPilot* by = any;
            for (const FrontierPilot& f : frontier_)
                if (f.arrived && f.region == c.region) by = &f;
            frontierRadio(*by, "cheer", c.name, true);
        }
    // Now and then, a word.
    if (clock_ - chatterAt_ >= WorldFrontier::chatterEvery) {
        chatterAt_ = clock_;
        const FrontierPilot& f = frontier_[size_t(frontierDice_() % frontier_.size())];
        if (f.arrived && !f.travelTo) frontierRadio(f, "chatter", world_->regions[f.region].file.name);
    }
}

// MARK: - Told

std::vector<net::Sky::Frontier> WorldMap::skyFrontier() const {
    std::vector<net::Sky::Frontier> out;
    for (const FrontierPilot& f : frontier_) {
        if (!f.arrived) continue;
        net::Sky::Frontier x;
        x.id = f.id;
        if (!f.travelTo) {
            x.region = int64_t(f.region);
            x.at = world_->regions[f.region].built.regionAt(frontierAt(f));
        }
        x.unit = f.unit && frontierUnitOf(f) ? *f.unit : 0;
        x.level = f.progress.level();
        x.doing = f.doing;
        out.push_back(x);
    }
    return out;
}

void WorldMap::frontierTold(net::ProgressMessage& m) const {
    for (const FrontierPilot& f : frontier_)
        if (f.arrived) m.frontier.push_back(net::ProgressMessage::FrontierPilot{f.id, f.callsign, f.habit, f.paint});
}

// MARK: - Saved

std::string WorldMap::encodeFrontier() const {
    if (frontier_.empty()) return "";
    json list = json::array();
    for (const FrontierPilot& f : frontier_) {
        json j{{"id", f.id},
               {"callsign", f.callsign},
               {"habit", int(f.habit)},
               {"paint", f.paint},
               {"progress", SessionStore::encode(f.progress, false)},
               {"ore", f.wallet.ore},
               {"mh", f.wallet.hydrogen},
               {"taxedOre", f.taxed.ore},
               {"taxedMh", f.taxed.hydrogen},
               {"arrived", f.arrived},
               {"comes", f.comes},
               {"region", world_->regions[f.travelTo ? *f.travelTo : f.region].file.id},
               {"at", {(f.travelTo ? f.travelAt : frontierAt(f)).x, (f.travelTo ? f.travelAt : frontierAt(f)).y}},
               {"cargo", f.cargo},
               {"mhCargo", f.hydrogen},
               {"down", f.downUntil},
               {"owed", f.owed},
               {"trips", f.trips},
               {"built", f.built},
               {"kills", f.kills}};
        if (f.unit && !f.travelTo) j["unit"] = *f.unit;
        if (f.building && !f.travelTo) j["building"] = *f.building;
        if (f.missionRegion) {
            j["mission"] = world_->regions[*f.missionRegion].file.id;
            j["tier"] = int(f.missionTier);
        }
        list.push_back(j);
    }
    return json{{"pilots", list}, {"radio", radioAt_}, {"chatter", chatterAt_}}.dump();
}

bool WorldMap::decodeFrontier(const std::string& text) {
    frontier_.clear();
    if (text.empty()) return true;
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object() || !root.contains("pilots") || !root["pilots"].is_array()) return false;
    for (const json& j : root["pilots"]) {
        if (!j.is_object()) return false;
        FrontierPilot f;
        f.id = j.value("id", uint32_t(0));
        f.callsign = j.value("callsign", std::string("Vesper"));
        f.key = "frontier:" + f.callsign;
        std::transform(f.key.begin(), f.key.end(), f.key.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        f.habit = Habit(std::clamp(j.value("habit", 0), 0, 3));
        f.paint = j.value("paint", 2);
        if (auto g = SessionStore::decode<ProspectorProgress>(j.value("progress", std::string()))) f.progress = *g;
        f.wallet = Wallet{j.value("ore", int64_t(0)), j.value("mh", int64_t(0))};
        f.taxed = Wallet{j.value("taxedOre", int64_t(0)), j.value("taxedMh", int64_t(0))};
        f.arrived = j.value("arrived", false);
        f.comes = j.value("comes", 0.0);
        const auto r = regionNamed(j.value("region", std::string()));
        f.region = r ? *r : start_;
        if (j.contains("at") && j["at"].is_array() && j["at"].size() == 2) f.at = Vec2(j["at"][0].get<double>(), j["at"][1].get<double>());
        if (r && j.contains("unit") && j["unit"].is_number_integer()) f.unit = j["unit"].get<int64_t>();
        if (r && j.contains("building") && j["building"].is_number_integer()) f.building = j["building"].get<int64_t>();
        f.cargo = j.value("cargo", int64_t(0));
        f.hydrogen = j.value("mhCargo", false);
        f.downUntil = j.value("down", 0.0);
        f.owed = j.value("owed", 0.0);
        f.trips = j.value("trips", int64_t(0));
        f.built = j.value("built", int64_t(0));
        f.kills = j.value("kills", int64_t(0));
        if (j.contains("mission"))
            if (const auto m = regionNamed(j.value("mission", std::string()))) {
                f.missionRegion = *m;
                f.missionTier = net::CitadelMission::Tier(std::clamp(j.value("tier", 0), 0, 2));
            }
        f.doing = f.arrived ? Doing::resting : Doing::coming;
        frontier_.push_back(std::move(f));
    }
    radioAt_ = root.value("radio", -1e9);
    chatterAt_ = root.value("chatter", -1e9);
    return true;
}

// MARK: - Dev and shots

std::string WorldMap::devFrontier(const std::string& line) {
    if (!world_) return "frontier: not a world";
    std::istringstream in(line);
    std::string word, what;
    in >> word;
    if (word == "frontier") in >> word;
    in >> what;
    PilotState* first = nullptr;
    for (auto& [id, p] : pilots_)
        if (!first) first = &p;
    if (!first || first->flight) return "frontier: no pilot on line";
    const auto mine = first->unit ? regions_[first->region]->simulation().state.unit(*first->unit) : std::nullopt;
    const Vec2 here = mine ? mine->position : Vec2(0, 0);
    const double heading = mine ? mine->heading : 0;
    // The roster now, come at once.
    if (frontier_.empty()) {
        const int64_t keep = params_.frontier;
        if (params_.frontier <= 0) params_.frontier = 5;
        first->firstStep = 0;
        frontierRoster();
        params_.frontier = keep;
    }
    // The one nearest the pilot: come ones in its region first.
    auto nearest = [&](std::optional<Habit> habit) -> FrontierPilot* {
        FrontierPilot* best = nullptr;
        auto rank = [&](const FrontierPilot& f) {
            return (f.arrived && !f.travelTo && f.region == first->region ? 0.0 : 1e9) + (f.arrived ? distance(frontierAt(f), here) : 1e8);
        };
        for (FrontierPilot& f : frontier_)
            if ((!habit || f.habit == *habit) && (!best || rank(f) < rank(*best))) best = &f;
        return best;
    };
    auto habitNamed = [](const std::string& w) -> std::optional<Habit> {
        for (const Habit h : {Habit::miner, Habit::builder, Habit::defender, Habit::wanderer}) {
            std::string t = net::title(h);
            std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (t == w) return h;
        }
        return std::nullopt;
    };
    if (word == "here") {
        // One of that habit beside the pilot, come and live.
        FrontierPilot* f = nearest(habitNamed(what));
        if (!f) return "frontier: none of that habit";
        f->arrived = true;
        f->travelTo = std::nullopt;
        f->downUntil = 0;
        double side = 0;
        std::string poor;
        in >> side >> poor;
        // `poor`: an empty wallet, so it only drills (no blueprint to buy,
        // nothing to build): the shots' miner at its drill.
        if (poor == "poor") f->wallet = Wallet{0, 0};
        const Vec2 ahead(std::cos(heading), std::sin(heading));
        const Vec2 left(-ahead.y, ahead.x);
        frontierEnter(*f, first->region, here + ahead * 6 + left * (side == 0 ? 4 : side));
        frontierUnit(*f, true);
        return "frontier: " + f->callsign + " (" + net::title(f->habit) + ") beside the pilot";
    }
    if (word == "build") {
        // The nearest one puts up a KIND ahead of it, its blueprint and price given.
        const auto kind = parse<StructureKind>(what);
        if (!kind) return "frontier: no such building";
        FrontierPilot* f = nearest(std::nullopt);
        if (!f || f->region != first->region) return "frontier: none here (frontier here builder first)";
        f->progress.giveBlueprint(*kind);
        if (f->progress.level() < f->progress.floor(*kind)) f->progress.xp = Progression::xp(f->progress.floor(*kind));
        f->wallet.ore += Progression::worldOre(*kind);
        f->wallet.hydrogen += Progression::worldHydrogen(*kind);
        const auto spot = frontierSpot(*f, *kind, frontierAt(*f));
        if (!spot) return "frontier: no spot for it";
        if (Unit* u = frontierUnitOf(*f)) u->position = openGround(regions_[f->region]->simulation(), *u, *spot + Vec2(Region::buildReach(*kind), 0), 4);
        else f->at = *spot;
        if (const auto why = frontierBuild(f->id, *kind, *spot)) return "frontier: " + *why;
        // Welded at once but the last second, so the deed comes in view.
        for (Structure& s : regions_[f->region]->simulation().state.structures)
            if (s.id == f->building) {
                s.buildLeft = 1.0;
                s.hp = Rules::hp(s.kind) * 0.9;
            }
        return "frontier: " + f->callsign + " builds " + kindWord(*kind);
    }
    if (word == "radio") {
        FrontierPilot* f = nearest(std::nullopt);
        if (!f) return "frontier: none";
        f->arrived = true;
        frontierRadio(*f, what.empty() ? "hello" : what, world_->regions[f->region].file.name, true);
        return "frontier: " + radioLog_.back();
    }
    if (word == "level") {
        int64_t n = 0;
        in.clear();
        std::istringstream num(what);
        num >> n;
        for (FrontierPilot& f : frontier_) f.progress.xp = Progression::xp(std::max<int64_t>(n, 1));
        return "frontier: level " + std::to_string(n);
    }
    if (word == "list") {
        std::string out = "frontier:";
        for (const FrontierPilot& f : frontier_)
            out += " " + f.callsign + "(" + net::title(f.habit) + " lv" + std::to_string(f.progress.level()) + " " +
                   world_->regions[f.region].file.id + " " + net::title(f.doing) + ")";
        return out;
    }
    return "frontier: here HABIT [SIDE] | build KIND | radio KIND | level N | list";
}

} // namespace ac::server
