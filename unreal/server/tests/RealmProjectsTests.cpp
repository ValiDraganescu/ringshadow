// Realm projects (R2, docs/mmorpg-play.md "Realm projects"): the realm
// wallet buys a world day of something the whole realm feels. Started at the
// realm's Citadel by a human pilot, paid by the realm wallet, one of each kind
// at a time, a frontier pilot never; the Beacon's fares, the Relay's night
// sight, the Armoury's marks, the Dock Crew's mercenaries; the end on the
// world's clock while the world is down; saved (schema 22) and an older save
// with none. Isolated scenes of a few seconds on The First Reach; files in
// build/save-tests. Run from unreal/server.

#include "test.h"

#include "CitadelSpot.h"

#include "Defence.h"
#include "Flight.h"
#include "RealmProjects.h"
#include "Rules.h"
#include "WorldClock.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using Kind = RealmProject::Kind;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId a = 85;
const std::string keyA = "e1e2e3e4e5e6e7e8e9eaebecedeeef10";

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
    p.devBlueprints = true;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.frontier = 0;
    return p;
}

Simulation& simOf(WorldMap& w, PilotId p) { return w.regionOf(p)->simulation(); }

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
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

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// Pilot A at level 10 with a Citadel east of the start's ore, standing at
/// its edge (where it unloads), and a full realm wallet.
struct Home {
    Home() : w(first(), params()) {
        w.seat(a, keyA);
        ProspectorProgress g;
        g.xp = Progression::xp(10);
        for (const StructureKind k : Progression::blueprintKinds()) g.giveBlueprint(k, 9);
        w.setProgress(a, g);
        simOf(w, a).state.players[0].ore = 3000;
        simOf(w, a).state.players[0].hydrogen = 500;
        w.step();
        Simulation& sim = simOf(w, a);
        Unit* u = drivenUnit(w, a);
        u->task = Unit::Task::idle;
        u->position = rig::citadelStand(w, a, w.regionOf(a)->places()->ground(Vec2(72, 48)));
        u->heading = pi;
        const auto spot = sim.snap(StructureKind::citadel, u->position + Vec2(-1, 0) * Region::buildReach(StructureKind::citadel));
        const int64_t id = sim.state.nextID;
        if (!spot || w.build(a, StructureKind::citadel, *spot)) return;
        for (int i = 0; i < int((Rules::buildTime(StructureKind::citadel) + 5) * 60); i++) {
            wait(w, a, 1);
            if (const Structure* s = structure(sim, id); s && s->complete()) {
                citadel = id;
                break;
            }
        }
        if (!citadel) return;
        const Structure c = *structure(sim, *citadel);
        u = drivenUnit(w, a);
        u->position = c.position + Vec2(1, 0) * (Rules::radius(c.kind) + 0.4);
        u->task = Unit::Task::idle;
        w.step();
        w.setRealmWallet({50000, 5000});
    }
    WorldMap w;
    std::optional<int64_t> citadel;
};

int countNews(WorldMap& w, PilotId p, const std::string& id) {
    int n = 0;
    for (const auto& x : w.takeNews(p))
        if (x.kind == NewsKind::project && x.id == id) n++;
    return n;
}

std::string freshFile(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

/// Started at the Citadel, paid by the realm wallet (not the pilot's), told
/// to the pilot; one of each kind at a time, other kinds alongside; a short
/// wallet and a frontier pilot refused; away from the Citadel refused.
TEST(realm_projects_start_and_the_rules) {
    Home h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const auto own0 = *w.wallet(a);
    const auto realm0 = w.realmWallet();
    const RealmProject::Price beacon = RealmProject::price(Kind::beacon, 0);
    // X2: a pilot off line, its ledger filling.
    const std::string keyAway = "0a0b0c0d0e0f10111213141516171819";
    ASSERT_TRUE(w.seat(89, keyAway) == WorldMap::Seated::fresh);
    w.leave(89);
    ASSERT_TRUE(!w.startProject(a, Kind::beacon).has_value());
    EXPECT_TRUE(w.projectOn(Kind::beacon));
    EXPECT_EQ(w.realmWallet().ore, realm0.ore - beacon.ore);
    EXPECT_EQ(w.realmWallet().hydrogen, realm0.hydrogen - beacon.hydrogen);
    EXPECT_TRUE(*w.wallet(a) == own0);
    EXPECT_EQ(w.projectRuns(Kind::beacon), int64_t(1));
    EXPECT_NEAR(w.projects()[size_t(Kind::beacon)]->ends, w.clock() + w.params().dayLength, 1e-6);
    EXPECT_EQ(w.projects()[size_t(Kind::beacon)]->by, WorldMap::callsign(keyA));
    EXPECT_EQ(countNews(w, a, "start/beacon"), 1);
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::projectStart && (row.who == WorldMap::callsign(keyA) && row.what == "the Beacon"));
        EXPECT_TRUE(found);
    }
    // One of a kind at a time; another kind alongside.
    const auto again = w.startProject(a, Kind::beacon);
    ASSERT_TRUE(again.has_value());
    std::printf("  again: %s\n", again->c_str());
    ASSERT_TRUE(!w.startProject(a, Kind::relay).has_value());
    EXPECT_TRUE(w.projectOn(Kind::beacon) && w.projectOn(Kind::relay));
    // A short wallet.
    w.setRealmWallet({100, 0});
    const auto shortOf = w.startProject(a, Kind::armoury);
    ASSERT_TRUE(shortOf.has_value());
    std::printf("  short: %s\n", shortOf->c_str());
    EXPECT_TRUE(!w.projectOn(Kind::armoury));
    // A frontier pilot never starts one, whatever the wallet.
    w.setRealmWallet({50000, 5000});
    const auto frontier = w.projectRefusal("frontier:vesper", Kind::armoury);
    ASSERT_TRUE(frontier.has_value());
    std::printf("  frontier: %s\n", frontier->c_str());
    // Away from the Citadel.
    drivenUnit(w, a)->position = drivenUnit(w, a)->position + Vec2(12, 0);
    w.step();
    EXPECT_TRUE(w.startProject(a, Kind::armoury).has_value());
}

/// The Beacon: fares halved realm-wide, a quarter from a Spacedock (rounded
/// up), nothing with Ringwalker as before.
TEST(realm_projects_beacon_halves_the_fares) {
    for (int64_t steps = 0; steps <= 4; steps++) {
        const int64_t whole = Flight::hydrogen(steps);
        EXPECT_EQ(Flight::price(steps, false, false, true), (whole + 1) / 2);
        EXPECT_EQ(Flight::price(steps, true, false, true), (whole + 3) / 4);
        EXPECT_EQ(Flight::price(steps, true, false, false), (whole + 1) / 2);
        EXPECT_EQ(Flight::price(steps, false, true, true), int64_t(0));
    }
    EXPECT_EQ(Flight::price(1, false, false, true), int64_t(8));
    EXPECT_EQ(Flight::price(1, true, false, true), int64_t(4));
}

/// The Relay: every region's pilots see half again as far at night; their
/// headlamp's wedge and every other side's sight are as before.
TEST(realm_projects_relay_widens_the_night) {
    Home h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    auto seen = [](const Simulation& sim) {
        const auto sight = sim.sight(Pilot::player);
        size_t n = 0;
        for (uint8_t c : sight->seen) n += c ? 1 : 0;
        return n;
    };
    w.setClock(WorldClock::timeOf(1.0, w.clock()));
    for (int i = 0; i < 10; i++) w.step();
    const size_t before = seen(simOf(w, a));
    ASSERT_TRUE(!w.startProject(a, Kind::relay).has_value());
    for (size_t r = 0; r < w.regionCount(); r++) EXPECT_NEAR(w.region(r).simulation().nightWiden, RealmProject::relaySight, 1e-9);
    for (int i = 0; i < 10; i++) w.step();
    const size_t after = seen(simOf(w, a));
    std::printf("  midnight: %zu cells seen, %zu with the Relay\n", before, after);
    EXPECT_TRUE(after > before + before / 4);
    // The rule itself: at full night 0.75 of a viewer's sight, not 0.5.
    EXPECT_NEAR(Rules::sightScale(0.0, Rules::nightSight * RealmProject::relaySight), 0.75, 1e-9);
    EXPECT_NEAR(Rules::sightScale(12.0, Rules::nightSight * RealmProject::relaySight), 1.0, 1e-9);
}

/// The Armoury: every turret of the realm fights one mark up in each stat,
/// the Citadel's corner guns too, to the marks' cap; others' as before.
TEST(realm_projects_armoury_one_mark_up) {
    Home h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    ASSERT_TRUE(!w.startProject(a, Kind::armoury).has_value());
    for (size_t r = 0; r < w.regionCount(); r++) EXPECT_EQ(w.region(r).simulation().turretBonus, RealmProject::armouryMarks);
    Structure g(1, StructureKind::gatling, Pilot::player, Vec2(0, 0));
    const UnitStats plain = *Defence::weapon(g), up = *Defence::weapon(g, 1);
    EXPECT_NEAR(up.cooldown, plain.cooldown / (1 + Defence::ratePerMark), 1e-9);
    EXPECT_NEAR(up.damage, plain.damage * (1 + Defence::damagePerMark), 1e-9);
    EXPECT_NEAR(up.range, plain.range + Defence::rangePerMark, 1e-9);
    // At the cap already: no more.
    g.marks = TurretMarks{Defence::marksMax, Defence::marksMax, Defence::marksMax, Defence::marksMax};
    EXPECT_NEAR(Defence::weapon(g, 1)->damage, Defence::weapon(g)->damage, 1e-9);
    // The Citadel's corner guns, armed with the realm's marks.
    const Structure* c = structure(simOf(w, a), *h.citadel);
    ASSERT_TRUE(c && c->marks.has_value());
    EXPECT_NEAR(Defence::weapon(*c, 1)->damage, UnitStats::citadelGuns.damage * (1 + Defence::damagePerMark * double(c->marks->damage + 1)),
                1e-9);
}

/// The Dock Crew: mercenaries at 1/12 of the pilot's rate, not 1/24, and the
/// rate the pilot is told doubles.
TEST(realm_projects_dock_crew_doubles_the_mercenaries) {
    for (const int64_t level : {5, 15, 30})
        EXPECT_NEAR(WorldMercs::rate(level, true), 2 * WorldMercs::rate(level), 1e-12);
    Home h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    const double before = w.progress(a) ? WorldMercs::rate(10) : 0;
    ASSERT_TRUE(!w.startProject(a, Kind::dockCrew).has_value());
    w.step();
    EXPECT_NEAR(WorldMercs::rate(10, w.projectOn(Kind::dockCrew)), 2 * before, 1e-12);
}

/// A project ends a world day later, on the world's clock: here stepped,
/// and while the world is down (caught up when it comes back); the pilot on
/// line is told; the effects go with it.
TEST(realm_projects_end_on_the_worlds_clock) {
    Home h;
    ASSERT_TRUE(h.citadel.has_value());
    WorldMap& w = h.w;
    ASSERT_TRUE(!w.startProject(a, Kind::relay).has_value());
    ASSERT_TRUE(!w.startProject(a, Kind::armoury).has_value());
    w.takeNews(a);
    // X2: a pilot off line, its ledger filling.
    const std::string keyAway = "0a0b0c0d0e0f10111213141516171819";
    ASSERT_TRUE(w.seat(89, keyAway) == WorldMap::Seated::fresh);
    w.leave(89);
    // Most of the day: still on.
    w.idle(w.params().dayLength - 5);
    EXPECT_TRUE(w.projectOn(Kind::relay));
    w.idle(10);
    EXPECT_TRUE(!w.projectOn(Kind::relay) && !w.projectOn(Kind::armoury));
    EXPECT_EQ(countNews(w, a, "end/relay"), 1);
    {
        const auto* rows = w.ledgerOf(keyAway);
        ASSERT_TRUE(rows != nullptr);
        bool found = false;
        for (const net::AwayRow& row : *rows) found = found || (row.kind == net::AwayKind::projectEnd && (row.what == "The Relay"));
        EXPECT_TRUE(found);
    }
    EXPECT_NEAR(simOf(w, a).nightWiden, 1.0, 1e-9);
    EXPECT_EQ(simOf(w, a).turretBonus, int64_t(0));
    // Started again: the second costs a fifth more.
    const auto before = w.realmWallet();
    ASSERT_TRUE(!w.startProject(a, Kind::relay).has_value());
    EXPECT_EQ(before.ore - w.realmWallet().ore, RealmProject::price(Kind::relay, 1).ore);
    // The world down for longer than a day: it ends on the catch-up.
    WorldMap v(first(), params());
    ASSERT_TRUE(v.restore(w.save()).empty());
    EXPECT_TRUE(v.projectOn(Kind::relay));
    v.catchUp(v.params().dayLength * 1.5);
    EXPECT_TRUE(!v.projectOn(Kind::relay));
    EXPECT_EQ(v.projectRuns(Kind::relay), int64_t(2));
}

/// Saved (schema 22) and loaded into a new server, as they were; a save
/// from before (schema 21) has none.
TEST(realm_projects_saved_and_loaded) {
    const std::string path = freshFile("realm-projects");
    std::string why;
    std::array<std::optional<WorldMap::Project>, RealmProject::count> running{};
    {
        Home h;
        ASSERT_TRUE(h.citadel.has_value());
        WorldMap& w = h.w;
        ASSERT_TRUE(!w.startProject(a, Kind::beacon).has_value());
        ASSERT_TRUE(!w.startProject(a, Kind::dockCrew).has_value());
        running = w.projects();
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), WorldStore::version());
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        EXPECT_EQ(store->rows("SELECT kind FROM realm_projects").size(), size_t(2));
    }
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        const auto save = store->load(&why);
        ASSERT_TRUE(save.has_value());
        WorldMap w(first(), params());
        EXPECT_TRUE(w.restore(*save).empty());
        EXPECT_TRUE(w.projects() == running);
        EXPECT_EQ(w.projectRuns(Kind::beacon), int64_t(1));
        EXPECT_EQ(w.projectRuns(Kind::relay), int64_t(0));
    }
    // Schema 21: none.
    const std::string old = freshFile("realm-projects-v21");
    std::vector<WorldStore::Migration> v21;
    for (const auto& m : WorldStore::migrations())
        if (m.version <= 21) v21.push_back(m);
    {
        auto store = WorldStore::open(old, "first", &why, v21);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(21));
    }
    auto store = WorldStore::open(old, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto save = store->load(&why);
    ASSERT_TRUE(save.has_value());
    EXPECT_TRUE(save->projects.empty());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*save).empty());
    for (int k = 0; k < RealmProject::count; k++) EXPECT_TRUE(!w.projects()[size_t(k)].has_value());
}
