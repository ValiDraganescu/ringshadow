// The world kept (WorldMap.h, docs/mmorpg-m3.md): pilots seated by their
// key, parked while off line, and the world as the save holds it
// (WorldStore.h) and put back from it.

#include "WorldMap.h"

#include <tuple>

#include "Brood.h"

#include "Defence.h"
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
    joinOrder_[pilot] = ++joins_;
    // H2: what its mercenaries delivered while it was away.
    if (in.mercAway > 0) {
        in.news.push_back(net::ProgressMessage::News{net::ProgressMessage::NewsKind::merc, "away", 0, in.mercAway, ""});
        in.mercAway = 0;
    }
    // X2: what the world did while it was away, told once on its next Progress.
    {
        // Told only when something happened (a row, or the realm's wallet moved).
        const net::AwayReport report = awayReport(in);
        if (!report.rows.empty() || report.realmOre != 0 || report.realmHydrogen != 0) in.awayReport = report;
        in.ledger.clear();
        in.leftAt = -1;
    }
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
    r.join(pilot);
    bankIn(in, in.region);
    r.setLevels(pilot, in.record);
    r.setProgress(pilot, in.progress, ownBuildings(in, in.region));
    r.take(pilot, *in.unit);
    in.last = r.places()->regionAt(r.simulation().state.units[size_t(*i)].position);
    return Seated::restored;
}

void WorldMap::park(const PilotState& p) {
    PilotState q = p;
    if (q.flight) {
        // Aboard the Dropship: it lands at the target pad now, with no one in it.
        const Flight f = *q.flight;
        Unit v = f.unit;
        v.position = f.at;
        q.unit = regions_[f.to]->admit(v);
        q.region = f.to;
        q.flight = std::nullopt;
    }
    q.last = std::nullopt;
    // X2: the ledger starts here; a report not yet shown is dropped.
    q.leftAt = clock_;
    q.realmAtLeave = realm_;
    q.ledger.clear();
    q.awayReport = std::nullopt;
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
            u.at = p.flight->at;
            landing[r].push_back(u);
        } else if (unit) {
            owned[r][*unit] = p.key;
        }
        // Its mercenaries' units (H2), as its own.
        for (const PilotState::Merc& m : p.mercs)
            if (m.unit) owned[mercIn(m)][*m.unit] = p.key;
        s.region = world_->regions[r].file.id;
        s.unit = unit;
        // Its progression; the strategy game's levels are not a world's
        // any more (written empty).
        s.progress = SessionStore::encode(p.progress, false);
        s.contracts = encodeContracts(p);
        s.missions = encodeMissions(p);
        s.mercs = encodeMercs(p);
        s.goals = encodeGoals(p);
        s.away = encodeAway(p);
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
    // P1: the frontier pilots' units, as theirs; the rest of them as JSON.
    for (const FrontierPilot& f : frontier_)
        if (f.unit && !f.travelTo && frontierUnitOf(f)) owned[f.region][*f.unit] = f.key;
    out.frontier = encodeFrontier();
    out.standings = encodeStandings();
    out.hunt = encodeHunt();
    // R2: every kind that ever ran.
    for (int k = 0; k < RealmProject::count; k++) {
        const auto& pr = projects_[size_t(k)];
        if (projectRuns_[size_t(k)] == 0 && !pr) continue;
        out.projects.push_back(SavedProject{RealmProject::slug(RealmProject::Kind(k)), projectRuns_[size_t(k)], pr ? pr->ends : 0.0,
                                            pr ? pr->by : std::string()});
    }
    std::sort(out.pilots.begin(), out.pilots.end(), [](const SavedPilot& a, const SavedPilot& b) { return a.id < b.id; });

    for (size_t i = 0; i < n; i++) {
        const GameState& st = regions_[i]->simulation().state;
        SavedRegion r;
        r.id = world_->regions[i].file.id;
        r.time = st.time;
        r.nextId = st.nextID + extra[i];
        for (const Structure& b : st.structures) {
            const auto by = builtBy_[i].find(b.id);
            SavedBuilding sb{b.id, b.kind, b.position, by == builtBy_[i].end() ? std::string() : by->second, b.buildLeft, b.hp, {}, 0};
            // D3: a placed turret's marks (a Citadel's are the realm's) and
            // what it has killed.
            if (Defence::upgradable(b.kind)) sb.marks = b.marks.value_or(TurretMarks{});
            if (const auto k = kills_[i].find(b.id); k != kills_[i].end()) sb.kills = k->second;
            r.buildings.push_back(sb);
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
            r.deposits.push_back(SavedDeposit{int64_t(k), st.patches[k].remaining, st.patches[k].depletedAt, st.patches[k].position});
        if (st.wells)
            for (size_t k = 0; k < st.wells->size(); k++)
                r.wells.push_back(SavedWell{int64_t(k), (*st.wells)[k].remaining, (*st.wells)[k].position});
        out.regions.push_back(std::move(r));
    }
    std::sort(out.regions.begin(), out.regions.end(), [](const SavedRegion& a, const SavedRegion& b) { return a.id < b.id; });
    out.clock = clock_;
    out.realmOre = realm_.ore;
    out.realmHydrogen = realm_.hydrogen;
    out.citadelMarks = citadelMarks_;
    for (const Fall& f : falls_) {
        const GameState& st = regions_[f.region]->simulation().state;
        const OreDeposit* d = f.patch && size_t(*f.patch) < st.patches.size() ? &st.patches[size_t(*f.patch)] : nullptr;
        out.starfalls.push_back(SavedStarfall{int64_t(f.id), world_->regions[f.region].file.id, f.at, f.impact, f.from,
                                              d ? std::optional<int64_t>(d->id) : std::nullopt, f.ore, d ? d->remaining : f.ore});
    }
    // The Matriarch down (N3): when she fell and when she comes back.
    if (const auto nr = nestRegion(); nr && nest_.downAt && nest_.backAt)
        out.nests.push_back(SavedNest{world_->regions[*nr].file.id, *nest_.downAt, *nest_.backAt});
    // The canyons' games (Z2): a wave out comes again whole after a break.
    for (const Canyon& c : canyons_) {
        const double next = !c.out ? c.next : clock_ + (c.state == CanyonState::waiting ? WorldCanyon::firstWait : WorldCanyon::breakFor);
        out.canyons.push_back(SavedCanyon{world_->regions[c.region].file.id, c.id, int64_t(c.state), c.wave, next});
    }
    // In the store's order (by region, then zone): an unchanged world writes
    // nothing (Z3: more than one zone).
    std::sort(out.canyons.begin(), out.canyons.end(),
              [](const SavedCanyon& x, const SavedCanyon& y) { return std::tie(x.region, x.zone) < std::tie(y.region, y.zone); });
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
        kills_[*i].clear();
        int64_t next = sr.nextId;
        for (const SavedBuilding& b : sr.buildings) {
            Structure s(b.id, b.kind, Pilot::player, b.at, b.buildLeft);
            s.hp = b.hp;
            if (Defence::upgradable(b.kind) && b.marks != TurretMarks{}) s.marks = b.marks;
            st.structures.push_back(s);
            if (!b.builtBy.empty()) builtBy_[*i][b.id] = b.builtBy;
            if (b.kills > 0) kills_[*i][b.id] = b.kills;
            next = std::max(next, b.id + 1);
        }
        for (const SavedUnit& u : sr.units) {
            Unit v = sim.freshUnit(u.kind, u.id, Pilot::player, u.at, u.heading);
            // Saved where the region's ground has changed since (W4: a wall
            // now stands there): on the nearest open ground.
            v.position = openGround(sim, v, u.at, 24);
            v.hp = u.hp;
            v.carrying = u.cargo;
            v.hydrogen = u.hydrogen ? std::optional<bool>(true) : std::nullopt;
            st.units.push_back(v);
            next = std::max(next, u.id + 1);
        }
        for (OreDeposit& d : st.patches) d.miner = std::nullopt;
        adopt(*i, sr);
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
        if (!decodeMissions(sp.missions, p))
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": missions do not read, drawn again");
        if (!decodeMercs(sp.mercs, p))
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": mercenaries do not read, left out");
        if (!decodeGoals(sp.goals, p))
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": goals do not read, picked again");
        if (!decodeAway(sp.away, p))
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": away ledger does not read, started again");
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
        // F2: Homing beacon is retired. Its points come back to spend
        // again (out of the slots too), and a Landing Pad blueprint takes
        // its place. Idempotent: a pilot without its ranks is left as it is.
        if (p.progress.ranks.erase(Skill::homingBeacon) > 0) {
            for (auto& s : p.progress.slots)
                if (s == Skill::homingBeacon) s = std::nullopt;
            std::erase_if(p.progress.recent, [](const SkillSpend& x) { return x.skill == Skill::homingBeacon; });
            p.progress.giveBlueprint(StructureKind::landingPad);
            problems.push_back("pilot " + sp.id.substr(0, 8) + ": Homing beacon retired, its points back and a Landing Pad blueprint given");
        }
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
    // The Citadel's marks (D1; none before schema 13).
    citadelMarks_ = save.citadelMarks;
    // The Matriarch down (N3; home before schema 17): she comes back on her
    // clock, the region's bugs calm a world day after her fall.
    nest_ = NestRegion{};
    for (const SavedNest& sn : save.nests) {
        if (regionNamed(sn.region) != nestRegion()) {
            problems.push_back("nest of " + sn.region + ": no nest there now, left out");
            continue;
        }
        nest_.downAt = sn.downAt;
        nest_.backAt = sn.backAt;
        nest_.calmUntil = sn.downAt + Matriarch::calmDays * params_.dayLength;
    }
    // The canyons' games (Z2; none before schema 20: each waits for wave 1).
    // FAILED comes back without its siege: at its end the zone's turrets
    // fall and it starts over.
    for (Canyon& c : canyons_) {
        c = [&] {
            Canyon fresh;
            fresh.region = c.region;
            fresh.zone = c.zone;
            fresh.id = c.id;
            fresh.name = c.name;
            fresh.middle = c.middle;
            fresh.reach = c.reach;
            return fresh;
        }();
        c.next = save.clock + WorldCanyon::firstWait;
        for (const SavedCanyon& sc : save.canyons) {
            if (regionNamed(sc.region) != c.region || sc.zone != c.id) continue;
            if (sc.state < 0 || sc.state > int64_t(CanyonState::held) || sc.wave < 1 || sc.wave > WorldCanyon::count()) {
                problems.push_back("canyon " + sc.region + "/" + sc.zone + ": state " + std::to_string(sc.state) + ", wave " +
                                   std::to_string(sc.wave) + " do not read, left waiting");
                continue;
            }
            c.state = CanyonState(sc.state);
            c.wave = sc.wave;
            c.next = sc.next;
        }
    }
    for (const SavedCanyon& sc : save.canyons)
        if (!regionNamed(sc.region) || !canyon(*regionNamed(sc.region), sc.zone))
            problems.push_back("canyon " + sc.region + "/" + sc.zone + ": no such zone now, left out");
    // P1: the frontier pilots (none before schema 21: they come after the
    // opening chain, as in a new world).
    if (!decodeFrontier(save.frontier)) problems.push_back("frontier pilots do not read, they come again");
    // R2: the realm's projects (none before schema 22); one that ran out
    // while the world was down ends at the next step (`projectsStep`).
    projects_ = {};
    projectRuns_ = {};
    for (const SavedProject& x : save.projects) {
        std::optional<RealmProject::Kind> kind;
        for (int k = 0; k < RealmProject::count; k++)
            if (x.kind == RealmProject::slug(RealmProject::Kind(k))) kind = RealmProject::Kind(k);
        if (!kind) {
            problems.push_back("realm project " + x.kind + " is not known: dropped");
            continue;
        }
        projectRuns_[size_t(*kind)] = x.runs;
        if (x.ends > 0) projects_[size_t(*kind)] = Project{*kind, x.ends, x.by};
    }
    // The world's hour goes on where it was.
    setClock(save.clock);
    // X2: a pilot saved on line (or before the away ledger, schema 22) is
    // away from the load on.
    loadedAt_ = clock_;
    loadedRealm_ = realm_;
    // S4: the week's standings (none before schema 24: the week starts now).
    if (!decodeStandings(save.standings)) problems.push_back("the week's standings do not read, the week starts again");
    // N4: the hunt (none before schema 25: the day's call is open).
    if (!decodeHunt(save.hunt)) problems.push_back("the hunt does not read, the day's call is open");
    return problems;
}

void WorldMap::adopt(size_t r, const SavedRegion& sr) {
    Simulation& sim = regions_[r]->simulation();
    GameState& st = sim.state;
    // The same place: a region file's positions as built, to the save's.
    constexpr double kSame = 0.5;
    const size_t own = std::min(ownPatches_[r], st.patches.size());
    const auto placed = [](const auto& rows) {
        return std::any_of(rows.begin(), rows.end(), [](const auto& x) { return x.at.has_value(); });
    };
    const auto derrickAt = [&](Vec2 at) {
        return std::any_of(st.structures.begin(), st.structures.end(),
                           [&](const Structure& b) { return b.kind == StructureKind::derrick && distance(b.position, at) < kSame; });
    };
    const auto put = [&](size_t k, const SavedDeposit& d) {
        st.patches[k].remaining = std::min(d.remaining, st.patches[k].initial);
        st.patches[k].depletedAt = d.depletedAt;
    };
    if (placed(sr.deposits)) {
        for (const SavedDeposit& d : sr.deposits)
            for (size_t k = 0; d.at && k < own; k++)
                if (distance(st.patches[k].position, *d.at) < kSame) {
                    put(k, d);
                    break;
                }
    } else if (sr.deposits.size() == own) {
        // An older save of the same layout: by index.
        for (const SavedDeposit& d : sr.deposits)
            if (d.index >= 0 && size_t(d.index) < own) put(size_t(d.index), d);
    }
    // A mound the new layout puts under a building: empty, for good.
    for (size_t k = 0; k < own; k++) {
        OreDeposit& d = st.patches[k];
        for (const Structure& b : st.structures)
            if (distance(b.position, d.position) < Rules::radius(b.kind) + 1.0) {
                d.initial = 0;
                d.remaining = 0;
                d.depletedAt = std::nullopt;
                break;
            }
    }
    if (!st.wells) return;
    std::vector<Well>& wells = *st.wells;
    for (Well& w : wells) w.harvester = std::nullopt;
    const auto keep = [&](Vec2 at, int64_t remaining) {
        wells.push_back(Well{at, remaining, std::nullopt});
        wellFull_[r].push_back(std::max(remaining, Rules::wellHydrogen));
        wellIdle_[r].push_back(0.0);
        wellLast_[r].push_back(remaining);
    };
    const size_t file = wells.size();
    if (placed(sr.wells)) {
        for (const SavedWell& w : sr.wells) {
            if (!w.at) continue;
            bool found = false;
            for (size_t k = 0; k < file && !found; k++)
                if (distance(wells[k].position, *w.at) < kSame) {
                    wells[k].remaining = w.remaining;
                    found = true;
                }
            if (!found && derrickAt(*w.at)) keep(*w.at, w.remaining);
        }
    } else if (sr.wells.size() == file) {
        for (const SavedWell& w : sr.wells)
            if (w.index >= 0 && size_t(w.index) < file) wells[size_t(w.index)].remaining = w.remaining;
    } else {
        // An older save of another layout: a Derrick off the file's wells
        // keeps a well under it, full (the save knew its well by index only).
        for (const Structure& b : st.structures) {
            if (b.kind != StructureKind::derrick) continue;
            const bool on = std::any_of(wells.begin(), wells.end(), [&](const Well& w) { return distance(w.position, b.position) < kSame; });
            if (!on) keep(b.position, Rules::wellHydrogen);
        }
    }
}

} // namespace ac::server
