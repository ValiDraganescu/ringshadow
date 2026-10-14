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
    EXPECT_EQ(store->schemaVersion(), int64_t(10));
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
    // C2 (schema 10): copies, as many as the realm may still build, at
    // least 1: the Citadel 1 (it stands, at least 1), Derricks 4 - 1.
    EXPECT_TRUE((g.blueprints == std::map<StructureKind, int64_t>{{StructureKind::citadel, 1}, {StructureKind::derrick, 3}}));
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
