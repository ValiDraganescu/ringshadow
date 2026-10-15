// Frontier pilots (P1, docs/frontier.md): the pilots the server runs come
// only after the first human finished the opening chain, by Dropship, and
// keep off the chain's places while a human is on it; each habit decides by
// the world's state (a miner drills, hauls, puts a Derrick on its well and a
// Hab Dome by a far field; a builder guards a raided field and gives a
// region its Landing Pad; a defender fights bugs at the realm's buildings,
// mends them and joins a canyon wave a human holds; a wanderer opens sites,
// takes Citadel surveys and moves on); they keep the pilot's rules (the
// shop's prices, the world's building prices and limits, the frontier's
// caps, the tax, the Dropship's fare); where no human is near their work is
// booked on the world's clock; they are saved, and an old save gets them.
// Isolated scenes on The First, a few world minutes at most; never a game.
// Run from unreal/server.

#include "test.h"

#include "Progression.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::server;
using Habit = WorldFrontier::Habit;
using Act = FrontierPlan::Act;
using NewsKind = net::ProgressMessage::NewsKind;
using FrontierPilot = WorldMap::FrontierPilot;

constexpr PilotId a = 71;
const std::string keyA = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
constexpr PilotId b = 72;
const std::string keyB = "e0e1e2e3e4e5e6e7e8e9eaebecedeeef";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Nothing else pays or spawns: only the frontier pilots at work. The chain
/// done (`firstStep` 0) unless a test says otherwise.
WorldParams params(int firstStep = 0) {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.starfalls = false;
    p.brood = false;
    p.canyons = false;
    p.firstStep = firstStep;
    p.frontierSeed = 0xf207ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(std::lround(seconds * 60));
    for (int i = 0; i < n; i++) w.step();
}

size_t named(const WorldMap& w, const std::string& id) { return *w.regionNamed(id); }

/// A finished building of the realm at region cell `cell` (or the nearest
/// spot round it), built by `by` (a key; empty: nobody known).
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

const Structure* structure(const WorldMap& w, size_t r, int64_t id) {
    for (const Structure& s : w.region(r).simulation().state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// Pilot A kept and on line, the chain done (or not), its Prospector in the
/// start region.
void seat(WorldMap& w, PilotId p = a, const std::string& key = keyA) {
    ASSERT_TRUE(w.seat(p, key) == WorldMap::Seated::fresh);
    w.step();
}

/// The roster made, and frontier pilot `habit` come into region `r` at
/// region cell `cell` (the others still to come), its wallet `ore`/`mh`.
FrontierPilot& come(WorldMap& w, Habit habit, size_t r, Vec2 cell, int64_t ore = 600, int64_t mh = 40, int64_t level = 0) {
    w.frontierRoster();
    FrontierPilot* out = nullptr;
    for (FrontierPilot& f : w.frontier())
        if (f.habit == habit && !out) out = &f;
    out->arrived = true;
    out->region = r;
    out->at = w.region(r).places()->ground(cell);
    out->wallet = WorldMap::Wallet{ore, mh};
    if (level > 0) out->progress.xp = Progression::xp(level);
    out->progress.regions.insert(w.loaded()->regions[r].file.id);
    for (FrontierPilot& f : w.frontier())
        if (&f != out) f.comes = 1e12;
    return *out;
}

FrontierPilot& pilot(WorldMap& w, Habit habit) {
    for (FrontierPilot& f : w.frontier())
        if (f.habit == habit) return f;
    return w.frontier().front();
}

std::vector<net::ProgressMessage::News> frontierNews(WorldMap& w, PilotId p) {
    std::vector<net::ProgressMessage::News> out;
    for (const auto& n : w.takeNews(p))
        if (n.kind == NewsKind::frontier) out.push_back(n);
    return out;
}

} // namespace

/// None come while the only human is on the opening chain; once it is done
/// the roster is made (five, a habit each, distinct callsigns and paints),
/// and they come by Dropship 45 s on, one every 40 s, each told ("come") and
/// heard on the radio.
TEST(Frontier_comeAfterTheOpeningChainOnly) {
    WorldMap w(first(), params(1));
    seat(w);
    stepFor(w, 60);
    EXPECT_TRUE(!(w.frontierOpen()));
    EXPECT_TRUE(w.chainRunning());
    EXPECT_TRUE(w.frontier().empty());
    // The chain done (a save and a restore with the pilot at step 0).
    WorldSave s = w.save();
    WorldMap v(first(), params(0));
    ASSERT_TRUE(v.restore(s).empty());
    ASSERT_TRUE(v.seat(a, keyA) == WorldMap::Seated::restored);
    // The restored pilot is still on the chain (its step saved): a new one done.
    ASSERT_TRUE(v.seat(b, keyB) == WorldMap::Seated::fresh);
    v.step();
    EXPECT_TRUE(v.frontierOpen());
    ASSERT_TRUE(v.frontier().size() == 5);
    std::set<std::string> names;
    std::set<int32_t> paints;
    std::set<Habit> habits;
    for (const FrontierPilot& f : v.frontier()) {
        names.insert(f.callsign);
        paints.insert(f.paint);
        habits.insert(f.habit);
        EXPECT_TRUE(!(f.arrived));
        EXPECT_TRUE(f.paint != 0 && f.paint != 1 && f.paint != 6);
        EXPECT_TRUE(f.progress.level() >= 2);
    }
    EXPECT_TRUE(names.size() == 5 && paints.size() == 5 && habits.size() == 4);
    const double t0 = v.clock();
    while (v.clock() - t0 < WorldFrontier::arriveAfter + 1) v.step();
    int64_t come = 0;
    for (const FrontierPilot& f : v.frontier()) come += f.arrived ? 1 : 0;
    EXPECT_EQ(come, int64_t(1));
    while (v.clock() - t0 < WorldFrontier::arriveAfter + WorldFrontier::arriveEvery + 1) v.step();
    come = 0;
    for (const FrontierPilot& f : v.frontier()) come += f.arrived ? 1 : 0;
    EXPECT_EQ(come, int64_t(2));
    const auto news = frontierNews(v, b);
    int64_t landed = 0, hello = 0;
    for (const auto& n : news) {
        landed += n.id == "come" ? 1 : 0;
        hello += n.id == "radio" ? 1 : 0;
    }
    std::printf("  %s\n", v.radioLog().empty() ? "" : v.radioLog().front().c_str());
    EXPECT_EQ(landed, int64_t(2));
    EXPECT_EQ(hello, int64_t(2));
    // `--frontier 0`: none.
    WorldParams none = params(0);
    none.frontier = 0;
    WorldMap n(first(), none);
    seat(n);
    stepFor(n, 1);
    EXPECT_TRUE(n.frontier().empty());
}

/// While a human is on the chain, its places are kept off: the start pad,
/// the chain's sites, the deposit by the pad and the Derrick's well; no
/// frontier pilot lands at the start pad, drills that deposit, opens those
/// sites or builds there. Once the chain is done, they may.
TEST(Frontier_keepOffTheChainsPlaces) {
    // A done with the chain (saved), B new and on it.
    WorldMap w0(first(), params(0));
    seat(w0);
    WorldSave s = w0.save();
    WorldMap w(first(), params(1));
    ASSERT_TRUE(w.restore(s).empty());
    ASSERT_TRUE(w.seat(b, keyB) == WorldMap::Seated::fresh);
    w.step();
    ASSERT_TRUE(w.frontierOpen());
    ASSERT_TRUE(w.chainRunning());
    const size_t start = named(w, "start");
    const auto places = w.chainPlaces(start);
    ASSERT_TRUE(places.size() >= 4);
    for (const Vec2 p : places) EXPECT_TRUE(w.keptOff(start, p));
    EXPECT_TRUE(w.chainPlaces(named(w, "cold-fork")).empty());
    // They come, never at the start pad.
    w.frontierRoster();
    for (FrontierPilot& f : w.frontier()) f.comes = w.clock();
    stepFor(w, 0.5);
    for (const FrontierPilot& f : w.frontier()) {
        ASSERT_TRUE(f.arrived);
        if (f.region == start) EXPECT_TRUE(!(w.keptOff(start, f.at)));
    }
    // A miner beside the chain's deposit drills another one.
    const Vec2 deposit = places[places.size() - 2];
    FrontierPilot& m = pilot(w, Habit::miner);
    m.region = start;
    m.at = deposit + Vec2(1, 0);
    m.travelTo = std::nullopt;
    place(w, start, StructureKind::citadel, Vec2(100, 40));
    const FrontierPlan plan = w.frontierDecide(m);
    std::printf("  miner: %s at %.0f,%.0f\n", plan.why.c_str(), plan.at.x, plan.at.y);
    if (plan.act == Act::mine) EXPECT_TRUE(distance(plan.at, deposit) >= WorldFrontier::keepOff);
    // Building there is refused by the caps.
    EXPECT_TRUE(w.frontierCap(start, StructureKind::habDome, places[0]).has_value());
    // B done: nothing is kept off.
    ASSERT_TRUE(w.save().pilots.size() == 2);
    WorldSave t = w.save();
    WorldMap v(first(), params(0));
    ASSERT_TRUE(v.restore(t).empty());
    ASSERT_TRUE(v.seat(a, keyA) == WorldMap::Seated::restored);
    v.step();
    EXPECT_TRUE(!(v.chainRunning()));
    EXPECT_TRUE(!(v.keptOff(start, places[0])));
}

/// A miner: drills the nearest deposit with a depot, hauls a load home,
/// pumps a tank at a Derrick every few trips, puts a Derrick on the well by
/// its field (its blueprint bought first, at the Citadel), and a Hab Dome by
/// a field far from every depot.
TEST(Frontier_minerDecides) {
    WorldMap w(first(), params());
    seat(w);
    const size_t r = named(w, "rust-hollow");
    const int64_t citadel = place(w, r, StructureKind::citadel, Vec2(72, 48));
    ASSERT_TRUE(citadel > 0);
    FrontierPilot& f = come(w, Habit::miner, r, Vec2(76, 52), 100, 0, 4);
    FrontierPlan p = w.frontierDecide(f);
    std::printf("  %s\n", p.why.c_str());
    EXPECT_TRUE(p.act == Act::mine || p.act == Act::buy);
    // Poor: it drills.
    f.wallet = WorldMap::Wallet{0, 0};
    p = w.frontierDecide(f);
    ASSERT_TRUE(p.act == Act::mine);
    const GameState& st = w.region(r).simulation().state;
    EXPECT_TRUE(st.patches[size_t(p.target)].remaining > 0);
    // A load: home to the Citadel.
    f.cargo = 5;
    p = w.frontierDecide(f);
    EXPECT_TRUE(p.act == Act::haul && p.target == citadel);
    f.cargo = 0;
    // Every few trips a tank of MH at a Derrick of the realm.
    std::optional<Vec2> well;
    for (const auto& x : *st.wells)
        if (!well || distance(x.position, f.at) < distance(*well, f.at)) well = x.position;
    ASSERT_TRUE(well.has_value());
    Simulation& sim = w.region(r).simulation();
    const auto spot = sim.snap(StructureKind::derrick, *well);
    ASSERT_TRUE(spot.has_value());
    Structure d(sim.state.nextID++, StructureKind::derrick, Pilot::player, *spot);
    d.hp = Rules::hp(StructureKind::derrick);
    sim.state.structures.push_back(d);
    sim.refreshNav();
    f.trips = WorldFrontier::pumpEvery;
    p = w.frontierDecide(f);
    EXPECT_TRUE(p.act == Act::pump && p.target == d.id);
    f.trips = 0;
    sim.state.structures.pop_back();
    sim.refreshNav();
    // With the money: the Derrick's blueprint at the Citadel, then the
    // Derrick on the well.
    f.at = *well + Vec2(4, 0);
    f.wallet = WorldMap::Wallet{2000, 200};
    p = w.frontierDecide(f);
    std::printf("  %s\n", p.why.c_str());
    EXPECT_TRUE(p.act == Act::buy && p.kind == StructureKind::derrick && p.target == citadel);
    f.progress.giveBlueprint(StructureKind::derrick);
    p = w.frontierDecide(f);
    EXPECT_TRUE(p.act == Act::build && p.kind == StructureKind::derrick && distance(p.at, *well) < 2);
    // At level 8 with a field far from every depot: a Hab Dome by it (the
    // wells dry, so no Derrick comes first).
    for (auto& x : *sim.state.wells) x.remaining = 0;
    f.progress.blueprints.clear();
    f.progress.giveBlueprint(StructureKind::habDome);
    f.progress.xp = Progression::xp(8);
    // The field farthest from the Citadel.
    std::optional<size_t> far;
    for (size_t k = 0; k < st.patches.size(); k++)
        if (!far || distance(st.patches[k].position, structure(w, r, citadel)->position) >
                        distance(st.patches[*far].position, structure(w, r, citadel)->position))
            far = k;
    f.at = st.patches[*far].position + Vec2(1, 1);
    p = w.frontierDecide(f);
    std::printf("  far field %.0f cells out: %s\n", distance(st.patches[*far].position, structure(w, r, citadel)->position),
                p.why.c_str());
    if (distance(st.patches[*far].position, structure(w, r, citadel)->position) > WorldFrontier::depotFar + 6)
        EXPECT_TRUE(p.act == Act::build && p.kind == StructureKind::habDome);
}

/// A builder: a Gatling Gun by a field the Brood lingered at (none by a
/// field a turret guards), a Landing Pad where the realm has a Citadel and
/// no pad; otherwise it drills for its keep.
TEST(Frontier_builderDecides) {
    WorldParams p = params();
    p.brood = true;
    WorldMap w(first(), p);
    seat(w);
    const size_t r = named(w, "rust-hollow");
    place(w, r, StructureKind::citadel, Vec2(72, 48));
    FrontierPilot& f = come(w, Habit::builder, r, Vec2(76, 52), 3000, 300, 12);
    f.progress.giveBlueprint(StructureKind::landingPad);
    f.progress.giveBlueprint(StructureKind::gatling);
    FrontierPlan plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::build && plan.kind == StructureKind::landingPad);
    // A group lingers at a deposit: the field was raided.
    w.step();
    const auto group = w.spawnBroodGroup(r, UnitKind::shardling, 2, WorldMap::Poi::deposit, 0, std::nullopt, true);
    ASSERT_TRUE(group.has_value());
    w.step();
    plan = w.frontierDecide(f);
    std::printf("  %s at %.0f,%.0f\n", plan.why.c_str(), plan.at.x, plan.at.y);
    EXPECT_TRUE(plan.act == Act::build && plan.kind == StructureKind::gatling);
    const Vec2 gun = plan.at;
    // A turret by the field: no second one.
    Simulation& sim = w.region(r).simulation();
    Structure s(sim.state.nextID++, StructureKind::gatling, Pilot::player, gun);
    s.hp = Rules::hp(StructureKind::gatling);
    sim.state.structures.push_back(s);
    sim.refreshNav();
    plan = w.frontierDecide(f);
    EXPECT_TRUE(!(plan.act == Act::build && plan.kind == StructureKind::gatling));
    // Nothing to build, no blueprint money: it drills.
    f.progress.blueprints.clear();
    f.wallet = WorldMap::Wallet{0, 0};
    plan = w.frontierDecide(f);
    EXPECT_TRUE(plan.act == Act::mine);
}

/// A defender: bugs near the realm's buildings draw it into a fight; a hurt
/// building is mended; else it patrols the realm's buildings (and drills
/// for its keep every other minute).
TEST(Frontier_defenderDecides) {
    WorldParams p = params();
    p.brood = true;
    WorldMap w(first(), p);
    seat(w);
    const size_t r = named(w, "rust-hollow");
    const int64_t citadel = place(w, r, StructureKind::citadel, Vec2(72, 48));
    FrontierPilot& f = come(w, Habit::defender, r, Vec2(80, 48), 300, 20, 10);
    w.setClock(120);
    FrontierPlan plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::patrol);
    // A hurt building: mended.
    Simulation& sim = w.region(r).simulation();
    for (Structure& s : sim.state.structures)
        if (s.id == citadel) s.hp = Rules::hp(StructureKind::citadel) * 0.5;
    plan = w.frontierDecide(f);
    EXPECT_TRUE(plan.act == Act::repair && plan.target == citadel);
    // Bugs at the Citadel: a fight first.
    w.step();
    const auto group = w.spawnBroodGroup(r, UnitKind::shardling, 3, WorldMap::Poi::building, citadel,
                                         structure(w, r, citadel)->position + Vec2(6, 0), true);
    ASSERT_TRUE(group.has_value());
    w.step();
    plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::fight);
    const auto bug = sim.state.unit(plan.target);
    ASSERT_TRUE(bug.has_value());
    EXPECT_TRUE(bug->bug.has_value());
}

/// A wanderer: opens a site here it has not opened (a vault only at its
/// floor), takes a Citadel survey of a region next door at the realm's
/// Citadel (refused anywhere else), walks there, and is paid on reaching it
/// as a mission of its tier.
TEST(Frontier_wandererDecides) {
    WorldMap w(first(), params());
    seat(w);
    const size_t r = named(w, "cold-fork");
    FrontierPilot& f = come(w, Habit::wanderer, r, Vec2(72, 52), 300, 40, 6);
    FrontierPlan plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    ASSERT_TRUE(plan.act == Act::open);
    // Both sites opened: on to a region it has not seen.
    for (size_t i = 0; i < w.loaded()->regions[r].built.sites.size(); i++) f.progress.sites.insert(w.siteId(r, i));
    plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::walk && plan.region != r);
    // At the realm's Citadel: a survey from the board.
    const int64_t citadel = place(w, r, StructureKind::citadel, Vec2(72, 48));
    plan = w.frontierDecide(f);
    ASSERT_TRUE(plan.act == Act::mission);
    EXPECT_TRUE(w.frontierMission(f.id, plan.region).has_value());  // not at the Citadel yet
    const Structure c = *structure(w, r, citadel);
    f.at = c.position + Vec2(Rules::radius(StructureKind::citadel) + 0.5, 0);
    EXPECT_TRUE(!(w.frontierMission(f.id, plan.region).has_value()));
    ASSERT_TRUE(f.missionRegion.has_value());
    plan = w.frontierDecide(f);
    EXPECT_TRUE(plan.act == Act::walk && plan.region == *f.missionRegion);
    // There: paid as a Citadel mission of its tier.
    const int64_t ore = f.wallet.ore;
    const double xp = f.progress.xp;
    const size_t to = *f.missionRegion;
    f.region = to;
    f.at = w.loaded()->regions[to].built.ground(w.loaded()->regions[to].built.extent() * 0.5);
    f.thought = -1e9;
    stepFor(w, 0.1);
    EXPECT_TRUE(!(f.missionRegion.has_value()));
    EXPECT_TRUE(f.wallet.ore > ore && f.progress.xp > xp);
}

/// The rules, as for a pilot: a blueprint at the shop's price only at the
/// realm's Citadel, from its own wallet, the next copy dearer; a building
/// only with its blueprint, at its floor, within the caps, off the pads'
/// plates, within its reach, at the world's price; a flight only from a
/// Landing Pad to one, at the Dropship's fare; the Citadel's tax on its
/// unloads.
TEST(Frontier_keepThePilotsRules) {
    WorldMap w(first(), params());
    seat(w);
    const size_t r = named(w, "rust-hollow");
    const int64_t citadel = place(w, r, StructureKind::citadel, Vec2(72, 48));
    FrontierPilot& f = come(w, Habit::builder, r, Vec2(20, 20), 1000, 200, 12);
    const uint32_t id = f.id;
    // Away from the Citadel: refused.
    EXPECT_TRUE(w.frontierBuy(id, StructureKind::habDome).has_value());
    const Structure c = *structure(w, r, citadel);
    f.at = c.position + Vec2(Rules::radius(StructureKind::citadel) + 0.5, 0);
    const ShopPrice price = f.progress.blueprintPrice(StructureKind::habDome);
    EXPECT_TRUE(!(w.frontierBuy(id, StructureKind::habDome).has_value()));
    EXPECT_EQ(f.wallet.ore, int64_t(1000) - price.ore);
    EXPECT_EQ(f.wallet.hydrogen, int64_t(200) - price.hydrogen);
    EXPECT_EQ(f.progress.copies(StructureKind::habDome), int64_t(1));
    EXPECT_TRUE(f.progress.blueprintPrice(StructureKind::habDome).ore > price.ore);
    // Short: refused, nothing taken.
    f.wallet = WorldMap::Wallet{10, 0};
    EXPECT_TRUE(w.frontierBuy(id, StructureKind::habDome).has_value());
    EXPECT_EQ(f.wallet.ore, int64_t(10));
    // A building: without its blueprint, refused; under its floor, refused.
    f.wallet = WorldMap::Wallet{5000, 500};
    f.at = w.region(r).places()->ground(Vec2(100, 70));
    EXPECT_TRUE(w.frontierBuild(id, StructureKind::gatling, f.at + Vec2(3, 0)).has_value());
    f.progress.giveBlueprint(StructureKind::foundry);
    EXPECT_TRUE(w.frontierBuild(id, StructureKind::foundry, f.at + Vec2(3, 0)).has_value());
    // Too far from its Prospector: refused.
    EXPECT_TRUE(w.frontierBuild(id, StructureKind::habDome, f.at + Vec2(20, 0)).has_value());
    // At its feet: built, the world's price paid, the blueprint used up, the
    // building the realm's, its builder the frontier pilot.
    const int64_t before = f.wallet.ore;
    const auto spot = w.region(r).simulation().snap(StructureKind::habDome, f.at + Vec2(Region::buildReach(StructureKind::habDome), 0));
    ASSERT_TRUE(spot.has_value());
    const auto why = w.frontierBuild(id, StructureKind::habDome, *spot);
    if (why) std::printf("  refused: %s\n", why->c_str());
    ASSERT_TRUE(!(why.has_value()));
    EXPECT_EQ(f.wallet.ore, before - Progression::worldOre(StructureKind::habDome));
    EXPECT_EQ(f.progress.copies(StructureKind::habDome), int64_t(0));
    ASSERT_TRUE(f.building.has_value());
    EXPECT_EQ(w.builder(r, *f.building), f.callsign);
    // The cap: a second Hab Dome of the frontier's, refused anywhere.
    f.progress.giveBlueprint(StructureKind::habDome);
    f.at = w.region(r).places()->ground(Vec2(30, 80));
    EXPECT_TRUE(w.frontierCap(r, StructureKind::habDome, f.at + Vec2(4, 0)).has_value());
    EXPECT_TRUE(w.frontierBuild(id, StructureKind::habDome, f.at + Vec2(4, 0)).has_value());
    // On a pad's plate: refused.
    const Vec2 pad = w.region(r).places()->pads[0].at;
    f.at = pad + Vec2(2, 0);
    f.progress.giveBlueprint(StructureKind::landingPad);
    EXPECT_TRUE(w.frontierBuild(id, StructureKind::landingPad, pad).has_value());
    // A flight: from no pad, refused; from a Landing Pad to one, the fare.
    const size_t to = named(w, "glass-flats");
    EXPECT_TRUE(w.frontierFly(id, to).has_value());
    const auto here = w.devLandingPad(r, w.region(r).places()->ground(Vec2(40, 40)));
    const auto there = w.devLandingPad(to, w.region(to).places()->ground(Vec2(72, 30)));
    ASSERT_TRUE(here && there);
    f.at = structure(w, r, *here)->position;
    f.wallet.hydrogen = 1;
    EXPECT_TRUE(w.frontierFly(id, to).has_value());
    f.wallet.hydrogen = 200;
    const int64_t steps = 3;  // Rust Hollow (0, 1) to Glass Flats (2, 0)
    EXPECT_TRUE(!(w.frontierFly(id, to).has_value()));
    EXPECT_EQ(f.wallet.hydrogen, int64_t(200) - ac::Flight::price(steps, false, false));
    EXPECT_TRUE(f.travelTo == to && f.aboard);
    // The tax: an unload of 100 ore pays the realm its share.
    const int64_t realm = w.realmWallet().ore;
    FrontierPilot& m = pilot(w, Habit::miner);
    m.arrived = true;
    m.region = r;
    m.travelTo = std::nullopt;
    m.wallet = WorldMap::Wallet{0, 0};
    m.cargo = 100;
    m.at = c.position + Vec2(Rules::radius(StructureKind::citadel) + 0.5, 0);
    m.comes = 0;
    // Booked (no human near Rust Hollow? A is in the start region next to
    // it: live). Its unit unloads at the Citadel within a few seconds.
    stepFor(w, 6);
    std::printf("  realm +%lld, miner +%lld\n", (long long)(w.realmWallet().ore - realm), (long long)m.wallet.ore);
    EXPECT_EQ(w.realmWallet().ore - realm, Progression::realmTax(100));
    EXPECT_EQ(m.wallet.ore, int64_t(100) - Progression::realmTax(100));
}

/// Live where a human can see it, booked elsewhere: its Prospector is in
/// the simulation while a human is in its region or next to it, and out of
/// it otherwise; booked, a miner's loads come at an unskilled Prospector's
/// pace from a real deposit, taxed; with no human on line (the server's
/// idle) the same.
TEST(Frontier_bookedWhereNoHumanIsNear) {
    WorldMap w(first(), params());
    seat(w);
    const size_t start = named(w, "start");
    const size_t far = named(w, "slag-pockets");  // not next to the start
    place(w, far, StructureKind::citadel, Vec2(72, 48));
    place(w, start, StructureKind::citadel, Vec2(100, 40));
    FrontierPilot& f = come(w, Habit::miner, start, Vec2(96, 44), 0, 0, 3);
    stepFor(w, 0.2);
    EXPECT_TRUE(f.unit.has_value());
    EXPECT_TRUE(w.frontierLive(f));
    // Into Slag Pockets: booked, no unit.
    f.region = far;
    f.at = w.region(far).places()->ground(Vec2(76, 52));
    stepFor(w, 0.2);
    EXPECT_TRUE(!(w.frontierLive(f)));
    EXPECT_TRUE(!(f.unit.has_value()));
    const int64_t realm = w.realmWallet().ore;
    // A few minutes of booked drilling.
    stepFor(w, 120);
    const int64_t mined = f.wallet.ore + f.taxed.ore;
    std::printf("  booked: %lld ore in 120 s (rate %.2f/s), taxed %lld\n", (long long)mined, WorldFrontier::bookRate(),
                (long long)f.taxed.ore);
    EXPECT_TRUE(mined > 0);
    EXPECT_TRUE(double(mined) <= WorldFrontier::bookRate() * 120 + Rules::carry);
    EXPECT_EQ(w.realmWallet().ore - realm, f.taxed.ore);
    // No human on line: the server's idle books it the same.
    w.leave(a);
    const int64_t was = f.wallet.ore;
    for (int i = 0; i < 60; i++) w.idle(1.0);
    std::printf("  idle: +%lld ore in 60 s\n", (long long)(f.wallet.ore - was));
    EXPECT_TRUE(f.wallet.ore > was);
}

/// A defender joins a canyon wave a human holds: it walks to the canyon's
/// region, fights the wave's bugs, and is paid its share by damage when
/// the wave is beaten, as any pilot who fought it.
TEST(Frontier_joinACanyonWave) {
    WorldParams p = params();
    p.canyons = true;
    p.broodSeed = 0xca2e0ULL;
    WorldMap w(first(), p);
    seat(w);
    const size_t r = named(w, "cold-fork");
    ProspectorProgress g;
    g.xp = Progression::xp(20);
    w.setProgress(a, g);
    ASSERT_TRUE(w.devTransfer(a, r, w.region(r).places()->ground(Vec2(100, 52))));
    w.step();
    FrontierPilot& f = come(w, Habit::defender, named(w, "start"), Vec2(72, 30), 100, 10, 14);
    const WorldMap::Canyon& c = *w.canyon(r, "the-fork");
    // No wave out: it stays.
    FrontierPlan plan = w.frontierDecide(f);
    EXPECT_TRUE(!(plan.act == Act::walk && plan.region == r));
    ASSERT_TRUE(w.canyonSend(r, "the-fork", 1));
    plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::walk && plan.region == r);
    // At the prize: it fights the wave's bugs.
    f.region = r;
    f.at = c.middle + Vec2(2, 0);
    f.thought = -1e9;
    w.step();
    plan = w.frontierDecide(f);
    std::printf("  %s\n", plan.why.c_str());
    EXPECT_TRUE(plan.act == Act::fight || plan.act == Act::patrol);
    double hit = 0;
    for (int i = 0; i < 90 * 4 && c.out; i++) {
        stepFor(w, 0.25);
        if (const auto d = c.frontierDamage.find(f.id); d != c.frontierDamage.end()) hit = d->second;
    }
    std::printf("  frontier damage %.0f, wave out %d\n", hit, int(c.out));
    EXPECT_TRUE(hit > 0);
    // Beaten: its share paid.
    if (c.out) {
        const int64_t ore = f.wallet.ore;
        ASSERT_TRUE(w.canyonBeat(r, "the-fork"));
        EXPECT_TRUE(f.wallet.ore > ore);
    }
}

/// Saved and loaded: the roster, each pilot's habit, level, wallet, region
/// and Prospector come back; its buildings keep it as their builder.
TEST(Frontier_savedAndLoaded) {
    WorldMap w(first(), params());
    seat(w);
    const size_t r = named(w, "start");
    FrontierPilot& f = come(w, Habit::miner, r, Vec2(96, 44), 777, 33, 9);
    f.taxed = WorldMap::Wallet{12, 3};
    const int64_t gun = place(w, r, StructureKind::gatling, Vec2(60, 70));
    stepFor(w, 0.2);
    WorldSave s = w.save();
    for (SavedRegion& sr : s.regions)
        for (SavedBuilding& sb : sr.buildings)
            if (sr.id == "start" && sb.id == gun) sb.builtBy = f.key;
    ASSERT_TRUE(!(s.frontier.empty()));
    // Through the store's schema 21.
    const std::string path = "build/save-tests/frontier.sqlite";
    std::remove(path.c_str());
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(s, &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_TRUE(loaded->frontier == s.frontier);
    WorldMap v(first(), params());
    ASSERT_TRUE(v.restore(*loaded).empty());
    ASSERT_TRUE(v.frontier().size() == w.frontier().size());
    const FrontierPilot* g = nullptr;
    for (const FrontierPilot& x : v.frontier())
        if (x.id == f.id) g = &x;
    ASSERT_TRUE(g != nullptr);
    EXPECT_EQ(g->callsign, f.callsign);
    EXPECT_TRUE(g->habit == f.habit && g->paint == f.paint && g->arrived);
    EXPECT_EQ(g->progress.level(), int64_t(9));
    EXPECT_TRUE(g->wallet == f.wallet && g->taxed == f.taxed);
    EXPECT_EQ(g->region, r);
    EXPECT_TRUE(distance(g->at, f.at) < 0.5);
    EXPECT_EQ(v.builder(r, gun), f.callsign);
    // Its Prospector, saved as its own unit.
    ASSERT_TRUE(g->unit.has_value());
    EXPECT_TRUE(v.region(r).simulation().state.unit(*g->unit).has_value());
}

/// An old save (none in it, schema 20): a pilot done with the chain gets the
/// frontier pilots on its first step after the load; one still on it waits.
TEST(Frontier_oldSaveGetsThem) {
    WorldMap w(first(), params());
    seat(w);
    WorldSave s = w.save();
    s.frontier.clear();
    WorldMap v(first(), params());
    ASSERT_TRUE(v.restore(s).empty());
    EXPECT_TRUE(v.frontier().empty());
    ASSERT_TRUE(v.seat(a, keyA) == WorldMap::Seated::restored);
    v.step();
    EXPECT_EQ(int64_t(v.frontier().size()), int64_t(5));
    // On the chain: none.
    WorldMap w1(first(), params(1));
    seat(w1);
    WorldSave t = w1.save();
    t.frontier.clear();
    WorldMap u(first(), params(1));
    ASSERT_TRUE(u.restore(t).empty());
    ASSERT_TRUE(u.seat(a, keyA) == WorldMap::Seated::restored);
    stepFor(u, 1);
    EXPECT_TRUE(u.frontier().empty());
}

/// Told: the sky's list (region, place, unit, level, doing) and the roster
/// in the Progress message (callsign, habit, paint); a frontier pilot's
/// building finished is a deed every human hears ("Vesper built a Hab Dome
/// in Rust Hollow"), not the plain "built" news.
TEST(Frontier_toldToThePilots) {
    WorldMap w(first(), params());
    seat(w);
    const size_t r = named(w, "start");
    FrontierPilot& f = come(w, Habit::builder, r, Vec2(96, 44), 3000, 300, 12);
    stepFor(w, 0.2);
    const auto sky = w.sky();
    ASSERT_TRUE(sky.has_value());
    ASSERT_TRUE(sky->frontier.size() == 1);
    EXPECT_TRUE(sky->frontier[0].id == f.id && sky->frontier[0].region == int64_t(r) && sky->frontier[0].unit == *f.unit);
    EXPECT_EQ(sky->frontier[0].level, int64_t(12));
    const auto m = w.progressMessage(a);
    ASSERT_TRUE(m.has_value());
    ASSERT_TRUE(m->frontier.size() == 1);
    EXPECT_TRUE(m->frontier[0].callsign == f.callsign && m->frontier[0].paint == f.paint);
    // A Hab Dome of its own, finished.
    w.takeNews(a);
    std::printf("  %s\n", w.devFrontier("frontier build habDome").c_str());
    ASSERT_TRUE(f.building.has_value());
    stepFor(w, 3);
    bool deed = false, plain = false;
    for (const auto& n : w.takeNews(a)) {
        if (n.kind == NewsKind::frontier && n.id == "deed") {
            std::printf("  %s\n", n.by.c_str());
            deed = deed || n.by.find("built a Hab Dome in") != std::string::npos;
        }
        plain = plain || n.kind == NewsKind::built;
    }
    EXPECT_TRUE(deed);
    EXPECT_TRUE(!(plain));
}
