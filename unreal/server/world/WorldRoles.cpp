// The buildings' jobs for the pilot in a world (WorldMap.h, P6;
// docs/prospector-progression.md "Buildings in the MMO"): a fallen
// Prospector comes back at the realm's Citadel or nearest Hab Dome, and the
// realm's Landing Pads and Spacedock the Dropship flies between (F2; R1:
// whoever built them), with the Wayfinder's flight nodes (Pad link, Short
// hop, Ringwalker).

#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace ac::server {

// MARK: - Landing Pads

std::vector<LandingPad> WorldMap::landingPads() const {
    std::vector<LandingPad> out;
    if (!world_) return out;
    // By region; in each its Landing Pads by id, "Landing Pad 1" and
    // "Landing Pad 2" (two at most), then the Spacedock (one in the realm).
    for (size_t r = 0; r < regions_.size(); r++) {
        std::vector<LandingPad> here;
        for (const Structure& b : regions_[r]->simulation().state.structures) {
            if (!b.complete() || !realm(b)) continue;
            if (b.kind != StructureKind::landingPad && b.kind != StructureKind::spacedock) continue;
            here.push_back(LandingPad{r, b.id, b.kind, "", b.position});
        }
        std::sort(here.begin(), here.end(), [](const LandingPad& a, const LandingPad& b) {
            return a.kind != b.kind ? a.kind == StructureKind::landingPad : a.building < b.building;
        });
        int64_t n = 0;
        for (LandingPad& o : here) {
            o.name = o.kind == StructureKind::landingPad ? "Landing Pad " + std::to_string(++n) : Simulation::title(o.kind);
            out.push_back(o);
        }
    }
    return out;
}

double WorldMap::padLink(const PilotState& p) const {
    double link = 0;
    for (const SkillEffect& e : Progression::effects(Skill::padLink, p.progress.rank(Skill::padLink)))
        if (const auto* l = e.as<SkillEffect::PadLink>()) link = std::max(link, l->radius);
    return link;
}

double WorldMap::padReach(const PilotState& p) const {
    // Pad link from the plate's edge (C5: the plate grew past its ranks).
    return params_.padReach + padLink(p);
}

double WorldMap::padReach(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? params_.padReach : padReach(it->second);
}

Ferry::Plan WorldMap::ferryPlan(const PilotState& p, size_t to) const {
    const WorldRegion& a = world_->regions[p.region];
    const WorldRegion& b = world_->regions[to];
    // Short hop: 20% off the dash a rank; Ringwalker: 30% faster.
    double faster = 0;
    for (const SkillEffect& e : Progression::effects(Skill::ringwalker, p.progress.rank(Skill::ringwalker)))
        if (const auto* w = e.as<SkillEffect::Ringwalker>()) faster = w->faster;
    return Ferry::plan(ac::Flight::steps(a.column, a.row, b.column, b.row), p.progress.skillBoost(SkillStat::flightTime).percent,
                       faster);
}

Ferry::Plan WorldMap::ferryPlan(PilotId pilot, size_t to) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_ || to >= regions_.size()) return Ferry::plan(0);
    return ferryPlan(it->second, to);
}

bool WorldMap::seen(const PilotState& p, size_t r) const {
    return r == p.region || r == start_ || p.progress.regions.count(world_->regions[r].file.id) > 0;
}

void WorldMap::landingPadsChanged() {
    const std::vector<LandingPad> now = landingPads();
    for (auto& [id, p] : pilots_) {
        if (now == p.padsTold) continue;
        p.padsTold = now;
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
                // C1: a Citadel's (complete) in its hall, 3 cells back from
                // the centre, away from the door; a Hab Dome's at its edge.
                best = Home{r, b.kind == StructureKind::citadel ? b.position + citadelRespawn : b.position, b.kind};
            }
        }
    }
    if (best) return best;
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
    // A new unit of its kind, with what Second wind kept.
    Unit u = to.simulation().freshUnit(pilotKind(p), 0, Pilot::player, h.at, 0);
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
