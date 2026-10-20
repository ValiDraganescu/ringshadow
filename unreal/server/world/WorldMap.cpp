// The world (WorldMap.h, docs/worlds.md): the world file and its checks,
// the pilots, their wallets, the edges and the pad flights.

#include "WorldMap.h"

#include "Shuttle.h"

#include "Flight.h"
#include "TerrainField.h"

#ifndef JSON_NOEXCEPTION
#define JSON_NOEXCEPTION 1
#endif
#include "json.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <mutex>
#include <set>
#include <thread>

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

/// Where the ground of `a`'s edge `e` and its neighbour `b`'s facing edge
/// differ by more than a centimetre: both heights there (nil: they meet).
std::optional<std::string> edgeMismatch(const WorldRegion& a, const WorldRegion& b, Edge e) {
    const TerrainField fa(a.built.map), fb(b.built.map);
    const Vec2 ea = a.built.extent();
    const double length = e == Edge::north || e == Edge::south ? ea.x : ea.y;
    for (double t = 0; t <= length + 1e-9; t += 0.25) {
        const Vec2 pa = a.built.ground(a.built.edgePoint(e, t, 0));
        const Vec2 pb = b.built.ground(b.built.edgePoint(opposite(e), t, 0));
        const double ha = fa.height(pa), hb = fb.height(pb);
        if (std::fabs(ha - hb) > 0.01) {
            char buf[160];
            std::snprintf(buf, sizeof buf, "at %g along it, %.2f against %.2f cells high", t, ha, hb);
            return std::string(buf);
        }
    }
    return std::nullopt;
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
            for (const region::Problem& p : region::validate(g.file, g.built, g.siteReach)) problem(where + ": " + p.text());
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
                // W4: with walls, a passage lines up exactly on both sides.
                else if ((g.file.border || n->file.border) &&
                         std::none_of(theirs.begin(), theirs.end(), [&](const region::Span& t) { return t == s; }))
                    problem(g.file.id + " " + std::string(region::edgeName(e)) + " opening " + spanText(s) + " does not line up with " +
                            n->file.id + "'s " + std::string(region::edgeName(opposite(e))) + " openings: the same span and level on both sides");
            }
            // W4: the ground meets along the shared edge (the walls and the
            // passages' floors), so the world is one piece. Once a pair.
            if ((g.file.border || n->file.border) && (e == Edge::east || e == Edge::south)) {
                if (const auto gap = edgeMismatch(g, *n, e))
                    problem(g.file.id + " " + std::string(region::edgeName(e)) + " edge and " + n->file.id + "'s " +
                            std::string(region::edgeName(opposite(e))) + " edge do not meet: " + *gap);
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
    kills_.resize(1);
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
    kills_.resize(regions_.size());
    owed_.resize(regions_.size());
    // The threads the regions with a pilot step on (`WorldParams::stepThreads`).
    int64_t threads = params_.stepThreads;
    if (threads == 0)
        if (const char* env = std::getenv("AUTOCRAFT_STEP_THREADS")) threads = std::atoll(env);
    const int64_t cores = std::max<int64_t>(1, int64_t(std::thread::hardware_concurrency()));
    if (threads < 0) threads = std::min<int64_t>(8, std::max<int64_t>(1, cores / 2));
    stepThreads_ = size_t(std::min<int64_t>(threads, cores));
    stepAlways_ = params_.stepAlways;
    if (const char* env = std::getenv("AUTOCRAFT_STEP_ALWAYS")) stepAlways_ = stepAlways_ || std::atoi(env) != 0;
    if (stepThreads_ > 1 && regions_.size() >= 2) stepPool_ = std::make_shared<StepPool>(std::min(stepThreads_, regions_.size()) - 1);
    // The sites' reach from the load: a pilot's first board or contract
    // would build each region's walking grid for it in a tick (12 ms).
    siteReach_.resize(regions_.size());
    for (size_t r = 0; r < regions_.size(); r++) {
        const WorldRegion& g = world.regions[r];
        if (!g.built.sites.empty() && g.siteReach.size() == g.built.sites.size()) siteReach_[r] = g.siteReach;
    }
    // What each well holds at the start is what it grows back to.
    for (const auto& r : regions_) {
        std::vector<int64_t> full;
        if (const auto& wells = r->simulation().state.wells)
            for (const Well& w : *wells) full.push_back(w.remaining);
        wellIdle_.emplace_back(full.size(), 0.0);
        wellLast_.push_back(full);
        wellFull_.push_back(std::move(full));
        ownPatches_.push_back(r->simulation().state.patches.size());
        // E1: the deposits grow back on the world's clock (`regrowVeins`),
        // not by the game's rule.
        r->simulation().regrows = false;
    }
    weather_ = std::make_unique<Weather>(WorldClock::weatherSeed(world.file.id));
    // The survey chips' dice: their own stream, off the world's id.
    chips_.seed(params_.chipSeed ? params_.chipSeed : uint64_t(WorldClock::weatherSeed(world.file.id)) ^ 0x5eedc41b5ULL);
    if (params_.weatherHold) weather_->hold(*params_.weatherHold);
    // The canyon zones' games (Z2): each waits for its first wave.
    if (params_.canyons)
        for (size_t r = 0; r < regions_.size(); r++) {
            const auto& places = regions_[r]->places();
            if (!places) continue;
            for (size_t k = 0; k < places->zones.size(); k++) {
                const region::Zone& z = places->zones[k];
                Canyon c;
                c.region = r;
                c.zone = k;
                c.id = z.id;
                c.name = z.name;
                c.next = WorldCanyon::firstWait;
                // The prize's middle (its deposits, else its wells), and the
                // zone's reach round it: its outline's farthest corner (Z4:
                // pilots build anywhere in it).
                std::vector<Vec2> prize;
                for (const int64_t i : z.prizeDeposits) prize.push_back(places->map.deposits[size_t(i)].position);
                if (prize.empty())
                    for (const int64_t i : z.prizeWells) prize.push_back(places->map.wells[size_t(i)].position);
                for (const Vec2 p : prize) c.middle = c.middle + p / double(prize.size());
                for (const Vec2 o : z.outline) c.reach = std::max(c.reach, distance(o, c.middle));
                canyons_.push_back(c);
            }
        }
    updateSky();
}

std::optional<net::Sky> WorldMap::sky() const {
    if (!world_) return std::nullopt;
    net::Sky out = sky_;
    out.pilots.clear();
    for (const auto& [id, p] : pilots_) {
        if (p.key.empty()) continue;
        const auto u = !p.flight && p.region < regions_.size() ? regions_[p.region]->driving(id) : std::nullopt;
        net::Sky::Human h{callsign(p.key), int64_t(p.region), u.value_or(0), std::nullopt};
        // Aboard: its flight for the others to watch (from the region it left).
        if (const auto& f = p.flight) {
            h.region = int64_t(f->from);
            net::Sky::Human::Ferry ship;
            ship.boarded = world_->regions[f->from].built.regionAt(f->boarded);
            ship.to = int64_t(f->to);
            ship.at = world_->regions[f->to].built.regionAt(f->at);
            ship.plan = f->plan;
            ship.elapsed = clock_ - f->started;
            h.ferry = ship;
        }
        out.pilots.push_back(h);
    }
    return out;
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
    return it != pilots_.end() && (it->second.flight.has_value() || riding(pilot));
}

std::optional<WorldMap::Wallet> WorldMap::wallet(PilotId pilot) const {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end() || !world_) return std::nullopt;
    return it->second.wallet;
}

// W5: each pilot's own bank in the region (Region::bank), another human
// driving there or not; a pilot not seated there (kept off line) only where
// no one drives.
void WorldMap::bankIn(const PilotState& p, size_t r) {
    for (const auto& [id, q] : pilots_)
        if (&q == &p && regions_[r]->joined(id)) {
            regions_[r]->setBank(id, p.wallet.ore, p.wallet.hydrogen);
            return;
        }
    Simulation& sim = regions_[r]->simulation();
    if (sim.pilot) return;
    Player& side = sim.state.players[0];
    side.ore = p.wallet.ore;
    side.hydrogen = p.wallet.hydrogen;
}

void WorldMap::bankOut(PilotState& p, size_t r) {
    for (const auto& [id, q] : pilots_)
        if (&q == &p && regions_[r]->joined(id)) {
            if (const auto b = regions_[r]->bank(id)) {
                p.wallet.ore = b->first;
                p.wallet.hydrogen = b->second;
            }
            return;
        }
    const Simulation& sim = regions_[r]->simulation();
    if (sim.pilot) return;
    const Player& side = sim.state.players[0];
    p.wallet.ore = side.ore;
    p.wallet.hydrogen = side.hydrogen;
}

Vec2 openGround(const Simulation& sim, const Unit& u, Vec2 at, double reach) {
    if (sim.pilotCanStand(u, at)) return at;
    for (double rad = 0.5; rad <= reach; rad += 0.5) {
        const int around = std::max(16, int(rad * 6));
        for (int k = 0; k < around; k++) {
            const double a = k * 6.283185307179586 / around;
            const Vec2 q = at + Vec2(std::cos(a), std::sin(a)) * rad;
            if (sim.pilotCanStand(u, q)) return q;
        }
    }
    return at;
}

bool WorldMap::arrive(PilotId pilot, PilotState& p, size_t to, const Unit& u, Vec2 at, int64_t lastInput, bool exact) {
    Region& r = *regions_[to];
    // W5: every human drives its own Prospector, another pilot here or not.
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
    v.position = exact ? at : openGround(sim, v, places.ground(at), 6);
    const int64_t id = r.admit(v);
    p.unit = id;
    p.last = places.regionAt(v.position);
    return r.take(pilot, id);
}

Unit WorldMap::startUnit(UnitKind kind) const {
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
    // C5: just off the pad's plate toward that ore (the Landing Pad is
    // 17 x 17 under the walk-in Dropship), so a Citadel fits by the pad
    // from where it stands; `arrive` steps it onto open ground nearby.
    const Vec2 off = at + Vec2(std::cos(heading), std::sin(heading)) * (region::padReach + 1.5);
    return start().simulation().freshUnit(kind, 0, Pilot::player, off, heading);
}

bool WorldMap::join(PilotId pilot) {
    if (pilots_.count(pilot)) return false;
    joinOrder_[pilot] = ++joins_;
    PilotState p;
    p.region = start_;
    if (!world_) {
        pilots_[pilot] = p;
        return start().join(pilot);
    }
    p.wallet = Wallet{params_.startOre, params_.startHydrogen};
    // A new pilot: the first ten minutes (docs/contracts.md).
    p.firstStep = std::clamp(params_.firstStep, 0, WorldContracts::firstSteps);
    // Dev: every blueprint, a stack of each (C2: learned, the stack to
    // learn again with).
    if (params_.devBlueprints)
        for (const StructureKind k : Progression::blueprintKinds()) {
            p.progress.learned.insert(k);
            p.progress.giveBlueprint(k, WorldParams::devCopies);
        }
    // Dev: regions been to.
    for (const std::string& id : params_.devSeen) p.progress.regions.insert(id);
    const Region& r = start();
    const Unit u = startUnit(pilotKind(p));
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
    joinOrder_.erase(pilot);
}

std::optional<std::string> WorldMap::fly(PilotId pilot, std::string_view regionId, std::string_view padName) {
    if (!world_) return "not a world";
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "not joined";
    PilotState& p = it->second;
    if (p.flight || riding(pilot)) return "In flight";
    const auto to = regionNamed(regionId);
    if (!to) return "No region " + std::string(regionId);
    const WorldRegion& dest = world_->regions[*to];
    // Only to a region the pilot has been to (F2).
    if (!seen(p, *to)) return "Not discovered yet: " + dest.file.name + ". Go there on foot first";
    const std::vector<LandingPad> pads = landingPads();
    const auto target = std::find_if(pads.begin(), pads.end(), [&](const LandingPad& o) { return o.region == *to && o.name == padName; });
    if (target == pads.end()) {
        const bool any = std::any_of(pads.begin(), pads.end(), [&](const LandingPad& o) { return o.region == *to; });
        if (!any) return "No Landing Pad in " + dest.file.name + ": build one there first";
        return "No " + std::string(padName) + " in " + dest.file.name;
    }
    Region& here = *regions_[p.region];
    Simulation& sim = here.simulation();
    const auto unit = here.driving(pilot);
    const auto u = unit ? sim.state.unit(*unit) : std::nullopt;
    if (!u) return "Nothing driven";
    // C4: from the take-off button of the Dropship it is in, the ramp shut.
    const Shuttle* ship = sim.aboard(*u);
    const auto trip = ship ? std::find_if(trips_.begin(), trips_.end(), [&](const ShuttleTrip& t) { return t.region == p.region && t.id == ship->id; })
                           : trips_.end();
    if (!ship || trip == trips_.end()) return std::string("Board the Dropship first: call it at a Landing Pad");
    if (auto why = sim.takeOffRefusal(*ship, *u)) return why;
    if (target->region == trip->region && target->building == trip->pad) return "At " + target->name + " already";
    // The flight's MH (F1): a base and a share a step across the grid; half
    // from a Spacedock, nothing with Ringwalker (F2); half again while the
    // realm's Beacon burns (R2). The one who pressed pays.
    const WorldRegion& origin = world_->regions[p.region];
    const bool ringwalker = p.progress.rank(Skill::ringwalker) > 0;
    const int64_t price = ac::Flight::price(ac::Flight::steps(origin.column, origin.row, dest.column, dest.row),
                                            trip->padKind == StructureKind::spacedock, ringwalker,
                                            projectOn(RealmProject::Kind::beacon));
    bankOut(p, p.region);
    if (auto why = ac::Flight::refusal(price, p.wallet.hydrogen)) return why;
    p.wallet.hydrogen -= price;
    bankIn(p, p.region);
    if (auto why = launch(*trip, *unit, *target, ferryPlan(p, *to))) {
        // Not lifted after all: the fare back.
        bankOut(p, p.region);
        p.wallet.hydrogen += price;
        bankIn(p, p.region);
        return why;
    }
    feat(pilot, WorldXp::flightFeat);
    return std::nullopt;
}

std::optional<std::string> WorldMap::build(PilotId pilot, StructureKind kind, Vec2 at) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return "Not joined";
    PilotState& p = it->second;
    if (p.flight || riding(pilot)) return "In flight";
    // A world builds what the pilot has learned (C2: no copy used up), at its
    // level floor, within its building limits (docs/prospector-progression.md, "Buildings in the
    // MMO").
    if (world_)
        if (auto why = p.progress.buildRefusal(kind, limitCount(kind, p.region))) return why;
    // Nothing on a pad's plate (the client's ghost says the same).
    if (world_) {
        const auto spot = regions_[p.region]->simulation().snap(kind, at);
        if (spot && region::touchesPadPlate(world_->regions[p.region].built.pads, *spot, Rules::radius(kind)))
            return std::string(region::padPlateRefusal);
    }
    const int64_t id = regions_[p.region]->simulation().state.nextID;
    if (auto why = regions_[p.region]->build(pilot, kind, at)) return why;
    builtBy_[p.region][id] = p.key;
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

/// The workers a world steps its busy regions on, made once. A dispatch
/// wakes as many workers as there are jobs past the caller's; each that
/// claims a place takes jobs by index, and the caller returns only when all
/// that claimed one are through (so the job is never run after it changes).
/// A worker that wakes without a place goes back to sleep without running
/// anything. Between dispatches the workers sleep on a condition. What one
/// dispatch costs is timed once, when the pool is made: two empty jobs (one
/// worker woken and heard back from) after the workers slept 2 ms, the
/// mean of 16 after 4 (a tick's wake: the slow ones count): `wakeUs`.
struct StepPool {
    explicit StepPool(size_t workers) {
        for (size_t k = 0; k < workers; k++) threads_.emplace_back([this] { work(); });
        const std::function<void(size_t)> nothing = [](size_t) {};
        double took = 0;
        for (int k = 0; k < 20; k++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            const auto t0 = std::chrono::steady_clock::now();
            run(2, nothing);
            if (k >= 4) took += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        }
        wakeUs = took / 16;
    }
    ~StepPool() {
        {
            std::lock_guard<std::mutex> g(lock_);
            quit_ = true;
        }
        wake_.notify_all();
        for (std::thread& t : threads_) t.join();
    }
    StepPool(const StepPool&) = delete;
    StepPool& operator=(const StepPool&) = delete;

    /// Runs `job(0)` to `job(count - 1)` once each, on the workers and the
    /// caller.
    void run(size_t count, const std::function<void(size_t)>& job) {
        if (count == 0) return;
        const size_t wanted = std::min(threads_.size(), count - 1);
        {
            std::lock_guard<std::mutex> g(lock_);
            job_ = &job;
            count_ = count;
            next_.store(0);
            wanted_ = wanted;
            claimed_ = 0;
            acked_ = 0;
            generation_ += 1;
        }
        for (size_t k = 0; k < wanted; k++) wake_.notify_one();
        take(job, count);
        std::unique_lock<std::mutex> g(lock_);
        // Every place handed out is claimed and acknowledged (a worker busy
        // when notified sees the new generation as it comes back to wait).
        done_.wait(g, [&] { return acked_ == wanted_; });
        job_ = nullptr;
    }

    /// One dispatch's cost, microseconds (the break-even of a region's step).
    double wakeUs = 0;

private:
    void take(const std::function<void(size_t)>& job, size_t count) {
        for (size_t i = next_.fetch_add(1); i < count; i = next_.fetch_add(1)) job(i);
    }
    void work() {
        uint64_t seen = 0;
        for (;;) {
            const std::function<void(size_t)>* job = nullptr;
            size_t count = 0;
            {
                std::unique_lock<std::mutex> g(lock_);
                wake_.wait(g, [&] { return quit_ || generation_ != seen; });
                if (quit_) return;
                seen = generation_;
                if (claimed_ >= wanted_) continue;
                claimed_ += 1;
                job = job_;
                count = count_;
            }
            take(*job, count);
            std::lock_guard<std::mutex> g(lock_);
            if (++acked_ == wanted_) done_.notify_one();
        }
    }

    std::vector<std::thread> threads_;
    std::mutex lock_;
    std::condition_variable wake_, done_;
    const std::function<void(size_t)>* job_ = nullptr;
    size_t count_ = 0;
    std::atomic<size_t> next_{0};
    size_t wanted_ = 0, claimed_ = 0, acked_ = 0;
    uint64_t generation_ = 0;
    bool quit_ = false;
};

namespace {

/// Steps `r`, keeping how long it took (`Region::stepCost`).
void timedStep(Region& r) {
    const auto t0 = std::chrono::steady_clock::now();
    r.step();
    r.stepCostBefore = r.stepCost;
    r.stepCost = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
}

} // namespace

double WorldMap::stepWakeUs() const { return stepPool_ ? stepPool_->wakeUs : 0; }

void WorldMap::step() {
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<Work> work = world_ ? workBefore() : std::vector<Work>{};
    if (world_) armCitadels();
    if (world_) installRepairPayers();
    // With `WorldParams::stepThreads`: the regions with a pilot whose last
    // two steps each took longer than a dispatch costs (`StepPool::wakeUs`;
    // with `stepAlways`, every one), on the pool when there are two or more
    // (each touches only its own simulation, so the order does not matter;
    // the longest first, for the caller, already awake); every other region
    // here, as before.
    if (stepPool_) {
        stepJobs_.clear();
        for (auto& r : regions_)
            if (r->seated() && (stepAlways_ || std::min(r->stepCost, r->stepCostBefore) > stepPool_->wakeUs)) stepJobs_.push_back(r.get());
        if (stepJobs_.size() >= 2) {
            std::stable_sort(stepJobs_.begin(), stepJobs_.end(), [](const Region* a, const Region* b) { return a->stepCost > b->stepCost; });
            const std::function<void(size_t)> job = [this](size_t i) { timedStep(*stepJobs_[i]); };
            stepPool_->run(stepJobs_.size(), job);
            pooledSteps_ += 1;
            for (auto& r : regions_)
                if (std::find(stepJobs_.begin(), stepJobs_.end(), r.get()) == stepJobs_.end()) {
                    if (r->seated()) timedStep(*r);
                    else r->step();
                }
        } else {
            for (auto& r : regions_) {
                if (r->seated()) timedStep(*r);
                else r->step();
            }
        }
    } else {
        for (auto& r : regions_) r->step();
    }
    lastStepMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    tick_ += 1;
    if (!world_) return;
    const double dt = regions_[0]->description().step;
    clock_ += dt;
    updateSky();
    workAfter(work);
    realmTaxes();
    mulesStep();
    mercsStep(dt);
    frontierStep(dt);
    realmNews();
    sites();
    contracts();
    goals();
    missions();
    starfall();
    brood();
    canyonsStep();
    projectsStep();
    standingsStep();
    huntStep();
    respawns();
    rigs();
    landingPadsChanged();
    regrowWells(dt);
    regrowVeins(clock_ - dt, clock_);
    for (auto& [id, p] : pilots_)
        if (!p.flight) {
            bankOut(p, p.region);
            p.record = regions_[p.region]->levels(id);
        }
    shuttlesStep();
    crossings();
    landings();
}

void WorldMap::idle(double seconds) {
    if (!world_ || !(seconds > 0)) return;
    clock_ += seconds;
    updateSky();
    sites();
    contracts();
    goals();
    missions();
    starfall();
    respawns();
    rigs();
    landingPadsChanged();
    regrowWells(seconds);
    regrowVeins(clock_ - seconds, clock_);
    // The mercenaries' work as bookkeeping, the regions held (H2).
    mercsBook(clock_ - seconds, clock_);
    // P1: the frontier pilots' work, booked (WorldFrontier.cpp).
    frontierIdle(seconds);
    // The Brood's director on the world's clock: with no pilot near it
    // spawns and raids nothing, and a region's bugs go `idleDespawn` s
    // after the last pilot left (WorldBrood.cpp). The canyons' waves wait;
    // their FAILED and HELD ends come on the clock (WorldCanyon.cpp).
    brood();
    canyonsStep();
    projectsStep();
    standingsStep();
    // N4: a hunt with nobody on line runs out on the clock.
    huntStep();
}

double WorldMap::catchUp(double seconds) {
    if (!world_ || !(seconds > 0)) return 0;
    const double moved = std::min(seconds, params_.catchUpCap);
    setClock(clock_ + moved);
    // S4: a week that ended while the world was down ends with what it
    // counted; the gap's work counts to the week now.
    standingsStep();
    huntStep();
    sites();
    contracts();
    goals();
    missions();
    starfall();
    respawns();
    rigs();
    landingPadsChanged();
    // No pilot on line: no group spawns, no raid is told, none piles up.
    brood();
    canyonsStep();
    offlineWork(moved);
    regrowVeins(clock_ - moved, clock_);
    // R2: after the mercenaries' booked work (a Dock Crew that ran out in
    // the gap counts until its end).
    projectsStep();
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
        // C4: aboard a Dropship (its bay may hang over the edge): no crossing.
        if (!here.simulation().state.shuttles.empty() && here.simulation().aboard(*u)) continue;
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
        // W4: a region with open edges (`MapDefinition::edges`) lets the unit
        // walk past its edge in a gap: it crosses once its centre is past,
        // and comes in at that same point of the world, so it walks on.
        const bool seamless = here.simulation().map && here.simulation().map->edges;
        for (const Edge e : kEdges) {
            double gap = 0, along = 0;
            Vec2 out(0, 0);
            switch (e) {
            case Edge::north: gap = at.y; along = at.x; out = Vec2(0, -1); break;
            case Edge::south: gap = ext.y - at.y; along = at.x; out = Vec2(0, 1); break;
            case Edge::west: gap = at.x; along = at.y; out = Vec2(-1, 0); break;
            case Edge::east: gap = ext.x - at.x; along = at.y; out = Vec2(1, 0); break;
            }
            if (seamless ? !(gap < 0) : gap > params_.crossBand) continue;
            if (!seamless && !(dot(moved, out) > 1e-6 || dot(wish, out) > 0.1)) continue;
            if (!inSpan(places.openings[size_t(e)], along)) continue;
            const auto to = neighbour(p.region, e);
            if (!to) continue;
            const region::Built& there = *regions_[*to]->places();
            if (!inSpan(there.openings[size_t(opposite(e))], along)) continue;
            // C4: its Mule crosses with its rider (the Prospector's clocks go along).
            const bool mule = drivesMule(pilot, p) && p.unit;
            const auto clocks = takeClocks(p.region, mule ? *p.unit : *unit);
            const std::optional<Unit> rider = mule ? here.release(*p.unit) : std::nullopt;
            const Unit leaving = *here.release(*unit);
            const int64_t lastInput = here.lastInputTick(pilot);
            const std::optional<Region::Inputs> inputs = here.inputs(pilot);
            p.record = here.levels(pilot);
            here.leave(pilot);
            if (here.pilots().empty()) bankIn(PilotState{}, p.region);
            // Its inputs carry on from where they were: seamless, the ones
            // waiting and the one in use too, at the same point of the world.
            const Vec2 in = seamless ? there.edgePoint(opposite(e), along, -gap) : there.edgePoint(opposite(e), along, params_.arriveDepth);
            if (rider) arriveMule(pilot, p, *to, leaving, *rider, in, lastInput);
            else arrive(pilot, p, *to, leaving, in, lastInput);
            p.walkedIn = seamless;
            if (seamless && inputs) regions_[*to]->carryOn(pilot, *inputs);
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

bool WorldMap::devTransfer(PilotId pilot, size_t to, Vec2 at) {
    const auto it = pilots_.find(pilot);
    if (!world_ || it == pilots_.end() || to >= regions_.size()) return false;
    PilotState& p = it->second;
    if (p.flight || !p.unit || p.region == to) return false;
    Region& here = *regions_[p.region];
    if (!here.simulation().state.unit(*p.unit)) return false;
    // As a crossing hands it over (`crossings`), to a point of its own.
    const auto clocks = takeClocks(p.region, *p.unit);
    const Unit leaving = *here.release(*p.unit);
    const int64_t lastInput = here.lastInputTick(pilot);
    p.record = here.levels(pilot);
    here.leave(pilot);
    if (here.pilots().empty()) bankIn(PilotState{}, p.region);
    arrive(pilot, p, to, leaving, regions_[to]->places()->regionAt(at), lastInput);
    p.walkedIn = false;
    giveClocks(to, *p.unit, clocks);
    firstVisit(pilot, world_->regions[to].file.id);
    Region& now = *regions_[to];
    p.visit += 1;
    net::RegionChange rc;
    rc.visit = p.visit;
    rc.region = now.description();
    rc.regionId = world_->regions[to].file.id;
    rc.driven = now.driving(pilot);
    rc.serverTick = now.tick();
    rc.time = now.simulation().state.time;
    moves_.push_back(Moved{pilot, rc});
    return true;
}

void WorldMap::landings() {
    for (auto& [pilot, p] : pilots_) {
        if (!p.flight) continue;
        Flight& f = *p.flight;
        // The approach: the pilot into the target region, nothing driven yet
        // (its snapshots there follow the target pad). W5: another pilot
        // driving there is no matter.
        if (!f.switched && tick_ >= f.switchAt) {
            Region& before = *regions_[p.region];
            const int64_t lastInput = before.lastInputTick(pilot);
            if (before.joined(pilot)) p.record = before.levels(pilot);
            bankOut(p, p.region);
            before.leave(pilot);
            if (before.pilots().empty()) bankIn(PilotState{}, p.region);
            Region& now = *regions_[f.to];
            now.join(pilot, lastInput);
            p.region = f.to;
            bankIn(p, f.to);
            now.watch(pilot, f.at);
            f.switched = true;
            p.walkedIn = false;
            regionChanged(pilot, p, f.to);
            continue;
        }
        if (!f.switched || tick_ < f.landsAt) continue;
        // The touch down: the Prospector on the target pad.
        const Flight done = f;
        p.flight = std::nullopt;
        Region& now = *regions_[done.to];
        const int64_t lastInput = now.lastInputTick(pilot);
        if (now.joined(pilot)) p.record = now.levels(pilot);
        now.watch(pilot, std::nullopt);
        arrive(pilot, p, done.to, done.unit, now.places()->regionAt(done.at), lastInput);
        giveClocks(done.to, *p.unit, done.clocks);
        firstLanding(pilot, world_->regions[done.to].file.id + "/" + done.pad);
        if (done.to != done.from) firstVisit(pilot, world_->regions[done.to].file.id);
        // The same region (a hop, or the switch made): the World message
        // again, its unit driven in the next snapshot.
        moves_.push_back(Moved{pilot, std::nullopt});
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
    if (world_) {
        s.xp = pilots_[pilot].progress.xp;
        if (s.sky) s.sky->starfalls = fallsFor(pilots_[pilot].region);
    }
    return s;
}

std::vector<net::Sky::Starfall> WorldMap::fallsFor(size_t region) const {
    // Every fall the world had stays (its crater, its deposit), and the sky
    // rides in every part of every snapshot: all of them made each part
    // ~1.1 KB, a snapshot of 16 entities 16 parts, more than a tick's budget
    // ever let through, so no snapshot was ever whole and the client's world
    // stood still, then came up empty on the next entry (the user's, 61
    // falls, 2026-10-15). A pilot is told the falls coming or just down (the
    // radio, the streaks, any region) and the craters of its own region (the
    // ones drawn), the newest first, at most `skyFallsMost`.
    const double now = clock_;
    std::vector<net::Sky::Starfall> out;
    for (auto it = falls_.rbegin(); it != falls_.rend() && out.size() < skyFallsMost; ++it) {
        const Fall& f = *it;
        const bool news = now < f.impact + 30.0;
        if (!news && f.region != region) continue;
        out.push_back(net::Sky::Starfall{f.id, int64_t(f.region), f.at, f.impact, f.from, f.patch});
    }
    std::reverse(out.begin(), out.end());
    return out;
}

void WorldMap::beginSnapshots() {
    for (auto& r : regions_) r->beginSnapshots();
}

void WorldMap::endSnapshots() {
    for (auto& r : regions_) r->endSnapshots();
}

void WorldMap::deferred(PilotId pilot, const std::vector<int64_t>& ids) {
    if (Region* r = regionOf(pilot)) r->deferred(pilot, ids);
}

bool WorldMap::input(PilotId pilot, const PilotInput& in) {
    // Made before the client heard of its last region change: it steered
    // the unit in the region it left (a walk held into the edge), so it
    // does not move it on here.
    // (After an edge crossing, the visit before still steers: the same unit
    // walking on through, W4.)
    if (world_) {
        const auto it = pilots_.find(pilot);
        const bool walked = it != pilots_.end() && it->second.walkedIn && in.visit == it->second.visit - 1;
        if (in.visit < visit(pilot) && !walked) return false;
    }
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
        out.ground = g.file.ground;
        out.column = g.column;
        out.row = g.row;
        for (const region::Place& p : g.file.pads) out.pads.push_back({p.name, p.at});
        out.nest = g.file.nest;
        m.regions.push_back(out);
    }
    const auto it = pilots_.find(pilot);
    // The realm's Landing Pads (F2), in region cells like the file's.
    for (const LandingPad& o : landingPads())
        m.regions[o.region].pads.push_back({o.name, world_->regions[o.region].built.regionAt(o.at), o.kind});
    if (it != pilots_.end()) {
        m.here = int64_t(it->second.region);
        if (const auto& f = it->second.flight) {
            m.flyingTo = int64_t(f->to);
            net::WorldMessage::Ferry out;
            out.from = int64_t(f->from);
            out.boarded = world_->regions[f->from].built.regionAt(f->boarded);
            out.pad = f->pad;
            out.at = world_->regions[f->to].built.regionAt(f->at);
            out.plan = f->plan;
            out.elapsed = clock_ - f->started;
            m.ferry = out;
        } else if (const ShuttleTrip* t = riderTrip(it->second)) {
            // C4: riding the walk-in Dropship: where it flies (the client
            // makes that region's ground ahead).
            m.flyingTo = int64_t(*t->to);
        }
    }
    return m;
}

} // namespace ac::server
