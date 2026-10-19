// Port of the `extension Simulation` of Sources/GameCore/Pilot.swift: the
// player driving one unit by hand. The types are in Pilot.h, the member
// declarations in Simulation+Pilot.members.h.
//
// Built for any unit kind: walking, facing and the hand-back are the same
// for all; what the action key does depends on the kind (`pilotTarget`,
// `pilotAct`), as does anything the unit is busy with (`pilotBusy`).

#include "Simulation.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ac {

// MARK: - Taking over

/// Take the unit `unit` under manual control; nil hands it back to the
/// AI. False when it is not the player's, or not a kind that can be
/// driven yet.
bool Simulation::take(std::optional<int64_t> unit) {
    if (pilot) {
        if (const auto i = unitIndex(pilot->unit)) {
            // Handed back: it finds work, goes home with its load, or rejoins
            // the army by itself.
            Unit u = state.units[*i];
            // C4: a Prospector on a Mule stays on it (its pilot drives the Mule).
            const bool riding = ridden(u).has_value();
            if (u.kind == Unit::Kind::prospector && u.task != Unit::Task::mining && u.task != Unit::Task::inDerrick && !riding) {
                u.task = Unit::Task::idle;
                u.timer = 0;
            }
            u.goal = std::nullopt;
            u.waypoints = std::nullopt;
            // Mid-jump, it lands; a healer stops healing; anyone aboard is
            // set down (the AI only unloads on a drop it planned itself).
            if (u.jumpTo && u.jumpFrom) {
                u.position = *u.jumpTo;
                u.jumpFrom = std::nullopt;
                u.jumpTo = std::nullopt;
            }
            // Driven, it stood where the AI's grid has no ground (beside a
            // rock, by a building's wall: `pilotCanStand` is closer than the
            // grid's cells): onto the nearest free ground, so the AI walks
            // on from there and `trapped` never takes it for boxed in.
            if (nav && !u.stats().air && !riding && !standsInHall(u) && !nav->walkable(u.position)) {
                if (const auto out = nav->nearestFree(u.position, 3)) u.position = *out;
            }
            if (u.task == Unit::Task::attacking) {
                u.task = Unit::Task::idle;
                u.target = std::nullopt;
            }
            // C4: a Mule's rider climbs off it beside its perch instead.
            const bool rider = u.kind == Unit::Kind::mule && u.cargo && !u.cargo->empty();
            if (u.cargo && !u.cargo->empty() && !rider) unload(u);
            // Its pilot's shield goes with the pilot.
            u.shield = std::nullopt;
            state.units[*i] = u;
            if (rider) muleLeave(u.id);
        }
    }
    pilot = std::nullopt;
    if (!unit) return true;
    const auto i = unitIndex(*unit);
    if (!i || state.units[*i].owner != Pilot::player || !Pilot::drivable.contains(state.units[*i].kind)
        || !canDrive(state.units[*i])) {
        return false;
    }
    Unit u = state.units[*i];
    // Whatever it was doing stops; a paid building order is refunded.
    if (u.order) {
        state.players[u.owner].ore += Rules::cost(u.order->kind);
        u.order = std::nullopt;
    }
    if (u.task == Unit::Task::building && u.structure) {
        const int64_t sid = *u.structure;
        const auto s = std::find_if(state.structures.begin(), state.structures.end(),
                                    [&](const Structure& st) { return st.id == sid; });
        if (s != state.structures.end() && s->builder == u.id) s->builder = std::nullopt;
    }
    switch (u.task) {
    case Unit::Task::mining:
    case Unit::Task::inDerrick: break;   // busy work carries on under the pilot
    case Unit::Task::building:
    case Unit::Task::repairing:
    case Unit::Task::toBuild:
    case Unit::Task::toBastion:
        u.structure = std::nullopt;
        u.task = Unit::Task::idle;
        break;
    default: u.task = Unit::Task::idle;
    }
    u.goal = std::nullopt;
    u.waypoints = std::nullopt;
    u.target = std::nullopt;
    u.mission = std::nullopt;
    state.units[*i] = u;
    pilot = Pilot(*unit, u.heading);
    // The kind joins this game's leveling record, at level 1 if new,
    // and counts a take-over.
    updateLevels([&](PilotRecord& r) { r.kinds[u.kind].tally.takeovers += 1; });
    return true;
}

/// A unit that is somewhere it cannot be driven from: inside a Bastion or
/// a Dropship, or mid-jump.
bool Simulation::canDrive(const Unit& u) const {
    return u.task != Unit::Task::inBastion && u.task != Unit::Task::aboard && !u.jumpFrom;
}

/// True for a unit a human drives (W5: any of them).
bool Simulation::piloted(int64_t id) const { return (pilot && pilot->unit == id) || driverIndex(id).has_value(); }

// MARK: - More humans driving (W5)

std::optional<size_t> Simulation::driverIndex(int64_t unit) const {
    for (size_t k = 0; k < drivers.size(); k++)
        if (drivers[k].pilot.unit == unit) return k;
    return std::nullopt;
}

void Simulation::swapDriver(size_t k) {
    Driver& d = drivers[k];
    std::optional<Pilot> active = pilot;
    pilot = d.pilot;
    d.pilot = active.value_or(Pilot{});
    std::swap(progress, d.progress);
    std::swap(skillEffects, d.skillEffects);
    std::swap(pilotBuildings, d.buildings);
    if (Pilot::player >= 0 && Pilot::player < static_cast<int64_t>(state.players.size())) {
        Player& side = state.players[static_cast<size_t>(Pilot::player)];
        std::swap(side.pilot, d.levels);
        std::swap(side.ore, d.ore);
        std::swap(side.hydrogen, d.hydrogen);
    }
}

std::pair<int64_t, int64_t> Simulation::bankFor(int64_t unit) const {
    const Player& side = state.players[static_cast<size_t>(Pilot::player)];
    const std::optional<size_t> k = (pilot && pilot->unit == unit) ? std::nullopt : driverIndex(unit);
    // `swapDriver` trades the side's bank with the driver's when the side is there.
    if (k && Pilot::player >= 0 && Pilot::player < static_cast<int64_t>(state.players.size()))
        return {drivers[*k].ore, drivers[*k].hydrogen};
    return {side.ore, side.hydrogen};
}

void Simulation::setBankFor(int64_t unit, int64_t ore, int64_t hydrogen) {
    Player& side = state.players[static_cast<size_t>(Pilot::player)];
    const std::optional<size_t> k = (pilot && pilot->unit == unit) ? std::nullopt : driverIndex(unit);
    if (k && Pilot::player >= 0 && Pilot::player < static_cast<int64_t>(state.players.size())) {
        drivers[*k].ore = ore;
        drivers[*k].hydrogen = hydrogen;
        return;
    }
    side.ore = ore;
    side.hydrogen = hydrogen;
}

bool Simulation::drive(int64_t unit, const std::optional<PilotRecord>& levels) {
    if (piloted(unit)) return false;
    if (!pilot) {
        if (Pilot::player >= 0 && Pilot::player < static_cast<int64_t>(state.players.size()))
            state.players[static_cast<size_t>(Pilot::player)].pilot = levels;
        return take(unit);
    }
    // One more: a fresh context (its own levels) swapped in, taken there,
    // swapped back out.
    drivers.push_back(Driver{});
    drivers.back().levels = levels;
    const size_t k = drivers.size() - 1;
    // The empty driver's `pilot` is a placeholder: swapped in as none.
    swapDriver(k);
    pilot = std::nullopt;
    const bool ok = take(unit);
    if (ok) {
        swapDriver(k);
    } else {
        // Back as it was: the active one returns, the empty one goes.
        swapDriver(k);
        drivers.pop_back();
    }
    return ok;
}

void Simulation::handBack(int64_t unit) {
    if (pilot && pilot->unit == unit) {
        take(std::nullopt);
    } else if (const auto k = driverIndex(unit)) {
        swapDriver(*k);
        take(std::nullopt);
        swapDriver(*k);
        drivers.erase(drivers.begin() + static_cast<long>(*k));
    }
    // No active driver (handed back, or its unit gone): the last other
    // driver becomes the active one, its context with it.
    if (!pilot && !drivers.empty()) {
        const size_t k = drivers.size() - 1;
        swapDriver(k);
        drivers.pop_back();
    }
}

Pilot* Simulation::pilotFor(int64_t unit) {
    if (pilot && pilot->unit == unit) return &*pilot;
    if (const auto k = driverIndex(unit)) return &drivers[*k].pilot;
    return nullptr;
}

std::optional<ProspectorProgress>* Simulation::progressFor(int64_t unit) {
    if (pilot && pilot->unit == unit) return &progress;
    if (const auto k = driverIndex(unit)) return &drivers[*k].progress;
    return nullptr;
}

std::optional<PilotRecord>* Simulation::levelsFor(int64_t unit) {
    if (pilot && pilot->unit == unit) {
        if (Pilot::player < 0 || Pilot::player >= static_cast<int64_t>(state.players.size())) return nullptr;
        return &state.players[static_cast<size_t>(Pilot::player)].pilot;
    }
    if (const auto k = driverIndex(unit)) return &drivers[*k].levels;
    return nullptr;
}

std::set<int64_t>* Simulation::buildingsFor(int64_t unit) {
    if (pilot && pilot->unit == unit) return &pilotBuildings;
    if (const auto k = driverIndex(unit)) return &drivers[*k].buildings;
    return nullptr;
}

void Simulation::setProgressFor(int64_t unit, const std::optional<ProspectorProgress>& p) {
    if (pilot && pilot->unit == unit) {
        setProgress(p);
        return;
    }
    if (const auto k = driverIndex(unit)) {
        swapDriver(*k);
        setProgress(p);
        swapDriver(*k);
    }
}

std::vector<int64_t> Simulation::drivenUnits() const {
    std::vector<int64_t> out;
    if (pilot) out.push_back(pilot->unit);
    for (const Driver& d : drivers) out.push_back(d.pilot.unit);
    return out;
}

/// The unit the player drives, if it is alive.
std::optional<Unit> Simulation::pilotUnit() const {
    if (!pilot) return std::nullopt;
    const auto i = unitIndex(pilot->unit);
    if (!i) return std::nullopt;
    return state.units[*i];
}

// MARK: - Every kind

/// One step of the driven unit: its upkeep and the ability key, then
/// busy work (it stands still for it), else it faces where the mouse
/// says and walks where the keys say; then the action key. Anyone
/// aboard rides along.
void Simulation::stepPiloted(Unit& u, double dt, std::vector<GameEvent>& events) {
    if (!pilot) return;
    const Pilot p = *pilot;
    // Swift's `defer { carry(u) }`: every way out below carries.
    struct Carry {
        Simulation& sim_;
        Unit& unit_;
        ~Carry() { sim_.carry(unit_); }
    } const carryOnExit{*this, u};
    u.goal = std::nullopt;
    u.waypoints = std::nullopt;
    if (u.cooldown && *u.cooldown > 0) u.cooldown = max(0.0, *u.cooldown - dt);
    if (u.railCooldown && *u.railCooldown > 0) u.railCooldown = max(0.0, *u.railCooldown - dt);
    // Only a heal sets it attacking, and only for this step.
    if (u.task == Unit::Task::attacking) {
        u.task = Unit::Task::idle;
        u.target = std::nullopt;
    }
    upkeep(u, dt);
    levelUpkeep(u, dt);
    if (p.ability) pilotUse(u, events);
    // The MMO skill tree's slot keys.
    if (p.slot > 0) useSlot(u, p.slot - 1, events);
    pilotPace = 0;
    if (pilotBusy(u, p, dt, events)) return;
    pilotFace(u, p, dt);
    pilotPays = true;
    pilotWalk(u, p.walk, dt);
    pilotPays = false;
    // Taking off: no shot on the way up either.
    if (u.jumpFrom) return;
    pilotArrive(u, events);
    // A tank anchoring or packing up cannot fire.
    if (!((p.act || p.hold) && settled(u))) return;
    const auto target = pilotTarget(u);
    if (!target) return pilotMiss(u, events);
    pilotAct(u, *target, p.act, dt, events);
}

/// Not between tank and anchored mode.
bool Simulation::settled(const Unit& u) const { return u.anchor ? (*u.anchor == 0 || *u.anchor >= 1) : true; }

/// A unit whose weapon reaches past arm's length (a Ranger's rifle):
/// it aims along its sight and fires with nothing in it, too.
bool Simulation::pilotShoots(const Unit& u) const {
    const UnitStats w = weapon(u);
    return (w.hitsGround || w.hitsAir) && w.range >= 1.5 && u.kind != Unit::Kind::prospector;
}

/// The trigger held with nothing under the sight: a gun still fires,
/// at its weapon's pace, into the distance. Not with an enemy inside its
/// minimum range on the sight: the shell would land on it.
void Simulation::pilotMiss(Unit& u, std::vector<GameEvent>& events) {
    // A Scorpion's sting is not wasted on the air.
    if (u.kind == Unit::Kind::scorpion) { u.lockTarget = std::nullopt; return; }
    if (!(pilotShoots(u) && u.cooldown.value_or(0) <= 0)) return;
    if (const auto s = pilotSight(u); s && s->distance < s->minRange) return;
    const UnitStats w = weapon(u);
    reload(u, w);
    // The round flies on until it meets something: along the sight, turned
    // inside the cone (the view casts it; `at` is only a ground point on
    // its way).
    const double c = cone(u, w, pilotPace);
    u.spread = u.spread.value_or(0) + w.coneFire;
    const double turn = 2 * pi * roll(u.id, 1), off = c * roll(u.id, 2);
    const Vec2 f(std::cos(u.look()), std::sin(u.look()));
    events.push_back(GameEvent::Missed{u.id, u.position + f * ac::max(Rules::sight, w.range), off * std::cos(turn),
                                       off * std::sin(turn)});
}

/// Walk in the unit's own frame; blocked, it slides along what blocks it
/// (`pilotSlide`; a jumper takes off at a cliff face instead). Wheels
/// and treads do not sidestep; wheels steer on an arc (C4); an anchored
/// Longbow creeps (`Pilot.pace`). A vehicle climbs no steeper than its
/// gear lets it (`climbRefused`).
void Simulation::pilotWalk(Unit& u, Vec2 walk, double dt) {
    const double pace = Pilot::pace(u);
    if (!(pace > 0)) return;
    Vec2 w = Pilot::strafes(u.kind) ? walk : Vec2(walk.x, 0);
    const double len = length(w);
    if (!(len > 0.01)) return;
    if (len > 1) w /= len;
    const double h = u.heading;
    const Vec2 forward(std::cos(h), std::sin(h));
    const Vec2 right(-std::sin(h), std::cos(h));
    // Backing up and sidestepping are slower than going ahead.
    const double ahead_ = w.x < 0 ? Pilot::backPace : 1.0;
    const bool slowed = u.slowUntil.value_or(-1) > state.time;
    // C4: a hauler drives at its own pace, hero or not.
    const double boosted = u.stats().hauler ? 1.0 : rate(Stat::Speed{}, u);
    const double speed = Rules::speed(u.kind) * boosted * pace * (slowed ? u.slowedTo.value_or(0.5) : 1) * dt;
    Vec2 dir = forward * (w.x * ahead_) + right * (w.y * Pilot::sidePace);
    // On wheels (C4): A/D steer, and the step goes along the arc they set,
    // its chord turned by half the arc's turn (`turnRadius` at full lock).
    const double curve = Pilot::wheeled(u.kind) ? max(-1.0, min(1.0, walk.y)) / Pilot::turnRadius(u.kind) : 0.0;
    const double roll = w.x * ahead_ * speed, half = 0.5 * roll * curve;
    const Vec2 chord(std::cos(h + half), std::sin(h + half));
    if (curve != 0) dir = chord * (w.x * ahead_ * (std::abs(half) > 1e-9 ? std::sin(half) / half : 1.0));
    const Vec2 d = dir * speed;
    if (pilotJumps(u) && pilotJump(u, d)) return;
    const Vec2 from = u.position;
    // Too steep a climb for its gear: refused, it stands (wheels and tracks
    // do not crab sideways up a hill; turned across the slope, it goes).
    if (climbRefused(u, from, from + d)) pilotSteep = state.time;
    else if (pilotCanStand(u, from + d)) u.position += d;
    else if (const std::optional<Vec2> step = pilotSlide(u, d)) u.position += *step;
    u.stride += distance(from, u.position);
    // The hull turns by how far it rolled along the arc: blocked, not at all.
    if (curve != 0) u.heading = std::remainder(h + dot(u.position - from, chord) * curve, 2 * pi);
    // How fast it went, for its gun's cone.
    const double full = Rules::speed(u.kind) * dt;
    pilotPace = full > 0 ? distance(from, u.position) / full : 0;
}

bool Simulation::climbRefused(const Unit& u, Vec2 from, Vec2 to) const {
    const std::optional<double> limit = Rules::climb(u.kind);
    if (!limit || !field) return false;
    const Vec2 d = to - from;
    const double len = length(d);
    if (!(len > 1e-9)) return false;
    return groundPitch(u, to, d / len) > std::tan(*limit);
}

double Simulation::groundPitch(const Unit& u, Vec2 p, Vec2 dir) const {
    if (!field) return 0;
    const double half = 0.5 * max(u.stats().wheelbase, 2 * Rules::radius(u.kind));
    return (field->height(p + dir * half) - field->height(p - dir * half)) / (2 * half);
}

/// The full step `d` is blocked: a step along what blocks it instead
/// (docs/shooter-feel.md "Third person (2026-10-18)"). In order: the step
/// projected on the blocker's tangent at the contact (`pilotContact`),
/// turned the way the step leans; the world axes' slides; the step turned
/// 25° and 50° to either side, the side away from the blocker first. Never
/// a step back against the one wanted, and only onto ground it can stand
/// on: in a concave corner it stands. Nil: no way.
std::optional<Vec2> Simulation::pilotSlide(const Unit& u, Vec2 d) const {
    const Vec2 from = u.position;
    auto open = [&](Vec2 s) {
        return length(s) > 1e-9 && dot(s, d) >= 0 && !climbRefused(u, from, from + s) && pilotCanStand(u, from + s);
    };
    const std::optional<Vec2> n = pilotContact(u, from, from + d);
    if (n) {
        Vec2 t(-n->y, n->x);
        if (dot(t, d) < 0) t = -t;
        if (const Vec2 s = t * dot(d, t); open(s)) return s;
    }
    for (const Vec2 s : {Vec2(d.x, 0), Vec2(0, d.y)}) {
        if (open(s)) return s;
    }
    for (const double a : {25.0, 50.0}) {
        const double ca = std::cos(a * pi / 180), sa = std::sin(a * pi / 180);
        const Vec2 l(d.x * ca - d.y * sa, d.x * sa + d.y * ca), rr(d.x * ca + d.y * sa, -d.x * sa + d.y * ca);
        // Away from the blocker first (with no contact known, the left).
        const bool leftFirst = !n || dot(l, *n) >= dot(rr, *n);
        for (const Vec2 s : {leftFirst ? l : rr, leftFirst ? rr : l}) {
            if (open(s)) return s;
        }
    }
    return std::nullopt;
}

/// What keeps the unit out of `to`, seen from `from`: the outward normal of
/// the blocker at the point of it nearest `from` (a rock's circle, a
/// building's slab or furniture, an ore deposit's box). For ground the grid
/// leaves out (a cliff, a well, a region's wall or edge), the way the open
/// ground round `from` lies. Nil: nothing blocks, or no way is open.
std::optional<Vec2> Simulation::pilotContact(const Unit& u, Vec2 from, Vec2 to) const {
    auto away = [&](Vec2 near) -> std::optional<Vec2> {
        const Vec2 n = from - near;
        const double len = length(n);
        return len > 1e-9 ? std::optional<Vec2>(n / len) : std::nullopt;
    };
    const double r = Rules::radius(u.kind) * 0.8;
    if (!u.stats().air) {
        if (const auto wall = shuttleContact(u, from, to, r)) return away(*wall);
        if (nav && !swallowedByCitadel(to)) {
            if (const auto rock = nav->rockAt(to, NavGrid::clearance)) return away(rock->first);
        }
        for (const Structure& s : state.structures) {
            if (Rules::walkable(s.kind) || !NavGrid::insideStructure(to, s, r)) continue;
            const Footprint& f = Rules::footprint(s.kind);
            const Vec2 at = to - s.position, seen = from - s.position;
            auto nearestOf = [&](const Slab& w) {
                return s.position + Vec2(std::max(w.min.x, std::min(w.max.x, seen.x)), std::max(w.min.y, std::min(w.max.y, seen.y)));
            };
            if (!f.interior || !s.complete()) return away(nearestOf(Slab{{-f.radius, -f.radius}, {f.radius, f.radius}}));
            for (const Slab& w : f.walls) {
                if (w.z0 <= 0 && w.contains(at, r)) return away(nearestOf(w));
            }
            if (f.door && !doorOpen(s) && f.door->contains(at, r)) return away(nearestOf(*f.door));
            for (const Block& b : f.furniture) {
                if (b.z0 <= 0.1 && b.contains(at, r)) return away(s.position + b.nearest(seen));
            }
        }
        for (const OreDeposit& m : state.patches) {
            if (!(m.remaining > 0 && NavGrid::insidePatch(to, m, r - 0.1))) continue;
            const Block box{m.position, Vec2(1, 0.5), -m.angle};
            return away(box.nearest(from));
        }
    }
    // The grid's ground (or the map's bounds for a flyer): the open
    // directions round `from`, summed.
    Vec2 sum(0, 0);
    for (int k = 0; k < 16; k++) {
        const Vec2 dir(std::cos(k * pi / 8), std::sin(k * pi / 8));
        if (pilotCanStand(u, from + dir * 0.3)) sum += dir;
    }
    const double len = length(sum);
    return len > 1e-6 ? std::optional<Vec2>(sum / len) : std::nullopt;
}

/// Nothing solid at `p` for the unit: open ground, clear of buildings
/// and live ore deposits (a flyer goes anywhere on the map).
bool Simulation::pilotCanStand(const Unit& u, Vec2 p) const {
    if (u.stats().air) {
        if (!map) return true;
        const GroundRect b = map->bounds;
        return p.x > b.minX && p.x < b.maxX && p.y > b.minZ && p.y < b.maxZ;
    }
    const double r = Rules::radius(u.kind) * 0.8;
    // A Dropship's walls, its shut ramp and what stands in its bay (C4);
    // on its deck or its ramp the ground under it does not matter.
    if (!state.shuttles.empty()) {
        const ShuttleWalk walk = shuttleWalk(u, p, r);
        if (walk == ShuttleWalk::blocked) return false;
        if (walk == ShuttleWalk::deck) return true;
    }
    // Past a region's open edge (W4): it stands on the ground just inside.
    if (map && map->edges) {
        const std::optional<Vec2> in = map->edges->standIn(map->bounds, p);
        if (!in) return false;
        p = *in;
    }
    // A map doodad a complete Citadel swallowed (a saved world's Citadel
    // grown over its rocks, C1) is not there: the game hides it.
    // The map's rocks are their circles grown by the grid's clearance, not
    // the grid's cells round them (a staircase up to 0.35 cells deeper,
    // which caught a walker sliding along a rock): their cells are
    // `rocky`, and the circles decide.
    if (nav) {
        const bool swallowed = swallowedByCitadel(p);
        if (!nav->standable(p) && !nav->rockyCell(p) && !(swallowed && swallowedDoodadCell(p))) return false;
        if (!swallowed && nav->rockAt(p, NavGrid::clearance)) return false;
    }
    // A complete Citadel with the hall blocks by its walls and its shut door
    // (C1): the pilot walks in through the open door. Anything else, its
    // whole square.
    if (std::any_of(state.structures.begin(), state.structures.end(), [&](const Structure& s) {
            if (Rules::walkable(s.kind) || !NavGrid::insideStructure(p, s, r)) return false;
            const Footprint& f = Rules::footprint(s.kind);
            if (!f.interior || !s.complete()) return true;
            return f.blocks(p - s.position, doorOpen(s), r) || f.furnished(p - s.position, r);
        })) {
        return false;
    }
    if (std::any_of(state.patches.begin(), state.patches.end(), [&](const OreDeposit& m) {
            return m.remaining > 0 && NavGrid::insidePatch(p, m, r - 0.1);
        })) {
        return false;
    }
    return true;
}

/// Something the unit is busy with that holds it in place (mid-jump,
/// a Prospector drilling, inside a Derrick, welding or mending). True while
/// it is.
bool Simulation::pilotBusy(Unit& u, const Pilot& p, double dt, std::vector<GameEvent>& events) {
    // At a corner gun (C1): it stands at the station, its gun aims and fires
    // (`stepCornerGun`); the walk keys do nothing.
    if (u.task == Unit::Task::atGun) {
        if (mannedGun(u)) return true;
        u.task = Unit::Task::idle;
        u.structure = std::nullopt;
    }
    if (u.jumpFrom) {
        pilotFace(u, p, dt);
        pilotFly(u, dt);
        return true;
    }
    switch (u.kind) {
    case Unit::Kind::prospector: return prospectorBusy(u, p, dt, events);
    default: return false;
    }
}

/// Whatever happens by walking somewhere (a Prospector drops its load at a
/// Citadel).
void Simulation::pilotArrive(Unit& u, std::vector<GameEvent>& events) {
    if (u.kind == Unit::Kind::prospector) prospectorDeposit(u, events);
}

/// `p` lies ahead of `u`, within about 70° of where it faces.
bool Simulation::ahead(const Unit& u, Vec2 p) const {
    const Vec2 d = p - u.position;
    const double len = length(d);
    return len < 1e-6 || dot(d / len, Vec2(std::cos(u.look()), std::sin(u.look()))) > 0.35;
}

/// The enemy the unit would strike: the nearest ahead, on the ground,
/// within its weapon's reach (and a little). A gun takes the first one
/// on its line of sight instead (`pilotAimed`).
std::optional<int64_t> Simulation::pilotFoe(const Unit& u) const {
    const UnitStats w = weapon(u);
    if (!(w.hitsGround || w.hitsAir)) return std::nullopt;
    if (pilotShoots(u)) return pilotAimed(u);
    const double reach = w.range + 0.25;
    std::optional<std::pair<int64_t, double>> best;
    for (const Unit& v : state.units) {
        if (!(state.hostile(v.owner, u.owner) && v.hp > 0)) continue;
        const auto t = foe(v.id, u.owner);
        if (!t || !canHit(u, *t) || !ahead(u, t->position)) continue;
        const double g = gap(u, *t);
        if (g <= reach && (best ? g < best->second : true)) best = std::make_pair(v.id, g);
    }
    for (const Structure& s : state.structures) {
        // An enemy's, or a Target Dummy standing (C2).
        if (!targetable(s, u.owner)) continue;
        const auto t = foe(s.id, u.owner);
        if (!t || !w.hitsGround) continue;
        const double g = max(std::abs(u.position.x - s.position.x), std::abs(u.position.y - s.position.y))
                         - t->radius - Rules::radius(u.kind);
        if (!(g <= reach && (ahead(u, s.position) || g < 0.3))) continue;
        if (best ? g < best->second : true) best = std::make_pair(s.id, g);
    }
    if (!best) return std::nullopt;
    return best->first;
}

/// The first enemy a gun's line of fire meets that a round would reach (any
/// distance, for rounds that fly on), if the first one on the line is.
std::optional<int64_t> Simulation::pilotAimed(const Unit& u) const {
    const auto s = pilotSight(u);
    if (!s || !s->inRange) return std::nullopt;
    return s->target;
}

/// The first enemy on the driven gun's line of fire (where it looks),
/// out to sight range: a unit or building whose edge comes within
/// `pilotAimSlack` of the line. `distance` is edge to edge; `inRange`
/// is true if and only if a shot now would reach it (not inside a
/// anchored tank's minimum range). A Dropship gets its patient instead
/// (`healSight`).
std::optional<PilotSight> Simulation::pilotSight(const Unit& u) const {
    if (u.kind == Unit::Kind::dropship) return healSight(u);
    if (Pilot::strafes(u.kind) && pilot && pilot->unit == u.id && pilot->crosshair) {
        return crosshairSight(u, *pilot->crosshair);
    }
    const UnitStats w = weapon(u);
    if (!(w.hitsGround || w.hitsAir)) return std::nullopt;
    std::optional<PilotSight> best;
    const auto consider = [&](int64_t id, const Target& t, double r) {
        const auto g = sightGap(u, t.position, r);
        if (!g) return;
        const UnitStats x = weapon(u, t.air);
        const double reach = range(x.range, u.position, t.position);
        if (best ? *g < best->distance : true) {
            best = PilotSight{.target = id, .distance = *g, .range = reach, .minRange = x.minRange,
                              .inRange = *g >= x.minRange && (x.falloff > 0 || *g <= reach + 0.25),
                              .share = falloffShare(x, *g, reach)};
        }
    };
    for (const Unit& v : state.units) {
        if (!(state.hostile(v.owner, u.owner) && v.hp > 0)) continue;
        const auto t = foe(v.id, u.owner);
        if (!t || !canHit(u, *t)) continue;
        consider(v.id, *t, t->radius);
    }
    for (const Structure& s : state.structures) {
        if (!(targetable(s, u.owner) && w.hitsGround)) continue;
        const auto t = foe(s.id, u.owner);
        if (!t) continue;
        consider(s.id, *t, t->radius);
    }
    return best;
}

/// A gun on foot's sight: the enemy the crosshair is on, if it can hit
/// it (at any distance, if its rounds fly on; else within sight); nothing
/// for a friend, a rock or the sky.
std::optional<PilotSight> Simulation::crosshairSight(const Unit& u, const Pilot::Crosshair& c) const {
    const auto on = c.as<Pilot::Crosshair::On>();
    if (!on) return std::nullopt;
    const int64_t id = on->_0;
    const auto t = foe(id, u.owner);
    if (!t || !canHit(u, *t)) return std::nullopt;
    // An enemy, or a Target Dummy standing (C2).
    if (t->structure) {
        const auto s = state.structure(id);
        if (!s || !targetable(*s, u.owner)) return std::nullopt;
    } else if (!state.hostile(t->owner, u.owner)) {
        return std::nullopt;
    }
    const UnitStats w = weapon(u, t->air);
    const double g = max(0.0, gap(u, *t));
    // A round that flies on reaches whatever the view shows; a flame or a
    // sting only what is near.
    if (!(w.falloff > 0 || g <= max(Rules::sight, w.range + 1))) return std::nullopt;
    const double reach = range(w.range, u.position, t->position);
    return PilotSight{.target = id, .distance = g, .range = reach, .minRange = w.minRange,
                      .inRange = g >= w.minRange && (w.falloff > 0 || g <= reach + 0.25),
                      .share = falloffShare(w, g, reach)};
}

/// Edge to edge, how far along `u`'s line of sight it meets something of
/// radius `r` at `p`: nil when the line passes it by, it is behind, or
/// it is out of sight range.
std::optional<double> Simulation::sightGap(const Unit& u, Vec2 p, double r) const {
    const Vec2 dir(std::cos(u.look()), std::sin(u.look()));
    const Vec2 rel = p - u.position;
    const double along = dot(rel, dir);
    const double off = length(rel - dir * along);
    if (!(along > 0 && off <= r + pilotAimSlack)) return std::nullopt;
    // Its near edge, where the line enters it.
    const double enter = along - std::sqrt(max(r * r - off * off, 0.0));
    const double g = max(0.0, enter - Rules::radius(u.kind));
    // It sees at least as far as its gun reaches (an anchored Longbow's 13).
    if (g <= max(Rules::sight, weapon(u).range + 1)) return g;
    return std::nullopt;
}

/// What one attack of `u` would take off target `id` now.
std::optional<double> Simulation::strikeDamage(const Unit& u, int64_t id) const {
    const auto t = target(id);
    if (!t) return std::nullopt;
    return damage(u, *t);
}

/// What the action key would do for `u` where it stands and faces.
std::optional<PilotTarget> Simulation::pilotTarget(const Unit& u) const {
    if (mending(u)) {
        if (u.task == Unit::Task::mining || u.task == Unit::Task::inDerrick || u.task == Unit::Task::atGun) return std::nullopt;
        return pilotRepair(u);
    }
    if (const auto f = pilotFoe(u)) return PilotTarget(PilotTarget::Enemy{*f});
    switch (u.kind) {
    case Unit::Kind::prospector: return prospectorTarget(u);
    case Unit::Kind::dropship: return healTarget(u);
    default: return std::nullopt;
    }
}

void Simulation::pilotAct(Unit& u, const PilotTarget& target_, bool pressed, double dt,
                          std::vector<GameEvent>& events) {
    if (const auto e = target_.as<PilotTarget::Enemy>()) {
        const int64_t id = e->_0;
        // No turning to it: the unit faces where the mouse says.
        const auto t = target(id);
        if (!t || !(cooldownFor(u, t->air) <= 0)) return;
        // A Scorpion stings only buried, and after a second's lock-on: the
        // fire button is held that long on the target.
        if (u.kind == Unit::Kind::scorpion && !(u.burrowed() && lockOn(u, id, dt))) return;
        const UnitStats w = weapon(u, t->air);
        reload(u, w);
        if (u.kind == Unit::Kind::scorpion) u.lockTarget = std::nullopt;
        const Aimed r = aimRound(u, w, *t, pilotPace);
        if (r.hit) {
            strike(u, id, *t, r.share);
            events.push_back(GameEvent::Shot{u.id, id, t->owner, t->position});
        } else {
            events.push_back(GameEvent::Missed{u.id, r.at, r.yaw, r.pitch});
        }
    } else if (const auto pa = target_.as<PilotTarget::Patch>()) {
        prospectorMine(u, pa->_0);
    } else if (const auto we = target_.as<PilotTarget::Well>()) {
        fieldPumpStart(u, we->_0);
    } else if (const auto de = target_.as<PilotTarget::Derrick>()) {
        if (pressed && !de->busy) prospectorEnter(u, de->_0);
    } else if (const auto sc = target_.as<PilotTarget::Scaffold>()) {
        if (pressed) prospectorResume(u, sc->_0);
    } else if (const auto re = target_.as<PilotTarget::Repair>()) {
        u.structure = re->_0;
        u.task = Unit::Task::repairing;
    } else if (target_.is<PilotTarget::Full>()) {
    } else if (const auto he = target_.as<PilotTarget::Heal>()) {
        const int64_t id = he->_0;
        if (!(pilot && pilot->hold)) return;
        heal(u, id, dt);
        // Double beam: the most hurt other patient in reach as well.
        const bool doubleBeam =
            effect(u, [](const Effect& x) -> std::optional<bool> {
                if (x.is<Effect::DoubleBeam>()) return true;
                return std::nullopt;
            }) == true;
        if (doubleBeam) {
            if (const auto other = secondPatient(u, id)) {
                heal(u, *other, dt);
                u.target = id;
            }
        }
    }
}

// MARK: - Prospector

std::optional<PilotTarget> Simulation::prospectorTarget(const Unit& u) const {
    if (u.task == Unit::Task::mining || u.task == Unit::Task::inDerrick || u.task == Unit::Task::atGun) return std::nullopt;
    // Its own finished Derrick, on a well with MH left.
    std::optional<Structure> derrick;
    for (const Structure& s : state.structures) {
        if (s.kind == Structure::Kind::derrick && s.owner == u.owner && s.complete()
            && NavGrid::insideStructure(u.position, s, pilotHydrogenReach) && ahead(u, s.position)) {
            const auto wi = wellIndex(s);
            if (wi && (*state.wells)[*wi].remaining > 0) {
                derrick = s;
                break;
            }
        }
    }
    std::optional<int64_t> patch;
    for (int64_t i = 0; i < static_cast<int64_t>(state.patches.size()); ++i) {
        const OreDeposit& p = state.patches[i];
        if (!(p.remaining > 0 && NavGrid::insidePatch(u.position, p, pilotReach) && ahead(u, p.position))) continue;
        if (!patch || distance(p.position, u.position) < distance(state.patches[*patch].position, u.position)) {
            patch = i;
        }
    }
    // Field pump: a bare well, when no Derrick or deposit is in reach.
    const auto bare = derrick || patch ? std::nullopt : fieldWell(u);
    if (derrick || patch || bare) {
        if (u.carrying > 0) return PilotTarget(PilotTarget::Full{});
        if (bare) return PilotTarget(PilotTarget::Well{*bare});
        if (derrick) {
            if (const auto gi = wellIndex(*derrick)) {
                const auto inside = (*state.wells)[*gi].harvester;
                return PilotTarget(PilotTarget::Derrick{derrick->id, inside.has_value() && inside != u.id});
            }
        }
        if (!patch) return std::nullopt;
        return PilotTarget(PilotTarget::Patch{*patch});
    }
    if (const auto v = hurtMule(u)) return PilotTarget(PilotTarget::Repair{*v});
    if (const auto v = hurtMachine(u)) return PilotTarget(PilotTarget::Repair{*v});
    // Its own buildings: pick up an abandoned one, or mend a hurt one.
    const Structure* own = nullptr;
    for (const Structure& s : state.structures) {
        if (!(s.owner == u.owner && NavGrid::insideStructure(u.position, s, workReach(u)) && ahead(u, s.position))) {
            continue;
        }
        if (!own || distance(s.position, u.position) < distance(own->position, u.position)) own = &s;
    }
    if (!own || (own->kind == Structure::Kind::lab && own->parent)) return std::nullopt;
    const Structure& s = *own;
    if (!s.complete()) {
        bool welded = false;
        if (s.builder) {
            if (const auto b = unitIndex(*s.builder)) {
                const Unit& w = state.units[*b];
                welded = w.task == Unit::Task::building && w.structure == s.id;
            }
        }
        if (welded) return std::nullopt;
        return PilotTarget(PilotTarget::Scaffold{s.id});
    }
    if (s.hp < fullHP(s)) return PilotTarget(PilotTarget::Repair{s.id});
    return std::nullopt;
}

/// The driven Prospector with R held (`Pilot.mend`): it only repairs.
bool Simulation::mending(const Unit& u) const {
    return u.kind == Unit::Kind::prospector && pilot && pilot->mend && piloted(u.id);
}

/// What the driven Prospector would repair with R held: a hurt machine
/// (Field welder), else the nearest damaged finished building of its
/// own, close and ahead. Nil: nothing to repair there.
std::optional<PilotTarget> Simulation::pilotRepair(const Unit& u) const {
    if (u.kind != Unit::Kind::prospector) return std::nullopt;
    if (const auto v = hurtMule(u)) return PilotTarget(PilotTarget::Repair{*v});
    if (const auto v = hurtMachine(u)) return PilotTarget(PilotTarget::Repair{*v});
    const Structure* best = nullptr;
    for (const Structure& s : state.structures) {
        if (!(s.owner == u.owner && s.complete() && !(s.kind == Structure::Kind::lab && s.parent) && s.hp < fullHP(s)
              && NavGrid::insideStructure(u.position, s, workReach(u)) && ahead(u, s.position))) {
            continue;
        }
        if (!best || distance(s.position, u.position) < distance(best->position, u.position)) best = &s;
    }
    if (!best) return std::nullopt;
    return PilotTarget(PilotTarget::Repair{best->id});
}

/// Field welder: a hurt machine of its side, close and ahead.
std::optional<int64_t> Simulation::hurtMachine(const Unit& u) const {
    if (!machineRepair(u)) return std::nullopt;
    const Unit* best = nullptr;
    for (const Unit& v : state.units) {
        if (!(v.owner == u.owner && v.id != u.id && v.hp > 0 && v.hp < maxHP(v) && Leveling::machines.contains(v.kind)
              && canDrive(v) && v.task != Unit::Task::inDerrick
              && gap(u, Target{.position = v.position, .radius = Rules::radius(v.kind), .structure = false,
                               .owner = v.owner}) <= pilotHydrogenReach
              && ahead(u, v.position))) {
            continue;
        }
        if (!best || distance(v.position, u.position) < distance(best->position, u.position)) best = &v;
    }
    if (!best) return std::nullopt;
    return best->id;
}

std::optional<int64_t> Simulation::wellIndex(const Structure& s) const {
    if (!state.wells) return std::nullopt;
    for (int64_t i = 0; i < static_cast<int64_t>(state.wells->size()); ++i) {
        if (distance((*state.wells)[i].position, s.position) < 0.5) return i;
    }
    return std::nullopt;
}

/// Drilling (while the key is held), inside a Derrick, welding a
/// building (until the Prospector walks off) or mending one (while held).
/// Walking off a patch, or letting go of the key, loses the drilling.
bool Simulation::prospectorBusy(Unit& u, const Pilot& p, double dt, std::vector<GameEvent>& events) {
    const bool walking = length(p.walk) > 0.1;
    if (fieldPumpBusy(u, p, dt)) return true;
    switch (u.task) {
    case Unit::Task::inDerrick: {
        const auto r = u.structure ? state.structure(*u.structure) : std::nullopt;
        const auto gi = r ? wellIndex(*r) : std::nullopt;
        if (!gi) {
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        // Quickfill: this pump is instant.
        if (takeQuickfill(u)) u.timer = 0;
        else u.timer -= dt * rate(Stat::Hydrogen{}, u);
        if (u.timer <= 0) {
            Well& g = (*state.wells)[*gi];
            pumpLoad(u, g);
            g.harvester = std::nullopt;
            u.task = Unit::Task::idle;
        }
        return true;
    }
    case Unit::Task::mining: {
        if (!(u.patch && state.patches[*u.patch].remaining > 0)) {
            u.task = Unit::Task::idle;
            return false;
        }
        const int64_t pi_ = *u.patch;
        if (walking || !p.hold) {
            leavePatch(u);
            return false;
        }
        turn(u, state.patches[pi_].position, dt);
        u.timer -= dt * rate(Stat::Drill{}, u);
        if (u.timer <= 0) {
            // Rich vein: every third trip carries double (and the skills).
            takeOre(u, pi_, events);
            state.patches[pi_].miner = std::nullopt;
            u.task = Unit::Task::idle;
        }
        return true;
    }
    case Unit::Task::building: {
        std::optional<size_t> i;
        if (u.structure) {
            for (size_t k = 0; k < state.structures.size(); ++k) {
                if (state.structures[k].id == *u.structure) {
                    i = k;
                    break;
                }
            }
        }
        if (!i || state.structures[*i].complete()) {
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        if (walking) {
            // Walked off: the building waits, half-built, for any Prospector.
            if (state.structures[*i].builder == u.id) state.structures[*i].builder = std::nullopt;
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        turn(u, state.structures[*i].position, dt);
        return true;
    }
    case Unit::Task::repairing: {
        // C4: a Mule is mended as a building is, paid for.
        if (u.structure)
            if (const auto m = unitIndex(*u.structure); m && state.units[size_t(*m)].kind == Unit::Kind::mule)
                return prospectorMendMule(u, *u.structure, p, dt, events);
        if (u.structure && unitIndex(*u.structure)) return prospectorMend(u, *u.structure, p, dt);
        std::optional<size_t> i;
        if (u.structure) {
            for (size_t k = 0; k < state.structures.size(); ++k) {
                if (state.structures[k].id == *u.structure) {
                    i = k;
                    break;
                }
            }
        }
        if (!(i && state.structures[*i].hp < fullHP(state.structures[*i]) && p.hold && !walking)) {
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        const Structure s = state.structures[*i];
        turn(u, s.position, dt);
        // It costs materials (`payRepair`): with nothing in the bank for
        // the next unit, the repair stops.
        if (repairRefusal(u, s)) {
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        // A full repair takes as long as building it; the driven
        // Prospector mends `Rules.heroWork` times as fast (or as its picks say).
        const double full = fullHP(s);
        state.structures[*i].hp = min(full, s.hp + full / Rules::buildTime(s.kind) * rate(Stat::Repair{}, u) * dt);
        const double mended = state.structures[*i].hp - s.hp;
        if (!payRepair(u, *i, mended, full, events)) {
            state.structures[*i].hp = s.hp;
            u.task = Unit::Task::idle;
            u.structure = std::nullopt;
            return false;
        }
        earn(Leveling::worth(s.kind) * mended / full * Leveling::mendXP, u);
        tally(u, [&](Tally& t) { t.repaired += mended; });
        return true;
    }
    default:
        u.task = Unit::Task::idle;
        return false;
    }
}

/// Field welder: the driven Prospector mends machine `id` while the key is
/// held and it stays close; a full repair takes the pick's share of the
/// machine's training time.
bool Simulation::prospectorMend(Unit& u, int64_t id, const Pilot& p, double dt) {
    const auto share = machineRepair(u);
    const auto j = unitIndex(id);
    if (!(share && j && p.hold && length(p.walk) <= 0.1)) {
        u.task = Unit::Task::idle;
        u.structure = std::nullopt;
        return false;
    }
    const Unit v = state.units[*j];
    const double full = maxHP(v);
    const double reach =
        gap(u, Target{.position = v.position, .radius = Rules::radius(v.kind), .structure = false, .owner = v.owner});
    if (!(v.hp > 0 && v.hp < full && reach <= pilotHydrogenReach + 0.3 && canDrive(v))) {
        u.task = Unit::Task::idle;
        u.structure = std::nullopt;
        return false;
    }
    turn(u, v.position, dt);
    const double mended = min(full - v.hp, full / (Rules::trainTime(v.kind) * *share) * dt);
    state.units[*j].hp += mended;
    earn(Leveling::worth(v.kind) * mended / full * Leveling::mendXP, u);
    tally(u, [&](Tally& t) { t.repaired += mended; });
    return true;
}

/// A load is dropped off by walking up to one's own Citadel (R1: every
/// pilot uses every building, whoever built it). C4: a Hab Dome takes no
/// loads in a world any more: the Mule hauls them to a Citadel.
void Simulation::prospectorDeposit(Unit& u, std::vector<GameEvent>& events) {
    if (!(u.carrying > 0)) return;
    std::optional<Structure> citadel;
    for (const Structure& b : bases(u.owner)) {
        if (NavGrid::insideStructure(u.position, b, pilotDepositReach)) {
            citadel = b;
            break;
        }
    }
    if (!citadel) return;
    const bool hydrogen = u.hydrogen == true;
    // Stardust heart, Silver tide: the load counts more, in the wallet and XP.
    const int64_t load_ = unloaded(u, u.carrying, hydrogen);
    earn(static_cast<double>(load_) * (hydrogen ? Leveling::hydrogenXP : Leveling::oreXP), u);
    tally(u, [&](Tally& t) {
        if (hydrogen) {
            t.hydrogen += load_;
        } else {
            t.ore += load_;
        }
    });
    // The Citadel's tax (C1): in a world the realm takes its cut of the load
    // into the realm wallet (the server banks it from the event); XP and
    // contracts count the whole load.
    const int64_t tax = skilled(u) ? Progression::realmTax(load_) : 0;
    if (hydrogen) {
        state.players[u.owner].hydrogen += load_ - tax;
    } else {
        state.players[u.owner].ore += load_ - tax;
        state.players[u.owner].totalMined += load_;
    }
    events.push_back(GameEvent::Deposited{u.id, load_, citadel->position, tax, hydrogen});
    u.carrying = 0;
    u.hydrogen = std::nullopt;
}

void Simulation::prospectorMine(Unit& u, int64_t pi_) {
    // The player goes first: a Prospector drilling here moves along.
    if (const auto other = state.patches[pi_].miner; other && *other != u.id) {
        if (const auto k = unitIndex(*other)) {
            state.units[*k].task = Unit::Task::toPatch;
            state.units[*k].timer = 0;
        }
    }
    state.patches[pi_].miner = u.id;
    u.patch = pi_;
    u.structure = std::nullopt;
    u.task = Unit::Task::mining;
    u.timer = Rules::miningTime;
}

void Simulation::prospectorEnter(Unit& u, int64_t rid) {
    const auto r = state.structure(rid);
    if (!r) return;
    const auto gi = wellIndex(*r);
    if (!gi) return;
    (*state.wells)[*gi].harvester = u.id;
    u.structure = rid;
    u.patch = std::nullopt;
    u.task = Unit::Task::inDerrick;
    u.timer = Rules::hydrogenTime;
    // Derrick tender: what the Derrick filled by itself goes to the wallet.
    collectTank(u, rid);
}

void Simulation::prospectorResume(Unit& u, int64_t sid) {
    for (Structure& s : state.structures) {
        if (s.id != sid) continue;
        s.builder = u.id;
        u.structure = sid;
        u.task = Unit::Task::building;
        return;
    }
}

// MARK: - Building by hand

/// Why player `owner` cannot build `kind` now (money, supply of tech),
/// or nil.
std::optional<std::string> Simulation::buildRefusal(Structure::Kind kind, int64_t owner) const {
    const Player& p = state.players[owner];
    const Price price = buildCost(kind, owner);
    if (p.ore < price.ore) return "Not enough ore";
    if (p.hydrogen < price.hydrogen) return "Not enough MH";
    // A world's pilot builds from blueprints, not up the strategy game's
    // tech tree.
    const auto d = pilotUnit();
    const bool world = d && d->owner == owner && skilled(*d);
    if (const auto need = Rules::requires_(kind); need && !world) {
        const bool has_ = std::any_of(state.structures.begin(), state.structures.end(), [&](const Structure& s) {
            return s.kind == *need && s.complete() && s.owner == owner;
        });
        if (!has_) return "Needs a " + title(*need);
    }
    return std::nullopt;
}

// MARK: - Demolish (C3)

Price Simulation::salvage(StructureKind kind, bool world) {
    const double ore = double(world ? Progression::worldOre(kind) : Rules::cost(kind));
    const double hydrogen = double(world ? Progression::worldHydrogen(kind) : Rules::hydrogenCost(kind));
    return Price{static_cast<int64_t>(std::floor(ore * Rules::salvageShare + 1e-9)),
                 static_cast<int64_t>(std::floor(hydrogen * Rules::salvageShare + 1e-9))};
}

bool Simulation::demolishReach(const Unit& by, const Structure& s, double slack) const {
    return NavGrid::insideStructure(by.position, s, workReach(by) + slack);
}

std::optional<std::string> Simulation::demolishRefusal(const Structure& s) const {
    if (s.kind != StructureKind::citadel) return std::nullopt;
    // A home with someone in it stays: in its hall or doorway, or at a gun.
    if (s.guns)
        for (const auto& g : *s.guns)
            if (g.gunner) return std::string("Someone is at its guns");
    const Footprint& f = Rules::footprint(s.kind);
    for (const Unit& v : state.units) {
        if (!(v.hp > 0) || v.airborne()) continue;
        if (v.task == Unit::Task::atGun && v.structure == s.id) return std::string("Someone is at its guns");
        if (f.interior && NavGrid::insideStructure(v.position, s, 0) && !f.blocks(v.position - s.position, true, 0))
            return std::string("Someone is inside");
    }
    return std::nullopt;
}

bool Simulation::demolish(int64_t id, Price refund) {
    const auto it = std::find_if(state.structures.begin(), state.structures.end(), [&](const Structure& s) { return s.id == id; });
    if (it == state.structures.end()) return false;
    const Structure s = *it;
    state.structures.erase(it);
    leave(s);
    between.push_back(GameEvent::Destroyed{s.id, s.kind, s.owner, s.position});
    between.push_back(GameEvent::Demolished{s.id, refund.ore, refund.hydrogen});
    reindex();
    refreshSupply();
    refreshNav();
    return true;
}

/// Where a building of `kind` would stand if placed near `p`: on the
/// build grid (whole cells for odd sizes, cell corners for even), and a
/// Derrick on the well nearest `p` (within 4 cells).
std::optional<Vec2> Simulation::snap(Structure::Kind kind, Vec2 p) const {
    if (kind == Structure::Kind::derrick) {
        if (!state.wells) return std::nullopt;
        std::optional<Vec2> best;
        for (const Well& w : *state.wells) {
            if (!(distance(w.position, p) < 4)) continue;
            if (!best || distance(w.position, p) < distance(*best, p)) best = w.position;
        }
        return best;
    }
    const double r = Rules::radius(kind);
    const bool odd = static_cast<int64_t>(rounded(2 * r)) % 2 == 1;
    const auto g = [&](double v) { return odd ? rounded(v - 0.5) + 0.5 : rounded(v); };
    return Vec2(g(p.x), g(p.y));
}

/// Why a `kind` cannot stand at `p` (the ground, or something in the
/// way), or nil when it can.
std::optional<std::string> Simulation::placementRefusal(Structure::Kind kind, Vec2 p) const {
    const double r = Rules::radius(kind);
    if (kind == Structure::Kind::derrick) {
        const bool onWell = state.wells && std::any_of(state.wells->begin(), state.wells->end(), [&](const Well& w) {
            return distance(w.position, p) < 0.5 && w.remaining > 0;
        });
        if (!onWell) return "Must go on an MH well";
        if (std::any_of(state.structures.begin(), state.structures.end(),
                        [&](const Structure& s) { return distance(s.position, p) < 0.5; })) {
            return "The well is taken";
        }
        return std::nullopt;
    }
    // Flat, open ground under the whole footprint.
    // Swift's `stride(from:through:by:)` over Doubles: start + i × step,
    // up to and including the end.
    const double from = -r + 0.25, through = r - 0.25;
    auto footprint = [&](auto&& at) {
        for (int64_t iz = 0;; ++iz) {
            const double dz = from + static_cast<double>(iz) * 0.5;
            if (dz > through) break;
            for (int64_t ix = 0;; ++ix) {
                const double dx = from + static_cast<double>(ix) * 0.5;
                if (dx > through) break;
                if (!at(p + Vec2(dx, dz))) return false;
            }
        }
        return true;
    };
    // The walking grid's cells, then the heights' spread: the terrain's
    // alone, so kept by spot.
    if (nav && !footprint([&](Vec2 q) { return nav->standable(q) && !nav->inRock(q); })) return "Can't build there";
    if (field) {
        uint64_t bx, by, br;
        std::memcpy(&bx, &p.x, 8);
        std::memcpy(&by, &p.y, 8);
        std::memcpy(&br, &r, 8);
        const SpreadKey key{bx, by, br};
        auto it = heightSpreads.find(key);
        if (it == heightSpreads.end()) {
            double low = std::numeric_limits<double>::infinity(), high = -std::numeric_limits<double>::infinity();
            footprint([&](Vec2 q) {
                const double h = field->height(q);
                low = min(low, h);
                high = max(high, h);
                return true;
            });
            if (heightSpreads.size() >= kHeightSpreads) heightSpreads.clear();
            it = heightSpreads.emplace(key, high - low).first;
        }
        // 0.2 over a footprint up to the strategy game's Citadel's (its
        // samples 4.5 cells apart), then in proportion to the samples' span:
        // the same slope that takes a 5x5 Citadel takes the 12x12 one (C1;
        // its hall's floor slab hides the rest).
        if (it->second > 0.2 * max(1.0, (2 * r - 0.5) / 4.5)) return "The ground is not level";
    }
    for (const Structure& s : state.structures) {
        if (std::abs(s.position.x - p.x) < r + Rules::radius(s.kind)
            && std::abs(s.position.y - p.y) < r + Rules::radius(s.kind)) {
            return "Something is in the way";
        }
    }
    // Add-ons need room on the +X side of a production building.
    for (const Structure& s : state.structures) {
        if (!((s.kind == Structure::Kind::garrison || s.kind == Structure::Kind::foundry
               || s.kind == Structure::Kind::spacedock)
              && !s.addon)) {
            continue;
        }
        const Vec2 lab = s.position + Rules::addonOffset;
        if (std::abs(lab.x - p.x) < r + 1 && std::abs(lab.y - p.y) < r + 1) return "Leave room for the Lab";
    }
    for (const OreDeposit& m : state.patches) {
        if (!(m.remaining > 0)) continue;
        // A field is 2x1: its corners and centre against the footprint.
        const double c = std::cos(m.angle), s = std::sin(m.angle);
        constexpr std::array<std::pair<double, double>, 5> corners{
            {{0.0, 0.0}, {1, 0.5}, {1, -0.5}, {-1, 0.5}, {-1, -0.5}}};
        for (const auto& [lx, lz] : corners) {
            const Vec2 q = m.position + Vec2(lx * c + lz * s, -lx * s + lz * c);
            if (std::abs(q.x - p.x) < r + 0.1 && std::abs(q.y - p.y) < r + 0.1) return "Something is in the way";
        }
    }
    if (state.wells) {
        for (const Well& g : *state.wells) {
            if (std::abs(g.position.x - p.x) < r + 1.5 && std::abs(g.position.y - p.y) < r + 1.5) {
                return "Something is in the way";
            }
        }
    }
    // A Citadel keeps 3 cells from its edge to the ore and 3.5 to a well
    // (5.5 and 6 from the 5x5's centre; the 12x12's follow its side, C1).
    if (kind == Structure::Kind::citadel
        && (std::any_of(state.patches.begin(), state.patches.end(),
                        [&](const OreDeposit& m) { return m.remaining > 0 && distance(m.position, p) < r + 3; })
            || (state.wells && std::any_of(state.wells->begin(), state.wells->end(),
                                           [&](const Well& w) { return distance(w.position, p) < r + 3.5; })))) {
        return "Too close to resources";
    }
    return std::nullopt;
}

/// The driven Prospector puts up a `kind` at `p`: paid now, and it welds it
/// until done or until it walks off. Returns why not, or nil when it
/// started.
std::optional<std::string> Simulation::pilotBuild(Structure::Kind kind, Vec2 p) {
    const auto i = pilot ? unitIndex(pilot->unit) : std::nullopt;
    if (!i || state.units[*i].kind != Unit::Kind::prospector) return "No Prospector";
    const Unit u = state.units[*i];
    if (auto why = buildRefusal(kind, u.owner)) return why;
    if (auto why = placementRefusal(kind, p)) return why;
    if (NavGrid::insideStructure(u.position, Structure(-1, kind, 0, p), Rules::radius(Unit::Kind::prospector))) {
        return "Step back first";
    }
    if (u.task == Unit::Task::mining || u.task == Unit::Task::waiting) {
        Unit v = u;
        leavePatch(v);
        state.units[*i] = v;
    }
    const Price price = buildCost(kind, u.owner);
    state.players[u.owner].ore -= price.ore;
    state.players[u.owner].hydrogen -= price.hydrogen;
    Structure s(state.nextID, kind, u.owner, p, Rules::buildTime(kind), u.id);
    state.nextID += 1;
    if (skilled(u)) {
        // The pilot's own; Head start: it starts part built.
        pilotBuildings.insert(s.id);
        const double share = min(0.95, max(0.0, skillBoost(SkillStat::builtOnPlacing).plus));
        s.buildLeft = Rules::buildTime(kind) * (1 - share);
        s.hp = fullHP(s) * (0.1 + 0.9 * share);
    }
    state.structures.push_back(s);
    state.units[*i].task = Unit::Task::building;
    state.units[*i].structure = s.id;
    refreshNav();
    return std::nullopt;
}

/// A building's name as the game writes it.
std::string Simulation::title(Structure::Kind k) {
    switch (k) {
    case Structure::Kind::citadel: return "Citadel";
    case Structure::Kind::habDome: return "Hab Dome";
    case Structure::Kind::garrison: return "Garrison";
    case Structure::Kind::bastion: return "Bastion";
    case Structure::Kind::derrick: return "Derrick";
    case Structure::Kind::foundry: return "Foundry";
    case Structure::Kind::spacedock: return "Spacedock";
    case Structure::Kind::lab: return "Lab";
    case Structure::Kind::sentinel: return "Sentinel";
    case Structure::Kind::gatling: return "Gatling Gun";
    case Structure::Kind::landingPad: return "Landing Pad";
    case Structure::Kind::dummy: return "Target Dummy";
    }
    return "";
}

/// A unit's name as the game writes it.
std::string Simulation::title(Unit::Kind k) { return std::string(Rules::stats(k).title); }

} // namespace ac
