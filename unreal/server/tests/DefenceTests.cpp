// The realm's defences in a world (D1, D2; WorldDefence.cpp,
// docs/mmorpg-m8.md): the Citadel's corner turrets armed with the realm's
// marks and upgraded from the realm wallet (refused when it is short, away
// from the Citadel, at the last mark), the marks saved; the AA tower and the
// gatling gun from blueprints, their floors and limits; a defence's builder
// paid for its hits; a raid on a defended Hab Dome beaten. Isolated scenes
// of a few seconds to a minute, never a game; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "Defence.h"
#include "Pilot.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 91;
const std::string keyA = "e0e1e2e3e4e5e6e7e8e9eaebecedeeef";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// No contracts, no starfalls; the director runs in Prospect Landing (past
/// the opening) only when a test spawns its group.
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.broodSeed = 0xdefe11ceULL;
    return p;
}

ProspectorProgress at(int64_t level) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    for (const StructureKind k : Progression::blueprintKinds()) g.giveBlueprint(k, 9);
    return g;
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

void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// A `kind` put up by pilot `p` west of its Prospector standing at `stand`
/// and welded to the end. Nil: refused, or not finished.
std::optional<int64_t> weld(WorldMap& w, PilotId p, StructureKind kind, Vec2 stand) {
    Simulation& sim = simOf(w, p);
    Unit* u = drivenUnit(w, p);
    u->task = Unit::Task::idle;
    u->position = stand;
    u->heading = pi;
    const auto spot = sim.snap(kind, stand + Vec2(-1, 0) * Region::buildReach(kind));
    if (!spot) return std::nullopt;
    const int64_t id = sim.state.nextID;
    if (const auto why = w.build(p, kind, *spot)) {
        std::printf("  %s refused: %s\n", Simulation::title(kind).c_str(), why->c_str());
        return std::nullopt;
    }
    for (int i = 0; i < int((Rules::buildTime(kind) + 5) * 60); i++) {
        wait(w, p, 1);
        if (const Structure* s = structure(sim, id); s && s->complete()) return id;
    }
    return std::nullopt;
}

/// The driven Prospector at the building's edge, where it unloads.
void besides(WorldMap& w, PilotId p, int64_t building) {
    const Structure s = *structure(simOf(w, p), building);
    Unit* u = drivenUnit(w, p);
    const Vec2 out = normalize(u->position - s.position);
    u->position = s.position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(s.kind) + 0.4);
    u->task = Unit::Task::idle;
    w.step();
}

/// The pilot's Citadel on the start region, east of the ore.
struct Home {
    Home() : w(first(), params()) {
        w.seat(a, keyA);
        w.setProgress(a, at(10));
        fill(w, a, 3000, 500);
        east = w.regionOf(a)->places()->ground(Vec2(72, 48));
        citadel = weld(w, a, StructureKind::citadel, east);
    }
    WorldMap w;
    Vec2 east;
    std::optional<int64_t> citadel;
};

std::string freshFile(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

/// Bugs of a group alive (not gone, not a body).
int64_t aliveIn(WorldMap& w, PilotId p, int64_t group) {
    int64_t n = 0;
    for (const Unit& u : simOf(w, p).state.units)
        if (u.bug && u.bug->group == group && u.hp > 0) n += 1;
    return n;
}

} // namespace

// D1: the realm's Citadel is armed (its corner turrets have the realm's
// marks); a mark is bought from the realm wallet at the Citadel, refused
// away from it, when the wallet is short and at the last mark; the
// progress tells every pilot the marks.
TEST(defence_citadel_upgrades_from_the_realm_wallet) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    w.step();
    const Structure* c = structure(simOf(w, a), *s.citadel);
    ASSERT_TRUE(c && c->marks.has_value());
    EXPECT_TRUE(*c->marks == TurretMarks{});

    Unit* u = drivenUnit(w, a);
    u->position = u->position + Vec2(0, -30);
    w.step();
    EXPECT_TRUE(w.upgradeCitadel(a, DefenceStat::rate).value_or("").rfind("Upgrade at the realm's Citadel", 0) == 0);

    besides(w, a, *s.citadel);
    EXPECT_TRUE(w.upgradeCitadel(a, DefenceStat::rate).value_or("") == "The realm wallet is short: 250 ore needed, it has 0");
    w.setRealmWallet({600, 40});
    EXPECT_TRUE(!w.upgradeCitadel(a, DefenceStat::rate).has_value());
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{350, 40}));
    EXPECT_EQ(w.citadelMarks().rate, int64_t(1));
    // The second mark costs 500 ore and 50 MH.
    EXPECT_TRUE(w.upgradeCitadel(a, DefenceStat::rate).value_or("").rfind("The realm wallet is short", 0) == 0);
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{350, 40}));
    // The pilot's own wallet pays nothing.
    EXPECT_EQ(w.wallet(a)->ore, int64_t(3000) - Progression::worldOre(StructureKind::citadel));
    w.step();
    EXPECT_EQ(structure(simOf(w, a), *s.citadel)->marks->rate, int64_t(1));
    EXPECT_EQ(w.progressMessage(a)->citadelMarks.rate, int64_t(1));

    w.setRealmWallet({5000, 500});
    EXPECT_TRUE(!w.upgradeCitadel(a, DefenceStat::range).has_value());
    EXPECT_TRUE(!w.upgradeCitadel(a, DefenceStat::range).has_value());
    EXPECT_TRUE(!w.upgradeCitadel(a, DefenceStat::range).has_value());
    EXPECT_TRUE(w.upgradeCitadel(a, DefenceStat::range).value_or("") == "Range is at its last mark");
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{5000 - 1750, 500 - 200}));
}

// The marks are saved with the world and loaded again; a save from before
// (schema 14) loads with none.
TEST(defence_citadel_marks_saved_and_loaded) {
    const std::string path = freshFile("citadel-marks");
    std::string why;
    {
        WorldMap w(first(), params());
        ASSERT_TRUE(w.join(a));
        w.setCitadelMarks(TurretMarks{.rate = 2, .damage = 1, .range = 3});
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
    }
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        const auto save = store->load(&why);
        ASSERT_TRUE(save.has_value());
        WorldMap w(first(), params());
        EXPECT_TRUE(w.restore(*save).empty());
        EXPECT_TRUE(w.citadelMarks() == (TurretMarks{.rate = 2, .damage = 1, .range = 3}));
        const auto n = store->write(w.save(), &why);
        ASSERT_TRUE(n.has_value());
        EXPECT_EQ(n->changed, int64_t(0));
    }
    const std::string old = freshFile("citadel-marks-v14");
    const std::vector<WorldStore::Migration> v14(WorldStore::migrations().begin(), WorldStore::migrations().end() - 1);
    {
        auto store = WorldStore::open(old, "first", &why, v14);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(14));
    }
    auto store = WorldStore::open(old, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), int64_t(15));
    const auto save = store->load(&why);
    ASSERT_TRUE(save.has_value());
    EXPECT_TRUE(save->citadelMarks == TurretMarks{});
}

// D2: the gatling gun opens at level 5 and the AA tower (the Sentinel) at
// 10, six and four to a realm; both are in the build menu, sold in the shop
// at the blueprint formula, and welded from a blueprint copy.
TEST(defence_turrets_blueprints_floors_and_limits) {
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::gatling), int64_t(5));
    EXPECT_EQ(Progression::blueprintFloor(StructureKind::sentinel), int64_t(10));
    ProspectorProgress g = at(4);
    EXPECT_TRUE(g.buildRefusal(StructureKind::gatling, 0).value_or("") == "The Gatling Gun opens at level 5");
    g = at(5);
    EXPECT_TRUE(!g.buildRefusal(StructureKind::gatling, 5).has_value());
    EXPECT_TRUE(g.buildRefusal(StructureKind::gatling, 6).value_or("") == "Gatling Gun limit reached: 6");
    EXPECT_TRUE(g.buildRefusal(StructureKind::sentinel, 0).value_or("") == "The Sentinel opens at level 10");
    g = at(10);
    EXPECT_TRUE(g.buildRefusal(StructureKind::sentinel, 4).value_or("").rfind("Sentinel limit reached: 4", 0) == 0);
    ProspectorProgress none;
    none.xp = Progression::xp(10);
    EXPECT_TRUE(none.buildRefusal(StructureKind::gatling, 0).value_or("") == "No blueprint: buy one at the Citadel (H)");
    bool gat = false, aa = false;
    for (const StructureKind k : PilotBuild::worldKinds) {
        gat = gat || k == StructureKind::gatling;
        aa = aa || k == StructureKind::sentinel;
    }
    EXPECT_TRUE(gat && aa);
    EXPECT_TRUE(Shop::blueprintPrice(StructureKind::gatling, 0).ore > 0);

    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const int64_t copies = w.progress(a)->copies(StructureKind::gatling);
    const auto gun = weld(w, a, StructureKind::gatling, s.east + Vec2(0, 10));
    const auto tower = weld(w, a, StructureKind::sentinel, s.east + Vec2(0, 16));
    ASSERT_TRUE(gun.has_value() && tower.has_value());
    EXPECT_EQ(w.progress(a)->copies(StructureKind::gatling), copies - 1);
    EXPECT_EQ(w.realmCount(StructureKind::gatling), int64_t(1));
    EXPECT_EQ(w.realmCount(StructureKind::sentinel), int64_t(1));
}

// A defence's hits pay its builder a share of the kill's XP, wherever the
// builder is; the gatling gun shreds a Shardling pack at it.
TEST(defence_builder_earns_from_its_gatling) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto gun = weld(w, a, StructureKind::gatling, s.east + Vec2(0, 14));
    ASSERT_TRUE(gun.has_value());
    const Vec2 g = structure(simOf(w, a), *gun)->position;
    // The pilot walks off, out of the fight.
    drivenUnit(w, a)->position = s.east + Vec2(-20, -20);
    w.step();
    const double xp = w.progress(a)->xp;
    const size_t r = *w.where(a);
    const auto group = w.spawnBroodGroup(r, UnitKind::shardling, 4, WorldMap::Poi::building, *gun, g + Vec2(5, 0), true);
    ASSERT_TRUE(group.has_value());
    for (int i = 0; i < 20 * 60 && aliveIn(w, a, *group) > 0; i++) w.step();
    EXPECT_EQ(aliveIn(w, a, *group), int64_t(0));
    const double earned = w.progress(a)->xp - xp;
    EXPECT_TRUE(earned >= 4 * Brood::xp(UnitKind::shardling) * WorldMap::defenceXpShare - 1);
    EXPECT_TRUE(structure(simOf(w, a), *gun) != nullptr);
}

// A raid of Shardlings and Gloamwings on a Hab Dome guarded by a gatling
// gun and an AA tower is beaten: the bugs turn on the guns that shoot them,
// every bug dies and the dome stands.
TEST(defence_a_raid_on_a_defended_hab_dome_is_beaten) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto dome = weld(w, a, StructureKind::habDome, s.east + Vec2(0, 30));
    ASSERT_TRUE(dome.has_value());
    const Vec2 d = structure(simOf(w, a), *dome)->position;
    const auto gun = weld(w, a, StructureKind::gatling, d + Vec2(3.5, 0) + Vec2(1, 0) * Region::buildReach(StructureKind::gatling));
    const auto tower = weld(w, a, StructureKind::sentinel, d + Vec2(-3.5, 0) + Vec2(1, 0) * Region::buildReach(StructureKind::sentinel));
    ASSERT_TRUE(gun.has_value() && tower.has_value());
    drivenUnit(w, a)->position = s.east + Vec2(-25, -10);
    w.step();
    const size_t r = *w.where(a);
    const auto shard = w.spawnBroodGroup(r, UnitKind::shardling, 8, WorldMap::Poi::building, *dome, d + Vec2(0, 14), false);
    const auto wings = w.spawnBroodGroup(r, UnitKind::gloamwing, 3, WorldMap::Poi::building, *dome, d + Vec2(0, -14), false);
    ASSERT_TRUE(shard.has_value() && wings.has_value());
    // Shot, a group turns on the defence that shot it.
    bool turned = false;
    for (int i = 0; i < 60 * 60 && aliveIn(w, a, *shard) + aliveIn(w, a, *wings) > 0; i++) {
        w.step();
        for (const auto& g : w.broodGroups(r)) turned = turned || (g.turret && (*g.turret == *gun || *g.turret == *tower));
    }
    EXPECT_TRUE(turned);
    EXPECT_EQ(aliveIn(w, a, *shard), int64_t(0));
    EXPECT_EQ(aliveIn(w, a, *wings), int64_t(0));
    const Structure* left = structure(simOf(w, a), *dome);
    ASSERT_TRUE(left != nullptr);
    EXPECT_TRUE(left->hp > 0.5 * Rules::hp(StructureKind::habDome));
}

// The Citadel's corner turrets beat a raid on the Citadel itself (Prospect
// Landing's Citadel may be raided now that it is armed).
TEST(defence_citadel_turrets_beat_a_raid) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const Vec2 c = structure(simOf(w, a), *s.citadel)->position;
    drivenUnit(w, a)->position = c + Vec2(-25, -10);
    w.step();
    const size_t r = *w.where(a);
    const auto shard = w.spawnBroodGroup(r, UnitKind::shardling, 6, WorldMap::Poi::building, *s.citadel, c + Vec2(0, 16), false);
    ASSERT_TRUE(shard.has_value());
    for (int i = 0; i < 40 * 60 && aliveIn(w, a, *shard) > 0; i++) w.step();
    EXPECT_EQ(aliveIn(w, a, *shard), int64_t(0));
    EXPECT_TRUE(structure(simOf(w, a), *s.citadel)->hp > 0.9 * Rules::hp(StructureKind::citadel));
}

// The shots' dev scenes (`defence gatling`, `defence aa`): a turret ahead
// of the pilot with bugs set on it, and the turret hits them.
TEST(defence_dev_scenes_stage_a_fight) {
    for (const std::string what : {"gatling", "aa"}) {
        WorldMap w(first(), params());
        ASSERT_TRUE(w.join(a));
        w.step();
        const std::string said = w.devDefence("defence " + what);
        std::printf("  %s\n", said.c_str());
        ASSERT_TRUE(said.find("group") != std::string::npos);
        const UnitKind kind = what == "gatling" ? UnitKind::shardling : UnitKind::gloamwing;
        int64_t hit = 0, alive = 0;
        for (int i = 0; i < 4 * 60; i++) w.step();
        for (const Unit& u : simOf(w, a).state.units)
            if (u.kind == kind && u.bug) {
                hit += u.hp < Rules::hp(kind) ? 1 : 0;
                alive += u.hp > 0 ? 1 : 0;
            }
        std::printf("  %s: %lld hit, %lld alive\n", what.c_str(), (long long)hit, (long long)alive);
        EXPECT_TRUE(hit >= 2);
    }
}
