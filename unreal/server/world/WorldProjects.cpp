// Realm projects (R2, docs/mmorpg-play.md "Realm projects"; RealmProjects.h):
// the realm wallet's sink. A human pilot starts one from the Citadel's REALM
// tab, the realm wallet pays, it runs a world day; one of each kind at a time.
// Their effects: the Beacon in `fly` (half fares), the Relay and the Armoury
// on every region's simulation here (night sight, turret marks), the Dock
// Crew in the mercenaries' rate (WorldMercs.cpp).

#include "WorldMap.h"

#include <cmath>

namespace ac::server {

namespace {

using News = net::ProgressMessage::News;
using NewsKind = net::ProgressMessage::NewsKind;

} // namespace

std::optional<std::string> WorldMap::projectRefusal(const std::string& key, RealmProject::Kind kind) const {
    if (key.rfind("frontier:", 0) == 0) return std::string("Frontier pilots never start a realm project");
    if (projectOn(kind)) return std::string("The realm's ") + RealmProject::title(kind) + " runs already";
    const RealmProject::Price price = projectPrice(kind);
    if (realm_.ore < price.ore || realm_.hydrogen < price.hydrogen)
        return "The realm wallet is short: the " + std::string(RealmProject::title(kind)) + " costs " + std::to_string(price.ore) +
               " ore and " + std::to_string(price.hydrogen) + " MH";
    return std::nullopt;
}

std::optional<std::string> WorldMap::startProject(PilotId pilot, RealmProject::Kind kind) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = projectRefusal(p->key, kind)) return why;
    if (!atRealm(pilot, *p, {StructureKind::citadel})) return std::string("Start a project at the realm's Citadel: stand where you unload");
    const RealmProject::Price price = projectPrice(kind);
    if (!spendRealm(Wallet{price.ore, price.hydrogen})) return std::string("The realm wallet is short");
    projectRuns_[size_t(kind)] += 1;
    projects_[size_t(kind)] = Project{kind, clock_ + RealmProject::days * params_.dayLength, callsign(p->key)};
    for (auto& [id, q] : pilots_) q.news.push_back(News{NewsKind::project, std::string("start/") + RealmProject::slug(kind), 0, 0, callsign(p->key)});
    projectsStep();
    return std::nullopt;
}

void WorldMap::setProjects(const std::array<std::optional<Project>, RealmProject::count>& p,
                           const std::array<int64_t, RealmProject::count>& runs) {
    projects_ = p;
    projectRuns_ = runs;
    projectsStep();
}

void WorldMap::projectsStep() {
    for (std::optional<Project>& pr : projects_) {
        if (!pr || clock_ < pr->ends) continue;
        const RealmProject::Kind kind = pr->kind;
        pr = std::nullopt;
        for (auto& [id, q] : pilots_) q.news.push_back(News{NewsKind::project, std::string("end/") + RealmProject::slug(kind), 0, 0, ""});
    }
    // The running ones' effects on every region's simulation.
    const double widen = projectOn(RealmProject::Kind::relay) ? RealmProject::relaySight : 1.0;
    const int64_t bonus = projectOn(RealmProject::Kind::armoury) ? RealmProject::armouryMarks : 0;
    for (auto& r : regions_) {
        r->simulation().nightWiden = widen;
        r->simulation().turretBonus = bonus;
    }
}

} // namespace ac::server
