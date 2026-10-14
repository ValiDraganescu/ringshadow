// The pilot's rig in a world (WorldMap.h, P7; docs/prospector-progression.md
// "Foundry and Lab"): its Foundry makes modules and its Lab researches their
// marks on the world's clock, away or not, and the modules are fitted at
// home. The rules are the core's (`Rig`); here are the places, the wallet
// and the clock.

#include "WorldMap.h"

#include "NavGrid.h"

#include <algorithm>

namespace ac::server {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

bool WorldMap::ownsFinished(const PilotState& p, StructureKind kind) const {
    if (!world_) return false;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if (b.kind == kind && b.complete() && owns(p, r, b.id)) return true;
    return false;
}

bool WorldMap::ownsFinished(PilotId pilot, StructureKind kind) const {
    const auto it = pilots_.find(pilot);
    return it != pilots_.end() && ownsFinished(it->second, kind);
}

bool WorldMap::atOwn(PilotId pilot, const PilotState& p, std::initializer_list<StructureKind> kinds) {
    if (p.flight) return false;
    Region& r = *regions_[p.region];
    const auto unit = r.driving(pilot);
    const auto u = unit ? r.simulation().state.unit(*unit) : std::nullopt;
    if (!u) return false;
    const auto& all = r.simulation().state.structures;
    return std::any_of(all.begin(), all.end(), [&](const Structure& b) {
        return std::find(kinds.begin(), kinds.end(), b.kind) != kinds.end() && b.complete() && owns(p, p.region, b.id) &&
               NavGrid::insideStructure(u->position, b, Simulation::pilotDepositReach);
    });
}

std::optional<std::string> WorldMap::make(PilotId pilot, Module module, int64_t mark) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (!ownsFinished(*p, StructureKind::foundry)) return "Build a Foundry first";
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
    if (!ownsFinished(*p, StructureKind::lab)) return "Build a Lab first";
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
    // At home: its own Citadel or Hab Dome, where it unloads.
    if (!atOwn(pilot, *p, {StructureKind::citadel, StructureKind::habDome}))
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
        if (made) p.news.push_back(News{NewsKind::made, std::string(rawValue(made->module)), 0, made->mark});
        if (researched)
            p.news.push_back(News{NewsKind::researched, std::string(rawValue(researched->module)), 0, researched->mark});
    }
    // Kept pilots off line: their jobs run on the world's clock too, told
    // when they come back.
    for (auto& [key, p] : away_) {
        Rig& rig = p.progress.rig;
        if (!rig.making && !rig.research) continue;
        const auto [made, researched] = rig.finish(clock_);
        if (made) p.news.push_back(News{NewsKind::made, std::string(rawValue(made->module)), 0, made->mark});
        if (researched)
            p.news.push_back(News{NewsKind::researched, std::string(rawValue(researched->module)), 0, researched->mark});
    }
}

} // namespace ac::server
