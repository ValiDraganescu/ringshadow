// The Citadel's interior (Simulation+Citadel.members.h; C1,
// docs/citadel-interior.md): the door, the shelter, the manned corner guns.

#include "Simulation.h"

#include "Act.h"
#include "Defence.h"

#include <algorithm>
#include <cmath>

namespace ac {

bool Simulation::sheltered(const Unit& u) const {
    if (u.task == Unit::Task::atGun) return true;
    // Aboard a Dropship in flight (C4): untouchable.
    if (!state.shuttles.empty())
        if (const Shuttle* s = aboard(u); s && s->flying()) return true;
    // The strategy game's Citadel has no hall (and `target` asks this often).
    if (!Rules::citadelHall() || u.airborne()) return false;
    const double r = Rules::radius(StructureKind::citadel);
    for (const Structure& s : state.structures) {
        if (s.kind != StructureKind::citadel || !s.complete() || !(s.hp > 0)) continue;
        const Vec2 local = u.position - s.position;
        if (std::abs(local.x) >= r || std::abs(local.y) >= r) continue;
        if (state.hostile(s.owner, u.owner)) continue;
        if (Rules::footprint(s.kind).inside(local)) return true;
    }
    return false;
}

bool Simulation::doorOpen(const Structure& s) { return s.door && s.door->open >= 0.95; }

bool Simulation::swallowedByCitadel(Vec2 at) const {
    if (!Rules::citadelHall()) return false;
    const double r = Rules::radius(StructureKind::citadel) + 1;
    for (const Structure& s : state.structures) {
        if (s.kind == StructureKind::citadel && s.complete() && s.hp > 0 && std::abs(at.x - s.position.x) < r
            && std::abs(at.y - s.position.y) < r) {
            return true;
        }
    }
    return false;
}

bool Simulation::swallowedDoodadCell(Vec2 p) const {
    if (!nav || !map) return false;
    // The grid leaves a cell out for a doodad within its circle grown by the
    // clearance (`NavGrid::buildGround`), measured from the cell's centre.
    const auto i = nav->index(p);
    if (!i || nav->cliffs[size_t(*i)]) return false;
    const Vec2 c = nav->centre(*i);
    for (const Doodad& d : map->doodads) {
        if (distance(d.position, c) > 12 * ac::max(1.0, d.scale)) continue;
        for (const NavGrid::Circle& k : NavGrid::circles(d)) {
            if (distance(k.first, c) < k.second + NavGrid::clearance) return true;
        }
    }
    return false;
}

bool Simulation::standsInHall(const Unit& u) const {
    if (u.task == Unit::Task::atGun) return true;
    // In a Dropship's bay or on its ramp (C4), in flight too.
    if (!state.shuttles.empty() && aboard(u)) return true;
    if (u.airborne()) return false;
    for (const Structure& s : state.structures) {
        if (s.kind != StructureKind::citadel || !s.complete()) continue;
        const Footprint& f = Rules::footprint(s.kind);
        if (!f.interior || !NavGrid::insideStructure(u.position, s, 0)) continue;
        // In the hall or the doorway; on a wall it is pushed out as ever.
        return !f.blocks(u.position - s.position, true, 0);
    }
    return false;
}

std::optional<std::string> Simulation::doorRefusal(const Structure& s, bool open, const Unit& u) const {
    const Footprint& f = Rules::footprint(s.kind);
    if (!f.door || !s.complete() || !s.door) return std::string("No door to work here");
    // Within reach of its leaf or a panel beside it (Act.h), either side.
    double off = Act::gap(u, *Act::doorLeaf(s));
    for (const bool inside : {false, true}) off = std::min(off, Act::gap(u, *Act::doorPanel(s, inside)));
    if (off > Act::serverReach) return std::string("Too far from the door");
    if (open) return std::nullopt;
    for (const Unit& v : state.units) {
        if (!(v.hp > 0) || v.airborne() || v.task == Unit::Task::aboard) continue;
        if (f.door->contains(v.position - s.position, Rules::radius(v.kind))) return std::string("Someone is in the doorway");
    }
    return std::nullopt;
}

std::optional<std::string> Simulation::door(int64_t unit, int64_t structure, bool open) {
    const auto i = unitIndex(unit);
    if (!i) return std::string("No such unit");
    for (Structure& s : state.structures) {
        if (s.id != structure) continue;
        if (auto why = doorRefusal(s, open, state.units[size_t(*i)])) return why;
        s.door->opening = open;
        return std::nullopt;
    }
    return std::string("No such building");
}

std::optional<std::pair<size_t, int>> Simulation::mannedGun(const Unit& u) const {
    if (u.task != Unit::Task::atGun || !u.structure) return std::nullopt;
    for (size_t k = 0; k < state.structures.size(); k++) {
        const Structure& s = state.structures[k];
        if (s.id != *u.structure || !s.guns) continue;
        for (int g = 0; g < 4; g++) {
            if ((*s.guns)[size_t(g)].gunner == u.id) return std::pair{k, g};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<std::string> Simulation::man(int64_t unit, int64_t structure, int gun) {
    const auto i = unitIndex(unit);
    if (!i) return std::string("No such unit");
    Unit& u = state.units[size_t(*i)];
    if (gun < 0 || gun > 3) return std::string("No such gun");
    for (Structure& s : state.structures) {
        if (s.id != structure) continue;
        if (s.kind != StructureKind::citadel || !s.complete()) return std::string("Only a finished Citadel has guns");
        if (!s.guns) return std::string("The Citadel's guns are not armed");
        if (mannedGun(u)) return std::string("Already at a gun");
        if (!sheltered(u)) return std::string("Get inside the Citadel first");
        if (Act::gap(u, Act::console(s, gun)) > Act::serverReach) return std::string("Too far from the gun's station");
        CornerGun& g = (*s.guns)[size_t(gun)];
        if (g.gunner) return std::string("Someone is at that gun");
        g.gunner = u.id;
        u.task = Unit::Task::atGun;
        u.structure = s.id;
        u.position = Defence::gunStation(s.position, gun);
        return std::nullopt;
    }
    return std::string("No such building");
}

void Simulation::unman(int64_t unit) {
    const auto i = unitIndex(unit);
    if (!i) return;
    Unit& u = state.units[size_t(*i)];
    if (u.task != Unit::Task::atGun) return;
    if (const auto m = mannedGun(u)) (*state.structures[m->first].guns)[size_t(m->second)].gunner = std::nullopt;
    u.task = Unit::Task::idle;
    u.structure = std::nullopt;
}

void Simulation::stepCitadels(double dt) {
    for (Structure& s : state.structures) {
        if (s.kind != StructureKind::citadel || !s.complete()) continue;
        if (Rules::footprint(s.kind).door) {
            if (!s.door) s.door = Door{};
            Door& d = *s.door;
            const double step = dt / Rules::doorSeconds;
            d.open = d.opening ? ac::min(1.0, d.open + step) : ac::max(0.0, d.open - step);
        }
        if (!s.guns) continue;
        for (CornerGun& g : *s.guns) {
            if (!g.gunner) continue;
            const auto k = unitIndex(*g.gunner);
            const Unit* u = k ? &state.units[size_t(*k)] : nullptr;
            if (!u || !(u->hp > 0) || u->task != Unit::Task::atGun || u->structure != s.id) g.gunner = std::nullopt;
        }
    }
}

void Simulation::stepCornerGun(Structure& s, int i, const UnitStats& w, double dt, std::vector<GameEvent>& events) {
    CornerGun& g = (*s.guns)[size_t(i)];
    const Pilot* p = g.gunner ? pilotFor(*g.gunner) : nullptr;
    std::vector<double> late;
    if (!p) {
        // No gunner: back to rest, never a shot; the barrels run down.
        const double turn_ = turretTurnRate * dt;
        const double diff = std::remainder(Defence::cornerAngle(i) - g.aim, 2 * pi);
        g.aim = std::remainder(g.aim + ac::max(-turn_, ac::min(turn_, diff)), 2 * pi);
        g.pitch = g.pitch > 0 ? ac::max(0.0, g.pitch - turn_) : ac::min(0.0, g.pitch + turn_);
        minigun(g.cooldown, g.heat, g.overheated, g.spin, g.bloom, w, false, dt, late);
        return;
    }
    g.aim = Defence::clampAim(i, p->heading);
    g.pitch = ac::max(Defence::gunPitchMin, ac::min(Defence::gunPitchMax, p->pitch));
    // C2: no range gate. It fires along its aim whenever the trigger is down
    // and it is not overheated; what the crosshair is on is for the pose.
    g.target = std::nullopt;
    if (p->crosshair) {
        if (const auto on = p->crosshair->as<Pilot::Crosshair::On>()) {
            const auto u = state.unit(on->_0);
            const auto o = u ? std::nullopt : state.structure(on->_0);
            if ((u && targetable(*u, s.owner)) || (o && targetable(*o, s.owner))) g.target = on->_0;
        }
    }
    minigun(g.cooldown, g.heat, g.overheated, g.spin, g.bloom, w, p->act || p->hold, dt, late);
    if (late.empty()) return;
    Vec3 m = Defence::gunMuzzle(s.position, i, g.aim, g.pitch);
    m.z += groundZ(s.position);
    for (double l : late) {
        fireRound(s, m, g.aim, g.pitch, w.coneStand + g.bloom, w.damage, l, events);
        g.bloom += w.coneFire;
    }
}

} // namespace ac
