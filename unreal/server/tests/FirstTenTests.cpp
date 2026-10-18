// The first ten minutes (docs/contracts.md, "First ten minutes"; O1;
// WorldContracts.cpp `firstTen`): a new pilot's chain of six KSTR jobs, one
// at a time, each opened by the last, counted from the work itself and paid
// once; the daily three after it. Each step's trigger, the order, abandoning
// refused, an old save's pilot skipping it, the save round trip and the dev
// start step. Isolated scenes of a few minutes on The First Reach; run from
// unreal/server.

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
using Kind = net::Contract::Kind;
using State = net::Contract::State;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId me = 61;
const std::string key = "f1f2f3f4f5f6f7f8f9fafbfcfdfeff00";

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
    p.contractSeed = 77;
    p.firstStep = 1;
    return p;
}

Unit* driven(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    const auto id = r ? r->driving(p) : std::nullopt;
    const auto i = id ? r->simulation().unitIndex(*id) : std::nullopt;
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

int64_t inputTick = 0;

/// Steps the world `ticks` times, the pilot facing `heading` (its own when
/// unset), walking `walk`, the act key down on the first tick when `act`,
/// held (`hold`) throughout; stops early when `done`.
template <class F>
bool run(WorldMap& w, PilotId p, int ticks, F&& done, std::optional<double> heading = std::nullopt, double walk = 0, bool act = false,
         bool hold = false) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.walk = Vec2(walk, 0);
        in.act = act && i == 0;
        in.hold = hold;
        if (const Unit* u = driven(w, p)) in.heading = heading.value_or(u->heading);
        w.input(p, in);
        w.step();
        if (done()) return true;
    }
    return false;
}

void wait(WorldMap& w, PilotId p, int ticks, bool hold = false) {
    run(w, p, ticks, [] { return false; }, std::nullopt, 0, false, hold);
}

Vec2 siteAt(const std::string& name) {
    for (const region::Site& s : first().regions[first().start].built.sites)
        if (s.name == name) return s.at;
    return Vec2(0, 0);
}

/// The open contract (the chain's only one), or nil.
std::optional<Contract> open(WorldMap& w) {
    for (const Contract& c : w.contracts(me))
        if (c.state == State::open) return c;
    return std::nullopt;
}

const Structure* own(const Simulation& sim, StructureKind kind) {
    for (const Structure& s : sim.state.structures)
        if (s.kind == kind && s.owner == Pilot::player) return &s;
    return nullptr;
}

/// A Citadel east of the ore, welded to the end (as HydrogenTests.cpp).
bool citadel(WorldMap& w) {
    Simulation& sim = w.regionOf(me)->simulation();
    Unit* u = driven(w, me);
    u->position = rig::citadelStand(w, me, w.regionOf(me)->places()->ground(Vec2(72, 48)));  // C1: where the 12x12 places
    u->heading = pi;
    u->task = Unit::Task::idle;
    const Vec2 home = *sim.snap(StructureKind::citadel, u->position + Vec2(-1, 0) * Region::buildReach(StructureKind::citadel));
    // C2: the crate's copy learned first, as the build wheel's LEARN does.
    if (!w.progress(me)->knows(StructureKind::citadel)) w.learn(me, StructureKind::citadel);
    if (const auto why = w.build(me, StructureKind::citadel, home)) {
        std::printf("  Citadel refused: %s\n", why->c_str());
        return false;
    }
    return run(w, me, int((Rules::buildTime(StructureKind::citadel) + 5) * 60), [&] {
        const Structure* c = own(sim, StructureKind::citadel);
        return c && c->complete();
    });
}

/// A load of ore unloaded at the finished Citadel's edge.
void haul(WorldMap& w, int64_t ore) {
    Simulation& sim = w.regionOf(me)->simulation();
    const Structure* c = own(sim, StructureKind::citadel);
    Unit* u = driven(w, me);
    u->position = c->position + normalize(u->position - c->position) * (Rules::radius(StructureKind::citadel) + 0.4);
    u->carrying = ore;
    u->hydrogen = false;
    wait(w, me, 10);
}

/// A Derrick on the start's well (E1: its one), welded, and pumped once (as
/// HydrogenTests.cpp). False: refused or never pumped.
bool derrickPumped(WorldMap& w) {
    Simulation& sim = w.regionOf(me)->simulation();
    Unit* u = driven(w, me);
    const Vec2 wellCell = w.regionOf(me)->places()->ground(Vec2(60, 36));
    std::optional<Vec2> wellAt;
    for (const Well& x : *sim.state.wells)
        if (distance(x.position, wellCell) < 1.0) wellAt = x.position;
    if (!wellAt) return false;
    for (int k = 0; k < 16; k++) {
        const double a = k * 2 * pi / 16;
        const Vec2 p = *wellAt - Vec2(std::cos(a), std::sin(a)) * Region::buildReach(StructureKind::derrick);
        if (sim.pilotCanStand(*u, p)) {
            u->position = p;
            u->heading = a;
            break;
        }
    }
    const auto snapped = sim.snap(StructureKind::derrick, u->position + Vec2(std::cos(u->heading), std::sin(u->heading)) * 3.0);
    if (!snapped) return false;
    if (!w.progress(me)->knows(StructureKind::derrick)) w.learn(me, StructureKind::derrick);
    if (const auto why = w.build(me, StructureKind::derrick, *snapped)) {
        std::printf("  Derrick refused: %s\n", why->c_str());
        return false;
    }
    if (!run(w, me, int((Rules::buildTime(StructureKind::derrick) + 5) * 60), [&] {
            const Structure* d = own(sim, StructureKind::derrick);
            return d && d->complete();
        }))
        return false;
    // Built: half the step.
    if (open(w) && open(w)->have != 1) return false;
    auto face = [&] {
        const Unit* v = driven(w, me);
        return std::atan2(wellAt->y - v->position.y, wellAt->x - v->position.x);
    };
    if (!run(w, me, 60 * 5, [&] {
            const auto t = sim.pilotTarget(*driven(w, me));
            return t && t->is<PilotTarget::Derrick>();
        }, face(), 0.6))
        return false;
    if (!run(w, me, 2, [&] { return driven(w, me)->task == Unit::Task::inDerrick; }, face(), 0, true)) return false;
    return run(w, me, int((Rules::hydrogenTime + 2) * 60), [&] { return driven(w, me)->carrying > 0; }, face());
}

int count(const std::vector<net::ProgressMessage::News>& news, NewsKind kind) {
    int n = 0;
    for (const auto& x : news) n += x.kind == kind ? 1 : 0;
    return n;
}

std::string path(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string p = "build/save-tests/" + name + ".sqlite";
    std::remove(p.c_str());
    std::remove((p + "-wal").c_str());
    std::remove((p + "-shm").c_str());
    return p;
}

std::optional<WorldSave> saved(WorldMap& w, const std::string& name) {
    const std::string p = path(name);
    std::string why;
    {
        auto store = WorldStore::open(p, "first", &why);
        if (!store || !store->write(w.save(), &why)) return std::nullopt;
    }
    auto store = WorldStore::open(p, "first", &why);
    return store ? store->load(&why) : std::nullopt;
}

} // namespace

// The numbers: the chain's pay with what the work pays by itself takes a
// new pilot past the Derrick's floor by step 4's end and to level 4 by the
// last.
TEST(first_ten_pay_reaches_the_derrick_floor_then_level_4) {
    double xp = 0;
    for (int s = 1; s <= WorldContracts::firstSteps; s++) {
        EXPECT_TRUE(WorldContracts::firstXp(s) > 0);
        EXPECT_TRUE(WorldContracts::firstHydrogen(s) > 0 && WorldContracts::firstHydrogen(s) <= 15);
    }
    EXPECT_EQ(WorldContracts::firstXp(0), 0.0);
    EXPECT_EQ(WorldContracts::firstXp(7), 0.0);
    // By the end of step 4: the Citadel's blueprint, welding and feat, a
    // load, the Derrick's blueprint.
    const double citadelCost = double(Rules::cost(StructureKind::citadel));
    xp += WorldXp::blueprintPerFloor * 1 + WorldXp::weld * citadelCost + WorldXp::citadel + Rules::carry * WorldXp::ore +
          WorldXp::blueprintPerFloor * double(Progression::blueprintFloor(StructureKind::derrick));
    for (int s = 1; s <= 4; s++) xp += WorldContracts::firstXp(s);
    EXPECT_TRUE(Progression::level(xp) >= Progression::blueprintFloor(StructureKind::derrick));
    xp += WorldContracts::firstXp(5) + WorldContracts::firstXp(6) + WorldXp::building;
    EXPECT_EQ(Progression::level(xp), int64_t(4));
}

// The whole chain played by its own work, in order: the crate opened, the
// Citadel built, a load home, the wreck held open, the Derrick built at its
// floor and pumped, a point spent; each step paid once with its news, the
// next one open at once; then the daily three.
TEST(first_ten_played_through) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    auto step = [&] { return open(w) ? int(open(w)->step) : 0; };
    ASSERT_TRUE(w.contracts(me).size() == size_t(1));
    EXPECT_EQ(step(), 1);
    EXPECT_TRUE(open(w)->title == "Touchdown");
    EXPECT_TRUE(open(w)->kind == Kind::reach);
    EXPECT_TRUE(open(w)->target == "start/Landing crate");
    EXPECT_TRUE(!open(w)->pitch.empty());
    w.takeNews(me);

    // 1: the crate.
    driven(w, me)->position = siteAt("Landing crate") + Vec2(1.0, 0);
    wait(w, me, 2);
    EXPECT_TRUE(w.progress(me)->blueprints.count(StructureKind::citadel) == 1);
    EXPECT_EQ(step(), 2);
    EXPECT_TRUE(open(w)->title == "A Roof over Your Head");
    // Under it, step 1 paid.
    ASSERT_TRUE(w.contracts(me).size() == size_t(2));
    EXPECT_TRUE(w.contracts(me)[1].step == 1 && w.contracts(me)[1].state == State::done);
    EXPECT_EQ(count(w.takeNews(me), NewsKind::contract), 1);
    EXPECT_NEAR(w.progress(me)->xp, WorldXp::blueprintPerFloor + WorldContracts::firstXp(1), 1e-6);
    EXPECT_EQ(w.wallet(me)->hydrogen, WorldContracts::firstHydrogen(1));

    // 2: the Citadel.
    ASSERT_TRUE(citadel(w));
    wait(w, me, 2);
    EXPECT_EQ(step(), 3);
    EXPECT_TRUE(open(w)->kind == Kind::deliverOre);
    EXPECT_EQ(open(w)->need, Rules::carry);
    EXPECT_EQ(w.progress(me)->level(), int64_t(2));

    // 3: a load home, in two halves.
    haul(w, 2);
    EXPECT_EQ(step(), 3);
    EXPECT_EQ(open(w)->have, int64_t(2));
    haul(w, 3);
    EXPECT_EQ(step(), 4);
    EXPECT_TRUE(open(w)->target == "start/Lander wreck");
    // The radio says where: on the pilot's map, its column up.
    EXPECT_TRUE(w.progress(me)->known.count("start/Lander wreck") == 1);
    EXPECT_TRUE(open(w)->pitch.find("lander wreck") != std::string::npos);

    // 4: the wreck, standing by it is not enough: held 5 s open.
    driven(w, me)->position = siteAt("Lander wreck") + Vec2(1.0, 0);
    wait(w, me, 60);
    EXPECT_EQ(step(), 4);
    wait(w, me, int((WorldSites::wreck + 0.5) * 60), true);
    EXPECT_TRUE(w.progress(me)->blueprints.count(StructureKind::derrick) == 1);
    EXPECT_EQ(step(), 5);
    EXPECT_TRUE(w.progress(me)->level() >= Progression::blueprintFloor(StructureKind::derrick));

    // 5: the Derrick, built (1 of 2), then pumped once.
    EXPECT_EQ(open(w)->need, int64_t(2));
    ASSERT_TRUE(derrickPumped(w));
    wait(w, me, 2);
    EXPECT_EQ(step(), 6);
    EXPECT_TRUE(open(w)->kind == Kind::skill);

    // 6: a point spent; the chain is over, the daily three drawn at once.
    w.takeNews(me);
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill).has_value());
    wait(w, me, 2);
    EXPECT_EQ(count(w.takeNews(me), NewsKind::contract), 1);
    const auto list = w.contracts(me);
    ASSERT_TRUE(list.size() == WorldContracts::slots);
    for (const Contract& c : list) {
        EXPECT_EQ(int(c.step), 0);
        EXPECT_TRUE(c.state == State::open);
    }
    EXPECT_EQ(w.progress(me)->level(), int64_t(4));
    std::printf("  chain done at %.0f XP, level %lld, %lld MH\n", w.progress(me)->xp, (long long)w.progress(me)->level(),
                (long long)w.wallet(me)->hydrogen);
}

// One at a time: work for a later step pays nothing toward it while an
// earlier one is open; once its turn comes, a step already done passes at
// once (and is paid then). The chain's steps can't be abandoned.
TEST(first_ten_in_order) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    // The wreck first: no step 4 yet.
    driven(w, me)->position = siteAt("Lander wreck") + Vec2(1.0, 0);
    wait(w, me, int((WorldSites::wreck + 0.5) * 60), true);
    EXPECT_TRUE(w.progress(me)->blueprints.count(StructureKind::derrick) == 1);
    EXPECT_EQ(int(open(w)->step), 1);
    EXPECT_EQ(w.contracts(me).size(), size_t(1));
    const auto id = open(w)->id;
    EXPECT_TRUE(w.abandon(me, id) == std::optional<std::string>("The radio's first jobs can't be dropped"));
    EXPECT_TRUE(open(w).has_value());
    // The crate, the Citadel, a load: then 4 passes at once, 5 waits.
    driven(w, me)->position = siteAt("Landing crate") + Vec2(1.0, 0);
    wait(w, me, 2);
    EXPECT_EQ(int(open(w)->step), 2);
    ASSERT_TRUE(citadel(w));
    wait(w, me, 2);
    haul(w, Rules::carry);
    wait(w, me, 2);
    EXPECT_EQ(int(open(w)->step), 5);
    EXPECT_EQ(open(w)->have, int64_t(0));
    // Contract numbers are never reused.
    EXPECT_TRUE(open(w)->id > id + 2);
}

// A pilot kept from a save made before the first ten minutes (its contracts
// carry no step) skips it: its daily contracts stay, the next day's are
// drawn as before. One saved before contracts at all draws the three.
TEST(first_ten_old_saves_skip_it) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    wait(w, me, 1);
    auto back = saved(w, "first-ten-old");
    ASSERT_TRUE(back.has_value());
    ASSERT_TRUE(back->pilots.size() == size_t(1));
    EXPECT_TRUE(back->pilots[0].contracts.find("\"first\":1") != std::string::npos);
    // As K1 wrote them: no "first", no "step".
    back->pilots[0].contracts =
        R"({"day":0,"list":[{"have":0,"hydrogen":10,"id":1,"kind":2,"need":6,"pitch":"p","region":"cold-fork","state":0,)"
        R"("target":"cold-fork","title":"Prove the Vein","xp":600.0}],"next":2})";
    {
        WorldMap again(first(), params());
        EXPECT_TRUE(again.restore(*back).empty());
        ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
        wait(again, me, 1);
        const auto list = again.contracts(me);
        ASSERT_TRUE(!list.empty());
        for (const Contract& c : list) EXPECT_EQ(int(c.step), 0);
        EXPECT_TRUE(list[0].title == "Prove the Vein");
    }
    back->pilots[0].contracts = "";
    WorldMap again(first(), params());
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    wait(again, me, 1);
    const auto list = again.contracts(me);
    ASSERT_TRUE(list.size() == WorldContracts::slots);
    for (const Contract& c : list) EXPECT_EQ(int(c.step), 0);
}

// Kept through the save mid-chain: the step, its progress and the numbers
// come back, and the chain goes on from there.
TEST(first_ten_kept_through_the_save) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    wait(w, me, 1);
    driven(w, me)->position = siteAt("Landing crate") + Vec2(1.0, 0);
    wait(w, me, 2);
    ASSERT_TRUE(citadel(w));
    wait(w, me, 2);
    haul(w, 2);
    const auto list = w.contracts(me);
    ASSERT_TRUE(int(open(w)->step) == 3);
    auto back = saved(w, "first-ten");
    ASSERT_TRUE(back.has_value());
    WorldMap again(first(), params());
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(again.contracts(me) == list);
    wait(again, me, 1);
    EXPECT_TRUE(again.contracts(me) == list);
    // On from there: the rest of the load.
    Simulation& sim = again.regionOf(me)->simulation();
    const Structure* c = own(sim, StructureKind::citadel);
    ASSERT_TRUE(c != nullptr);
    Unit* u = driven(again, me);
    u->position = c->position + normalize(u->position - c->position) * (Rules::radius(StructureKind::citadel) + 0.4);
    u->carrying = 3;
    u->hydrogen = false;
    wait(again, me, 10);
    std::optional<Contract> now;
    for (const Contract& k : again.contracts(me))
        if (k.state == State::open) now = k;
    ASSERT_TRUE(now.has_value());
    EXPECT_EQ(int(now->step), 4);
}

// The dev start step (`--first-step 5`, pictures): a new pilot starts there;
// 0: no chain, the daily three.
TEST(first_ten_dev_start_step) {
    WorldParams p = params();
    p.firstStep = 5;
    {
        WorldMap w(first(), p);
        ASSERT_TRUE(w.join(me));
        wait(w, me, 1);
        ASSERT_TRUE(w.contracts(me).size() == size_t(1));
        EXPECT_EQ(int(w.contracts(me)[0].step), 5);
        EXPECT_TRUE(w.contracts(me)[0].title == "Liquid Gold");
    }
    p.firstStep = 0;
    WorldMap w(first(), p);
    ASSERT_TRUE(w.join(me));
    wait(w, me, 1);
    EXPECT_EQ(w.contracts(me).size(), WorldContracts::slots);
}
