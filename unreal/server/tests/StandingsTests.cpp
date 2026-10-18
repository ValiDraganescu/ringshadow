// Realm standings (S4, docs/mmorpg-play.md "Standings"): each pilot's week,
// humans and frontier pilots alike, counted from its source: ore unloaded
// (a mercenary's deliveries to its hirer), bugs felled (its Prospector's and
// its defences'), canyon waves held (as the pay split counts the fighters),
// its buildings finished. The week ends after seven world days: the top
// hauler told, the counts started again; saved with the world (schema 24)
// and sent in a Standings message of its own. Isolated scenes; never a game.

#include "test.h"

#include "CitadelSpot.h"

#include "HeadlessClient.h"
#include "Progression.h"
#include "Rules.h"
#include "Server.h"
#include "Standings.h"
#include "WorldMap.h"
#include "WorldStore.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using NewsKind = net::ProgressMessage::NewsKind;
using Habit = WorldFrontier::Habit;
using FrontierPilot = WorldMap::FrontierPilot;

constexpr PilotId a = 61;
const std::string keyA = "5a5b5c5d5e5f505152535455565758ab";
constexpr PilotId b = 62;
const std::string keyB = "6a6b6c6d6e6f606162636465666768cd";
const std::string fork = "the-fork";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Only what the tests make happen.
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.missions = false;
    p.brood = false;
    p.canyons = false;
    p.firstStep = 0;
    p.frontier = 0;
    p.broodSeed = 0xca2e0ULL;
    p.frontierSeed = 0xf207ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(std::lround(seconds * 60));
    for (int i = 0; i < n; i++) w.step();
}

Simulation& simOf(WorldMap& w, PilotId p) { return w.regionOf(p)->simulation(); }

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
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

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// A `kind` put up west of the Prospector standing at `stand`, welded to
/// the end. Nil: refused, or not finished.
std::optional<int64_t> weld(WorldMap& w, PilotId p, StructureKind kind, Vec2 stand) {
    Simulation& sim = simOf(w, p);
    Unit* u = drivenUnit(w, p);
    u->task = Unit::Task::idle;
    u->position = stand;
    u->heading = pi;
    const auto spot = sim.snap(kind, stand + Vec2(-1, 0) * Region::buildReach(kind));
    if (!spot) return std::nullopt;
    const int64_t id = sim.state.nextID;
    if (w.build(p, kind, *spot)) return std::nullopt;
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

WorldMap::Standing of(const WorldMap& w, const std::string& key) {
    const auto it = w.standings().find(key);
    return it == w.standings().end() ? WorldMap::Standing{} : it->second;
}

/// A finished building of the realm at region cell `cell` (or round it).
int64_t place(WorldMap& w, size_t r, StructureKind kind, Vec2 cell) {
    Simulation& sim = w.region(r).simulation();
    const Vec2 near = w.region(r).places()->ground(cell);
    for (double rad = 0; rad <= 12; rad += 1)
        for (int k = 0; k < 16; k++) {
            const double t = k * 6.283185307179586 / 16;
            const auto spot = sim.snap(kind, near + Vec2(std::cos(t), std::sin(t)) * rad);
            if (!spot || sim.placementRefusal(kind, *spot)) continue;
            Structure s(sim.state.nextID++, kind, Pilot::player, *spot);
            s.hp = Rules::hp(kind);
            sim.state.structures.push_back(s);
            sim.reindex();
            sim.refreshNav();
            return s.id;
        }
    return -1;
}

/// The roster made and its miner come into region `r` at `cell`, wallet
/// empty (it only drills), the rest kept away.
FrontierPilot& miner(WorldMap& w, size_t r, Vec2 cell) {
    w.frontierRoster();
    FrontierPilot* out = nullptr;
    for (FrontierPilot& f : w.frontier())
        if (f.habit == Habit::miner && !out) out = &f;
    out->arrived = true;
    out->region = r;
    out->at = w.region(r).places()->ground(cell);
    out->wallet = WorldMap::Wallet{0, 0};
    out->progress.xp = Progression::xp(3);
    out->progress.regions.insert(w.loaded()->regions[r].file.id);
    for (FrontierPilot& f : w.frontier())
        if (&f != out) f.comes = 1e12;
    return *out;
}

std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

// A human's week from each source: its Citadel raised, ore unloaded (MH
// not), its mercenary's deliveries (as unloaded, before the tax).
TEST(standings_count_a_humans_ore_buildings_and_mercenaries) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    ProspectorProgress g;
    g.xp = Progression::xp(10);
    g.giveBlueprint(StructureKind::citadel);
    w.setProgress(a, g);
    simOf(w, a).state.players[0].ore = 20000;
    simOf(w, a).state.players[0].hydrogen = 3000;
    w.step();
    const auto citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, w.regionOf(a)->places()->ground(Vec2(72, 48))));
    ASSERT_TRUE(citadel.has_value());
    besides(w, a, *citadel);
    EXPECT_EQ(of(w, keyA).built, int64_t(1));
    EXPECT_EQ(of(w, keyA).ore, int64_t(0));
    drivenUnit(w, a)->carrying = 200;
    drivenUnit(w, a)->hydrogen = false;
    wait(w, a, 10);
    EXPECT_EQ(of(w, keyA).ore, int64_t(200));
    drivenUnit(w, a)->carrying = 40;
    drivenUnit(w, a)->hydrogen = true;
    wait(w, a, 10);
    EXPECT_EQ(of(w, keyA).ore, int64_t(200));
    // Its mercenary's hour, booked: the ore it brought, before the tax.
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    const int64_t wallet = w.wallet(a)->ore, realm = w.realmWallet().ore;
    w.catchUp(3600);
    const int64_t brought = (w.wallet(a)->ore - wallet) + (w.realmWallet().ore - realm);
    std::printf("  mercenary: %lld ore brought in an hour\n", (long long)brought);
    EXPECT_TRUE(brought > 0);
    EXPECT_EQ(of(w, keyA).ore, 200 + brought);
    EXPECT_EQ(of(w, keyA).bugs, int64_t(0));
    EXPECT_EQ(of(w, keyA).waves, int64_t(0));
}

// A canyon wave beaten by the pilot's gun: a wave held for its builder,
// a bug for each of the gun's kills.
TEST(standings_count_waves_held_and_bugs_felled) {
    WorldParams p = params();
    p.canyons = true;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    ProspectorProgress g;
    g.xp = Progression::xp(20);
    for (const StructureKind k : Progression::blueprintKinds()) g.giveBlueprint(k, 9);
    w.setProgress(a, g);
    w.step();
    const size_t r = *w.regionNamed("cold-fork");
    ASSERT_TRUE(w.devTransfer(a, r, w.region(r).places()->ground(Vec2(100, 52))));
    w.step();
    std::printf("  %s\n", w.devCanyon("canyon turrets 1 0 0").c_str());
    int64_t gun = 0;
    for (const Structure& s : w.region(r).simulation().state.structures)
        if (s.kind == StructureKind::gatling) gun = s.id;
    ASSERT_TRUE(gun != 0);
    const int64_t built = of(w, keyA).built;
    ASSERT_TRUE(w.canyonSend(r, fork, 2));
    for (int i = 0; i < 150 * 4 && w.canyon(r, fork)->out; i++) stepFor(w, 0.25);
    ASSERT_TRUE(!w.canyon(r, fork)->out);
    std::printf("  gun kills %lld, week: %lld bugs, %lld waves\n", (long long)w.turretKills(r, gun), (long long)of(w, keyA).bugs,
                (long long)of(w, keyA).waves);
    EXPECT_EQ(of(w, keyA).waves, int64_t(1));
    EXPECT_TRUE(w.turretKills(r, gun) > 0);
    EXPECT_EQ(of(w, keyA).bugs, w.turretKills(r, gun));
    // The dev line's gun is placed whole, not raised by the pilot.
    EXPECT_EQ(of(w, keyA).built, built);
}

// A frontier pilot's week is its own work: the ore it unloaded (booked
// here, no human near), as the tax and its wallet show it.
TEST(standings_count_a_frontier_pilots_own_work) {
    WorldParams p = params();
    p.frontier = 5;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    w.step();
    const size_t far = *w.regionNamed("slag-pockets");
    place(w, far, StructureKind::citadel, Vec2(72, 48));
    FrontierPilot& f = miner(w, far, Vec2(76, 52));
    stepFor(w, 120);
    const int64_t mined = f.wallet.ore + f.taxed.ore;
    std::printf("  %s: %lld ore in 120 s booked, week %lld\n", f.callsign.c_str(), (long long)mined, (long long)of(w, f.key).ore);
    EXPECT_TRUE(mined > 0);
    EXPECT_EQ(of(w, f.key).ore, mined);
    // Told with its callsign, a frontier pilot's row, above the human who did nothing.
    const auto m = w.standingsMessage(a);
    ASSERT_TRUE(m.has_value() && m->rows.size() == 2 && m->you == int64_t(1));
    EXPECT_TRUE(m->rows[0].callsign == f.callsign && m->rows[0].frontier && m->rows[0].rank == 1);
    EXPECT_TRUE(m->rows[1].callsign == WorldMap::callsign(keyA) && !m->rows[1].frontier && m->rows[1].rank == 2);
    EXPECT_EQ(net::standingLine(m->rows[0].rank, m->rows[0]), "1st · " + net::awayNumber(mined) + " ore this week");
}

// B2's curve: a frontier miner's booked hour, then its week, in the range
// its booked rate gives (WorldFrontier::bookRate, 1,489 ore an hour; the
// week is seven world days of 40 minutes).
TEST(standings_balance_a_frontier_haulers_week) {
    WorldParams p = params();
    p.frontier = 5;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    w.step();
    const size_t far = *w.regionNamed("slag-pockets");
    place(w, far, StructureKind::citadel, Vec2(72, 48));
    FrontierPilot& f = miner(w, far, Vec2(76, 52));
    w.leave(a);
    const double hour = 3600;
    for (int i = 0; i < int(hour); i++) w.idle(1.0);
    const double ore = double(of(w, f.key).ore);
    const double rate = WorldFrontier::bookRate() * hour;
    const double week = ore * w.weekLength() / hour;
    std::printf("  booked hour: %.0f ore (the curve %.0f), a week at that: %.0f (the curve's most %.0f)\n", ore, rate, week,
                WorldFrontier::bookRate() * w.weekLength());
    EXPECT_TRUE(ore >= 0.5 * rate);
    EXPECT_TRUE(ore <= rate + Rules::carry);
    EXPECT_TRUE(week <= WorldFrontier::bookRate() * w.weekLength() + Rules::carry * 7);
    EXPECT_NEAR(w.weekLength(), 7 * 2400.0, 1e-9);
}

// The week ends after seven world days: its top hauler told to every pilot
// on line (news) and off line (a row in its away ledger), the counts
// started again, the next week from the end; saved and loaded with the
// world (schema 24), an old save starts its week at the load.
TEST(standings_weekly_reset_saved_and_loaded) {
    const std::string path = fresh("standings");
    std::string why;
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::fresh);
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    EXPECT_EQ(w.weekStart(), 0.0);
    w.standNote(keyA, &WorldMap::Standing::ore, 18400);
    w.standNote(keyA, &WorldMap::Standing::bugs, 12);
    w.standNote(keyB, &WorldMap::Standing::waves, 2);
    w.standNote("frontier:vesper", &WorldMap::Standing::ore, 900);
    // B's view: A first, the frontier key next, B last with its own row.
    const auto m = *w.standingsMessage(b);
    ASSERT_TRUE(m.rows.size() == 3 && m.you == int64_t(2));
    EXPECT_TRUE(m.rows[0].callsign == WorldMap::callsign(keyA) && m.rows[0].ore == 18400);
    EXPECT_TRUE(m.rows[1].frontier && m.rows[2].waves == 2 && m.rows[2].rank == 3);
    EXPECT_NEAR(m.weekEnds, 7 * 2400.0, 1e-9);
    // Saved and loaded as they were.
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        EXPECT_TRUE(!store->rows("SELECT state FROM standings WHERE id = 1")[0][0].empty());
        const auto loaded = store->load(&why);
        ASSERT_TRUE(loaded.has_value());
        WorldMap v(first(), params());
        EXPECT_TRUE(v.restore(*loaded).empty());
        EXPECT_TRUE(v.standings() == w.standings());
        EXPECT_EQ(v.weekStart(), w.weekStart());
        EXPECT_TRUE(store->write(v.save(), &why)->changed == int64_t(0));
        // A save from before: the week starts at the load.
        WorldSave old = *loaded;
        old.standings.clear();
        old.clock = 5000;
        WorldMap u(first(), params());
        EXPECT_TRUE(u.restore(old).empty());
        EXPECT_TRUE(u.standings().empty());
        EXPECT_EQ(u.weekStart(), 5000.0);
    }
    // B off line; the week's end passes.
    w.leave(b);
    w.takeNews(a);
    w.setClock(w.weekEnds() + 30);
    w.step();
    std::vector<net::ProgressMessage::News> told;
    for (const auto& n : w.takeNews(a))
        if (n.kind == NewsKind::week) told.push_back(n);
    ASSERT_TRUE(told.size() == size_t(1));
    EXPECT_TRUE(told[0].id == "top" && told[0].by == WorldMap::callsign(keyA) && told[0].value == 18400);
    const auto* ledger = w.ledgerOf(keyB);
    ASSERT_TRUE(ledger != nullptr && !ledger->empty());
    EXPECT_EQ(net::awayLine(ledger->back()), "This week's top hauler: " + WorldMap::callsign(keyA) + ", 18,400 ore");
    {
        // The row kept through a save.
        WorldMap v(first(), params());
        EXPECT_TRUE(v.restore(w.save()).empty());
        ASSERT_TRUE(v.ledgerOf(keyB) != nullptr && !v.ledgerOf(keyB)->empty());
        EXPECT_TRUE(v.ledgerOf(keyB)->back() == ledger->back());
    }
    EXPECT_TRUE(w.standings().empty());
    EXPECT_NEAR(w.weekStart(), 7 * 2400.0, 1e-9);
    // Its own row still, nothing counted.
    const auto again = *w.standingsMessage(a);
    ASSERT_TRUE(again.rows.size() == size_t(1) && again.you == int64_t(0));
    EXPECT_EQ(again.rows[0].ore, int64_t(0));
    // A week with no hauler: "none". Down three weeks: told once.
    w.setClock(w.weekEnds() + 3 * w.weekLength());
    w.step();
    told.clear();
    for (const auto& n : w.takeNews(a))
        if (n.kind == NewsKind::week) told.push_back(n);
    ASSERT_TRUE(told.size() == size_t(1));
    EXPECT_TRUE(told[0].id == "none");
    EXPECT_NEAR(w.weekStart(), 5 * 7 * 2400.0, 1e-9);
    // The dev line ends it now.
    w.standNote(keyA, &WorldMap::Standing::ore, 10);
    std::printf("  %s\n", w.devWeek("week end").c_str());
    EXPECT_TRUE(w.standings().empty());
    EXPECT_EQ(w.weekStart(), w.clock());
}

// The server sends each pilot the table in a Standings message of its own,
// again when it changed.
TEST(standings_sent_to_the_pilot) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = first();
    o.worldParams = params();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    double now = 100;
    HeadlessClient client(clientPort, nettest::clientConfig());
    auto run = [&](double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            server.tick(now);
            client.tick(now);
        }
    };
    client.connect(now);
    client.join(*net::pilotKeyFromHex(keyA));
    run(3);
    ASSERT_TRUE(client.standings().has_value());
    ASSERT_TRUE(client.standings()->you.has_value());
    EXPECT_EQ(client.standings()->rows[0].ore, int64_t(0));
    server.world().standNote(keyA, &WorldMap::Standing::ore, 350);
    run(2);
    EXPECT_EQ(client.standings()->rows[size_t(*client.standings()->you)].ore, int64_t(350));
}
