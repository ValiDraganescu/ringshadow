// The Prospector's skill tree in the region's sim (MMORPG mode,
// docs/prospector-progression.md): the members declared in
// Simulation+Progression.members.h. C++ only, no Swift original.
//
// Only while `progress` is set, and only for the driven Prospector (its
// own buildings for the building nodes). What needs the server or the
// world (chips, blueprints, building limits, flights, sites) is not here.
#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ac {

namespace {

/// The first of `effects` of case `T`.
template <class T> const T* first(const std::vector<SkillEffect>& effects) {
    for (const SkillEffect& e : effects) {
        if (const T* t = e.as<T>()) return t;
    }
    return nullptr;
}

/// A whole number of seconds, for "Ready in 12 s".
std::string seconds(const char* format, double s) {
    char buf[64];
    std::snprintf(buf, sizeof buf, format, std::ceil(s - 1e-9));
    return buf;
}

} // namespace

// MARK: - The pilot's progress

void Simulation::setProgress(const std::optional<ProspectorProgress>& p) {
    progress = p;
    skillEffects.clear();
    if (!p) return;
    for (const auto& [s, rank] : p->ranks) {
        const auto e = Progression::effects(s, rank);
        skillEffects.insert(skillEffects.end(), e.begin(), e.end());
    }
}

bool Simulation::skilled(const Unit& u) const {
    return progress && u.kind == UnitKind::prospector && piloted(u.id);
}

Boost Simulation::skillBoost(SkillStat stat) const {
    if (!progress) return Boost::none;
    Boost b;
    for (const SkillEffect& e : skillEffects) {
        if (const auto* s = e.as<SkillEffect::Stat>(); s && s->stat == stat) b.add(s->change);
    }
    return b;
}

void Simulation::addSkills(const Stat& stat, const Unit& u, Boost& b) const {
    if (!skilled(u)) return;
    for (const SkillEffect& e : skillEffects) {
        const auto* c = e.as<SkillEffect::Core>();
        if (!c) continue;
        if (const auto* s = c->effect.as<Effect::Stat>(); s && s->scope.is<Scope::Driven>() && s->stat == stat)
            b.add(s->change);
    }
    if (stat.is<Stat::Speed>()) {
        // Quicksilver stride, Dust runner.
        const bool hydrogen = u.carrying > 0 && u.hydrogen == true;
        const auto carrying = hydrogen         ? std::optional<SkillStat>(SkillStat::speedCarryingHydrogen)
                              : u.carrying <= 0 ? std::optional<SkillStat>(SkillStat::speedCarryingNothing)
                                                : std::nullopt;
        if (carrying) {
            for (const SkillEffect& e : skillEffects) {
                if (const auto* s = e.as<SkillEffect::Stat>(); s && s->stat == *carrying) b.add(s->change);
            }
        }
    }
    // The actives running now: their channels.
    const auto clock = skillClocks.find(u.id);
    if (clock == skillClocks.end()) return;
    for (const auto& [s, until] : clock->second.until) {
        if (!(until > state.time)) continue;
        for (const SkillEffect& e : Progression::node(s).effects) {
            if (const auto* a = e.as<SkillEffect::Burst>()) {
                const ActiveBurst& x = a->burst;
                const double more = stat.is<Stat::Drill>()         ? x.drill
                                    : stat.is<Stat::Build>()       ? x.build
                                    : stat.is<Stat::Repair>()      ? x.repair
                                    : stat.is<Stat::Speed>()       ? x.speed
                                    : stat.is<Stat::DamageTaken>() ? x.damageTaken
                                                                   : 0.0;
                if (more != 0) b.add(Change::Percent{more});
            } else if (const auto* c = e.as<SkillEffect::Core>()) {
                if (const auto* k = c->effect.as<Effect::Burst>()) {
                    if (stat.is<Stat::Speed>() && k->burst.speed != 0) b.add(Change::Percent{k->burst.speed});
                    if (stat.is<Stat::FireRate>() && k->burst.fireRate != 0) b.add(Change::Percent{k->burst.fireRate});
                }
            }
        }
    }
}

// MARK: - Every Nth

bool Simulation::everyNth(Work work, const Unit& u) {
    if (!skilled(u)) return false;
    int64_t n = 0;
    for (const SkillEffect& e : skillEffects) {
        if (const auto* x = e.as<SkillEffect::EveryNth>(); x && x->work == work) n = x->n;
    }
    if (n <= 0) return false;
    int64_t& count = skillClocks[u.id].counts[work];
    count += 1;
    return count % n == 0;
}

// MARK: - The pilot's buildings

bool Simulation::ownBuilding(const Structure& s) const { return progress && pilotBuildings.count(s.id) > 0; }

double Simulation::fullHP(const Structure& s) const {
    const double hp = Rules::hp(s.kind);
    return ownBuilding(s) ? skillBoost(SkillStat::ownBuildingHp).apply(hp) : hp;
}

// MARK: - Work

double Simulation::workReach(const Unit& u) const {
    return pilotHydrogenReach + (skilled(u) ? skillBoost(SkillStat::workReach).plus : 0.0);
}

int64_t Simulation::drawn(const Unit& u, int64_t load, bool hydrogen) const {
    if (!(skilled(u) && load > 0)) return load;
    const Boost b = skillBoost(hydrogen ? SkillStat::wellYield : SkillStat::depositYield);
    return max<int64_t>(1, b.apply(load));
}

int64_t Simulation::unloaded(const Unit& u, int64_t load, bool hydrogen) const {
    if (!skilled(u)) return load;
    return skillBoost(hydrogen ? SkillStat::unloadHydrogen : SkillStat::unloadOre).apply(load);
}

void Simulation::takeOre(Unit& u, int64_t at, std::vector<GameEvent>& events) {
    OreDeposit& p = state.patches[static_cast<size_t>(at)];
    // Rich vein: a pick's every third trip, or the node's every Nth, carries
    // double (both count their trips); Motherlode: some carry triple.
    const bool third = thirdTime(Action::oreTrip, u);
    const bool nth = everyNth(Work::oreTrip, u);
    int64_t times = third || nth ? 2 : 1;
    if (skilled(u)) {
        const double triple = skillBoost(SkillStat::loadTriple).plus;
        if (triple > 0 && roll(u.id, 0x6d6f7468) < triple) times = 3;
    }
    const int64_t want = load(u, false) * times;
    const int64_t cost = drawn(u, want, false);
    if (cost <= p.remaining) {
        u.carrying = want;
        p.remaining -= cost;
    } else {
        u.carrying = min(want, p.remaining);
        p.remaining = 0;
    }
    u.hydrogen = std::nullopt;
    if (p.remaining <= 0) {
        p.depletedAt = state.time;
        events.push_back(GameEvent::PatchDepleted{at});
        // Quick regrowth: a deposit the pilot empties grows back sooner.
        if (skilled(u)) {
            const double share = skillBoost(SkillStat::regrowTime).rate();
            if (share != 1) regrowShare[at] = max(0.0, share);
        }
    }
}

void Simulation::pumpLoad(Unit& u, Well& g) {
    // Twin canister: every Nth pump carries double.
    const int64_t want = load(u, true) * (everyNth(Work::pump, u) ? 2 : 1);
    const int64_t cost = drawn(u, want, true);
    if (cost <= g.remaining) {
        u.carrying = want;
        g.remaining -= cost;
    } else {
        u.carrying = min(want, g.remaining);
        g.remaining = 0;
    }
    u.hydrogen = true;
}

bool Simulation::takeQuickfill(const Unit& u) {
    if (!skilled(u)) return false;
    const auto c = skillClocks.find(u.id);
    if (c == skillClocks.end() || !c->second.quickfill) return false;
    c->second.quickfill = false;
    return true;
}

std::optional<int64_t> Simulation::fieldWell(const Unit& u) const {
    if (!(skill<SkillEffect::FieldPump>(u) && state.wells)) return std::nullopt;
    std::optional<int64_t> best;
    const double reach = Rules::radius(StructureKind::derrick) + pilotHydrogenReach;
    for (int64_t i = 0; i < static_cast<int64_t>(state.wells->size()); ++i) {
        const Well& w = (*state.wells)[static_cast<size_t>(i)];
        if (!(w.remaining > 0 && distance(u.position, w.position) <= reach
              && ahead(u, w.position)))
            continue;
        const bool derrick = std::any_of(state.structures.begin(), state.structures.end(),
                                         [&](const Structure& s) { return distance(s.position, w.position) < 0.5; });
        if (derrick) continue;
        if (!best || distance(w.position, u.position) < distance((*state.wells)[static_cast<size_t>(*best)].position, u.position))
            best = i;
    }
    return best;
}

void Simulation::fieldPumpStart(Unit& u, int64_t well) {
    if (!(state.wells && well >= 0 && well < static_cast<int64_t>(state.wells->size()))) return;
    if (fieldPumps.count(u.id)) return;
    // No harvester mark: that is a Derrick's (`releaseStaleMiners`).
    fieldPumps[u.id] = well;
    u.patch = std::nullopt;
    u.structure = std::nullopt;
    u.timer = Rules::hydrogenTime;
}

void Simulation::fieldPumpStop(Unit& u) { fieldPumps.erase(u.id); }

bool Simulation::fieldPumpBusy(Unit& u, const Pilot& p, double dt) {
    const auto it = fieldPumps.find(u.id);
    if (it == fieldPumps.end()) return false;
    const int64_t gi = it->second;
    const auto pump = skill<SkillEffect::FieldPump>(u);
    const bool walking = length(p.walk) > 0.1;
    if (!(pump && state.wells && gi >= 0 && gi < static_cast<int64_t>(state.wells->size())
          && (*state.wells)[static_cast<size_t>(gi)].remaining > 0 && p.hold && !walking && u.carrying <= 0)) {
        fieldPumpStop(u);
        return false;
    }
    Well& g = (*state.wells)[static_cast<size_t>(gi)];
    turn(u, g.position, dt);
    u.timer -= dt * rate(Stat::Hydrogen{}, u) * pump->share;
    if (u.timer <= 0) {
        pumpLoad(u, g);
        fieldPumpStop(u);
    }
    return true;
}

void Simulation::collectTank(const Unit& u, int64_t derrick) {
    if (!skilled(u)) return;
    const auto it = derrickTanks.find(derrick);
    if (it == derrickTanks.end()) return;
    const double whole = std::floor(it->second);
    if (!(whole >= 1 && u.owner >= 0 && u.owner < static_cast<int64_t>(state.players.size()))) return;
    state.players[static_cast<size_t>(u.owner)].hydrogen += static_cast<int64_t>(whole);
    it->second -= whole;
}

// MARK: - Actives

std::optional<Skill> Simulation::slotted(const Unit& u, int64_t slot) const {
    if (!(skilled(u) && slot >= 0 && slot < Progression::slotCount)) return std::nullopt;
    const auto s = progress->slots[static_cast<size_t>(slot)];
    if (!(s && progress->rank(*s) > 0)) return std::nullopt;
    return s;
}

std::optional<double> Simulation::activeReady(int64_t unit, Skill s) const {
    const auto c = skillClocks.find(unit);
    if (c == skillClocks.end()) return std::nullopt;
    const auto r = c->second.ready.find(s);
    if (r == c->second.ready.end()) return std::nullopt;
    return r->second;
}

bool Simulation::activeRunning(int64_t unit, Skill s) const {
    const auto c = skillClocks.find(unit);
    if (c == skillClocks.end()) return false;
    const auto r = c->second.until.find(s);
    return r != c->second.until.end() && r->second > state.time;
}

std::optional<int64_t> Simulation::seismicDeposit(const Unit& u, double radius) const {
    std::optional<int64_t> best;
    for (int64_t i = 0; i < static_cast<int64_t>(state.patches.size()); ++i) {
        const OreDeposit& p = state.patches[static_cast<size_t>(i)];
        if (!(p.remaining > 0 && NavGrid::insidePatch(u.position, p, radius))) continue;
        if (!best || distance(p.position, u.position) < distance(state.patches[static_cast<size_t>(*best)].position, u.position))
            best = i;
    }
    return best;
}

std::optional<int64_t> Simulation::weldTarget(const Unit& u, double radius) const {
    const Structure* best = nullptr;
    for (const Structure& s : state.structures) {
        if (!(ownBuilding(s) && s.complete() && s.hp > 0 && s.hp < fullHP(s)
              && distance(u.position, s.position) - Rules::radius(s.kind) <= radius))
            continue;
        if (!best || distance(s.position, u.position) < distance(best->position, u.position)) best = &s;
    }
    if (!best) return std::nullopt;
    return best->id;
}

std::optional<std::string> Simulation::activeRefusal(const Unit& u, Skill s) const {
    const SkillNode& n = Progression::node(s);
    if (!n.active) return n.title + " is not an active";
    if (!(skilled(u) && progress->rank(s) > 0)) return n.title + " is not learned";
    if (const auto r = activeReady(u.id, s); r && *r > state.time) return seconds("Ready in %.0f s", *r - state.time);
    for (const SkillEffect& e : n.effects) {
        if (const auto* a = e.as<SkillEffect::Burst>(); a && a->burst.hydrogenCost > 0) {
            const int64_t cost = static_cast<int64_t>(std::ceil(a->burst.hydrogenCost));
            const int64_t carried = u.carrying > 0 && u.hydrogen == true ? u.carrying : 0;
            const int64_t wallet = u.owner >= 0 && u.owner < static_cast<int64_t>(state.players.size())
                ? state.players[static_cast<size_t>(u.owner)].hydrogen
                : 0;
            if (carried + wallet < cost) return "Needs " + std::to_string(cost) + " MH";
        } else if (const auto* c = e.as<SkillEffect::Core>()) {
            if (const auto* b = c->effect.as<Effect::Burst>(); b && b->burst.hpCost > 0 && u.hp <= b->burst.hpCost)
                return std::string("Not enough hp");
        } else if (const auto* q = e.as<SkillEffect::SeismicCharge>()) {
            if (u.carrying > 0 && u.hydrogen == true) return std::string("Carrying MH");
            if (u.carrying >= load(u, false)) return std::string("The cargo is full");
            if (!seismicDeposit(u, q->radius)) return seconds("No ore within %.0f cells", q->radius);
        } else if (const auto* w = e.as<SkillEffect::EmergencyWeld>()) {
            if (!weldTarget(u, w->radius)) return std::string("Nothing of yours to weld");
        }
    }
    return std::nullopt;
}

bool Simulation::useSlot(Unit& u, int64_t slot, std::vector<GameEvent>& events) {
    const auto s = slotted(u, slot);
    if (!s || activeRefusal(u, *s)) return false;
    const SkillNode& n = Progression::node(*s);
    const double now = state.time;
    SkillClock& clock = skillClocks[u.id];
    clock.ready[*s] = now + n.active->cooldown;
    for (const SkillEffect& e : n.effects) {
        if (const auto* a = e.as<SkillEffect::Burst>()) {
            // Hydrogen boost: the cargo first, then the wallet.
            int64_t cost = static_cast<int64_t>(std::ceil(a->burst.hydrogenCost));
            if (cost > 0 && u.carrying > 0 && u.hydrogen == true) {
                const int64_t from = min(cost, u.carrying);
                u.carrying -= from;
                cost -= from;
                if (u.carrying <= 0) u.hydrogen = std::nullopt;
            }
            if (cost > 0) state.players[static_cast<size_t>(u.owner)].hydrogen -= cost;
            clock.until[*s] = now + a->burst.seconds;
        } else if (const auto* c = e.as<SkillEffect::Core>()) {
            if (const auto* b = c->effect.as<Effect::Burst>()) {
                u.hp -= b->burst.hpCost;
                clock.until[*s] = now + b->burst.seconds;
            } else if (const auto* g = c->effect.as<Effect::Grenade>()) {
                throwGrenade(u, g->grenade, events);
            }
        } else if (const auto* q = e.as<SkillEffect::SeismicCharge>()) {
            const int64_t at = *seismicDeposit(u, q->radius);
            OreDeposit& p = state.patches[static_cast<size_t>(at)];
            const int64_t want = load(u, false) - u.carrying;
            const int64_t cost = drawn(u, want, false);
            const int64_t got = cost <= p.remaining ? want : min(want, p.remaining);
            p.remaining = cost <= p.remaining ? p.remaining - cost : 0;
            u.carrying += got;
            u.hydrogen = std::nullopt;
            if (p.remaining <= 0) {
                p.depletedAt = now;
                events.push_back(GameEvent::PatchDepleted{at});
                const double share = skillBoost(SkillStat::regrowTime).rate();
                if (share != 1) regrowShare[at] = max(0.0, share);
            }
        } else if (e.is<SkillEffect::Quickfill>()) {
            clock.quickfill = true;
        } else if (const auto* w = e.as<SkillEffect::EmergencyWeld>()) {
            const int64_t id = *weldTarget(u, w->radius);
            for (Structure& b : state.structures) {
                if (b.id != id) continue;
                const double full = fullHP(b);
                b.hp = min(full, b.hp + full * w->share);
            }
        }
    }
    return true;
}

// MARK: - Upkeep

void Simulation::stepSkills(double dt) {
    if (!progress) return;
    const auto* tender = first<SkillEffect::DerrickTender>(skillEffects);
    const auto* spring = first<SkillEffect::Wellspring>(skillEffects);
    if (!tender && !spring) return;
    for (const Structure& s : state.structures) {
        if (!(s.kind == StructureKind::derrick && s.complete() && s.hp > 0 && ownBuilding(s))) continue;
        if (tender) {
            double& tank = derrickTanks[s.id];
            tank = min(tender->tank, tank + tender->perMinute / 60.0 * dt);
        }
        if (spring) {
            const auto gi = wellIndex(s);
            if (!gi) continue;
            Well& w = (*state.wells)[static_cast<size_t>(*gi)];
            if (w.remaining >= Rules::wellHydrogen) continue;
            double& grown = wellGrowth[*gi];
            grown += spring->perMinute / 60.0 * dt;
            const double whole = std::floor(grown);
            if (whole >= 1) {
                w.remaining = min(Rules::wellHydrogen, w.remaining + static_cast<int64_t>(whole));
                grown -= whole;
            }
        }
    }
}

} // namespace ac
