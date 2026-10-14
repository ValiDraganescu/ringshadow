// The Prospector's progression in a world (WorldMap.h, docs/prospector-progression.md):
// the XP the server counts from its regions' work and the one-offs, the skill
// tree's requests, what the client is told, and the wells growing back.

#include "WorldMap.h"

#include "NavGrid.h"
#include "Ringshadow.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>

namespace ac::server {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

namespace {

/// A building's price in a world, ore and MH (the welding and repair XP).
double price(StructureKind kind) { return double(Progression::worldOre(kind) + Progression::worldHydrogen(kind)); }

/// An ore load just drilled from `patch` under the ringshadow (Ringshadow.h)
/// carries half again, so its unload pays that ore and its XP.
void sweptLoad(Simulation& sim, int64_t unit, std::optional<int64_t> patch, double clock, double day) {
    if (!sim.map || !patch || size_t(*patch) >= sim.state.patches.size()) return;
    for (Unit& u : sim.state.units)
        if (u.id == unit)
            u.carrying = Ringshadow::load(u.carrying, clock, sim.state.patches[size_t(*patch)].position, sim.map->bounds, day);
}

} // namespace

std::string WorldXp::built(StructureKind kind) { return "built-" + std::string(rawValue(kind)); }

double WorldXp::feat(const std::string& id) {
    if (id == built(StructureKind::citadel)) return citadel;
    if (id.rfind("built-", 0) == 0) return building;
    if (id == hydrogenFeat) return firstHydrogen;
    if (id == flightFeat) return firstFlight;
    if (id == emptiedFeat) return firstEmptied;
    return 0;
}

WorldMap::PilotState* WorldMap::pilotState(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() || !world_ ? nullptr : &it->second;
}

std::optional<ProspectorProgress> WorldMap::progress(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_) return std::nullopt;
    return it->second.progress;
}

void WorldMap::setProgress(PilotId pilot, const ProspectorProgress& progress) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    const int64_t level = p->progress.level();
    p->progress = progress;
    if (progress.level() != level) p->news.push_back(News{NewsKind::level, "", 0, progress.level()});
    pushProgress(pilot);
}

bool WorldMap::owns(const PilotState& p, size_t r, int64_t building) const {
    const auto by = builtBy_[r].find(building);
    return by != builtBy_[r].end() && by->second == p.key;
}

std::set<int64_t> WorldMap::ownBuildings(const PilotState& p, size_t r) const {
    std::set<int64_t> out;
    for (const auto& [id, key] : builtBy_[r])
        if (key == p.key) out.insert(id);
    return out;
}

void WorldMap::pushProgress(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p || p->flight) return;
    Region& r = *regions_[p->region];
    if (r.joined(pilot)) r.setProgress(pilot, p->progress, ownBuildings(*p, p->region));
}

void WorldMap::earn(PilotState& p, double xp) {
    if (!(xp > 0) || !std::isfinite(xp)) return;
    const int64_t level = p.progress.level();
    p.progress.xp += xp;
    // A level reached: its points wait to be spent (the client is told).
    if (p.progress.level() != level) p.news.push_back(News{NewsKind::level, "", 0, p.progress.level()});
}

void WorldMap::earn(PilotId pilot, double xp) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    const int64_t level = p->progress.level();
    earn(*p, xp);
    if (p->progress.level() != level) pushProgress(pilot);
}

bool WorldMap::oneOff(PilotId pilot, std::set<std::string> ProspectorProgress::*set, const std::string& id, double xp,
                      NewsKind kind) {
    PilotState* p = pilotState(pilot);
    if (!p || !((p->progress.*set).insert(id).second)) return false;
    p->news.push_back(News{kind, id, xp, 0});
    earn(*p, xp);
    pushProgress(pilot);
    return true;
}

bool WorldMap::firstVisit(PilotId pilot, const std::string& region) {
    // The start region is where every pilot begins: known, no XP.
    const bool start = world_ && region == world_->regions[start_].file.id;
    return oneOff(pilot, &ProspectorProgress::regions, region, start ? 0 : WorldXp::region, NewsKind::region);
}

bool WorldMap::firstLanding(PilotId pilot, const std::string& pad) {
    const bool start = world_ && pad == world_->regions[start_].file.id + "/" + world_->file.startPad;
    return oneOff(pilot, &ProspectorProgress::pads, pad, start ? 0 : WorldXp::pad, NewsKind::pad);
}

bool WorldMap::feat(PilotId pilot, const std::string& id) {
    return oneOff(pilot, &ProspectorProgress::feats, id, WorldXp::feat(id), NewsKind::feat);
}

bool WorldMap::findBlueprint(PilotId pilot, StructureKind kind, int64_t floor) {
    PilotState* p = pilotState(pilot);
    if (!p || !p->progress.blueprints.insert(kind).second) return false;
    const double xp = WorldXp::blueprintPerFloor * double(std::max<int64_t>(1, floor));
    // The level it opens at for this pilot (Blueprint savant).
    p->news.push_back(News{NewsKind::blueprint, std::string(rawValue(kind)), xp, p->progress.floor(kind)});
    earn(*p, xp);
    pushProgress(pilot);
    return true;
}

// MARK: - The skill tree's requests

std::optional<std::string> WorldMap::spend(PilotId pilot, Skill skill) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = p->progress.spendRefusal(skill)) return why;
    if (!p->progress.spend(skill, clock_)) return "Can't spend a point there now";
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::undo(PilotId pilot, Skill skill) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = p->progress.undoRefusal(skill, clock_)) return why;
    if (!p->progress.undo(skill, clock_)) return "Can't take that point back now";
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::slot(PilotId pilot, int64_t index, std::optional<Skill> skill) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = p->progress.slotRefusal(index, skill)) return why;
    if (!p->progress.slot(index, skill)) return "Can't put that there now";
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::respec(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    static const std::string where = "Respec at your Citadel: stand where you unload";
    if (p->flight) return where;
    // At its own finished Citadel, in its unload range.
    Region& r = *regions_[p->region];
    const auto unit = r.driving(pilot);
    const auto u = unit ? r.simulation().state.unit(*unit) : std::nullopt;
    if (!u) return where;
    const bool home = std::any_of(r.simulation().state.structures.begin(), r.simulation().state.structures.end(),
                                  [&](const Structure& b) {
                                      return b.kind == StructureKind::citadel && b.complete() && owns(*p, p->region, b.id) &&
                                             NavGrid::insideStructure(u->position, b, Simulation::pilotDepositReach);
                                  });
    if (!home) return where;
    bankOut(*p, p->region);
    if (auto why = p->progress.respecRefusal(p->wallet.hydrogen)) return why;
    const auto paid = p->progress.respec(p->wallet.hydrogen);
    if (!paid) return "Can't respec now";
    p->wallet.hydrogen -= *paid;
    bankIn(*p, p->region);
    pushProgress(pilot);
    return std::nullopt;
}

// MARK: - What the client is told

std::optional<net::ProgressMessage> WorldMap::progressMessage(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_) return std::nullopt;
    const ProspectorProgress& g = it->second.progress;
    net::ProgressMessage m;
    m.xp = g.xp;
    m.level = g.level();
    m.points = g.unspent();
    m.ranks = g.ranks;
    m.slots = g.slots;
    for (const SkillSpend& s : g.recent)
        if (s.at + Progression::undoWindow > clock_) m.undo.push_back({s.skill, s.at + Progression::undoWindow});
    m.respecFree = g.respecIsFree();
    m.respecPrice = g.respecCost();
    m.blueprints = g.blueprints;
    m.sitesFound = g.sites;
    m.sitesKnown = g.known;
    m.contracts = it->second.contracts;
    m.contractsDawn = nextDawn(clock_);
    m.respawn = respawning(pilot);
    m.ringwalkerReady = it->second.ringwalkerReady > clock_ ? it->second.ringwalkerReady : 0;
    m.rig = g.rig;
    m.foundry = ownsFinished(it->second, StructureKind::foundry);
    m.lab = ownsFinished(it->second, StructureKind::lab);
    if (const auto& o = it->second.opening)
        m.opening = net::ProgressMessage::Opening{siteId(o->region, o->site), o->refusal.empty() ? o->since : 0.0,
                                                  WorldSites::needs(world_->regions[o->region].built.sites[o->site].kind),
                                                  o->refusal};
    // When its actives are ready again, on its region's clock.
    if (!it->second.flight) {
        const Region& r = *regions_[it->second.region];
        if (const auto unit = r.driving(pilot))
            for (const auto& [skill, rank] : g.ranks)
                if (Progression::node(skill).active)
                    if (const auto at = r.simulation().activeReady(*unit, skill)) m.ready[skill] = *at;
    }
    return m;
}

std::vector<News> WorldMap::takeNews(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return {};
    std::vector<News> out;
    out.swap(it->second.news);
    return out;
}

// MARK: - Work XP

std::vector<WorldMap::Work> WorldMap::workBefore() const {
    std::vector<Work> out;
    for (const auto& [id, p] : pilots_) {
        if (p.flight) continue;
        const Region& r = *regions_[p.region];
        const auto unit = r.driving(id);
        const auto u = unit ? r.simulation().state.unit(*unit) : std::nullopt;
        if (!u) continue;
        Work w;
        w.pilot = id;
        w.region = p.region;
        w.unit = u->id;
        w.hydrogen = u->hydrogen.value_or(false);
        w.carrying = u->carrying;
        w.patch = u->patch;
        for (const Structure& b : r.simulation().state.structures) {
            if (!owns(p, p.region, b.id)) continue;
            if (b.buildLeft) {
                if (b.builder == u->id) w.welding[b.id] = *b.buildLeft;
            } else {
                w.hp[b.id] = b.hp;
            }
        }
        out.push_back(std::move(w));
    }
    return out;
}

void WorldMap::workAfter(const std::vector<Work>& before) {
    const int64_t day = int64_t(std::floor(clock_ / WorldXp::repairDay));
    for (const Work& w : before) {
        PilotState* p = pilotState(w.pilot);
        if (!p) continue;
        const Region& r = *regions_[w.region];
        const GameState& st = r.simulation().state;
        double xp = 0;
        std::vector<std::string> feats;
        for (const GameEvent& e : r.lastEvents()) {
            // Ore and MH unloaded, once each.
            if (const auto* d = e.as<GameEvent::Deposited>(); d && d->unit == w.unit) {
                xp += double(d->amount) * (w.hydrogen ? WorldXp::hydrogen : WorldXp::ore);
                if (w.hydrogen) feats.push_back(WorldXp::hydrogenFeat);
                contractWork(w.pilot, *p, w.hydrogen ? net::Contract::Kind::deliverHydrogen : net::Contract::Kind::deliverOre, "",
                             d->amount);
            }
            if (const auto* c = e.as<GameEvent::Constructed>(); c && owns(*p, w.region, c->structure)) {
                feats.push_back(WorldXp::built(c->kind));
                contractWork(w.pilot, *p, net::Contract::Kind::build, std::string(rawValue(c->kind)), 1);
            }
            if (const auto* d = e.as<GameEvent::PatchDepleted>(); d && w.patch == d->patch) feats.push_back(WorldXp::emptiedFeat);
        }
        for (const Structure& b : st.structures) {
            // Welding: the share of its build time put in this step.
            if (const auto was = w.welding.find(b.id); was != w.welding.end()) {
                const double put = was->second - b.buildLeft.value_or(0.0);
                if (put > 0) xp += WorldXp::weld * price(b.kind) * put / Rules::buildTime(b.kind);
                continue;
            }
            const auto was = w.hp.find(b.id);
            if (was == w.hp.end()) continue;
            double& owed = owed_[w.region][b.id];
            if (b.hp < was->second) {
                // Lost to something else: mending it back pays.
                owed += was->second - b.hp;
            } else if (b.hp > was->second && owed > 0) {
                const double mended = std::min(b.hp - was->second, owed);
                owed -= mended;
                auto& [on, paid] = repaid_[{w.region, b.id}];
                if (on != day) {
                    on = day;
                    paid = 0;
                }
                const double cap = WorldXp::weld * price(b.kind);
                const double full = std::max(1.0, Rules::hp(b.kind));
                const double pay = std::min(WorldXp::repair * price(b.kind) * mended / full, cap - paid);
                if (pay > 0) {
                    paid += pay;
                    xp += pay;
                }
            }
            if (owed <= 0) owed_[w.region].erase(b.id);
        }
        const int64_t level = p->progress.level();
        earn(*p, xp);
        for (const std::string& f : feats) feat(w.pilot, f);
        // An ore load drilled (the cargo fills at once): a survey chip's roll.
        if (const auto u = st.unit(w.unit); u && w.carrying <= 0 && u->carrying > 0 && !u->hydrogen.value_or(false)) {
            oreLoad(w.pilot);
            contractWork(w.pilot, *p, net::Contract::Kind::drill, world_->regions[w.region].file.id, 1);
            if (underSweep(w.pilot)) contractWork(w.pilot, *p, net::Contract::Kind::sweep, "", 1);
            sweptLoad(regions_[w.region]->simulation(), w.unit, u->patch ? u->patch : w.patch, clock_, params_.dayLength);
        }
        if (p->progress.level() != level) pushProgress(w.pilot);
    }
}

// MARK: - Skill clocks across regions

std::optional<WorldMap::Carried> WorldMap::takeClocks(size_t r, int64_t unit) {
    Simulation& sim = regions_[r]->simulation();
    const auto it = sim.skillClocks.find(unit);
    if (it == sim.skillClocks.end()) return std::nullopt;
    Carried c{it->second, clock_};
    // Region time to seconds from now.
    for (auto& [s, t] : c.clock.ready) t -= sim.state.time;
    for (auto& [s, t] : c.clock.until) t -= sim.state.time;
    sim.skillClocks.erase(it);
    return c;
}

void WorldMap::giveClocks(size_t r, int64_t unit, const std::optional<Carried>& c) {
    if (!c) return;
    Simulation& sim = regions_[r]->simulation();
    // The time on the way counts: a cooldown runs on in flight.
    const double from = sim.state.time - (clock_ - c->at);
    Simulation::SkillClock k = c->clock;
    for (auto& [s, t] : k.ready) t += from;
    for (auto& [s, t] : k.until) t += from;
    sim.skillClocks[unit] = k;
}

// MARK: - Wells

void WorldMap::regrowWells(double dt) {
    if (!(params_.wellRegrowEvery > 0)) return;
    for (size_t r = 0; r < regions_.size(); r++) {
        Simulation& sim = regions_[r]->simulation();
        auto& wells = sim.state.wells;
        if (!wells) continue;
        // Field pump: a Prospector pumping one with no Derrick.
        std::set<int64_t> fieldPumped;
        for (const auto& [unit, well] : sim.fieldPumps) fieldPumped.insert(well);
        for (size_t i = 0; i < wells->size() && i < wellFull_[r].size(); i++) {
            Well& well = (*wells)[i];
            double& idle = wellIdle_[r][i];
            int64_t& last = wellLast_[r][i];
            // Pumped (someone in it, or MH drawn since the last step), or
            // full: the clock starts again.
            const bool drawn = well.remaining < last;
            last = well.remaining;
            if (well.harvester || fieldPumped.count(int64_t(i)) || drawn || well.remaining >= wellFull_[r][i]) {
                idle = 0;
                continue;
            }
            idle += dt;
            while (idle >= params_.wellRegrowEvery && well.remaining < wellFull_[r][i]) {
                idle -= params_.wellRegrowEvery;
                well.remaining += 1;
            }
            last = well.remaining;
        }
    }
}

} // namespace ac::server
