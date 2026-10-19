// Demolish in a world (WorldMap.h, C3; docs/guns-and-practice.md "Limits per
// region and demolish"): a pilot takes down a building of the realm in its
// region and half its price comes back to its own wallet. The rule for the
// building itself (the reach, a Citadel with someone in it, its leaving) is
// the core's (Simulation+Pilot.members.h, "Demolish").

#include "WorldMap.h"

namespace ac::server {

std::optional<std::string> WorldMap::demolish(PilotId pilot, int64_t structure) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_) return "Not in a world";
    if (p->flight) return "In flight";
    const size_t r = p->region;
    Region& region = *regions_[r];
    Simulation& sim = region.simulation();
    const auto s = sim.state.structure(structure);
    if (!s || !realm(*s)) return "No building of the realm's here";
    // Driving: within the repair reach; the top-down view: anywhere here.
    if (const auto driven = region.driving(pilot)) {
        const auto me = sim.state.unit(*driven);
        if (!me || !(me->hp > 0)) return "Nothing driven";
        if (!sim.demolishReach(*me, *s, demolishSlack)) return "Too far away";
    }
    // A home must remain: the realm's last finished Citadel stays.
    if (s->kind == StructureKind::citadel && s->complete()) {
        int64_t homes = 0;
        for (size_t k = 0; k < regions_.size(); k++)
            for (const Structure& b : regions_[k]->simulation().state.structures)
                if (b.kind == StructureKind::citadel && b.complete() && realm(b) && !(k == r && b.id == s->id)) homes += 1;
        if (homes == 0) return std::string("The realm's last Citadel stays: a home must remain");
    }
    if (auto why = sim.demolishRefusal(*s)) return why;
    const Price back = Simulation::salvage(s->kind, true);
    bankOut(*p, r);
    p->wallet.ore += back.ore;
    p->wallet.hydrogen += back.hydrogen;
    bankIn(*p, r);
    if (!region.demolish(pilot, structure, back)) return "No building of the realm's here";
    builtBy_[r].erase(structure);
    kills_[r].erase(structure);
    // A frontier pilot working on it stops and thinks again.
    for (FrontierPilot& f : frontier_) {
        if (f.region != r) continue;
        if (f.plan.target == structure &&
            (f.plan.act == FrontierPlan::Act::repair || f.plan.act == FrontierPlan::Act::pump || f.plan.act == FrontierPlan::Act::buy))
            f.plan = {};
        if (f.building == structure) f.building = std::nullopt;
    }
    pushProgress(pilot);
    return std::nullopt;
}

} // namespace ac::server
