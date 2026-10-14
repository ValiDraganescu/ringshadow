// The buildings' jobs in a world (P6; WorldRoles.cpp, docs/prospector-progression.md
// "Buildings in the MMO"): a fallen Prospector back at its Citadel or nearest
// Hab Dome (Second wind sooner, with cargo), the Hab Dome a depot, the
// respec only at the Citadel, the world's MH prices, the Lab, the Spacedock
// a pad of the pilot's own, and the Wayfinder's flight nodes (Pad link,
// Homing beacon, Short hop, Ringwalker). Isolated scenes of a few seconds on
// The First Reach; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 61;
constexpr PilotId other = 62;
const std::string key = "0123456789abcdef0123456789abcdef";

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

/// A pilot at `level` with these ranks and every blueprint.
ProspectorProgress at(int64_t level, const std::map<Skill, int64_t>& ranks = {}) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    g.ranks = ranks;
    for (const StructureKind k : Progression::blueprintKinds()) g.blueprints.insert(k);
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

void wait(WorldMap& w, PilotId p, double seconds) {
    const int ticks = int(std::llround(seconds / w.start().description().step));
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

/// The wallet set where the pilot is (the side's bank), told to it.
void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

/// A `kind` of the pilot's put up round its Prospector, off the pads'
/// plates, and finished at once by hand. Nil: no spot, or refused.
std::optional<int64_t> put(WorldMap& w, PilotId p, StructureKind kind, std::string* why = nullptr) {
    Simulation& sim = simOf(w, p);
    const Unit u = *drivenUnit(w, p);
    const auto& pads = first().regions[*w.where(p)].built.pads;
    const double reach = Region::buildReach(kind);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
        for (int k = 0; k < 16; k++) {
            const double h = k * 2 * pi / 16;
            const auto spot = sim.snap(kind, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (!spot || distance(*spot, u.position) > reach + Region::buildSlack || sim.placementRefusal(kind, *spot) ||
                region::touchesPadPlate(pads, *spot, Rules::radius(kind)))
                continue;
            const int64_t id = sim.state.nextID;
            if (const auto no = w.build(p, kind, *spot)) {
                if (why) *why = *no;
                return std::nullopt;
            }
            for (Structure& s : sim.state.structures)
                if (s.id == id) {
                    s.buildLeft = std::nullopt;
                    s.hp = sim.fullHP(s);
                }
            w.step();
            return id;
        }
    return std::nullopt;
}

const Structure* building(WorldMap& w, PilotId p, int64_t id) {
    for (const Structure& s : simOf(w, p).state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// The driven Prospector moved to `at` (ground) and told it.
void moveTo(WorldMap& w, PilotId p, Vec2 at) {
    drivenUnit(w, p)->position = at;
    w.step();
}

/// The driven Prospector falls (with `cargo` ore).
void fall(WorldMap& w, PilotId p, int64_t cargo = 0) {
    Unit* u = drivenUnit(w, p);
    u->carrying = cargo;
    if (cargo > 0) u->hydrogen = std::nullopt;
    u->hp = 0;
    w.step();
}

Vec2 startPad() { return first().regions[first().start].built.pads[first().startPad].at; }

} // namespace

// A fallen Prospector waits 10 s, then comes back beside its Citadel, empty;
// the Progress message tells when and where. With no home: the start pad.
TEST(roles_respawn_at_the_citadel) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    w.setProgress(me, at(5));
    const auto citadel = put(w, me, StructureKind::citadel);
    ASSERT_TRUE(citadel.has_value());
    const Vec2 home = building(w, me, *citadel)->position;
    const int64_t before = drivenUnit(w, me)->id;
    fall(w, me, 30);
    EXPECT_TRUE(drivenUnit(w, me) == nullptr);
    const auto r = w.respawning(me);
    ASSERT_TRUE(r.has_value());
    EXPECT_NEAR(r->at, w.clock() + 10, 1e-6);
    EXPECT_TRUE(r->home == std::optional<StructureKind>(StructureKind::citadel));
    EXPECT_EQ(r->region, std::string("start"));
    EXPECT_EQ(r->cargo, int64_t(0));
    EXPECT_TRUE(w.progressMessage(me)->respawn == r);
    wait(w, me, 9.5);
    EXPECT_TRUE(drivenUnit(w, me) == nullptr);
    wait(w, me, 0.6);
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(u->id != before);
    EXPECT_EQ(u->carrying, int64_t(0));
    EXPECT_TRUE(distance(u->position, home) < Rules::radius(StructureKind::citadel) + 3);
    EXPECT_TRUE(!w.progressMessage(me)->respawn.has_value());
    EXPECT_EQ(*w.where(me), first().start);

    // No Citadel, no Hab Dome: the start pad.
    WorldMap n(first(), params());
    n.join(me);
    moveTo(n, me, startPad() + Vec2(0, 14));
    fall(n, me);
    EXPECT_TRUE(!n.respawning(me)->home.has_value());
    wait(n, me, 10.1);
    ASSERT_TRUE(drivenUnit(n, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(n, me)->position, startPad()) < 3);
}

// Second wind rank 3: back 75% sooner (2.5 s) with 75% of the cargo.
TEST(roles_respawn_with_second_wind) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(40, {{Skill::secondWind, 3}}));
    ASSERT_TRUE(put(w, me, StructureKind::citadel).has_value());
    const int64_t ore = w.wallet(me)->ore;
    fall(w, me, 40);
    const auto r = w.respawning(me);
    ASSERT_TRUE(r.has_value());
    EXPECT_NEAR(r->at - w.clock(), 2.5, 1e-6);
    EXPECT_EQ(r->cargo, int64_t(30));
    EXPECT_TRUE(!r->hydrogen);
    // The sim's note is taken.
    EXPECT_TRUE(!w.start().simulation().fallen.has_value());
    // Back beside its Citadel: the kept 30 go straight into the wallet.
    wait(w, me, 2.6);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_EQ(drivenUnit(w, me)->carrying + w.wallet(me)->ore - ore, int64_t(30));
}

// The nearest home: a Hab Dome in the region where it fell beats the
// Citadel a region away; with the dome gone, the Citadel, a region change.
TEST(roles_respawn_at_the_nearest_hab_dome) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(10));
    fill(w, me, 2000, 500);
    ASSERT_TRUE(put(w, me, StructureKind::citadel).has_value());
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing"));
    wait(w, me, 4.2);
    ASSERT_TRUE(first().regions[*w.where(me)].file.id == std::string("cold-fork"));
    const auto dome = put(w, me, StructureKind::habDome);
    ASSERT_TRUE(dome.has_value());
    const Vec2 domeAt = building(w, me, *dome)->position;
    fall(w, me);
    EXPECT_TRUE(w.respawning(me)->home == std::optional<StructureKind>(StructureKind::habDome));
    EXPECT_EQ(w.respawning(me)->region, std::string("cold-fork"));
    wait(w, me, 10.1);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, domeAt) < Rules::radius(StructureKind::habDome) + 3);
    // The dome gone: home is the Citadel, in the start region.
    for (Structure& s : simOf(w, me).state.structures)
        if (s.id == *dome) s.hp = 0;
    w.step();
    w.takeMoves();
    const int64_t visit = w.visit(me);
    fall(w, me);
    EXPECT_TRUE(w.respawning(me)->home == std::optional<StructureKind>(StructureKind::citadel));
    EXPECT_EQ(w.respawning(me)->region, std::string("start"));
    wait(w, me, 10.1);
    EXPECT_EQ(*w.where(me), first().start);
    EXPECT_EQ(w.visit(me), visit + 1);
    bool told = false;
    for (const Moved& m : w.takeMoves()) told = told || (m.pilot == me && m.change && m.change->regionId == "start");
    EXPECT_TRUE(told);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
}

// A pilot who leaves while fallen comes back at home when it seats again.
TEST(roles_seated_again_after_a_fall_comes_home) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    const auto citadel = put(w, me, StructureKind::citadel);
    ASSERT_TRUE(citadel.has_value());
    const Vec2 home = building(w, me, *citadel)->position;
    moveTo(w, me, startPad() + Vec2(0, 14));
    fall(w, me);
    w.leave(me);
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, home) < Rules::radius(StructureKind::citadel) + 3);
    EXPECT_TRUE(!w.respawning(me).has_value());
}

// The Hab Dome is a field depot: ore and MH walked up to it go into the
// wallet with their XP, as at the Citadel. The respec is refused there.
TEST(roles_hab_dome_depot) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(10));
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    fill(w, me, 1000, 100);
    const auto dome = put(w, me, StructureKind::habDome);
    ASSERT_TRUE(dome.has_value());
    const auto wallet = *w.wallet(me);
    EXPECT_EQ(wallet.ore, int64_t(850));
    EXPECT_EQ(wallet.hydrogen, int64_t(50));
    const Structure d = *building(w, me, *dome);
    const Vec2 out = normalize(drivenUnit(w, me)->position - d.position);
    moveTo(w, me, d.position + out * (Rules::radius(StructureKind::habDome) + 0.5));
    EXPECT_TRUE(w.respec(me).value_or("").rfind("Respec at your Citadel", 0) == 0);
    const double xp = w.progress(me)->xp;
    drivenUnit(w, me)->carrying = 25;
    w.step();
    EXPECT_EQ(w.wallet(me)->ore, int64_t(875));
    EXPECT_NEAR(w.progress(me)->xp - xp, 25 * WorldXp::ore, 1e-9);
    drivenUnit(w, me)->carrying = 20;
    drivenUnit(w, me)->hydrogen = true;
    w.step();
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(70));
}

// The world's prices: the MH part refused when the wallet can't pay it;
// a Bastion and a Sentinel need no Garrison (their fights come later).
TEST(roles_mh_costs) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(60));
    fill(w, me, 5000, 0);
    std::string why;
    EXPECT_TRUE(!put(w, me, StructureKind::habDome, &why).has_value());
    EXPECT_EQ(why, std::string("Not enough MH"));
    const std::pair<StructureKind, std::pair<int64_t, int64_t>> table[] = {
        {StructureKind::habDome, {150, 50}},   {StructureKind::foundry, {300, 150}},
        {StructureKind::lab, {200, 200}},      {StructureKind::spacedock, {400, 300}},
        {StructureKind::bastion, {250, 100}},  {StructureKind::sentinel, {150, 100}}};
    // (The Citadel and the Derrick, ore only: the core's price test.)
    for (const auto& [kind, price] : table) {
        fill(w, me, 5000, 1000);
        why.clear();
        const auto id = put(w, me, kind, &why);
        if (!id) std::printf("  %s: %s\n", Simulation::title(kind).c_str(), why.empty() ? "no spot" : why.c_str());
        ASSERT_TRUE(id.has_value());
        EXPECT_EQ(w.wallet(me)->ore, 5000 - price.first);
        EXPECT_EQ(w.wallet(me)->hydrogen, 1000 - price.second);
    }
}

// The Lab: a world's Prospector puts one up (level 25); the strategy
// game's build list still lacks it.
TEST(roles_lab_in_a_world) {
    EXPECT_TRUE(std::find(PilotBuild::kinds.begin(), PilotBuild::kinds.end(), StructureKind::lab) == PilotBuild::kinds.end());
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(24));
    fill(w, me, 1000, 1000);
    std::string why;
    EXPECT_TRUE(!put(w, me, StructureKind::lab, &why).has_value());
    EXPECT_EQ(why, std::string("The Lab opens at level 25"));
    w.setProgress(me, at(25));
    const auto lab = put(w, me, StructureKind::lab, &why);
    ASSERT_TRUE(lab.has_value());
    EXPECT_TRUE(!building(w, me, *lab)->parent.has_value());
    EXPECT_EQ(w.owned(me, StructureKind::lab), int64_t(1));
    EXPECT_TRUE(!put(w, me, StructureKind::lab, &why).has_value());
    EXPECT_TRUE(why.rfind("Lab limit reached", 0) == 0);
}

// The Spacedock: a pad of the pilot's own. It shows in its World message
// (not another pilot's); it flies from it, and lands on it from any pad,
// with no first-landing XP.
TEST(roles_spacedock_pad) {
    WorldParams fuel = params();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    w.seat(me, key);
    w.setProgress(me, at(35));
    fill(w, me, 1000, 1000);
    moveTo(w, me, startPad() + Vec2(0, 9));
    const auto dock = put(w, me, StructureKind::spacedock);
    ASSERT_TRUE(dock.has_value());
    const Vec2 dockAt = building(w, me, *dock)->position;
    const auto pads = w.ownPads(me);
    ASSERT_TRUE(pads.size() == size_t(1));
    EXPECT_EQ(pads[0].name, std::string("Spacedock"));
    const auto m = *w.worldMessage(me);
    const auto& start = m.regions[first().start].pads;
    ASSERT_TRUE(!start.empty());
    EXPECT_TRUE(start.back().name == "Spacedock" && start.back().building == std::optional<StructureKind>(StructureKind::spacedock));
    w.join(other);
    for (const auto& r : w.worldMessage(other)->regions)
        for (const auto& p : r.pads) EXPECT_TRUE(!p.building.has_value());
    // From its far side, out of the start pad's reach.
    const Vec2 away = normalize(dockAt - startPad());
    moveTo(w, me, dockAt + away * (Rules::radius(StructureKind::spacedock) + 1));
    ASSERT_TRUE(distance(drivenUnit(w, me)->position, startPad()) > w.padReach(me));
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing"));
    wait(w, me, 4.2);
    ASSERT_TRUE(first().regions[*w.where(me)].file.id == std::string("cold-fork"));
    // Back onto it.
    EXPECT_TRUE(w.fly(me, "start", "Dock").has_value());
    ASSERT_TRUE(!w.fly(me, "start", "Spacedock"));
    wait(w, me, 4.2);
    EXPECT_EQ(*w.where(me), first().start);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, dockAt) < Rules::radius(StructureKind::spacedock) + 3);
    EXPECT_TRUE(w.progress(me)->pads.count("start/Spacedock") == 0);
    // The other pilot can't land on it.
    EXPECT_TRUE(w.fly(other, "start", "Spacedock").has_value());
}

// Homing beacon: rank 1 the Citadel is a pad, rank 2 the Hab Domes too
// ("Hab Dome 1"); a change sends the World message again.
TEST(roles_homing_beacon) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(30));
    fill(w, me, 3000, 1000);
    const auto citadel = put(w, me, StructureKind::citadel);
    ASSERT_TRUE(citadel.has_value());
    moveTo(w, me, startPad() + Vec2(0, 14));
    ASSERT_TRUE(put(w, me, StructureKind::habDome).has_value());
    EXPECT_TRUE(w.ownPads(me).empty());
    w.takeMoves();
    w.setProgress(me, at(30, {{Skill::homingBeacon, 1}}));
    w.step();
    ASSERT_TRUE(w.ownPads(me).size() == size_t(1));
    EXPECT_EQ(w.ownPads(me)[0].name, std::string("Citadel"));
    bool told = false;
    for (const Moved& mv : w.takeMoves()) told = told || mv.pilot == me;
    EXPECT_TRUE(told);
    w.setProgress(me, at(30, {{Skill::homingBeacon, 2}}));
    w.step();
    ASSERT_TRUE(w.ownPads(me).size() == size_t(2));
    EXPECT_EQ(w.ownPads(me)[1].name, std::string("Hab Dome 1"));
    // Fly from the dome to the Citadel.
    const Vec2 citadelAt = building(w, me, *citadel)->position;
    ASSERT_TRUE(!w.fly(me, "start", "Citadel"));
    wait(w, me, 4.2);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, citadelAt) < Rules::radius(StructureKind::citadel) + 3);
}

// Pad link: rank 3 flies from 16 cells of a pad's centre, not 17.
// Short hop: rank 3 lands in 1.6 s, not 4.
TEST(roles_pad_link_and_short_hop) {
    WorldParams fuel = params();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    w.seat(me, key);
    w.setProgress(me, at(40, {{Skill::padLink, 3}, {Skill::shortHop, 3}}));
    EXPECT_NEAR(w.padReach(me), 16, 1e-9);
    EXPECT_NEAR(w.flightTime(me), 1.6, 1e-9);
    moveTo(w, me, startPad() + Vec2(0, 17));
    EXPECT_TRUE(w.fly(me, "cold-fork", "landing") == std::optional<std::string>("not at a pad"));
    moveTo(w, me, startPad() + Vec2(0, 15.5));
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing"));
    wait(w, me, 1.5);
    EXPECT_TRUE(w.flying(me));
    wait(w, me, 0.2);
    EXPECT_TRUE(!w.flying(me));
    EXPECT_EQ(first().regions[*w.where(me)].file.id, std::string("cold-fork"));
}

// Ringwalker: a flight from anywhere, then not again for 120 s (the
// Progress message says when); a pad still works meanwhile.
TEST(roles_ringwalker) {
    WorldParams fuel = params();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    w.seat(me, key);
    w.setProgress(me, at(80, {{Skill::ringwalker, 1}}));
    moveTo(w, me, startPad() + Vec2(0, 30));
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing"));
    EXPECT_NEAR(w.progressMessage(me)->ringwalkerReady, w.clock() + 120, 1e-6);
    wait(w, me, 4.2);
    const Vec2 pad = first().regions[*w.where(me)].built.pads[0].at;
    moveTo(w, me, pad + Vec2(0, 30));
    const auto why = w.fly(me, "start", "landing");
    EXPECT_TRUE(why.value_or("").rfind("Ringwalker is ready in", 0) == 0);
    moveTo(w, me, pad);
    EXPECT_TRUE(!w.fly(me, "start", "landing"));
}
