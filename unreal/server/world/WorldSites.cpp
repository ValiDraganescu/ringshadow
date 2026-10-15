// Blueprint sites and survey chips in a world (WorldMap.h `WorldSites`,
// docs/prospector-progression.md "Blueprint sites"): sites seen and opened
// by each pilot for itself, the survey chips rolled on ore loads, and the
// buildings a pilot owns (its building limits).

#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ac::server {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

double WorldSites::needs(SiteKind kind) {
    switch (kind) {
    case SiteKind::crate: return 0;
    case SiteKind::wreck: return wreck;
    case SiteKind::cache: return cache;
    case SiteKind::vault: return vault;
    }
    return 0;
}

std::string WorldMap::siteId(size_t region, size_t site) const {
    if (!world_ || region >= world_->regions.size()) return "";
    const WorldRegion& g = world_->regions[region];
    return site < g.built.sites.size() ? g.file.id + "/" + g.built.sites[site].name : "";
}

int64_t WorldMap::realmCount(StructureKind kind) const {
    int64_t n = 0;
    for (size_t r = 0; r < regions_.size(); r++)
        for (const Structure& b : regions_[r]->simulation().state.structures)
            if (b.kind == kind && realm(b)) n += 1;
    return n;
}

int64_t WorldMap::limitCount(StructureKind kind, size_t region) const {
    if (!Progression::perRegion(kind)) return realmCount(kind);
    int64_t n = 0;
    for (const Structure& b : regions_[region]->simulation().state.structures)
        if (b.kind == kind && realm(b)) n += 1;
    return n;
}

// MARK: - Seeing and opening

void WorldMap::scan(PilotState& p, size_t r) {
    if (!world_) return;
    std::vector<SiteKind> kinds;
    const int64_t rank = p.progress.rank(Skill::scannerPulse);
    for (const SkillEffect& e : Progression::effects(Skill::scannerPulse, rank))
        if (const auto* s = e.as<SkillEffect::ScannerPulse>()) kinds = s->kinds;
    const auto& sites = world_->regions[r].built.sites;
    for (size_t i = 0; i < sites.size(); i++)
        if (std::find(kinds.begin(), kinds.end(), sites[i].kind) != kinds.end()) p.progress.known.insert(siteId(r, i));
}

void WorldMap::sites() {
    for (auto& [pilot, p] : pilots_) {
        const std::optional<PilotState::Opening> was = p.opening;
        p.opening = std::nullopt;
        if (p.flight) continue;
        Region& here = *regions_[p.region];
        const auto unit = here.driving(pilot);
        const auto u = unit ? here.simulation().state.unit(*unit) : std::nullopt;
        if (!u) continue;
        // Surveyor: sites within its radius are seen, whatever their kind.
        double surveyor = 0;
        const int64_t rank = p.progress.rank(Skill::surveyor);
        for (const SkillEffect& e : Progression::effects(Skill::surveyor, rank))
            if (const auto* s = e.as<SkillEffect::Surveyor>()) surveyor = std::max(surveyor, s->radius);
        const auto& sites = world_->regions[p.region].built.sites;
        std::optional<size_t> near;
        double nearest = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < sites.size(); i++) {
            const std::string id = siteId(p.region, i);
            const double d = distance(u->position, sites[i].at);
            const double sight = std::max(sites[i].kind == SiteKind::cache ? WorldSites::cacheSeen : WorldSites::seen, surveyor);
            if (d <= sight) p.progress.known.insert(id);
            if (d <= WorldSites::reach && d < nearest && !p.progress.sites.count(id)) {
                near = i;
                nearest = d;
            }
        }
        if (!near) continue;
        const region::Site& site = sites[*near];
        const double needs = WorldSites::needs(site.kind);
        if (needs <= 0) {
            openSite(pilot, p, p.region, *near);
            continue;
        }
        // A vault opens only at its blueprint's floor.
        if (site.kind == SiteKind::vault && p.progress.level() < p.progress.floor(site.blueprint)) {
            p.opening = PilotState::Opening{p.region, *near, 0,
                                            "The " + Simulation::title(site.blueprint) + " vault opens at level " +
                                                std::to_string(p.progress.floor(site.blueprint))};
            continue;
        }
        const auto in = here.held(pilot);
        if (!in || !in->hold) continue;
        // Held on: the clock runs from when it started at this site.
        const bool same = was && was->region == p.region && was->site == *near && was->refusal.empty();
        p.opening = PilotState::Opening{p.region, *near, same ? was->since : clock_, ""};
        if (clock_ - p.opening->since >= needs - 1e-9) {
            p.opening = std::nullopt;
            openSite(pilot, p, p.region, *near);
        }
    }
}

void WorldMap::openSite(PilotId pilot, PilotState& p, size_t r, size_t i) {
    const std::string id = siteId(r, i);
    if (!p.progress.sites.insert(id).second) return;
    p.progress.known.insert(id);
    const StructureKind kind = world_->regions[r].built.sites[i].blueprint;
    // C2: every site gives a copy of its blueprint, held already or not.
    p.news.push_back(News{NewsKind::site, id, 0, 0, ""});
    findBlueprint(pilot, kind, Progression::blueprintFloor(kind));
}

// MARK: - Survey chips

bool WorldMap::oreLoad(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p) return false;
    if (p->progress.chipAt && clock_ - *p->progress.chipAt < WorldSites::chipEvery) return false;
    if (std::uniform_real_distribution<double>(0, 1)(chips_) >= p->progress.chipChance()) return false;
    findChip(pilot);
    return true;
}

std::optional<std::pair<size_t, size_t>> WorldMap::nearestUnknown(const PilotState& p) const {
    if (!world_) return std::nullopt;
    // On the world's grid: each region at its column and row, a region's
    // extent apart, positions in cells.
    const WorldRegion& here = world_->regions[p.region];
    const Vec2 me = p.last ? *p.last : here.built.extent() * 0.5;
    const Vec2 cell = here.built.extent();
    const Vec2 origin(double(here.column) * cell.x, double(here.row) * cell.y);
    std::optional<std::pair<size_t, size_t>> best;
    double bestD = std::numeric_limits<double>::infinity();
    for (size_t r = 0; r < world_->regions.size(); r++) {
        const WorldRegion& g = world_->regions[r];
        if (std::abs(g.column - here.column) + std::abs(g.row - here.row) > 1) continue;
        const Vec2 at0(double(g.column) * cell.x, double(g.row) * cell.y);
        for (size_t i = 0; i < g.built.sites.size(); i++) {
            const std::string id = siteId(r, i);
            if (p.progress.known.count(id) || p.progress.sites.count(id)) continue;
            const double d = distance(origin + me, at0 + g.built.regionAt(g.built.sites[i].at));
            if (d < bestD) {
                bestD = d;
                best = std::make_pair(r, i);
            }
        }
    }
    return best;
}

void WorldMap::findChip(PilotId pilot) {
    PilotState* p = pilotState(pilot);
    if (!p) return;
    p->progress.chips += 1;
    p->progress.chipAt = clock_;
    // One in 20: a blueprint the pilot lacks, not far above its level.
    std::vector<StructureKind> lacking;
    for (const StructureKind k : Progression::blueprintKinds())
        if (!p->progress.blueprints.count(k) &&
            Progression::blueprintFloor(k) <= p->progress.level() + WorldSites::chipBlueprintReach)
            lacking.push_back(k);
    const bool blueprint =
        !lacking.empty() && std::uniform_int_distribution<int64_t>(0, WorldSites::chipBlueprintOneIn - 1)(chips_) == 0;
    std::string marked;
    if (blueprint) {
        marked = "blueprint";
    } else if (const auto site = nearestUnknown(*p)) {
        marked = siteId(site->first, site->second);
        p->progress.known.insert(marked);
    }
    p->news.push_back(News{NewsKind::chip, marked, WorldXp::chip, p->progress.chips, ""});
    earn(*p, WorldXp::chip);
    if (blueprint) {
        const StructureKind k = lacking[size_t(std::uniform_int_distribution<size_t>(0, lacking.size() - 1)(chips_))];
        findBlueprint(pilot, k, Progression::blueprintFloor(k));
        return;
    }
    pushProgress(pilot);
}

} // namespace ac::server
