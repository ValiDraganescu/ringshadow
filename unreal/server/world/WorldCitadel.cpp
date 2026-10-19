// The Citadel's interior in a world (WorldMap.h, C1; docs/citadel-interior.md):
// the door and the corner guns asked for by a pilot, the realm's Citadels
// grown in place on load (schema 27), and the dev lines for the flow tests
// (C2: a Target Dummy ahead; C4: a load in the gripper).
// The door, the shelter and the guns themselves are the core's
// (Simulation+Citadel.members.h).

#include "WorldMap.h"

#include "Defence.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ac::server {

namespace {

/// The realm's complete Citadel nearest `at` in `st`, by `realm`.
template <class Realm> const Structure* nearestCitadel(const GameState& st, Vec2 at, const Realm& realm) {
    const Structure* best = nullptr;
    for (const Structure& s : st.structures)
        if (s.kind == StructureKind::citadel && s.complete() && s.hp > 0 && realm(s) &&
            (!best || distance(s.position, at) < distance(best->position, at)))
            best = &s;
    return best;
}

} // namespace

std::optional<std::string> WorldMap::door(PilotId pilot, bool open) {
    PilotState* p = pilotState(pilot);
    if (!p || p->flight) return "Not in a world";
    Simulation& sim = regions_[p->region]->simulation();
    const auto me = p->unit ? sim.state.unit(*p->unit) : std::nullopt;
    if (!me || !(me->hp > 0)) return "Nothing driven";
    const Structure* c = nearestCitadel(sim.state, me->position, [&](const Structure& s) { return realm(s); });
    if (!c) return "No finished Citadel of the realm here";
    return sim.door(me->id, c->id, open);
}

std::optional<std::string> WorldMap::man(PilotId pilot, int gun) {
    PilotState* p = pilotState(pilot);
    if (!p || p->flight) return "Not in a world";
    Simulation& sim = regions_[p->region]->simulation();
    const auto me = p->unit ? sim.state.unit(*p->unit) : std::nullopt;
    if (!me || !(me->hp > 0)) return "Nothing driven";
    if (gun < 0) {
        if (!sim.mannedGun(*me)) return "Not at a gun";
        sim.unman(me->id);
        return std::nullopt;
    }
    if (gun > 3) return "No such gun";
    const Structure* c = nearestCitadel(sim.state, me->position, [&](const Structure& s) { return realm(s); });
    if (!c) return "No finished Citadel of the realm here";
    return sim.man(me->id, c->id, gun);
}

bool WorldMap::humanIn(size_t r) const {
    for (const auto& [id, p] : pilots_)
        if (!p.flight && p.region == r && p.unit) return true;
    return false;
}

bool WorldMap::siteSwallowed(size_t r, size_t i) const {
    if (!world_ || r >= world_->regions.size() || r >= regions_.size() || i >= world_->regions[r].built.sites.size()) return false;
    // The core's predicate, the one the game hides the crate by.
    return regions_[r]->simulation().swallowedByCitadel(world_->regions[r].built.sites[i].at);
}

std::vector<std::string> WorldMap::adoptCitadels(size_t r) {
    std::vector<std::string> said;
    if (!world_ || r >= regions_.size()) return said;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const std::string& region = world_->regions[r].file.id;
    const double half = Rules::radius(StructureKind::citadel) + 1;
    auto in = [&](const Structure& c, Vec2 at, double grow) {
        return std::abs(at.x - c.position.x) < half + grow && std::abs(at.y - c.position.y) < half + grow;
    };
    auto where = [&](Vec2 at) {
        const Vec2 g = regions_[r]->places()->regionAt(at);
        char out[48];
        std::snprintf(out, sizeof out, "(%.1f, %.1f)", g.x, g.y);
        return std::string(out);
    };
    for (const Structure& c : st.structures) {
        if (c.kind != StructureKind::citadel || !realm(c)) continue;
        const std::string citadel = "citadel " + std::to_string(c.id) + " in " + region;
        // Its ore: emptied for good, as a mound under a building.
        for (size_t k = 0; k < st.patches.size(); k++) {
            OreDeposit& d = st.patches[k];
            if (!in(c, d.position, 0) || (d.initial == 0 && d.remaining == 0)) continue;
            said.push_back("schema 27: " + citadel + " grown over deposit " + std::to_string(k) + " at " + where(d.position) +
                           ", " + std::to_string(d.remaining) + " ore: emptied");
            d.initial = 0;
            d.remaining = 0;
            d.depletedAt = std::nullopt;
            d.miner = std::nullopt;
        }
        const auto& sites = world_->regions[r].built.sites;
        for (size_t i = 0; i < sites.size(); i++)
            if (c.complete() && in(c, sites[i].at, 0))
                said.push_back("schema 27: " + citadel + " grown over site " + sites[i].name + " at " + where(sites[i].at) +
                               ": swallowed (never seen, never opened)");
        if (sim.map) {
            int64_t rocks = 0;
            for (const Doodad& d : sim.map->doodads) rocks += in(c, d.position, 0) ? 1 : 0;
            if (rocks > 0)
                said.push_back("schema 27: " + citadel + " grown over " + std::to_string(rocks) +
                               " of the map's doodads: left (the core and the game leave them out)");
        }
        for (const Structure& b : st.structures)
            if (b.id != c.id && in(c, b.position, Rules::radius(b.kind)) && !Rules::walkable(b.kind))
                said.push_back("schema 27: " + citadel + " grown over " + Simulation::title(b.kind) + " " + std::to_string(b.id) +
                               " at " + where(b.position) + ": left");
    }
    return said;
}

std::string WorldMap::devCarry(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    long long n = 0;
    in >> word >> what >> n;
    if (!world_ || pilots_.empty()) return "dev: carry: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (p.flight) return "dev: carry: in flight";
    Simulation& sim = regions_[p.region]->simulation();
    const auto me = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
    if (!me || sim.state.units[size_t(*me)].kind != UnitKind::prospector) return "dev: carry: the pilot drives no Prospector";
    Unit& u = sim.state.units[size_t(*me)];
    if (what == "none") {
        u.carrying = 0;
        u.hydrogen = std::nullopt;
        return "dev: carry none";
    }
    if ((what != "ore" && what != "mh") || n <= 0) return "dev: carry: '" + line + "' is not ore|mh N or none";
    u.carrying = n;
    u.hydrogen = what == "mh";
    return "dev: carry " + what + " " + std::to_string(n);
}

std::string WorldMap::devCitadel(const std::string& line) {
    std::istringstream in(line);
    std::string word, what, arg;
    in >> word >> what >> arg;
    if (!world_ || pilots_.empty()) return "dev: citadel: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (what == "here") {
        // A complete Citadel of the realm on clear ground ahead of the
        // pilot, armed, its door shut; the Prospector 4 cells out from its
        // door (the +y face), facing it.
        if (p.flight) return "dev: citadel here: in flight";
        const size_t r = p.region;
        Simulation& sim = regions_[r]->simulation();
        GameState& st = sim.state;
        const auto me = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
        if (!me) return "dev: citadel here: the pilot drives nothing";
        const Vec2 at = st.units[size_t(*me)].position;
        const double heading = st.units[size_t(*me)].heading;
        const double half = Rules::radius(StructureKind::citadel);
        // Ahead first, then round to the sides and behind (C5: the bigger
        // buildings an earlier staging left crowd the start).
        for (double d = half + 3; d <= half + 40; d += 1)
            for (const double turn : {0.0, 0.4, -0.4, 0.8, -0.8, 1.2, -1.2, 1.6, -1.6, 2.0, -2.0, 2.4, -2.4, 2.8, -2.8, pi}) {
                const auto spot = sim.snap(StructureKind::citadel, at + Vec2(std::cos(heading + turn), std::sin(heading + turn)) * d);
                // Off the pads' plates, as a pilot's build.
                if (!spot || sim.placementRefusal(StructureKind::citadel, *spot) ||
                    region::touchesPadPlate(world_->regions[r].built.pads, *spot, half))
                    continue;
                const Vec2 stand = *spot + Vec2(0, half + 4);
                if (!sim.nav || !sim.nav->walkable(stand) || sim.nav->inRock(stand)) continue;
                // The way to its door clear of buildings, a Mule's length
                // past the stand (C5: a 7x7 Foundry put up first may stand
                // across it).
                bool clear = true;
                for (double y = half; y <= half + 12 && clear; y += 0.5)
                    for (double x = -2; x <= 2 && clear; x += 0.5)
                        for (const Structure& b : st.structures)
                            if (!Rules::walkable(b.kind) && NavGrid::insideStructure(*spot + Vec2(x, y), b, 0.5)) clear = false;
                if (!clear) continue;
                Structure c(st.nextID++, StructureKind::citadel, Pilot::player, *spot);
                c.hp = Rules::hp(StructureKind::citadel);
                st.structures.push_back(c);
                builtBy_[r][c.id] = p.key;
                Unit& u = st.units[size_t(*me)];
                u.position = stand;
                u.heading = -pi / 2;
                u.task = Unit::Task::idle;
                if (Pilot* driver = sim.pilotFor(u.id)) driver->heading = u.heading;
                sim.reindex();
                sim.refreshNav();
                armCitadels();
                const Vec2 g = regions_[r]->places()->regionAt(*spot);
                char out[160];
                std::snprintf(out, sizeof out, "dev: citadel here: citadel %lld at (%.1f, %.1f) in %s", (long long)c.id, g.x, g.y,
                              world_->regions[r].file.id.c_str());
                return out;
            }
        return "dev: citadel here: no clear ground ahead";
    }
    if (what == "door" && (arg == "open" || arg == "close")) {
        const auto why = door(pilot, arg == "open");
        return why ? "dev: citadel door " + arg + ": refused: " + *why : "dev: citadel door " + arg;
    }
    if (what == "man" && !arg.empty()) {
        int gun = -1;
        if (arg != "none") {
            char* end = nullptr;
            const long n = std::strtol(arg.c_str(), &end, 10);
            if (!end || *end != '\0' || n < 0 || n > 3) return "dev: citadel man: '" + arg + "' is not 0|1|2|3|none";
            gun = int(n);
        }
        const auto why = man(pilot, gun);
        return why ? "dev: citadel man " + arg + ": refused: " + *why : "dev: citadel man " + arg;
    }
    return "dev: citadel: '" + what + (arg.empty() ? "" : " " + arg) + "' is not here, door open|close or man N|none";
}

std::string WorldMap::devDummy(const std::string& line) {
    std::istringstream in(line);
    std::string word, what, arg;
    in >> word >> what >> arg;
    if (!world_ || pilots_.empty()) return "dev: dummy: no world, or no pilot";
    if (what != "here") return "dev: dummy: '" + what + "' is not here";
    // How far ahead: 20 cells, or the line's number (2 to 80).
    double ahead = 20;
    if (!arg.empty()) {
        char* end = nullptr;
        const double d = std::strtod(arg.c_str(), &end);
        if (!end || *end != '\0' || !(d >= 2 && d <= 80)) return "dev: dummy here: '" + arg + "' is not a distance from 2 to 80 cells";
        ahead = d;
    }
    auto& [pilot, p] = *devPilot();
    if (p.flight) return "dev: dummy here: in flight";
    const size_t r = p.region;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const auto me = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
    if (!me) return "dev: dummy here: the pilot drives nothing";
    const Unit& u = st.units[size_t(*me)];
    // Ahead: the gun's aim when the unit mans one, else its heading.
    double heading = u.heading;
    for (const Structure& b : st.structures)
        if (b.guns)
            for (const CornerGun& g : *b.guns)
                if (g.gunner == u.id) heading = g.aim;
    const Vec2 at = u.position;
    const double slack = std::min(6.0, ahead * 0.3);
    for (double d = 0; d <= slack; d += 0.5)
        for (const double sign : {1.0, -1.0})
            for (const double turn : {0.0, 0.1, -0.1, 0.2, -0.2}) {
                const double reach = ahead + sign * d;
                const auto spot = sim.snap(StructureKind::dummy, at + Vec2(std::cos(heading + turn), std::sin(heading + turn)) * reach);
                if (!spot || sim.placementRefusal(StructureKind::dummy, *spot) ||
                    region::touchesPadPlate(world_->regions[r].built.pads, *spot, Rules::radius(StructureKind::dummy)))
                    continue;
                Structure dummy(st.nextID++, StructureKind::dummy, Pilot::player, *spot);
                dummy.hp = Rules::hp(StructureKind::dummy);
                st.structures.push_back(dummy);
                builtBy_[r][dummy.id] = p.key;
                sim.reindex();
                sim.refreshNav();
                const Vec2 g = regions_[r]->places()->regionAt(*spot);
                char out[192];
                std::snprintf(out, sizeof out, "dev: dummy here: dummy %lld at (%.1f, %.1f) in %s, %.1f cells out", (long long)dummy.id, g.x,
                              g.y, world_->regions[r].file.id.c_str(), distance(*spot, at));
                return out;
            }
    char none[96];
    std::snprintf(none, sizeof none, "dev: dummy here: no clear ground %.0f cells ahead", ahead);
    return none;
}

} // namespace ac::server
