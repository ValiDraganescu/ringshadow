// Blueprints learned once and the Target Dummy (C2, docs/guns-and-practice.md
// sections 3 and 4): a copy held is learned with the Learn request and used
// up; a known kind is not learned again; a site's blueprint of a known kind
// pays its ore; a pilot saved before (schema 28) learns what it bought,
// built or held; a dummy needs no blueprint. Isolated scenes on The First
// Reach; files in build/save-tests. Run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "Session.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 93;
const std::string keyA = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

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
    p.firstStep = 0;
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

int64_t inputTick = 0;

void wait(WorldMap& w, PilotId p, int ticks) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

const net::ProgressMessage::News* find(const std::vector<net::ProgressMessage::News>& news, net::ProgressMessage::NewsKind kind) {
    for (const auto& x : news)
        if (x.kind == kind) return &x;
    return nullptr;
}

std::string savePath(const std::string& name) {
    ::mkdir("build", 0755);
    ::mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

/// A spot on open ground round the driven Prospector for a `kind` within
/// its build reach, or nil.
std::optional<Vec2> spot(WorldMap& w, PilotId p, StructureKind kind, double turn = 0) {
    Simulation& sim = w.regionOf(p)->simulation();
    const Unit u = *drivenUnit(w, p);
    const double reach = Region::buildReach(kind);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
        for (int k = 0; k < 32; k++) {
            const double h = turn + k * 2 * pi / 32;
            const auto at = sim.snap(kind, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (!at || distance(*at, u.position) > reach + Region::buildSlack || sim.placementRefusal(kind, *at) ||
                region::touchesPadPlate(first().regions[first().start].built.pads, *at, Rules::radius(kind)))
                continue;
            return at;
        }
    return std::nullopt;
}

} // namespace

// Two Derrick copies held: the first Learn uses one up and the Derrick is
// known; a second Learn is refused and leaves the other copy; a kind with
// no copy is refused. The client is told what is learned.
TEST(learn_consumesACopyOnce) {
    WorldMap w(first(), params());
    w.seat(a, keyA);
    ProspectorProgress g;
    g.xp = Progression::xp(10);
    g.giveBlueprint(StructureKind::derrick, 2);
    w.setProgress(a, g);
    w.step();
    EXPECT_TRUE(!w.progress(a)->knows(StructureKind::derrick));
    EXPECT_TRUE(!w.learn(a, StructureKind::derrick).has_value());
    EXPECT_TRUE(w.progress(a)->knows(StructureKind::derrick));
    EXPECT_EQ(w.progress(a)->copies(StructureKind::derrick), int64_t(1));
    const auto again = w.learn(a, StructureKind::derrick);
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(*again == "You know the " + Simulation::title(StructureKind::derrick));
    EXPECT_EQ(w.progress(a)->copies(StructureKind::derrick), int64_t(1));
    // No copy: refused, nothing learned.
    const auto none = w.learn(a, StructureKind::habDome);
    ASSERT_TRUE(none.has_value());
    EXPECT_TRUE(!w.progress(a)->knows(StructureKind::habDome));
    // A free kind is known without a copy.
    EXPECT_TRUE(w.learn(a, StructureKind::dummy).has_value());
    const auto m = *w.progressMessage(a);
    EXPECT_TRUE(m.learned == std::set<StructureKind>{StructureKind::derrick});
    EXPECT_EQ(m.blueprints.at(StructureKind::derrick), int64_t(1));
}

// The Citadel's crate by Prospect Landing's pad, opened by a pilot that
// knows the Citadel: no copy, the blueprint's ore price into the wallet,
// the find's XP, and the news says so (by "known", the ore).
TEST(site_givesOreForAKnownBlueprint) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(a));
    wait(w, a, 1);
    ProspectorProgress g = *w.progress(a);
    g.learned.insert(StructureKind::citadel);
    w.setProgress(a, g);
    w.takeNews(a);
    const int64_t ore = w.wallet(a)->ore;
    const int64_t price = Shop::blueprintPrice(StructureKind::citadel, 0).ore;
    EXPECT_TRUE(price > 0);
    // Walk into the crate.
    for (size_t r = 0; r < first().regions.size(); r++)
        if (first().regions[r].file.id == "start")
            for (const auto& s : first().regions[r].built.sites)
                if (s.name == "Landing crate") drivenUnit(w, a)->position = s.at + Vec2(1.0, 0);
    wait(w, a, 1);
    const auto after = *w.progress(a);
    EXPECT_TRUE(after.sites.count("start/Landing crate") == 1);
    EXPECT_EQ(after.copies(StructureKind::citadel), int64_t(0));
    EXPECT_EQ(w.wallet(a)->ore, ore + price);
    EXPECT_NEAR(after.xp, g.xp + WorldXp::blueprintPerFloor * 1, 1e-9);
    const auto news = w.takeNews(a);
    const auto* b = find(news, net::ProgressMessage::NewsKind::blueprint);
    ASSERT_TRUE(b != nullptr);
    EXPECT_TRUE(b->id == "citadel");
    EXPECT_TRUE(b->by == "known");
    EXPECT_EQ(b->value, price);
    // Another find (a chip's, a wreck's): a copy of an unknown kind, the ore
    // of a known one (missions, the Matriarch and the canyon waves give
    // through the same `giveBlueprint`).
    EXPECT_TRUE(w.findBlueprint(a, StructureKind::derrick, Progression::blueprintFloor(StructureKind::derrick)));
    EXPECT_EQ(w.progress(a)->copies(StructureKind::derrick), int64_t(1));
    EXPECT_EQ(w.wallet(a)->ore, ore + price);
    EXPECT_TRUE(w.findBlueprint(a, StructureKind::citadel, 1));
    EXPECT_EQ(w.wallet(a)->ore, ore + 2 * price);
    EXPECT_EQ(w.progress(a)->copies(StructureKind::citadel), int64_t(0));
}

// A Target Dummy needs no blueprint and no level: 10 ore, built at once by
// a fresh pilot; twelve a region, the thirteenth refused.
TEST(build_dummyNeedsNoBlueprint) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(a));
    wait(w, a, 1);
    EXPECT_TRUE(w.progress(a)->blueprints.empty() && w.progress(a)->learned.empty());
    EXPECT_EQ(Progression::worldOre(StructureKind::dummy), int64_t(10));
    EXPECT_EQ(Progression::worldHydrogen(StructureKind::dummy), int64_t(0));
    Simulation& sim = w.regionOf(a)->simulation();
    sim.state.players[0].ore = 1000;
    w.step();
    const int64_t before = w.wallet(a)->ore;
    const auto at = spot(w, a, StructureKind::dummy);
    ASSERT_TRUE(at.has_value());
    const auto why = w.build(a, StructureKind::dummy, *at);
    if (why) std::printf("  refused: %s\n", why->c_str());
    ASSERT_TRUE(!why.has_value());
    wait(w, a, 1);
    EXPECT_EQ(before - w.wallet(a)->ore, int64_t(10));
    EXPECT_EQ(w.limitCount(StructureKind::dummy, w.loaded()->start), int64_t(1));
    EXPECT_TRUE(w.progress(a)->learned.empty());
    // Twelve a region: eleven more stand; the thirteenth is refused.
    const size_t r = w.loaded()->start;
    int64_t placed = 1;
    for (int k = 0; k < 64 && placed < 12; k++) {
        const auto s = sim.snap(StructureKind::dummy, drivenUnit(w, a)->position + Vec2(std::cos(k * 0.7), std::sin(k * 0.7)) * (4 + k / 3.0));
        if (!s || sim.placementRefusal(StructureKind::dummy, *s)) continue;
        Structure d(sim.state.nextID++, StructureKind::dummy, Pilot::player, *s);
        d.hp = Rules::hp(StructureKind::dummy);
        sim.state.structures.push_back(d);
        sim.reindex();
        placed++;
    }
    ASSERT_TRUE(placed == 12);
    EXPECT_EQ(w.limitCount(StructureKind::dummy, r), int64_t(12));
    const auto more = spot(w, a, StructureKind::dummy, 1.3);
    ASSERT_TRUE(more.has_value());
    const auto full = w.build(a, StructureKind::dummy, *more);
    ASSERT_TRUE(full.has_value());
    EXPECT_TRUE(full->find("limit") != std::string::npos);
}

// Schema 28: a pilot row from schema 27 (its progress JSON with copies held
// and bought, a building it built) learns on load what it bought, built or
// held; those copies are gone; it is written learned, and a copy bought
// after stays a copy through the next save.
TEST(load_schema28LearnsWhatWasBoughtOrBuilt) {
    const std::string path = savePath("schema28");
    std::string why;
    const std::string key = "a1a2a3a4a5a6a7a8a9aaabacadaeafa0";
    {
        std::vector<WorldStore::Migration> v27 = WorldStore::migrations();
        while (!v27.empty() && v27.back().version > 27) v27.pop_back();
        auto store = WorldStore::open(path, "first", &why, v27);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(27));
        ProspectorProgress was;
        was.xp = Progression::xp(20);
        was.blueprints = {{StructureKind::derrick, 2}, {StructureKind::habDome, 1}};
        // Its copies bought per kind, as a schema 27 blob kept them.
        std::string blob = SessionStore::encode(was, false);
        blob.insert(1, "\"bought\":{\"gatling\":1,\"derrick\":2},");
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 80, 9)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO buildings (region, id, kind, x, y, built_by, build_left, hp) VALUES "
                                "('start', 4, 'lab', 60, 40, '" + key + "', NULL, 400)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, stacked) VALUES ('" + key +
                                "', 40, 7, 'start', NULL, '', '" + blob + "', 1, 1)", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), int64_t(28));
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->pilots.size() == 1);
    EXPECT_TRUE(!loaded->pilots[0].learned);
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.learnedOnLoad().size() == 1);
    EXPECT_TRUE(w.learnedOnLoad()[0].find("schema 28: learned") != std::string::npos);
    constexpr PilotId me = 7;
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    const ProspectorProgress g = *w.progress(me);
    EXPECT_TRUE((g.learned == std::set<StructureKind>{StructureKind::derrick, StructureKind::habDome, StructureKind::gatling,
                                                     StructureKind::lab}));
    EXPECT_TRUE(g.blueprints.empty());
    EXPECT_NEAR(g.xp, Progression::xp(20), 1e-9);
    // Written learned; a copy given now stays a copy through a save.
    ProspectorProgress more = g;
    more.giveBlueprint(StructureKind::sentinel);
    w.setProgress(me, more);
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    EXPECT_TRUE(store->rows("SELECT learned FROM pilots")[0][0] == "1");
    const auto again = store->load(&why);
    ASSERT_TRUE(again.has_value());
    WorldMap w2(first(), params());
    EXPECT_TRUE(w2.restore(*again).empty());
    EXPECT_TRUE(w2.learnedOnLoad().empty());
    ASSERT_TRUE(w2.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_EQ(w2.progress(me)->copies(StructureKind::sentinel), int64_t(1));
    EXPECT_TRUE(!w2.progress(me)->knows(StructureKind::sentinel));
}
