// The pilot's rig in a world (WorldMap.h, P7; docs/prospector-progression.md
// "Foundry and Lab"): its Foundry makes modules and its Lab researches their
// marks on the world's clock, away or not, and the modules are fitted at
// home. The rules are the core's (`Rig`); here are the places, the wallet
// and the clock.

#include "WorldMap.h"

#include "NavGrid.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>

namespace ac::server {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

bool WorldMap::realmFinished(StructureKind kind) const {
    if (!world_) return false;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if (b.kind == kind && b.complete() && realm(b)) return true;
    return false;
}

bool WorldMap::builtFinished(const PilotState& p, StructureKind kind) const {
    if (!world_) return false;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if (b.kind == kind && b.complete() && built(p, r, b.id)) return true;
    return false;
}

std::optional<std::pair<size_t, int64_t>> WorldMap::realmCitadel() const {
    if (!world_) return std::nullopt;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if (b.kind == StructureKind::citadel && b.complete() && realm(b)) return std::pair{r, b.id};
    return std::nullopt;
}

std::optional<std::pair<size_t, int64_t>> WorldMap::citadelAt(PilotId pilot, const PilotState& p) {
    if (p.flight) return std::nullopt;
    Region& r = *regions_[p.region];
    const auto unit = r.driving(pilot);
    const auto u = unit ? r.simulation().state.unit(*unit) : std::nullopt;
    if (!u) return std::nullopt;
    for (const Structure& b : r.simulation().state.structures)
        if (b.kind == StructureKind::citadel && b.complete() && realm(b) && NavGrid::insideStructure(u->position, b, Simulation::pilotDepositReach))
            return std::pair{p.region, b.id};
    return std::nullopt;
}

bool WorldMap::atRealm(PilotId pilot, const PilotState& p, std::initializer_list<StructureKind> kinds) {
    if (p.flight) return false;
    Region& r = *regions_[p.region];
    const auto unit = r.driving(pilot);
    const auto u = unit ? r.simulation().state.unit(*unit) : std::nullopt;
    if (!u) return false;
    const auto& all = r.simulation().state.structures;
    return std::any_of(all.begin(), all.end(), [&](const Structure& b) {
        return std::find(kinds.begin(), kinds.end(), b.kind) != kinds.end() && b.complete() && realm(b) &&
               NavGrid::insideStructure(u->position, b, Simulation::pilotDepositReach);
    });
}

std::optional<std::string> WorldMap::make(PilotId pilot, Module module, int64_t mark) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (!realmFinished(StructureKind::foundry)) return "Build a Foundry first";
    if (!p->flight) bankOut(*p, p->region);
    Rig& rig = p->progress.rig;
    if (auto why = rig.makeRefusal(module, mark, p->wallet.ore, p->wallet.hydrogen)) return why;
    if (!rig.make(module, mark, p->wallet.ore, p->wallet.hydrogen, clock_)) return "The Foundry can't start that now";
    p->wallet.ore -= Modules::ore(mark);
    p->wallet.hydrogen -= Modules::hydrogen(mark);
    if (!p->flight) bankIn(*p, p->region);
    return std::nullopt;
}

std::optional<std::string> WorldMap::research(PilotId pilot, Module module, int64_t mark) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (!realmFinished(StructureKind::lab)) return "Build a Lab first";
    if (!p->flight) bankOut(*p, p->region);
    Rig& rig = p->progress.rig;
    if (auto why = rig.researchRefusal(module, mark, p->wallet.hydrogen)) return why;
    if (!rig.startResearch(module, mark, p->wallet.hydrogen, clock_)) return "The Lab can't start that now";
    p->wallet.hydrogen -= Modules::researchHydrogen(mark);
    if (!p->flight) bankIn(*p, p->region);
    return std::nullopt;
}

std::optional<std::string> WorldMap::fit(PilotId pilot, Module module, int64_t mark) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = p->progress.rig.fitRefusal(module, mark)) return why;
    // At home: the realm's Citadel or a Hab Dome, where it unloads.
    if (!atRealm(pilot, *p, {StructureKind::citadel, StructureKind::habDome}))
        return std::string("Fit modules at your Citadel or a Hab Dome: stand where you unload");
    if (!p->progress.rig.fit(module, mark)) return "Can't fit that now";
    // Into the sim: the module's blocks on the driven Prospector.
    pushProgress(pilot);
    return std::nullopt;
}

void WorldMap::rigs() {
    for (auto& [id, p] : pilots_) {
        Rig& rig = p.progress.rig;
        if (!rig.making && !rig.research) continue;
        const auto [made, researched] = rig.finish(clock_);
        if (made) goalDid(p, "made");
        if (made) p.news.push_back(News{NewsKind::made, std::string(rawValue(made->module)), 0, made->mark, ""});
        if (researched)
            p.news.push_back(News{NewsKind::researched, std::string(rawValue(researched->module)), 0, researched->mark, ""});
    }
    // Kept pilots off line: their jobs run on the world's clock too, told
    // when they come back.
    for (auto& [key, p] : away_) {
        Rig& rig = p.progress.rig;
        if (!rig.making && !rig.research) continue;
        const auto [made, researched] = rig.finish(clock_);
        if (made) goalDid(p, "made");
        if (made) p.news.push_back(News{NewsKind::made, std::string(rawValue(made->module)), 0, made->mark, ""});
        if (researched)
            p.news.push_back(News{NewsKind::researched, std::string(rawValue(researched->module)), 0, researched->mark, ""});
    }
}

void WorldMap::devRig(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p || p->flight || !world_) return;
    // The blueprints (C2: learned) and the level the Lab's floor asks for.
    for (const StructureKind k : {StructureKind::citadel, StructureKind::foundry, StructureKind::lab}) p->progress.learned.insert(k);
    const int64_t floor = std::max(Progression::blueprintFloor(StructureKind::foundry), Progression::blueprintFloor(StructureKind::lab));
    if (p->progress.level() < floor) earn(pilot, Progression::xp(floor) - p->progress.xp);
    // Enough for the three and some to spend at the Foundry and the Lab.
    bankOut(*p, p->region);
    p->wallet.ore += 6000;
    p->wallet.hydrogen += 4000;
    bankIn(*p, p->region);
    Region& r = *regions_[p->region];
    Simulation& sim = r.simulation();
    const auto& pads = world_->regions[p->region].built.pads;
    auto driven = [&]() -> Unit* {
        const auto id = r.driving(pilot);
        const auto i = id ? sim.unitIndex(*id) : std::nullopt;
        return i ? &sim.state.units[size_t(*i)] : nullptr;
    };
    // The Citadel put up round the Prospector, off the pads' plates, and
    // finished; the Foundry and the Lab on the Citadel's far side, away from
    // the pad where the flows play (C5: 7x7 and 5x5, round the Prospector
    // they crowd it), the Prospector stepped over to weld each.
    const Unit* first = driven();
    if (!first) return;
    const Vec2 start = first->position;
    std::optional<int64_t> citadel;
    std::optional<Vec2> citadelAt;
    for (const StructureKind kind : {StructureKind::citadel, StructureKind::foundry, StructureKind::lab}) {
        if (!driven()) return;
        const double reach = Region::buildReach(kind);
        // Where to look: round the Prospector for the Citadel; out beyond
        // the Citadel, the far side first, for the others.
        std::vector<Vec2> spots;
        if (kind == StructureKind::citadel || !citadelAt) {
            for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
                for (int k = 0; k < 24; k++) spots.push_back(start + Vec2(std::cos(k * 2 * pi / 24), std::sin(k * 2 * pi / 24)) * d);
        } else {
            const Vec2 away = length(*citadelAt - start) > 1e-6 ? normalize(*citadelAt - start) : Vec2(1, 0);
            const double base = std::atan2(away.y, away.x);
            const double out = Rules::radius(StructureKind::citadel) + Rules::radius(kind) + 1.5;
            for (double d = out; d <= out + 8; d += 1)
                for (int k = 0; k < 24; k++) {
                    const double h = base + (k % 2 ? 1 : -1) * double((k + 1) / 2) * 2 * pi / 24;
                    spots.push_back(*citadelAt + Vec2(std::cos(h), std::sin(h)) * d);
                }
        }
        bool done = false;
        for (const Vec2 q : spots) {
            if (done) break;
            const auto spot = sim.snap(kind, q);
            if (!spot || sim.placementRefusal(kind, *spot) || region::touchesPadPlate(pads, *spot, Rules::radius(kind))) continue;
            if (kind != StructureKind::citadel && citadelAt) {
                // Stepped over to weld it: its build reach off its far side.
                const Vec2 off = length(*spot - *citadelAt) > 1e-6 ? normalize(*spot - *citadelAt) : Vec2(1, 0);
                if (Unit* u = driven()) u->position = *spot + off * reach;
            } else if (distance(*spot, start) > reach + Region::buildSlack) {
                continue;
            }
            const int64_t id = sim.state.nextID;
            if (build(pilot, kind, *spot)) continue;
            for (Structure& b : sim.state.structures)
                if (b.id == id) {
                    b.buildLeft = std::nullopt;
                    b.hp = sim.fullHP(b);
                }
            if (kind == StructureKind::citadel) {
                citadel = id;
                citadelAt = *spot;
            }
            done = true;
        }
    }
    // The Prospector where it unloads at the Citadel, on the side it stood.
    if (Unit* u = driven()) u->position = start;
    if (citadel)
        for (const Structure& b : sim.state.structures)
            if (b.id == *citadel)
                if (Unit* u = driven()) {
                    Vec2 out = u->position - b.position;
                    out = length(out) > 1e-6 ? normalize(out) : Vec2(1, 0);
                    // Half a cell off its square's edge that way (C5: off a
                    // diagonal, r + 0.5 along it lands inside the square).
                    const double edge = std::max(std::abs(out.x), std::abs(out.y));
                    u->position = b.position + out * ((Rules::radius(b.kind) + 0.5) / edge);
                }
    // The rig: marks researched, modules in the stores and fitted, a job at each.
    Rig& rig = p->progress.rig;
    rig.researched = {{Module::drillHead, 3}, {Module::cargoPod, 2}, {Module::plate, 2}, {Module::surveyLens, 2}};
    rig.fitted = {{Module::drillHead, 2}, {Module::cargoPod, 1}, {Module::plate, 1}};
    rig.store(Module::drillHead, 3);
    rig.store(Module::cargoPod, 2, 2);
    rig.store(Module::plate, 2);
    rig.store(Module::torchTip, 1, 2);
    rig.store(Module::surveyLens, 2);
    rig.making = ModuleJob{Module::plate, 2, clock_ + 150};
    rig.research = ModuleJob{Module::torchTip, 2, clock_ + 420};
    pushProgress(pilot);
}

} // namespace ac::server
