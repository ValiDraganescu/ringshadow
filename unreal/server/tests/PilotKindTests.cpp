// The pilot's kind (docs/kind-registry.md, "MMORPG: the pilot's kind"): a
// pilot's progress blob as the server wrote it before the kind was kept
// loads as a Prospector's and spawns a Prospector, its keys unchanged when
// written again; a pilot saved with another drivable kind spawns that kind.
// Isolated scene; files in build/save-tests. Run from unreal/server (as `make test` does).

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

/// A progress blob with every key the server wrote on 2026-10-17, before
/// the kind: compact, as `SessionStore::encode(p, false)` writes it.
const std::string oldBlob =
    R"({"blueprints":{"citadel":1},"bought":{},"chips":0,"feats":[],"freeRespec":false,"items":{},"known":[],"pads":[],)"
    R"("ranks":{"quickDrill":1},"recent":[],"regions":["start"],"respecs":0,"signature":"","sites":[],"slots":[null,null,null],"xp":900})";

/// The pilot `key` saved in the start region with `progress`, its unit gone
/// (it fell): seated, it gets a new one on the start pad. Its driven unit.
std::optional<Unit> seated(WorldMap& w, PilotId me, const std::string& progress, const std::string& file) {
    const std::string path = fresh(file);
    std::string why;
    WorldSave s;
    SavedRegion r;
    r.id = "start";
    r.time = 10;
    r.nextId = 20;
    s.regions.push_back(r);
    SavedPilot p{key, 100, 0, "start", std::nullopt, "", progress};
    s.pilots.push_back(p);
    {
        auto store = WorldStore::open(path, "first", &why);
        if (!store || !store->write(s, &why)) return std::nullopt;
    }
    auto store = WorldStore::open(path, "first", &why);
    if (!store) return std::nullopt;
    const auto loaded = store->load(&why);
    if (!loaded || !w.restore(*loaded).empty()) return std::nullopt;
    if (w.seat(me, key) != WorldMap::Seated::restored) return std::nullopt;
    const auto id = w.start().driving(me);
    if (!id) return std::nullopt;
    return w.start().simulation().state.unit(*id);
}

} // namespace

TEST(pilotKind_an_old_blob_is_a_prospectors_and_keeps_its_keys) {
    WorldMap w(firstWorld());
    constexpr PilotId me = 3;
    const auto u = seated(w, me, oldBlob, "kind-old");
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(u->kind == UnitKind::prospector);
    const ProspectorProgress g = *w.progress(me);
    EXPECT_TRUE(g.kind == UnitKind::prospector);
    EXPECT_NEAR(g.xp, 900, 1e-9);
    EXPECT_EQ(g.rank(Skill::quickDrill), int64_t(1));
    // Written again: byte for byte the blob it was, no "kind" key, and (C2)
    // no "bought" key: the copies bought are learned kinds now.
    std::string want = oldBlob;
    const std::string bought = "\"bought\":{},";
    if (const size_t at = want.find(bought); at != std::string::npos) want.erase(at, bought.size());
    const std::string again = SessionStore::encode(g, false);
    if (again != want) std::printf("  was %s\n  now %s\n", want.c_str(), again.c_str());
    EXPECT_TRUE(again == want);
}

TEST(pilotKind_a_ranger_pilot_spawns_a_ranger) {
    PilotProgress was;
    was.kind = UnitKind::ranger;
    was.xp = 900;
    const std::string blob = SessionStore::encode(was, false);
    EXPECT_TRUE(blob.find(R"("kind":"ranger")") != std::string::npos);
    WorldMap w(firstWorld());
    constexpr PilotId me = 3;
    const auto u = seated(w, me, blob, "kind-ranger");
    ASSERT_TRUE(u.has_value());
    EXPECT_TRUE(u->kind == UnitKind::ranger);
    EXPECT_TRUE(w.progress(me)->kind == UnitKind::ranger);
    // Its tree is empty: a point is refused, as for a node it has not got.
    EXPECT_TRUE(w.spend(me, Skill::quickDrill).has_value());
}
