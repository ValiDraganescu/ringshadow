// KSTR contracts in a world (docs/contracts.md, K1; WorldContracts.cpp):
// three drawn per pilot and world day by seeded dice, always reachable at
// the pilot's level, each kind's progress and its pay, the dawn refresh,
// abandoning, and the contracts kept by the save. Isolated scenes of a few
// seconds on The First Reach; run from unreal/server.

#include "test.h"

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
using Kind = net::Contract::Kind;
using State = net::Contract::State;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId me = 51;
const std::string key = "aabbccddeeff00112233445566778899";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

WorldParams params(uint64_t seed = 77) {
    WorldParams p;
    p.devBlueprints = true;
    p.contractSeed = seed;
    // The daily three from the start (FirstTenTests.cpp has the first ten
    // minutes).
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

void wait(WorldMap& w, PilotId p, int ticks, bool hold = false) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.hold = hold;
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

/// One contract of `kind` for `target`, open, as the radio would word it.
Contract job(uint32_t id, Kind kind, int64_t need, const std::string& target = "", const std::string& region = "") {
    Contract c;
    c.id = id;
    c.kind = kind;
    c.need = need;
    c.target = target;
    c.region = region;
    c.title = "Test";
    c.pitch = "A test.";
    c.xp = WorldContracts::xp(kind, 1);
    c.hydrogen = WorldContracts::hydrogen(1);
    return c;
}

/// A Citadel of the pilot's put up round it, off the pad's plate (nil: no
/// spot); welded to the end when `finish`.
std::optional<int64_t> citadel(WorldMap& w, PilotId p, bool finish) {
    Simulation& sim = w.start().simulation();
    const Unit u = *drivenUnit(w, p);
    const double reach = Region::buildReach(StructureKind::citadel);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
        for (int k = 0; k < 16; k++) {
            const double h = k * 2 * pi / 16;
            const auto at = sim.snap(StructureKind::citadel, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (!at || distance(*at, u.position) > reach + Region::buildSlack || sim.placementRefusal(StructureKind::citadel, *at) ||
                region::touchesPadPlate(first().regions[first().start].built.pads, *at, Rules::radius(StructureKind::citadel)))
                continue;
            const int64_t id = sim.state.nextID;
            if (w.build(p, StructureKind::citadel, *at)) continue;
            if (finish)
                for (int i = 0; i < int((Rules::buildTime(StructureKind::citadel) + 5) * 60); i++) {
                    wait(w, p, 1);
                    for (const Structure& s : sim.state.structures)
                        if (s.id == id && s.complete()) return id;
                }
            return id;
        }
    return std::nullopt;
}

const net::ProgressMessage::News* find(const std::vector<net::ProgressMessage::News>& news, NewsKind kind) {
    for (const auto& x : news)
        if (x.kind == kind) return &x;
    return nullptr;
}

} // namespace

// The numbers: each reward is 8 to 12 minutes of play at the pace model's
// rate, rounded to 5; the asks grow 2% a level.
TEST(contracts_rewards_are_8_to_12_minutes_of_play) {
    for (const int64_t level : {1, 10, 50, 100})
        for (const Kind k : {Kind::deliverOre, Kind::deliverHydrogen, Kind::drill, Kind::reach, Kind::build, Kind::sweep}) {
            const double minutes = WorldContracts::xp(k, level) / WorldContracts::perMinute(level);
            EXPECT_TRUE(minutes >= 7.9 && minutes <= 12.1);
        }
    EXPECT_NEAR(WorldContracts::perMinute(1), 75, 1e-9);
    EXPECT_NEAR(WorldContracts::xp(Kind::drill, 1), 600, 1e-9);
    EXPECT_NEAR(WorldContracts::xp(Kind::reach, 50), 1780, 1e-9);
    EXPECT_EQ(WorldContracts::hydrogen(1), int64_t(10));
    EXPECT_EQ(WorldContracts::hydrogen(50), int64_t(110));
    EXPECT_EQ(WorldContracts::ore(1), int64_t(300));
    EXPECT_EQ(WorldContracts::ore(50), int64_t(600));
    EXPECT_EQ(WorldContracts::hydrogenNeed(1), int64_t(60));
    EXPECT_EQ(WorldContracts::loads(1), int64_t(6));
    EXPECT_EQ(WorldContracts::loads(100), int64_t(11));
}

// Three a pilot, drawn on its first step by the dice of the world, the
// pilot and the day: the same seed draws the same; each worded, open,
// numbered 1 to 3, and a kind each while kinds remain; the message carries
// them and the next dawn.
TEST(contracts_three_drawn_by_seeded_dice) {
    WorldMap a(first(), params()), b(first(), params());
    ASSERT_TRUE(a.seat(me, key) == WorldMap::Seated::fresh);
    ASSERT_TRUE(b.seat(me, key) == WorldMap::Seated::fresh);
    EXPECT_TRUE(a.contracts(me).empty());
    wait(a, me, 1);
    wait(b, me, 1);
    const auto list = a.contracts(me);
    ASSERT_TRUE(list.size() == WorldContracts::slots);
    EXPECT_TRUE(list == b.contracts(me));
    for (size_t i = 0; i < list.size(); i++) {
        EXPECT_EQ(list[i].id, uint32_t(i + 1));
        EXPECT_TRUE(list[i].state == State::open);
        EXPECT_TRUE(!list[i].title.empty() && !list[i].pitch.empty());
        EXPECT_TRUE(list[i].title.find('{') == std::string::npos && list[i].pitch.find('{') == std::string::npos);
        EXPECT_TRUE(list[i].xp > 0 && list[i].hydrogen > 0 && list[i].need >= 1);
        std::printf("  %s: %s\n", list[i].title.c_str(), list[i].pitch.c_str());
    }
    EXPECT_TRUE(list[0].kind != list[1].kind && list[1].kind != list[2].kind && list[0].kind != list[2].kind);
    const auto m = *a.progressMessage(me);
    EXPECT_TRUE(m.contracts == list);
    EXPECT_NEAR(m.contractsDawn, a.nextDawn(a.clock()), 1e-9);
    EXPECT_TRUE(m.contractsDawn > a.clock());
    // Another seed, another draw.
    WorldMap c(first(), params(78));
    c.seat(me, key);
    wait(c, me, 1);
    EXPECT_TRUE(c.contracts(me) != list);
}

// Always reachable: over 40 days a new pilot (no Citadel) is never asked to
// deliver; a build is a blueprint it has at its floor; a place is a site it
// hasn't opened that it can walk to, or a pad in another region; a drill is
// a region with ore. No sweep until the server pays the sweep.
TEST(contracts_always_reachable_at_the_pilots_level) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    ProspectorProgress g;
    g.blueprints = {{StructureKind::citadel, 1}, {StructureKind::derrick, 1}};
    w.setProgress(me, g);
    for (int day = 0; day < 40; day++) {
        w.setClock(w.nextDawn(w.clock()) + 1);
        wait(w, me, 1);
        for (Contract c : w.contracts(me)) {
            EXPECT_TRUE(c.kind != Kind::deliverOre && c.kind != Kind::deliverHydrogen && c.kind != Kind::sweep);
            if (c.kind == Kind::build) EXPECT_TRUE(c.target == "citadel");
            if (c.kind == Kind::drill) EXPECT_TRUE(w.regionNamed(c.target).has_value());
            if (c.kind == Kind::reach) {
                const auto r = w.regionNamed(c.region);
                ASSERT_TRUE(r.has_value());
                EXPECT_TRUE(c.target.rfind(c.region + "/", 0) == 0);
                if (c.target.find("/pad/") != std::string::npos) EXPECT_TRUE(*r != *w.where(me));
            }
            // Abandoned: the next dawn draws a new one in its place.
            w.abandon(me, c.id);
        }
    }
    WorldParams sweep = params();
    sweep.sweepContracts = true;
    WorldMap s(first(), sweep);
    s.join(me);
    bool drawn = false;
    for (int day = 0; day < 30 && !drawn; day++) {
        s.setClock(s.nextDawn(s.clock()) + 1);
        wait(s, me, 1);
        for (const Contract& c : s.contracts(me)) {
            drawn = drawn || c.kind == Kind::sweep;
            s.abandon(me, c.id);
        }
    }
    EXPECT_TRUE(drawn);
}

// Deliveries: ore and MH unloaded at the pilot's own Citadel count; the
// contract that fills is paid its XP and MH once, with its news.
TEST(contracts_deliver_ore_and_mh) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    w.setContracts(me, {job(10, Kind::deliverOre, 300), job(11, Kind::deliverHydrogen, 30)});
    const auto home = citadel(w, me, true);
    ASSERT_TRUE(home.has_value());
    // At its edge, where it unloads.
    Unit* u = drivenUnit(w, me);
    for (const Structure& c : w.start().simulation().state.structures)
        if (c.id == *home)
            u->position = c.position + normalize(u->position - c.position) * (Rules::radius(StructureKind::citadel) + 0.4);
    w.takeNews(me);
    const double xp = w.progress(me)->xp;
    const int64_t mh = w.wallet(me)->hydrogen;
    u->carrying = 200;
    u->hydrogen = false;
    wait(w, me, 10);
    EXPECT_EQ(w.contracts(me)[0].have, int64_t(200));
    EXPECT_TRUE(w.contracts(me)[0].state == State::open);
    drivenUnit(w, me)->carrying = 200;
    wait(w, me, 10);
    auto list = w.contracts(me);
    EXPECT_EQ(list[0].have, int64_t(300));
    EXPECT_TRUE(list[0].state == State::done);
    EXPECT_NEAR(w.progress(me)->xp - xp, 400 * WorldXp::ore + list[0].xp, 1e-6);
    EXPECT_EQ(w.wallet(me)->hydrogen - mh, list[0].hydrogen);
    const auto news = w.takeNews(me);
    const auto* n = find(news, NewsKind::contract);
    ASSERT_TRUE(n != nullptr);
    EXPECT_TRUE(n->id == "10");
    EXPECT_EQ(n->value, list[0].hydrogen);
    // U1: its title rides with it (the toast reads it, never "CONTRACT").
    EXPECT_TRUE(n->by == list[0].title && !n->by.empty());
    // MH: 20 a trip.
    for (int trip = 0; trip < 2; trip++) {
        drivenUnit(w, me)->carrying = 20;
        drivenUnit(w, me)->hydrogen = true;
        wait(w, me, 10);
    }
    list = w.contracts(me);
    EXPECT_EQ(list[1].have, int64_t(30));
    EXPECT_TRUE(list[1].state == State::done);
    // More ore: nothing more for a done contract.
    const double after = w.progress(me)->xp;
    drivenUnit(w, me)->carrying = 100;
    drivenUnit(w, me)->hydrogen = false;
    wait(w, me, 10);
    EXPECT_NEAR(w.progress(me)->xp - after, 100 * WorldXp::ore, 1e-6);
}

// Drilling: an ore load drilled in the named region counts; in another
// region it doesn't.
TEST(contracts_drill_in_a_named_region) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    w.setContracts(me, {job(1, Kind::drill, 1, "start", "start"), job(2, Kind::drill, 1, "cold-fork", "cold-fork")});
    Simulation& sim = w.start().simulation();
    const OreDeposit& d = sim.state.patches[0];
    Unit* u = drivenUnit(w, me);
    u->position = d.position + Vec2(1, 0) * (Rules::radius(UnitKind::prospector) + 1.0);
    u->heading = std::atan2(0.0, -1.0);
    u->carrying = 0;
    u->hydrogen = std::nullopt;
    for (int i = 0; i < 30 * 60 && drivenUnit(w, me)->carrying == 0; i++) wait(w, me, 1, true);
    ASSERT_TRUE(drivenUnit(w, me)->carrying > 0);
    const auto list = w.contracts(me);
    EXPECT_TRUE(list[0].state == State::done);
    EXPECT_EQ(list[1].have, int64_t(0));
}

// Reaching: a site of the region (standing at it) and a pad in another
// region (landing there).
TEST(contracts_reach_a_site_and_a_pad) {
    WorldParams fuel = params();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    w.setContracts(me, {job(1, Kind::reach, 1, "start/Lander wreck", "start"),
                        job(2, Kind::reach, 1, "cold-fork/pad/landing", "cold-fork")});
    Vec2 wreck;
    for (const auto& s : first().regions[first().start].built.sites)
        if (s.name == "Lander wreck") wreck = s.at;
    drivenUnit(w, me)->position = wreck + Vec2(WorldSites::reach + 1, 0);
    wait(w, me, 2);
    EXPECT_TRUE(w.contracts(me)[0].state == State::open);
    drivenUnit(w, me)->position = wreck + Vec2(1, 0);
    wait(w, me, 1);
    EXPECT_TRUE(w.contracts(me)[0].state == State::done);
    // Over to Cold Fork, onto its pad (F2: the region file's pad is reached,
    // not flown to).
    const size_t fork = *w.regionNamed("cold-fork");
    ASSERT_TRUE(w.devTransfer(me, fork, first().regions[fork].built.pads[0].at));
    wait(w, me, 2);
    EXPECT_TRUE(w.contracts(me)[1].state == State::done);
}

// Building: a building of the named kind of the pilot's own, finished.
TEST(contracts_build_finished) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    w.setContracts(me, {job(1, Kind::build, 1, "citadel"), job(2, Kind::build, 1, "derrick")});
    const auto id = citadel(w, me, false);
    ASSERT_TRUE(id.has_value());
    EXPECT_TRUE(w.contracts(me)[0].state == State::open);
    for (int i = 0; i < int((Rules::buildTime(StructureKind::citadel) + 5) * 60) && w.contracts(me)[0].state == State::open; i++)
        wait(w, me, 1);
    EXPECT_TRUE(w.contracts(me)[0].state == State::done);
    EXPECT_TRUE(w.contracts(me)[1].state == State::open);
}

// Abandoning: free, refused for one done, dropped or unknown; at the next
// dawn the done and dropped slots get new contracts and the open one stays.
TEST(contracts_abandon_and_dawn_refresh) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    Contract done = job(4, Kind::drill, 3, "start", "start");
    done.state = State::done;
    w.setContracts(me, {job(5, Kind::deliverOre, 300), done, job(6, Kind::reach, 1, "cold-fork/pad/landing", "cold-fork")});
    const double xp = w.progress(me)->xp;
    EXPECT_TRUE(!w.abandon(me, 5));
    EXPECT_TRUE(w.abandon(me, 5) == std::optional<std::string>("That contract is dropped already"));
    EXPECT_TRUE(w.abandon(me, 4) == std::optional<std::string>("That contract is done already"));
    EXPECT_TRUE(w.abandon(me, 99) == std::optional<std::string>("No such contract"));
    EXPECT_NEAR(w.progress(me)->xp, xp, 1e-9);
    // Not yet dawn: nothing new.
    wait(w, me, 5);
    EXPECT_TRUE(w.contracts(me)[0].state == State::abandoned);
    w.setClock(w.nextDawn(w.clock()) + 0.5);
    wait(w, me, 1);
    const auto list = w.contracts(me);
    ASSERT_TRUE(list.size() == 3);
    EXPECT_TRUE(list[0].id == 7 && list[0].state == State::open);
    EXPECT_TRUE(list[1].id == 8 && list[1].state == State::open);
    EXPECT_TRUE(list[2].id == 6 && list[2].state == State::open);
}

// Kept by the save: the contracts, their progress, the day and the next
// number come back as they were, and are not drawn again the same day.
TEST(contracts_kept_through_the_save) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    wait(w, me, 1);
    w.abandon(me, w.contracts(me)[1].id);
    const auto list = w.contracts(me);
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/contracts.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    ASSERT_TRUE(back->pilots.size() == 1);
    EXPECT_TRUE(!back->pilots[0].contracts.empty());
    WorldMap again(first(), params());
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(again.contracts(me) == list);
    wait(again, me, 1);
    EXPECT_TRUE(again.contracts(me) == list);
}

// A save from before contracts (schema 4) opens at 5; its pilot has none
// until its first step draws them.
TEST(contracts_v4_save_migrates_and_draws) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/v4contracts.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    const std::vector<WorldStore::Migration> v4(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 4);
    {
        auto store = WorldStore::open(path, "first", &why, v4);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(4));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated) VALUES ('" + key +
                                "', 10, 3, 'start', NULL, '', '', 1)", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->pilots.size() == 1);
    EXPECT_TRUE(loaded->pilots[0].contracts.empty());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.contracts(me).empty());
    wait(w, me, 1);
    EXPECT_EQ(w.contracts(me).size(), WorldContracts::slots);
}
