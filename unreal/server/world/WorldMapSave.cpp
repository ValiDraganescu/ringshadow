// The world kept (WorldMap.h, docs/mmorpg-m3.md): pilots seated by their
// key, parked while off line, and the world as the save holds it
// (WorldStore.h) and put back from it.

#include "WorldMap.h"

#include "Session.h"

#include <algorithm>

namespace ac::server {

namespace {

SavedUnit savedUnit(const Unit& u, const std::string& owner) {
    SavedUnit s;
    s.id = u.id;
    s.kind = u.kind;
    s.at = u.position;
    s.heading = u.heading;
    s.hp = u.hp;
    s.cargo = u.carrying;
    s.hydrogen = u.hydrogen.value_or(false);
    s.owner = owner;
    return s;
}

/// An old pilot's levels as a progression (docs/prospector-progression.md,
/// "Old MMO saves"): its Prospector's XP × 10 (it was earned at a tenth),
/// the picks dropped (their points come back unspent, the first respec
/// still free), the Citadel and Derrick blueprints (it has built them), and
/// the feats for the buildings it owns, without their XP.
ProspectorProgress converted(const PilotRecord& old, const std::string& key, const WorldSave& save) {
    ProspectorProgress g;
    g.xp = old.xp(UnitKind::prospector) * 10;
    g.blueprints = {StructureKind::citadel, StructureKind::derrick};
    for (const SavedRegion& r : save.regions)
        for (const SavedBuilding& b : r.buildings)
            if (b.owner == key && !b.buildLeft) g.feats.insert(WorldXp::built(b.kind));
    return g;
}

} // namespace

WorldMap::Seated WorldMap::seat(PilotId pilot, const std::string& key) {
    if (pilots_.count(pilot)) return Seated::refused;
    const bool kept = world_.has_value() && !key.empty();
    if (kept)
        for (const auto& [id, p] : pilots_)
            if (p.key == key) return Seated::refused;
    const auto back = kept ? away_.find(key) : away_.end();
    if (back == away_.end()) {
        if (!join(pilot)) return Seated::refused;
        if (kept) pilots_[pilot].key = key;
        return Seated::fresh;
    }
    PilotState& in = pilots_[pilot] = back->second;
    away_.erase(back);
    in.flight = std::nullopt;
    in.last = std::nullopt;
    // A new connection: its client starts at visit 0.
    in.visit = 0;
    Region& r = *regions_[in.region];
    const auto i = in.unit ? r.simulation().unitIndex(*in.unit) : std::nullopt;
    if (!i) {
        // Its Prospector is gone: a new one at the start pad, its wallet
        // and levels kept.
        const Unit u = startProspector();
        arrive(pilot, in, start_, u, start().places()->regionAt(u.position), -1);
        return Seated::restored;
    }
    // Where it left its Prospector, driving it again.
    const bool free = !r.driven();
    r.join(pilot);
    bankIn(in, in.region);
    r.setLevels(pilot, in.record);
    r.setProgress(pilot, in.progress, ownBuildings(in, in.region));
    if (free) r.take(pilot, *in.unit);
    in.last = r.places()->regionAt(r.simulation().state.units[size_t(*i)].position);
    return Seated::restored;
}

void WorldMap::park(const PilotState& p) {
    PilotState q = p;
    if (q.flight) {
        // In the air: it lands at its pad now, with no one in it.
        const Flight f = *q.flight;
        Unit v = f.unit;
        v.position = world_->regions[f.to].built.pads[f.pad].at;
        q.unit = regions_[f.to]->admit(v);
        q.region = f.to;
        q.flight = std::nullopt;
    }
    q.last = std::nullopt;
    away_[q.key] = q;
}

WorldSave WorldMap::save() const {
    WorldSave out;
    if (!world_) return out;
    const size_t n = regions_.size();
    // The pilots' own units, per region; a unit in flight is saved landed
    // at its pad, under the next id of the region it flies to.
    std::vector<std::map<int64_t, std::string>> owned(n);
    std::vector<std::vector<SavedUnit>> landing(n);
    std::vector<int64_t> extra(n, 0);
    auto pilotRow = [&](const PilotState& p) {
        SavedPilot s;
        s.id = p.key;
        s.ore = p.wallet.ore;
        s.hydrogen = p.wallet.hydrogen;
        size_t r = p.region;
        std::optional<int64_t> unit = p.unit;
        if (p.flight) {
            r = p.flight->to;
            unit = regions_[r]->simulation().state.nextID + extra[r]++;
            SavedUnit u = savedUnit(p.flight->unit, p.key);
            u.id = *unit;
            u.at = world_->regions[r].built.pads[p.flight->pad].at;
            landing[r].push_back(u);
        } else if (unit) {
            owned[r][*unit] = p.key;
        }
        s.region = world_->regions[r].file.id;
        s.unit = unit;
        // Its progression; the strategy game's levels are not a world's
        // any more (written empty).
        s.progress = SessionStore::encode(p.progress, false);
        out.pilots.push_back(s);
    };
    for (const auto& [id, p] : pilots_)
        if (!p.key.empty()) pilotRow(p);
    for (const auto& [key, p] : away_) pilotRow(p);
    std::sort(out.pilots.begin(), out.pilots.end(), [](const SavedPilot& a, const SavedPilot& b) { return a.id < b.id; });

    for (size_t i = 0; i < n; i++) {
        const GameState& st = regions_[i]->simulation().state;
        SavedRegion r;
        r.id = world_->regions[i].file.id;
        r.time = st.time;
        r.nextId = st.nextID + extra[i];
        for (const Structure& b : st.structures) {
            const auto by = builtBy_[i].find(b.id);
            r.buildings.push_back(
                SavedBuilding{b.id, b.kind, b.position, by == builtBy_[i].end() ? std::string() : by->second, b.buildLeft, b.hp});
        }
        for (const Unit& u : st.units) {
            const auto by = owned[i].find(u.id);
            if (by != owned[i].end()) r.units.push_back(savedUnit(u, by->second));
        }
        r.units.insert(r.units.end(), landing[i].begin(), landing[i].end());
        std::sort(r.buildings.begin(), r.buildings.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        std::sort(r.units.begin(), r.units.end(), [](const auto& a, const auto& b) { return a.id < b.id; });
        for (size_t k = 0; k < st.patches.size(); k++)
            r.deposits.push_back(SavedDeposit{int64_t(k), st.patches[k].remaining, st.patches[k].depletedAt});
        if (st.wells)
            for (size_t k = 0; k < st.wells->size(); k++) r.wells.push_back(SavedWell{int64_t(k), (*st.wells)[k].remaining});
        out.regions.push_back(std::move(r));
    }
    std::sort(out.regions.begin(), out.regions.end(), [](const SavedRegion& a, const SavedRegion& b) { return a.id < b.id; });
    out.clock = clock_;
    return out;
}

std::vector<std::string> WorldMap::restore(const WorldSave& save) {
    std::vector<std::string> problems;
    if (!world_) {
        if (!save.regions.empty() || !save.pilots.empty()) problems.push_back("not a world: the save is not used");
        return problems;
    }
    for (const SavedRegion& sr : save.regions) {
        const auto i = regionNamed(sr.id);
        if (!i) {
            problems.push_back("region " + sr.id + ": not in the world any more, left out");
            continue;
        }
        Simulation& sim = regions_[*i]->simulation();
        GameState& st = sim.state;
        st.time = sr.time;
        st.structures.clear();
        st.units.clear();
        builtBy_[*i].clear();
        int64_t next = sr.nextId;
        for (const SavedBuilding& b : sr.buildings) {
            Structure s(b.id, b.kind, Pilot::player, b.at, b.buildLeft);
            s.hp = b.hp;
            st.structures.push_back(s);
            if (!b.owner.empty()) builtBy_[*i][b.id] = b.owner;
            next = std::max(next, b.id + 1);
        }
        for (const SavedUnit& u : sr.units) {
            Unit v = sim.freshUnit(u.kind, u.id, Pilot::player, u.at, u.heading);
            v.hp = u.hp;
            v.carrying = u.cargo;
            v.hydrogen = u.hydrogen ? std::optional<bool>(true) : std::nullopt;
            st.units.push_back(v);
            next = std::max(next, u.id + 1);
        }
        for (OreDeposit& d : st.patches) d.miner = std::nullopt;
        for (const SavedDeposit& d : sr.deposits) {
            if (d.index < 0 || d.index >= int64_t(st.patches.size())) {
                problems.push_back("region " + sr.id + ": no deposit " + std::to_string(d.index) + " any more");
                continue;
            }
            st.patches[size_t(d.index)].remaining = d.remaining;
            st.patches[size_t(d.index)].depletedAt = d.depletedAt;
        }
        for (const SavedWell& w : sr.wells) {
            if (!st.wells || w.index < 0 || w.index >= int64_t(st.wells->size())) {
                problems.push_back("region " + sr.id + ": no well " + std::to_string(w.index) + " any more");
                continue;
            }
            (*st.wells)[size_t(w.index)].remaining = w.remaining;
            (*st.wells)[size_t(w.index)].harvester = std::nullopt;
        }
        st.nextID = next;
        sim.reindex();
        sim.refreshSupply();
        sim.refreshNav();
    }
    for (const SavedPilot& sp : save.pilots) {
        PilotState p;
        p.key = sp.id;
        p.wallet = Wallet{sp.ore, sp.hydrogen};
        p.unit = sp.unit;
        if (const auto i = regionNamed(sp.region)) {
            p.region = *i;
        } else {
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": region " + sp.region + " is gone, back to the start pad");
            p.region = start_;
            p.unit = std::nullopt;
        }
        std::string why;
        if (!sp.progress.empty()) {
            if (auto g = SessionStore::decode<ProspectorProgress>(sp.progress, &why)) p.progress = *g;
            else problems.push_back("pilot " + sp.id.substr(0, 8) + ": progress does not read (" + why + "), left out");
        } else if (!sp.levels.empty()) {
            if (auto rec = SessionStore::decode<PilotRecord>(sp.levels, &why)) p.progress = converted(*rec, sp.id, save);
            else problems.push_back("pilot " + sp.id.substr(0, 8) + ": levels do not read (" + why + "), left out");
            // Told on its next login.
            p.news.push_back(net::ProgressMessage::News{net::ProgressMessage::NewsKind::reset, "", 0, p.progress.unspent()});
        }
        // Saved before buildings needed blueprints: the blueprints of what
        // it has built, so its buildings stay its own to build again.
        if (!sp.gated)
            for (const SavedRegion& r : save.regions)
                for (const SavedBuilding& b : r.buildings)
                    if (b.owner == sp.id && Progression::blueprintFloor(b.kind) > 0) p.progress.blueprints.insert(b.kind);
        away_[sp.id] = p;
    }
    // The world's hour goes on where it was.
    setClock(save.clock);
    return problems;
}

} // namespace ac::server
