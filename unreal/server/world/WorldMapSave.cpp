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
    g.blueprints = {{StructureKind::citadel, 1}, {StructureKind::derrick, 1}};
    for (const SavedRegion& r : save.regions)
        for (const SavedBuilding& b : r.buildings)
            if (b.builtBy == key && !b.buildLeft) g.feats.insert(WorldXp::built(b.kind));
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
        // Dev: a home with a Foundry and a Lab, the rig stocked (its
        // buildings are the key's).
        if (params_.devRig) devRig(pilot);
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
        // Its Prospector is gone (it fell): a new one at home, its Citadel
        // or a Hab Dome, else the start pad; its wallet and levels kept.
        in.down = std::nullopt;
        const auto h = home(in, in.region, Vec2(0, 0));
        if (h && h->kind) {
            const Unit u = regions_[h->region]->simulation().freshUnit(UnitKind::prospector, 0, Pilot::player, h->at, 0);
            arrive(pilot, in, h->region, u, regions_[h->region]->places()->regionAt(h->at), -1);
            return Seated::restored;
        }
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
        v.position = landing(f);
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
            u.at = this->landing(*p.flight);
            landing[r].push_back(u);
        } else if (unit) {
            owned[r][*unit] = p.key;
        }
        s.region = world_->regions[r].file.id;
        s.unit = unit;
        // Its progression; the strategy game's levels are not a world's
        // any more (written empty).
        s.progress = SessionStore::encode(p.progress, false);
        s.contracts = encodeContracts(p);
        // Its rig (P7), apart from the progress JSON.
        if (p.progress.rig != Rig{}) s.rig = SessionStore::encode(p.progress.rig, false);
        // What it has given the realm (C1).
        s.taxedOre = p.taxed.ore;
        s.taxedHydrogen = p.taxed.hydrogen;
        s.donatedOre = p.donated.ore;
        s.donatedHydrogen = p.donated.hydrogen;
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
        // The region file's own deposits; starfall deposits go with their falls.
        for (size_t k = 0; k < std::min(st.patches.size(), ownPatches_[i]); k++)
            r.deposits.push_back(SavedDeposit{int64_t(k), st.patches[k].remaining, st.patches[k].depletedAt});
        if (st.wells)
            for (size_t k = 0; k < st.wells->size(); k++) r.wells.push_back(SavedWell{int64_t(k), (*st.wells)[k].remaining});
        out.regions.push_back(std::move(r));
    }
    std::sort(out.regions.begin(), out.regions.end(), [](const SavedRegion& a, const SavedRegion& b) { return a.id < b.id; });
    out.clock = clock_;
    out.realmOre = realm_.ore;
    out.realmHydrogen = realm_.hydrogen;
    for (const Fall& f : falls_) {
        const GameState& st = regions_[f.region]->simulation().state;
        const OreDeposit* d = f.patch && size_t(*f.patch) < st.patches.size() ? &st.patches[size_t(*f.patch)] : nullptr;
        out.starfalls.push_back(SavedStarfall{int64_t(f.id), world_->regions[f.region].file.id, f.at, f.impact, f.from,
                                              d ? std::optional<int64_t>(d->id) : std::nullopt, f.ore, d ? d->remaining : f.ore});
    }
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
            if (!b.builtBy.empty()) builtBy_[*i][b.id] = b.builtBy;
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
        p.taxed = Wallet{sp.taxedOre, sp.taxedHydrogen};
        p.donated = Wallet{sp.donatedOre, sp.donatedHydrogen};
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
            p.news.push_back(net::ProgressMessage::News{net::ProgressMessage::NewsKind::reset, "", 0, p.progress.unspent(), ""});
        }
        if (!decodeContracts(sp.contracts, p))
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": contracts do not read, drawn again");
        // Its rig (P7): none before schema 7.
        p.progress.rig = Rig{};
        if (!sp.rig.empty()) {
            if (auto rig = SessionStore::decode<Rig>(sp.rig, &why)) p.progress.rig = *rig;
            else problems.push_back("pilot " + sp.id.substr(0, 8) + ": rig does not read (" + why + "), left out");
        }
        // Saved before buildings needed blueprints: the blueprints of what
        // it has built, so its buildings stay its own to build again.
        if (!sp.gated)
            for (const SavedRegion& r : save.regions)
                for (const SavedBuilding& b : r.buildings)
                    if (b.builtBy == sp.id && Progression::blueprintFloor(b.kind) > 0 && !p.progress.copies(b.kind))
                        p.progress.giveBlueprint(b.kind);
        // Saved before blueprints stacked (C2, schema 10): each blueprint it
        // had becomes as many copies as the realm may still build of that
        // kind under its limit (the pilot's, with Overbuild), at least 1.
        if (!sp.stacked)
            for (auto& [kind, n] : p.progress.blueprints)
                n = std::max<int64_t>(1, p.progress.limit(kind) - realmCount(kind));
        away_[sp.id] = p;
    }
    // Starfalls coming and craters with their deposits: the deposits back
    // after the region file's own, with their ids and the ore left.
    falls_.clear();
    for (const SavedStarfall& sf : save.starfalls) {
        const auto i = regionNamed(sf.region);
        if (!i) {
            problems.push_back("starfall " + std::to_string(sf.id) + ": region " + sf.region + " is gone, left out");
            continue;
        }
        Fall f{uint32_t(sf.id), *i, sf.at, sf.impact, sf.from, std::nullopt, sf.ore, sf.ore};
        if (sf.patch) landFall(f, sf.remaining, sf.patch);
        falls_.push_back(f);
        nextFall_ = std::max(nextFall_, uint32_t(sf.id) + 1);
    }
    fallsRead_ = std::nullopt;
    // The realm wallet (C1; empty before schema 9).
    realm_ = Wallet{save.realmOre, save.realmHydrogen};
    // The world's hour goes on where it was.
    setClock(save.clock);
    return problems;
}

} // namespace ac::server
