// The buildings' jobs in a world (P6; WorldRoles.cpp, docs/prospector-progression.md
// "Buildings in the MMO"): a fallen Prospector back at its Citadel or nearest
// Hab Dome (Second wind sooner, with cargo), the Hab Dome a depot, the
// respec only at the Citadel, the world's MH prices, the Lab, the Spacedock
// the big Landing Pad (F2), and the Wayfinder's flight nodes (Pad link,
// Short hop, Ringwalker) on the Dropship's flights between Landing Pads. Isolated scenes of a few seconds on
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
    // Back beside its Citadel: the kept 30 go straight into the wallet,
    // less the realm's tax (C1).
    wait(w, me, 2.6);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    const int64_t kept = drivenUnit(w, me)->carrying;
    EXPECT_EQ(kept + w.wallet(me)->ore - ore, kept > 0 ? int64_t(30) : 30 - Progression::realmTax(30));
}

// The nearest home: a Hab Dome in the region where it fell beats the
// Citadel a region away; with the dome gone, the Citadel, a region change.
TEST(roles_respawn_at_the_nearest_hab_dome) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(10));
    fill(w, me, 2000, 500);
    ASSERT_TRUE(put(w, me, StructureKind::citadel).has_value());
    const size_t fork = *w.regionNamed("cold-fork");
    ASSERT_TRUE(w.devTransfer(me, fork, first().regions[fork].built.pads[0].at));
    wait(w, me, 0.2);
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
    // 25 less the realm's tax of 3 (C1); the XP on all 25.
    EXPECT_EQ(w.wallet(me)->ore, int64_t(872));
    EXPECT_NEAR(w.progress(me)->xp - xp, 25 * WorldXp::ore, 1e-9);
    drivenUnit(w, me)->carrying = 20;
    drivenUnit(w, me)->hydrogen = true;
    w.step();
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(68));
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
    EXPECT_EQ(w.realmCount(StructureKind::lab), int64_t(1));
    EXPECT_TRUE(!put(w, me, StructureKind::lab, &why).has_value());
    EXPECT_TRUE(why.rfind("Lab limit reached", 0) == 0);
}

// F2, the Dropship (docs/mmorpg-m8.md): Landing Pads the realm builds, the
// flight between two of them, its refusals, its price and its stages.

namespace {

/// The pilot's Prospector into region `id` at its pad (`devTransfer`): the
/// region seen.
void goTo(WorldMap& w, PilotId p, const std::string& id) {
    const size_t r = *w.regionNamed(id);
    ASSERT_TRUE(w.devTransfer(p, r, first().regions[r].built.pads[0].at));
    w.step();
}

/// A Landing Pad put up beside the Prospector (finished), and the Prospector
/// moved onto it. Its ground point.
Vec2 landingPad(WorldMap& w, PilotId p) {
    const auto id = put(w, p, StructureKind::landingPad);
    EXPECT_TRUE(id.has_value());
    if (!id) return Vec2(0, 0);
    const Vec2 at = building(w, p, *id)->position;
    moveTo(w, p, at);
    return at;
}

/// Is the pilot's Prospector in any region's simulation?
bool anywhere(WorldMap& w, int64_t kindCount) {
    int64_t n = 0;
    for (size_t r = 0; r < w.regionCount(); r++)
        for (const Unit& u : w.region(r).simulation().state.units)
            if (u.kind == UnitKind::prospector) n += 1;
    return n >= kindCount;
}

} // namespace

// A Landing Pad: level 1, ore only, two in a region (a third refused), more
// in another region; the realm's, in every pilot's World message, numbered
// across the world; a region file's pad is not one.
TEST(roles_landing_pad_built_and_limited) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(1));
    fill(w, me, 2000, 0);
    moveTo(w, me, startPad() + Vec2(0, 10));
    ASSERT_TRUE(put(w, me, StructureKind::landingPad).has_value());
    moveTo(w, me, startPad() + Vec2(10, 10));
    ASSERT_TRUE(put(w, me, StructureKind::landingPad).has_value());
    moveTo(w, me, startPad() + Vec2(-10, 10));
    std::string why;
    EXPECT_TRUE(!put(w, me, StructureKind::landingPad, &why).has_value());
    EXPECT_EQ(why, std::string("Landing Pad limit reached: 2 in a region"));
    goTo(w, me, "cold-fork");
    fill(w, me, 2000, 0);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    ASSERT_TRUE(put(w, me, StructureKind::landingPad).has_value());
    const auto pads = w.landingPads();
    ASSERT_TRUE(pads.size() == size_t(3));
    // Numbered in their region: Cold Fork's is its Landing Pad 1.
    EXPECT_EQ(pads[0].name, std::string("Landing Pad 1"));
    EXPECT_EQ(first().regions[pads[0].region].file.id, std::string("cold-fork"));
    EXPECT_EQ(pads[2].name, std::string("Landing Pad 2"));
    // Every pilot's World message: the region file's pad, then the realm's.
    w.join(other);
    const auto theirs = *w.worldMessage(other);
    const auto& start = theirs.regions[first().start].pads;
    ASSERT_TRUE(start.size() == size_t(3));
    EXPECT_TRUE(!start[0].building);
    EXPECT_TRUE(start[1].name == "Landing Pad 1" && start[1].building == std::optional(StructureKind::landingPad));
    EXPECT_TRUE(start[2].name == "Landing Pad 2");
}

// The refusals: not on a Landing Pad, a region not seen yet, no Landing Pad
// there, a pad that isn't there, the pad it stands on, MH short; a region
// file's pad is no Landing Pad.
TEST(roles_flight_refusals) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 2000, 0);
    EXPECT_EQ(w.fly(me, "start", "landing").value_or(""), std::string("No Landing Pad in Prospect Landing: build one there first"));
    goTo(w, me, "cold-fork");
    fill(w, me, 2000, 0);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    const Vec2 far = landingPad(w, me);
    goTo(w, me, "start");
    fill(w, me, 2000, 0);
    moveTo(w, me, startPad() + Vec2(0, 10));
    const Vec2 here = landingPad(w, me);
    moveTo(w, me, here + Vec2(0, w.padReach(me) + 0.5));
    EXPECT_EQ(w.fly(me, "cold-fork", "Landing Pad 1").value_or(""),
              std::string("Not at a Landing Pad: stand on one to call the Dropship"));
    moveTo(w, me, here);
    EXPECT_EQ(w.fly(me, "slag-pockets", "Landing Pad 1").value_or(""),
              std::string("Not discovered yet: Slag Pockets. Go there on foot first"));
    EXPECT_EQ(w.fly(me, "cold-fork", "Landing Pad 7").value_or(""), std::string("No Landing Pad 7 in Cold Fork"));
    EXPECT_EQ(w.fly(me, "start", "Landing Pad 1").value_or(""), std::string("At Landing Pad 1 already"));
    EXPECT_EQ(w.fly(me, "cold-fork", "Landing Pad 1").value_or(""), std::string("Not enough MH to fly: 15 MH, you have 0"));
    (void)far;
    EXPECT_TRUE(!w.flying(me));
}

// The flight: the price taken at the call (10 + 5 a step), the Prospector
// aboard at once (in no region: nothing can touch it), the pilot moved into
// the target region when the approach starts with nothing driven (the World
// message's ferry says where and when), then the Prospector on the target
// pad at the touch down, a first landing paid once.
TEST(roles_flight_between_landing_pads) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    goTo(w, me, "cold-fork");
    fill(w, me, 2000, 0);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    const Vec2 there = landingPad(w, me);
    goTo(w, me, "start");
    fill(w, me, 2000, 40);
    moveTo(w, me, startPad() + Vec2(0, 10));
    const Vec2 here = landingPad(w, me);
    const double xp = w.progress(me)->xp;
    ASSERT_TRUE(!w.fly(me, "cold-fork", "Landing Pad 1"));
    EXPECT_TRUE(w.flying(me));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(25));
    EXPECT_TRUE(drivenUnit(w, me) == nullptr);
    EXPECT_TRUE(!anywhere(w, 1));
    const auto m = *w.worldMessage(me);
    ASSERT_TRUE(m.ferry.has_value());
    EXPECT_EQ(m.ferry->pad, std::string("Landing Pad 1"));
    EXPECT_EQ(m.ferry->from, int64_t(first().start));
    EXPECT_TRUE(m.ferry->plan == Ferry::plan(1));
    const size_t fork = *w.regionNamed("cold-fork");
    EXPECT_TRUE(length(m.ferry->at - first().regions[fork].built.regionAt(there)) < 1e-9);
    EXPECT_TRUE(length(m.ferry->boarded - first().regions[first().start].built.regionAt(here)) < 1e-9);
    const Ferry::Plan plan = Ferry::plan(1);
    // Before the approach: still in its region, aboard.
    wait(w, me, plan.approachAt() - 0.2);
    EXPECT_EQ(*w.where(me), first().start);
    EXPECT_TRUE(!anywhere(w, 1));
    w.takeMoves();
    wait(w, me, 0.4);
    // The approach: in the target region, nothing driven yet; its region
    // change told.
    EXPECT_EQ(*w.where(me), fork);
    EXPECT_TRUE(w.flying(me) && drivenUnit(w, me) == nullptr);
    bool changed = false;
    for (const Moved& mv : w.takeMoves()) changed = changed || (mv.pilot == me && mv.change && mv.change->regionId == "cold-fork");
    EXPECT_TRUE(changed);
    EXPECT_TRUE(w.worldMessage(me)->ferry.has_value());
    // The touch down: on the target pad.
    wait(w, me, plan.approach - 0.4);
    EXPECT_TRUE(w.flying(me));
    wait(w, me, 0.3);
    EXPECT_TRUE(!w.flying(me));
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, there) < Rules::radius(StructureKind::landingPad) + 1);
    EXPECT_TRUE(!w.worldMessage(me)->ferry.has_value());
    EXPECT_TRUE(w.progress(me)->pads.count("cold-fork/Landing Pad 1") == 1);
    EXPECT_TRUE(w.progress(me)->xp > xp);
}

// Aboard, untouchable: a Prospector at 1 hp that boards comes down whole
// as it was, never hit; bugs on the pad find nothing to bite.
TEST(roles_flight_aboard_untouchable) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 2000, 40);
    moveTo(w, me, startPad() + Vec2(0, 10));
    landingPad(w, me);
    moveTo(w, me, startPad() + Vec2(12, 10));
    landingPad(w, me);
    drivenUnit(w, me)->hp = 1;
    ASSERT_TRUE(!w.fly(me, "start", "Landing Pad 1"));
    for (int i = 0; i < 40; i++) {
        wait(w, me, 0.5);
        if (!w.flying(me)) break;
        EXPECT_TRUE(drivenUnit(w, me) == nullptr && !anywhere(w, 1));
        EXPECT_TRUE(!w.respawning(me).has_value());
    }
    ASSERT_TRUE(!w.flying(me) && drivenUnit(w, me) != nullptr);
    EXPECT_NEAR(drivenUnit(w, me)->hp, 1, 1e-9);
    // A hop on the same ground: no region change, the full plan's time.
    EXPECT_EQ(*w.where(me), first().start);
}

// The Spacedock: the big pad. Flights from it cost half (rounded up); it
// is a Landing Pad to land on too, by name.
TEST(roles_spacedock_halves_the_price) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(35));
    goTo(w, me, "slag-pockets");
    fill(w, me, 3000, 1000);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    landingPad(w, me);
    goTo(w, me, "start");
    fill(w, me, 3000, 1000);
    moveTo(w, me, startPad() + Vec2(0, 12));
    const auto dock = put(w, me, StructureKind::spacedock);
    ASSERT_TRUE(dock.has_value());
    const Vec2 dockAt = building(w, me, *dock)->position;
    bool named = false;
    for (const LandingPad& o : w.landingPads()) named = named || (o.name == "Spacedock" && o.kind == StructureKind::spacedock);
    EXPECT_TRUE(named);
    // From its edge: 20 MH to Slag Pockets (two steps), 10 from the dock.
    moveTo(w, me, dockAt + Vec2(0, Rules::radius(StructureKind::spacedock) + 1));
    fill(w, me, 3000, 100);
    ASSERT_TRUE(!w.fly(me, "slag-pockets", "Landing Pad 1"));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(90));
    wait(w, me, Ferry::plan(2).total() + 0.2);
    ASSERT_TRUE(!w.flying(me));
    EXPECT_EQ(first().regions[*w.where(me)].file.id, std::string("slag-pockets"));
    // Back onto the Spacedock: full price from a Landing Pad.
    fill(w, me, 3000, 100);
    ASSERT_TRUE(!w.fly(me, "start", "Spacedock"));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(80));
    wait(w, me, Ferry::plan(2).total() + 0.2);
    ASSERT_TRUE(drivenUnit(w, me) != nullptr);
    EXPECT_TRUE(distance(drivenUnit(w, me)->position, dockAt) < Rules::radius(StructureKind::spacedock) + 3);
}

// Ringwalker: flights cost no MH, from any Landing Pad, and the Dropship
// comes 30% faster (the plan, and the landing).
TEST(roles_ringwalker_free_and_faster) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(80, {{Skill::ringwalker, 1}}));
    goTo(w, me, "cold-fork");
    fill(w, me, 2000, 0);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    landingPad(w, me);
    goTo(w, me, "start");
    fill(w, me, 2000, 0);
    moveTo(w, me, startPad() + Vec2(0, 10));
    landingPad(w, me);
    const Ferry::Plan plan = w.ferryPlan(me, *w.regionNamed("cold-fork"));
    EXPECT_NEAR(plan.total() * 1.3, Ferry::plan(1).total(), 1e-9);
    ASSERT_TRUE(!w.fly(me, "cold-fork", "Landing Pad 1"));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(0));
    wait(w, me, plan.total() + 0.2);
    EXPECT_TRUE(!w.flying(me));
    EXPECT_EQ(first().regions[*w.where(me)].file.id, std::string("cold-fork"));
}

// Pad link: rank 3 calls the Dropship from 16 cells of a Landing Pad's
// centre, not 17. Short hop: rank 3 takes 60% off the dash, only.
TEST(roles_pad_link_and_short_hop) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(40, {{Skill::padLink, 3}, {Skill::shortHop, 3}}));
    EXPECT_NEAR(w.padReach(me), 16, 1e-9);
    const Ferry::Plan plan = w.ferryPlan(me, *w.regionNamed("cold-fork"));
    EXPECT_NEAR(plan.dash, Ferry::plan(1).dash * 0.4, 1e-9);
    EXPECT_NEAR(plan.lift, Ferry::lift, 1e-9);
    goTo(w, me, "cold-fork");
    fill(w, me, 2000, 0);
    moveTo(w, me, drivenUnit(w, me)->position + Vec2(0, 10));
    landingPad(w, me);
    goTo(w, me, "start");
    fill(w, me, 2000, 100);
    moveTo(w, me, startPad() + Vec2(0, 20));
    const Vec2 pad = landingPad(w, me);
    moveTo(w, me, pad + Vec2(17, 0));
    EXPECT_TRUE(w.fly(me, "cold-fork", "Landing Pad 1").has_value());
    moveTo(w, me, pad + Vec2(15.5, 0));
    EXPECT_TRUE(!w.fly(me, "cold-fork", "Landing Pad 1"));
}
