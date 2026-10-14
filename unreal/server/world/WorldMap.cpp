// The world (WorldMap.h, docs/worlds.md): the world file and its checks,
// the pilots, their wallets, the edges and the pad flights.

#include "WorldMap.h"

#include "Flight.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <set>

namespace ac::server {

using region::Edge;

namespace {

using json = nlohmann::json;

std::optional<std::string> readFile(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return std::nullopt;
    std::string out;
    char buf[8192];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) out.append(buf, n);
    std::fclose(f);
    return out;
}

std::string folderOf(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

Edge opposite(Edge e) {
    switch (e) {
    case Edge::north: return Edge::south;
    case Edge::south: return Edge::north;
    case Edge::east: return Edge::west;
    case Edge::west: return Edge::east;
    }
    return e;
}

constexpr Edge kEdges[] = {Edge::north, Edge::east, Edge::south, Edge::west};

bool inSpan(const std::vector<region::Span>& spans, double along) {
    return std::any_of(spans.begin(), spans.end(), [&](const region::Span& s) { return along >= s.from && along <= s.to; });
}

std::string spanText(const region::Span& s) {
    char b[64];
    std::snprintf(b, sizeof b, "[%g, %g]", s.from, s.to);
    return b;
}

} // namespace

// MARK: - The world file

WorldLoad loadWorld(const std::string& path) {
    const std::optional<std::string> text = readFile(path);
    if (!text) return WorldLoad{std::nullopt, {path + ": cannot read"}};
    return loadWorld(*text, folderOf(path));
}

WorldLoad loadWorld(std::string_view text, const std::string& folder) {
    WorldLoad out;
    auto problem = [&](const std::string& p) { out.problems.push_back(p); };
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        problem("not a JSON object");
        return out;
    }
    WorldFile wf;
    for (const auto& [key, v] : root.items()) {
        if (key == "id" && v.is_string()) wf.id = v.get<std::string>();
        else if (key == "name" && v.is_string()) wf.name = v.get<std::string>();
        else if (key == "grid" && v.is_array()) {
            for (const json& row : v) {
                if (!row.is_array()) {
                    problem("grid: each row is a list of region file paths");
                    continue;
                }
                std::vector<std::string> cells;
                for (const json& c : row) {
                    if (c.is_string()) cells.push_back(c.get<std::string>());
                    else if (c.is_null()) cells.emplace_back();
                    else problem("grid: a cell is a region file path or null");
                }
                wf.grid.push_back(cells);
            }
        } else if (key == "start" && v.is_object()) {
            for (const auto& [k, sv] : v.items()) {
                if (k == "region" && sv.is_string()) wf.startRegion = sv.get<std::string>();
                else if (k == "pad" && sv.is_string()) wf.startPad = sv.get<std::string>();
                else problem("start." + k + ": unknown key or wrong type (region, pad: strings)");
            }
        } else {
            problem(key + ": unknown key or wrong type (id, name: strings; grid: rows of paths; start: {region, pad})");
        }
    }
    if (wf.id.empty()) problem("id: missing");
    if (wf.grid.empty()) problem("grid: missing or empty");
    if (wf.startRegion.empty() || wf.startPad.empty()) problem("start: missing region or pad");
    for (size_t r = 1; r < wf.grid.size(); r++)
        if (wf.grid[r].size() != wf.grid[0].size()) problem("grid: row " + std::to_string(r) + " is not as long as row 0");
    if (!out.problems.empty()) return out;

    LoadedWorld w;
    std::set<std::string> ids;
    for (size_t row = 0; row < wf.grid.size(); row++) {
        for (size_t col = 0; col < wf.grid[row].size(); col++) {
            const std::string& rel = wf.grid[row][col];
            if (rel.empty()) continue;
            const std::string where = "grid[" + std::to_string(row) + "][" + std::to_string(col) + "] " + rel;
            const std::string full = !rel.empty() && rel[0] == '/' ? rel : folder + "/" + rel;
            const std::optional<std::string> body = readFile(full);
            if (!body) {
                problem(where + ": cannot read");
                continue;
            }
            const region::Parsed parsed = region::parse(*body);
            if (!parsed.file) {
                for (const region::Problem& p : parsed.problems) problem(where + ": " + p.text());
                continue;
            }
            WorldRegion g;
            g.path = rel;
            g.column = int64_t(col);
            g.row = int64_t(row);
            g.file = *parsed.file;
            g.text = *body;
            g.built = region::build(g.file);
            for (const region::Problem& p : region::validate(g.file, g.built)) problem(where + ": " + p.text());
            if (g.file.pads.empty()) problem(where + ": no pad (every region needs one)");
            if (!ids.insert(g.file.id).second) problem(where + ": region id " + g.file.id + " used twice");
            w.regions.push_back(std::move(g));
        }
    }
    if (!out.problems.empty()) return out;

    // Neighbours' openings: every opening on a shared edge meets one of the
    // neighbour's.
    auto at = [&](int64_t col, int64_t row) -> const WorldRegion* {
        for (const WorldRegion& g : w.regions)
            if (g.column == col && g.row == row) return &g;
        return nullptr;
    };
    for (const WorldRegion& g : w.regions) {
        for (const Edge e : kEdges) {
            const int64_t dc = e == Edge::east ? 1 : e == Edge::west ? -1 : 0;
            const int64_t dr = e == Edge::south ? 1 : e == Edge::north ? -1 : 0;
            const WorldRegion* n = at(g.column + dc, g.row + dr);
            if (!n) continue;
            for (const region::Span& s : g.file.openings[size_t(e)]) {
                const auto& theirs = n->file.openings[size_t(opposite(e))];
                const bool meets = std::any_of(theirs.begin(), theirs.end(), [&](const region::Span& t) {
                    return std::min(s.to, t.to) - std::max(s.from, t.from) > 0;
                });
                if (!meets)
                    problem(g.file.id + " " + std::string(region::edgeName(e)) + " opening " + spanText(s) + " meets no opening of " +
                            n->file.id + "'s " + std::string(region::edgeName(opposite(e))) + " edge");
            }
        }
    }
    bool startFound = false;
    for (size_t i = 0; i < w.regions.size(); i++) {
        if (w.regions[i].file.id != wf.startRegion) continue;
        startFound = true;
        w.start = i;
        const auto& pads = w.regions[i].file.pads;
        const auto pad = std::find_if(pads.begin(), pads.end(), [&](const region::Place& p) { return p.name == wf.startPad; });
        if (pad == pads.end()) problem("start: region " + wf.startRegion + " has no pad " + wf.startPad);
        else w.startPad = size_t(pad - pads.begin());
    }
    if (!startFound) problem("start: no region " + wf.startRegion + " in the grid");
    if (!out.problems.empty()) return out;
    w.file = wf;
    out.world = std::move(w);
    return out;
}

// MARK: - The running world

WorldMap::WorldMap(const RegionConfig& config) {
    regions_.push_back(std::make_unique<Region>(config));
    builtBy_.resize(1);
    owed_.resize(1);
}

WorldMap::WorldMap(const LoadedWorld& world, const WorldParams& params, const RegionConfig& base)
    : world_(world), params_(params), start_(world.start) {
    for (const WorldRegion& g : world.regions) {
        RegionConfig c = base;
        c.map = MapChoice{g.file.style, g.file.size, 2};
        c.built = g.built;
        c.regionFile = g.text;
        c.wild = true;
        c.ai = false;
        c.state = std::nullopt;
        regions_.push_back(std::make_unique<Region>(c));
    }
    builtBy_.resize(regions_.size());
    owed_.resize(regions_.size());
    // What each well holds at the start is what it grows back to.
    for (const auto& r : regions_) {
        std::vector<int64_t> full;
        if (const auto& wells = r->simulation().state.wells)
            for (const Well& w : *wells) full.push_back(w.remaining);
        wellIdle_.emplace_back(full.size(), 0.0);
        wellLast_.push_back(full);
        wellFull_.push_back(std::move(full));
        ownPatches_.push_back(r->simulation().state.patches.size());
    }
    weather_ = std::make_unique<Weather>(WorldClock::weatherSeed(world.file.id));
    // The survey chips' dice: their own stream, off the world's id.
    chips_.seed(params_.chipSeed ? params_.chipSeed : uint64_t(WorldClock::weatherSeed(world.file.id)) ^ 0x5eedc41b5ULL);
    if (params_.weatherHold) weather_->hold(*params_.weatherHold);
    updateSky();
}

void WorldMap::setClock(double time) {
    clock_ = std::max(0.0, time);
    updateSky();
}

void WorldMap::updateSky() {
    if (!world_) return;
    weatherNow_ = weather_->sample(clock_);
    sky_.time = clock_;
    sky_.dayLength = params_.dayLength;
    sky_.heldHour = params_.heldHour;
    sky_.weatherSeed = uint32_t(weather_->seed());
    sky_.weatherHold = params_.weatherHold;
    sky_.weather = weatherNow_.state;
    // Every region at the world's hour: the core's sight at night
    // (`Rules::sightScale`) and the headlamp's beam read it.
    const double h = sky_.hour();
    for (auto& r : regions_) {
        GameState& st = r->simulation().state;
        st.dayLength = params_.dayLength;
        st.heldHour = h;
    }
}

std::optional<size_t> WorldMap::regionNamed(std::string_view id) const {
    if (!world_) return std::nullopt;
    for (size_t i = 0; i < world_->regions.size(); i++)
        if (world_->regions[i].file.id == id) return i;
    return std::nullopt;
}

std::optional<size_t> WorldMap::where(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    return it->second.region;
}

Region* WorldMap::regionOf(PilotId pilot) {
    const auto r = where(pilot);
    return r ? regions_[*r].get() : nullptr;
}

int64_t WorldMap::visit(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it == pilots_.end() ? 0 : it->second.visit;
}

bool WorldMap::flying(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    return it != pilots_.end() && it->second.flight.has_value();
}

std::optional<WorldMap::Wallet> WorldMap::wallet(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_) return std::nullopt;
    return it->second.wallet;
}

void WorldMap::bankIn(const PilotState& p, size_t r) {
    Player& side = regions_[r]->simulation().state.players[0];
    side.ore = p.wallet.ore;
    side.hydrogen = p.wallet.hydrogen;
}

void WorldMap::bankOut(PilotState& p, size_t r) {
    const Player& side = regions_[r]->simulation().state.players[0];
    p.wallet.ore = side.ore;
    p.wallet.hydrogen = side.hydrogen;
}

bool WorldMap::arrive(PilotId pilot, PilotState& p, size_t to, const Unit& u, Vec2 at, int64_t lastInput) {
    Region& r = *regions_[to];
    const bool free = !r.driven();
    r.join(pilot, lastInput);
    p.region = to;
    bankIn(p, to);
    // Its own levels and progression: into the simulation as it takes the unit.
    r.setLevels(pilot, p.record);
    r.setProgress(pilot, p.progress, ownBuildings(p, to));
    scan(p, to);
    // On open ground at `at`, or the nearest open spot round it.
    Unit v = u;
    const region::Built& places = *r.places();
    const Simulation& sim = r.simulation();
    v.position = places.ground(at);
    for (double rad = 0.5; rad <= 6 && !sim.pilotCanStand(v, v.position); rad += 0.5) {
        for (int k = 0; k < 16; k++) {
            const double a = k * 6.283185307179586 / 16;
            const Vec2 q = places.ground(at) + Vec2(std::cos(a), std::sin(a)) * rad;
            if (sim.pilotCanStand(v, q)) {
                v.position = q;
                break;
            }
        }
    }
    const int64_t id = r.admit(v);
    p.unit = id;
    p.last = places.regionAt(v.position);
    return free && r.take(pilot, id);
}

Unit WorldMap::startProspector() const {
    const Vec2 at = world_->regions[start_].built.pads[world_->startPad].at;
    const GameState& s = start().simulation().state;
    const OreDeposit* near = nullptr;
    for (const OreDeposit& d : s.patches) {
        const bool better = !near || (d.remaining > 0) > (near->remaining > 0) ||
                            ((d.remaining > 0) == (near->remaining > 0) && distance(d.position, at) < distance(near->position, at));
        if (better) near = &d;
    }
    const Vec2 to = near ? near->position - at : Vec2(1, 0);
    const double heading = length(to) > 1e-9 ? std::atan2(to.y, to.x) : 0;
    return start().simulation().freshUnit(UnitKind::prospector, 0, Pilot::player, at, heading);
}

bool WorldMap::join(PilotId pilot) {
    if (pilots_.count(pilot)) return false;
    PilotState p;
    p.region = start_;
    if (!world_) {
        pilots_[pilot] = p;
        return start().join(pilot);
    }
    p.wallet = Wallet{params_.startOre, params_.startHydrogen};
    // A new pilot: the first ten minutes (docs/contracts.md).
    p.firstStep = std::clamp(params_.firstStep, 0, WorldContracts::firstSteps);
    // Dev: every blueprint, a stack of each.
    if (params_.devBlueprints)
        for (const StructureKind k : Progression::blueprintKinds()) p.progress.giveBlueprint(k, WorldParams::devCopies);
    // Dev: regions been to.
    for (const std::string& id : params_.devSeen) p.progress.regions.insert(id);
    const Region& r = start();
    const Unit u = startProspector();
    PilotState& in = pilots_[pilot] = p;
    arrive(pilot, in, start_, u, r.places()->regionAt(u.position), -1);
    // Dev: its progression's XP at once.
    if (params_.devXp > 0) earn(pilot, params_.devXp);
    return true;
}

std::optional<PilotRecord> WorldMap::levels(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return std::nullopt;
    const Region& r = *regions_[it->second.region];
    return r.joined(pilot) ? r.levels(pilot) : it->second.record;
}

void WorldMap::setLevels(PilotId pilot, const std::optional<PilotRecord>& levels) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    it->second.record = levels;
    Region& r = *regions_[it->second.region];
    if (r.joined(pilot)) r.setLevels(pilot, levels);
}

std::optional<std::string> WorldMap::pick(PilotId pilot, UnitKind kind, Perk perk) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "Not joined";
    // A world's Prospector climbs the skill tree: no cards.
    if (world_) return std::string(kWorldPickRefusal);
    Region& r = *regions_[it->second.region];
    if (!r.joined(pilot)) return "Not joined";
    if (auto why = r.pick(pilot, kind, perk)) return why;
    it->second.record = r.levels(pilot);
    return std::nullopt;
}

void WorldMap::leave(PilotId pilot) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    Region& r = *regions_[it->second.region];
    it->second.record = r.levels(pilot);
    r.leave(pilot);
    // The side's bank was this pilot's: no one's now.
    if (world_ && r.pilots().empty()) bankIn(PilotState{}, it->second.region);
    // A kept pilot waits for its key (WorldMapSave.cpp).
    if (world_ && !it->second.key.empty()) park(it->second);
    pilots_.erase(it);
}

std::optional<std::string> WorldMap::fly(PilotId pilot, std::string_view regionId, std::string_view padName) {
    if (!world_) return "not a world";
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "not joined";
    PilotState& p = it->second;
    if (p.flight) return "already flying";
    const auto to = regionNamed(regionId);
    if (!to) return "no region " + std::string(regionId);
    const auto& pads = world_->regions[*to].built.pads;
    const auto pad = std::find_if(pads.begin(), pads.end(), [&](const region::Place& q) { return q.name == padName; });
    // Or a pad of its own there (P6): its Spacedock, a Homing beacon.
    const std::vector<OwnPad> own = ownPads(p);
    const auto mine = std::find_if(own.begin(), own.end(), [&](const OwnPad& o) { return o.region == *to && o.name == padName; });
    if (pad == pads.end() && mine == own.end()) return "no pad " + std::string(padName) + " in " + std::string(regionId);
    Region& here = *regions_[p.region];
    const auto unit = here.driving(pilot);
    if (!unit) return "nothing driven";
    const auto u = here.simulation().state.unit(*unit);
    if (!u || u->kind != UnitKind::prospector) return "only a Prospector flies";
    // The client's rule too (`net::padsOnGround`, `region::nearestPad`),
    // within Pad link's reach; or by a pad of its own.
    const double reach = padReach(p);
    const auto near = region::nearestPad(world_->regions[p.region].built.pads, u->position);
    bool atPad = near && near->distance <= reach;
    for (const OwnPad& o : own)
        if (o.region == p.region &&
            distance(o.at, u->position) <= std::max(reach, Rules::radius(o.kind) + WorldRoles::buildingPadReach))
            atPad = true;
    // Ringwalker: from anywhere, once every its cooldown.
    bool ringwalk = false;
    if (!atPad) {
        std::optional<double> cooldown;
        for (const SkillEffect& e : Progression::effects(Skill::ringwalker, p.progress.rank(Skill::ringwalker)))
            if (const auto* w = e.as<SkillEffect::Ringwalker>()) cooldown = w->cooldown;
        if (!cooldown) return "not at a pad";
        if (clock_ < p.ringwalkerReady) return "Ringwalker is ready in " + std::to_string(int64_t(std::ceil(p.ringwalkerReady - clock_))) + " s";
        ringwalk = true;
        p.ringwalkerReady = clock_ + *cooldown;
    }
    if (*to != p.region && regions_[*to]->driven()) {
        if (ringwalk) p.ringwalkerReady = 0;
        return "another pilot drives there";
    }
    // The flight's MH (F1): a base and a share a step across the grid.
    const WorldRegion& from = world_->regions[p.region];
    const WorldRegion& dest = world_->regions[*to];
    const int64_t price = ac::Flight::hydrogen(ac::Flight::steps(from.column, from.row, dest.column, dest.row));
    if (auto why = ac::Flight::refusal(price, here.simulation().state.players[0].hydrogen)) {
        if (ringwalk) p.ringwalkerReady = 0;
        return why;
    }
    bankOut(p, p.region);
    p.wallet.hydrogen -= price;
    Flight f;
    f.clocks = takeClocks(p.region, *unit);
    f.unit = *here.release(*unit);
    p.record = here.levels(pilot);
    f.to = *to;
    if (pad != pads.end()) f.pad = size_t(pad - pads.begin());
    else f.own = mine->at;
    f.landsAt = tick_ + std::max<int64_t>(1, std::llround(flightTime(p) / here.description().step));
    p.flight = f;
    p.last = std::nullopt;
    moves_.push_back(Moved{pilot, std::nullopt});
    feat(pilot, WorldXp::flightFeat);
    return std::nullopt;
}

std::optional<std::string> WorldMap::build(PilotId pilot, StructureKind kind, Vec2 at) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "Not joined";
    PilotState& p = it->second;
    if (p.flight) return "In flight";
    // A world builds from the pilot's blueprints, at its level floor, within
    // its building limits (docs/prospector-progression.md, "Buildings in the
    // MMO").
    if (world_)
        if (auto why = p.progress.buildRefusal(kind, realmCount(kind))) return why;
    // Nothing on a pad's plate (the client's ghost says the same).
    if (world_) {
        const auto spot = regions_[p.region]->simulation().snap(kind, at);
        if (spot && region::touchesPadPlate(world_->regions[p.region].built.pads, *spot, Rules::radius(kind)))
            return std::string(region::padPlateRefusal);
    }
    const int64_t id = regions_[p.region]->simulation().state.nextID;
    if (auto why = regions_[p.region]->build(pilot, kind, at)) return why;
    builtBy_[p.region][id] = p.key;
    // C2: placing it uses up one blueprint copy.
    if (world_) p.progress.useBlueprint(kind);
    // Paid from the side's bank: the wallet follows at once. Its skills
    // reach the new building.
    if (world_) {
        bankOut(p, p.region);
        pushProgress(pilot);
    }
    return std::nullopt;
}

std::optional<size_t> WorldMap::neighbour(size_t r, Edge e) const {
    if (!world_) return std::nullopt;
    const WorldRegion& g = world_->regions[r];
    const int64_t col = g.column + (e == Edge::east ? 1 : e == Edge::west ? -1 : 0);
    const int64_t row = g.row + (e == Edge::south ? 1 : e == Edge::north ? -1 : 0);
    for (size_t i = 0; i < world_->regions.size(); i++)
        if (world_->regions[i].column == col && world_->regions[i].row == row) return i;
    return std::nullopt;
}

void WorldMap::step() {
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<Work> work = world_ ? workBefore() : std::vector<Work>{};
    if (world_) armCitadels();
    for (auto& r : regions_) r->step();
    lastStepMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    tick_ += 1;
    if (!world_) return;
    const double dt = regions_[0]->description().step;
    clock_ += dt;
    updateSky();
    workAfter(work);
    realmTaxes();
    mercsStep(dt);
    realmNews();
    sites();
    contracts();
    missions();
    starfall();
    brood();
    respawns();
    rigs();
    ownPadsChanged();
    regrowWells(dt);
    for (auto& [id, p] : pilots_)
        if (!p.flight) {
            bankOut(p, p.region);
            p.record = regions_[p.region]->levels(id);
        }
    crossings();
    landings();
}

void WorldMap::idle(double seconds) {
    if (!world_ || !(seconds > 0)) return;
    clock_ += seconds;
    updateSky();
    sites();
    contracts();
    missions();
    starfall();
    respawns();
    rigs();
    ownPadsChanged();
    regrowWells(seconds);
    // The mercenaries' work as bookkeeping, the regions held (H2).
    mercsBook(clock_ - seconds, clock_);
    // The Brood's director on the world's clock: with no pilot near it
    // spawns and raids nothing, and a region's bugs go `idleDespawn` s
    // after the last pilot left (WorldBrood.cpp).
    brood();
}

double WorldMap::catchUp(double seconds) {
    if (!world_ || !(seconds > 0)) return 0;
    const double moved = std::min(seconds, params_.catchUpCap);
    setClock(clock_ + moved);
    sites();
    contracts();
    missions();
    starfall();
    respawns();
    rigs();
    ownPadsChanged();
    // No pilot on line: no group spawns, no raid is told, none piles up.
    brood();
    offlineWork(moved);
    return moved;
}

void WorldMap::offlineWork(double seconds) {
    // H2: the mercenaries' ore for the gap, to their pilots' wallets (taxed),
    // their fields emptied by as much, the wages at each dawn on the way.
    mercsBook(clock_ - seconds, clock_);
}

void WorldMap::crossings() {
    for (auto& [pilot, p] : pilots_) {
        if (p.flight) continue;
        Region& here = *regions_[p.region];
        const auto unit = here.driving(pilot);
        if (!unit) {
            p.last = std::nullopt;
            continue;
        }
        const auto u = here.simulation().state.unit(*unit);
        if (!u) continue;
        const region::Built& places = *here.places();
        const Vec2 at = places.regionAt(u->position);
        const std::optional<Vec2> before = p.last;
        p.last = at;
        // Out: it moved toward the edge this step, or the pilot steers it
        // there (pressed against the edge it stands still).
        const Vec2 moved = before ? at - *before : Vec2(0, 0);
        Vec2 wish(0, 0);
        if (const auto in = here.held(pilot)) {
            const double h = u->heading;
            wish = Vec2(std::cos(h), std::sin(h)) * in->walk.x + Vec2(-std::sin(h), std::cos(h)) * in->walk.y;
        }
        const Vec2 ext = places.extent();
        for (const Edge e : kEdges) {
            double gap = 0, along = 0;
            Vec2 out(0, 0);
            switch (e) {
            case Edge::north: gap = at.y; along = at.x; out = Vec2(0, -1); break;
            case Edge::south: gap = ext.y - at.y; along = at.x; out = Vec2(0, 1); break;
            case Edge::west: gap = at.x; along = at.y; out = Vec2(-1, 0); break;
            case Edge::east: gap = ext.x - at.x; along = at.y; out = Vec2(1, 0); break;
            }
            if (gap > params_.crossBand) continue;
            if (!(dot(moved, out) > 1e-6 || dot(wish, out) > 0.1)) continue;
            if (!inSpan(places.openings[size_t(e)], along)) continue;
            const auto to = neighbour(p.region, e);
            if (!to) continue;
            const region::Built& there = *regions_[*to]->places();
            if (!inSpan(there.openings[size_t(opposite(e))], along) || regions_[*to]->driven()) continue;
            const auto clocks = takeClocks(p.region, *unit);
            const Unit leaving = *here.release(*unit);
            const int64_t lastInput = here.lastInputTick(pilot);
            p.record = here.levels(pilot);
            here.leave(pilot);
            if (here.pilots().empty()) bankIn(PilotState{}, p.region);
            // Its inputs carry on from where they were.
            arrive(pilot, p, *to, leaving, there.edgePoint(opposite(e), along, params_.arriveDepth), lastInput);
            giveClocks(*to, *p.unit, clocks);
            firstVisit(pilot, world_->regions[*to].file.id);
            Region& now = *regions_[*to];
            p.visit += 1;
            net::RegionChange rc;
            rc.visit = p.visit;
            rc.region = now.description();
            rc.regionId = world_->regions[*to].file.id;
            rc.driven = now.driving(pilot);
            rc.serverTick = now.tick();
            rc.time = now.simulation().state.time;
            moves_.push_back(Moved{pilot, rc});
            break;
        }
    }
}

void WorldMap::landings() {
    for (auto& [pilot, p] : pilots_) {
        if (!p.flight || tick_ < p.flight->landsAt) continue;
        const Flight f = *p.flight;
        // Another pilot drives there now: it circles until the ground is free.
        if (f.to != p.region && regions_[f.to]->driven()) continue;
        p.flight = std::nullopt;
        const size_t from = p.region;
        Region& before = *regions_[from];
        const int64_t lastInput = before.lastInputTick(pilot);
        // Picked in flight: its seat there has them.
        if (before.joined(pilot)) p.record = before.levels(pilot);
        if (f.to != from) {
            before.leave(pilot);
            if (before.pilots().empty()) bankIn(PilotState{}, from);
        }
        const region::Built& there = world_->regions[f.to].built;
        Region& now = *regions_[f.to];
        arrive(pilot, p, f.to, f.unit, there.regionAt(landing(f)), lastInput);
        giveClocks(f.to, *p.unit, f.clocks);
        // A pad of its own pays no first landing.
        if (!f.own) firstLanding(pilot, world_->regions[f.to].file.id + "/" + there.pads[f.pad].name);
        if (f.to != from) firstVisit(pilot, world_->regions[f.to].file.id);
        if (f.to == from) {
            moves_.push_back(Moved{pilot, std::nullopt});
            continue;
        }
        p.visit += 1;
        net::RegionChange rc;
        rc.visit = p.visit;
        rc.region = now.description();
        rc.regionId = world_->regions[f.to].file.id;
        rc.driven = now.driving(pilot);
        rc.serverTick = now.tick();
        rc.time = now.simulation().state.time;
        moves_.push_back(Moved{pilot, rc});
    }
}

std::vector<Moved> WorldMap::takeMoves() {
    std::vector<Moved> out;
    out.swap(moves_);
    return out;
}

Snapshot WorldMap::snapshot(PilotId pilot, const InterestParams& params) {
    Region* r = regionOf(pilot);
    if (!r) return Snapshot{};
    Snapshot s = r->snapshot(pilot, params);
    s.game = net::visitGame(visit(pilot), s.game);
    s.sky = sky();
    if (world_) s.xp = pilots_[pilot].progress.xp;
    return s;
}

void WorldMap::deferred(PilotId pilot, const std::vector<int64_t>& ids) {
    if (Region* r = regionOf(pilot)) r->deferred(pilot, ids);
}

bool WorldMap::input(PilotId pilot, const PilotInput& in) {
    // Made before the client heard of its last region change: it steered
    // the unit in the region it left (a walk held into the edge), so it
    // does not move it on here.
    if (world_ && in.visit < visit(pilot)) return false;
    Region* r = regionOf(pilot);
    return r && r->input(pilot, in);
}

RegionDescription WorldMap::description(PilotId pilot) const {
    const auto r = where(pilot);
    return regions_[r ? *r : start_]->description();
}

std::optional<net::WorldMessage> WorldMap::worldMessage(PilotId pilot) const {
    if (!world_) return std::nullopt;
    net::WorldMessage m;
    for (const WorldRegion& g : world_->regions) {
        m.columns = std::max(m.columns, g.column + 1);
        m.rows = std::max(m.rows, g.row + 1);
        net::WorldMessage::Region out;
        out.id = g.file.id;
        out.name = g.file.name;
        out.style = g.file.style;
        out.size = g.file.size;
        out.column = g.column;
        out.row = g.row;
        for (const region::Place& p : g.file.pads) out.pads.push_back({p.name, p.at});
        m.regions.push_back(out);
    }
    const auto it = pilots_.find(pilot);
    // Its own pads (P6), in region cells like the file's.
    if (it != pilots_.end())
        for (const OwnPad& o : ownPads(it->second))
            m.regions[o.region].pads.push_back({o.name, world_->regions[o.region].built.regionAt(o.at), o.kind});
    if (it != pilots_.end()) {
        m.here = int64_t(it->second.region);
        if (it->second.flight) m.flyingTo = int64_t(it->second.flight->to);
    }
    return m;
}

} // namespace ac::server
