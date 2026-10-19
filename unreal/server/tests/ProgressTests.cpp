// The Prospector's progression in a world (docs/prospector-progression.md,
// P3; WorldProgress.cpp): XP from unloading and welding at ×1, the
// one-offs paid once per pilot, the skill tree's requests accepted and
// refused (a respec only at the pilot's Citadel, its MH from the wallet),
// the progression kept through an edge and a flight, the Progress message
// over the protocol, and the wells growing back. Isolated scenes of a few
// seconds; run from unreal/server.

#include "test.h"

#include "HeadlessClient.h"
#include "Rules.h"
#include "Server.h"
#include "ShuttleBoard.h"
#include "WorldMap.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;
using region::Edge;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId me = 31;
constexpr PilotId other = 32;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// Every blueprint for a new pilot (`--dev-blueprints`): these scenes are
/// about building, not finding blueprints (SitesTests.cpp gates them).
WorldParams blueprinted() {
    WorldParams p;
    p.devBlueprints = true;
    // No first-ten-minutes pay in the sums (FirstTenTests.cpp).
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

/// Steps with the pilot's walk at `heading` until `done`, at most `ticks`.
template <class F> bool drive(WorldMap& w, PilotId p, double heading, int ticks, F&& done, Vec2 walk = Vec2(1, 0)) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.walk = walk;
        in.heading = heading;
        w.input(p, in);
        w.step();
        if (done()) return true;
    }
    return false;
}

void stand(WorldMap& w, PilotId p, int ticks) {
    drive(w, p, drivenUnit(w, p)->heading, ticks, [] { return false; }, Vec2(0, 0));
}

/// A Citadel placed by the pilot round the start pad, off its plate.
std::optional<int64_t> placeCitadel(WorldMap& w, PilotId p) {
    Simulation& sim = w.start().simulation();
    const Unit u = *drivenUnit(w, p);
    const double reach = Region::buildReach(StructureKind::citadel);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5) {
        for (int k = 0; k < 16; k++) {
            const double h = k * 2 * pi / 16;
            const auto at = sim.snap(StructureKind::citadel, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (!at || distance(*at, u.position) > reach + Region::buildSlack || sim.placementRefusal(StructureKind::citadel, *at)
                || region::touchesPadPlate(first().regions[first().start].built.pads, *at, Rules::radius(StructureKind::citadel)))
                continue;
            const int64_t id = sim.state.nextID;
            if (!w.build(p, StructureKind::citadel, *at)) return id;
        }
    }
    return std::nullopt;
}

Structure* structure(WorldMap& w, int64_t id) {
    for (Structure& s : w.start().simulation().state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

/// A finished Citadel of the pilot's, and its driven unit at its edge.
Structure* atHome(WorldMap& w, PilotId p) {
    const auto id = placeCitadel(w, p);
    if (!id) return nullptr;
    Structure* c = structure(w, *id);
    c->buildLeft = std::nullopt;
    c->builder = std::nullopt;
    c->hp = Rules::hp(StructureKind::citadel);
    Unit* v = drivenUnit(w, p);
    v->position = c->position + normalize(v->position - c->position) * (Rules::radius(StructureKind::citadel) + 0.4);
    return c;
}

bool hasNews(const std::vector<net::ProgressMessage::News>& news, NewsKind kind, const std::string& id = "") {
    for (const auto& n : news)
        if (n.kind == kind && (id.empty() || n.id == id)) return true;
    return false;
}

/// A progression at `level`, nothing spent, every blueprint.
ProspectorProgress at(int64_t level) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    for (const StructureKind k : Progression::blueprintKinds()) g.learned.insert(k);
    return g;
}

} // namespace

// Unloading at the pilot's own Citadel: 1 XP an ore, 2 an MH, at ×1; the
// first MH unloaded is a feat (50), paid once; level 2 at 500 is news.
TEST(progress_unloading_earns_ore_1_and_mh_2) {
    WorldMap w(first(), blueprinted());
    w.join(me);
    ASSERT_TRUE(atHome(w, me) != nullptr);
    EXPECT_EQ(w.progress(me)->xp, 0.0);
    drivenUnit(w, me)->carrying = 600;
    drivenUnit(w, me)->hydrogen = false;
    stand(w, me, 10);
    EXPECT_EQ(drivenUnit(w, me)->carrying, int64_t(0));
    EXPECT_NEAR(w.progress(me)->xp, 600 * WorldXp::ore, 1e-9);
    EXPECT_EQ(w.progress(me)->level(), int64_t(2));
    auto news = w.takeNews(me);
    EXPECT_TRUE(hasNews(news, NewsKind::level));
    // MH: 2 each, and the feat once.
    for (int trip = 0; trip < 2; trip++) {
        drivenUnit(w, me)->carrying = 20;
        drivenUnit(w, me)->hydrogen = true;
        stand(w, me, 10);
    }
    EXPECT_NEAR(w.progress(me)->xp, 600 + 2 * 20 * WorldXp::hydrogen + WorldXp::firstHydrogen, 1e-9);
    news = w.takeNews(me);
    int feats = 0;
    for (const auto& n : news) feats += n.kind == NewsKind::feat && n.id == WorldXp::hydrogenFeat;
    EXPECT_EQ(feats, 1);
    // Off the strategy game's leveling: nothing there.
    EXPECT_TRUE(!w.levels(me) || w.levels(me)->xp(UnitKind::prospector) == 0);
    // The snapshot carries the XP.
    EXPECT_NEAR(*w.snapshot(me).xp, w.progress(me)->xp, 1e-9);
}

// Welding a whole Citadel of its own: ½ its cost, and the Citadel's feat
// (150) when it is finished.
TEST(progress_welding_pays_half_the_cost_and_the_citadel_feat) {
    WorldMap w(first(), blueprinted());
    w.join(me);
    const auto id = placeCitadel(w, me);
    ASSERT_TRUE(id.has_value());
    const bool done = drive(w, me, drivenUnit(w, me)->heading, int((Rules::buildTime(StructureKind::citadel) + 5) * 60), [&] {
        return structure(w, *id)->complete();
    }, Vec2(0, 0));
    ASSERT_TRUE(done);
    const double cost = double(Rules::cost(StructureKind::citadel) + Rules::hydrogenCost(StructureKind::citadel));
    std::printf("  a Citadel welded: %.1f XP\n", w.progress(me)->xp);
    EXPECT_NEAR(w.progress(me)->xp, WorldXp::weld * cost + WorldXp::citadel, 1e-6);
    EXPECT_TRUE(w.progress(me)->feats.count(WorldXp::built(StructureKind::citadel)) == 1);
}

// The one-offs, once per pilot: a first region (250), a first flight (50),
// a first landing on a Landing Pad (50, F2: the start region's too); the
// start region pays nothing, and doing it all again pays nothing more.
// Another pilot is paid its own.
TEST(progress_one_offs_paid_once) {
    WorldParams fuel = blueprinted();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    w.join(me);
    w.join(other);
    const std::string start = first().regions[first().start].file.id;
    const std::string startPad = "Landing Pad 1";
    const Vec2 home = first().regions[first().start].built.pads[first().startPad].at;
    ASSERT_TRUE(w.devLandingPad(first().start, home + Vec2(0, 10)).has_value());
    const Vec2 ext = w.start().places()->extent();
    const auto& span = w.start().places()->openings[size_t(Edge::east)][0];
    auto east = [&] {
        Unit* u = drivenUnit(w, me);
        u->position = w.regionOf(me)->places()->ground(Vec2(ext.x - 4, (span.from + span.to) / 2));
        u->heading = 0;
        const int64_t v = w.visit(me);
        return drive(w, me, 0, 180, [&] { return w.visit(me) == v + 1; });
    };
    // From the Landing Pad of the region it is in.
    // C4: aboard the ship it last landed in, or called at a pad here.
    auto flyTo = [&](const std::string& region, const std::string& pad) {
        if (w.shuttleOf(me)) {
            shuttletest::standIn(w, me, shuttletest::kAtConsole);
        } else {
            for (const LandingPad& o : w.landingPads())
                if (o.region == *w.where(me)) drivenUnit(w, me)->position = o.at;
            if (shuttletest::board(w, me)) return false;
        }
        if (w.fly(me, region, pad)) return false;
        for (int i = 0; i < 60 * 30 && w.flying(me); i++) w.step();
        return !w.flying(me);
    };
    ASSERT_TRUE(east());
    const std::string there = first().regions[*w.where(me)].file.id;
    EXPECT_NEAR(w.progress(me)->xp, WorldXp::region, 1e-9);
    EXPECT_TRUE(hasNews(w.takeNews(me), NewsKind::region, there));
    // M2: the regions been to go to the client (the world map's pictures).
    EXPECT_TRUE(w.progressMessage(me)->regionsSeen.count(there) == 1);
    const size_t thereAt = *w.regionNamed(there);
    ASSERT_TRUE(w.devLandingPad(thereAt, drivenUnit(w, me)->position + Vec2(-8, 0)).has_value());
    // Back to the start's Landing Pad: a first flight and a first landing;
    // the start region pays nothing.
    ASSERT_TRUE(flyTo(start, startPad));
    EXPECT_NEAR(w.progress(me)->xp, WorldXp::region + WorldXp::firstFlight + WorldXp::pad, 1e-9);
    // To the other region's pad: its first landing.
    const std::string farPad = "Landing Pad 1";
    ASSERT_TRUE(flyTo(there, farPad));
    const double all = WorldXp::region + WorldXp::firstFlight + 2 * WorldXp::pad;
    EXPECT_NEAR(w.progress(me)->xp, all, 1e-9);
    EXPECT_TRUE(w.progress(me)->pads.count(there + "/" + farPad) == 1);
    // All again: nothing more.
    ASSERT_TRUE(flyTo(start, startPad));
    ASSERT_TRUE(east());
    ASSERT_TRUE(flyTo(start, startPad));
    ASSERT_TRUE(flyTo(there, farPad));
    EXPECT_NEAR(w.progress(me)->xp, all, 1e-9);
    // A feat asked for twice is paid once; the other pilot has none of it.
    EXPECT_TRUE(!w.feat(me, WorldXp::flightFeat));
    EXPECT_EQ(w.progress(other)->xp, 0.0);
    EXPECT_TRUE(w.feat(other, WorldXp::flightFeat));
    EXPECT_NEAR(w.progress(other)->xp, WorldXp::firstFlight, 1e-9);
}

// Spend, Undo and Slot, accepted and refused with the catalogue's reasons.
TEST(progress_spend_undo_and_slot) {
    WorldMap w(first(), blueprinted());
    w.join(me);
    // Level 1: no point yet.
    EXPECT_TRUE(w.spend(me, Skill::quickDrill) == std::optional<std::string>("Tier 1 opens at level 2"));
    w.setProgress(me, at(10));
    EXPECT_EQ(w.progress(me)->unspent(), int64_t(9));
    // Gates: a tier 2 active wants 5 points in its branch; a capstone level 70.
    EXPECT_TRUE(w.spend(me, Skill::overcharge) == std::optional<std::string>("Needs 5 points in Deep Vein"));
    EXPECT_TRUE(w.spend(me, Skill::stardustHeart).value_or("").rfind("Capstones open at level 70", 0) == 0);
    // Five in Deep Vein's first tier, then Overcharge.
    int spent = 0;
    for (const SkillNode& n : Progression::nodes()) {
        if (n.branch != Branch::deepVein || n.tier != 1) continue;
        for (int64_t r = 0; r < n.ranks && spent < 5; r++, spent++) ASSERT_TRUE(!w.spend(me, n.skill));
    }
    ASSERT_TRUE(spent == 5);
    EXPECT_TRUE(!w.spend(me, Skill::overcharge));
    EXPECT_EQ(w.progress(me)->unspent(), int64_t(3));
    // Slots: Q opens at 10, C at 25; only a learned active goes in.
    EXPECT_TRUE(!w.slot(me, 0, Skill::overcharge));
    EXPECT_TRUE(w.slot(me, 1, Skill::overcharge) == std::optional<std::string>("Slot C opens at level 25"));
    EXPECT_TRUE(w.slot(me, 0, Skill::quickDrill) == std::optional<std::string>("Quick drill is not an active"));
    EXPECT_TRUE(w.slot(me, 0, Skill::quickfill) == std::optional<std::string>("Quickfill is not learned"));
    EXPECT_TRUE(w.slot(me, 3, std::nullopt) == std::optional<std::string>("No such slot"));
    EXPECT_TRUE(w.progress(me)->slots[0] == std::optional<Skill>(Skill::overcharge));
    // Undo: a tier 1 point holds Overcharge up; Overcharge itself comes back
    // and leaves its slot.
    EXPECT_TRUE(w.undo(me, Skill::quickDrill) == std::optional<std::string>("Holds up Overcharge"));
    EXPECT_TRUE(!w.undo(me, Skill::overcharge));
    EXPECT_TRUE(!w.progress(me)->slots[0].has_value());
    EXPECT_EQ(w.progress(me)->unspent(), int64_t(4));
    EXPECT_TRUE(!w.undo(me, Skill::quickDrill));
    // A minute later the rest stay.
    w.setClock(w.clock() + Progression::undoWindow + 1);
    EXPECT_TRUE(w.undo(me, Skill::quickDrill) == std::optional<std::string>("No point spent here in the last minute"));
    EXPECT_EQ(w.progress(me)->spent(), int64_t(4));
}

// A respec only at the pilot's own Citadel, where it unloads; the first is
// free, the next costs MH from the wallet (40 at level 10).
TEST(progress_respec_at_the_citadel_for_mh) {
    WorldMap w(first(), blueprinted());
    w.join(me);
    w.setProgress(me, at(10));
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    // Away from home.
    EXPECT_TRUE(w.respec(me).value_or("").rfind("Respec at your Citadel", 0) == 0);
    ASSERT_TRUE(atHome(w, me) != nullptr);
    // The first: free.
    const int64_t mh = w.wallet(me)->hydrogen;
    EXPECT_TRUE(!w.respec(me));
    EXPECT_EQ(w.progress(me)->spent(), int64_t(0));
    EXPECT_EQ(w.wallet(me)->hydrogen, mh);
    EXPECT_TRUE(w.respec(me) == std::optional<std::string>("No points spent"));
    // The next: 40 MH, which the wallet lacks, then has.
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    ASSERT_TRUE(!w.slot(me, 0, std::nullopt));
    EXPECT_EQ(w.progressMessage(me)->respecPrice, Progression::respecPrice(10));
    EXPECT_TRUE(!w.progressMessage(me)->respecFree);
    w.start().simulation().state.players[0].hydrogen = 25;
    w.step();
    EXPECT_TRUE(w.respec(me) == std::optional<std::string>("Needs 40 MH"));
    w.start().simulation().state.players[0].hydrogen = 100;
    w.step();
    EXPECT_TRUE(!w.respec(me));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(60));
    EXPECT_EQ(w.start().simulation().state.players[0].hydrogen, int64_t(60));
    EXPECT_TRUE(w.progress(me)->ranks.empty());
    EXPECT_EQ(w.progress(me)->unspent(), int64_t(9));
    // Another pilot's Citadel is not home.
    w.join(other);
    w.setProgress(other, at(10));
    ASSERT_TRUE(!w.spend(other, Skill::quickDrill));
    EXPECT_TRUE(w.respec(other).value_or("").rfind("Respec at your Citadel", 0) == 0);
}

// The progression goes with the pilot across an edge and on a flight.
TEST(progress_kept_through_an_edge_and_a_flight) {
    WorldParams fuel = blueprinted();
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    w.join(me);
    w.setProgress(me, at(12));
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    const ProspectorProgress before = *w.progress(me);
    const Vec2 ext = w.start().places()->extent();
    const auto& span = w.start().places()->openings[size_t(Edge::east)][0];
    Unit* u = drivenUnit(w, me);
    u->position = w.start().places()->ground(Vec2(ext.x - 4, (span.from + span.to) / 2));
    u->heading = 0;
    ASSERT_TRUE(drive(w, me, 0, 180, [&] { return w.visit(me) == 1; }));
    EXPECT_EQ(w.progress(me)->rank(Skill::quickDrill), int64_t(1));
    EXPECT_NEAR(w.progress(me)->xp, before.xp + WorldXp::region, 1e-9);
    // F2: from a Landing Pad here to one at the start.
    const Vec2 home = first().regions[first().start].built.pads[first().startPad].at;
    ASSERT_TRUE(w.devLandingPad(first().start, home + Vec2(0, 10)).has_value());
    const auto padHere = w.devLandingPad(*w.where(me), drivenUnit(w, me)->position + Vec2(-8, 0));
    ASSERT_TRUE(padHere.has_value());
    for (const LandingPad& o : w.landingPads())
        if (o.building == *padHere && o.region == *w.where(me)) drivenUnit(w, me)->position = o.at;
    ASSERT_TRUE(!shuttletest::boardAndFly(w, me, first().regions[first().start].file.id, "Landing Pad 1"));
    // In flight: still its own, and a request still lands.
    ASSERT_TRUE(!w.spend(me, Skill::quickDrill));
    for (int i = 0; i < 60 * 30 && w.flying(me); i++) w.step();
    ASSERT_TRUE(!w.flying(me));
    EXPECT_EQ(w.progress(me)->rank(Skill::quickDrill), int64_t(2));
}

// The region's simulation has the driving pilot's progression (P2's
// skills) from the take and after every change, and its own buildings; a
// slot key in the input fires the slotted active, whose cooldown goes in the
// Progress message and goes on across an edge.
TEST(progress_in_the_regions_sim_and_actives_as_inputs) {
    WorldMap w(first(), blueprinted());
    w.join(me);
    const Simulation& sim = w.start().simulation();
    EXPECT_TRUE(sim.progress == w.progress(me));
    w.setProgress(me, at(10));
    int spent = 0;
    for (const SkillNode& n : Progression::nodes()) {
        if (n.branch != Branch::deepVein || n.tier != 1) continue;
        for (int64_t r = 0; r < n.ranks && spent < 5; r++, spent++) ASSERT_TRUE(!w.spend(me, n.skill));
    }
    ASSERT_TRUE(!w.spend(me, Skill::overcharge));
    ASSERT_TRUE(!w.slot(me, 0, Skill::overcharge));
    ASSERT_TRUE(sim.progress.has_value());
    EXPECT_TRUE(*sim.progress == *w.progress(me));
    EXPECT_TRUE(!sim.skillEffects.empty());
    const auto home = placeCitadel(w, me);
    ASSERT_TRUE(home.has_value());
    EXPECT_TRUE(sim.pilotBuildings.count(*home) == 1);
    // Q down: Overcharge fires once, then waits.
    const int64_t unit = *w.start().driving(me);
    PilotInput in;
    in.tick = ++inputTick;
    in.heading = drivenUnit(w, me)->heading;
    in.slot = 1;
    ASSERT_TRUE(w.input(me, in));
    w.step();
    const auto ready = sim.activeReady(unit, Skill::overcharge);
    ASSERT_TRUE(ready.has_value());
    EXPECT_NEAR(*ready - sim.state.time, Progression::node(Skill::overcharge).active->cooldown, 0.05);
    ASSERT_TRUE(w.progressMessage(me)->ready.count(Skill::overcharge) == 1);
    EXPECT_NEAR(w.progressMessage(me)->ready.at(Skill::overcharge), *ready, 1e-9);
    // The press is spent: the next input without it fires nothing more.
    stand(w, me, 30);
    EXPECT_NEAR(*sim.activeReady(unit, Skill::overcharge), *ready, 1e-9);
    // Handed back: no skills in the sim.
    w.start().handBack(me);
    EXPECT_TRUE(!sim.progress.has_value());
    EXPECT_TRUE(sim.pilotBuildings.empty());
    ASSERT_TRUE(w.start().take(me, unit));
    // (Its XP alone is not pushed every step: the skills read the ranks.)
    ASSERT_TRUE(sim.progress.has_value());
    EXPECT_TRUE(sim.progress->ranks == w.progress(me)->ranks);
    EXPECT_TRUE(sim.progress->slots == w.progress(me)->slots);
    // East across the edge: the progression there, none left behind, and
    // the cooldown goes on where it was.
    const double left = *ready - sim.state.time;
    const Vec2 ext = w.start().places()->extent();
    const auto& span = w.start().places()->openings[size_t(Edge::east)][0];
    Unit* u = drivenUnit(w, me);
    u->position = w.start().places()->ground(Vec2(ext.x - 4, (span.from + span.to) / 2));
    u->heading = 0;
    const double clockBefore = w.clock();
    ASSERT_TRUE(drive(w, me, 0, 180, [&] { return w.visit(me) == 1; }));
    const Simulation& there = w.regionOf(me)->simulation();
    EXPECT_TRUE(!sim.progress.has_value());
    ASSERT_TRUE(there.progress.has_value());
    EXPECT_EQ(there.progress->rank(Skill::overcharge), int64_t(1));
    const auto again = there.activeReady(*w.regionOf(me)->driving(me), Skill::overcharge);
    ASSERT_TRUE(again.has_value());
    EXPECT_NEAR(*again - there.state.time, left - (w.clock() - clockBefore), 0.05);
}

// A well no one pumps grows back 1 MH every 10 s, up to what it held at
// the start; one being pumped does not.
TEST(progress_world_wells_regrow_while_idle) {
    WorldMap w(first(), blueprinted());
    std::optional<size_t> r;
    for (size_t i = 0; i < w.regionCount() && !r; i++)
        if (w.region(i).simulation().state.wells && !w.region(i).simulation().state.wells->empty()) r = i;
    ASSERT_TRUE(r.has_value());
    Well& well = (*w.region(*r).simulation().state.wells)[0];
    const int64_t full = well.remaining;
    well.remaining = full - 3;
    for (int i = 0; i < 25 * 60; i++) w.step();
    EXPECT_EQ(well.remaining, full - 1);
    // Pumped (a Prospector in a Derrick on it): it does not grow.
    Simulation& sim = w.region(*r).simulation();
    const int64_t derrick = sim.state.nextID++;
    sim.state.structures.push_back(Structure(derrick, StructureKind::derrick, Pilot::player, well.position));
    sim.state.structures.back().hp = Rules::hp(StructureKind::derrick);
    const int64_t pump = w.region(*r).admit(sim.freshUnit(UnitKind::prospector, 0, Pilot::player, well.position, 0));
    int grew = 0;
    for (int i = 0; i < 5 * 60; i++) {
        Unit& u = sim.state.units[size_t(*sim.unitIndex(pump))];
        u.task = Unit::Task::inDerrick;
        u.structure = derrick;
        u.carrying = 0;
        well.harvester = pump;
        const int64_t was = well.remaining;
        w.step();
        grew += well.remaining > was;
    }
    EXPECT_EQ(grew, 0);
    std::printf("  pumped 5 s: %lld of %lld left\n", (long long)well.remaining, (long long)full);
    EXPECT_TRUE(well.remaining > 0 && well.remaining < full - 1);
    w.region(*r).release(pump);
    sim.state.structures.pop_back();
    sim.reindex();
    well.harvester = std::nullopt;
    well.remaining = full - 2;
    // Never past full.
    for (int i = 0; i < 45 * 60; i++) w.step();
    EXPECT_EQ(well.remaining, full);
}

// Over the protocol: the Progress message when the progression changes
// (more than its XP), with its news; Spend and Slot requests answered,
// refusals with their reason; the XP in the mirror; a Pick refused.
TEST(progress_over_the_protocol) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    ServerOptions o;
    o.world = first();
    o.worldParams.devBlueprints = true;
    o.worldParams.devXp = Progression::xp(10);
    o.worldParams.firstStep = 0;
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(clientPort, nettest::clientConfig());
    double now = 100;
    auto until = [&](auto&& done, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            now += 1.0 / 60;
            server.tick(now);
            client.tick(now);
            if (done()) return true;
        }
        return false;
    };
    client.join(net::newPilotKey());
    client.connect(now);
    ASSERT_TRUE(until([&] { return client.progress() && client.mirror() && client.mirror()->xp(); }, 10));
    EXPECT_EQ(client.progress()->level, int64_t(10));
    EXPECT_EQ(client.progress()->points, int64_t(9));
    EXPECT_TRUE(client.progress()->respecFree);
    EXPECT_NEAR(*client.mirror()->xp(), Progression::xp(10), 1e-3);
    EXPECT_TRUE(hasNews(client.news, NewsKind::level));
    auto ask = [&](uint32_t id) -> net::ReplyMessage {
        if (!until([&] { return client.replies.count(id) > 0; }, 3)) return net::ReplyMessage{id, false, "no reply"};
        return client.replies[id];
    };
    net::ReplyMessage r = ask(client.spend(Skill::overcharge));
    EXPECT_TRUE(!r.done);
    EXPECT_TRUE(r.reason == "Needs 5 points in Deep Vein");
    EXPECT_TRUE(ask(client.spend(Skill::quickDrill)).done);
    ASSERT_TRUE(until([&] { return client.progress()->ranks.count(Skill::quickDrill) == 1; }, 2));
    EXPECT_EQ(client.progress()->points, int64_t(8));
    ASSERT_TRUE(client.progress()->undo.size() == 1);
    EXPECT_TRUE(client.progress()->undo[0].skill == Skill::quickDrill);
    r = ask(client.slot(1, Skill::quickDrill));
    EXPECT_TRUE(r.reason == "Slot C opens at level 25");
    EXPECT_TRUE(ask(client.undo(Skill::quickDrill)).done);
    ASSERT_TRUE(until([&] { return client.progress()->ranks.empty(); }, 2));
    r = ask(client.respec());
    EXPECT_TRUE(r.reason.rfind("Respec at your Citadel", 0) == 0);
    r = ask(client.pick(UnitKind::prospector, Perk::prospectorQuickDrill));
    EXPECT_TRUE(r.reason == kWorldPickRefusal);
}
