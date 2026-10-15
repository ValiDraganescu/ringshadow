// The hunt (N4, docs/mmorpg-play.md "The hunt"; WorldHunt.cpp): a human
// calls THE HUNT at the realm's Citadel, free, once a world day realm-wide,
// never while she is down or a hunt is on; the frontier defenders and the
// guard mercenaries walk to their places on the ring round her nest by the
// realm's rules (no jumps: a Prospector's pace, through the openings) and
// hold; at two minutes, the caller inside the ring, they go in; her fall
// (N3's, her pay as it is) wins it, ten minutes or the party down fail it.
// Saved with the world (schema 25). Isolated scenes in Tern Ridge and Slag
// Pockets of a few minutes; never a game.

#include "test.h"

#include "Brood.h"
#include "Progression.h"
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
using NewsKind = net::ProgressMessage::NewsKind;
using Habit = WorldFrontier::Habit;
using FrontierPilot = WorldMap::FrontierPilot;
using State = WorldMap::Hunt::State;

constexpr PilotId a = 81;
const std::string keyA = "8a8b8c8d8e8f808182838485868788ab";
constexpr PilotId b = 82;
const std::string keyB = "9a9b9c9d9e9f909192939495969798cd";

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
    p.canyons = false;
    p.firstStep = 0;
    p.frontier = 0;
    p.broodSeed = 0x4a17ULL;
    p.frontierSeed = 0x4a18ULL;
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

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
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

/// The driven Prospector at the building's edge, where it unloads.
void besides(WorldMap& w, PilotId p, int64_t building) {
    const Structure s = *structure(simOf(w, p), building);
    Unit* u = drivenUnit(w, p);
    const Vec2 out = normalize(u->position - s.position);
    u->position = s.position + (out == Vec2(0, 0) ? Vec2(1, 0) : out) * (Rules::radius(s.kind) + 0.4);
    u->task = Unit::Task::idle;
    u->goal = std::nullopt;
    u->waypoints = std::nullopt;
    w.step();
}

/// Pilot `p` (key `key`) at level 20 with a full wallet, in Tern Ridge
/// beside a realm Citadel there (returned).
int64_t atTernCitadel(WorldMap& w, PilotId p, const std::string& key) {
    if (w.seat(p, key) != WorldMap::Seated::fresh) return -1;
    ProspectorProgress g;
    g.xp = Progression::xp(20);
    w.setProgress(p, g);
    w.step();
    const size_t r = *w.regionNamed("tern-ridge");
    const int64_t citadel = place(w, r, StructureKind::citadel, Vec2(60, 50));
    if (!w.devTransfer(p, r, structure(w.region(r).simulation(), citadel)->position + Vec2(8, 0))) return -1;
    w.step();
    simOf(w, p).state.players[0].ore = 40000;
    simOf(w, p).state.players[0].hydrogen = 4000;
    besides(w, p, citadel);
    return citadel;
}

/// The roster made: its defender (made one if none) come into region `r`
/// at `cell`, the rest kept away.
FrontierPilot& defender(WorldMap& w, size_t r, Vec2 cell, size_t which = 0) {
    w.frontierRoster();
    FrontierPilot* out = nullptr;
    size_t seen = 0;
    for (FrontierPilot& f : w.frontier())
        if (!out && f.habit == Habit::defender && seen++ == which) out = &f;
    if (!out)
        for (FrontierPilot& f : w.frontier())
            if (!out && f.habit != Habit::defender && !f.arrived) {
                f.habit = Habit::defender;
                out = &f;
            }
    out->arrived = true;
    out->comes = 0;
    out->region = r;
    out->at = w.region(r).places()->ground(cell);
    out->wallet = WorldMap::Wallet{0, 0};
    out->progress.xp = Progression::xp(12);
    out->progress.regions.insert(w.loaded()->regions[r].file.id);
    for (FrontierPilot& f : w.frontier())
        if (!f.arrived) f.comes = 1e12;
    return *out;
}

/// Where a frontier pilot is (its region and point), nil on its way.
std::optional<std::pair<size_t, Vec2>> whereIs(const WorldMap& w, uint32_t id) {
    for (const FrontierPilot& f : w.frontier())
        if (f.id == id && !f.travelTo) {
            if (f.unit)
                if (const auto u = w.region(f.region).simulation().state.unit(*f.unit)) return std::pair{f.region, u->position};
            return std::pair{f.region, f.at};
        }
    return std::nullopt;
}

int64_t newsOf(const std::vector<net::ProgressMessage::News>& news, const std::string& prefix) {
    int64_t n = 0;
    for (const auto& x : news) n += x.kind == NewsKind::hunt && x.id.rfind(prefix, 0) == 0 ? 1 : 0;
    return n;
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

// The call's rules: a human at a Citadel, free; refused while a hunt is on,
// for the rest of the world day once called, while she is down; never a
// frontier pilot.
TEST(hunt_call_rules) {
    WorldMap w(first(), params());
    const int64_t citadel = atTernCitadel(w, a, keyA);
    ASSERT_TRUE(citadel > 0);
    EXPECT_TRUE(!w.huntRefusal(keyA).has_value());
    EXPECT_TRUE(*w.huntRefusal("frontier:vesper") == "Only a pilot of the realm calls the hunt");
    // Away from the Citadel: refused, nothing spent.
    Unit* u = drivenUnit(w, a);
    u->position = u->position + Vec2(30, 0);
    w.step();
    const auto away = w.callHunt(a);
    ASSERT_TRUE(away.has_value());
    EXPECT_TRUE(*away == "Call the hunt at a Citadel: stand where you unload");
    EXPECT_TRUE(w.hunt().state == State::none);
    // At the Citadel: called, free.
    besides(w, a, citadel);
    const WorldMap::Wallet before = *w.wallet(a);
    w.takeNews(a);
    ASSERT_TRUE(!w.callHunt(a).has_value());
    EXPECT_TRUE(w.hunt().state == State::gathering);
    EXPECT_TRUE(w.hunt().caller == WorldMap::callsign(keyA));
    EXPECT_EQ(w.wallet(a)->ore, before.ore);
    EXPECT_EQ(w.wallet(a)->hydrogen, before.hydrogen);
    EXPECT_EQ(newsOf(w.takeNews(a), "called/Slag Pockets"), int64_t(1));
    // On: refused.
    EXPECT_TRUE(*w.callHunt(a) == "A hunt is on");
    // Over (failed): the day's call is spent until dawn.
    std::printf("  %s\n", w.devHunt("hunt fail").c_str());
    EXPECT_TRUE(w.hunt().state == State::failed);
    const auto spent = w.callHunt(a);
    ASSERT_TRUE(spent.has_value());
    std::printf("  %s\n", spent->c_str());
    EXPECT_TRUE(spent->rfind("The day's hunt was called: the next at dawn, in ", 0) == 0);
    const auto told = w.huntMessage(a);
    ASSERT_TRUE(told.has_value());
    EXPECT_TRUE(told->nextCall > w.clock() && told->refusal == *spent);
    // The next world day: open again.
    w.idle(told->nextCall - w.clock() + 1);
    EXPECT_TRUE(!w.huntRefusal(keyA).has_value());
    // She is down (N3): refused until she is back in three world days.
    std::printf("  %s\n", w.devMatriarch("matriarch here 40").c_str());
    stepFor(w, 0.5);
    std::printf("  %s\n", w.devMatriarch("matriarch down").c_str());
    stepFor(w, 0.5);
    ASSERT_TRUE(w.matriarchBack().has_value());
    const auto down = w.huntRefusal(keyA);
    ASSERT_TRUE(down.has_value());
    std::printf("  %s\n", down->c_str());
    EXPECT_TRUE(down->rfind("She is down: back in ", 0) == 0);
    EXPECT_NEAR(*w.matriarchBack() - w.clock(), Matriarch::returnDays * w.params().dayLength, 2);
}

// The gathering by the rules: a defender and a guard mercenary in Tern Ridge
// walk to their places on the ring at a Prospector's pace (no jump, into
// Slag Pockets through the opening), and hold there; a defender four
// regions off (no pads) walks in after them, a defender down at the call
// comes late; a miner mercenary stays at its field.
TEST(hunt_gathering_travels_by_the_rules) {
    WorldParams p = params();
    p.frontier = 5;
    // The ways alone: no Brood on them (the ring's fights are the assault's test).
    p.brood = false;
    WorldMap w(first(), p);
    const int64_t citadel = atTernCitadel(w, a, keyA);
    ASSERT_TRUE(citadel > 0);
    const size_t tern = *w.regionNamed("tern-ridge"), slag = *w.nestRegion(), far = *w.regionNamed("ashen-shelf");
    // A guard and a miner hired at the Citadel.
    ASSERT_TRUE(!w.hireMerc(a, true).has_value());
    ASSERT_TRUE(!w.hireMerc(a, false).has_value());
    ASSERT_TRUE(w.mercs(a).size() == 2 && w.mercs(a)[0].guard && !w.mercs(a)[1].guard);
    const uint32_t near = defender(w, tern, Vec2(70, 60)).id;
    const uint32_t farOff = defender(w, far, Vec2(60, 60), 1).id;
    FrontierPilot& down = defender(w, tern, Vec2(64, 64), 2);
    const uint32_t late = down.id;
    stepFor(w, 2);
    // Its Prospector down until 150 s after the call.
    for (FrontierPilot& f : w.frontier())
        if (f.id == late) f.downUntil = w.clock() + 150;
    ASSERT_TRUE(!w.callHunt(a).has_value());
    ASSERT_TRUE(w.hunt().party.size() == size_t(4));
    (void)down;
    // Each second: the defender and the guard move at most a Prospector's
    // pace (with its skills' 30% at the most), regions only next door.
    auto guardAt = [&]() -> std::pair<size_t, Vec2> {
        const auto m = w.mercs(a)[0];
        const auto u = w.region(m.in).simulation().state.unit(*m.unit);
        return {m.in, u ? u->position : Vec2(0, 0)};
    };
    auto lastD = whereIs(w, near);
    auto lastG = guardAt();
    const double pace = Rules::prospectorSpeed * 1.3 + 0.5;
    double reachedD = -1, reachedG = -1, reachedFar = -1, reachedLate = -1;
    bool lateThere = false;
    const double called = w.clock();
    for (int s = 0; s < 240; s++) {
        stepFor(w, 1);
        const auto d = whereIs(w, near);
        if (d && lastD) {
            if (d->first == lastD->first) EXPECT_TRUE(distance(d->second, lastD->second) <= pace);
            else EXPECT_TRUE(d->first == slag && lastD->first == tern);
        }
        if (d) lastD = d;
        const auto g = guardAt();
        if (g.first == lastG.first) EXPECT_TRUE(distance(g.second, lastG.second) <= pace);
        else EXPECT_TRUE(g.first == slag && lastG.first == tern);
        lastG = g;
        for (const auto& m : w.hunt().party) {
            if (m.key.rfind("frontier:", 0) == 0 && m.there && reachedD < 0 && whereIs(w, near) && whereIs(w, near)->first == slag)
                reachedD = w.clock() - called;
            if (m.key.rfind("merc:", 0) == 0 && m.there && reachedG < 0) reachedG = w.clock() - called;
        }
        for (const auto& m : w.hunt().party)
            for (const FrontierPilot& f : w.frontier()) {
                if (f.key != m.key || !m.there) continue;
                if (f.id == farOff && reachedFar < 0) reachedFar = w.clock() - called;
                if (f.id == late && reachedLate < 0) reachedLate = w.clock() - called;
            }
        // When the two minutes run out.
        if (s == 118)
            for (const auto& m : w.hunt().party)
                for (const FrontierPilot& f : w.frontier())
                    if (f.id == late && f.key == m.key) lateThere = m.there;
        if (reachedD >= 0 && reachedG >= 0 && reachedFar >= 0 && reachedLate >= 0) break;
    }
    std::printf("  at the ring after: defender %.0f s, guard %.0f s, from Ashen Shelf %.0f s, down at the call %.0f s\n", reachedD, reachedG,
                reachedFar, reachedLate);
    EXPECT_TRUE(reachedFar > reachedD);
    EXPECT_TRUE(reachedD > 10 && reachedG > 10);
    EXPECT_TRUE(reachedD >= 0 && reachedG >= 0);
    // At their places, holding.
    ASSERT_TRUE(lastD.has_value() && lastD->first == slag);
    EXPECT_TRUE(distance(lastD->second, *w.nestAt(slag)) <= WorldHunt::ring + WorldHunt::there);
    EXPECT_TRUE(guardAt().first == slag && distance(guardAt().second, *w.nestAt(slag)) <= WorldHunt::ring + WorldHunt::there);
    // The late one: not at the ring when the two minutes ran out, there after.
    EXPECT_TRUE(!lateThere);
    EXPECT_TRUE(reachedLate > 150);
    // The miner stayed at its work in Tern Ridge.
    EXPECT_TRUE(!w.mercs(a)[1].hunting && w.mercs(a)[1].in == tern);
    // Still gathering: the caller is not inside the ring.
    EXPECT_TRUE(w.hunt().state == State::gathering);
}

// The assault: the gathering's time over and the caller inside the ring,
// they go in; her fall (cut short by the dev line) wins it, N3's pay comes
// to the caller, she counts as a bug felled to her hardest hitter, a pilot
// off line has the hunt in its away report, the guard walks home.
TEST(hunt_assault_her_fall_and_the_pay) {
    WorldParams p = params();
    p.frontier = 5;
    WorldMap w(first(), p);
    const int64_t citadel = atTernCitadel(w, a, keyA);
    ASSERT_TRUE(citadel > 0);
    // A pilot kept, off line.
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    w.leave(b);
    const size_t tern = *w.regionNamed("tern-ridge"), slag = *w.nestRegion();
    ASSERT_TRUE(!w.hireMerc(a, true).has_value());
    const uint32_t d = defender(w, slag, Vec2(20, 20)).id;
    (void)d;
    stepFor(w, 1);
    ASSERT_TRUE(!w.callHunt(a).has_value());
    // Two minutes on, the caller still in Tern Ridge: they wait for it.
    stepFor(w, WorldHunt::gather + 2);
    EXPECT_TRUE(w.hunt().state == State::gathering);
    auto news = w.takeNews(a);
    EXPECT_EQ(newsOf(news, "gathered/"), int64_t(1));
    // The caller inside the ring: they go in.
    std::printf("  %s\n", w.devMatriarch("matriarch here 25").c_str());
    stepFor(w, 1);
    EXPECT_TRUE(w.hunt().state == State::assault);
    EXPECT_EQ(newsOf(w.takeNews(a), "charge/"), int64_t(1));
    stepFor(w, 3);
    for (const FrontierPilot& f : w.frontier())
        if (f.arrived && f.habit == Habit::defender && f.region == slag) {
            std::printf("  %s: %s\n", f.callsign.c_str(), f.plan.why.c_str());
            EXPECT_TRUE(f.plan.act == FrontierPlan::Act::fight);
        }
    // Her fall: N3's share to the caller, the hunt won.
    const WorldMap::Wallet before = *w.wallet(a);
    std::printf("  %s\n", w.devMatriarch("matriarch hurt 0.6").c_str());
    std::printf("  %s\n", w.devMatriarch("matriarch down").c_str());
    stepFor(w, 0.5);
    EXPECT_TRUE(w.hunt().state == State::won);
    news = w.takeNews(a);
    EXPECT_EQ(newsOf(news, "won/Slag Pockets"), int64_t(1));
    bool share = false;
    for (const auto& n : news) share = share || (n.kind == NewsKind::matriarch && n.id.rfind("share/", 0) == 0);
    EXPECT_TRUE(share);
    EXPECT_TRUE(w.wallet(a)->ore > before.ore);
    EXPECT_EQ(w.standings().at(keyA).bugs, int64_t(1));
    const auto* ledger = w.ledgerOf(keyB);
    ASSERT_TRUE(ledger != nullptr);
    bool row = false;
    for (const net::AwayRow& r : *ledger) row = row || (r.kind == net::AwayKind::hunt && r.what == "won");
    EXPECT_TRUE(row);
    // The day's call spent; the guard back in Tern Ridge in time.
    EXPECT_TRUE(w.huntRefusal(keyA).has_value());
    for (int s = 0; s < 300 && w.mercs(a)[0].in != tern; s++) stepFor(w, 1);
    EXPECT_TRUE(w.mercs(a)[0].in == tern && !w.mercs(a)[0].hunting);
}

// The hunt fails: ten minutes and she stands; or the caller down in the
// assault with no party in the fight. KSTR says so, the day's call is spent.
TEST(hunt_fails_on_the_clock_or_with_the_party_down) {
    {
        WorldMap w(first(), params());
        ASSERT_TRUE(atTernCitadel(w, a, keyA) > 0);
        ASSERT_TRUE(!w.callHunt(a).has_value());
        w.takeNews(a);
        stepFor(w, WorldHunt::lasts - 5);
        EXPECT_TRUE(w.hunt().state == State::gathering);
        stepFor(w, 6);
        EXPECT_TRUE(w.hunt().state == State::failed);
        const auto news = w.takeNews(a);
        EXPECT_EQ(newsOf(news, "failed/"), int64_t(1));
        for (const auto& n : news)
            if (n.kind == NewsKind::hunt) EXPECT_EQ(n.value, int64_t(0));
        EXPECT_TRUE(w.matriarch().has_value());
        EXPECT_TRUE(w.huntRefusal(keyA)->rfind("The day's hunt was called", 0) == 0);
    }
    {
        WorldMap w(first(), params());
        ASSERT_TRUE(atTernCitadel(w, a, keyA) > 0);
        ASSERT_TRUE(!w.callHunt(a).has_value());
        std::printf("  %s\n", w.devHunt("hunt gathered").c_str());
        std::printf("  %s\n", w.devMatriarch("matriarch here 20").c_str());
        stepFor(w, 1);
        ASSERT_TRUE(w.hunt().state == State::assault);
        w.takeNews(a);
        // The caller falls.
        drivenUnit(w, a)->hp = 0;
        stepFor(w, 1);
        EXPECT_TRUE(w.hunt().state == State::failed);
        const auto news = w.takeNews(a);
        EXPECT_EQ(newsOf(news, "failed/"), int64_t(1));
        for (const auto& n : news)
            if (n.kind == NewsKind::hunt) EXPECT_EQ(n.value, int64_t(1));
    }
}

// Saved with the world (schema 25): the hunt mid-gathering, its party and
// their places, the day's call, a guard's role and its walk.
TEST(hunt_saved_and_loaded) {
    const std::string path = fresh("hunt");
    WorldParams p = params();
    p.frontier = 5;
    WorldMap w(first(), p);
    ASSERT_TRUE(atTernCitadel(w, a, keyA) > 0);
    ASSERT_TRUE(!w.hireMerc(a, true).has_value());
    defender(w, *w.regionNamed("tern-ridge"), Vec2(70, 60));
    ASSERT_TRUE(!w.callHunt(a).has_value());
    stepFor(w, 20);
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(!loaded->hunt.empty());
    WorldMap v(first(), p);
    const auto problems = v.restore(*loaded);
    for (const std::string& x : problems) std::printf("  %s\n", x.c_str());
    EXPECT_TRUE(problems.empty());
    EXPECT_TRUE(v.hunt() == w.hunt());
    EXPECT_TRUE(v.hunt().state == State::gathering && v.hunt().party.size() == size_t(2));
    const auto mercs = v.mercsOf(keyA);
    ASSERT_TRUE(mercs.size() == size_t(1));
    EXPECT_TRUE(mercs[0].guard && mercs[0].hunting);
    // Written again unchanged: nothing to write.
    EXPECT_TRUE(store->write(v.save(), &why)->changed == int64_t(0));
    // An old save (no hunt): the day's call open.
    WorldSave old = *loaded;
    old.hunt.clear();
    WorldMap u(first(), p);
    EXPECT_TRUE(u.restore(old).empty());
    EXPECT_TRUE(u.hunt().state == State::none && !u.huntRefusal(keyA).has_value());
}
