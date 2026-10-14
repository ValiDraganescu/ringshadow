// The buildings' jobs for the pilot in a world (WorldMap.h, P6;
// docs/prospector-progression.md "Buildings in the MMO"): a fallen
// Prospector comes back at the realm's Citadel or nearest Hab Dome, and the
// pilot's own pads (the realm's Spacedock; with Homing beacon the realm's
// Citadel, Hab Domes and Derricks; R1: whoever built them), with the
// Wayfinder's flight nodes (Pad link, Short hop).

#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace ac::server {

// MARK: - Pads of its own

std::vector<OwnPad> WorldMap::ownPads(const PilotState& p) const {
    std::vector<OwnPad> out;
    if (!world_) return out;
    // Homing beacon: these of its buildings count as pads.
    std::vector<Beacon> beacons;
    for (const SkillEffect& e : Progression::effects(Skill::homingBeacon, p.progress.rank(Skill::homingBeacon)))
        if (const auto* h = e.as<SkillEffect::HomingBeacon>()) beacons = h->from;
    auto beacon = [&](StructureKind k) {
        const std::optional<Beacon> b = k == StructureKind::citadel   ? std::optional(Beacon::citadel)
                                        : k == StructureKind::habDome ? std::optional(Beacon::habDome)
                                        : k == StructureKind::derrick ? std::optional(Beacon::derrick)
                                                                      : std::nullopt;
        return b && std::find(beacons.begin(), beacons.end(), *b) != beacons.end();
    };
    // By kind, then id across the world: "Hab Dome 1", "Hab Dome 2".
    std::map<StructureKind, std::vector<OwnPad>> kinds;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures) {
            if (!b.complete() || !realm(b)) continue;
            if (b.kind != StructureKind::spacedock && !beacon(b.kind)) continue;
            kinds[b.kind].push_back(OwnPad{r, b.id, b.kind, "", b.position});
        }
    for (auto& [kind, list] : kinds) {
        std::sort(list.begin(), list.end(), [](const OwnPad& a, const OwnPad& b) { return a.building < b.building; });
        const bool numbered = Progression::buildingLimit(kind) > 1;
        for (size_t i = 0; i < list.size(); i++) {
            list[i].name = Simulation::title(kind) + (numbered ? " " + std::to_string(i + 1) : "");
            out.push_back(list[i]);
        }
    }
    return out;
}

std::vector<OwnPad> WorldMap::ownPads(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? std::vector<OwnPad>{} : ownPads(it->second);
}

double WorldMap::padReach(const PilotState& p) const {
    double reach = params_.padReach;
    for (const SkillEffect& e : Progression::effects(Skill::padLink, p.progress.rank(Skill::padLink)))
        if (const auto* l = e.as<SkillEffect::PadLink>()) reach = std::max(reach, l->radius);
    return reach;
}

double WorldMap::padReach(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? params_.padReach : padReach(it->second);
}

double WorldMap::flightTime(const PilotState& p) const {
    // Short hop: 20% less a rank.
    const double share = 1 + p.progress.skillBoost(SkillStat::flightTime).percent;
    return params_.flightTime * std::max(0.0, share);
}

double WorldMap::flightTime(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? params_.flightTime : flightTime(it->second);
}

Vec2 WorldMap::landing(const Flight& f) const {
    return f.own ? *f.own : world_->regions[f.to].built.pads[f.pad].at;
}

void WorldMap::ownPadsChanged() {
    for (auto& [id, p] : pilots_) {
        std::vector<OwnPad> now = ownPads(p);
        if (now == p.padsTold) continue;
        p.padsTold = std::move(now);
        // The World message again (`Server::sendMoves`).
        moves_.push_back(Moved{id, std::nullopt});
    }
}

// MARK: - Respawn

std::optional<WorldMap::Home> WorldMap::home(const PilotState&, size_t region, Vec2 from) const {
    std::optional<Home> best;
    double bestScore = std::numeric_limits<double>::infinity();
    const WorldRegion& fell = world_->regions[region];
    for (size_t r = 0; r < regions_.size(); r++) {
        // Another pilot drives there: not now.
        if (r != region && regions_[r]->driven()) continue;
        const WorldRegion& g = world_->regions[r];
        const double steps = double(std::llabs(g.column - fell.column) + std::llabs(g.row - fell.row));
        for (const Structure& b : regions_[r]->simulation().state.structures) {
            if (!(b.kind == StructureKind::citadel || b.kind == StructureKind::habDome) || !b.complete() || !realm(b))
                continue;
            // Its own region by distance; then a region a step away beats
            // any spot two steps away.
            const double d = r == region ? distance(regions_[r]->places()->regionAt(b.position), from) : 0;
            const double score = steps * 1e6 + d + double(b.id) * 1e-9;
            if (score < bestScore) {
                bestScore = score;
                best = Home{r, b.position, b.kind};
            }
        }
    }
    if (best) return best;
    if (start_ != region && regions_[start_]->driven()) return std::nullopt;
    return Home{start_, world_->regions[start_].built.pads[world_->startPad].at, std::nullopt};
}

std::optional<net::ProgressMessage::Respawn> WorldMap::respawning(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_ || !it->second.down) return std::nullopt;
    const Down& d = *it->second.down;
    net::ProgressMessage::Respawn out;
    out.at = d.until;
    out.cargo = d.cargo;
    out.hydrogen = d.hydrogen;
    const auto h = home(it->second, d.region, d.at);
    const size_t r = h ? h->region : d.region;
    out.region = world_->regions[r].file.id;
    if (h) out.home = h->kind;
    return out;
}

void WorldMap::respawns() {
    for (auto& [id, p] : pilots_) {
        if (p.flight) continue;
        Region& here = *regions_[p.region];
        if (!p.down) {
            // Its Prospector is gone from its region: it fell.
            if (!p.unit || here.simulation().state.unit(*p.unit)) continue;
            Down d;
            d.region = p.region;
            d.at = p.last.value_or(Vec2(0, 0));
            double sooner = 0;
            // Second wind: back sooner, with part of the cargo.
            auto& fallen = here.simulation().fallen;
            if (fallen && fallen->unit == *p.unit) {
                d.cargo = fallen->cargo;
                d.hydrogen = fallen->hydrogen;
                sooner = fallen->sooner;
                fallen = std::nullopt;
            }
            d.until = clock_ + params_.respawnDelay * std::max(0.0, 1 - sooner);
            p.down = d;
            p.unit = std::nullopt;
            p.last = std::nullopt;
            continue;
        }
        if (clock_ + 1e-9 < p.down->until) continue;
        const auto h = home(p, p.down->region, p.down->at);
        if (!h) continue;
        const Down d = *p.down;
        respawn(id, p, *h, d.cargo, d.hydrogen);
    }
}

void WorldMap::respawn(PilotId pilot, PilotState& p, const Home& h, int64_t cargo, bool hydrogen) {
    Region& from = *regions_[p.region];
    const int64_t lastInput = from.lastInputTick(pilot);
    const size_t was = p.region;
    if (h.region != was) {
        if (from.joined(pilot)) p.record = from.levels(pilot);
        from.leave(pilot);
        if (from.pilots().empty()) bankIn(PilotState{}, was);
    }
    Region& to = *regions_[h.region];
    // A new Prospector, with what Second wind kept.
    Unit u = to.simulation().freshUnit(UnitKind::prospector, 0, Pilot::player, h.at, 0);
    if (cargo > 0) {
        u.carrying = cargo;
        if (hydrogen) u.hydrogen = true;
    }
    p.down = std::nullopt;
    arrive(pilot, p, h.region, u, to.places()->regionAt(h.at), lastInput);
    if (h.region == was) moves_.push_back(Moved{pilot, std::nullopt});
    else regionChanged(pilot, p, h.region);
}

void WorldMap::regionChanged(PilotId pilot, PilotState& p, size_t to) {
    Region& now = *regions_[to];
    p.visit += 1;
    net::RegionChange rc;
    rc.visit = p.visit;
    rc.region = now.description();
    rc.regionId = world_->regions[to].file.id;
    rc.driven = now.driving(pilot);
    rc.serverTick = now.tick();
    rc.time = now.simulation().state.time;
    moves_.push_back(Moved{pilot, rc});
}

} // namespace ac::server
