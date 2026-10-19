// The Mule (Simulation+Mule.members.h; C4, docs/haul-and-ferry.md section 2):
// its rider, its fill at the hopper, its booking in a Citadel's hall, its
// roll-out.

#include "Simulation.h"

#include "Mule.h"

#include <cmath>

namespace ac {

namespace {

bool isMule(const Unit& u) { return u.kind == UnitKind::mule; }

} // namespace

std::optional<int64_t> Simulation::muleRider(int64_t mule) const {
    const auto m = state.unit(mule);
    if (!m || !isMule(*m) || !m->cargo || m->cargo->empty()) return std::nullopt;
    return m->cargo->front();
}

std::optional<int64_t> Simulation::ridden(const Unit& u) const {
    if (u.task != Unit::Task::aboard || !u.structure) return std::nullopt;
    const auto i = unitIndex(*u.structure);
    if (!i || !isMule(state.units[size_t(*i)])) return std::nullopt;
    return *u.structure;
}

std::optional<std::string> Simulation::muleBoardRefusal(const Unit& u, const Unit& m) const {
    if (!isMule(m) || !(m.hp > 0)) return "No Mule there";
    if (u.kind != UnitKind::prospector || !(u.hp > 0) || u.owner != m.owner) return "Only a Prospector drives the Mule";
    if (u.task == Unit::Task::aboard || u.task == Unit::Task::atGun || u.task == Unit::Task::inDerrick) return "On foot first";
    if (m.cargo && !m.cargo->empty()) return "Someone drives it already";
    if (distance(u.position, Mule::perch(m)) > Mule::reach) return "Get to the Mule's perch";
    return std::nullopt;
}

std::optional<std::string> Simulation::muleBoard(int64_t prospector, int64_t mule) {
    const auto i = unitIndex(prospector);
    const auto mi = unitIndex(mule);
    if (!i || !mi) return "No Mule there";
    if (auto why = muleBoardRefusal(state.units[size_t(*i)], state.units[size_t(*mi)])) return why;
    Unit& m = state.units[size_t(*mi)];
    m.cargo = std::vector<int64_t>{prospector};
    Unit& u = state.units[size_t(*i)];
    u.task = Unit::Task::aboard;
    u.structure = mule;
    u.patch = std::nullopt;
    u.target = std::nullopt;
    u.goal = std::nullopt;
    u.waypoints = std::nullopt;
    u.position = m.position;
    u.heading = m.heading;
    return std::nullopt;
}

std::optional<int64_t> Simulation::muleLeave(int64_t mule) {
    const auto mi = unitIndex(mule);
    if (!mi) return std::nullopt;
    const Unit m = state.units[size_t(*mi)];
    if (!isMule(m) || !m.cargo || m.cargo->empty()) return std::nullopt;
    const int64_t id = m.cargo->front();
    state.units[size_t(*mi)].cargo = std::vector<int64_t>{};
    const auto i = unitIndex(id);
    if (!i) return std::nullopt;
    Unit& u = state.units[size_t(*i)];
    u.task = Unit::Task::idle;
    u.structure = std::nullopt;
    u.timer = 0;
    // Down beside the perch, off its left side, else its right; else the
    // nearest open ground round it.
    const double side = Mule::width / 2 + Rules::radius(u.kind) + 0.15;
    Vec2 at = m.position;
    bool placed = false;
    for (const double s : {side, -side, side + 0.6, -side - 0.6}) {
        const Vec2 q = Mule::point(m, Vec2(Mule::perchLocal.x, s));
        if (pilotCanStand(u, q)) {
            at = q;
            placed = true;
            break;
        }
    }
    if (!placed) {
        for (double rad = 1.0; rad <= 4.0 && !placed; rad += 0.5) {
            for (int k = 0; k < 16; k++) {
                const double a = k * 2 * pi / 16;
                const Vec2 q = m.position + Vec2(std::cos(a), std::sin(a)) * rad;
                if (pilotCanStand(u, q)) {
                    at = q;
                    placed = true;
                    break;
                }
            }
        }
    }
    u.position = at;
    u.heading = m.heading;
    return id;
}

std::optional<std::string> Simulation::muleFillRefusal(const Unit& u, const Unit& m) const {
    if (!isMule(m) || !(m.hp > 0)) return "No Mule there";
    if (u.task == Unit::Task::aboard) return "Climb off the Mule first";
    if (u.kind != UnitKind::prospector || u.owner != m.owner) return "Only a Prospector fills the Mule";
    if (!(u.carrying > 0)) return "Nothing to put in";
    if (distance(u.position, Mule::hopper(m)) > Mule::reach) return "Get to the Mule's hopper";
    if (Mule::room(m) <= 0) return std::string(Mule::full);
    return std::nullopt;
}

std::optional<std::string> Simulation::muleFill(int64_t prospector, int64_t mule) {
    const auto i = unitIndex(prospector);
    const auto mi = unitIndex(mule);
    if (!i || !mi) return "No Mule there";
    if (auto why = muleFillRefusal(state.units[size_t(*i)], state.units[size_t(*mi)])) return why;
    Unit& u = state.units[size_t(*i)];
    Haul& h = *(state.units[size_t(*mi)].haul = state.units[size_t(*mi)].haul.value_or(Haul{}));
    // The whole load, even the one that takes it over its room.
    (u.hydrogen == true ? h.hydrogen : h.ore) += u.carrying;
    u.carrying = 0;
    u.hydrogen = std::nullopt;
    if (u.task == Unit::Task::toBase || u.task == Unit::Task::depositing) u.task = Unit::Task::idle;
    return std::nullopt;
}

std::optional<int64_t> Simulation::rollOut(int64_t foundry, int64_t owner) {
    const auto b = state.structure(foundry);
    if (!b) return std::nullopt;
    const Vec2 door = Mule::door(b->position);
    Vec2 at = door;
    if (nav) at = nav->nearestFree(door).value_or(door);
    Unit u = freshUnit(UnitKind::mule, state.nextID, owner, at, Mule::doorHeading, Unit::Task::idle);
    state.nextID += 1;
    state.units.push_back(u);
    reindex();
    between.push_back(GameEvent::Trained{u.id, u.kind, u.position});
    return u.id;
}

void Simulation::stepMules(std::vector<GameEvent>& events) {
    for (size_t k = 0; k < state.units.size(); k++) {
        if (!isMule(state.units[k]) || !(state.units[k].hp > 0)) continue;
        const Unit m = state.units[k];
        // Its rider on it, wherever it went.
        if (m.cargo)
            for (const int64_t id : *m.cargo)
                if (const auto i = unitIndex(id)) {
                    state.units[size_t(*i)].position = m.position;
                    state.units[size_t(*i)].heading = m.heading;
                }
        if (!m.haul || m.haul->total() <= 0 || !piloted(m.id)) continue;
        const Structure* hall = nullptr;
        for (const Structure& s : state.structures) {
            if (s.kind != StructureKind::citadel || !s.complete() || !(s.hp > 0) || state.hostile(s.owner, m.owner)) continue;
            if (Rules::footprint(s.kind).inside(m.position - s.position)) {
                hall = &s;
                break;
            }
        }
        if (!hall) continue;
        const Vec2 at = hall->position;
        const Haul load = *m.haul;
        // Its driver's bank and skills: swapped in while another human
        // drives too (W5).
        asDriver(m.id, [&] {
            // Stardust heart, Silver tide: as a Prospector's unload counts.
            auto counted = [&](int64_t n, bool hydrogen) {
                if (!progress) return n;
                return skillBoost(hydrogen ? SkillStat::unloadHydrogen : SkillStat::unloadOre).apply(n);
            };
            const int64_t ore = counted(load.ore, false), hydrogen = counted(load.hydrogen, true);
            // The realm's cut (C1), in a world.
            const int64_t taxOre = progress ? Progression::realmTax(ore) : 0;
            const int64_t taxHydrogen = progress ? Progression::realmTax(hydrogen) : 0;
            Player& side = state.players[size_t(m.owner)];
            side.ore += ore - taxOre;
            side.hydrogen += hydrogen - taxHydrogen;
            side.totalMined += ore;
            events.push_back(GameEvent::Booked{m.id, ore, hydrogen, at, taxOre, taxHydrogen});
        });
        state.units[k].haul = Haul{};
    }
}

std::optional<int64_t> Simulation::hurtMule(const Unit& u) const {
    if (u.kind != UnitKind::prospector || !piloted(u.id)) return std::nullopt;
    const Unit* best = nullptr;
    for (const Unit& v : state.units) {
        if (!(isMule(v) && v.owner == u.owner && v.hp > 0 && v.hp < maxHP(v)
              && gap(u, Target{.position = v.position, .radius = Rules::radius(v.kind), .structure = false, .owner = v.owner})
                     <= workReach(u)
              && ahead(u, v.position))) {
            continue;
        }
        if (!best || distance(v.position, u.position) < distance(best->position, u.position)) best = &v;
    }
    if (!best) return std::nullopt;
    return best->id;
}

bool Simulation::prospectorMendMule(Unit& u, int64_t mule, const Pilot& p, double dt, std::vector<GameEvent>& events) {
    const auto j = unitIndex(mule);
    auto stop = [&] {
        u.task = Unit::Task::idle;
        u.structure = std::nullopt;
        return false;
    };
    if (!j || !p.hold || length(p.walk) > 0.1) return stop();
    const Unit v = state.units[size_t(*j)];
    const double full = maxHP(v);
    const double reach =
        gap(u, Target{.position = v.position, .radius = Rules::radius(v.kind), .structure = false, .owner = v.owner});
    if (!(v.hp > 0 && v.hp < full && reach <= workReach(u) + 0.3)) return stop();
    turn(u, v.position, dt);
    const double mended = min(full - v.hp, full / Rules::trainTime(v.kind) * rate(Stat::Repair{}, u) * dt);
    // Its price's share, as a building's repair (`repairCost`).
    const double share = mended / full * Rules::repairShare;
    Structure::Owed& owed = muleOwed[mule];
    const Structure::Owed next{owed.ore + share * double(Mule::ore()), owed.hydrogen + share * double(Mule::hydrogen())};
    const int64_t dueOre = static_cast<int64_t>(std::floor(next.ore + 1e-9));
    const int64_t dueHydrogen = static_cast<int64_t>(std::floor(next.hydrogen + 1e-9));
    if (dueOre > 0 || dueHydrogen > 0) {
        if (u.owner < 0 || u.owner >= static_cast<int64_t>(state.players.size())) return stop();
        Player& bank = state.players[size_t(u.owner)];
        if (bank.ore < dueOre || bank.hydrogen < dueHydrogen) return stop();
        bank.ore -= dueOre;
        bank.hydrogen -= dueHydrogen;
        events.push_back(GameEvent::Repaired{mule, u.id, dueOre, dueHydrogen});
    }
    owed = Structure::Owed{next.ore - double(dueOre), next.hydrogen - double(dueHydrogen)};
    state.units[size_t(*j)].hp += mended;
    tally(u, [&](Tally& t) { t.repaired += mended; });
    return true;
}

void Simulation::muleDown(const Unit& m) {
    if (!isMule(m) || !m.cargo || m.cargo->empty()) return;
    const auto i = unitIndex(m.cargo->front());
    if (i && state.units[size_t(*i)].hp > 0) muleLeave(m.id);
}

} // namespace ac
