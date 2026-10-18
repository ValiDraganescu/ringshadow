// A world saved before today's schema steps (v3: progression, no blueprint
// gate, contracts, starfalls or rig) loads in this build: migrated to v8,
// the pilot keeps its buildings and is given their blueprints, and its
// contracts are drawn. Isolated scene; files in build/save-tests. Run from unreal/server (as `make test` does).

#include "test.h"

#include "Session.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

const LoadedWorld& firstWorld() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

const std::string key = "00112233445566778899aabbccddeeff";

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

} // namespace

TEST(save_v3_pilot_keeps_its_buildings_and_gets_their_blueprints) {
    const std::string path = fresh("v3buildings");
    std::string why;
    const std::vector<WorldStore::Migration> v3(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 3);
    ProspectorProgress was;
    was.xp = 900;
    {
        auto store = WorldStore::open(path, "first", &why, v3);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(3));
        // Its rows as that build wrote them: a finished Citadel and Derrick.
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 80, 9)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO buildings (region, id, kind, x, y, owner, build_left, hp) VALUES "
                                "('start', 4, 'citadel', 60, 40, '" + key + "', NULL, 1500), "
                                "('start', 5, 'derrick', 70, 40, '" + key + "', NULL, 100)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress) VALUES ('" + key +
                                "', 40, 7, 'start', NULL, '', '" + SessionStore::encode(was, false) + "')", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(firstWorld());
    EXPECT_TRUE(w.restore(*loaded).empty());
    constexpr PilotId me = 3;
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    // Its buildings stand, its own.
    const Simulation& sim = w.start().simulation();
    const Structure* citadel = structure(sim, 4);
    const Structure* derrick = structure(sim, 5);
    ASSERT_TRUE(citadel != nullptr && derrick != nullptr);
    EXPECT_TRUE(citadel->kind == StructureKind::citadel && citadel->complete());
    EXPECT_TRUE(derrick->kind == StructureKind::derrick && derrick->complete());
    EXPECT_TRUE(w.realmFinished(StructureKind::citadel));
    EXPECT_TRUE(w.realmFinished(StructureKind::derrick));
    // Its progression kept, the blueprints of what it built given.
    const ProspectorProgress g = *w.progress(me);
    EXPECT_NEAR(g.xp, 900, 1e-9);
    // C2 (schema 10, then 28): the copies it was given for what it built
    // are learned on load, and used up.
    EXPECT_TRUE(g.blueprints.empty());
    EXPECT_TRUE((g.learned == std::set<StructureKind>{StructureKind::citadel, StructureKind::derrick}));
    // Its contracts are drawn on the next step.
    w.step();
    EXPECT_TRUE(!w.contracts(me).empty());
    // Saved again, it is a v8 pilot: gated, its blueprints written.
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    const auto rows = store->rows("SELECT gated, progress FROM pilots");
    ASSERT_TRUE(rows.size() == 1);
    EXPECT_TRUE(rows[0][0] == "1");
    std::string bad;
    const auto back = SessionStore::decode<ProspectorProgress>(rows[0][1], &bad);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->blueprints == g.blueprints);
}

// W4: a world saved before the regions had walls. A pilot's Prospector saved
// where Prospect Landing's west wall now stands is put on the nearest open
// ground on load, and walks from there.
TEST(save_from_before_the_walls_puts_the_prospector_on_open_ground) {
    WorldSave s;
    SavedRegion r;
    r.id = "start";
    r.time = 10;
    r.nextId = 20;
    // Region (4, 20): a wall now, 4 cells in from the west edge.
    r.units.push_back(SavedUnit{7, UnitKind::prospector, Vec2(4 - 72, 20 - 48), 0, 60, 0, false, key});
    s.regions.push_back(r);
    s.pilots.push_back(SavedPilot{key, 100, 0, "start", 7, "", ""});
    WorldMap w(firstWorld());
    EXPECT_TRUE(w.restore(s).empty());
    constexpr PilotId me = 3;
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    const Simulation& sim = w.start().simulation();
    const Unit* u = nullptr;
    for (const Unit& v : sim.state.units)
        if (v.id == 7) u = &v;
    ASSERT_TRUE(u != nullptr);
    const Vec2 was(4 - 72, 20 - 48);
    std::printf("  saved at %.1f,%.1f (a wall now); stands at %.1f,%.1f, %.1f cells off\n", was.x, was.y, u->position.x,
                u->position.y, distance(u->position, was));
    EXPECT_TRUE(!sim.pilotCanStand(*u, was));
    EXPECT_TRUE(sim.pilotCanStand(*u, u->position));
    EXPECT_TRUE(distance(u->position, was) < 24);
}
