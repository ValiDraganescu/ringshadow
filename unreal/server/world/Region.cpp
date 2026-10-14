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

MapDefinition buildMap(const RegionConfig& c) {
    if (c.built) return c.built->map;
    const MapChoice& choice = c.map;
    return choice.players > 2 ? WindowMaps::buildSquare(choice) : WindowMaps::build(choice);
}

/// Wild ground: the map's ore and wells, one player with nothing.
GameState wildState(const MapDefinition& map) {
    GameState s = GameState::new_(map);
    s.structures.clear();
    s.units.clear();
    s.players = {Player()};
    s.players[0].ore = 0;
    s.players[0].supplyUsed = 0;
    s.score = {0};
    return s;
}

Simulation startSimulation(const RegionConfig& c, const MapDefinition& map) {
    const GameState state = c.state ? *c.state : c.wild ? wildState(map) : GameState::new_(map, std::nullopt, 0, c.teams);
    const bool ai = c.ai && !c.wild;
    Simulation sim(state, map, ai ? Commander::all(map) : std::vector<Commander>{}, c.fog);
    if (c.fastPaths) sim.setFastPaths(true, c.pathBudget);
    // A world's region levels slower.
    if (c.wild) sim.xpScale = kWorldXpScale;
    return sim;
}

double clampUnit(double v) { return std::isfinite(v) ? std::clamp(v, -1.0, 1.0) : 0.0; }

} // namespace

Region::Region(const RegionConfig& config)
    : built_(config.built), map_(buildMap(config)), sim_(startSimulation(config, map_)) {
    description_.map = config.map;
    description_.regionFile = config.regionFile;
    description_.wild = config.wild;
    description_.mapName = map_.name;
    description_.mapVersion = map_.version;
    description_.seed = map_.seed;
    description_.fog = config.fog;
    ownPatches_ = sim_.state.patches.size();
}

bool Region::join(PilotId pilot, int64_t lastInput) {
    Seat seat;
    seat.lastApplied = lastInput;
    return pilots_.emplace(pilot, seat).second;
}

std::vector<PilotId> Region::pilots() const {
    std::vector<PilotId> out;
    for (const auto& [id, seat] : pilots_) out.push_back(id);
    return out;
}

bool Region::driven() const {
    for (const auto& [id, seat] : pilots_)
        if (seat.drives) return true;
    return false;
}

int64_t Region::admit(const Unit& u) {
    GameState& s = sim_.state;
    Unit in = arrival(u);
    in.id = s.nextID++;
    s.units.push_back(in);
    sim_.reindex();
    sim_.refreshSupply();
    return in.id;
}

std::optional<Unit> Region::release(int64_t unit) {
    const auto i = sim_.unitIndex(unit);
    if (!i) return std::nullopt;
    const Unit out = sim_.state.units[static_cast<size_t>(*i)];
    for (auto& [id, seat] : pilots_)
        if (seat.drives == unit) {
            if (drivesNow(seat)) stashLevels(seat);
            seat.drives = std::nullopt;
        }
    // Taken off the ground as it is: no hand-back to an AI.
    if (sim_.pilot && sim_.pilot->unit == unit) sim_.pilot = std::nullopt;
    GameState& s = sim_.state;
    for (auto& p : s.patches)
        if (p.miner == unit) p.miner = std::nullopt;
    if (s.wells)
        for (auto& w : *s.wells)
            if (w.harvester == unit) w.harvester = std::nullopt;
    for (auto& b : s.structures)
        if (b.builder == unit) b.builder = std::nullopt;
    s.units.erase(s.units.begin() + static_cast<long>(*i));
    sim_.reindex();
    sim_.refreshSupply();
    return out;
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
    // Its own levels in the simulation before the core's take, which adds
    // the kind at level 1 when it is new.
    if (!drivesNow(it->second)) loadLevels(it->second);
    if (!sim_.take(unit)) {
        // The core hands back what it drove before it refuses.
        if (!sim_.pilot) {
            stashLevels(it->second);
            it->second.drives = std::nullopt;
        }
        return false;
    }
    it->second.drives = unit;
    it->second.drove = unit;
    // Its skills on the unit it drives now.
    loadProgress(it->second);
    // Until its first input the unit stands, facing where it faces.
    PilotInput& h = it->second.held;
    h.walk = Vec2(0, 0);
    h.heading = sim_.pilot->heading;
    h.act = h.ability = h.hold = h.mend = false;
    h.crosshair = std::nullopt;
    h.slot = 0;
    return true;
}

void Region::handBack(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.drives) return;
    if (sim_.pilot && sim_.pilot->unit == *it->second.drives) {
        sim_.take(std::nullopt);
        stashLevels(it->second);
    }
    it->second.drives = std::nullopt;
}

bool Region::drivesNow(const Seat& seat) const { return seat.drives && sim_.pilot && sim_.pilot->unit == *seat.drives; }

std::optional<PilotRecord>* Region::sideLevels() {
    auto& players = sim_.state.players;
    if (Pilot::player < 0 || Pilot::player >= static_cast<int64_t>(players.size())) return nullptr;
    return &players[static_cast<size_t>(Pilot::player)].pilot;
}

void Region::loadLevels(Seat& seat) {
    if (auto* side = sideLevels()) *side = seat.levels;
}

void Region::stashLevels(Seat& seat) {
    if (auto* side = sideLevels()) {
        seat.levels = *side;
        *side = std::nullopt;
    }
    clearProgress();
}

void Region::loadProgress(const Seat& seat) {
    sim_.setProgress(seat.progress);
    sim_.pilotBuildings = seat.buildings;
}

void Region::clearProgress() {
    sim_.setProgress(std::nullopt);
    sim_.pilotBuildings.clear();
}

std::optional<PilotRecord> Region::levels(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    if (drivesNow(it->second)) {
        const auto& players = sim_.state.players;
        if (Pilot::player >= 0 && Pilot::player < static_cast<int64_t>(players.size()))
            return players[static_cast<size_t>(Pilot::player)].pilot;
    }
    return it->second.levels;
}

void Region::setLevels(PilotId pilot, const std::optional<PilotRecord>& levels) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    if (drivesNow(it->second)) {
        if (auto* side = sideLevels()) *side = levels;
    } else {
        it->second.levels = levels;
    }
}

void Region::setProgress(PilotId pilot, const std::optional<ProspectorProgress>& progress, const std::set<int64_t>& buildings) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    it->second.progress = progress;
    it->second.buildings = buildings;
    if (drivesNow(it->second)) loadProgress(it->second);
}

std::optional<std::string> Region::pick(PilotId pilot, UnitKind kind, Perk perk) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "Not joined";
    if (ac::kind(perk) != kind) return "Not a card of that kind";
    if (const auto why = levels(pilot).value_or(PilotRecord{}).refusal(perk)) return why;
    Seat& seat = it->second;
    auto* side = sideLevels();
    if (!side) return "No side";
    // The core's own pick on the pilot's record: in the simulation while it
    // drives; else lent to it for the pick (whoever drives keeps theirs).
    const bool driving = drivesNow(seat);
    std::optional<PilotRecord> driver;
    if (!driving) {
        driver = *side;
        *side = seat.levels;
    }
    const bool kept = sim_.pick(perk);
    if (!driving) {
        seat.levels = *side;
        *side = driver;
    }
    return kept ? std::nullopt : std::optional<std::string>("Can't pick that now");
}

double Region::buildReach(StructureKind kind) {
    return kind == StructureKind::derrick ? 3.0 : Rules::radius(kind) + 2.2;
}

std::optional<std::string> Region::build(PilotId pilot, StructureKind kind, Vec2 at) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.drives) return "Nothing driven";
    // A world's Prospector also puts up a Lab (P6).
    const auto& kinds = description_.wild ? std::vector<StructureKind>(PilotBuild::worldKinds.begin(), PilotBuild::worldKinds.end())
                                          : std::vector<StructureKind>(PilotBuild::kinds.begin(), PilotBuild::kinds.end());
    if (std::find(kinds.begin(), kinds.end(), kind) == kinds.end()) return "A Prospector can't build that";
    const auto u = sim_.pilotUnit();
    if (!u || u->id != *it->second.drives || !PilotBuild::builds(u->kind)) return "No Prospector";
    if (!std::isfinite(at.x) || !std::isfinite(at.y)) return "Can't build there";
    const auto p = sim_.snap(kind, at);
    if (!p) return "No well here";
    if (distance(u->position, *p) > buildReach(kind) + buildSlack) return "Too far away";
    return sim_.pilotBuild(kind, *p);
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
        if (seat.pending.front().slot == 0) seat.pending.front().slot = old.slot;
        seat.lastApplied = old.tick;
    }
    return true;
}

int64_t Region::lastInputTick(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? -1 : it->second.lastApplied;
}

std::optional<PilotInput> Region::held(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    return it->second.held;
}

void Region::nextInput(Seat& seat) {
    // Presses count once; held keys, the walk and the heading carry on
    // until the next input comes.
    seat.held.act = false;
    seat.held.ability = false;
    seat.held.slot = 0;
    if (seat.pending.empty()) return;
    seat.held = seat.pending.front();
    seat.pending.pop_front();
    seat.lastApplied = seat.held.tick;
}

void Region::step() {
    // Who drives this step: its levels' events are its own.
    std::optional<PilotId> driver;
    for (auto& [id, seat] : pilots_)
        if (drivesNow(seat)) driver = id;
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
        p.slot = h.slot;
        p.crosshair = h.crosshair;
    }
    std::vector<GameEvent> events = sim_.step(description_.step);
    lastEvents_ = events;
    tick_ += 1;
    for (auto& [id, seat] : pilots_) {
        // The driven unit died, or got where it cannot be driven from.
        if (seat.drives && !drivesNow(seat)) {
            stashLevels(seat);
            seat.drives = std::nullopt;
        }
        for (const GameEvent& e : events) {
            // A level up is the driver's own, not every pilot's of the side.
            if (e.is<GameEvent::LeveledUp>() && driver != id) continue;
            if (mayKnow(sim_, side(id), e)) seat.events.push_back(e);
        }
        if (seat.events.size() > maxEvents)
            seat.events.erase(seat.events.begin(), seat.events.begin() + static_cast<long>(seat.events.size() - maxEvents));
    }
    // The victory lap, then the next game on the same map.
    if (sim_.state.endedAt && sim_.state.time - *sim_.state.endedAt >= Rules::victoryLap) {
        // The pilots' levels are their own: kept into the next game.
        for (auto& [id, seat] : pilots_)
            if (drivesNow(seat)) stashLevels(seat);
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

Unit arrival(const Unit& u) {
    Unit out = scrubbed(u);
    out.task = Unit::Task::idle;
    out.patch = std::nullopt;
    out.structure = std::nullopt;
    out.target = std::nullopt;
    out.lockTarget = std::nullopt;
    out.timer = 0;
    return out;
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
