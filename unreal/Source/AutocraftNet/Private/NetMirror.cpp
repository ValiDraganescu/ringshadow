#include "NetMirror.h"

#include "RegionFile.h"
#include "WindowMaps.h"

#include <algorithm>
#include <set>

namespace ac::net {

namespace {

MapDefinition buildMap(const RegionDescription& region) {
    // A world's region: built from its region file, as the server built it.
    if (!region.regionFile.empty())
        if (const region::Parsed p = region::parse(region.regionFile); p.file) return region::build(*p.file).map;
    const MapChoice& choice = region.map;
    return choice.players > 2 ? WindowMaps::buildSquare(choice) : WindowMaps::build(choice);
}

} // namespace

Mirror::Mirror(const RegionDescription& region, int64_t side) : region_(region), side_(side) {
    const MapDefinition map = buildMap(region);
    matches_ = map.name == region.mapName && map.version == region.mapVersion && map.seed == region.seed;
    sim_ = std::make_unique<Simulation>(GameState::new_(map), map, region.fog);
    startOver();
}

Mirror::~Mirror() = default;

void Mirror::inherit(const Mirror& from) {
    if (snapshotId_ != 0 || !from.playersKnown_) return;
    sim_->state.players = from.sim_->state.players;
    playersKnown_ = true;
    if (!sky_) sky_ = from.sky_;
    if (!xp_) xp_ = from.xp_;
}

void Mirror::expect(int64_t unit, double heading) {
    if (snapshotId_ != 0) return;
    driven_ = unit;
    sim_->pilot = Pilot(unit, heading);
}

void Mirror::carried(const Unit& u) {
    if (snapshotId_ == 0) units_[u.id] = u;
}

RegionDescription Mirror::describe(const RegionDescription& like, const std::string& regionFile) {
    RegionDescription d = like;
    d.regionFile = regionFile;
    if (const region::Parsed p = region::parse(regionFile); p.file) {
        d.map = MapChoice{p.file->style, p.file->size, 2};
        const MapDefinition map = region::build(*p.file).map;
        d.mapName = map.name;
        d.mapVersion = map.version;
        d.seed = map.seed;
    }
    return d;
}

void Mirror::startOver() {
    // The pilot's controls kept for the same unit (W4: a mirror's first
    // snapshot is a new game to it, and the unit carried over a seamless
    // edge walks on through it).
    if (sim_->pilot) keptPilot_ = sim_->pilot;
    // The map's patches and wells as they start; the rest comes in snapshots.
    sim_->state = GameState::new_(*sim_->map);
    sim_->state.units.clear();
    sim_->state.structures.clear();
    sim_->pilot = std::nullopt;
    units_.clear();
    structures_.clear();
    remembered_.clear();
    driven_ = std::nullopt;
    playersKnown_ = false;
    rebuild();
}

const Unit* Mirror::unit(int64_t id) const {
    const auto it = units_.find(id);
    return it == units_.end() ? nullptr : &it->second;
}

const Structure* Mirror::structure(int64_t id) const {
    const auto it = structures_.find(id);
    return it == structures_.end() ? nullptr : &it->second;
}

void Mirror::apply(const SnapshotPart& part) {
    const Snapshot& s = part.snapshot;
    if (s.game != game_) {
        game_ = s.game;
        startOver();
    }
    if (part.id >= snapshotId_) {
        snapshotId_ = part.id;
        serverTick_ = s.serverTick;
        lastInputTick_ = s.lastInputTick;
        driven_ = s.driven;
        GameState& st = sim_->state;
        st.time = s.time;
        st.winner = s.winner;
        st.endedAt = s.endedAt;
        st.score = s.score;
        if (s.sky) {
            sky_ = s.sky;
            st.dayLength = s.sky->dayLength;
            st.heldHour = s.sky->hour();
        }
        if (s.xp) xp_ = s.xp;
    }
    for (int64_t id : s.removed) {
        units_.erase(id);
        structures_.erase(id);
        remembered_.erase(id);
    }
    for (const EntityState& e : s.entities) {
        if (const Unit* u = e.unit()) {
            units_[u->id] = *u;
            continue;
        }
        const Structure& b = *e.structure();
        if (e.remembered) {
            structures_.erase(b.id);
            remembered_[b.id] = b;
        } else {
            remembered_.erase(b.id);
            structures_[b.id] = b;
        }
    }
    GameState& st = sim_->state;
    if (!s.players.empty()) {
        st.players = s.players;
        playersKnown_ = true;
    }
    for (const PatchView& p : s.patches) {
        if (p.index < 0) continue;
        if (size_t(p.index) >= st.patches.size()) st.patches.resize(size_t(p.index) + 1);
        st.patches[size_t(p.index)] = p.patch;
    }
    for (const WellView& w : s.wells) {
        if (w.index < 0) continue;
        if (!st.wells) st.wells = std::vector<Well>{};
        if (size_t(w.index) >= st.wells->size()) st.wells->resize(size_t(w.index) + 1);
        (*st.wells)[size_t(w.index)] = w.well;
    }
    if (s.fog && side_ >= 0) {
        const size_t players = std::max(st.players.size(), size_t(side_) + 1);
        if (sim_->sights.size() < players) sim_->sights.resize(players);
        Sight& sight = sim_->sights[size_t(side_)];
        sight.seen = s.fog->seen;
        sight.near = s.fog->near;
        sight.marked = std::nullopt;
        if (!st.intel) st.intel = std::vector<Intel>{};
        if (st.intel->size() < players) st.intel->resize(players);
        Intel& intel = (*st.intel)[size_t(side_)];
        intel.explored = s.fog->explored;
        intel.sites = s.fog->sites;
    }
    rebuild();
}

void Mirror::rebuild() {
    GameState& st = sim_->state;
    st.units.clear();
    st.units.reserve(units_.size());
    for (const auto& [id, u] : units_) st.units.push_back(u);
    st.structures.clear();
    st.structures.reserve(structures_.size());
    for (const auto& [id, b] : structures_) st.structures.push_back(b);
    if (st.intel && side_ >= 0 && size_t(side_) < st.intel->size()) {
        auto& buildings = (*st.intel)[size_t(side_)].buildings;
        buildings.clear();
        for (const auto& [id, b] : remembered_) buildings.push_back(b);
    }
    // What the side sees of the others: every one of theirs it was told of
    // and does not only remember.
    if (side_ >= 0 && size_t(side_) < sim_->sights.size()) {
        std::set<int64_t>& ids = sim_->sights[size_t(side_)].ids;
        ids.clear();
        for (const auto& [id, u] : units_)
            if (!st.allied(u.owner, side_)) ids.insert(id);
        for (const auto& [id, b] : structures_)
            if (!st.allied(b.owner, side_)) ids.insert(id);
    }
    sim_->reindex();
    if (driven_ && units_.count(*driven_)) {
        if (!sim_->pilot || sim_->pilot->unit != *driven_) {
            Pilot p;
            p.unit = *driven_;
            p.heading = units_[*driven_].heading;
            if (keptPilot_ && keptPilot_->unit == *driven_) p = *keptPilot_;
            sim_->pilot = p;
        }
        keptPilot_ = std::nullopt;
    } else {
        sim_->pilot = std::nullopt;
    }
}

} // namespace ac::net
