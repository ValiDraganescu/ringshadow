// A late team game staged directly, never played (AGENTS.md: no AI plays a
// game to reach a state): `stageLate(map, teams)` puts down, for every
// player, a grown base (three Citadels where there is room, the Hab Domes
// for the supply cap, Garrisons, Foundries and a Spacedock with Labs,
// Bastions, Sentinels, Derricks), all placed by the AI's own spot finders
// (`Commander::garrisonSpot` and friends) as its AI would, half-mined ore,
// Prospectors on their patches and an army of all eleven soldier kinds near
// the supply cap. Half of each team's armies meet the other team's in the
// middle; the rest stand at home. `simbench --stage-late8 OUT` saves it as
// the fixture `unreal/bench/late8-grown.json` (PORTING.md "A late 8-player
// game").
#pragma once

#include "Commander.h"
#include "Rules.h"
#include "Simulation.h"
#include "Types.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace bench {

using namespace ac;

/// The soldiers of an army by its style, in supply a share (scaled to fit).
inline std::vector<std::pair<UnitKind, int64_t>> lateMix(Player::Style style) {
    using K = UnitKind;
    switch (style) {
    case Player::Style::mech:
        return {{K::ranger, 16}, {K::juggernaut, 4}, {K::comet, 2},   {K::firefly, 8},   {K::longbow, 6},  {K::hailstorm, 6},
                {K::atlas, 3},   {K::scorpion, 6},   {K::kestrel, 3}, {K::peregrine, 3}, {K::dropship, 1}};
    case Player::Style::harass:
        return {{K::ranger, 28}, {K::juggernaut, 6}, {K::comet, 4},   {K::firefly, 6},   {K::longbow, 2},  {K::hailstorm, 2},
                {K::atlas, 1},   {K::scorpion, 8},   {K::kestrel, 2}, {K::peregrine, 4}, {K::dropship, 2}};
    case Player::Style::bio:
    default:
        return {{K::ranger, 40}, {K::juggernaut, 10}, {K::comet, 4},   {K::firefly, 2},   {K::longbow, 2},  {K::hailstorm, 2},
                {K::atlas, 1},   {K::scorpion, 4},    {K::kestrel, 2}, {K::peregrine, 2}, {K::dropship, 2}};
    }
}

/// A late game on `map` for `teams` (one entry a player), at `time` game
/// seconds. Supply used ends at most `supplyUsed` a player (the cap is 200).
inline GameState stageLate(const MapDefinition& map, const std::vector<int64_t>& teams, double time = 1440,
                           int64_t supplyUsed = 194) {
    using SK = StructureKind;
    GameState start = GameState::new_(map, std::nullopt, 0, teams);
    start.units.clear();
    start.time = time;
    Simulation sim(start, map, std::vector<Commander>{}, true);
    GameState& s = sim.state;
    const size_t players = s.players.size();
    std::vector<Commander> ai;
    for (size_t p = 0; p < players; ++p) ai.emplace_back(map, static_cast<int64_t>(p));

    auto put = [&](SK kind, int64_t owner, Vec2 at) -> Structure& {
        s.structures.push_back(Structure(s.nextID, kind, owner, at));
        s.nextID += 1;
        if (sim.nav) sim.nav->setDynamic(s);
        return s.structures.back();
    };
    auto citadels = [&](int64_t p) {
        std::vector<Structure> out;
        for (const auto& b : s.structures) if (b.owner == p && b.kind == SK::citadel) out.push_back(b);
        return out;
    };
    auto siteOf = [&](Vec2 at) {
        size_t best = 0;
        for (size_t i = 1; i < sim.sites.size(); ++i)
            if (distance(sim.sites[i], at) < distance(sim.sites[best], at)) best = i;
        return best;
    };

    // Expansions: a natural each, then a third where one is free, taken
    // in turns as the AIs would (the nearest free site, contested last).
    std::vector<std::set<int64_t>> owned(players);
    std::vector<std::vector<size_t>> bases(players);  // main, natural, third
    for (size_t p = 0; p < players; ++p) {
        const size_t main = siteOf(ai[p].start(s));
        owned[p].insert(static_cast<int64_t>(main));
        bases[p].push_back(main);
    }
    for (int round = 0; round < 2; ++round) {
        for (size_t p = 0; p < players; ++p) {
            std::optional<Vec2> site = ai[p].nextSite(s, sim, owned[p], ai[p].start(s), false);
            if (!site && round == 1) continue;
            if (!site) site = ai[p].nextSite(s, sim, owned[p], ai[p].start(s), true);
            if (!site) continue;
            const size_t i = siteOf(*site);
            put(SK::citadel, static_cast<int64_t>(p), sim.sites[i]);
            owned[p].insert(static_cast<int64_t>(i));
            bases[p].push_back(i);
        }
    }

    // The ore as a late game leaves it: the main's mostly mined (two
    // patches dry), the natural's partly, a third's untouched; the wells
    // likewise.
    for (size_t p = 0; p < players; ++p) {
        for (size_t b = 0; b < bases[p].size() && b < 2; ++b) {
            int64_t dry = b == 0 ? 2 : 0;
            for (size_t k = 0; k < s.patches.size(); ++k) {
                if (sim.patchBase[k] != static_cast<int64_t>(bases[p][b])) continue;
                auto& o = s.patches[k];
                if (dry > 0) { o.remaining = 0; o.depletedAt = time - 90; --dry; continue; }
                o.remaining = static_cast<int64_t>(std::round(double(o.initial) * (b == 0 ? 0.3 : 0.7)));
            }
            if (s.wells) {
                for (auto& w : *s.wells)
                    if (distance(w.position, sim.sites[bases[p][b]]) < 12) w.remaining = b == 0 ? 900 : 1800;
            }
        }
    }

    // Buildings, by the AI's own spot finders, in the order it puts them up.
    for (size_t p = 0; p < players; ++p) {
        const int64_t pl = static_cast<int64_t>(p);
        const Commander& c = ai[p];
        const Player::Style style = s.players[p].style.value_or(Player::Style::bio);
        const std::vector<Structure> cs = citadels(pl);
        const Structure main = cs.front();
        auto production = [&](SK kind) -> std::optional<int64_t> {
            std::optional<Vec2> spot;
            for (const auto& ci : cs) if ((spot = c.garrisonSpot(s, ci, sim.navGrid()))) break;
            if (!spot) return std::nullopt;
            return put(kind, pl, *spot).id;
        };
        auto lab = [&](std::optional<int64_t> holder) {
            if (!holder) return;
            const auto b = s.structure(*holder);
            if (!b || !c.labFits(*b, s)) return;
            const int64_t id = put(SK::lab, pl, b->position + Rules::addonOffset()).id;
            for (auto& x : s.structures) {
                if (x.id == *holder) x.addon = id;
                if (x.id == id) x.parent = *holder;
            }
        };
        auto dome = [&](size_t near) {
            for (size_t k = 0; k < cs.size(); ++k) {
                if (auto spot = c.habDomeSpot(s, cs[(near + k) % cs.size()], sim.navGrid())) { put(SK::habDome, pl, *spot); return; }
            }
        };
        // Supply: 15 a Citadel, 8 a Hab Dome, to the cap of 200.
        const int64_t domes = std::max<int64_t>(0, (Rules::maxSupply - 15 * int64_t(cs.size()) + 7) / 8);
        for (int64_t k = 0; k < std::min<int64_t>(domes, 4); ++k) dome(0);
        // A Bastion before the main and the natural, put up early (it
        // wants open ground in front of the buildings).
        for (size_t k = 0; k < cs.size() && k < 2; ++k)
            if (auto spot = c.bastionSpot(s, sim, cs[k])) put(SK::bastion, pl, *spot);
        const int64_t garrisons = style == Player::Style::mech ? 2 : 3;
        const int64_t foundries = style == Player::Style::mech ? 2 : 1;
        std::vector<std::optional<int64_t>> gs, fs;
        for (int64_t k = 0; k < garrisons; ++k) gs.push_back(production(SK::garrison));
        for (int64_t k = 0; k < foundries; ++k) fs.push_back(production(SK::foundry));
        const std::optional<int64_t> dock = production(SK::spacedock);
        lab(gs[0]);
        if (gs.size() > 1) lab(gs[1]);
        lab(fs[0]);
        lab(dock);
        for (int64_t k = 4; k < domes; ++k) dome(size_t(k));
        // Defences: two Sentinels by each ore line.
        for (const auto& ci : cs) {
            std::vector<Vec2> have;
            for (int k = 0; k < 2; ++k) {
                if (auto spot = c.sentinelSpot(s, ci, have)) { put(SK::sentinel, pl, *spot); have.push_back(*spot); }
            }
        }
        // Derricks on the wells of the main and the natural.
        std::vector<Structure> done(cs.begin(), cs.begin() + std::min<size_t>(cs.size(), 2));
        for (int k = 0; k < 4; ++k)
            if (auto spot = c.derrickSpot(s, done)) put(SK::derrick, pl, *spot);
        (void)main;
    }

    // The players: a late bank, upgrades, waves sent.
    for (auto& pl : s.players) {
        pl.ore = 850;
        pl.hydrogen = 420;
        pl.totalMined = 26000;
        pl.waves = 6;
        pl.unitsLost = 90;
        pl.upgrades = std::set<Upgrade>{Upgrade::minigun, Upgrade::aegisShield, Upgrade::novaIgniters};
    }

    // Open ground for a unit: walkable, off buildings, ore and wells.
    auto open = [&](Vec2 p, double r) {
        if (sim.nav && !sim.nav->walkable(p)) return false;
        for (const auto& b : s.structures) if (distance(b.position, p) < Rules::radius(b.kind) + r + 0.3) return false;
        for (const auto& o : s.patches) if (distance(o.position, p) < 1.2 + r) return false;
        if (s.wells) for (const auto& w : *s.wells) if (distance(w.position, p) < 2.0 + r) return false;
        return true;
    };
    auto unit = [&](UnitKind kind, int64_t owner, Vec2 at, double facing) -> Unit& {
        Unit u(s.nextID, kind, owner, at, facing, Unit::Task::idle);
        s.nextID += 1;
        if (kind == UnitKind::dropship) { u.energy = 120; u.cargo = std::vector<int64_t>{}; }
        if (kind == UnitKind::longbow) { u.anchor = 0; u.anchored = false; u.aim = facing; }
        if (kind == UnitKind::scorpion) { u.anchor = 0; u.anchored = false; }
        if (kind == UnitKind::firefly || kind == UnitKind::hailstorm || kind == UnitKind::atlas) u.aim = facing;
        s.units.push_back(u);
        return s.units.back();
    };

    // Prospectors: two a live patch at the main and the natural, one at a
    // third, on their way to it from beside it; three a Derrick, idle by it
    // (the AI sends them in).
    std::vector<int64_t> supply(players, 0);
    for (size_t p = 0; p < players; ++p) {
        const int64_t pl = static_cast<int64_t>(p);
        for (size_t b = 0; b < bases[p].size(); ++b) {
            const Vec2 centre = sim.sites[bases[p][b]];
            for (size_t k = 0; k < s.patches.size(); ++k) {
                if (sim.patchBase[k] != static_cast<int64_t>(bases[p][b]) || s.patches[k].remaining <= 0) continue;
                const Vec2 o = s.patches[k].position, in = normalize(centre - o);
                for (int n = 0; n < (b < 2 ? 2 : 1); ++n) {
                    Unit& u = unit(UnitKind::prospector, pl, o + in * (1.6 + 1.2 * n), std::atan2(-in.y, -in.x));
                    u.task = Unit::Task::toPatch;
                    u.patch = static_cast<int64_t>(k);
                    supply[p] += 1;
                }
            }
        }
        for (const auto& d : s.structures) {
            if (d.owner != pl || d.kind != SK::derrick) continue;
            const Vec2 in = normalize(sim.sites[siteOf(d.position)] - d.position);
            for (int n = 0; n < 3; ++n) {
                const Vec2 side(-in.y, in.x);
                unit(UnitKind::prospector, pl, d.position + in * 2.6 + side * (double(n) - 1) * 0.9, std::atan2(in.y, in.x));
                supply[p] += 1;
            }
        }
    }

    // Where the armies stand. Each team's first two players' armies meet
    // the other team's first two in the middle; the others hold at home,
    // in front of their base nearest the middle.
    Vec2 centre = Vec2::zero;
    for (size_t p = 0; p < players; ++p) centre = centre + sim.sites[bases[p][0]];
    centre = centre / double(std::max<size_t>(players, 1));
    std::map<int64_t, std::vector<size_t>> byTeam;
    for (size_t p = 0; p < players; ++p) byTeam[s.team(static_cast<int64_t>(p))].push_back(p);
    std::vector<int64_t> teamIDs;
    for (const auto& [t, members] : byTeam) teamIDs.push_back(t);
    auto teamCentre = [&](int64_t t) {
        Vec2 c = Vec2::zero;
        for (size_t p : byTeam[t]) c = c + sim.sites[bases[p][0]];
        return c / double(byTeam[t].size());
    };

    // An army's ranks: rows of 12 behind `front`, facing `facing`.
    auto ranks = [&](const std::vector<UnitKind>& army, int64_t owner, Vec2 front, Vec2 dir, bool home) {
        const Vec2 side(-dir.y, dir.x);
        const double facing = std::atan2(dir.y, dir.x);
        size_t placed = 0;
        bool anchoredNext = home;
        for (int k = 0; placed < army.size() && k < 4000; ++k) {
            const int row = k / 12, col = k % 12;
            const Vec2 at = front - dir * (double(row) * 1.7) + side * ((double(col) - 5.5) * 1.6);
            const UnitKind kind = army[placed];
            const bool air = Rules::stats(kind).air;
            if (!air && !open(at, Rules::radius(kind))) continue;
            Unit& u = unit(kind, owner, at, facing);
            if (kind == UnitKind::longbow) {
                // Half the Longbows dug in at home, all mobile out in the field.
                if (home && anchoredNext) { u.anchor = 1; u.anchored = true; }
                anchoredNext = !anchoredNext;
            }
            if (kind == UnitKind::scorpion && home) { u.anchor = 1; u.anchored = true; u.aim = facing; }
            ++placed;
        }
    };
    for (size_t p = 0; p < players; ++p) {
        const int64_t pl = static_cast<int64_t>(p);
        const Player::Style style = s.players[p].style.value_or(Player::Style::bio);
        // Scale the mix to the supply left; every kind at least one.
        const auto mix = lateMix(style);
        int64_t want = 0;
        for (const auto& [k, n] : mix) want += n * Rules::supply(k);
        const double scale = double(supplyUsed - supply[p]) / double(want);
        std::vector<std::pair<UnitKind, int64_t>> counts;
        int64_t got = 0;
        for (const auto& [k, n] : mix) {
            const int64_t m = std::max<int64_t>(1, int64_t(std::floor(double(n) * scale)));
            counts.emplace_back(k, m);
            got += m * Rules::supply(k);
        }
        // Fill (or trim) with Rangers.
        counts[0].second = std::max<int64_t>(1, counts[0].second + (supplyUsed - supply[p] - got));
        std::vector<UnitKind> army;
        // Interleaved, so every rank has a bit of everything.
        for (bool more = true; more;) {
            more = false;
            for (auto& [k, n] : counts) if (n > 0) { army.push_back(k); --n; more = true; }
        }
        const int64_t t = s.team(pl);
        const auto& members = byTeam[t];
        const size_t rank = size_t(std::find(members.begin(), members.end(), p) - members.begin());
        const bool out = rank < 2 && teamIDs.size() > 1;
        if (out) {
            const int64_t foe = teamIDs[(std::find(teamIDs.begin(), teamIDs.end(), t) - teamIDs.begin() + 1) % teamIDs.size()];
            const Vec2 way = normalize(teamCentre(foe) - teamCentre(t));
            const Vec2 side(-way.y, way.x);
            // The two pairs meet a little apart on either side of the middle.
            const Vec2 meet = centre + side * (rank == 0 ? -16.0 : 16.0);
            ranks(army, pl, meet - way * 3.0, way, false);
        } else {
            Vec2 forward = sim.sites[bases[p][0]];
            for (size_t b : bases[p]) if (distance(sim.sites[b], centre) < distance(forward, centre)) forward = sim.sites[b];
            const Vec2 dir = normalize(centre - forward);
            // The back rank 14 cells out, clear of the base's buildings.
            const double depth = double((army.size() + 11) / 12) * 1.7;
            ranks(army, pl, forward + dir * (14.0 + depth), dir, true);
        }
    }

    // What each team remembers by now: the whole map explored, every base
    // site seen lately, every enemy building known (as a team, alike).
    Simulation done(s, map, std::vector<Commander>{}, true);
    GameState out = done.state;
    for (auto& u : out.units) u.hp = done.maxHP(u);
    if (done.vision) {
        const int64_t cells = done.vision->width * done.vision->height;
        std::vector<Intel> intel(players);
        for (size_t p = 0; p < players; ++p) {
            Intel& k = intel[p];
            k.explored.assign(size_t((cells + 63) / 64), ~uint64_t(0));
            k.sites.assign(done.sites.size(), time - 30);
            for (const auto& b : out.structures)
                if (out.hostile(b.owner, static_cast<int64_t>(p))) k.buildings.push_back(b);
        }
        out.intel = intel;
    }
    return out;
}

}  // namespace bench
