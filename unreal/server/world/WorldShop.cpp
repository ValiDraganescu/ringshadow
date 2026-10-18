// The Citadel shop (WorldMap.h, C2; docs/prospector-progression.md "The
// Citadel shop"): blueprints (each once a pilot, learned once) and
// consumables bought from the pilot's own wallet at a Citadel, the
// consumables used in the field, and learning a blueprint held.
// The prices and the items are the core's (`Shop`), so the client shows
// the same numbers.

#include "WorldMap.h"

#include <cctype>
#include <cmath>

namespace ac::server {

namespace {

/// "repair kits", "MH canisters": the title in a sentence, more than one.
std::string plural(const ItemInfo& x) {
    std::string t = x.title;
    if (t.size() > 1 && !std::isupper(static_cast<unsigned char>(t[1])))
        t[0] = char(std::tolower(static_cast<unsigned char>(t[0])));
    return t + "s";
}

} // namespace

std::optional<std::string> WorldMap::buy(PilotId pilot, std::optional<StructureKind> blueprint, Item item) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    ShopPrice price;
    if (blueprint) {
        if (Progression::blueprintFloor(*blueprint) <= 0)
            return "No " + Simulation::title(*blueprint) + " blueprint in this world";
        // C2: each blueprint once a pilot, at its first copy's price.
        if (p->progress.knows(*blueprint)) return "You know the " + Simulation::title(*blueprint);
        if (p->progress.copies(*blueprint) > 0)
            return "You hold a " + Simulation::title(*blueprint) + " blueprint: learn it (B)";
        price = Shop::blueprintPrice(*blueprint, 0);
    } else {
        const ItemInfo& x = Shop::item(item);
        if (p->progress.held(item) >= x.stack)
            return std::string(x.title) + ": you hold " + std::to_string(x.stack) + ", the most";
        price = {x.ore, x.hydrogen};
    }
    // At the realm's Citadel, in its unload range.
    if (!atRealm(pilot, *p, {StructureKind::citadel})) return std::string("Buy at a Citadel: stand where you unload");
    bankOut(*p, p->region);
    if (price.ore > p->wallet.ore)
        return "Not enough ore: " + std::to_string(price.ore) + ", you have " + std::to_string(p->wallet.ore);
    if (price.hydrogen > p->wallet.hydrogen)
        return "Not enough MH: " + std::to_string(price.hydrogen) + ", you have " + std::to_string(p->wallet.hydrogen);
    p->wallet.ore -= price.ore;
    p->wallet.hydrogen -= price.hydrogen;
    bankIn(*p, p->region);
    if (blueprint) {
        p->progress.giveBlueprint(*blueprint);
    } else {
        p->progress.items[item] += 1;
    }
    goalDid(*p, "shop");
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<std::string> WorldMap::learn(PilotId pilot, StructureKind kind) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (auto why = p->progress.learnRefusal(kind)) return why;
    p->progress.learn(kind);
    pushProgress(pilot);
    return std::nullopt;
}

std::optional<double> WorldMap::itemReady(PilotId pilot, Item item) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    const auto r = it->second.itemReady.find(item);
    return r != it->second.itemReady.end() && r->second > clock_ ? std::optional(r->second) : std::nullopt;
}

std::optional<std::string> WorldMap::use(PilotId pilot, Item item) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    const ItemInfo& x = Shop::item(item);
    if (p->progress.held(item) <= 0) return "No " + plural(x) + ": buy them at the Citadel (H)";
    if (const auto ready = itemReady(pilot, item))
        return std::string(x.title) + " ready in " + std::to_string(int64_t(std::ceil(*ready - clock_))) + " s";
    if (p->flight) return "In flight";
    if (p->down) return std::string("Your Prospector is down");
    Simulation& sim = regions_[p->region]->simulation();
    const auto unit = regions_[p->region]->driving(pilot);
    Unit* u = nullptr;
    if (unit)
        for (Unit& v : sim.state.units)
            if (v.id == *unit) u = &v;
    if (!u) return std::string("Drive your Prospector first");
    switch (item) {
    case Item::repairKit: {
        const double full = sim.maxHP(*u);
        if (u->hp >= full - 1e-9) return std::string("Your Prospector is not hurt");
        u->hp = std::min(full, u->hp + full * Shop::repairShare);
        break;
    }
    case Item::hydrogenCanister:
        bankOut(*p, p->region);
        p->wallet.hydrogen += Shop::canisterHydrogen;
        bankIn(*p, p->region);
        break;
    case Item::shieldCharge: sim.skillClocks[u->id].shield = sim.state.time + x.lasts; break;
    case Item::surveyFlare: {
        // The sites within reach go on the map for good; the deposits show
        // while it burns (the client draws them).
        const auto& sites = world_->regions[p->region].built.sites;
        for (size_t i = 0; i < sites.size(); i++)
            if (distance(u->position, sites[i].at) <= Shop::flareRadius && !siteSwallowed(p->region, i))
                p->progress.known.insert(siteId(p->region, i));
        p->flare = PilotState::Flare{p->region, u->position, clock_ + x.lasts};
        break;
    }
    }
    p->progress.useItem(item);
    p->itemReady[item] = clock_ + x.cooldown;
    pushProgress(pilot);
    return std::nullopt;
}

} // namespace ac::server
