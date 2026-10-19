// The realm's defences in a world (D1, D2, D3; WorldDefence.cpp,
// docs/mmorpg-m8.md): the Citadel's corner turrets armed with the realm's
// marks and upgraded from the realm wallet (refused when it is short, away
// from the Citadel, at the last mark), the marks saved; the AA tower and the
// gatling gun from blueprints, their floors and limits; a defence's builder
// paid for its hits; a raid on a defended Hab Dome beaten; a placed turret
// upgraded by any pilot from its own wallet, its marks and kills saved,
// lost when it is felled. Isolated scenes
// of a few seconds to a minute, never a game; run from unreal/server.

#include "test.h"

#include "GunAim.h"

#include "CitadelSpot.h"

#include "Brood.h"
#include "Defence.h"
#include "Pilot.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <map>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 91;
const std::string keyA = "e0e1e2e3e4e5e6e7e8e9eaebecedeeef";
constexpr PilotId b = 92;
const std::string keyB = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

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
    for (const StructureKind k : Progression::blueprintKinds()) g.learned.insert(k);
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
        citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, east));
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
    EXPECT_TRUE(w.upgradeCitadel(a, DefenceStat::rate).value_or("").rfind("Upgrade at a Citadel", 0) == 0);

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
    // The steps up to 14 (15 is the Citadel's marks, 16 D3's turrets).
    std::vector<WorldStore::Migration> v14;
    for (const auto& m : WorldStore::migrations())
        if (m.version <= 14) v14.push_back(m);
    {
        auto store = WorldStore::open(old, "first", &why, v14);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(14));
    }
    auto store = WorldStore::open(old, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
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
    EXPECT_TRUE(g.buildRefusal(StructureKind::gatling, 6).value_or("") == "Gatling Gun limit reached: 6 in this region");
    EXPECT_TRUE(g.buildRefusal(StructureKind::sentinel, 0).value_or("") == "The Sentinel opens at level 10");
    g = at(10);
    EXPECT_TRUE(g.buildRefusal(StructureKind::sentinel, 4).value_or("").rfind("Sentinel limit reached: 4", 0) == 0);
    ProspectorProgress none;
    none.xp = Progression::xp(10);
    EXPECT_TRUE(none.buildRefusal(StructureKind::gatling, 0).value_or("") == "No blueprint: buy one at a Citadel (H)");
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
    // C2: building uses no copy.
    EXPECT_EQ(w.progress(a)->copies(StructureKind::gatling), copies);
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
// Landing's Citadel may be raided now that it is armed). C1: only with a
// gunner: the pilot mans the north-east gun from its station in the hall
// and aims at the bugs (its heading, its crosshair, the act key held). C2:
// its rounds fly a ballistic arc; the gunner fires on the core's solve, led,
// all the way in. The raid charges from about 10 cells out to the walls,
// and the gun (mounted outboard, its pitch down to -70 degrees) reaches it
// at their foot: some hit lands within 4 cells of the walls.
TEST(defence_citadel_turrets_beat_a_raid) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const Vec2 c = structure(simOf(w, a), *s.citadel)->position;
    Unit* u = drivenUnit(w, a);
    u->position = Defence::gunStation(c, 0);
    u->task = Unit::Task::idle;
    w.step();
    ASSERT_TRUE(!w.man(a, 0).has_value());
    const size_t r = *w.where(a);
    const auto shard = w.spawnBroodGroup(r, UnitKind::shardling, 6, WorldMap::Poi::building, *s.citadel, c + Vec2(16, 16), false);
    ASSERT_TRUE(shard.has_value());
    std::map<int64_t, Vec2> was;
    const double dt = w.regionOf(a)->description().step;
    int64_t fired = 0, hits = 0;
    // How near the walls a round met a bug: the hit's distance outside the
    // Citadel's square.
    double nearest = 1e9;
    const double half = Rules::radius(StructureKind::citadel);
    for (int i = 0; i < 40 * 60 && aliveIn(w, a, *shard) > 0; i++) {
        for (const GameEvent& e : w.regionOf(a)->lastEvents()) {
            if (e.is<GameEvent::Fired>()) fired++;
            if (const auto* im = e.as<GameEvent::Impact>(); im && im->what == GameEvent::Impact::What::unit) {
                hits++;
                nearest = std::min(nearest, std::max(std::abs(im->at.x - c.x), std::abs(im->at.y - c.y)) - half);
            }
        }
        // The nearest bug the gun's solve reaches, led by its last step.
        const Simulation& sim = simOf(w, a);
        const Unit* aim = nullptr;
        std::optional<rig::GunAim> how;
        for (const Unit& bug : sim.state.units) {
            if (!bug.bug || bug.bug->group != *shard || bug.hp <= 0) continue;
            const auto prev = was.find(bug.id);
            const Vec2 v = prev == was.end() ? Vec2(0, 0) : (bug.position - prev->second) / dt;
            const auto solve = rig::aimCornerGun(sim, c, 0, bug, v);
            if (solve && (!aim || distance(bug.position, c) < distance(aim->position, c))) {
                aim = &bug;
                how = solve;
            }
        }
        was.clear();
        for (const Unit& bug : sim.state.units)
            if (bug.bug && bug.bug->group == *shard) was[bug.id] = bug.position;
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(a);
        if (aim) {
            in.heading = how->yaw;
            in.pitch = how->pitch;
            in.crosshair = Pilot::Crosshair(Pilot::Crosshair::On{aim->id});
            in.hold = true;
        }
        w.input(a, in);
        w.step();
    }
    std::printf("  %lld rounds, %lld on a bug, the nearest %.1f cells from the walls\n", (long long)fired, (long long)hits, nearest);
    for (const Unit& bug : simOf(w, a).state.units)
        if (bug.bug && bug.bug->group == *shard && bug.hp > 0)
            std::printf("  alive: (%.1f, %.1f) from the Citadel, hp %.1f, solves %d%d%d%d\n", bug.position.x - c.x, bug.position.y - c.y, bug.hp,
                        int(bool(rig::aimCornerGun(simOf(w, a), c, 0, bug))), int(bool(rig::aimCornerGun(simOf(w, a), c, 1, bug))),
                        int(bool(rig::aimCornerGun(simOf(w, a), c, 2, bug))), int(bool(rig::aimCornerGun(simOf(w, a), c, 3, bug))));
    EXPECT_EQ(aliveIn(w, a, *shard), int64_t(0));
    EXPECT_TRUE(nearest <= 4);
    EXPECT_TRUE(structure(simOf(w, a), *s.citadel)->hp > 0.9 * Rules::hp(StructureKind::citadel));
    EXPECT_TRUE(drivenUnit(w, a)->task == Unit::Task::atGun);
}

// The same gun meets a raid from 85 cells on its way in (C2): the gunner
// holds fire until 45 cells, where the cone (8 mrad) still covers a bug, then
// fires on the core's solve, led; the raid never reaches the walls.
TEST(defence_citadel_gun_meets_a_raid_on_its_way_in) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const Vec2 c = structure(simOf(w, a), *s.citadel)->position;
    Unit* u = drivenUnit(w, a);
    u->position = Defence::gunStation(c, 0);
    u->task = Unit::Task::idle;
    w.step();
    ASSERT_TRUE(!w.man(a, 0).has_value());
    const size_t r = *w.where(a);
    const auto shard = w.spawnBroodGroup(r, UnitKind::shardling, 6, WorldMap::Poi::building, *s.citadel, c + Vec2(60, 60), false);
    ASSERT_TRUE(shard.has_value());
    std::map<int64_t, Vec2> was;
    const double dt = w.regionOf(a)->description().step;
    int64_t fired = 0, hits = 0;
    for (int i = 0; i < 40 * 60 && aliveIn(w, a, *shard) > 0; i++) {
        for (const GameEvent& e : w.regionOf(a)->lastEvents()) {
            if (e.is<GameEvent::Fired>()) fired++;
            if (const auto* im = e.as<GameEvent::Impact>(); im && im->what == GameEvent::Impact::What::unit) hits++;
        }
        // The nearest bug the gun's solve reaches, led by its last step.
        const Simulation& sim = simOf(w, a);
        const Unit* aim = nullptr;
        std::optional<rig::GunAim> how;
        for (const Unit& bug : sim.state.units) {
            if (!bug.bug || bug.bug->group != *shard || bug.hp <= 0) continue;
            const auto prev = was.find(bug.id);
            const Vec2 v = prev == was.end() ? Vec2(0, 0) : (bug.position - prev->second) / dt;
            const auto solve = rig::aimCornerGun(sim, c, 0, bug, v);
            // Held until 45 cells, where the cone (8 mrad) still covers a bug.
            if (solve && distance(bug.position, Defence::cornerGun(c, 0)) <= 45 &&
                (!aim || distance(bug.position, c) < distance(aim->position, c))) {
                aim = &bug;
                how = solve;
            }
        }
        was.clear();
        for (const Unit& bug : sim.state.units)
            if (bug.bug && bug.bug->group == *shard) was[bug.id] = bug.position;
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(a);
        if (aim) {
            in.heading = how->yaw;
            in.pitch = how->pitch;
            in.crosshair = Pilot::Crosshair(Pilot::Crosshair::On{aim->id});
            in.hold = true;
        }
        w.input(a, in);
        w.step();
    }
    std::printf("  %lld rounds, %lld on a bug\n", (long long)fired, (long long)hits);
    for (const Unit& bug : simOf(w, a).state.units)
        if (bug.bug && bug.bug->group == *shard && bug.hp > 0)
            std::printf("  alive: %.1f cells from the Citadel, hp %.1f\n", distance(bug.position, c), bug.hp);
    EXPECT_EQ(aliveIn(w, a, *shard), int64_t(0));
    EXPECT_TRUE(structure(simOf(w, a), *s.citadel)->hp > 0.9 * Rules::hp(StructureKind::citadel));
    EXPECT_TRUE(drivenUnit(w, a)->task == Unit::Task::atGun);
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

// D3: any pilot upgrades any turret of the realm from its own wallet (no
// tax), standing near it; refused too far, when the wallet is short, at the
// last mark, and for a building that takes no marks. Every pilot in the
// region is told who paid.
TEST(defence_any_pilot_upgrades_a_turret) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    const auto gun = weld(w, a, StructureKind::gatling, s.east + Vec2(0, 14));
    ASSERT_TRUE(gun.has_value());
    w.leave(a);
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.setProgress(b, at(10));
    ASSERT_TRUE(w.where(b) == w.where(a) || w.where(b).has_value());
    Unit* u = drivenUnit(w, b);
    ASSERT_TRUE(u != nullptr);
    const Vec2 g = structure(simOf(w, b), *gun)->position;
    u->position = g + Vec2(0, 12);
    u->task = Unit::Task::idle;
    fill(w, b, 1000, 100);
    EXPECT_TRUE(w.upgradeTurret(b, *gun, DefenceStat::air).value_or("") == "Too far: stand within 4 cells of the Gatling Gun");
    besides(w, b, *gun);
    const double realm = double(w.realmWallet().ore);
    w.takeNews(b);
    ASSERT_TRUE(!w.upgradeTurret(b, *gun, DefenceStat::air).has_value());
    EXPECT_TRUE(*w.wallet(b) == (WorldMap::Wallet{900, 50}));
    EXPECT_NEAR(double(w.realmWallet().ore), realm, 1e-9);
    EXPECT_TRUE(structure(simOf(w, b), *gun)->marks == (TurretMarks{.air = 1}));
    // Its toast: who paid, the stat, the mark.
    bool told = false;
    for (const auto& n : w.takeNews(b))
        told = told || (n.kind == net::ProgressMessage::NewsKind::upgraded && n.id == "gatling/air" && n.value == 1 &&
                        n.by == WorldMap::callsign(keyB));
    EXPECT_TRUE(told);
    ASSERT_TRUE(!w.upgradeTurret(b, *gun, DefenceStat::rate).has_value());
    EXPECT_TRUE(*w.wallet(b) == (WorldMap::Wallet{800, 0}));
    EXPECT_TRUE(w.upgradeTurret(b, *gun, DefenceStat::damage).value_or("") == "Your wallet is short: 50 MH needed, you have 0");
    fill(w, b, 5000, 2000);
    ASSERT_TRUE(!w.upgradeTurret(b, *gun, DefenceStat::air).has_value());
    ASSERT_TRUE(!w.upgradeTurret(b, *gun, DefenceStat::air).has_value());
    EXPECT_TRUE(w.upgradeTurret(b, *gun, DefenceStat::air).value_or("") == "Air reach is at its last mark");
    EXPECT_TRUE(*w.wallet(b) == (WorldMap::Wallet{5000 - 300 - 800, 2000 - 200 - 500}));
    // Its weapon: air reach 6 now.
    EXPECT_NEAR(Defence::weapon(*structure(simOf(w, b), *gun))->airRange, 6, 1e-9);
    // Not a turret: the Citadel's marks are the realm's.
    besides(w, b, *s.citadel);
    EXPECT_TRUE(w.upgradeTurret(b, *s.citadel, DefenceStat::rate).value_or("") == "Only a Sentinel or a Gatling Gun takes marks");
    w.step();
    EXPECT_TRUE(simOf(w, b).state.structure(*gun)->marks == (TurretMarks{.rate = 1, .air = 3}));
}

// D3: a turret's kills count on its card; its marks and kills are saved
// with it and loaded again; felled, it loses both (a new one in its place
// starts bare).
TEST(defence_turret_marks_and_kills_saved_and_lost) {
    const std::string path = freshFile("turret-marks");
    std::string why;
    int64_t gunId = 0;
    Vec2 g;
    {
        Home s;
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        const auto gun = weld(w, a, StructureKind::gatling, s.east + Vec2(0, 14));
        ASSERT_TRUE(gun.has_value());
        gunId = *gun;
        g = structure(simOf(w, a), *gun)->position;
        besides(w, a, *gun);
        fill(w, a, 5000, 2000);
        ASSERT_TRUE(!w.upgradeTurret(a, *gun, DefenceStat::damage).has_value());
        ASSERT_TRUE(!w.upgradeTurret(a, *gun, DefenceStat::range).has_value());
        drivenUnit(w, a)->position = s.east + Vec2(-20, -20);
        w.step();
        const size_t r = *w.where(a);
        const auto group = w.spawnBroodGroup(r, UnitKind::shardling, 3, WorldMap::Poi::building, *gun, g + Vec2(5, 0), true);
        ASSERT_TRUE(group.has_value());
        for (int i = 0; i < 20 * 60 && aliveIn(w, a, *group) > 0; i++) w.step();
        EXPECT_EQ(aliveIn(w, a, *group), int64_t(0));
        EXPECT_EQ(w.turretKills(r, *gun), int64_t(3));
        EXPECT_EQ(w.progressMessage(a)->turretKills[*gun], int64_t(3));
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        const auto rows = store->rows("SELECT mark_damage, mark_range, mark_air, kills FROM buildings WHERE id = " + std::to_string(*gun));
        ASSERT_TRUE(rows.size() == 1);
        EXPECT_TRUE(rows[0] == (std::vector<std::string>{"1", "1", "0", "3"}));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto save = store->load(&why);
    ASSERT_TRUE(save.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*save).empty());
    // Written again unchanged: nothing.
    EXPECT_EQ(store->write(w.save(), &why)->changed, int64_t(0));
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    w.step();
    const size_t r = *w.where(a);
    const auto loaded = simOf(w, a).state.structure(gunId);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->marks == (TurretMarks{.damage = 1, .range = 1}));
    EXPECT_EQ(w.turretKills(r, gunId), int64_t(3));

    // Felled: its marks and kills go with it.
    for (Structure& x : simOf(w, a).state.structures)
        if (x.id == gunId) x.hp = 0;
    for (int i = 0; i < 30; i++) w.step();
    EXPECT_TRUE(!simOf(w, a).state.structure(gunId).has_value());
    EXPECT_EQ(w.turretKills(r, gunId), int64_t(0));
    fill(w, a, 5000, 2000);
    w.setProgress(a, at(10));
    const auto again = weld(w, a, StructureKind::gatling, g + Vec2(Region::buildReach(StructureKind::gatling), 0));
    ASSERT_TRUE(again.has_value());
    EXPECT_TRUE(!simOf(w, a).state.structure(*again)->marks.has_value());
    EXPECT_TRUE(Defence::weapon(*simOf(w, a).state.structure(*again))->damage == UnitStats::gatling.damage);
}

// D3's shot scene (`defence air`): Gloamwings hovering past a bare gatling
// gun's air reach stay unhit; its air marks reach them.
TEST(defence_dev_air_scene_needs_the_air_marks) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(a));
    w.step();
    // Out from the start, as the shots stand.
    Unit* me = drivenUnit(w, a);
    me->position = me->position + Vec2(0, 30);
    me->heading = pi / 2;
    w.step();
    const std::string said = w.devDefence("defence air");
    std::printf("  %s\n", said.c_str());
    ASSERT_TRUE(said.find("group") != std::string::npos);
    Simulation& sim = simOf(w, a);
    Structure* gun = nullptr;
    for (Structure& s : sim.state.structures)
        if (s.kind == StructureKind::gatling) gun = &s;
    ASSERT_TRUE(gun != nullptr);
    const int64_t gunId = gun->id;
    auto report = [&](const char* when) {
        int64_t hit = 0;
        const Structure* g = sim.state.structure(gunId) ? &*std::find_if(sim.state.structures.begin(), sim.state.structures.end(),
                                                                       [&](const Structure& s) { return s.id == gunId; })
                                                        : nullptr;
        for (const Unit& u : sim.state.units)
            if (u.kind == UnitKind::gloamwing && u.bug) {
                hit += u.hp < Rules::hp(u.kind) ? 1 : 0;
                if (g)
                    std::printf("  %s: gap %.2f state %d hp %.0f\n", when,
                                distance(u.position, g->position) - Rules::radius(u.kind) - Rules::radius(g->kind), int(u.bug->state), u.hp);
            }
        return hit;
    };
    for (int i = 0; i < 4 * 60; i++) w.step();
    EXPECT_EQ(report("bare"), int64_t(0));
    for (Structure& s : sim.state.structures)
        if (s.id == gunId) s.marks = TurretMarks{.air = 3};
    for (int i = 0; i < 3 * 60; i++) w.step();
    EXPECT_TRUE(report("marked") >= 1);
}
