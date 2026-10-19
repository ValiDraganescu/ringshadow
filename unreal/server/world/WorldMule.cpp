// The Mule in a world (WorldMap.h, C4, docs/haul-and-ferry.md section 2):
// bought at a realm Foundry, rolled out of its door, boarded and left by its
// owner alone, filled at its hopper, carried across region edges with its
// rider, forgotten when destroyed. Its booking in a Citadel's hall is the
// core's (`Simulation::stepMules`); its XP, feats and the rest are
// `workAfter`'s, its realm's cut `realmTaxes`'.

#include "WorldMap.h"

#include "Mule.h"

#include <cmath>
#include <cstdio>
#include <sstream>

namespace ac::server {

namespace {

bool isMule(const Simulation& sim, int64_t unit) {
    const auto u = sim.state.unit(unit);
    return u && u->kind == UnitKind::mule;
}

} // namespace

bool WorldMap::drivesMule(PilotId pilot, const PilotState& p) const {
    if (p.flight || !p.mule || !p.mule->unit || p.mule->region != p.region) return false;
    const auto d = regions_[p.region]->driving(pilot);
    return d && *d == *p.mule->unit;
}

bool WorldMap::drivesMule(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it != pilots_.end() && drivesMule(pilot, it->second);
}

std::optional<std::pair<size_t, int64_t>> WorldMap::mule(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.mule || !it->second.mule->unit) return std::nullopt;
    return std::pair{it->second.mule->region, *it->second.mule->unit};
}

std::optional<std::string> WorldMap::buyMule(PilotId pilot, int64_t foundry) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_) return "Not in a world";
    if (p->flight) return "In flight";
    if (p->mule) return std::string(p->mule->unit ? "You have a Mule already" : "Your Mule is rolling out");
    const Simulation& sim = regions_[p->region]->simulation();
    const auto b = sim.state.structure(foundry);
    if (!b || b->kind != StructureKind::foundry || !b->complete() || !realm(*b)) return "Buy the Mule at a finished Foundry";
    bankOut(*p, p->region);
    if (p->wallet.ore < Mule::ore()) return "Not enough ore: you have " + std::to_string(p->wallet.ore);
    if (p->wallet.hydrogen < Mule::hydrogen()) return "Not enough MH: you have " + std::to_string(p->wallet.hydrogen);
    p->wallet.ore -= Mule::ore();
    p->wallet.hydrogen -= Mule::hydrogen();
    bankIn(*p, p->region);
    p->mule = PilotState::OwnMule{p->region, std::nullopt, foundry, Mule::door(b->position), clock_ + Mule::rollOut()};
    return std::nullopt;
}

std::optional<std::string> WorldMap::muleFill(PilotId pilot, int64_t mule) {
    PilotState* p = pilotState(pilot);
    if (!p || !world_) return "Not in a world";
    if (p->flight) return "In flight";
    Region& r = *regions_[p->region];
    Simulation& sim = r.simulation();
    if (!isMule(sim, mule)) return "No Mule there";
    if (!p->mule || p->mule->region != p->region || p->mule->unit != mule) return "Not your Mule";
    const auto d = r.driving(pilot);
    if (!d || !p->unit || *d != *p->unit) return "Climb off the Mule first";
    // Its own bank's context: the fill touches no wallet, the rule is the core's.
    return sim.muleFill(*p->unit, mule);
}

std::optional<std::string> WorldMap::take(PilotId pilot, int64_t unit) {
    Region* here = regionOf(pilot);
    if (!here) return "not joined";
    if (flying(pilot)) return "in flight";
    if (PilotState* p = pilotState(pilot)) {
        Simulation& sim = here->simulation();
        if (isMule(sim, unit)) {
            if (!p->mule || p->mule->region != p->region || p->mule->unit != unit) return "Not your Mule";
            if (drivesMule(pilot, *p)) return std::nullopt;
            const auto d = here->driving(pilot);
            if (!d || !p->unit || *d != *p->unit) return "On foot first";
            const auto u = sim.state.unit(*p->unit);
            const auto m = sim.state.unit(unit);
            if (!u || !m) return "No Mule there";
            if (auto why = sim.muleBoardRefusal(*u, *m)) return why;
            // Its Prospector handed back first (it stands where it is), the
            // Mule driven, the Prospector on the perch.
            if (!here->take(pilot, unit)) return "That Mule cannot be driven now";
            if (auto why = sim.muleBoard(*p->unit, unit)) {
                here->take(pilot, *p->unit);
                return why;
            }
            return std::nullopt;
        }
        // Off the Mule: its own Prospector again, set down by the perch.
        if (p->unit && unit == *p->unit && drivesMule(pilot, *p)) {
            if (!here->take(pilot, unit)) return "Can't climb off here";
            return std::nullopt;
        }
    }
    if (!here->take(pilot, unit)) return "that unit cannot be driven";
    return std::nullopt;
}

std::optional<std::string> WorldMap::handBack(PilotId pilot) {
    Region* here = regionOf(pilot);
    if (!here) return "not joined";
    if (const PilotState* p = pilotState(pilot); p && p->unit && drivesMule(pilot, *p)) return take(pilot, *p->unit);
    if (!here->driving(pilot)) return "nothing driven";
    here->handBack(pilot);
    return std::nullopt;
}

void WorldMap::arriveMule(PilotId pilot, PilotState& p, size_t to, const Unit& mule, const Unit& rider, Vec2 at,
                          int64_t lastInput, bool exact) {
    Region& r = *regions_[to];
    r.join(pilot, lastInput);
    p.region = to;
    bankIn(p, to);
    r.setLevels(pilot, p.record);
    r.setProgress(pilot, p.progress, ownBuildings(p, to));
    scan(p, to);
    Simulation& sim = r.simulation();
    const region::Built& places = *r.places();
    Unit m = mule;
    m.cargo = std::vector<int64_t>{};
    m.position = exact ? at : openGround(sim, m, places.ground(at), 6);
    const int64_t mid = r.admit(m);
    Unit w = rider;
    w.position = m.position;
    const int64_t pid = r.admit(w);
    p.unit = pid;
    p.mule->region = to;
    p.mule->unit = mid;
    p.last = places.regionAt(m.position);
    // On it again, driving it.
    r.take(pilot, mid);
    sim.muleBoard(pid, mid);
}

void WorldMap::mulesStep() {
    auto step = [&](PilotState& p, std::optional<PilotId> pilot) {
        if (!p.mule) return;
        PilotState::OwnMule& m = *p.mule;
        Region& r = *regions_[m.region];
        Simulation& sim = r.simulation();
        if (!m.unit) {
            if (clock_ < m.readyAt) return;
            // Out of the door (the Foundry's, or where its door was).
            m.unit = sim.state.structure(m.foundry) ? *sim.rollOut(m.foundry, Pilot::player) : sim.rollOutAt(m.door, Pilot::player);
            m.readyAt = 0;
            return;
        }
        if (sim.state.unit(*m.unit)) return;
        // Destroyed: its load lost, its rider set down by the core; the
        // pilot drives its Prospector again; a new one may be bought.
        p.mule = std::nullopt;
        if (!pilot || p.flight || !p.unit || p.region != m.region) return;
        const auto d = r.driving(*pilot);
        if (!d || !sim.state.unit(*d)) r.take(*pilot, *p.unit);
    };
    for (auto& [id, p] : pilots_) step(p, id);
    for (auto& [key, p] : away_) step(p, std::nullopt);
}

std::string WorldMap::devMule(const std::string& line) {
    std::istringstream in(line);
    std::string word, what;
    in >> word >> what;
    if (!world_ || pilots_.empty()) return "dev: mule: no world, or no pilot";
    auto& [pilot, p] = *devPilot();
    if (p.flight) return "dev: mule: in flight";
    const size_t r = p.region;
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    const auto me = p.unit ? sim.unitIndex(*p.unit) : std::nullopt;
    if (what == "foundry") {
        // A complete Foundry ahead, its door (+y) toward the pilot.
        if (!me) return "dev: mule foundry: the pilot has no Prospector here";
        const Vec2 at = st.units[size_t(*me)].position;
        const double heading = st.units[size_t(*me)].heading;
        const double half = Rules::radius(StructureKind::foundry);
        for (double d = half + 4; d <= half + 40; d += 1)
            for (const double turn : {0.0, 0.4, -0.4, 0.8, -0.8, 1.2, -1.2}) {
                const auto spot = sim.snap(StructureKind::foundry, at + Vec2(std::cos(heading + turn), std::sin(heading + turn)) * d);
                if (!spot || sim.placementRefusal(StructureKind::foundry, *spot)) continue;
                const Vec2 stand = Mule::door(*spot) + Vec2(0, Mule::length / 2 + 3);
                if (!sim.nav || !sim.nav->walkable(stand) || sim.nav->inRock(stand) || !sim.nav->walkable(Mule::door(*spot))) continue;
                Structure f(st.nextID++, StructureKind::foundry, Pilot::player, *spot);
                f.hp = Rules::hp(StructureKind::foundry);
                st.structures.push_back(f);
                builtBy_[r][f.id] = p.key;
                Unit& u = st.units[size_t(*me)];
                u.position = stand;
                u.heading = -pi / 2;
                u.task = Unit::Task::idle;
                if (Pilot* driver = sim.pilotFor(u.id)) driver->heading = u.heading;
                sim.reindex();
                sim.refreshNav();
                const Vec2 g = regions_[r]->places()->regionAt(*spot);
                char out[160];
                std::snprintf(out, sizeof out, "dev: mule foundry: foundry %lld at (%.1f, %.1f) in %s", (long long)f.id, g.x, g.y,
                              world_->regions[r].file.id.c_str());
                return out;
            }
        return "dev: mule foundry: no clear ground ahead";
    }
    if (what == "here") {
        if (p.mule) return "dev: mule here: it has a Mule already";
        if (!me) return "dev: mule here: the pilot has no Prospector here";
        const Unit& u = st.units[size_t(*me)];
        // Beside it, on its right, facing as it faces.
        const Vec2 right(std::sin(u.heading), -std::cos(u.heading));
        const Vec2 at = openGround(sim, u, u.position + right * 2.5, 6);
        const int64_t id = sim.rollOutAt(at, Pilot::player);
        if (const auto i = sim.unitIndex(id)) {
            st.units[size_t(*i)].position = at;
            st.units[size_t(*i)].heading = u.heading;
        }
        PilotState::OwnMule m;
        m.region = r;
        m.unit = id;
        p.mule = m;
        return "dev: mule here: mule " + std::to_string(id);
    }
    if (what == "ready") {
        if (!p.mule || p.mule->unit) return "dev: mule ready: none rolling out";
        p.mule->readyAt = clock_;
        mulesStep();
        return p.mule && p.mule->unit ? "dev: mule ready: mule " + std::to_string(*p.mule->unit) : "dev: mule ready: not out";
    }
    if (what == "door") {
        // Out of the realm's Citadel here, on its door's axis facing in,
        // its centre 9.5 cells from the hall's middle, the door open; the
        // Prospector on foot beside its hopper (1.5 cells off the axis).
        if (!p.mule || !p.mule->unit || p.mule->region != r) return "dev: mule door: no Mule out here";
        Structure* c = nullptr;
        for (Structure& b : st.structures)
            if (b.kind == StructureKind::citadel && b.complete() && realm(b)) c = &b;
        if (!c) return "dev: mule door: no Citadel here";
        const auto mi = sim.unitIndex(*p.mule->unit);
        if (!mi) return "dev: mule door: no Mule out here";
        // Its way in: open ground from there to the hall's middle.
        {
            Unit probe = st.units[size_t(*mi)];
            const Structure shut = *c;
            c->door = Door{1, true};
            for (double y = 9.5; y >= 0; y -= 0.25)
                if (!sim.pilotCanStand(probe, c->position + Vec2(0, y))) {
                    c->door = shut.door;
                    char out[120];
                    std::snprintf(out, sizeof out, "dev: mule door: the way in is blocked %.2f cells out", y);
                    return out;
                }
        }
        if (drivesMule(pilot, p)) take(pilot, *p.unit);
        Unit& m = st.units[size_t(*mi)];
        m.position = c->position + Vec2(0, 9.5);
        m.heading = -pi / 2;
        if (const auto me2 = p.unit ? sim.unitIndex(*p.unit) : std::nullopt) {
            Unit& u = st.units[size_t(*me2)];
            u.position = Mule::hopper(m) + Vec2(1.5, 0);
            u.heading = pi;
            u.task = Unit::Task::idle;
            if (Pilot* driver = sim.pilotFor(u.id)) driver->heading = u.heading;
        }
        sim.reindex();
        return "dev: mule door: mule " + std::to_string(m.id) + " at the door of citadel " + std::to_string(c->id);
    }
    if (what == "slope" || what == "steep") {
        // C4, the drive (docs/haul-and-ferry.md "The drive"): the pilot's
        // Mule set 6 cells short of a slope of the region's ground, facing
        // up it on gentle open ground: `slope` one its wheels climb (12° to
        // 20°), `steep` one too steep for them (28° to 40°, ground a walker
        // stands on). The nearest such within 40 cells.
        if (!p.mule || !p.mule->unit || p.mule->region != r) return "dev: mule " + what + ": no Mule out here";
        const auto mi = sim.unitIndex(*p.mule->unit);
        if (!mi || !sim.nav || !sim.field) return "dev: mule " + what + ": no Mule out here";
        Unit& m = st.units[size_t(*mi)];
        const bool steep = what == "steep";
        const double lo = std::tan((steep ? 28.0 : 12.0) * pi / 180), hi = std::tan((steep ? 40.0 : 20.0) * pi / 180);
        const double flat = std::tan(6.0 * pi / 180);
        const NavGrid& nav = *sim.nav;
        std::optional<std::pair<Vec2, double>> best;
        for (int64_t i = 0; i < nav.width * nav.height; i++) {
            if (!nav.free(i)) continue;
            const Vec2 q = nav.centre(i);
            if (distance(q, m.position) > 40 || (best && distance(q, m.position) >= distance(best->first, m.position))) continue;
            for (int k = 0; k < 16; k++) {
                const Vec2 dir(std::cos(k * pi / 8), std::sin(k * pi / 8));
                const double up = sim.groundPitch(m, q, dir);
                if (!(up >= lo && up <= hi)) continue;
                // Its run-up: open, gentle ground 6 to 9 cells back.
                bool clear = true;
                for (double t = 0.5; t <= 9 && clear; t += 0.5) {
                    const Vec2 b = q - dir * t;
                    clear = nav.walkable(b) && !nav.inRock(b) && (t < 6 || std::abs(sim.groundPitch(m, b, dir)) < flat);
                }
                if (!clear) continue;
                best = std::make_pair(q - dir * 6, std::atan2(dir.y, dir.x));
                break;
            }
        }
        if (!best) return "dev: mule " + what + ": no such slope here";
        m.position = best->first;
        m.heading = best->second;
        if (Pilot* driver = sim.pilotFor(m.id)) driver->heading = m.heading;
        sim.reindex();
        const Vec2 g = regions_[r]->places()->regionAt(m.position);
        char out[160];
        std::snprintf(out, sizeof out, "dev: mule %s: mule %lld at (%.1f, %.1f) facing %.2f", what.c_str(), (long long)m.id, g.x, g.y,
                      m.heading);
        return out;
    }
    if (what == "load") {
        int64_t ore = 0, mh = 0;
        in >> ore >> mh;
        if (!p.mule || !p.mule->unit) return "dev: mule load: no Mule out";
        const auto i = regions_[p.mule->region]->simulation().unitIndex(*p.mule->unit);
        if (!i || ore < 0 || mh < 0) return "dev: mule load: refused";
        regions_[p.mule->region]->simulation().state.units[size_t(*i)].haul = Haul{ore, mh};
        return "dev: mule load: " + std::to_string(ore) + " ore, " + std::to_string(mh) + " MH";
    }
    return "dev: mule: '" + what + "' is not foundry, here, ready, door, slope, steep or load N [MH]";
}

void WorldMap::muleTold(const PilotState& p, net::ProgressMessage& m) const {
    if (!p.mule) return;
    m.mule = net::ProgressMessage::Mule{world_->regions[p.mule->region].file.id, p.mule->unit.value_or(0), p.mule->foundry,
                                        p.mule->unit ? 0.0 : p.mule->readyAt};
}

} // namespace ac::server
