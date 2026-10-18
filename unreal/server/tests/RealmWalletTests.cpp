// The Citadel's tax and the realm wallet (C1, docs/prospector-progression.md
// "The realm's tax"): every unload into a pilot's wallet, at the realm's
// Citadel or a Hab Dome, pays the realm its cut; the XP and the deliver
// contracts count the whole load; pilots donate at the Citadel; two pilots'
// taxes add up; the realm wallet and what each pilot gave are saved, and a
// save from before (schema 8) starts with an empty realm wallet. Isolated
// scenes of a few seconds on The First Reach; files in build/save-tests.
// Run from unreal/server.

#include "test.h"

#include "CitadelSpot.h"

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
using Contract = net::Contract;

constexpr PilotId a = 81;
constexpr PilotId b = 82;
const std::string keyA = "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf";
const std::string keyB = "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint, no contracts drawn (they pay MH into the wallet).
WorldParams params() {
    WorldParams p;
    p.devBlueprints = true;
    p.contracts = false;
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

/// A load of `load` (MH when `hydrogen`) unloaded at `building`.
void unload(WorldMap& w, PilotId p, int64_t building, int64_t load, bool hydrogen) {
    besides(w, p, building);
    drivenUnit(w, p)->carrying = load;
    drivenUnit(w, p)->hydrogen = hydrogen ? std::optional<bool>(true) : std::nullopt;
    wait(w, p, 10);
}

/// Pilot A's Citadel and Hab Dome on the start region, east of the ore.
struct Home {
    explicit Home(WorldParams p = params()) : w(first(), p) {
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

// The tax at the Citadel and at a Hab Dome, ore and MH, a small load too:
// the wallet gets the load less the cut, the realm wallet the cut, the
// pilot's tally what it paid; the XP and a deliver contract count the whole
// load; the progress message tells it all.
TEST(realm_tax_on_unload_at_the_citadel_and_a_hab_dome) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value() && s.dome.has_value());
    WorldMap& w = s.w;
    EXPECT_TRUE(w.realmWallet() == WorldMap::Wallet{});
    w.setContracts(a, {Contract{1, Contract::Kind::deliverOre, Contract::State::open, 0, "ORE RUN", "", "", "", 300, 0, 50, 0}});
    fill(w, a, 1000, 100);

    // 25 ore at the Citadel: 3 to the realm.
    const double xp = w.progress(a)->xp;
    unload(w, a, *s.citadel, 25, false);
    EXPECT_EQ(w.wallet(a)->ore, int64_t(1000 + 22));
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{3, 0}));
    EXPECT_TRUE(*w.taxed(a) == (WorldMap::Wallet{3, 0}));
    EXPECT_NEAR(w.progress(a)->xp - xp, 25 * WorldXp::ore, 1e-9);
    EXPECT_EQ(w.contracts(a)[0].have, int64_t(25));

    // 20 MH at the Hab Dome: 2 to the realm.
    unload(w, a, *s.dome, 20, true);
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(100 + 18));
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{3, 2}));

    // A small load still pays: 1 of 4; a load of 1 pays nothing.
    unload(w, a, *s.dome, 4, false);
    EXPECT_EQ(w.wallet(a)->ore, int64_t(1000 + 22 + 3));
    unload(w, a, *s.citadel, 1, false);
    EXPECT_EQ(w.wallet(a)->ore, int64_t(1000 + 22 + 3 + 1));
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{4, 2}));
    EXPECT_TRUE(*w.taxed(a) == (WorldMap::Wallet{4, 2}));
    EXPECT_EQ(w.contracts(a)[0].have, int64_t(25 + 4 + 1));

    const auto m = w.progressMessage(a);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->taxPercent, Progression::realmTaxPercent);
    EXPECT_TRUE(m->realmWallet == (net::ProgressMessage::Purse{4, 2}));
    EXPECT_TRUE(m->taxed == (net::ProgressMessage::Purse{4, 2}));
    EXPECT_TRUE(m->donated == (net::ProgressMessage::Purse{}));
}

// Donating: at the Citadel, from the wallet into the realm's; refused away
// from it, for more than the pilot has, and for nothing. A Hab Dome is a
// depot, not where one donates.
TEST(realm_donate_at_the_citadel) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value() && s.dome.has_value());
    WorldMap& w = s.w;
    fill(w, a, 1000, 100);

    besides(w, a, *s.dome);
    EXPECT_TRUE(w.donate(a, 100, 0).value_or("").rfind("Donate at a Citadel", 0) == 0);
    Unit* u = drivenUnit(w, a);
    u->position = u->position + Vec2(0, -30);
    w.step();
    EXPECT_TRUE(w.donate(a, 100, 0).has_value());
    EXPECT_TRUE(w.realmWallet() == WorldMap::Wallet{});

    besides(w, a, *s.citadel);
    EXPECT_TRUE(w.donate(a, 1001, 0).value_or("") == "Not enough ore: you have 1000");
    EXPECT_TRUE(w.donate(a, 0, 101).value_or("") == "Not enough MH: you have 100");
    EXPECT_TRUE(w.donate(a, 0, 0).value_or("") == "Nothing to donate");
    EXPECT_TRUE(w.donate(a, -5, 10).value_or("") == "Nothing to donate");
    EXPECT_EQ(w.wallet(a)->ore, int64_t(1000));
    EXPECT_TRUE(w.realmWallet() == WorldMap::Wallet{});

    EXPECT_TRUE(!w.donate(a, 1000, 0).has_value());
    EXPECT_TRUE(!w.donate(a, 0, 40).has_value());
    EXPECT_EQ(w.wallet(a)->ore, int64_t(0));
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(60));
    // The sim's bank too: the next step keeps it.
    w.step();
    EXPECT_EQ(w.wallet(a)->ore, int64_t(0));
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{1000, 40}));
    EXPECT_TRUE(*w.donated(a) == (WorldMap::Wallet{1000, 40}));
    EXPECT_TRUE(*w.taxed(a) == WorldMap::Wallet{});

    // D1 spends from it; never more than it holds.
    EXPECT_TRUE(!w.spendRealm({1001, 0}));
    EXPECT_TRUE(!w.spendRealm({-1, 0}));
    EXPECT_TRUE(w.spendRealm({400, 40}));
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{600, 0}));
}

// Two pilots unload at the same Citadel: their taxes add up in the one
// realm wallet, each pilot's tally its own, and both are told the realm's.
TEST(realm_two_pilots_taxes_add_up) {
    Home s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    unload(w, a, *s.citadel, 25, false);
    w.leave(a);

    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.setProgress(b, at(10));
    unload(w, b, *s.citadel, 30, false);
    unload(w, b, *s.citadel, 20, true);
    EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{3 + 3, 2}));
    EXPECT_TRUE(*w.taxed(b) == (WorldMap::Wallet{3, 2}));

    // A back: its own tally kept, the realm's as B left it.
    w.leave(b);
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(*w.taxed(a) == (WorldMap::Wallet{3, 0}));
    EXPECT_TRUE(w.progressMessage(a)->realmWallet == (net::ProgressMessage::Purse{6, 2}));
}

// The realm wallet and each pilot's tax and donations, saved (schema 9) and
// loaded into a new server; a save from before (schema 8) starts with an
// empty realm wallet and nothing given.
TEST(realm_wallet_saved_and_loaded) {
    const std::string path = freshFile("realm-wallet");
    std::string why;
    {
        Home s;
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        fill(w, a, 500, 50);
        unload(w, a, *s.citadel, 25, false);
        besides(w, a, *s.citadel);
        ASSERT_TRUE(!w.donate(a, 100, 10).has_value());
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), WorldStore::version());
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        const auto rows = store->rows("SELECT ore, hydrogen FROM realm_wallet");
        ASSERT_TRUE(rows.size() == 1);
        EXPECT_TRUE(rows[0][0] == "103" && rows[0][1] == "10");
    }
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        const auto save = store->load(&why);
        ASSERT_TRUE(save.has_value());
        WorldMap w(first(), params());
        EXPECT_TRUE(w.restore(*save).empty());
        EXPECT_TRUE(w.realmWallet() == (WorldMap::Wallet{103, 10}));
        ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
        EXPECT_TRUE(*w.taxed(a) == (WorldMap::Wallet{3, 0}));
        EXPECT_TRUE(*w.donated(a) == (WorldMap::Wallet{100, 10}));
        // Written again unchanged: nothing.
        const auto n = store->write(w.save(), &why);
        ASSERT_TRUE(n.has_value());
    }

    // A file made by schema 8, with a pilot: opened by this build it
    // migrates, its realm wallet empty, the pilot's tally nothing.
    const std::string old = freshFile("realm-wallet-v8");
    const std::vector<WorldStore::Migration> v8(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 8);
    {
        auto store = WorldStore::open(old, "first", &why, v8);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(8));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, contracts, rig) "
                                "VALUES ('" + keyA + "', 40, 4, 'start', NULL, '', '', 1, '', '')",
                                &why));
    }
    auto store = WorldStore::open(old, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto save = store->load(&why);
    ASSERT_TRUE(save.has_value());
    EXPECT_EQ(save->realmOre, int64_t(0));
    EXPECT_EQ(save->realmHydrogen, int64_t(0));
    ASSERT_TRUE(save->pilots.size() == 1);
    EXPECT_EQ(save->pilots[0].ore, int64_t(40));
    EXPECT_EQ(save->pilots[0].taxedOre + save->pilots[0].donatedOre, int64_t(0));
}
