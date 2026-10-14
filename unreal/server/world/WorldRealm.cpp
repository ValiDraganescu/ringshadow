// The realm wallet (WorldMap.h, C1; docs/prospector-progression.md "The
// realm's tax"): the Citadel's cut of every unload, the pilots' donations,
// and what the Citadel's upgrades (D1) spend from it. The cut itself is the
// core's (`Progression::realmTax`, taken in `Simulation::prospectorDeposit`):
// here it is banked, from the unload's event.

#include "WorldMap.h"

namespace ac::server {

void WorldMap::realmTaxes() {
    for (size_t r = 0; r < regions_.size(); r++)
        for (const GameEvent& e : regions_[r]->lastEvents()) {
            const auto* d = e.as<GameEvent::Deposited>();
            if (!d || d->tax <= 0) continue;
            (d->hydrogen ? realm_.hydrogen : realm_.ore) += d->tax;
            // Whose Prospector paid it.
            for (auto& [id, p] : pilots_)
                if (!p.flight && p.region == r && p.unit == d->unit) {
                    (d->hydrogen ? p.taxed.hydrogen : p.taxed.ore) += d->tax;
                    break;
                }
        }
}

std::optional<std::string> WorldMap::donate(PilotId pilot, int64_t ore, int64_t hydrogen) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (ore < 0 || hydrogen < 0 || ore + hydrogen <= 0) return "Nothing to donate";
    // At the realm's Citadel, in its unload range.
    if (!atRealm(pilot, *p, {StructureKind::citadel})) return std::string("Donate at the realm's Citadel: stand where you unload");
    bankOut(*p, p->region);
    if (ore > p->wallet.ore) return "Not enough ore: you have " + std::to_string(p->wallet.ore);
    if (hydrogen > p->wallet.hydrogen) return "Not enough MH: you have " + std::to_string(p->wallet.hydrogen);
    p->wallet.ore -= ore;
    p->wallet.hydrogen -= hydrogen;
    bankIn(*p, p->region);
    realm_.ore += ore;
    realm_.hydrogen += hydrogen;
    p->donated.ore += ore;
    p->donated.hydrogen += hydrogen;
    return std::nullopt;
}

bool WorldMap::spendRealm(const Wallet& cost) {
    if (cost.ore < 0 || cost.hydrogen < 0 || cost.ore > realm_.ore || cost.hydrogen > realm_.hydrogen) return false;
    realm_.ore -= cost.ore;
    realm_.hydrogen -= cost.hydrogen;
    return true;
}

std::optional<WorldMap::Wallet> WorldMap::taxed(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? std::nullopt : std::optional(it->second.taxed);
}

std::optional<WorldMap::Wallet> WorldMap::donated(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? std::nullopt : std::optional(it->second.donated);
}

} // namespace ac::server
