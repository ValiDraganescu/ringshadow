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

void Region::watch(PilotId pilot, std::optional<Vec2> at) {
    const auto it = pilots_.find(pilot);
    if (it != pilots_.end()) it->second.watching = at;
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
    // C1: off its gun first (it leaves the region as a walker).
    sim_.unman(unit);
    const Unit out = sim_.state.units[static_cast<size_t>(*i)];
    for (auto& [id, seat] : pilots_)
        if (seat.drives == unit) {
            keepBank(seat);
            if (drivesNow(seat)) stashLevels(seat);
            seat.drives = std::nullopt;
        }
    // Taken off the ground as it is (W5: its driver's context goes, the
    // next human driving here becomes the active one).
    if (sim_.piloted(unit)) sim_.handBack(unit);
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
    Seat& seat = it->second;
    // W5: another human's unit is not free; every human drives its own.
    if (seat.drives != unit && sim_.piloted(unit)) return false;
    // Its own unit already: nothing to do.
    if (drivesNow(seat) && *seat.drives == unit) return true;
    // Driving another: that one goes back to the AI first.
    if (drivesNow(seat)) {
        keepBank(seat);
        stashLevels(seat);
        // C1: a unit handed back leaves the gun it manned.
        sim_.unman(*seat.drives);
        sim_.handBack(*seat.drives);
        seat.drives = std::nullopt;
    }
    // Its own levels in the simulation with the take, which adds the kind
    // at level 1 when it is new.
    if (!sim_.drive(unit, seat.levels)) return false;
    seat.drives = unit;
    seat.drove = unit;
    // Another human drives too: its own bank in its context.
    if (const auto k = sim_.driverIndex(unit); k && seat.bank) {
        sim_.drivers[*k].ore = seat.bank->first;
        sim_.drivers[*k].hydrogen = seat.bank->second;
    }
    // Its skills on the unit it drives now.
    loadProgress(seat);
    // Until its first input the unit stands, facing where it faces.
    PilotInput& h = seat.held;
    h.walk = Vec2(0, 0);
    h.heading = sim_.pilotFor(unit) ? sim_.pilotFor(unit)->heading : 0.0;
    h.pitch = 0;
    h.act = h.ability = h.hold = h.mend = false;
    h.crosshair = std::nullopt;
    h.slot = 0;
    return true;
}

void Region::handBack(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !it->second.drives) return;
    keepBank(it->second);
    if (drivesNow(it->second)) {
        stashLevels(it->second);
        // C1: a unit handed back leaves the gun it manned.
        sim_.unman(*it->second.drives);
        sim_.handBack(*it->second.drives);
    }
    it->second.drives = std::nullopt;
}

std::optional<std::pair<int64_t, int64_t>> Region::bank(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    Seat& seat = it->second;
    const Player& side = sim_.state.players[static_cast<size_t>(Pilot::player)];
    if (drivesNow(seat)) return sim_.bankFor(*seat.drives);
    if (!sim_.pilot) return std::pair{side.ore, side.hydrogen};
    return seat.bank;
}

void Region::setBank(PilotId pilot, int64_t ore, int64_t hydrogen) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    Seat& seat = it->second;
    seat.bank = std::pair{ore, hydrogen};
    Player& side = sim_.state.players[static_cast<size_t>(Pilot::player)];
    auto set = [&] {
        side.ore = ore;
        side.hydrogen = hydrogen;
    };
    if (drivesNow(seat)) sim_.setBankFor(*seat.drives, ore, hydrogen);
    else if (!sim_.pilot) set();
}

void Region::keepBank(Seat& seat) {
    const Player& side = sim_.state.players[static_cast<size_t>(Pilot::player)];
    if (drivesNow(seat)) seat.bank = sim_.bankFor(*seat.drives);
    // Its unit gone and no one driving: the side's is still its own.
    else if (seat.drives && !sim_.pilot) seat.bank = std::pair{side.ore, side.hydrogen};
}

bool Region::drivesNow(const Seat& seat) const { return seat.drives && sim_.piloted(*seat.drives); }

std::optional<PilotRecord>* Region::sideLevels() {
    auto& players = sim_.state.players;
    if (Pilot::player < 0 || Pilot::player >= static_cast<int64_t>(players.size())) return nullptr;
    return &players[static_cast<size_t>(Pilot::player)].pilot;
}

void Region::loadLevels(Seat& seat) {
    if (!seat.drives) return;
    if (auto* own = sim_.levelsFor(*seat.drives)) *own = seat.levels;
}

void Region::stashLevels(Seat& seat) {
    if (!seat.drives) return;
    // Its unit gone while the active driver: its context is still the active
    // one (`handBack` hands the slot on after).
    auto* own = sim_.levelsFor(*seat.drives);
    if (!own && !sim_.pilot) own = sideLevels();
    if (own) {
        seat.levels = *own;
        *own = std::nullopt;
    }
    clearProgress(seat);
}

void Region::loadProgress(const Seat& seat) {
    if (!seat.drives) return;
    sim_.setProgressFor(*seat.drives, seat.progress);
    if (auto* own = sim_.buildingsFor(*seat.drives)) *own = seat.buildings;
}

void Region::clearProgress(const Seat& seat) {
    if (!seat.drives) return;
    if (!sim_.piloted(*seat.drives) && !sim_.pilot) {
        sim_.setProgress(std::nullopt);
        sim_.pilotBuildings.clear();
        return;
    }
    sim_.setProgressFor(*seat.drives, std::nullopt);
    if (auto* own = sim_.buildingsFor(*seat.drives)) own->clear();
}

std::optional<PilotRecord> Region::levels(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    if (drivesNow(it->second))
        if (auto* own = const_cast<Simulation&>(sim_).levelsFor(*it->second.drives)) return *own;
    return it->second.levels;
}

void Region::setLevels(PilotId pilot, const std::optional<PilotRecord>& levels) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    if (drivesNow(it->second)) {
        if (auto* own = sim_.levelsFor(*it->second.drives)) *own = levels;
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
    // drives (W5: its own context); else lent to it for the pick (whoever
    // drives keeps theirs).
    if (drivesNow(seat)) {
        const bool kept = sim_.asDriver(*seat.drives, [&] { return sim_.pick(perk); });
        return kept ? std::nullopt : std::optional<std::string>("Can't pick that now");
    }
    const bool driving = false;
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
    // W5: with the context of the human driving it.
    return sim_.asDriver(*it->second.drives, [&]() -> std::optional<std::string> {
        const auto u = sim_.pilotUnit();
        if (!u || u->id != *it->second.drives || !PilotBuild::builds(u->kind)) return "No Prospector";
        if (!std::isfinite(at.x) || !std::isfinite(at.y)) return "Can't build there";
        const auto p = sim_.snap(kind, at);
        if (!p) return "No well here";
        if (distance(u->position, *p) > buildReach(kind) + buildSlack) return "Too far away";
        return sim_.pilotBuild(kind, *p);
    });
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
    if (!std::isfinite(clean.pitch)) clean.pitch = seat.held.pitch;
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

std::optional<Region::Inputs> Region::inputs(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    return Inputs{it->second.pending, it->second.held};
}

void Region::carryOn(PilotId pilot, const Inputs& in) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    it->second.pending = in.pending;
    it->second.held = in.held;
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
        Pilot* own = seat.drives ? sim_.pilotFor(*seat.drives) : nullptr;
        if (!own) continue;
        Pilot& p = *own;
        const PilotInput& h = seat.held;
        p.walk = h.walk;
        p.heading = h.heading;
        // C1: the look's pitch (a gunner's aim; the core reads it at a gun only).
        p.pitch = h.pitch;
        p.act = h.act;
        p.ability = h.ability;
        p.hold = h.hold;
        p.mend = h.mend;
        p.slot = h.slot;
        p.crosshair = h.crosshair;
    }
    // A world's region by day (S3): the pilots' daylight round the one who
    // drives, else round the first one's focus (aboard, its pad).
    sim_.daylightAt = std::nullopt;
    if (daylight_ && built_ && !pilots_.empty()) {
        const auto& [id, seat] = driver ? *pilots_.find(*driver) : *pilots_.begin();
        sim_.daylightAt = focus(seat, side(id));
    }
    std::vector<GameEvent> events = sim_.step(description_.step);
    lastEvents_ = events;
    tick_ += 1;
    // C2: who fired each round in flight, for its impact's reach.
    for (const GameEvent& e : events)
        if (const auto* f = e.as<GameEvent::Fired>()) rounds_[f->round] = {f->shooter, sim_.state.time};
    for (auto& [id, seat] : pilots_) {
        // The driven unit died, or got where it cannot be driven from: its
        // levels kept, its driver's context handed back (W5).
        if (seat.drives && (!drivesNow(seat) || !sim_.unitIndex(*seat.drives))) {
            keepBank(seat);
            stashLevels(seat);
            sim_.unman(*seat.drives);
            sim_.handBack(*seat.drives);
            seat.drives = std::nullopt;
        }
        for (const GameEvent& e : events) {
            // A level up is the driver's own, not every pilot's of the side.
            if (e.is<GameEvent::LeveledUp>() && driver != id) continue;
            std::optional<int64_t> shooter;
            if (const auto* i = e.as<GameEvent::Impact>())
                if (const auto r = rounds_.find(i->round); r != rounds_.end()) shooter = r->second.first;
            if (mayKnow(sim_, side(id), e, shooter)) seat.events.push_back(e);
        }
        if (seat.events.size() > maxEvents)
            seat.events.erase(seat.events.begin(), seat.events.begin() + static_cast<long>(seat.events.size() - maxEvents));
    }
    for (const GameEvent& e : events)
        if (const auto* i = e.as<GameEvent::Impact>()) rounds_.erase(i->round);
    std::erase_if(rounds_, [&](const auto& r) { return sim_.state.time - r.second.second > Rules::roundLife + 1; });
    // The victory lap, then the next game on the same map.
    if (sim_.state.endedAt && sim_.state.time - *sim_.state.endedAt >= Rules::victoryLap) {
        // The pilots' levels are their own: kept into the next game.
        for (auto& [id, seat] : pilots_)
            if (drivesNow(seat)) stashLevels(seat);
        sim_.newGame();
        game_ += 1;
        rounds_.clear();
        for (auto& [id, seat] : pilots_) {
            seat.drives = std::nullopt;
            seat.waiting.clear();
            seat.known.clear();
            seat.events.clear();
        }
    }
}

// MARK: - What a side may know

bool mayKnow(const Simulation& sim, int64_t side, const GameEvent& e, std::optional<int64_t> shooter) {
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
    // C2: a round's flight and its end go to whoever may know the shooter
    // (a pilot's own guns: anywhere in the region) or sees where it is.
    auto at3 = [&](Vec3 p) { return at(Vec2(p.x, p.y)); };
    if (const auto* f = e.as<GameEvent::Fired>()) return id(f->shooter) || at3(f->muzzle);
    if (const auto* i = e.as<GameEvent::Impact>())
        return (shooter && id(*shooter)) || (i->what != GameEvent::Impact::What::ground && id(i->victim)) || at3(i->at);
    if (const auto* d = e.as<GameEvent::DummyDown>()) return id(d->id);
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
    // A bug's reload ticks every step: its strikes counter tells the client.
    if (out.bug) out.cooldown = std::nullopt;
    return out;
}

} // namespace ac::server
