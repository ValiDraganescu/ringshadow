// The Citadel's interior in a world (C1, WorldCitadel.cpp,
// docs/citadel-interior.md section 10, the server's row): the door opened
// from outside and closed only from inside; a corner gun manned from its
// station, freed when its gunner dies or is handed back; the Brood blind to
// a pilot in the hall; the guns silent without a gunner; a saved world's
// Citadel grown in place on load (schema 27); a pilot back in the hall
// after a fall; the dev lines. Isolated scenes of a few seconds, never a
// game; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "Defence.h"
#include "Pilot.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 81;
const std::string keyA = "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf";
constexpr PilotId b = 82;
const std::string keyB = "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.broodSeed = 0xc17ade1ULL;
    return p;
}

Simulation& simOf(WorldMap& w, PilotId p) { return w.regionOf(p)->simulation(); }

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

int64_t inputTick = 0;

/// `ticks` steps with the pilot's input: its walk and heading, keys held.
void drive(WorldMap& w, PilotId p, int ticks, Vec2 walk = Vec2(0, 0), std::optional<double> heading = std::nullopt) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.walk = walk;
        if (const Unit* u = drivenUnit(w, p)) in.heading = heading.value_or(u->heading);
        w.input(p, in);
        w.step();
    }
}

/// The pilot's Prospector stood at `at`, idle.
void standAt(WorldMap& w, PilotId p, Vec2 at) {
    Unit* u = drivenUnit(w, p);
    u->position = at;
    u->task = Unit::Task::idle;
    simOf(w, p).reindex();
}

/// The realm's Citadel put up by the dev line (complete, armed, its door
/// shut) ahead of the seated pilot; its id.
std::optional<int64_t> citadelHere(WorldMap& w) {
    const std::string said = w.devCitadel("citadel here");
    std::printf("  %s\n", said.c_str());
    const std::string head = "dev: citadel here: citadel ";
    if (said.rfind(head, 0) != 0) return std::nullopt;
    return std::atoll(said.c_str() + head.size());
}

struct Hall {
    Hall() : w(first(), params()) {
        w.seat(a, keyA);
        w.step();
        citadel = citadelHere(w);
        if (citadel) {
            w.step();
            centre = structure(simOf(w, a), *citadel)->position;
        }
    }
    WorldMap w;
    std::optional<int64_t> citadel;
    Vec2 centre{0, 0};
    /// Just outside the door, in front of it (the door slab is y 5 to 6).
    Vec2 atDoor() const { return centre + Vec2(0, 6 + 1.2); }
    Vec2 inHall() const { return centre + Vec2(0, -2); }
};

/// Shots and misses by building `id` in the pilot's region on the last step.
int64_t shotsBy(WorldMap& w, PilotId p, int64_t id) {
    int64_t n = 0;
    for (const GameEvent& e : w.regionOf(p)->lastEvents()) {
        if (const auto* s = e.as<GameEvent::Shot>(); s && s->unit == id) n += 1;
        if (const auto* m = e.as<GameEvent::Missed>(); m && m->unit == id) n += 1;
    }
    return n;
}

/// Bugs of a group alive (not gone, not a body).
int64_t aliveIn(WorldMap& w, PilotId p, int64_t group) {
    int64_t n = 0;
    for (const Unit& u : simOf(w, p).state.units)
        if (u.bug && u.bug->group == group && u.hp > 0) n += 1;
    return n;
}

} // namespace

// The door: opened from outside within reach, refused too far; closed only
// from inside, refused while someone stands in the doorway; it takes
// `Rules::doorSeconds` to open.
TEST(door_openFromOutsideCloseOnlyInside) {
    Hall h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const Structure* c = structure(simOf(w, a), *h.citadel);
    ASSERT_TRUE(c && c->door.has_value());
    EXPECT_EQ(c->door->open, 0.0);
    EXPECT_TRUE(!c->door->opening);

    standAt(w, a, h.centre + Vec2(0, 6 + 8));
    EXPECT_TRUE(w.door(a, true).value_or("") == "Too far from the door");
    standAt(w, a, h.atDoor());
    // Not from outside.
    EXPECT_TRUE(w.door(a, false).value_or("") == "Close it from inside");
    ASSERT_TRUE(!w.door(a, true).has_value());
    drive(w, a, int(Rules::doorSeconds * 60) + 2);
    c = structure(simOf(w, a), *h.citadel);
    EXPECT_TRUE(c->door->opening);
    EXPECT_TRUE(Simulation::doorOpen(*c));

    // Inside, far from the door: closes.
    standAt(w, a, h.inHall());
    // Someone in the doorway first: refused.
    Simulation& sim = simOf(w, a);
    Unit other = sim.freshUnit(UnitKind::prospector, sim.state.nextID++, Pilot::player, h.centre + Vec2(0, 5.5), 0);
    sim.state.units.push_back(other);
    sim.reindex();
    EXPECT_TRUE(w.door(a, false).value_or("") == "Someone is in the doorway");
    std::erase_if(sim.state.units, [&](const Unit& u) { return u.id == other.id; });
    sim.reindex();
    ASSERT_TRUE(!w.door(a, false).has_value());
    drive(w, a, int(Rules::doorSeconds * 60) + 2);
    c = structure(simOf(w, a), *h.citadel);
    EXPECT_EQ(c->door->open, 0.0);
    EXPECT_TRUE(!Simulation::doorOpen(*c));
    // Opened from inside too, at the door; the request's dev line says the same.
    standAt(w, a, h.centre + Vec2(0, 4));
    EXPECT_TRUE(w.devCitadel("citadel door open") == "dev: citadel door open");
    standAt(w, a, h.centre + Vec2(0, 30));
    EXPECT_TRUE(w.devCitadel("citadel door close") == "dev: citadel door close: refused: Close it from inside");
}

// A gun: taken from its station in the hall when free; refused outside, too
// far from the station, when another pilot is at it, and when the pilot is
// at a gun already; left with -1.
TEST(man_takesAFreeGunWithinReach) {
    Hall h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const Vec2 s0 = Defence::gunStation(h.centre, 0);
    standAt(w, a, h.centre + Vec2(0, 6 + 8));
    EXPECT_TRUE(w.man(a, 0).value_or("") == "Get inside the Citadel first");
    standAt(w, a, Defence::gunStation(h.centre, 2));
    EXPECT_TRUE(w.man(a, 0).value_or("") == "Too far from the gun's station");
    EXPECT_TRUE(w.man(a, 5).value_or("") == "No such gun");
    EXPECT_TRUE(w.man(a, -1).value_or("") == "Not at a gun");
    standAt(w, a, s0 + Vec2(0.8, -0.6));
    ASSERT_TRUE(!w.man(a, 0).has_value());
    Unit* u = drivenUnit(w, a);
    EXPECT_TRUE(u->task == Unit::Task::atGun);
    EXPECT_TRUE(u->structure == h.citadel);
    EXPECT_NEAR(u->position.x, s0.x, 1e-9);
    EXPECT_NEAR(u->position.y, s0.y, 1e-9);
    EXPECT_TRUE((*structure(simOf(w, a), *h.citadel)->guns)[0].gunner == u->id);
    EXPECT_TRUE(simOf(w, a).sheltered(*u));
    EXPECT_TRUE(w.man(a, 1).value_or("") == "Already at a gun");
    // It holds there: the walk does nothing.
    drive(w, a, 30, Vec2(1, 0));
    u = drivenUnit(w, a);
    EXPECT_TRUE(u->task == Unit::Task::atGun);
    EXPECT_NEAR(u->position.x, s0.x, 1e-6);

    // A second pilot: not at a taken gun.
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    ASSERT_TRUE(w.where(b) == w.where(a));
    standAt(w, b, s0 + Vec2(-0.5, 0.5));
    EXPECT_TRUE(w.man(b, 0).value_or("") == "Someone is at that gun");

    // Left: free again, for the other.
    ASSERT_TRUE(!w.man(a, -1).has_value());
    u = drivenUnit(w, a);
    EXPECT_TRUE(u->task == Unit::Task::idle);
    EXPECT_TRUE(!(*structure(simOf(w, a), *h.citadel)->guns)[0].gunner.has_value());
    ASSERT_TRUE(!w.man(b, 0).has_value());
    EXPECT_TRUE((*structure(simOf(w, b), *h.citadel)->guns)[0].gunner == drivenUnit(w, b)->id);
    // The dev line, as the seated pilot.
    standAt(w, a, Defence::gunStation(h.centre, 3));
    EXPECT_TRUE(w.devCitadel("citadel man 3") == "dev: citadel man 3");
    EXPECT_TRUE(drivenUnit(w, a)->task == Unit::Task::atGun);
    EXPECT_TRUE(w.devCitadel("citadel man none") == "dev: citadel man none");
    EXPECT_TRUE(w.devCitadel("citadel man 7").find("is not 0|1|2|3|none") != std::string::npos);
}

// A gun whose gunner dies is free the next step; one whose gunner's pilot
// goes off line (its unit handed back) is free at once, and the unit stands
// idle.
TEST(man_freedWhenTheGunnerDies) {
    Hall h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    standAt(w, a, Defence::gunStation(h.centre, 1));
    ASSERT_TRUE(!w.man(a, 1).has_value());
    const int64_t gunner = drivenUnit(w, a)->id;
    drivenUnit(w, a)->hp = 0;
    w.step();
    w.step();
    const Structure* c = structure(simOf(w, a), *h.citadel);
    EXPECT_TRUE(!(*c->guns)[1].gunner.has_value());
    EXPECT_TRUE(!simOf(w, a).state.unit(gunner).has_value() || simOf(w, a).state.unit(gunner)->hp <= 0);

    // Off line at a gun.
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    standAt(w, b, Defence::gunStation(h.centre, 2));
    ASSERT_TRUE(!w.man(b, 2).has_value());
    const int64_t kept = drivenUnit(w, b)->id;
    Simulation& sim = simOf(w, b);
    w.leave(b);
    w.step();
    EXPECT_TRUE(!(*structure(sim, *h.citadel)->guns)[2].gunner.has_value());
    ASSERT_TRUE(sim.state.unit(kept).has_value());
    EXPECT_TRUE(sim.state.unit(kept)->task != Unit::Task::atGun);
}

// The Brood: a raid on the Citadel never takes the pilot in the hall for
// prey (the Citadel takes the hits); one that had it for prey outside drops
// it once it is inside.
TEST(brood_raidIgnoresThePilotInTheHall) {
    Hall h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const size_t r = *w.where(a);
    // Outside, in the raid's site: it is the prey.
    standAt(w, a, h.centre + Vec2(-8, 0));
    const auto g = w.spawnBroodGroup(r, UnitKind::shardling, 4, WorldMap::Poi::building, *h.citadel, h.centre + Vec2(-14, 0), true);
    ASSERT_TRUE(g.has_value());
    const int64_t me = drivenUnit(w, a)->id;
    auto prey = [&]() -> std::optional<int64_t> {
        for (const auto& x : w.broodGroups(r))
            if (x.id == *g) return x.prey;
        return std::nullopt;
    };
    for (int i = 0; i < 60 && prey() != me; i++) drive(w, a, 1);
    ASSERT_TRUE(prey() == me);
    // Into the hall (the door shut): dropped, and never taken again.
    standAt(w, a, h.inHall());
    ASSERT_TRUE(simOf(w, a).sheltered(*drivenUnit(w, a)));
    const double full = Rules::hp(UnitKind::prospector);
    drivenUnit(w, a)->hp = full;
    bool again = false;
    for (int i = 0; i < 20 * 60; i++) {
        drive(w, a, 1);
        // (The Brood thinks every `WorldBrood::think` s: a second to let go.)
        if (i > 60 && prey() == me) again = true;
    }
    EXPECT_TRUE(!again);
    EXPECT_TRUE(prey() != me);
    EXPECT_EQ(drivenUnit(w, a)->hp, full);
    // The Citadel itself was hit.
    EXPECT_TRUE(structure(simOf(w, a), *h.citadel)->hp < Rules::hp(StructureKind::citadel));
}

// The corner guns never fire by themselves: a raid on an armed Citadel with
// nobody at a gun draws no shot from it, and the bugs stay whole.
TEST(citadel_noAutoFireWithoutAGunner) {
    Hall h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const size_t r = *w.where(a);
    standAt(w, a, h.centre + Vec2(-30, -10));
    const auto g = w.spawnBroodGroup(r, UnitKind::shardling, 6, WorldMap::Poi::building, *h.citadel, h.centre + Vec2(12, 12), false);
    ASSERT_TRUE(g.has_value());
    int64_t shots = 0;
    for (int i = 0; i < 20 * 60; i++) {
        w.step();
        shots += shotsBy(w, a, *h.citadel);
    }
    EXPECT_EQ(shots, int64_t(0));
    EXPECT_EQ(aliveIn(w, a, *g), int64_t(6));
    for (const Unit& u : simOf(w, a).state.units)
        if (u.bug && u.bug->group == *g) EXPECT_EQ(u.hp, Rules::hp(UnitKind::shardling));
    const Structure* c = structure(simOf(w, a), *h.citadel);
    EXPECT_TRUE(c->hp < Rules::hp(StructureKind::citadel));
    for (const CornerGun& gun : *c->guns) EXPECT_TRUE(!gun.gunner.has_value());
}

// Schema 27: a save from before the Citadel grew (a 5x5 Citadel beside a
// deposit and on a site, a Hab Dome beside it) comes back with the Citadel
// grown in place: the deposit emptied for good, the site swallowed (never
// opened), the Hab Dome left; each told in the restore's lines. Through a
// real store, which migrates the file to 27.
TEST(load_schema27ClearsTheGrownFootprint) {
    WorldSave save;
    {
        WorldMap w(first(), params());
        w.seat(a, keyA);
        w.step();
        save = w.save();
    }
    const WorldRegion& start = first().regions[first().start];
    ASSERT_TRUE(!start.built.sites.empty());
    SavedRegion* sr = nullptr;
    for (SavedRegion& x : save.regions)
        if (x.id == start.file.id) sr = &x;
    ASSERT_TRUE(sr != nullptr);
    // The start's first deposit as the region file lays it, and a site.
    WorldMap probe(first(), params());
    const GameState& fresh = probe.start().simulation().state;
    ASSERT_TRUE(!fresh.patches.empty());
    const Vec2 ore = fresh.patches[0].position;
    // The site farthest from that deposit, so the two Citadels stand apart.
    size_t far = 0;
    for (size_t i = 1; i < start.built.sites.size(); i++)
        if (distance(start.built.sites[i].at, ore) > distance(start.built.sites[far].at, ore)) far = i;
    ASSERT_TRUE(distance(start.built.sites[far].at, ore) > 20);
    const Vec2 site = start.built.sites[far].at;
    const Vec2 c1 = ore + Vec2(4.5, 0);
    const Vec2 c2 = site + Vec2(-3.5, 2.0);
    sr->buildings.clear();
    sr->buildings.push_back(SavedBuilding{500, StructureKind::citadel, c1, keyA, std::nullopt, 1500});
    sr->buildings.push_back(SavedBuilding{501, StructureKind::citadel, c2, keyA, std::nullopt, 1500});
    // A Hab Dome 4 cells off the first: inside its grown walls now.
    sr->buildings.push_back(SavedBuilding{502, StructureKind::habDome, c1 + Vec2(0, -4), keyA, std::nullopt, 400});
    sr->nextId = std::max<int64_t>(sr->nextId, 503);

    const std::string path = "build/save-tests/citadel-27.sqlite";
    mkdir("build/save-tests", 0755);
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    {
        std::string error;
        auto store = WorldStore::open(path, "first", &error);
        ASSERT_TRUE(store != nullptr);
        EXPECT_TRUE(store->schemaVersion() >= int64_t(27));
        ASSERT_TRUE(store->write(save, &error).has_value());
    }
    std::string error;
    auto store = WorldStore::open(path, "first", &error);
    ASSERT_TRUE(store != nullptr);
    const auto back = store->load(&error);
    ASSERT_TRUE(back.has_value());

    WorldMap w(first(), params());
    for (const std::string& l : w.restore(*back)) std::printf("  problem: %s\n", l.c_str());
    const std::vector<std::string>& lines = w.grownCitadels();
    bool deposit = false, swallowed = false, dome = false;
    for (const std::string& l : lines) {
        std::printf("  %s\n", l.c_str());
        if (l.rfind("schema 27: ", 0) != 0) continue;
        // (Deposit 0, 4.5 cells off, is emptied by the old rule already: a
        // mound under a building; the grown square takes those beyond it.)
        deposit = deposit || (l.find("citadel 500") != std::string::npos && l.find(" deposit ") != std::string::npos);
        swallowed = swallowed || (l.find("citadel 501") != std::string::npos && l.find("site " + start.built.sites[far].name) != std::string::npos);
        dome = dome || (l.find("citadel 500") != std::string::npos && l.find("Hab Dome 502") != std::string::npos && l.find(": left") != std::string::npos);
    }
    EXPECT_TRUE(deposit);
    EXPECT_TRUE(swallowed);
    EXPECT_TRUE(dome);
    const GameState& st = w.start().simulation().state;
    EXPECT_EQ(st.patches[0].remaining, int64_t(0));
    EXPECT_EQ(st.patches[0].initial, int64_t(0));
    EXPECT_TRUE(w.siteSwallowed(first().start, far));
    // A site away from both stays.
    for (size_t i = 0; i < start.built.sites.size(); i++) {
        const Vec2 at = start.built.sites[i].at;
        const bool under = (std::abs(at.x - c1.x) < 7 && std::abs(at.y - c1.y) < 7) || (std::abs(at.x - c2.x) < 7 && std::abs(at.y - c2.y) < 7);
        EXPECT_EQ(w.siteSwallowed(first().start, i), under);
    }
    // The Hab Dome stands.
    EXPECT_TRUE(std::any_of(st.structures.begin(), st.structures.end(), [](const Structure& s) { return s.id == 502; }));

    // A pilot on the swallowed site, the act key held past the longest
    // opening, never opens it.
    w.seat(a, keyA);
    w.step();
    standAt(w, a, site);
    for (int i = 0; i < int(WorldSites::vault + 1) * 60; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(a);
        in.hold = true;
        w.input(a, in);
        w.step();
        drivenUnit(w, a)->position = site;
    }
    const std::string id = start.file.id + "/" + start.built.sites[far].name;
    EXPECT_TRUE(!w.progress(a)->sites.count(id));
}

// After a fall, a pilot whose home is a complete Citadel comes back in its
// hall (3 cells back from the centre), sheltered.
TEST(respawn_insideACompleteCitadel) {
    WorldParams p = params();
    p.respawnDelay = 1;
    WorldMap w(first(), p);
    w.seat(a, keyA);
    w.step();
    const auto citadel = citadelHere(w);
    ASSERT_TRUE(citadel.has_value());
    w.step();
    const Vec2 c = structure(simOf(w, a), *citadel)->position;
    // Fallen far off.
    standAt(w, a, c + Vec2(-30, 0));
    drivenUnit(w, a)->hp = 0;
    for (int i = 0; i < 3 * 60 && !w.respawning(a).has_value(); i++) w.step();
    for (int i = 0; i < 3 * 60 && !drivenUnit(w, a); i++) w.step();
    for (int i = 0; i < 3 * 60 && w.respawning(a).has_value(); i++) w.step();
    const Unit* u = drivenUnit(w, a);
    ASSERT_TRUE(u != nullptr);
    std::printf("  back at (%.2f, %.2f) from the centre\n", u->position.x - c.x, u->position.y - c.y);
    EXPECT_TRUE(distance(u->position, c + WorldMap::citadelRespawn) < 0.6);
    EXPECT_TRUE(simOf(w, a).sheltered(*u));
}
