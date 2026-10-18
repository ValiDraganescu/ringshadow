// The Matriarch and her nest (docs/creatures.md, "The Matriarch"; N3). One
// giant in the world, at the nest its region file names (Slag Pockets): the
// director keeps her there with her horde round her whenever a pilot is in
// the region or next to it, its numbers kept up one hatchling every 20 s.
// She roars when a pilot first comes near and every 30 s in a fight: the
// nest's bugs within reach turn on her foe and run faster, Shardlings hatch
// from her pods, and every pilot in earshot hears it (`Sky::roars`). Her
// slam, spit and mending are the core's (`Simulation::stepMatriarch`).
//
// Killing her pays every pilot who hurt her (with its Prospector or its
// defences) by its share of the damage: XP as for any bug, then ore, MH and
// a rare blueprint copy; the whole world hears "THE MATRIARCH IS DOWN". She
// comes back `Matriarch::returnDays` world days later and the nest grows
// back from empty; the region's bugs stay away for `Matriarch::calmDays`.
//
// The Z2 hook (`matriarchSiege`): she leaves the nest with an escort for a
// place (a canyon), fells every turret of the realm there, and goes home.

#include "WorldMap.h"

#include "Brood.h"
#include "Defence.h"
#include "Progression.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace ac::server {

namespace {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr double kFar = 1e18;

/// A turret a siege fells: the pilots' placed guns.
bool turret(StructureKind k) { return k == StructureKind::sentinel || k == StructureKind::gatling; }

/// The rare blueprints her death pays: the three with the highest floors.
std::vector<StructureKind> rareBlueprints() {
    std::vector<StructureKind> k;
    for (const StructureKind b : Progression::blueprintKinds())
        if (b != StructureKind::citadel) k.push_back(b);
    std::stable_sort(k.begin(), k.end(), [](StructureKind a, StructureKind b) {
        return Progression::blueprintFloor(a) > Progression::blueprintFloor(b);
    });
    if (k.size() > 3) k.resize(3);
    return k;
}

} // namespace

// MARK: - Reading

std::optional<size_t> WorldMap::nestRegion() const {
    if (!world_) return std::nullopt;
    for (size_t r = 0; r < world_->regions.size(); r++)
        if (world_->regions[r].built.nest) return r;
    return std::nullopt;
}

std::optional<Vec2> WorldMap::nestAt(size_t r) const {
    if (!world_ || r >= world_->regions.size()) return std::nullopt;
    return world_->regions[r].built.nest;
}

std::optional<std::pair<size_t, int64_t>> WorldMap::matriarch() const {
    if (!world_) return std::nullopt;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Unit& u : regions_[r]->simulation().state.units)
            if (u.kind == UnitKind::matriarch && u.hp > 0) return std::pair{r, u.id};
    return std::nullopt;
}

std::optional<double> WorldMap::matriarchBack() const { return nest_.backAt; }

const std::map<PilotId, double>& WorldMap::matriarchDamage() const { return nest_.damage; }

std::pair<int64_t, int64_t> WorldMap::hordeIn(size_t r) const {
    int64_t packs = 0, flyers = 0;
    if (r >= regions_.size() || r >= brood_.size()) return {0, 0};
    const GameState& st = regions_[r]->simulation().state;
    for (const BroodGroup& g : brood_[r].groups) {
        if (!g.nest) continue;
        for (const int64_t id : g.bugs)
            if (const auto u = st.unit(id); u && u->hp > 0) (u->kind == UnitKind::gloamwing ? flyers : packs) += 1;
    }
    return {packs, flyers};
}

std::pair<int64_t, int64_t> WorldMap::nestHorde() const {
    const auto r = nestRegion();
    return r ? hordeIn(*r) : std::pair<int64_t, int64_t>{0, 0};
}

// MARK: - The nest

void WorldMap::hatch(size_t r, UnitKind kind, int64_t key, Vec2 from) {
    const auto id = spawnBroodGroup(r, kind, 1, Poi::nest, key, from, false);
    if (!id) return;
    // Into its pack's group, if the pack is still there.
    std::vector<BroodGroup>& groups = brood_[r].groups;
    BroodGroup fresh = groups.back();
    const auto pack = std::find_if(groups.begin(), groups.end() - 1, [&](const BroodGroup& g) { return g.nest && g.poi == Poi::nest && g.key == key; });
    if (pack == groups.end() - 1) return;
    groups.pop_back();
    BroodGroup& g = *std::find_if(groups.begin(), groups.end(), [&](const BroodGroup& x) { return x.nest && x.key == key; });
    for (const int64_t b : fresh.bugs) {
        g.bugs.push_back(b);
        for (Unit& u : regions_[r]->simulation().state.units)
            if (u.id == b && u.bug) u.bug->group = g.id;
    }
}

void WorldMap::nestThink(size_t r) {
    if (!world_ || nestRegion() != r) return;
    const Vec2 home = *nestAt(r);
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    NestRegion& n = nest_;
    // Her return, by the rule: the nest grows back from empty.
    if (n.backAt && clock_ >= *n.backAt) {
        n.downAt = n.backAt = std::nullopt;
        n.regrow = true;
        n.shardlings = n.gloamwings = 0;
        for (auto& [pilot, p] : pilots_) p.news.push_back(News{NewsKind::matriarch, "back", 0, 0, ""});
        awayNoteAll(net::AwayRow{0, net::AwayKind::queenBack, "", "", world_->regions[r].file.name, 0});
    }
    if (n.downAt) return;
    // Out on a siege elsewhere: the nest waits without her.
    const bool away = siege_ && siege_->region != r;

    // Her, at the nest's centre (whole: a nest asleep keeps no wounds).
    std::optional<Unit*> queen;
    for (Unit& u : st.units)
        if (n.queen && u.id == *n.queen && u.hp > 0) queen = &u;
    if (!queen && n.queen && !away) {
        // Felled by something the hits did not tell (dev, a starfall).
        if (const auto body = st.unit(*n.queen); body && body->hp <= 0) {
            queenDown(r);
            return;
        }
    }
    if (!queen && !away) {
        const auto g = spawnBroodGroup(r, UnitKind::matriarch, 1, Poi::nest, WorldNest::queenKey, home, true);
        if (!g) return;
        n.queen = brood_[r].groups.back().bugs.front();
        n.damage.clear();
        for (Unit& u : st.units)
            if (u.id == *n.queen) {
                u.position = sim.nav ? sim.nav->nearestFree(home).value_or(home) : home;
                queen = &u;
            }
    }

    // The horde: its counts drawn once; full at once unless it grows back.
    if (n.shardlings == 0) {
        n.shardlings = Matriarch::nestShardlings + int64_t(broodDice_() % uint64_t(Matriarch::nestShardlingsMax - Matriarch::nestShardlings + 1));
        n.gloamwings = Matriarch::nestGloamwings + int64_t(broodDice_() % uint64_t(Matriarch::nestGloamwingsMax - Matriarch::nestGloamwings + 1));
    }
    auto [packs, flyers] = hordeIn(r);
    const bool any = std::any_of(brood_[r].groups.begin(), brood_[r].groups.end(), [](const BroodGroup& g) { return g.nest; });
    if (!any && !n.regrow && packs + flyers == 0) {
        for (int64_t k = 1; k <= WorldNest::packs; k++) {
            const int64_t count = n.shardlings / WorldNest::packs + (k <= n.shardlings % WorldNest::packs ? 1 : 0);
            if (count > 0) spawnBroodGroup(r, UnitKind::shardling, count, Poi::nest, k, std::nullopt, true);
        }
        spawnBroodGroup(r, UnitKind::gloamwing, n.gloamwings, Poi::nest, WorldNest::flyersKey, std::nullopt, true);
        n.nextRefill = clock_ + Matriarch::refill;
        std::tie(packs, flyers) = hordeIn(r);
    }
    // One hatchling every `refill` s while the nest is below its count.
    if (clock_ >= n.nextRefill && (packs < n.shardlings || flyers < n.gloamwings)) {
        n.nextRefill = clock_ + Matriarch::refill;
        if (flyers * n.shardlings < packs * n.gloamwings || packs >= n.shardlings) {
            hatch(r, UnitKind::gloamwing, WorldNest::flyersKey, home);
        } else {
            // Into the smallest pack.
            int64_t best = 1, fewest = INT64_MAX;
            for (int64_t k = 1; k <= WorldNest::packs; k++) {
                int64_t c = 0;
                for (const BroodGroup& g : brood_[r].groups)
                    if (g.nest && g.key == k) c += int64_t(g.bugs.size());
                if (c < fewest) { fewest = c; best = k; }
            }
            hatch(r, UnitKind::shardling, best, home);
        }
        if (packs + 1 >= n.shardlings && flyers + 1 >= n.gloamwings) n.regrow = false;
    }
    if (!queen || away) return;
    // The horde's spawns and the hatchling may have moved the units
    // (`spawnBroodGroup` adds to them): her by id again.
    queen = std::nullopt;
    for (Unit& u : st.units)
        if (u.id == *n.queen) queen = &u;
    if (!queen) return;
    Unit& q = **queen;
    Bug& b = *q.bug;
    if (siege_ && siege_->region == r) return;  // her orders are the siege's
    b.site = home;
    b.tether = Matriarch::tether;

    // Her foe: whoever hit her last, else the nearest Prospector, else a
    // building of the realm, within her tether and her spit.
    const double reach = Matriarch::tether + Matriarch::spitRange + Rules::radius(UnitKind::matriarch);
    std::optional<int64_t> prey, raid;
    if (b.hitBy) {
        if (const auto t = sim.target(*b.hitBy); t && distance(t->position, home) <= reach + t->radius && st.hostile(t->owner, q.owner))
            (t->structure ? raid : prey) = *b.hitBy;
    }
    // The pilots' units as they stand (`st.unit` gives a copy: kept here,
    // not a pointer to it).
    std::vector<std::pair<PilotId, Unit>> near;
    for (const auto& [pilot, p] : pilots_) {
        if (p.flight || p.region != r || !p.unit) continue;
        const auto u = st.unit(*p.unit);
        // C1: not one sheltered in a Citadel's hall or at its guns.
        if (u && u->hp > 0 && u->task != Unit::Task::aboard && u->task != Unit::Task::inDerrick && !sim.sheltered(*u))
            near.push_back({pilot, *u});
    }
    if (!prey && !raid) {
        double best = reach;
        for (const auto& [pilot, u] : near)
            if (distance(u.position, home) <= best) {
                best = distance(u.position, home);
                prey = u.id;
            }
    }
    if (!prey && !raid) {
        double best = kFar;
        for (const Structure& s : st.structures)
            if (realm(s) && s.hp > 0 && distance(s.position, home) <= reach + Rules::radius(s.kind) && distance(s.position, q.position) < best) {
                best = distance(s.position, q.position);
                raid = s.id;
            }
    }
    if (n.held) {
        b.spitReady = std::max(b.spitReady, st.time + 5);
    } else {
        b.prey = prey;
        b.raid = raid;
    }

    // Her roars: once when a pilot first comes near, and every
    // `roarEvery` s in a fight.
    if (st.time < b.roarUntil) return;
    std::erase_if(n.seen, [&](PilotId id) {
        return std::none_of(near.begin(), near.end(), [&](const auto& x) {
            return x.first == id && distance(x.second.position, q.position) <= Matriarch::roarSight + 15;
        });
    });
    for (const auto& [pilot, u] : near)
        if (!n.seen.count(pilot) && distance(u.position, q.position) <= Matriarch::roarSight) {
            n.seen.insert(pilot);
            roar(r, q, u.id);
            return;
        }
    if ((prey || raid) && clock_ - n.lastRoar >= Matriarch::roarEvery) roar(r, q, prey ? prey : raid);
}

void WorldMap::roar(size_t r, Unit& q, std::optional<int64_t> at) {
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    q.bug->roarUntil = st.time + Matriarch::roarFor;
    nest_.lastRoar = clock_;
    // Where she stands: the hatchlings below add to the units, so `q` is
    // not read past the first.
    const Vec2 at0 = q.position;
    const auto foe = at ? sim.target(*at) : std::nullopt;
    // Every bug of the nest within reach turns on her foe, and runs.
    for (BroodGroup& g : brood_[r].groups) {
        if (!(g.nest || (siege_ && g.key == WorldNest::escortKey && g.poi == Poi::nest))) continue;
        bool reached = false;
        for (Unit& u : st.units) {
            if (!u.bug || u.bug->group != g.id || distance(u.position, q.position) > Matriarch::roarReach) continue;
            reached = true;
            u.bug->rallyUntil = st.time + Matriarch::rallyFor;
        }
        if (!reached || !foe || g.still) continue;
        if (foe->structure) g.turret = *at; else g.prey = *at;
    }
    // Shardlings from her pods, while the nest holds fewer than its most.
    const auto [packs, flyers] = hordeIn(r);
    (void)flyers;
    if (nestRegion() == r && !siege_) {
        int64_t key = 1;
        double best = kFar;
        if (foe)
            for (const BroodGroup& g : brood_[r].groups)
                if (g.nest && g.key >= 1 && g.key <= WorldNest::packs && distance(g.site, foe->position) < best) {
                    best = distance(g.site, foe->position);
                    key = g.key;
                }
        for (int64_t i = 0; i < std::min<int64_t>(Matriarch::hatch, Matriarch::nestShardlingsMax - packs); i++) {
            hatch(r, UnitKind::shardling, key, at0);
            // The hatchlings join the rally at once.
            for (const BroodGroup& g : brood_[r].groups)
                if (g.nest && g.key == key)
                    for (Unit& u : st.units)
                        if (u.bug && u.bug->group == g.id && u.bug->rallyUntil < st.time) u.bug->rallyUntil = st.time + Matriarch::rallyFor;
        }
        for (BroodGroup& g : brood_[r].groups)
            if (g.nest && g.key == key && foe) (foe->structure ? g.turret : g.prey) = *at;
    }
    net::Sky::Roar x;
    x.id = nextRoar_++;
    x.region = int64_t(r);
    x.position = at0;
    x.at = clock_;
    roars_.push_back(x);
    sky_.roars = roars_;
}

// MARK: - Her death

void WorldMap::queenHit(size_t r, const Simulation::BugHit& h) {
    // A pilot's Prospector, or a defence its builder placed.
    std::optional<PilotId> who;
    for (const auto& [pilot, p] : pilots_)
        if (!p.flight && p.region == r && p.unit == h.shooter) who = pilot;
    if (!who && r < builtBy_.size())
        if (const auto by = builtBy_[r].find(h.shooter); by != builtBy_[r].end())
            for (const auto& [pilot, p] : pilots_)
                if (!p.key.empty() && p.key == by->second) who = pilot;
    // N4: a mercenary's hits count to its hirer on line, as its defences'.
    if (!who)
        for (const auto& [pilot, p] : pilots_)
            for (const PilotState::Merc& m : p.mercs)
                if (m.unit == h.shooter && mercIn(m) == r) who = pilot;
    if (who && h.hp > 0) nest_.damage[*who] += h.hp;
    if (h.kill) queenDown(r);
}

void WorldMap::queenDown(size_t r) {
    NestRegion& n = nest_;
    if (n.downAt) return;
    n.queen = std::nullopt;
    n.downAt = clock_;
    n.backAt = clock_ + Matriarch::returnDays * params_.dayLength;
    n.calmUntil = clock_ + Matriarch::calmDays * params_.dayLength;
    n.shardlings = n.gloamwings = 0;
    n.seen.clear();
    siege_ = std::nullopt;
    // Her rewards, by each pilot's share of the damage.
    double total = 0;
    for (const auto& [pilot, d] : n.damage) total += d;
    const std::vector<StructureKind> rare = rareBlueprints();
    std::string top, topKey;
    double most = 0;
    int64_t fighters = 0;
    for (const auto& [pilot, d] : n.damage) {
        PilotState* p = pilotState(pilot);
        if (!p || !(d > 0)) continue;
        fighters += 1;
        if (d > most) {
            most = d;
            top = callsign(p->key);
            topKey = p->key;
        }
        const double share = std::max(d / std::max(1.0, total), Matriarch::rewardFloor);
        const int64_t ore = std::llround(double(Matriarch::rewardOre) * share);
        const int64_t mh = std::llround(double(Matriarch::rewardHydrogen) * share);
        const StructureKind bp = rare[size_t(broodDice_() % rare.size())];
        if (!p->flight) bankOut(*p, p->region);
        p->wallet.ore += ore;
        p->wallet.hydrogen += mh;
        if (!p->flight) bankIn(*p, p->region);
        p->progress.giveBlueprint(bp, 1);
        p->news.push_back(News{NewsKind::matriarch, "share/" + std::to_string(mh) + "/" + std::string(rawValue(bp)), 0, ore, ""});
        goalDid(*p, "matriarch");
        // A hunt counts her for every pilot who fought her (K2).
        broodKill(pilot, UnitKind::matriarch, r);
        pushProgress(pilot);
    }
    n.damage.clear();
    // S4: she counts as a bug felled to her hardest hitter.
    if (!topKey.empty()) standNote(topKey, &Standing::bugs, 1);
    // N4: a hunt on is won.
    huntEnd(true, false, top, fighters);
    // The whole world hears it.
    for (auto& [pilot, p] : pilots_) p.news.push_back(News{NewsKind::matriarch, "down", 0, fighters, top});
    awayNoteAll(net::AwayRow{0, net::AwayKind::queenDown, top, "", world_->regions[r].file.name, fighters});
    // The horde scatters, and every group of the region with it.
    if (r < brood_.size())
        for (BroodGroup& g : brood_[r].groups) {
            g.nest = false;
            g.prey = std::nullopt;
            g.turret = std::nullopt;
            g.raid = false;
            g.leaving = true;
            g.lingerUntil = clock_;
        }
    if (const auto nr = nestRegion(); nr && *nr != r && *nr < brood_.size())
        for (BroodGroup& g : brood_[*nr].groups) {
            g.nest = false;
            g.leaving = true;
            g.lingerUntil = clock_;
        }
}

void WorldMap::matriarchReturnNow() {
    if (nest_.backAt) nest_.backAt = clock_;
    nest_.calmUntil = clock_;
}

// MARK: - The siege (Z2's hook)

bool WorldMap::matriarchSiege(size_t r, Vec2 at, double radius, int64_t escort, std::optional<Vec2> from) {
    const auto nr = nestRegion();
    if (!nr || nest_.downAt || siege_ || r >= regions_.size() || !regions_[r]->simulation().broodSide) return false;
    if (brood_.size() < regions_.size()) brood_.resize(regions_.size());
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    std::optional<int64_t> queen;
    if (r == *nr && !from) {
        // From the nest: she walks there.
        for (const Unit& u : st.units)
            if (nest_.queen && u.id == *nest_.queen && u.hp > 0) queen = u.id;
        if (!queen) {
            const auto g = spawnBroodGroup(r, UnitKind::matriarch, 1, Poi::nest, WorldNest::queenKey, *nestAt(r), true);
            if (!g) return false;
            queen = brood_[r].groups.back().bugs.front();
            nest_.queen = queen;
        }
    } else {
        // From elsewhere: she comes in out of sight at the region's edge,
        // as any group, and leaves her nest's region.
        if (nest_.queen) {
            Simulation& home = regions_[*nr]->simulation();
            std::erase_if(home.state.units, [&](const Unit& u) { return u.id == *nest_.queen; });
            home.reindex();
            for (BroodGroup& g : brood_[*nr].groups) std::erase_if(g.bugs, [&](int64_t id) { return id == *nest_.queen; });
            std::erase_if(brood_[*nr].groups, [](const BroodGroup& g) { return g.bugs.empty(); });
            nest_.queen = std::nullopt;
        }
        // A canyon's tunnel (Z2), else the region's edge.
        const auto in = from ? from : spawnPoint(r, at);
        if (!in) return false;
        const auto g = spawnBroodGroup(r, UnitKind::matriarch, 1, Poi::nest, WorldNest::queenKey, *in, false);
        if (!g) return false;
        queen = brood_[r].groups.back().bugs.front();
    }
    Siege s;
    s.region = r;
    s.at = at;
    s.radius = radius;
    s.escort = escort;
    siege_ = s;
    // Her escort: Shardlings at her side, on the same turrets.
    const auto q = st.unit(*queen);
    siegeEscort_ = 0;
    if (escort > 0 && q) {
        if (const auto e = spawnBroodGroup(r, UnitKind::shardling, escort, Poi::nest, WorldNest::escortKey, q->position, false)) {
            siegeEscort_ = *e;
            BroodGroup& g = brood_[r].groups.back();
            g.site = at;
            g.radius = radius;
            g.leash = WorldBrood::leash;
            g.lingerUntil = kFar;
        }
    }
    for (Unit& u : st.units)
        if (u.id == *queen && u.bug) {
            u.bug->site = at;
            u.bug->tether = radius;
            u.bug->prey = std::nullopt;
            u.bug->raid = std::nullopt;
        }
    for (BroodGroup& g : brood_[r].groups)
        if (g.queen) g.site = at;
    siegeThink();
    return true;
}

void WorldMap::siegeThink() {
    if (!siege_) return;
    Siege& s = *siege_;
    Simulation& sim = regions_[s.region]->simulation();
    GameState& st = sim.state;
    Unit* q = nullptr;
    for (Unit& u : st.units)
        if (u.kind == UnitKind::matriarch && u.hp > 0 && u.bug) q = &u;
    if (!q) {
        // Felled out there (her death's rewards came with the hit), or
        // taken away by a region gone to sleep: the siege is over.
        siege_ = std::nullopt;
        return;
    }
    BroodGroup* escort = nullptr;
    for (BroodGroup& g : brood_[s.region].groups)
        if (g.id == siegeEscort_) escort = &g;
    const auto nr = nestRegion();
    if (!s.home) {
        // The nearest turret of the realm left in the zone.
        std::optional<int64_t> next;
        double best = kFar;
        for (const Structure& b : st.structures)
            if (realm(b) && b.hp > 0 && turret(b.kind) && distance(b.position, s.at) <= s.radius + Rules::radius(b.kind) &&
                distance(b.position, q->position) < best) {
                best = distance(b.position, q->position);
                next = b.id;
            }
        if (next) {
            q->bug->raid = next;
            q->bug->prey = std::nullopt;
            if (escort) {
                escort->turret = next;
                escort->prey = std::nullopt;
            }
            return;
        }
        // Every turret down: home.
        s.home = true;
        q->bug->raid = std::nullopt;
        q->bug->prey = std::nullopt;
        if (nr && *nr == s.region) {
            q->bug->site = *nestAt(s.region);
            q->bug->tether = Matriarch::tether;
        } else {
            q->bug->site = exitPoint(s.region, q->position);
            q->bug->tether = 0;
        }
        for (BroodGroup& g : brood_[s.region].groups)
            if (g.queen) g.site = q->bug->site;
        if (escort) {
            escort->turret = std::nullopt;
            escort->prey = std::nullopt;
            escort->leaving = true;
            escort->lingerUntil = clock_;
        }
    }
    // On her way home: in her nest's region she is home at the nest; from
    // elsewhere she leaves at the edge, out of sight, and is back at the
    // nest the next time its region wakes.
    q->bug->raid = std::nullopt;
    q->bug->prey = std::nullopt;
    if (distance(q->position, q->bug->site) > 2.5) return;
    if (nr && *nr == s.region) {
        siege_ = std::nullopt;
        return;
    }
    const int64_t id = q->id;
    std::erase_if(st.units, [&](const Unit& u) { return u.id == id; });
    sim.reindex();
    for (BroodGroup& g : brood_[s.region].groups) std::erase_if(g.bugs, [&](int64_t b) { return b == id; });
    std::erase_if(brood_[s.region].groups, [](const BroodGroup& g) { return g.bugs.empty(); });
    siege_ = std::nullopt;
}

// MARK: - Dev and shots

std::string WorldMap::devMatriarch(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: matriarch: no world, or no pilot";
    const auto nr = nestRegion();
    if (!nr) return "dev: matriarch: no nest in the world";
    auto& [pilot, p] = *devPilot();
    const Vec2 home = *nestAt(*nr);
    if (what == "back") {
        nest_.held = false;
        matriarchReturnNow();
        broodNow(*nr);
        return "dev: matriarch back";
    }
    // `here`: the pilot taken to the nest's region first, toward its pad.
    if (p.region != *nr && (what == "here" || what == "near")) {
        const region::Built& places = *regions_[*nr]->places();
        const Vec2 pad = places.pads.empty() ? home + Vec2(-40, 0) : places.pads.front().at;
        if (!devTransfer(pilot, *nr, home + normalize(pad - home) * 45)) return "dev: matriarch: the pilot can't be taken there";
    }
    if (p.region != *nr) return "dev: matriarch: the pilot is not in " + world_->regions[*nr].file.id;
    Simulation& sim = regions_[*nr]->simulation();
    GameState& st = sim.state;
    const auto me = p.unit ? st.unit(*p.unit) : std::nullopt;
    if (!me) return "dev: matriarch: the pilot drives nothing";
    // Make sure she and her nest are there now.
    if (brood_.size() < regions_.size()) brood_.resize(regions_.size());
    brood_[*nr].lastNear = clock_;
    nestThink(*nr);
    Unit* q = nullptr;
    for (Unit& u : st.units)
        if (nest_.queen && u.id == *nest_.queen && u.hp > 0) q = &u;
    if (what == "here" || what == "near") {
        // Her horde stays its own; the pilot is set `d` cells off her, facing her.
        // `d` cells off; at `bearing` degrees (ground frame, -90 north), else
        // on the pilot's own bearing from her.
        double d = 40, bearing = 1e9;
        in >> d >> bearing;
        const double a = bearing < 1e8 ? bearing * pi / 180 : std::atan2(me->position.y - home.y, me->position.x - home.x);
        Vec2 at = home + Vec2(std::cos(a), std::sin(a)) * d;
        if (sim.nav) at = sim.nav->nearestFree(at).value_or(at);
        for (Unit& u : st.units)
            if (u.id == *p.unit) {
                u.position = at;
                u.heading = std::atan2(home.y - at.y, home.x - at.x);
                u.goal = std::nullopt;
                u.waypoints = std::nullopt;
            }
        sim.reindex();
        return "dev: matriarch " + what + ": the pilot " + std::to_string(int(d)) + " cells from her";
    }
    if (!q) return "dev: matriarch: she is not at her nest";
    if (what == "roar") {
        roar(*nr, *q, *p.unit);
        return "dev: matriarch roars";
    }
    if (what == "calm") {
        // The horde holds still for pictures: nobody turns on the pilot.
        for (BroodGroup& g : brood_[*nr].groups) g.still = g.nest;
        nest_.held = true;
        q->bug->prey = std::nullopt;
        q->bug->raid = std::nullopt;
        return "dev: matriarch calm";
    }
    if (what == "slam") {
        // One slam on the pilot: she names it her foe; `calm` after takes it back.
        q->bug->prey = *p.unit;
        q->cooldown = std::nullopt;
        q->bug->roarUntil = -1;
        return "dev: matriarch slams";
    }
    if (what == "hurt") {
        double share = 0.5;
        in >> share;
        q->hp = std::max(1.0, Rules::hp(UnitKind::matriarch) * (1 - share));
        q->hitAt = st.time;
        nest_.damage[pilot] += Rules::hp(UnitKind::matriarch) * share;
        return "dev: matriarch hurt";
    }
    if (what == "down") {
        // Felled by the pilot's hit, as in a fight: her body lies, her
        // rewards and the world's news come with the hit (`queenHit`).
        const Unit shooter = *me;
        sim.hit(q->id, q->hp + 1, false, &shooter);
        return "dev: matriarch down";
    }
    if (what == "siege") {
        return matriarchSiege(*nr, me->position, 12, 6) ? "dev: matriarch siege at the pilot" : "dev: matriarch siege: refused";
    }
    return "dev: matriarch: '" + what + "' is not here|near D [BEARING]|roar|calm|slam|hurt|down|back|siege";
}

} // namespace ac::server
