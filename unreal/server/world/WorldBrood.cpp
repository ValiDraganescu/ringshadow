// The Brood's director in a world (docs/creatures.md, "Where they come from
// and what draws them"): region by region it spawns groups of bugs out of
// sight, sends them to the points of interest their kind weighs, lets them
// linger, turns them on a pilot in their site or one that hits them, raids
// the realm's buildings with a warning, sends Lodebacks to eat unprotected
// ore, and takes every bug away again out of sight. The core steps each bug
// by the orders it is given here (`Simulation::stepBug`); none of this goes
// on the wire but the bugs themselves and the raids' warnings.
//
// The opening's rule (docs/creatures.md, "Brood level"): in a level 1 region
// (Prospect Landing) the director holds while any pilot in it or next to it
// is still on the first ten minutes' chain, and its groups never linger
// within 25 cells of the start pad, so the opening's places stay calm.

#include "WorldMap.h"

#include "Brood.h"
#include "Ringshadow.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace ac::server {

using region::Edge;

namespace {

constexpr double kInf = 1e18;

struct Weights {
    double deposit, well, site, pad, building, crater;
};

/// docs/creatures.md, "Points of interest".
Weights weightsOf(UnitKind k) {
    switch (k) {
    case UnitKind::gloamwing: return {0.5, 0.5, 1, 1, 3, 2};
    case UnitKind::lodeback: return {3, 0, 0, 0, 0, 8};
    default: return {2, 1, 3, 1, 2, 5};
    }
}

double weightOf(const Weights& w, WorldMap::Poi p) {
    switch (p) {
    case WorldMap::Poi::deposit: return w.deposit;
    case WorldMap::Poi::well: return w.well;
    case WorldMap::Poi::site: return w.site;
    case WorldMap::Poi::pad: return w.pad;
    case WorldMap::Poi::building: return w.building;
    case WorldMap::Poi::crater: return w.crater;
    }
    return 0;
}

int64_t between(std::mt19937_64& dice, int64_t lo, int64_t hi) {
    return lo + int64_t(dice() % uint64_t(std::max<int64_t>(1, hi - lo + 1)));
}

double unit01(std::mt19937_64& dice) { return double(dice() >> 11) * 0x1.0p-53; }

struct Candidate {
    WorldMap::Poi poi;
    int64_t key;
    Vec2 at;
    double radius;
    double weight;
};

} // namespace

int64_t WorldBrood::cap(int64_t level) {
    switch (level) {
    case 1: return 6;
    case 2: return 14;
    case 3: case 4: return 22;
    default: return 0;
    }
}

double WorldBrood::every(int64_t level) {
    switch (level) {
    case 1: return 90;
    case 2: return 60;
    default: return 45;
    }
}

// MARK: - Reading

const std::vector<WorldMap::BroodGroup>& WorldMap::broodGroups(size_t r) const {
    static const std::vector<BroodGroup> none;
    return r < brood_.size() ? brood_[r].groups : none;
}

std::optional<int64_t> WorldMap::broodSide(size_t r) const {
    if (r >= regions_.size()) return std::nullopt;
    return regions_[r]->simulation().broodSide;
}

int64_t WorldMap::broodAlive(size_t r) const {
    const auto side = broodSide(r);
    if (!side) return 0;
    int64_t n = 0;
    for (const Unit& u : regions_[r]->simulation().state.units)
        if (u.owner == *side && u.hp > 0) n++;
    return n;
}

double WorldMap::sweepCover(size_t r, Vec2 at) const {
    const Simulation& sim = regions_[r]->simulation();
    if (!sim.map) return 0;
    return Ringshadow::cover(clock_, at, sim.map->bounds, params_.dayLength);
}

bool WorldMap::inSight(size_t r, Vec2 at) const {
    const GameState& st = regions_[r]->simulation().state;
    for (const Unit& u : st.units)
        if (u.owner == Pilot::player && u.hp > 0 && distance(u.position, at) < WorldBrood::sightProspector) return true;
    for (const Structure& b : st.structures)
        if (realm(b) && distance(b.position, at) < WorldBrood::sightBuilding + Rules::radius(b.kind)) return true;
    return false;
}

Vec2 WorldMap::exitPoint(size_t r, Vec2 at) const {
    const region::Built& places = *regions_[r]->places();
    const Vec2 ext = places.extent();
    const Vec2 here = places.regionAt(at);
    // The nearest edge first; a point a little inside it (the bug walks
    // there and is taken away once out of sight, on its way or there).
    const double gaps[4] = {here.y, ext.x - here.x, ext.y - here.y, here.x};
    std::optional<Vec2> best;
    double bestGap = kInf;
    const Simulation& sim = regions_[r]->simulation();
    for (int e = 0; e < 4; e++) {
        Vec2 p = here;
        switch (e) {
        case 0: p.y = 3; break;
        case 1: p.x = ext.x - 3; break;
        case 2: p.y = ext.y - 3; break;
        case 3: p.x = 3; break;
        }
        const Vec2 g = places.ground(p);
        const Vec2 q = sim.nav ? sim.nav->nearestFree(g).value_or(g) : g;
        const double score = gaps[e] + (inSight(r, q) ? 1000 : 0);
        if (score < bestGap) {
            bestGap = score;
            best = q;
        }
    }
    return best.value_or(at);
}

std::optional<Vec2> WorldMap::spawnPoint(size_t r, std::optional<Vec2> toward) const {
    const Region& region = *regions_[r];
    const region::Built& places = *region.places();
    const Simulation& sim = region.simulation();
    const GameState& st = sim.state;
    const Vec2 ext = places.extent();
    std::mt19937_64 dice(broodDice_);
    dice.discard(size_t(clock_ * 7) % 97);
    auto fine = [&](Vec2 g) {
        if (sim.nav && (!sim.nav->walkable(g) || sim.nav->inRock(g))) return false;
        for (const Unit& u : st.units)
            if (u.owner == Pilot::player && u.hp > 0 && distance(u.position, g) < WorldBrood::spawnFromProspector) return false;
        for (const Structure& b : st.structures)
            if (realm(b) && distance(b.position, g) < WorldBrood::spawnFromBuilding + Rules::radius(b.kind)) return false;
        for (const auto& [pilot, was] : respawned_) {
            const auto p = pilots_.find(pilot);
            if (p != pilots_.end() && p->second.region == r && clock_ - was.first < WorldBrood::respawnCalm &&
                distance(was.second, g) < WorldBrood::spawnFromRespawn)
                return false;
        }
        return true;
    };
    std::optional<Vec2> best;
    double bestScore = -kInf;
    for (int k = 0; k < 40; k++) {
        // At the region's edge (as if it came from elsewhere): 3 to 8 cells in.
        const int e = int(dice() % 4);
        const double along = unit01(dice), in = 3 + 5 * unit01(dice);
        Vec2 p;
        switch (e) {
        case 0: p = Vec2(along * ext.x, in); break;
        case 1: p = Vec2(ext.x - in, along * ext.y); break;
        case 2: p = Vec2(along * ext.x, ext.y - in); break;
        default: p = Vec2(in, along * ext.y); break;
        }
        const Vec2 g = places.ground(p);
        if (!fine(g)) continue;
        // Nearer the POI is better; under a sweep, under the band is best.
        double score = toward ? -distance(g, *toward) : 0;
        score += 60 * sweepCover(r, g);
        if (score > bestScore) {
            bestScore = score;
            best = g;
        }
    }
    return best;
}

// MARK: - Spawning

std::optional<int64_t> WorldMap::spawnBroodGroup(size_t r, UnitKind kind, int64_t count, Poi poi, int64_t key,
                                                 std::optional<Vec2> from, bool arrived, int64_t escort) {
    if (r >= regions_.size() || !regions_[r]->simulation().broodSide) return std::nullopt;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const int64_t side = *sim.broodSide;
    BroodGroup g;
    g.id = nextGroup_++;
    g.poi = poi;
    g.key = key;
    switch (poi) {
    case Poi::deposit:
        if (key < 0 || key >= int64_t(st.patches.size())) return std::nullopt;
        g.site = st.patches[size_t(key)].position;
        break;
    case Poi::well:
        if (!st.wells || key < 0 || key >= int64_t(st.wells->size())) return std::nullopt;
        g.site = (*st.wells)[size_t(key)].position;
        break;
    case Poi::site:
        if (key < 0 || key >= int64_t(regions_[r]->places()->sites.size())) return std::nullopt;
        g.site = regions_[r]->places()->sites[size_t(key)].at;
        break;
    case Poi::pad:
        if (key < 0 || key >= int64_t(regions_[r]->places()->pads.size())) return std::nullopt;
        g.site = regions_[r]->places()->pads[size_t(key)].at;
        break;
    case Poi::building: {
        const auto b = st.structure(key);
        if (!b) return std::nullopt;
        g.site = b->position;
        g.radius = Rules::radius(b->kind) + WorldBrood::buildingSite;
        g.raid = true;
        break;
    }
    case Poi::crater: {
        const auto f = std::find_if(falls_.begin(), falls_.end(), [&](const Fall& x) { return int64_t(x.id) == key; });
        if (f == falls_.end() || f->region != r) return std::nullopt;
        g.site = f->at;
        g.radius = WorldBrood::craterSite;
        break;
    }
    }
    const bool night = Rules::dark(sky_.hour()) > 0.5;
    g.lingerUntil = clock_ + (WorldBrood::lingerMin + (WorldBrood::lingerMax - WorldBrood::lingerMin) * unit01(broodDice_)) *
                                 (night ? WorldBrood::nightLinger : 1);
    const double cover = sweepCover(r, g.site);
    if (cover >= Ringshadow::bonusCover) {
        g.radius *= WorldBrood::sweepSite;
        g.leash += WorldBrood::sweepLeash;
    }
    // A Lodeback's meal: the deposit, or the crater's.
    std::optional<int64_t> meal;
    if (poi == Poi::deposit) meal = key;
    if (poi == Poi::crater)
        for (const Fall& f : falls_)
            if (int64_t(f.id) == key) meal = f.patch;
    const Vec2 start = from ? *from : spawnPoint(r, g.site).value_or(Vec2(kInf, kInf));
    if (start.x >= kInf) return std::nullopt;
    g.exit = exitPoint(r, start);
    auto add = [&](UnitKind k, int64_t i) {
        const double a = double(i) * 2.399963;  // the golden angle: a loose cluster
        const double rr = 0.9 * std::sqrt(double(i));
        Vec2 p = start + Vec2(std::cos(a), std::sin(a)) * rr;
        if (arrived) p = g.site + Vec2(std::cos(a), std::sin(a)) * (g.radius * 0.35 + rr * 0.5);
        if (Rules::stats(k).air == false && sim.nav) p = sim.nav->nearestFree(p).value_or(p);
        const int64_t id = st.nextID++;
        Unit u = sim.freshUnit(k, id, side, p, std::atan2(g.site.y - p.y, g.site.x - p.x));
        Bug b;
        b.group = g.id;
        b.site = g.site;
        b.siteRadius = g.radius;
        b.leash = g.leash;
        b.coming = !arrived;
        b.exit = g.exit;
        if (k == UnitKind::gloamwing) b.height = Brood::hoverHeight;
        if (k == UnitKind::lodeback) b.deposit = meal;
        if (g.raid) b.raid = key;
        u.bug = b;
        st.units.push_back(u);
        g.bugs.push_back(id);
    };
    for (int64_t i = 0; i < count; i++) add(kind, i);
    for (int64_t i = 0; i < escort; i++) add(UnitKind::shardling, count + i);
    sim.reindex();
    if (brood_.size() < regions_.size()) brood_.resize(regions_.size());
    brood_[r].groups.push_back(g);
    if (g.raid) brood_[r].lastRaid = clock_;
    return g.id;
}

bool WorldMap::spawnGroup(size_t r) {
    const int64_t level = world_->regions[r].file.brood;
    Region& region = *regions_[r];
    const region::Built& places = *region.places();
    const GameState& st = region.simulation().state;
    BroodRegion& br = brood_[r];
    const int64_t room = WorldBrood::cap(level) - broodAlive(r);
    if (room <= 0) return false;

    // The kind first, by the level; then its POI by its weights.
    const double roll = unit01(broodDice_);
    UnitKind kind = UnitKind::shardling;
    if (level == 1) kind = roll < 0.8 ? UnitKind::shardling : UnitKind::lodeback;
    else kind = roll < 0.5 ? UnitKind::shardling : roll < 0.75 ? UnitKind::gloamwing : UnitKind::lodeback;

    // Raids: at most one a region every few minutes.
    const bool raidReady = clock_ - br.lastRaid >= (level == 1 ? WorldBrood::raidEveryGentle : WorldBrood::raidEvery);
    std::optional<Vec2> citadel;
    for (const Structure& b : st.structures)
        if (realm(b) && b.kind == StructureKind::citadel) citadel = b.position;
    const std::optional<Vec2> startPad =
        level == 1 && world_->start == r && world_->startPad < places.pads.size() ? std::optional<Vec2>(places.pads[world_->startPad].at)
                                                                                  : std::nullopt;
    auto held = [&](Poi poi, int64_t key) {
        for (const BroodGroup& g : br.groups)
            if (g.poi == poi && g.key == key) return true;
        return false;
    };
    auto calm = [&](Vec2 at) {
        if (citadel && distance(at, *citadel) < WorldBrood::citadelCalm) return true;
        if (startPad && distance(at, *startPad) < WorldBrood::startCalm) return true;
        return false;
    };
    auto unprotected = [&](Vec2 at) {
        for (const Structure& b : st.structures)
            if (realm(b) && b.complete() && distance(b.position, at) < WorldBrood::guardBuilding + Rules::radius(b.kind)) return false;
        for (const Unit& u : st.units)
            if (u.owner == Pilot::player && u.kind == UnitKind::prospector && distance(u.position, at) < WorldBrood::guardProspector) return false;
        return true;
    };
    const Weights w = weightsOf(kind);
    const bool sweep = std::any_of(st.patches.begin(), st.patches.end(), [&](const OreDeposit& d) {
        return d.remaining > 0 && sweepCover(r, d.position) >= Ringshadow::bonusCover;
    });
    std::vector<Candidate> cs;
    std::set<int64_t> craterPatches;
    for (const Fall& f : falls_)
        if (f.region == r && f.patch) craterPatches.insert(*f.patch);
    for (size_t i = 0; i < st.patches.size(); i++) {
        const OreDeposit& d = st.patches[i];
        if (d.remaining <= 0 || craterPatches.count(int64_t(i)) || held(Poi::deposit, int64_t(i)) || calm(d.position)) continue;
        if (kind == UnitKind::lodeback && !unprotected(d.position)) continue;
        // Under a sweep the group goes for the ore under the band.
        if (sweep && sweepCover(r, d.position) < Ringshadow::bonusCover) continue;
        cs.push_back({Poi::deposit, int64_t(i), d.position, WorldBrood::siteRadius, weightOf(w, Poi::deposit)});
    }
    if (!sweep) {
        if (st.wells)
            for (size_t i = 0; i < st.wells->size(); i++) {
                const Vec2 at = (*st.wells)[i].position;
                if (!held(Poi::well, int64_t(i)) && !calm(at)) cs.push_back({Poi::well, int64_t(i), at, WorldBrood::siteRadius, w.well});
            }
        for (size_t i = 0; i < places.sites.size(); i++) {
            const std::string id = world_->regions[r].file.id + "/" + places.sites[i].name;
            bool open = false;
            for (const auto& [pilot, p] : pilots_)
                if (!p.progress.sites.count(id)) open = true;
            if (open && !held(Poi::site, int64_t(i)) && !calm(places.sites[i].at))
                cs.push_back({Poi::site, int64_t(i), places.sites[i].at, WorldBrood::siteRadius, w.site});
        }
        for (size_t i = 0; i < places.pads.size(); i++)
            if (!held(Poi::pad, int64_t(i)) && !calm(places.pads[i].at))
                cs.push_back({Poi::pad, int64_t(i), places.pads[i].at, WorldBrood::siteRadius, w.pad});
        if (raidReady && kind != UnitKind::lodeback)
            for (const Structure& b : st.structures) {
                if (!realm(b) || !b.complete() || held(Poi::building, b.id)) continue;
                if (level == 1 && (b.kind == StructureKind::citadel || (citadel && distance(b.position, *citadel) < WorldBrood::raidFromCitadel))) continue;
                // Weighted by its cost too: a Derrick draws more than a Lab.
                const double cost = double(Rules::cost(b.kind) + Rules::hydrogenCost(b.kind)) / 100;
                cs.push_back({Poi::building, b.id, b.position, Rules::radius(b.kind) + WorldBrood::buildingSite, w.building * cost});
            }
        for (const Fall& f : falls_)
            if (f.region == r && f.patch && clock_ - f.impact >= WorldBrood::craterHot && !held(Poi::crater, int64_t(f.id)) &&
                *f.patch < int64_t(st.patches.size()) && st.patches[size_t(*f.patch)].remaining > 0 &&
                (kind != UnitKind::lodeback || unprotected(f.at)))
                cs.push_back({Poi::crater, int64_t(f.id), f.at, WorldBrood::craterSite, w.crater});
    }
    std::erase_if(cs, [](const Candidate& c) { return !(c.weight > 0); });
    double total = 0;
    for (const Candidate& c : cs) total += c.weight;
    if (!(total > 0)) return false;
    double pick = unit01(broodDice_) * total;
    const Candidate* chosen = &cs.back();
    for (const Candidate& c : cs) {
        if (pick < c.weight) {
            chosen = &c;
            break;
        }
        pick -= c.weight;
    }
    int64_t count = 1, escort = 0;
    const bool raid = chosen->poi == Poi::building;
    switch (kind) {
    case UnitKind::shardling:
        count = raid ? (level == 1 ? between(broodDice_, 3, 4) : between(broodDice_, 6, 12))
                     : level == 1 ? between(broodDice_, 1, 3) : level == 2 ? between(broodDice_, 3, 6) : between(broodDice_, 4, 8);
        break;
    case UnitKind::gloamwing: count = between(broodDice_, level >= 3 ? 3 : 2, 4); break;
    case UnitKind::lodeback:
        count = level == 1 ? 1 : between(broodDice_, 1, 2);
        escort = level == 1 ? 0 : between(broodDice_, 2, 4);
        break;
    default: break;
    }
    count = std::min(count, room);
    escort = std::min(escort, std::max<int64_t>(0, room - count));
    const auto id = spawnBroodGroup(r, kind, count, chosen->poi, chosen->key, std::nullopt, false, escort);
    // A mixed raid off level 1: Gloamwings with the pack.
    if (id && raid && kind == UnitKind::shardling && level >= 2 && room - count >= 2) {
        BroodGroup& g = brood_[r].groups.back();
        const Vec2 from = regions_[r]->simulation().state.unit(g.bugs.front())->position;
        const auto extra = spawnBroodGroup(r, UnitKind::gloamwing, std::min<int64_t>(between(broodDice_, 2, 4), room - count),
                                           Poi::building, chosen->key, from);
        // One group: the flyers join the pack's.
        if (extra) {
            BroodGroup flyers = brood_[r].groups.back();
            brood_[r].groups.pop_back();
            BroodGroup& pack = brood_[r].groups.back();
            for (const int64_t b : flyers.bugs) {
                pack.bugs.push_back(b);
                for (Unit& u : regions_[r]->simulation().state.units)
                    if (u.id == b && u.bug) u.bug->group = pack.id;
            }
        }
    }
    return id.has_value();
}

void WorldMap::broodNow(size_t r) {
    if (brood_.size() < regions_.size()) brood_.resize(regions_.size());
    if (r < brood_.size()) brood_[r].next = clock_;
}

// MARK: - Each step

void WorldMap::brood() {
    if (!world_) return;
    if (brood_.size() < regions_.size()) {
        brood_.resize(regions_.size());
        broodDice_.seed(params_.broodSeed ? params_.broodSeed : uint64_t(WorldClock::weatherSeed(world_->file.id)) ^ 0xb200d5eedULL);
        // The Brood's side, in every region with a brood level: a player of
        // its own, before any sight is worked out.
        for (size_t r = 0; r < regions_.size(); r++) {
            Simulation& sim = regions_[r]->simulation();
            if (world_->regions[r].file.brood <= 0 || sim.broodSide) continue;
            sim.state.players.push_back(Player());
            sim.state.score.push_back(0);
            sim.broodSide = int64_t(sim.state.players.size()) - 1;
        }
    }
    // Respawns: a pilot with a new Prospector came back at home just now.
    for (const auto& [pilot, p] : pilots_) {
        if (!p.unit) continue;
        const auto was = broodUnits_.find(pilot);
        if (was != broodUnits_.end() && was->second != *p.unit && !p.flight)
            if (const auto u = regions_[p.region]->simulation().state.unit(*p.unit)) respawned_[pilot] = {clock_, u->position};
        broodUnits_[pilot] = *p.unit;
    }
    for (size_t r = 0; r < regions_.size(); r++) {
        Simulation& sim = regions_[r]->simulation();
        if (!sim.broodSide) continue;
        BroodRegion& br = brood_[r];
        GameState& st = sim.state;
        // The hits on bugs: the hitter's XP (the damage's share of the
        // bug's hp × its worth), and the group turns on it.
        for (const Simulation::BugHit& h : sim.bugHits) {
            const double share = h.hp / std::max(1.0, Rules::hp(h.kind));
            for (auto& [pilot, p] : pilots_)
                if (!p.flight && p.region == r && p.unit == h.shooter) earn(p, Brood::xp(h.kind) * share);
            const auto u = st.unit(h.bug);
            if (!u || !u->bug || u->kind == UnitKind::lodeback) continue;
            for (BroodGroup& g : br.groups)
                if (g.id == u->bug->group && !g.prey) g.prey = h.shooter;
        }
        sim.bugHits.clear();
        // A dead Lodeback's sac goes back into its deposit (else the
        // nearest within reach, else it is lost).
        for (const GameEvent& e : regions_[r]->lastEvents()) {
            const auto* d = e.as<GameEvent::Died>();
            if (!d || d->kind != UnitKind::lodeback) continue;
            const auto it = br.sacs.find(d->unit);
            if (it == br.sacs.end()) continue;
            const int64_t ore = int64_t(std::floor(it->second.first));
            std::optional<size_t> to;
            if (it->second.second && *it->second.second < int64_t(st.patches.size())) to = size_t(*it->second.second);
            if (!to) {
                double best = WorldBrood::returnReach;
                for (size_t i = 0; i < st.patches.size(); i++)
                    if (distance(st.patches[i].position, d->at) <= best) { best = distance(st.patches[i].position, d->at); to = i; }
            }
            if (to && ore > 0) {
                OreDeposit& dep = st.patches[*to];
                dep.remaining += ore;
                dep.depletedAt = std::nullopt;
            }
            br.sacs.erase(it);
        }
        for (const Unit& u : st.units)
            if (u.kind == UnitKind::lodeback && u.bug) br.sacs[u.id] = {u.bug->sac, u.bug->ate};
        // A starfall's impact: it kills every bug near and scares the rest.
        for (const Fall& f : falls_) {
            if (f.region != r || !f.patch || fallsFelt_.count(f.id)) continue;
            fallsFelt_.insert(f.id);
            for (Unit& u : st.units) {
                if (!u.bug) continue;
                const double d = distance(u.position, f.at);
                if (d <= WorldBrood::impactKill) u.hp = 0;
                else if (d <= WorldBrood::impactScare) {
                    u.bug->scaredUntil = st.time + Brood::scareFor;
                    u.bug->scaredFrom = f.at;
                }
            }
        }
    }
    if (clock_ - broodThought_ < WorldBrood::think - 1e-9) return;
    broodThought_ = clock_;
    for (size_t r = 0; r < regions_.size(); r++)
        if (regions_[r]->simulation().broodSide) broodThink(r);
    tellRaids();
}

void WorldMap::broodThink(size_t r) {
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    BroodRegion& br = brood_[r];
    const int64_t level = world_->regions[r].file.brood;
    // Pilots in the region or next to it.
    bool near = false, opening = false;
    for (const auto& [pilot, p] : pilots_) {
        if (p.flight) continue;
        bool here = p.region == r;
        for (const Edge e : {Edge::north, Edge::east, Edge::south, Edge::west})
            if (neighbour(r, e) == p.region) here = true;
        if (!here) continue;
        near = true;
        if (p.firstStep > 0) opening = true;
    }
    if (near) br.lastNear = clock_;
    if (!near && clock_ - br.lastNear > WorldBrood::idleDespawn) {
        if (!br.groups.empty() || std::any_of(st.units.begin(), st.units.end(), [](const Unit& u) { return u.bug.has_value(); })) {
            std::erase_if(st.units, [](const Unit& u) { return u.bug.has_value(); });
            sim.reindex();
            br.groups.clear();
            br.sacs.clear();
        }
        br.next = std::nullopt;
        return;
    }

    // The groups: their bugs, their prey, their orders.
    std::vector<const Unit*> prospectors;
    for (const Unit& u : st.units)
        if (u.owner == Pilot::player && u.hp > 0 && u.task != Unit::Task::aboard && u.task != Unit::Task::inDerrick) prospectors.push_back(&u);
    std::set<int64_t> gone;
    for (BroodGroup& g : br.groups) {
        std::erase_if(g.bugs, [&](int64_t id) { return !st.unit(id); });
        if (g.prey) {
            // Gone, or run past the leash: the group lets it go.
            const auto t = st.unit(*g.prey);
            if (!t || t->hp <= 0 || distance(t->position, g.site) > g.radius + g.leash) g.prey = std::nullopt;
        }
        // A pilot inside the site: the whole group turns on it (the nearest).
        if (!g.prey && !g.leaving) {
            double best = g.radius;
            for (const Unit* u : prospectors)
                if (distance(u->position, g.site) <= best) {
                    best = distance(u->position, g.site);
                    g.prey = u->id;
                }
        }
        bool lodebacks = false, lodebacksLeaving = true;
        for (const int64_t id : g.bugs)
            if (const auto u = st.unit(id); u && u->kind == UnitKind::lodeback) {
                lodebacks = true;
                if (!u->bug->leaving) lodebacksLeaving = false;
            }
        // A raid's building fell: the raid is over.
        if (g.raid && !st.structure(g.key)) g.leaving = true;
        if (!g.leaving && !g.raid && clock_ >= g.lingerUntil && !g.prey) g.leaving = true;
        if (lodebacks && lodebacksLeaving && !g.prey) g.leaving = true;
        for (Unit& u : st.units) {
            if (!u.bug || u.bug->group != g.id) continue;
            Bug& b = *u.bug;
            b.site = g.site;
            b.siteRadius = g.radius;
            b.leash = g.leash;
            if (u.kind != UnitKind::lodeback) {
                b.prey = g.prey;
                b.raid = g.raid && !g.leaving ? std::optional<int64_t>(g.key) : std::nullopt;
                b.leaving = g.leaving && !g.prey;
            }
            if (b.leaving) b.exit = g.exit;
            // Out of sight on its way out (out of its site, or the site
            // itself unseen): it vanishes.
            if (b.leaving && !inSight(r, u.position) && (distance(u.position, g.site) > g.radius || !inSight(r, g.site)))
                gone.insert(u.id);
        }
    }
    if (!gone.empty()) {
        std::erase_if(st.units, [&](const Unit& u) { return gone.count(u.id) > 0; });
        sim.reindex();
        for (const int64_t id : gone) br.sacs.erase(id);
        for (BroodGroup& g : br.groups) std::erase_if(g.bugs, [&](int64_t id) { return gone.count(id) > 0; });
    }
    std::erase_if(br.groups, [](const BroodGroup& g) { return g.bugs.empty(); });

    // Raids' warnings: 20 s before the swarm reaches its building.
    for (BroodGroup& g : br.groups) {
        if (!g.raid || g.raidId || g.leaving) continue;
        double nearest = kInf;
        for (const int64_t id : g.bugs)
            if (const auto u = st.unit(id)) nearest = std::min(nearest, distance(u->position, g.site) - g.radius);
        const double eta = std::max(0.0, nearest) / Brood::marchSpeed(UnitKind::shardling);
        if (eta > WorldBrood::raidWarning) continue;
        const auto b = st.structure(g.key);
        if (!b) continue;
        net::Sky::Raid x;
        x.id = nextRaid_++;
        x.region = int64_t(r);
        x.building = g.key;
        x.kind = b->kind;
        x.at = b->position;
        if (const auto u = st.unit(g.bugs.front())) x.from = u->position;
        x.arrives = clock_ + eta;
        raids_.push_back(x);
        g.raidId = x.id;
    }

    // The next group.
    if (!params_.brood || level <= 0) return;
    // The opening's rule: Prospect Landing holds while a pilot there or
    // next door is on the first ten minutes' chain.
    if (level == 1 && opening) {
        br.next = std::nullopt;
        return;
    }
    const bool night = Rules::dark(sky_.hour()) > 0.5;
    const double every = WorldBrood::every(level) * (night ? WorldBrood::nightEvery : 1);
    if (!br.next) br.next = clock_ + every;
    if (clock_ < *br.next) return;
    spawnGroup(r);
    br.next = clock_ + every;
}

void WorldMap::tellRaids() {
    // Told until it arrives, and a moment after (a client that missed a
    // snapshot still hears it).
    std::erase_if(raids_, [&](const net::Sky::Raid& x) { return clock_ > x.arrives + 2; });
    sky_.raids = raids_;
}

// MARK: - Dev and shots

std::string WorldMap::devBrood(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: brood: no world, or no pilot";
    const auto& [pilot, p] = *pilots_.begin();
    const size_t r = p.region;
    if (!regions_[r]->simulation().broodSide) return "dev: brood: no brood level in " + world_->regions[r].file.id;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const auto me = p.unit ? st.unit(*p.unit) : std::nullopt;
    if (!me) return "dev: brood: the pilot drives nothing";
    const Vec2 at = me->position;
    const double heading = me->heading;
    const Vec2 ahead(std::cos(heading), std::sin(heading));
    auto nearestDeposit = [&]() -> std::optional<size_t> {
        std::optional<size_t> best;
        double d = kInf;
        for (size_t i = 0; i < st.patches.size(); i++)
            if (st.patches[i].remaining > 0 && distance(st.patches[i].position, at) < d) {
                d = distance(st.patches[i].position, at);
                best = i;
            }
        return best;
    };
    std::optional<int64_t> g;
    if (what == "linger") {
        // Shardlings lingering at the deposit nearest the pilot.
        const auto dep = nearestDeposit();
        if (!dep) return "dev: brood linger: no deposit";
        g = spawnBroodGroup(r, UnitKind::shardling, 4, Poi::deposit, int64_t(*dep), st.patches[*dep].position, true);
    } else if (what == "attack") {
        // Shardlings on the pilot, its site round it.
        const auto dep = nearestDeposit();
        if (!dep) return "dev: brood attack: no deposit";
        g = spawnBroodGroup(r, UnitKind::shardling, 3, Poi::deposit, int64_t(*dep), at + ahead * 3, false);
        if (g) {
            BroodGroup& grp = brood_[r].groups.back();
            grp.site = at;
            grp.prey = me->id;
        }
    } else if (what == "dive") {
        const auto dep = nearestDeposit();
        if (!dep) return "dev: brood dive: no deposit";
        g = spawnBroodGroup(r, UnitKind::gloamwing, 2, Poi::deposit, int64_t(*dep), at + ahead * 3.5, false);
        if (g) {
            BroodGroup& grp = brood_[r].groups.back();
            grp.site = at;
            grp.prey = me->id;
        }
    } else if (what == "lodeback") {
        // A Lodeback with a full sac, leaving past the pilot.
        const auto dep = nearestDeposit();
        if (!dep) return "dev: brood lodeback: no deposit";
        g = spawnBroodGroup(r, UnitKind::lodeback, 1, Poi::deposit, int64_t(*dep), at + ahead * 6, false);
        if (g) {
            BroodGroup& grp = brood_[r].groups.back();
            for (Unit& u : st.units)
                if (u.bug && u.bug->group == grp.id) {
                    u.bug->sac = Brood::sacFull;
                    u.bug->leaving = true;
                    u.bug->deposit = std::nullopt;
                    u.bug->exit = at + Vec2(-ahead.y, ahead.x) * 60;
                }
            grp.exit = at + Vec2(-ahead.y, ahead.x) * 60;
            grp.leaving = true;
        }
    } else if (what == "raid") {
        // A raid on the realm's building nearest the pilot (a Hab Dome first).
        std::optional<Structure> target;
        for (const Structure& b : st.structures)
            if (realm(b) && b.complete() && (!target || (b.kind == StructureKind::habDome && target->kind != StructureKind::habDome) ||
                                             (b.kind == target->kind && distance(b.position, at) < distance(target->position, at))))
                target = b;
        if (!target) return "dev: brood raid: no building";
        const Vec2 off = normalize(target->position - at + Vec2(0.01, 0));
        g = spawnBroodGroup(r, UnitKind::shardling, 6, Poi::building, target->id, target->position + off * 9, false);
        if (g) {
            const auto extra = spawnBroodGroup(r, UnitKind::gloamwing, 2, Poi::building, target->id, target->position + off * 11, false);
            if (extra) {
                BroodGroup flyers = brood_[r].groups.back();
                brood_[r].groups.pop_back();
                BroodGroup& pack = brood_[r].groups.back();
                for (const int64_t b : flyers.bugs) {
                    pack.bugs.push_back(b);
                    for (Unit& u : st.units)
                        if (u.id == b && u.bug) u.bug->group = pack.id;
                }
            }
        }
    } else if (what == "clear") {
        std::erase_if(st.units, [](const Unit& u) { return u.bug.has_value(); });
        sim.reindex();
        brood_[r].groups.clear();
        return "dev: brood cleared";
    } else {
        return "dev: brood: '" + what + "' is not linger|attack|dive|lodeback|raid|clear";
    }
    if (!g) return "dev: brood " + what + ": not placed";
    return "dev: brood " + what + ": group " + std::to_string(*g) + " in " + world_->regions[r].file.id;
}

} // namespace ac::server
