// A pilot's snapshot (World.h): only what its side sees, most important
// first, near things every time and far ones less often, within a budget
// where waiting raises an entity's priority (docs/networking.md, "Interest").

#include "World.h"
#include "Brood.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace ac::server {

Vec2 Region::focus(const Seat& seat, int64_t side) const {
    if (seat.drives)
        if (const auto i = sim_.unitIndex(*seat.drives)) return sim_.state.units[static_cast<size_t>(*i)].position;
    // Aboard the Dropship (F2): the pad it boarded at or lands on.
    if (seat.watching) return *seat.watching;
    // Not driving: the unit it drove last, as its AI works it; else the
    // side's start base.
    if (seat.drove)
        if (const auto i = sim_.unitIndex(*seat.drove)) return sim_.state.units[static_cast<size_t>(*i)].position;
    const GameState& s = sim_.state;
    if (side >= 0 && side < static_cast<int64_t>(s.players.size())) {
        const std::optional<int64_t> start = s.players[static_cast<size_t>(side)].start;
        const int64_t b = start ? *start : side < static_cast<int64_t>(map_.starts.size()) ? map_.starts[static_cast<size_t>(side)] : -1;
        if (b >= 0 && b < static_cast<int64_t>(map_.bases.size())) return map_.bases[static_cast<size_t>(b)].center;
    }
    return Vec2(0, 0);
}

Snapshot Region::snapshot(PilotId pilot, const InterestParams& params) {
    Snapshot out;
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return out;
    Seat& seat = it->second;
    const int64_t me = side(pilot);
    const GameState& s = sim_.state;
    const int64_t n = seat.snapshots++;

    out.serverTick = tick_;
    out.lastInputTick = seat.lastApplied;
    out.game = game_;
    out.time = s.time;
    out.driven = seat.drives;
    out.winner = s.winner;
    out.endedAt = s.endedAt;
    out.score = s.score;

    // What the driven unit fights: what it shoots or aims at, and what
    // shoots at it.
    const Vec2 from = focus(seat, me);
    std::unordered_set<int64_t> engaged;
    if (seat.drives) {
        if (const auto i = sim_.unitIndex(*seat.drives)) {
            const Unit& d = s.units[static_cast<size_t>(*i)];
            if (d.target) engaged.insert(*d.target);
            if (d.lockTarget) engaged.insert(*d.lockTarget);
        }
        if (seat.held.crosshair)
            if (const auto* on = seat.held.crosshair->as<Pilot::Crosshair::On>()) engaged.insert(on->_0);
    } else if (seat.drove) {
        // Handed back, the unit it drove is still the one it watches: in
        // every snapshot, or its walk is drawn in steps.
        engaged.insert(*seat.drove);
    }
    const int64_t driven = seat.drives.value_or(-1);
    auto rankOf = [&](int64_t id, bool targetsDriven) {
        if (id == driven) return Rank::driven;
        return targetsDriven || engaged.count(id) ? Rank::engaged : Rank::nearby;
    };

    // Everything the side knows of now.
    std::vector<EntityState> all;
    all.reserve(s.units.size() + s.structures.size());
    for (const Unit& u : s.units) {
        const bool own = s.allied(u.owner, me);
        // The Matriarch is seen from far, in sight or not (N3).
        const bool queen = u.kind == UnitKind::matriarch && distance(u.position, from) <= Matriarch::seenFrom;
        if (!own && !queen && !sim_.sees(me, u.id)) continue;
        // A bug on the driven unit is in every snapshot from the moment it
        // turns on it, closing in or not.
        const bool at = driven >= 0 && (u.target == driven || u.lockTarget == driven || (u.bug && u.bug->prey == driven));
        all.push_back(EntityState{own ? u : scrubbed(u), false, rankOf(u.id, at), distance(u.position, from)});
    }
    for (const Structure& b : s.structures) {
        if (!s.allied(b.owner, me) && !sim_.sees(me, b.id)) continue;
        const bool at = driven >= 0 && b.target == driven;
        all.push_back(EntityState{b, false, rankOf(b.id, at), distance(b.position, from)});
    }
    if (sim_.vision && s.intel && me < static_cast<int64_t>(s.intel->size())) {
        for (const Structure& b : (*s.intel)[static_cast<size_t>(me)].buildings) {
            if (sim_.sees(me, b.id)) continue;
            all.push_back(EntityState{b, true, Rank::nearby, distance(b.position, from)});
        }
    }

    // Gone from the side's knowledge since it was told.
    std::unordered_set<int64_t> present;
    present.reserve(all.size());
    for (const EntityState& e : all) present.insert(e.id());
    for (auto k = seat.known.begin(); k != seat.known.end();) {
        if (present.count(*k)) { ++k; continue; }
        out.removed.push_back(*k);
        seat.waiting.erase(*k);
        k = seat.known.erase(k);
    }

    // Due now: the driven unit and what it fights always; the rest by
    // distance band. Then the budget, in order of rank and of distance
    // less what it has waited.
    auto every = [&](double d) {
        return d <= params.nearRadius ? 1 : d <= params.midRadius ? std::max<int64_t>(params.midEvery, 1)
                                                                   : std::max<int64_t>(params.farEvery, 1);
    };
    struct Due { size_t i; double score; };
    std::vector<Due> due;
    due.reserve(all.size());
    for (size_t i = 0; i < all.size(); ++i) {
        const EntityState& e = all[i];
        const auto w = seat.waiting.find(e.id());
        const bool never = w == seat.waiting.end() || w->second.sentAt < 0;
        if (e.rank == Rank::nearby && !never && n - w->second.sentAt < every(e.distance)) continue;
        const double waited = w == seat.waiting.end() ? 0 : static_cast<double>(w->second.waited);
        due.push_back(Due{i, e.distance - waited * params.agingCells});
    }
    std::sort(due.begin(), due.end(), [&](const Due& a, const Due& b) {
        if (all[a.i].rank != all[b.i].rank) return all[a.i].rank < all[b.i].rank;
        if (a.score != b.score) return a.score < b.score;
        return all[a.i].id() < all[b.i].id();
    });
    const size_t room = static_cast<size_t>(std::max<int64_t>(params.maxEntities, 0));
    for (size_t k = 0; k < due.size(); ++k) {
        Waiting& w = seat.waiting[all[due[k].i].id()];
        if (k < room) {
            w.sentAt = n;
            w.waited = 0;
        } else {
            w.waited += 1;
        }
    }
    if (due.size() > room) due.resize(room);
    // Sent most important first: by rank, then by distance.
    std::sort(due.begin(), due.end(), [&](const Due& a, const Due& b) {
        const EntityState &x = all[a.i], &y = all[b.i];
        if (x.rank != y.rank) return x.rank < y.rank;
        if (x.distance != y.distance) return x.distance < y.distance;
        return x.id() < y.id();
    });
    out.entities.reserve(due.size());
    for (const Due& d : due) {
        seat.known.insert(all[d.i].id());
        out.entities.push_back(std::move(all[d.i]));
    }

    // The players: the side's whole, the others' upgrades and team.
    out.players.reserve(s.players.size());
    for (size_t p = 0; p < s.players.size(); ++p) {
        if (s.allied(static_cast<int64_t>(p), me)) {
            out.players.push_back(s.players[p]);
        } else {
            Player other;
            other.upgrades = s.players[p].upgrades;
            other.team = s.players[p].team;
            out.players.push_back(other);
        }
    }
    // The pilot's own levels, not the side's driver's.
    if (me >= 0 && me < static_cast<int64_t>(out.players.size())) out.players[static_cast<size_t>(me)].pilot = levels(pilot);
    for (size_t i = 0; i < s.patches.size(); ++i)
        if (i >= ownPatches_ || sim_.sees(me, s.patches[i].position)) out.patches.push_back(PatchView{static_cast<int64_t>(i), s.patches[i]});
    if (s.wells)
        for (size_t i = 0; i < s.wells->size(); ++i)
            if (sim_.sees(me, (*s.wells)[i].position)) out.wells.push_back(WellView{static_cast<int64_t>(i), (*s.wells)[i]});
    if (sim_.vision && me < static_cast<int64_t>(sim_.sights.size())) {
        FogView fog;
        fog.seen = sim_.sights[static_cast<size_t>(me)].seen;
        fog.near = sim_.sights[static_cast<size_t>(me)].near;
        if (s.intel && me < static_cast<int64_t>(s.intel->size())) {
            fog.explored = (*s.intel)[static_cast<size_t>(me)].explored;
            fog.sites = (*s.intel)[static_cast<size_t>(me)].sites;
        }
        out.fog = std::move(fog);
    }
    out.events = std::move(seat.events);
    seat.events.clear();
    return out;
}

void Region::deferred(PilotId pilot, const std::vector<int64_t>& ids) {
    const auto it = pilots_.find(pilot);
    if (it == pilots_.end()) return;
    for (int64_t id : ids) {
        Waiting& w = it->second.waiting[id];
        // Never sent: due at once, and ahead of those that waited less.
        w.sentAt = -1;
        w.waited += 1;
    }
}

} // namespace ac::server
