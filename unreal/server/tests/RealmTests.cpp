// Buildings belong to the realm (R1, docs/prospector-progression.md
// "Buildings in the MMO"): a second pilot unloads at, respawns at and
// respecs at the first pilot's Hab Dome and Citadel; the limits are the
// realm's; a finished building is told to every pilot with who built it; a
// pilot landing where the Citadel stands reaches it instead of building one;
// a save from before (schema 7, buildings with an owner) loads with its
// buildings the realm's. Isolated scenes of a few seconds on The First
// Reach; files in build/save-tests. Run from unreal/server.

#include "test.h"

#include "CitadelSpot.h"

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
using Contract = net::Contract;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId a = 71;
constexpr PilotId b = 72;
const std::string keyA = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf";
const std::string keyB = "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint, no contracts (they pay MH into the wallet).
WorldParams params() {
    WorldParams p;
    p.devBlueprints = true;
    p.contracts = false;
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
/// (ground; as FirstTenTests.cpp) and welded to the end: its Constructed
/// event fires. Nil: refused, or not finished.
std::optional<int64_t> weld(WorldMap& w, PilotId p, StructureKind kind, Vec2 stand) {
    Simulation& sim = simOf(w, p);
    Unit* u = drivenUnit(w, p);
    u->task = Unit::Task::idle;
    u->position = stand;
    u->heading = pi;
    const auto at = sim.snap(kind, stand + Vec2(-1, 0) * Region::buildReach(kind));
    if (!at) return std::nullopt;
    const int64_t id = sim.state.nextID;
    if (const auto why = w.build(p, kind, *at)) {
        std::printf("  %s refused: %s\n", Simulation::title(kind).c_str(), why->c_str());
        return std::nullopt;
    }
    for (int i = 0; i < int((Rules::buildTime(kind) + 5) * 60); i++) {
        wait(w, p, 1);
        if (const Structure* s = structure(sim, id); s && s->complete()) return id;
    }
    return std::nullopt;
}

/// The driven Prospector at `b`'s edge, where it unloads.
void besides(WorldMap& w, PilotId p, const Structure& s) {
    Unit* u = drivenUnit(w, p);
    const Vec2 out = normalize(u->position - s.position);
    u->position = s.position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(s.kind) + 0.4);
    u->task = Unit::Task::idle;
    w.step();
}

/// Two buildings of pilot A's on the start region, east of the ore: a
/// Citadel and a Hab Dome, welded; then A leaves the world.
struct TwoPilots {
    explicit TwoPilots(WorldParams p = params()) : w(first(), p) {
        w.seat(a, keyA);
        w.setProgress(a, at(10));
        fill(w, a, 3000, 500);
        const Vec2 east = w.regionOf(a)->places()->ground(Vec2(72, 48));
        citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, east));
        dome = weld(w, a, StructureKind::habDome, east + Vec2(0, 9));
    }
    WorldMap w;
    std::optional<int64_t> citadel;
    std::optional<int64_t> dome;
};

std::string freshFile(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

// Pilot B uses what pilot A built: it unloads at A's Hab Dome and Citadel,
// respecs at A's Citadel, and comes back at them when it falls. A second
// Citadel in its region is refused (one a region, R3), and the progress message counts the
// realm's buildings and names their builder.
TEST(realm_second_pilot_uses_the_first_pilots_hab_dome_and_citadel) {
    TwoPilots s;
    ASSERT_TRUE(s.citadel.has_value() && s.dome.has_value());
    WorldMap& w = s.w;
    EXPECT_EQ(w.realmCount(StructureKind::citadel), int64_t(1));
    EXPECT_EQ(w.realmCount(StructureKind::habDome), int64_t(1));
    w.leave(a);

    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.setProgress(b, at(10));
    ASSERT_TRUE(drivenUnit(w, b) != nullptr);
    ASSERT_TRUE(!w.spend(b, Skill::quickDrill));
    const Simulation& sim = simOf(w, b);
    const Structure citadel = *structure(sim, *s.citadel);
    const Structure dome = *structure(sim, *s.dome);

    // The limits: no second Citadel in this region (R3: one a region).
    const Vec2 west = w.regionOf(b)->places()->ground(Vec2(30, 48));
    fill(w, b, 1000, 100);
    const int64_t ore = w.wallet(b)->ore;
    EXPECT_TRUE(!weld(w, b, StructureKind::citadel, west).has_value());
    EXPECT_EQ(w.realmCount(StructureKind::citadel), int64_t(1));
    EXPECT_EQ(w.wallet(b)->ore, ore);

    // A's Hab Dome is B's depot.
    besides(w, b, dome);
    drivenUnit(w, b)->carrying = 25;
    drivenUnit(w, b)->hydrogen = false;
    wait(w, b, 10);
    // Less the realm's tax (C1): 3 of 25, 3 of 30.
    EXPECT_EQ(w.wallet(b)->ore, ore + 22);
    // And A's Citadel: its depot, and where it respecs.
    besides(w, b, citadel);
    drivenUnit(w, b)->carrying = 30;
    drivenUnit(w, b)->hydrogen = false;
    wait(w, b, 10);
    EXPECT_EQ(w.wallet(b)->ore, ore + 22 + 27);
    EXPECT_TRUE(!w.respec(b).has_value());

    // B falls far west: it comes back at the realm's nearest home.
    drivenUnit(w, b)->position = west;
    drivenUnit(w, b)->hp = 0;
    w.step();
    const auto back = w.respawning(b);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(back->home.has_value());

    // Told to B: the realm's counts, A's callsign on both, its own.
    const auto m = *w.progressMessage(b);
    EXPECT_EQ(m.realm.at(StructureKind::citadel), int64_t(1));
    EXPECT_EQ(m.realm.at(StructureKind::habDome), int64_t(1));
    EXPECT_TRUE(m.builders.count(*s.citadel) == 1 && m.builders.at(*s.citadel) == WorldMap::callsign(keyA));
    EXPECT_TRUE(m.builders.count(*s.dome) == 1 && m.builders.at(*s.dome) == WorldMap::callsign(keyA));
    EXPECT_EQ(m.callsign, WorldMap::callsign(keyB));
    EXPECT_TRUE(m.callsign != m.builders.at(*s.citadel));
    // A callsign tells nothing of the key.
    EXPECT_TRUE(WorldMap::callsign(keyA).rfind("Pilot ", 0) == 0 && WorldMap::callsign(keyA).size() == 10);
    EXPECT_TRUE(WorldMap::callsign(keyA).find(keyA.substr(0, 4)) == std::string::npos);
}

// A building finished is told to every pilot in the world, with its
// builder's callsign; only the builder gets its feat.
TEST(realm_building_finished_is_told_to_every_pilot) {
    WorldMap w(first(), params());
    w.seat(a, keyA);
    w.setProgress(a, at(10));
    w.seat(b, keyB);
    fill(w, a, 3000, 500);
    (void)w.takeNews(a);
    (void)w.takeNews(b);
    const auto dome = weld(w, a, StructureKind::habDome, w.regionOf(a)->places()->ground(Vec2(72, 48)));
    ASSERT_TRUE(dome.has_value());
    auto built = [&](PilotId p) {
        std::vector<net::ProgressMessage::News> out;
        for (const auto& n : w.takeNews(p))
            if (n.kind == NewsKind::built) out.push_back(n);
        return out;
    };
    const auto toA = built(a);
    const auto toB = built(b);
    ASSERT_TRUE(toA.size() == 1 && toB.size() == 1);
    EXPECT_EQ(toB[0].id, std::string("habDome"));
    EXPECT_EQ(toB[0].by, WorldMap::callsign(keyA));
    EXPECT_EQ(toA[0].by, WorldMap::callsign(keyA));
    EXPECT_TRUE(w.progress(a)->feats.count(WorldXp::built(StructureKind::habDome)) == 1);
    EXPECT_TRUE(w.progress(b)->feats.count(WorldXp::built(StructureKind::habDome)) == 0);
    // Through the wire.
    net::ProgressMessage m = *w.progressMessage(b);
    m.news = toB;
    net::ProgressMessage back;
    ASSERT_TRUE(net::decode(net::encode(m), back));
    EXPECT_TRUE(back.realm == m.realm && back.builders == m.builders && back.callsign == m.callsign);
    EXPECT_TRUE(back.news == m.news);
    EXPECT_EQ(back.realm.at(StructureKind::habDome), int64_t(1));
}

// The opening chain in a realm (O1, R1): pilot A, new in an empty world,
// builds the Citadel at step 2. Pilot B, new after it, reaches it instead:
// step 2 says who built it and is done at its edge.
TEST(realm_second_pilots_citadel_step_reaches_the_realms) {
    WorldParams p = params();
    p.contracts = true;
    p.firstStep = 2;
    TwoPilots s(p);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    auto stepOf = [&](PilotId who) -> std::optional<Contract> {
        for (const Contract& c : w.contracts(who))
            if (c.state == Contract::State::open) return c;
        return std::nullopt;
    };
    // A built it: its step 2 is paid, the chain goes on.
    ASSERT_TRUE(stepOf(a).has_value());
    EXPECT_EQ(int(stepOf(a)->step), 3);
    w.leave(a);

    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    const auto two = stepOf(b);
    ASSERT_TRUE(two.has_value());
    EXPECT_EQ(int(two->step), 2);
    EXPECT_TRUE(two->kind == Contract::Kind::reach);
    EXPECT_EQ(two->target, std::string("start/Citadel"));
    EXPECT_TRUE(two->pitch.find(WorldMap::callsign(keyA)) != std::string::npos);
    std::printf("  B's step 2: %s: %s\n", two->title.c_str(), two->pitch.c_str());
    // Not done away from it.
    wait(w, b, 30);
    EXPECT_EQ(int(stepOf(b)->step), 2);
    besides(w, b, *structure(simOf(w, b), *s.citadel));
    wait(w, b, 2);
    ASSERT_TRUE(stepOf(b).has_value());
    EXPECT_EQ(int(stepOf(b)->step), 3);
}

// A save from before the realm (schema 7: buildings with an owner) loads:
// migrated to 8, its buildings stand as the realm's, their builder kept for
// credit; a new pilot unloads at the old pilot's Hab Dome; saved again, the
// builder is written in `built_by`.
TEST(realm_old_save_buildings_become_the_realms) {
    const std::string path = freshFile("v7realm");
    std::string why;
    const std::vector<WorldStore::Migration> v7(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 7);
    {
        auto store = WorldStore::open(path, "first", &why, v7);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(7));
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 80, 9)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO buildings (region, id, kind, x, y, owner, build_left, hp) VALUES "
                                "('start', 4, 'citadel', 60, 40, '" + keyA + "', NULL, 1500), "
                                "('start', 5, 'habDome', 70, 40, '" + keyA + "', NULL, 600)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, contracts, rig) VALUES ('" +
                                keyA + "', 40, 7, 'start', NULL, '', '" + SessionStore::encode(at(10), false) + "', 1, '', '')", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    EXPECT_TRUE(store->rows("SELECT count(*) FROM pragma_table_info('buildings') WHERE name = 'built_by'")[0][0] == "1");
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->regions.size() >= 1);
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    // B, new: A's buildings are the realm's.
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.setProgress(b, at(10));
    EXPECT_EQ(w.realmCount(StructureKind::citadel), int64_t(1));
    EXPECT_TRUE(w.realmFinished(StructureKind::habDome));
    const Simulation& sim = simOf(w, b);
    ASSERT_TRUE(structure(sim, 5) != nullptr);
    const int64_t ore = w.wallet(b)->ore;
    besides(w, b, *structure(sim, 5));
    drivenUnit(w, b)->carrying = 25;
    drivenUnit(w, b)->hydrogen = false;
    wait(w, b, 10);
    EXPECT_EQ(w.wallet(b)->ore, ore + 22);
    EXPECT_EQ(w.progressMessage(b)->builders.at(4), WorldMap::callsign(keyA));
    // Saved again: the builder in `built_by`.
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    const auto rows = store->rows("SELECT id, built_by FROM buildings ORDER BY id");
    ASSERT_TRUE(rows.size() == 2);
    EXPECT_TRUE(rows[0][1] == keyA && rows[1][1] == keyA);
}
