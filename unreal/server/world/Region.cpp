// The region: the game's simulation headless, its pilots and their inputs
// (World.h). Steps as UAcSimSubsystem does in the game: fixed 1/60 s, the
// fast paths on, the next game on the same map after the victory lap.

#include "World.h"

#include "Commander.h"
#include "Rules.h"

#include <algorithm>
#include <cmath>

namespace ac::server {

namespace {

MapDefinition buildMap(const MapChoice& choice) {
    return choice.players > 2 ? WindowMaps::buildSquare(choice) : WindowMaps::build(choice);
}

Simulation startSimulation(const RegionConfig& c, const MapDefinition& map) {
    const GameState state = c.state ? *c.state : GameState::new_(map, std::nullopt, 0, c.teams);
    Simulation sim(state, map, c.ai ? Commander::all(map) : std::vector<Commander>{}, c.fog);
    if (c.fastPaths) sim.setFastPaths(true, c.pathBudget);
    return sim;
}

double clampUnit(double v) { return std::isfinite(v) ? std::clamp(v, -1.0, 1.0) : 0.0; }

} // namespace

Region::Region(const RegionConfig& config)
    : map_(buildMap(config.map)), sim_(startSimulation(config, map_)) {
    description_.map = config.map;
    description_.mapName = map_.name;
    description_.mapVersion = map_.version;
    description_.seed = map_.seed;
    description_.fog = config.fog;
}

bool Region::join(PilotId pilot) {
    return pilots_.emplace(pilot, Seat{}).second;
}

void Region::leave(PilotId pilot) {
    if (!joined(pilot)) return;
    handBack(pilot);
    pilots_.erase(pilot);
}

bool Region::take(PilotId pilot, int64_t unit) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return false;
    // One pilot in the simulation: another one's unit is not free.
    for (const auto& [id, seat] : pilots_)
        if (id != pilot && seat.drives) return false;
    if (!sim_.take(unit)) {
        // The core hands back what it drove before it refuses.
        if (!sim_.pilot) it->second.drives = std::nullopt;
        return false;
    }
    it->second.drives = unit;
    // Until its first input the unit stands, facing where it faces.
    PilotInput& h = it->second.held;
    h.walk = Vec2(0, 0);
    h.heading = sim_.pilot->heading;
    h.act = h.ability = h.hold = h.mend = false;
    h.crosshair = std::nullopt;
    return true;
}

void Region::handBack(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.drives) return;
    if (sim_.pilot && sim_.pilot->unit == *it->second.drives) sim_.take(std::nullopt);
    it->second.drives = std::nullopt;
}

std::optional<int64_t> Region::driving(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? std::nullopt : it->second.drives;
}

bool Region::input(PilotId pilot, const PilotInput& in) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return false;
    Seat& seat = it->second;
    if (in.tick <= seat.lastApplied) return false;
    // In tick order; a repeat (inputs go out with the last few again) is dropped.
    auto at = std::lower_bound(seat.pending.begin(), seat.pending.end(), in.tick,
                               [](const PilotInput& p, int64_t t) { return p.tick < t; });
    if (at != seat.pending.end() && at->tick == in.tick) return false;
    PilotInput clean = in;
    clean.walk = Vec2(clampUnit(in.walk.x), clampUnit(in.walk.y));
    if (!std::isfinite(clean.heading)) clean.heading = seat.held.heading;
    seat.pending.insert(at, clean);
    // A client far ahead: its oldest inputs are used up now, their presses
    // kept for the next.
    while (seat.pending.size() > maxBuffered) {
        const PilotInput old = seat.pending.front();
        seat.pending.pop_front();
        seat.pending.front().act = seat.pending.front().act || old.act;
        seat.pending.front().ability = seat.pending.front().ability || old.ability;
        seat.lastApplied = old.tick;
    }
    return true;
}

int64_t Region::lastInputTick(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? -1 : it->second.lastApplied;
}

void Region::nextInput(Seat& seat) {
    // Presses count once; held keys, the walk and the heading carry on
    // until the next input comes.
    seat.held.act = false;
    seat.held.ability = false;
    if (seat.pending.empty()) return;
    seat.held = seat.pending.front();
    seat.pending.pop_front();
    seat.lastApplied = seat.held.tick;
}

void Region::step() {
    for (auto& [id, seat] : pilots_) {
        nextInput(seat);
        if (!(seat.drives && sim_.pilot && sim_.pilot->unit == *seat.drives)) continue;
        Pilot& p = *sim_.pilot;
        const PilotInput& h = seat.held;
        p.walk = h.walk;
        p.heading = h.heading;
        p.act = h.act;
        p.ability = h.ability;
        p.hold = h.hold;
        p.mend = h.mend;
        p.crosshair = h.crosshair;
    }
    std::vector<GameEvent> events = sim_.step(description_.step);
    tick_ += 1;
    for (auto& [id, seat] : pilots_) {
        // The driven unit died, or got where it cannot be driven from.
        if (seat.drives && !(sim_.pilot && sim_.pilot->unit == *seat.drives)) seat.drives = std::nullopt;
        for (const GameEvent& e : events)
            if (mayKnow(sim_, side(id), e)) seat.events.push_back(e);
        if (seat.events.size() > maxEvents)
            seat.events.erase(seat.events.begin(), seat.events.begin() + static_cast<long>(seat.events.size() - maxEvents));
    }
    // The victory lap, then the next game on the same map.
    if (sim_.state.endedAt && sim_.state.time - *sim_.state.endedAt >= Rules::victoryLap) {
        sim_.newGame();
        game_ += 1;
        for (auto& [id, seat] : pilots_) {
            seat.drives = std::nullopt;
            seat.waiting.clear();
            seat.known.clear();
            seat.events.clear();
        }
    }
}

// MARK: - What a side may know

bool mayKnow(const Simulation& sim, int64_t side, const GameEvent& e) {
    const GameState& s = sim.state;
    auto at = [&](Vec2 p) { return sim.sees(side, p); };
    auto id = [&](int64_t i) { return sim.sees(side, i); };
    auto ours = [&](int64_t owner) { return s.allied(owner, side); };
    auto unitOwner = [&](int64_t i) -> std::optional<int64_t> {
        if (const auto k = sim.unitIndex(i)) return s.units[static_cast<size_t>(*k)].owner;
        return std::nullopt;
    };
    auto ourUnit = [&](int64_t i) { const auto o = unitOwner(i); return o && ours(*o); };
    auto patchSeen = [&](int64_t i) {
        return i >= 0 && i < static_cast<int64_t>(s.patches.size()) && at(s.patches[static_cast<size_t>(i)].position);
    };
    if (const auto* d = e.as<GameEvent::Deposited>()) return ourUnit(d->unit) || at(d->at);
    if (const auto* d = e.as<GameEvent::PatchDepleted>()) return patchSeen(d->patch);
    if (const auto* r = e.as<GameEvent::PatchRegrown>()) return patchSeen(r->patch);
    if (const auto* t = e.as<GameEvent::Trained>()) return ourUnit(t->unit) || at(t->at);
    if (const auto* c = e.as<GameEvent::ConstructionStarted>()) return id(c->structure) || at(c->at);
    if (const auto* c = e.as<GameEvent::Constructed>()) return id(c->structure) || at(c->at);
    if (const auto* sh = e.as<GameEvent::Shot>()) return ours(sh->victim) || id(sh->unit) || id(sh->target) || at(sh->at);
    if (const auto* m = e.as<GameEvent::Missed>()) return id(m->unit) || at(m->at);
    if (const auto* l = e.as<GameEvent::Landed>()) return id(l->unit) || id(l->target);
    if (const auto* d = e.as<GameEvent::Died>()) return ours(d->owner) || id(d->unit) || at(d->at);
    if (const auto* d = e.as<GameEvent::Destroyed>()) return ours(d->owner) || id(d->structure) || at(d->at);
    if (e.is<GameEvent::Victory>()) return true;
    if (const auto* r = e.as<GameEvent::Researched>()) return ours(r->owner);
    if (const auto* st = e.as<GameEvent::Stuck>()) return ours(st->owner) || at(st->at);
    if (const auto* t = e.as<GameEvent::Trapped>()) return ours(t->owner) || at(t->at);
    // The human side's driving levels (`Pilot::player`'s record).
    if (e.is<GameEvent::LeveledUp>()) return ours(Pilot::player);
    if (const auto* b = e.as<GameEvent::Blast>()) return ourUnit(b->unit) || id(b->unit) || at(b->at);
    if (const auto* st = e.as<GameEvent::Stomp>()) return ourUnit(st->unit) || id(st->unit) || at(st->at);
    return false;
}

Unit scrubbed(const Unit& u) {
    Unit out = u;
    out.goal = std::nullopt;
    out.waypoints = std::nullopt;
    out.mission = std::nullopt;
    out.order = std::nullopt;
    out.autoFollow = std::nullopt;
    out.shotFrom = std::nullopt;
    out.boardedAt = std::nullopt;
    return out;
}

} // namespace ac::server
