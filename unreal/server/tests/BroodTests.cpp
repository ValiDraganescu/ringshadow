// The Brood in a world (docs/creatures.md; WorldBrood.cpp): the director's
// groups spawned out of sight at a point of interest, lingering, turning on a
// pilot in their site and letting it go past the leash, a Lodeback eating a
// deposit and vanishing (and its ore back when it is killed), a raid's
// warning and the building it fells, the sweep's and the starfall's
// effects, the caps and the opening's calm. Isolated scenes of a few
// seconds to a few minutes of one group each, never a game; run from
// unreal/server.

#include "test.h"

#include "Brood.h"
#include "Ringshadow.h"
#include "Rules.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 61;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// A world past the opening (the director runs in Prospect Landing), no
/// contracts, no starfalls, its own brood dice.
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.broodSeed = 0xb00d1e5ULL;
    return p;
}

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(seconds * 60);
    for (int i = 0; i < n; i++) w.step();
}

Simulation& simOf(WorldMap& w) { return w.regionOf(me)->simulation(); }

/// The deposit of the pilot's region farthest from its Prospector (out of
/// its sight), with ore.
size_t farDeposit(WorldMap& w) {
    const Simulation& sim = simOf(w);
    const GameState& st = sim.state;
    const Vec2 at = drivenUnit(w, me)->position;
    // The tests stand the pilot south of it, in its site and past its leash:
    // open ground there (W4b: regions' walls run close to some deposits).
    const auto room = [&](Vec2 p) {
        for (const double k : {WorldBrood::siteRadius - 2.5, WorldBrood::siteRadius + 3, 9.0, WorldBrood::siteRadius + WorldBrood::leash + 6})
            if (!sim.nav || !sim.nav->walkable(p + Vec2(0, k))) return false;
        return true;
    };
    size_t best = 0;
    double d = -1;
    for (size_t i = 0; i < st.patches.size(); i++)
        if (st.patches[i].remaining > 100 && room(st.patches[i].position) && distance(st.patches[i].position, at) > d) {
            d = distance(st.patches[i].position, at);
            best = i;
        }
    return best;
}

/// Puts the pilot's Prospector at `at` (on free ground), idle.
void place(WorldMap& w, Vec2 at) {
    Simulation& sim = simOf(w);
    Unit* u = drivenUnit(w, me);
    u->position = sim.nav ? sim.nav->nearestFree(at).value_or(at) : at;
    u->goal = std::nullopt;
    u->waypoints = std::nullopt;
}

std::vector<Unit> bugsOf(WorldMap& w, int64_t group) {
    std::vector<Unit> out;
    for (const Unit& u : simOf(w).state.units)
        if (u.bug && u.bug->group == group) out.push_back(u);
    return out;
}

/// A bug alive (not gone, not a body).
bool alive(WorldMap& w, int64_t id) {
    const auto u = simOf(w).state.unit(id);
    return u && u->hp > 0;
}

const WorldMap::BroodGroup* groupOf(WorldMap& w, int64_t id) {
    for (const auto& g : w.broodGroups(*w.where(me)))
        if (g.id == id) return &g;
    return nullptr;
}

} // namespace

/// Every region with a brood level has the Brood's side once the world
/// steps; Prospect Landing is level 1, Slag Pockets level 4.
TEST(brood_side_in_every_region_with_a_level) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    for (size_t r = 0; r < w.regionCount(); r++) {
        const int64_t level = first().regions[r].file.brood;
        EXPECT_EQ(w.broodSide(r).has_value(), level > 0);
    }
    EXPECT_EQ(first().regions[*w.regionNamed("start")].file.brood, int64_t(1));
    EXPECT_EQ(first().regions[*w.regionNamed("slag-pockets")].file.brood, int64_t(4));
    EXPECT_EQ(first().regions[*w.regionNamed("cold-fork")].file.brood, int64_t(2));
    EXPECT_EQ(first().regions[*w.regionNamed("glass-flats")].file.brood, int64_t(3));
    // Bugs never win: the start region's game goes on.
    EXPECT_TRUE(!w.region(*w.regionNamed("start")).simulation().state.endedAt);
}

/// The director's group spawns out of the pilot's sight, walks to its POI
/// and lingers in its site.
TEST(brood_group_spawns_out_of_sight_and_lingers_at_its_poi) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    w.broodNow(r);
    stepFor(w, 0.5);
    ASSERT_TRUE(w.broodGroups(r).size() == 1);
    const auto g = w.broodGroups(r)[0];
    const Vec2 pilot = drivenUnit(w, me)->position;
    // Prospect Landing's groups: 1 to 3 Shardlings, or a lone Lodeback.
    EXPECT_TRUE(g.bugs.size() >= 1 && g.bugs.size() <= 3);
    for (const Unit& u : bugsOf(w, g.id)) {
        EXPECT_TRUE(distance(u.position, pilot) >= WorldBrood::spawnFromProspector - 1);
        EXPECT_TRUE(u.bug->coming || u.kind == UnitKind::lodeback);
    }
    // Not within 25 cells of the start pad, nor 15 of a Citadel.
    const region::Built& places = *w.region(r).places();
    EXPECT_TRUE(distance(g.site, places.pads[first().startPad].at) >= WorldBrood::startCalm);
    // There: lingering in the site (a Lodeback eats), not leaving.
    bool there = false;
    for (int k = 0; k < 120 && !there; k++) {
        stepFor(w, 1);
        const auto* now = groupOf(w, g.id);
        ASSERT_TRUE(now != nullptr);
        EXPECT_TRUE(!now->leaving);
        there = true;
        for (const Unit& u : bugsOf(w, g.id))
            if (u.kind == UnitKind::lodeback ? u.bug->state != Bug::State::eat
                                             : u.bug->coming || distance(u.position, g.site) > g.radius + 0.5)
                there = false;
    }
    EXPECT_TRUE(there);
    // And it stays a while: wandering in its site.
    stepFor(w, 20);
    ASSERT_TRUE(groupOf(w, g.id) != nullptr);
    for (const Unit& u : bugsOf(w, g.id))
        if (u.kind != UnitKind::lodeback) EXPECT_TRUE(distance(u.position, g.site) <= g.radius + 0.5);
}

/// A pilot walking into a group's site is attacked by all of it; running
/// out past the leash, it gets away and the bugs go back to their site.
TEST(brood_aggro_inside_the_site_leash_outside) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const size_t dep = farDeposit(w);
    const Vec2 site = simOf(w).state.patches[dep].position;
    const auto id = w.spawnBroodGroup(r, UnitKind::shardling, 3, WorldMap::Poi::deposit, int64_t(dep), site, true);
    ASSERT_TRUE(id.has_value());
    stepFor(w, 1);
    // Outside its site: left alone.
    EXPECT_TRUE(!groupOf(w, *id)->prey.has_value());
    const double full = drivenUnit(w, me)->hp;
    // In: the whole group turns on it and bites.
    place(w, site + Vec2(0, WorldBrood::siteRadius - 2.5));
    stepFor(w, 4);
    ASSERT_TRUE(groupOf(w, *id)->prey.has_value());
    EXPECT_EQ(*groupOf(w, *id)->prey, drivenUnit(w, me)->id);
    EXPECT_TRUE(drivenUnit(w, me)->hp < full);
    for (const Unit& u : bugsOf(w, *id)) EXPECT_TRUE(u.bug->prey == drivenUnit(w, me)->id);
    // Out past the leash: let go, back to the site.
    place(w, site + Vec2(0, WorldBrood::siteRadius + WorldBrood::leash + 6));
    stepFor(w, 8);
    EXPECT_TRUE(!groupOf(w, *id)->prey.has_value());
    for (const Unit& u : bugsOf(w, *id)) EXPECT_TRUE(distance(u.position, site) <= WorldBrood::siteRadius + 1);
}

/// A pilot that hits a bug anywhere turns its whole group on it, and earns
/// XP for the damage (its share of the bug's worth).
TEST(brood_a_hit_turns_the_group_and_pays_xp) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const size_t dep = farDeposit(w);
    const Vec2 site = simOf(w).state.patches[dep].position;
    const auto id = w.spawnBroodGroup(r, UnitKind::shardling, 2, WorldMap::Poi::deposit, int64_t(dep), site, true);
    ASSERT_TRUE(id.has_value());
    // Outside its site, within the leash.
    place(w, site + Vec2(0, WorldBrood::siteRadius + 3));
    stepFor(w, 0.5);
    EXPECT_TRUE(!groupOf(w, *id)->prey.has_value());
    const double xp = w.progress(me)->xp;
    const Unit shooter = *drivenUnit(w, me);
    simOf(w).hit(groupOf(w, *id)->bugs[0], 5, false, &shooter);
    w.step();
    EXPECT_NEAR(w.progress(me)->xp - xp, Brood::xp(UnitKind::shardling) * 5 / 20, 1e-6);
    stepFor(w, 0.5);
    ASSERT_TRUE(groupOf(w, *id)->prey.has_value());
    EXPECT_EQ(*groupOf(w, *id)->prey, shooter.id);
}

/// A Lodeback eats an unprotected deposit (60 ore gone from the world),
/// walks out of sight and vanishes.
TEST(brood_lodeback_eats_a_deposit_and_vanishes) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const size_t dep = farDeposit(w);
    const int64_t before = simOf(w).state.patches[dep].remaining;
    const auto id = w.spawnBroodGroup(r, UnitKind::lodeback, 1, WorldMap::Poi::deposit, int64_t(dep),
                                      simOf(w).state.patches[dep].position + Vec2(4, 0), false);
    ASSERT_TRUE(id.has_value());
    const int64_t bug = groupOf(w, *id)->bugs[0];
    // It eats until its sac is full (40 s), then leaves.
    std::optional<Unit> u;
    for (int k = 0; k < 60 * 60; k++) {
        w.step();
        u = simOf(w).state.unit(bug);
        ASSERT_TRUE(u.has_value());
        if (u->bug->leaving) break;
    }
    EXPECT_TRUE(u->bug->leaving);
    EXPECT_NEAR(u->bug->sac, Brood::sacFull, 1e-6);
    EXPECT_EQ(simOf(w).state.patches[dep].remaining, before - 60);
    // Out of every pilot's sight: gone, and its ore with it.
    for (int k = 0; k < 120 && simOf(w).state.unit(bug); k++) stepFor(w, 1);
    EXPECT_TRUE(!simOf(w).state.unit(bug).has_value());
    EXPECT_TRUE(groupOf(w, *id) == nullptr);
    EXPECT_EQ(simOf(w).state.patches[dep].remaining, before - 60);
}

/// Killing a Lodeback bursts its sac: its ore goes back into the deposit.
TEST(brood_killing_a_lodeback_returns_its_ore) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const size_t dep = farDeposit(w);
    const int64_t before = simOf(w).state.patches[dep].remaining;
    const auto id = w.spawnBroodGroup(r, UnitKind::lodeback, 1, WorldMap::Poi::deposit, int64_t(dep),
                                      simOf(w).state.patches[dep].position + Vec2(3, 0), true);
    ASSERT_TRUE(id.has_value());
    const int64_t bug = groupOf(w, *id)->bugs[0];
    stepFor(w, 15);
    const int64_t eaten = before - simOf(w).state.patches[dep].remaining;
    EXPECT_TRUE(eaten >= 15 && eaten <= 23);
    const Unit shooter = *drivenUnit(w, me);
    simOf(w).hit(bug, 500, false, &shooter);
    stepFor(w, 0.5);
    // Dead: its body lies a while (the death pose), its sac burst.
    ASSERT_TRUE(simOf(w).state.unit(bug).has_value());
    EXPECT_TRUE(simOf(w).state.unit(bug)->bug->state == Bug::State::dead);
    EXPECT_TRUE(!alive(w, bug));
    stepFor(w, Brood::bodyStays);
    EXPECT_TRUE(!simOf(w).state.unit(bug).has_value());
    EXPECT_EQ(simOf(w).state.patches[dep].remaining, before);
}

/// A Lodeback eats only unprotected ore: a realm building within 10 cells
/// guards a deposit, so the director never sends one there.
TEST(brood_lodebacks_skip_guarded_ore) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    GameState& st = simOf(w).state;
    // A Hab Dome by every deposit within 40 cells of the Prospector.
    std::vector<Vec2> domes;
    const Vec2 home = drivenUnit(w, me)->position;
    for (size_t i = 0; i < st.patches.size(); i++) {
        if (distance(st.patches[i].position, home) > 40) continue;
        const Vec2 at = simOf(w).nav->nearestFree(st.patches[i].position + Vec2(4, 0)).value_or(st.patches[i].position);
        st.structures.push_back(Structure(st.nextID++, StructureKind::habDome, Pilot::player, at));
        domes.push_back(at);
    }
    simOf(w).reindex();
    int lodebacks = 0;
    for (int k = 0; k < 60; k++) {
        w.broodNow(r);
        stepFor(w, 0.3);
        for (const auto& g : w.broodGroups(r))
            for (const int64_t b : g.bugs)
                if (const auto u = simOf(w).state.unit(b); u && u->kind == UnitKind::lodeback) {
                    lodebacks++;
                    ASSERT_TRUE(g.poi == WorldMap::Poi::deposit);
                    for (const Vec2 d : domes)
                        EXPECT_TRUE(distance(d, st.patches[size_t(g.key)].position) >= WorldBrood::guardBuilding + Rules::radius(StructureKind::habDome));
                }
        w.devBrood("brood clear");
    }
    EXPECT_TRUE(lodebacks > 0);
}

/// A raid comes for a realm building: told 20 s before it arrives (the
/// sky's raid, every pilot near hears it), it bites the building down and
/// the building falls; the raid is over then.
TEST(brood_a_raid_is_told_damages_a_building_and_fells_it) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    Simulation& sim = simOf(w);
    const size_t dep = farDeposit(w);
    const Vec2 at = sim.nav->nearestFree(sim.state.patches[dep].position + Vec2(0, 9)).value_or(sim.state.patches[dep].position);
    const int64_t dome = sim.state.nextID++;
    sim.state.structures.push_back(Structure(dome, StructureKind::habDome, Pilot::player, at));
    sim.reindex();
    // Its swarm comes from far off: 70 cells or more, on open ground.
    Vec2 from = at;
    for (const Vec2 d : {Vec2(-1, 0), Vec2(1, 0), Vec2(0, -1), Vec2(0, 1)})
        for (double k = 90; k >= 60; k -= 5) {
            const Vec2 q = at + d * k;
            const GroundRect in = sim.map->bounds.inset(6);
            if (q.x > in.minX && q.x < in.maxX && q.y > in.minZ && q.y < in.maxZ && sim.nav->walkable(q) && distance(q, at) > distance(from, at))
                from = q;
        }
    ASSERT_TRUE(distance(from, at) >= 60);
    // X2: a pilot off line, its ledger filling.
    const std::string keyAway = "0a0b0c0d0e0f10111213141516171819";
    ASSERT_TRUE(w.seat(69, keyAway) == WorldMap::Seated::fresh);
    w.leave(69);
    const auto id = w.spawnBroodGroup(r, UnitKind::shardling, 8, WorldMap::Poi::building, dome, from, false);
    ASSERT_TRUE(id.has_value());
    EXPECT_TRUE(groupOf(w, *id)->raid);
    // Not told yet: still far.
    stepFor(w, 1);
    EXPECT_TRUE(w.sky()->raids.empty());
    bool told = false;
    double arrives = 0;
    for (int k = 0; k < 60 * 30 && !told; k++) {
        w.step();
        if (!w.sky()->raids.empty()) {
            told = true;
            arrives = w.sky()->raids[0].arrives;
            EXPECT_EQ(w.sky()->raids[0].building, dome);
            EXPECT_TRUE(w.sky()->raids[0].kind == StructureKind::habDome);
            EXPECT_TRUE(arrives - w.clock() <= WorldBrood::raidWarning + 1e-6);
        }
    }
    ASSERT_TRUE(told);
    // It takes damage, and falls.
    bool hurt = false, fell = false;
    for (int k = 0; k < 240 && !fell; k++) {
        stepFor(w, 1);
        const auto b = sim.state.structure(dome);
        if (!b) fell = true;
        else if (b->hp < Rules::hp(StructureKind::habDome)) hurt = true;
    }
    EXPECT_TRUE(hurt);
    EXPECT_TRUE(fell);
    stepFor(w, 1);
    const auto* g = groupOf(w, *id);
    if (g) EXPECT_TRUE(g->leaving);
    // Told until it arrived, then dropped.
    EXPECT_TRUE(w.clock() > arrives);
    EXPECT_TRUE(w.sky()->raids.empty());
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::raid && (row.what == "Hab Dome" && !row.where.empty()));
        EXPECT_TRUE(found);
    }
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::fell && (row.what == "Hab Dome" && !row.where.empty()));
        EXPECT_TRUE(found);
    }
}

/// Under a ringshadow sweep a group's site is half again as wide and its
/// leash 4 longer.
TEST(brood_the_sweep_makes_them_bolder) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const Simulation& sim = simOf(w);
    const size_t dep = farDeposit(w);
    const Vec2 at = sim.state.patches[dep].position;
    // A moment the deposit is deep under the band.
    const Ringshadow::Sweep s = Ringshadow::next(w.clock() + 1, w.params().dayLength);
    double when = -1;
    for (double t = s.start; t < s.end; t += 1)
        if (Ringshadow::cover(t, at, sim.map->bounds, w.params().dayLength) >= 0.99) { when = t; break; }
    ASSERT_TRUE(when >= 0);
    w.setClock(when);
    const auto id = w.spawnBroodGroup(r, UnitKind::shardling, 2, WorldMap::Poi::deposit, int64_t(dep), at, true);
    ASSERT_TRUE(id.has_value());
    EXPECT_NEAR(groupOf(w, *id)->radius, WorldBrood::siteRadius * WorldBrood::sweepSite, 1e-9);
    EXPECT_NEAR(groupOf(w, *id)->leash, WorldBrood::leash + WorldBrood::sweepLeash, 1e-9);
}

/// A starfall's impact kills the bugs under it and sends those near it
/// running; its crater draws nothing while it is hot.
TEST(brood_a_starfall_kills_and_scares) {
    WorldParams p = params();
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const auto f = w.forceStarfall(30, r, me);
    ASSERT_TRUE(f.has_value());
    const auto under = w.spawnBroodGroup(r, UnitKind::shardling, 2, WorldMap::Poi::deposit, int64_t(farDeposit(w)), f->at, false);
    const auto near = w.spawnBroodGroup(r, UnitKind::shardling, 2, WorldMap::Poi::deposit, int64_t(farDeposit(w)),
                                        simOf(w).nav->nearestFree(f->at + Vec2(12, 0)).value_or(f->at + Vec2(12, 0)), false);
    ASSERT_TRUE(under && near);
    const auto killed = groupOf(w, *under)->bugs;
    const auto scared = groupOf(w, *near)->bugs;
    // Hold them where they are until it lands.
    for (Unit& u : simOf(w).state.units)
        if (u.bug) u.bug->site = u.position, u.bug->coming = false;
    w.setClock(f->impact - 0.2);
    stepFor(w, 0.5);
    for (const int64_t b : killed) EXPECT_TRUE(!alive(w, b));
    for (const int64_t b : scared) {
        const auto u = simOf(w).state.unit(b);
        ASSERT_TRUE(u.has_value());
        EXPECT_TRUE(u->bug->scaredUntil > simOf(w).state.time);
        EXPECT_TRUE(u->bug->state == Bug::State::flee);
    }
}

/// Caps: never more bugs alive than the level allows, however often the
/// director is asked; in Prospect Landing never more than 6.
TEST(brood_caps_hold) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    for (int k = 0; k < 30; k++) {
        w.broodNow(r);
        stepFor(w, 0.3);
        EXPECT_TRUE(w.broodAlive(r) <= WorldBrood::cap(1));
    }
    EXPECT_TRUE(w.broodAlive(r) >= 4);
}

/// The opening's calm: in Prospect Landing the director holds while the
/// pilot is on the first ten minutes' chain.
TEST(brood_the_opening_stays_calm) {
    WorldParams p = params();
    p.firstStep = 1;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    for (int k = 0; k < 6; k++) {
        w.broodNow(r);
        stepFor(w, 1);
    }
    EXPECT_EQ(w.broodAlive(r), int64_t(0));
    EXPECT_TRUE(w.broodGroups(r).empty());
    // Off: no director at all.
    WorldParams off = params();
    off.brood = false;
    WorldMap quiet(first(), off);
    ASSERT_TRUE(quiet.join(me));
    for (int k = 0; k < 6; k++) {
        quiet.broodNow(r);
        for (int i = 0; i < 60; i++) quiet.step();
    }
    EXPECT_EQ(quiet.broodAlive(r), int64_t(0));
}

/// A region no pilot is in or next to loses its bugs (after two minutes of
/// none; a far corner has had none since the start).
TEST(brood_despawn_where_no_pilot_is_near) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t far = *w.regionNamed("slag-pockets");
    const Simulation& sim = w.region(far).simulation();
    ASSERT_TRUE(!sim.state.patches.empty());
    const auto id = w.spawnBroodGroup(far, UnitKind::shardling, 3, WorldMap::Poi::deposit, 0, sim.state.patches[0].position, true);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(w.broodAlive(far), int64_t(3));
    stepFor(w, 0.5);
    EXPECT_EQ(w.broodAlive(far), int64_t(0));
    EXPECT_TRUE(w.broodGroups(far).empty());
}

/// A bug on the pilot is in its snapshot with its state, and ranked with
/// what fights it (sent every time).
TEST(brood_bugs_on_the_pilot_are_always_sent) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const size_t r = *w.where(me);
    const Vec2 at = drivenUnit(w, me)->position;
    const auto id = w.devBrood("brood attack");
    EXPECT_TRUE(id.find("group") != std::string::npos);
    (void)r;
    stepFor(w, 0.5);
    const Snapshot s = w.regionOf(me)->snapshot(me);
    int bugs = 0;
    for (const auto& e : s.entities)
        if (const Unit* u = e.unit(); u && u->brood()) {
            bugs++;
            ASSERT_TRUE(u->bug.has_value());
            EXPECT_TRUE(e.rank == net::Rank::engaged);
            EXPECT_TRUE(distance(u->position, at) < 6);
        }
    EXPECT_EQ(bugs, 3);
}

/// The shots' raid (`brood raid`, -AcMmoShotsN2): at the dev rig's home,
/// the pilot out from the Citadel on every side and facing every way, the
/// Hab Dome put up ahead of it is bitten within 40 s.
TEST(brood_dev_raid_bites_its_dome_round_the_rig) {
    int bitten = 0, staged = 0;
    for (int side = 0; side < 8; side++)
        for (int facing = 0; facing < 4; facing++) {
            WorldParams p = params();
            p.devRig = true;
            WorldMap w(first(), p);
            ASSERT_TRUE(w.seat(me, "") == WorldMap::Seated::fresh);
            w.step();
            Simulation& sim = simOf(w);
            const Structure* c = nullptr;
            for (const Structure& b : sim.state.structures)
                if (b.kind == StructureKind::citadel) c = &b;
            ASSERT_TRUE(c != nullptr);
            const double a = side * pi / 4;
            Unit* u = drivenUnit(w, me);
            const Vec2 at = c->position + Vec2(std::cos(a), std::sin(a)) * 13;
            if (!sim.pilotCanStand(*u, at)) continue;
            u->position = at;
            u->heading = a + facing * pi / 2;
            sim.reindex();
            const std::string said = w.devBrood("brood raid");
            if (said.find("group") == std::string::npos) {
                std::printf("  side %d facing %d: %s\n", side, facing, said.c_str());
                continue;
            }
            staged++;
            const Structure* dome = nullptr;
            for (const Structure& b : sim.state.structures)
                if (b.kind == StructureKind::habDome) dome = &b;
            ASSERT_TRUE(dome != nullptr);
            const int64_t id = dome->id;
            const Vec2 domeAt = dome->position;
            double hp = Rules::hp(StructureKind::habDome);
            double t = 0;
            for (; t < 40 && hp >= Rules::hp(StructureKind::habDome) - 8; t += 0.5) {
                stepFor(w, 0.5);
                for (const Structure& b : simOf(w).state.structures)
                    if (b.id == id) hp = b.hp;
            }
            if (hp < Rules::hp(StructureKind::habDome) - 8) {
                bitten++;
                continue;
            }
            int alive = 0;
            double nearest = 1e9;
            for (const Unit& x : simOf(w).state.units)
                if (x.bug && x.hp > 0) {
                    alive++;
                    nearest = std::min(nearest, distance(x.position, domeAt));
                }
            std::printf("  side %d facing %d: dome at %.1f,%.1f not bitten in 40 s; %d bugs, nearest %.1f cells off\n", side, facing,
                        domeAt.x, domeAt.y, alive, nearest);
        }
    std::printf("  %d of %d raids bit their dome\n", bitten, staged);
    EXPECT_EQ(bitten, staged);
}

/// The shots' lingering scene (`brood linger`, -AcMmoShotsN2): all four
/// Shardlings in the pilot's snapshots for 10 s (in the Prospector's sight),
/// none on it.
TEST(brood_dev_linger_stays_in_sight) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    const std::string said = w.devBrood("brood linger");
    ASSERT_TRUE(said.find("group") != std::string::npos);
    int fewest = 4;
    for (int k = 0; k < 20; k++) {
        stepFor(w, 0.5);
        const Snapshot s = w.regionOf(me)->snapshot(me);
        int seen = 0;
        for (const auto& e : s.entities)
            if (const Unit* u = e.unit(); u && u->kind == UnitKind::shardling && u->bug) {
                seen++;
                EXPECT_TRUE(u->bug->state == Bug::State::idle || u->bug->state == Bug::State::move);
            }
        fewest = std::min(fewest, seen);
    }
    std::printf("  at least %d of 4 in sight\n", fewest);
    EXPECT_EQ(fewest, 4);
}
