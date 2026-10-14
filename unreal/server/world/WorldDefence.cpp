// The realm's defences (WorldMap.h, D1 and D2; docs/mmorpg-m8.md): the
// Citadel's four corner turrets, armed with the realm's marks and upgraded
// from the realm wallet. The guns themselves are the core's
// (`Defence::weapon`, `Simulation::stepTurret`); a placed AA tower or
// gatling gun is a building like any other, from a blueprint.

#include "WorldMap.h"

#include "Defence.h"
#include "NavGrid.h"

#include <cmath>
#include <sstream>

namespace ac::server {

void WorldMap::armCitadels() {
    for (auto& r : regions_)
        for (Structure& s : r->simulation().state.structures)
            if (s.kind == StructureKind::citadel && realm(s)) s.marks = citadelMarks_;
}

std::optional<std::string> WorldMap::upgradeCitadel(PilotId pilot, DefenceStat stat) {
    PilotState* p = pilotState(pilot);
    if (!p) return "Not in a world";
    if (!atRealm(pilot, *p, {StructureKind::citadel}))
        return std::string("Upgrade at the realm's Citadel: stand where you unload");
    if (const auto no = Defence::citadelRefusal(stat, citadelMarks_, realm_.ore, realm_.hydrogen)) return no;
    const ShopPrice price = Defence::citadelPrice(stat, citadelMarks_.of(stat) + 1);
    if (!spendRealm(Wallet{price.ore, price.hydrogen})) return "The realm wallet is short";
    citadelMarks_.of(stat) += 1;
    armCitadels();
    return std::nullopt;
}

std::string WorldMap::devDefence(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: defence: no world, or no pilot";
    auto& [pilot, p] = *pilots_.begin();
    if (what == "wallet") {
        realm_.ore += 3000;
        realm_.hydrogen += 400;
        return "dev: defence: the realm wallet holds " + std::to_string(realm_.ore) + " ore, " + std::to_string(realm_.hydrogen) + " MH";
    }
    if (what == "blueprints") {
        p.progress.giveBlueprint(StructureKind::gatling, 2);
        p.progress.giveBlueprint(StructureKind::sentinel, 2);
        return "dev: defence: two gatling gun and two AA tower blueprints";
    }
    const size_t r = p.region;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    if (!sim.broodSide) return "dev: defence: no brood level in " + world_->regions[r].file.id;
    const auto me = p.unit ? st.unit(*p.unit) : std::nullopt;
    if (!me) return "dev: defence: the pilot drives nothing";
    const Vec2 at = me->position;
    const double heading = me->heading;
    // A finished `kind` of the realm about `k` cells ahead, on clear ground.
    auto placeAhead = [&](StructureKind kind, double k) -> std::optional<Structure> {
        for (double d = k; d <= k + 10; d += 1)
            for (const double turn : {0.0, 0.4, -0.4, 0.8, -0.8}) {
                const Vec2 spot = at + Vec2(std::cos(heading + turn), std::sin(heading + turn)) * d;
                if (!sim.nav || !sim.nav->walkable(spot) || sim.nav->inRock(spot)) continue;
                bool clear = true;
                for (const Structure& b : st.structures)
                    if (distance(b.position, spot) < Rules::radius(b.kind) + Rules::radius(kind) + 1.5) clear = false;
                if (!clear) continue;
                st.structures.push_back(Structure(st.nextID++, kind, Pilot::player, spot));
                st.structures.back().hp = Rules::hp(kind);
                builtBy_[r][st.structures.back().id] = p.key;
                sim.reindex();
                sim.refreshNav();
                return st.structures.back();
            }
        return std::nullopt;
    };
    std::optional<int64_t> g;
    if (what == "citadel") {
        // A raid on the realm's Citadel nearest the pilot, from the pilot's side.
        std::optional<Structure> c;
        for (const Structure& b : st.structures)
            if (realm(b) && b.complete() && b.kind == StructureKind::citadel && (!c || distance(b.position, at) < distance(c->position, at)))
                c = b;
        if (!c) return "dev: defence citadel: no Citadel of the realm here";
        // One group (one warning): Shardlings with Gloamwings, from the pilot's left.
        const Vec2 off = normalize(at - c->position + Vec2(0.01, 0));
        const Vec2 side(-off.y, off.x);
        g = spawnBroodGroup(r, UnitKind::shardling, 8, Poi::building, c->id, c->position + off * 12 + side * 9, false);
        if (g && spawnBroodGroup(r, UnitKind::gloamwing, 3, Poi::building, c->id, c->position + off * 12 + side * 11, false)) {
            BroodGroup flyers = brood_[r].groups.back();
            brood_[r].groups.pop_back();
            BroodGroup& pack = brood_[r].groups.back();
            for (const int64_t b : flyers.bugs) {
                pack.bugs.push_back(b);
                for (Unit& u : st.units)
                    if (u.id == b && u.bug) u.bug->group = pack.id;
            }
        }
    } else if (what == "gatling" || what == "aa") {
        const StructureKind kind = what == "gatling" ? StructureKind::gatling : StructureKind::sentinel;
        const auto gun = placeAhead(kind, 5);
        if (!gun) return "dev: defence " + what + ": no room ahead";
        // Its bugs beyond it, inside its reach and across the pilot's view,
        // set on it (no raid: no warning over the picture).
        const Vec2 dir = normalize(gun->position - at + Vec2(0.01, 0));
        const Vec2 side(-dir.y, dir.x);
        std::optional<size_t> dep;
        for (size_t i = 0; i < st.patches.size(); i++)
            if (st.patches[i].remaining > 0 && (!dep || distance(st.patches[i].position, at) < distance(st.patches[*dep].position, at))) dep = i;
        if (!dep) return "dev: defence " + what + ": no deposit";
        // On open ground 4 to 5 cells from it: beyond it first, then round it.
        Vec2 site = gun->position + dir * 4 + side * 3;
        bool found = false;
        for (const double turn : {0.6, -0.6, 0.0, 1.2, -1.2, 1.8, -1.8, 2.4, -2.4, 3.1})
            for (const double k : {4.5, 4.0, 5.0}) {
                if (found) break;
                const double h = std::atan2(dir.y, dir.x) + turn;
                const Vec2 q = gun->position + Vec2(std::cos(h), std::sin(h)) * k;
                if (sim.nav && sim.nav->walkable(q) && !sim.nav->inRock(q)) {
                    site = q;
                    found = true;
                }
            }
        if (!found) return "dev: defence " + what + ": no open ground round it";
        if (kind == StructureKind::gatling) {
            // Shardlings march on it: a raid (it ignores the opening's calm
            // round the start pad), from 9 cells beyond.
            g = spawnBroodGroup(r, UnitKind::shardling, 6, Poi::building, gun->id, gun->position + dir * 9 + side * 3, false);
            return g ? "dev: defence gatling: group " + std::to_string(*g) + " in " + world_->regions[r].file.id
                     : std::string("dev: defence gatling: not placed");
        }
        g = spawnBroodGroup(r, UnitKind::gloamwing, 3, Poi::deposit, int64_t(*dep), site, true);
        if (g) {
            BroodGroup& grp = brood_[r].groups.back();
            grp.site = site;
            grp.radius = 2.5;
            grp.turret = gun->id;
        }
    } else {
        return "dev: defence: '" + what + "' is not wallet|blueprints|citadel|gatling|aa";
    }
    if (!g) return "dev: defence " + what + ": not placed";
    return "dev: defence " + what + ": group " + std::to_string(*g) + " in " + world_->regions[r].file.id;
}

} // namespace ac::server
