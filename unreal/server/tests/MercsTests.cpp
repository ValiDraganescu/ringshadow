// Mercenary prospectors (H2, docs/prospector-progression.md "Mercenaries"):
// hired at a Citadel for a fee that rises with each one held, up
// to 1, 2 or 3 by level; a wage in MH at each world dawn, and a mercenary
// the wallet cannot pay leaves; in a day each delivers what its pilot mines
// in an hour, taxed, from a field that really empties, live, idle and
// caught up; a downed one is back at its Citadel and resumes after ten
// world minutes; all of it saved. Isolated scenes on The First; files in
// build/save-tests. Run from unreal/server.

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

constexpr PilotId a = 93;
const std::string keyA = "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// No contracts or missions (they pay into the wallet), no bugs, no
/// starfalls (a crater's deposit is not a field's).
WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.missions = false;
    p.brood = false;
    p.starfalls = false;
    return p;
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

void setLevel(WorldMap& w, PilotId p, int64_t level) {
    ProspectorProgress g = *w.progress(p);
    g.xp = Progression::xp(level);
    w.setProgress(p, g);
}

/// Pilot A at `level` with its Citadel welded east of the start region's
/// ore, a full wallet, standing where it unloads.
struct Mercs1 {
    explicit Mercs1(int64_t level = 10, WorldParams p = params()) : w(first(), p) {
        w.seat(a, keyA);
        ProspectorProgress g;
        g.xp = Progression::xp(level);
        g.giveBlueprint(StructureKind::citadel);
        w.setProgress(a, g);
        fill(w, a, 20000, 3000);
        citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, w.regionOf(a)->places()->ground(Vec2(72, 48))));
        if (citadel) besides(w, a, *citadel);
    }
    WorldMap w;
    std::optional<int64_t> citadel;
};

const Unit* unitOf(WorldMap& w, const WorldMap::MercView& m) {
    if (!m.unit) return nullptr;
    Simulation& sim = w.region(m.region).simulation();
    const auto i = sim.unitIndex(*m.unit);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

/// Gross ore delivered: what reached the wallet and the realm's tax on it.
int64_t gross(int64_t net) {
    // Loads of 10 pay 1: 9 of each 10 reach the wallet (a part load at the
    // end pays its own rounding; the tests count whole loads).
    return net / 9 * 10 + net % 9;
}

std::string savePath(const std::string& name) {
    ::mkdir("build", 0755);
    ::mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

// The limits by level, the fee doubling with each one held, the wage.
TEST(mercs_rules) {
    EXPECT_EQ(WorldMercs::limit(1), int64_t(0));
    EXPECT_EQ(WorldMercs::limit(4), int64_t(0));
    EXPECT_EQ(WorldMercs::limit(5), int64_t(1));
    EXPECT_EQ(WorldMercs::limit(14), int64_t(1));
    EXPECT_EQ(WorldMercs::limit(15), int64_t(2));
    EXPECT_EQ(WorldMercs::limit(29), int64_t(2));
    EXPECT_EQ(WorldMercs::limit(30), int64_t(3));
    EXPECT_EQ(WorldMercs::limit(100), int64_t(3));
    EXPECT_EQ(WorldMercs::nextLevel(0), int64_t(5));
    EXPECT_EQ(WorldMercs::nextLevel(1), int64_t(15));
    EXPECT_EQ(WorldMercs::nextLevel(2), int64_t(30));
    EXPECT_EQ(WorldMercs::nextLevel(3), int64_t(0));
    EXPECT_EQ(WorldMercs::feeOre(0), int64_t(500));
    EXPECT_EQ(WorldMercs::feeOre(1), int64_t(1000));
    EXPECT_EQ(WorldMercs::feeOre(2), int64_t(2000));
    EXPECT_EQ(WorldMercs::feeHydrogen(0), int64_t(50));
    EXPECT_EQ(WorldMercs::feeHydrogen(2), int64_t(200));
    EXPECT_EQ(WorldMercs::wage(5), int64_t(6));
    EXPECT_EQ(WorldMercs::wage(30), int64_t(11));
    // A day of a mercenary is an hour of its pilot's (the pace model).
    for (int64_t level : {5, 20, 50, 100})
        EXPECT_NEAR(WorldMercs::rate(level) * 24 * 3600, WorldContracts::perMinute(level) * 60, 1e-6);
    EXPECT_NEAR(WorldMercs::rate(5) * 3600, 4500 * 1.08 / 24, 1e-6);
}

// Hired at the Citadel: the fee from the wallet, a Prospector of the realm
// beside the Citadel bound for the nearest field; refused below level 5,
// at the limit (and told when the next place opens), short of the fee, away
// from the Citadel. The fee doubles with each one held.
TEST(mercs_hire_limits_fees_and_refusals) {
    Mercs1 s(4);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    auto why = w.hireMerc(a);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Mercenaries sign on with pilots of level 5 and up");
    setLevel(w, a, 12);
    fill(w, a, 400, 3000);
    why = w.hireMerc(a);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Not enough ore: 500, you have 400");
    fill(w, a, 20000, 3000);
    const auto before = *w.wallet(a);
    EXPECT_TRUE(!w.hireMerc(a).has_value());
    EXPECT_EQ(before.ore - w.wallet(a)->ore, int64_t(500));
    EXPECT_EQ(before.hydrogen - w.wallet(a)->hydrogen, int64_t(50));
    auto mercs = w.mercs(a);
    ASSERT_TRUE(mercs.size() == size_t(1));
    const Unit* u = unitOf(w, mercs[0]);
    ASSERT_TRUE(u != nullptr);
    const Structure c = *structure(simOf(w, a), *s.citadel);
    EXPECT_TRUE(u->kind == UnitKind::prospector && u->owner == Pilot::player);
    EXPECT_TRUE(distance(u->position, c.position) < Rules::radius(StructureKind::citadel) + 4);
    EXPECT_TRUE(!mercs[0].name.empty());
    // Its field is the nearest with ore to the Citadel.
    for (size_t f = 0; f < w.fields(mercs[0].region).size(); f++) {
        Vec2 at(0, 0);
        int64_t ore = 0;
        for (size_t k : w.fields(mercs[0].region)[f]) {
            at = at + w.region(mercs[0].region).simulation().state.patches[k].position;
            ore += w.region(mercs[0].region).simulation().state.patches[k].remaining;
        }
        at = at * (1.0 / double(w.fields(mercs[0].region)[f].size()));
        Vec2 mine(0, 0);
        for (size_t k : w.fields(mercs[0].region)[size_t(mercs[0].field)])
            mine = mine + w.region(mercs[0].region).simulation().state.patches[k].position;
        mine = mine * (1.0 / double(w.fields(mercs[0].region)[size_t(mercs[0].field)].size()));
        if (ore > 0) EXPECT_TRUE(distance(mine, c.position) <= distance(at, c.position) + 1e-9);
    }
    // At the limit at level 12.
    why = w.hireMerc(a);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "You hold 1 mercenary, the most at level 12: 2 at level 15");
    setLevel(w, a, 15);
    auto mid = *w.wallet(a);
    EXPECT_TRUE(!w.hireMerc(a).has_value());
    EXPECT_EQ(mid.ore - w.wallet(a)->ore, int64_t(1000));
    EXPECT_EQ(mid.hydrogen - w.wallet(a)->hydrogen, int64_t(100));
    setLevel(w, a, 30);
    mid = *w.wallet(a);
    EXPECT_TRUE(!w.hireMerc(a).has_value());
    EXPECT_EQ(mid.ore - w.wallet(a)->ore, int64_t(2000));
    why = w.hireMerc(a);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "You hold 3 mercenaries, the most at level 30");
    mercs = w.mercs(a);
    ASSERT_TRUE(mercs.size() == size_t(3));
    EXPECT_TRUE(mercs[0].name != mercs[1].name && mercs[1].name != mercs[2].name && mercs[0].name != mercs[2].name);
    // Dismissed: gone with its unit, no refund; a place again.
    const auto kept = *w.wallet(a);
    const int64_t unit = *mercs[1].unit;
    EXPECT_TRUE(!w.dismissMerc(a, mercs[1].id).has_value());
    EXPECT_TRUE(*w.wallet(a) == kept);
    EXPECT_EQ(w.mercs(a).size(), size_t(2));
    EXPECT_TRUE(!w.region(mercs[1].region).simulation().unitIndex(unit).has_value());
    EXPECT_TRUE(w.dismissMerc(a, 99).has_value());
    // Away from the Citadel.
    drivenUnit(w, a)->position = drivenUnit(w, a)->position + Vec2(0, 20);
    w.step();
    why = w.hireMerc(a);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Hire at a Citadel: stand where you unload");
    // The client is told.
    const auto m = *w.progressMessage(a);
    EXPECT_EQ(m.mercs.size(), size_t(2));
    EXPECT_EQ(m.mercLimit, int64_t(3));
    EXPECT_TRUE(m.mercFee.ore == 2000 && m.mercFee.hydrogen == 200);
    EXPECT_EQ(m.mercWage, int64_t(11));
    EXPECT_TRUE(!m.mercFields.empty());
    EXPECT_EQ(m.mercTags.size(), size_t(2));
    EXPECT_TRUE(m.mercTags.at(*mercs[0].unit) == mercs[0].name);
}

// Live: the mercenary walks to its field, drills, hauls to the Citadel; the
// ore it has worked is exactly the rate × the time, and what it unloads
// reaches the pilot's wallet less the realm's tax, with no XP; the field
// loses as much.
/// The ore counted to the unit: no regrowth (E1).
static WorldParams still() {
    WorldParams p = params();
    p.veinRegrow = 0;
    return p;
}

TEST(mercs_live_work_paced_and_taxed) {
    Mercs1 s(100, still());
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    const auto m0 = w.mercs(a)[0];
    const int64_t fieldBefore = [&] {
        int64_t n = 0;
        for (size_t k : w.fields(m0.region)[size_t(m0.field)]) n += w.region(m0.region).simulation().state.patches[k].remaining;
        return n;
    }();
    const auto wallet0 = *w.wallet(a);
    const double xp0 = w.progress(a)->xp;
    const auto realm0 = w.realmWallet();
    const int64_t taxed0 = w.taxed(a)->ore;
    // The pilot stands still at the Citadel: 6 world minutes.
    const double t0 = w.clock();
    bool mined = false, hauled = false;
    for (int i = 0; i < 6 * 60 * 60; i++) {
        wait(w, a, 1);
        if (const Unit* u = unitOf(w, w.mercs(a)[0])) {
            mined = mined || u->task == Unit::Task::mining;
            hauled = hauled || (u->task == Unit::Task::toBase && u->carrying > 0);
        }
    }
    const double t = w.clock() - t0;
    const auto m = w.mercs(a)[0];
    const Unit* u = unitOf(w, m);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(mined && hauled);
    const int64_t net = w.wallet(a)->ore - wallet0.ore;
    const int64_t tax = w.realmWallet().ore - realm0.ore;
    EXPECT_EQ(net, m.delivered);
    EXPECT_EQ(w.taxed(a)->ore - taxed0, tax);
    // The tax is the realm's 10% a load of 10.
    EXPECT_EQ(net, tax * 9);
    EXPECT_TRUE(tax >= 3);
    // Worked: unloaded, in hand, owed; all of it the rate × the time.
    EXPECT_NEAR(double(net + tax + u->carrying) + m.owed, WorldMercs::rate(100) * t, 0.05);
    // The field gave what it unloaded and what it carries.
    int64_t fieldAfter = 0;
    for (size_t k : w.fields(m.region)[size_t(m.field)]) fieldAfter += w.region(m.region).simulation().state.patches[k].remaining;
    EXPECT_EQ(fieldBefore - fieldAfter, net + tax + u->carrying);
    // No XP for the pilot.
    EXPECT_NEAR(w.progress(a)->xp, xp0, 1e-9);
    std::printf("  6 min at level 100: %lld delivered, %lld tax, %lld in hand, %.2f owed\n", (long long)net, (long long)tax,
                (long long)u->carrying, m.owed);
}

// A world day at a time with the regions held (S2's idle) and the rest of a
// day caught up after a stop: the mercenary delivers what its pilot mines in
// an hour at its level (the pace model), taxed, and its field gives as
// much.
TEST(mercs_a_day_is_the_pilots_hour) {
    Mercs1 s(5, still());
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    fill(w, a, 0, 3000);
    const auto m0 = w.mercs(a)[0];
    // The pace model alone: a field that does not run dry in the day (E1's
    // veins would: then the mercenary waits, `mercs_wait_at_a_dry_field`),
    // counted to the unit with no regrowth.
    Simulation& sim = w.region(m0.region).simulation();
    for (size_t k : w.fields(m0.region)[size_t(m0.field)]) sim.state.patches[k].initial = sim.state.patches[k].remaining = 4000;
    auto fieldOre = [&] {
        int64_t n = 0;
        for (size_t k : w.fields(m0.region)[size_t(m0.field)]) n += w.region(m0.region).simulation().state.patches[k].remaining;
        return n;
    };
    const int64_t before = fieldOre();
    const auto realm0 = w.realmWallet();
    const int64_t hand0 = unitOf(w, m0)->carrying;
    const double owed0 = w.mercs(a)[0].owed;
    // Two hours idle, half a second at a time, then 22 hours caught up.
    for (int i = 0; i < 2 * 3600 * 2; i++) w.idle(0.5);
    EXPECT_NEAR(w.catchUp(22 * 3600), 22 * 3600, 1e-9);
    const auto m = w.mercs(a)[0];
    const int64_t net = w.wallet(a)->ore;
    const int64_t tax = w.realmWallet().ore - realm0.ore;
    const double hour = WorldContracts::perMinute(5) * 60;
    EXPECT_NEAR(double(net + tax) + m.owed - owed0 + double(unitOf(w, m)->carrying - hand0), hour, 0.05);
    EXPECT_TRUE(std::abs(double(net + tax) - hour) < double(WorldMercs::load));
    EXPECT_EQ(before - fieldOre(), net + tax);
    EXPECT_EQ(m.delivered, net);
    std::printf("  a day at level 5: %lld delivered + %lld tax (the pilot's hour: %.0f), field %lld -> %lld\n", (long long)net,
                (long long)tax, hour, (long long)before, (long long)fieldOre());
}

// The wage at each dawn from the wallet's MH; at a dawn the wallet cannot
// pay, the mercenary leaves (its unit with it) and the pilot is told.
TEST(mercs_wage_at_dawn_and_leaving_unpaid) {
    Mercs1 s(10);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    const int64_t wage = WorldMercs::wage(10);
    fill(w, a, 0, wage + 3);
    (void)w.takeNews(a);
    // Up to just before the next dawn: nothing taken.
    const double dawn = w.nextDawn(w.clock());
    while (w.clock() < dawn - 1) w.idle(std::min(0.5, dawn - 1 - w.clock()));
    EXPECT_EQ(w.wallet(a)->hydrogen, wage + 3);
    w.idle(2);
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(3));
    EXPECT_EQ(w.mercs(a).size(), size_t(1));
    // The next dawn it cannot be paid.
    const auto m = w.mercs(a)[0];
    const double next = w.nextDawn(w.clock());
    while (w.clock() < next + 1) w.idle(0.5);
    EXPECT_EQ(w.mercs(a).size(), size_t(0));
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(3));
    EXPECT_TRUE(!w.region(m.region).simulation().unitIndex(*m.unit).has_value());
    bool told = false;
    for (const auto& n : w.takeNews(a))
        told = told || (n.kind == net::ProgressMessage::NewsKind::merc && n.id == "left" && n.by == m.name && n.value == wage);
    EXPECT_TRUE(told);
}

// Downed by the bugs: back at its Citadel at once with its load, idle for
// ten world minutes (no work counted), then off to its field again.
TEST(mercs_downed_walks_back_and_resumes) {
    Mercs1 s(20);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    // Out at its field first.
    for (int i = 0; i < 30 * 60; i++) wait(w, a, 1);
    auto m = w.mercs(a)[0];
    {
        Simulation& sim = w.region(m.region).simulation();
        Unit& u = sim.state.units[size_t(*sim.unitIndex(*m.unit))];
        EXPECT_TRUE(distance(u.position, structure(sim, *s.citadel)->position) > 6);
        // Hauling a load home when it falls.
        for (auto& d : sim.state.patches)
            if (d.miner == u.id) d.miner = std::nullopt;
        u.carrying = 7;
        u.task = Unit::Task::toBase;
    }
    wait(w, a, 1);
    const int64_t delivered = w.mercs(a)[0].delivered;
    {
        Simulation& sim = w.region(m.region).simulation();
        sim.state.units[size_t(*sim.unitIndex(*m.unit))].hp = 0;
    }
    (void)w.takeNews(a);
    wait(w, a, 3);
    m = w.mercs(a)[0];
    const Unit* back = unitOf(w, m);
    ASSERT_TRUE(back != nullptr);
    EXPECT_TRUE(distance(back->position, structure(simOf(w, a), *s.citadel)->position) < Rules::radius(StructureKind::citadel) + 4);
    // Its load kept: in hand, or unloaded at the Citadel already (7 less the
    // realm's 1).
    EXPECT_TRUE(back->carrying == 7 || w.mercs(a)[0].delivered - delivered == 6);
    EXPECT_TRUE(m.downUntil > w.clock() + WorldMercs::downFor - 1);
    bool told = false;
    for (const auto& n : w.takeNews(a)) told = told || (n.kind == net::ProgressMessage::NewsKind::merc && n.id == "down");
    EXPECT_TRUE(told);
    // Waiting: its load goes home, then it stands; nothing owed meanwhile.
    const double owed = w.mercs(a)[0].owed;
    for (int i = 0; i < 20 * 60; i++) wait(w, a, 1);
    EXPECT_NEAR(w.mercs(a)[0].owed, owed, 1e-9);
    EXPECT_EQ(unitOf(w, w.mercs(a)[0])->carrying, int64_t(0));
    EXPECT_TRUE(unitOf(w, w.mercs(a)[0])->task == Unit::Task::idle);
    // Ten minutes on, it works again.
    w.setClock(m.downUntil + 0.1);
    wait(w, a, 5);
    EXPECT_TRUE(unitOf(w, w.mercs(a)[0])->task == Unit::Task::toPatch);
    EXPECT_TRUE(w.mercs(a)[0].owed > owed);
}

// Sent to another field: refused where there is no realm Citadel to haul
// to and for no such field; in its region it walks over.
TEST(mercs_assigned_to_a_field) {
    Mercs1 s(10);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    const auto m = w.mercs(a)[0];
    const std::string here = w.loaded()->regions[m.region].file.id;
    EXPECT_TRUE(w.assignMerc(a, m.id, here, 999).has_value());
    EXPECT_TRUE(w.assignMerc(a, m.id, "nowhere", 0).has_value());
    // A neighbour region without a Citadel.
    for (size_t r = 0; r < w.regionCount(); r++) {
        if (r == m.region || w.fields(r).empty()) continue;
        const auto why = w.assignMerc(a, m.id, w.loaded()->regions[r].file.id, 0);
        ASSERT_TRUE(why.has_value());
        EXPECT_TRUE(why->find("No realm Citadel in ") == 0);
        break;
    }
    // Another field here.
    std::optional<int64_t> other;
    for (size_t f = 0; f < w.fields(m.region).size(); f++)
        if (int64_t(f) != m.field) {
            int64_t ore = 0;
            for (size_t k : w.fields(m.region)[f]) ore += w.region(m.region).simulation().state.patches[k].remaining;
            if (ore > 0) other = int64_t(f);
        }
    ASSERT_TRUE(other.has_value());
    EXPECT_TRUE(!w.assignMerc(a, m.id, here, *other).has_value());
    EXPECT_EQ(w.mercs(a)[0].field, *other);
    wait(w, a, 2);
    const Unit* u = unitOf(w, w.mercs(a)[0]);
    ASSERT_TRUE(u != nullptr && u->patch.has_value());
    const auto& f = w.fields(m.region)[size_t(*other)];
    EXPECT_TRUE(std::find(f.begin(), f.end(), size_t(*u->patch)) != f.end());
}

// Saved with the world: off line its mercenaries work on (live and caught
// up after a restart), the pilot's wallet gets their ore, and on its return
// it is told what they delivered while it was away.
TEST(mercs_saved_loaded_and_caught_up_off_line) {
    const std::string path = savePath("mercs");
    std::string why;
    std::vector<WorldMap::MercView> kept;
    int64_t fieldBefore = 0;
    WorldMap::Wallet wallet;
    {
        Mercs1 s(20, still());
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        ASSERT_TRUE(!w.hireMerc(a).has_value());
        fill(w, a, 1000, 3000);
        for (int i = 0; i < 60 * 60; i++) wait(w, a, 1);
        w.leave(a);
        kept = w.mercsOf(keyA);
        ASSERT_TRUE(kept.size() == size_t(1));
        wallet = w.save().pilots[0].ore == 0 ? WorldMap::Wallet{} : WorldMap::Wallet{w.save().pilots[0].ore, w.save().pilots[0].hydrogen};
        for (size_t k : w.fields(kept[0].region)[size_t(kept[0].field)])
            fieldBefore += w.region(kept[0].region).simulation().state.patches[k].remaining;
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        EXPECT_TRUE(!store->rows("SELECT mercs FROM pilots")[0][0].empty());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), still());
    EXPECT_TRUE(w.restore(*loaded).empty());
    auto back = w.mercsOf(keyA);
    ASSERT_TRUE(back.size() == size_t(1));
    EXPECT_TRUE(back[0].id == kept[0].id && back[0].name == kept[0].name && back[0].region == kept[0].region &&
                back[0].field == kept[0].field && back[0].unit == kept[0].unit && back[0].delivered == kept[0].delivered);
    EXPECT_NEAR(back[0].owed, kept[0].owed, 1e-9);
    ASSERT_TRUE(unitOf(w, back[0]) != nullptr);
    // Down two hours: caught up on the start.
    const double moved = w.catchUp(2 * 3600);
    EXPECT_NEAR(moved, 2 * 3600, 1e-9);
    back = w.mercsOf(keyA);
    const int64_t delivered = back[0].delivered - kept[0].delivered;
    EXPECT_TRUE(delivered > 0);
    int64_t fieldAfter = 0;
    for (size_t k : w.fields(back[0].region)[size_t(back[0].field)])
        fieldAfter += w.region(back[0].region).simulation().state.patches[k].remaining;
    EXPECT_EQ(fieldBefore - fieldAfter, gross(delivered));
    // Two hours: rate × 7200 within a load.
    EXPECT_TRUE(std::abs(double(gross(delivered)) - WorldMercs::rate(20) * 7200) <= double(WorldMercs::load));
    // Back on line: its wallet has the ore, less three dawns' wages, and it
    // is told.
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_EQ(w.wallet(a)->ore, wallet.ore + delivered);
    const int64_t dawns = w.contractDay(w.clock()) - w.contractDay(w.clock() - 7200);
    EXPECT_EQ(wallet.hydrogen - w.wallet(a)->hydrogen, dawns * WorldMercs::wage(20));
    bool told = false;
    for (const auto& n : w.takeNews(a))
        told = told || (n.kind == net::ProgressMessage::NewsKind::merc && n.id == "away" && n.value == delivered);
    EXPECT_TRUE(told);
    // X2: the away report, saved with the world and made on the return:
    // the deliveries by mercenary, the two hours, the realm's tax.
    const auto report = w.takeAway(a);
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report->ran >= 7200);
    ASSERT_TRUE(report->rows.size() == size_t(1));
    EXPECT_TRUE(report->rows[0].kind == net::AwayKind::merc && report->rows[0].who == kept[0].name);
    EXPECT_EQ(report->rows[0].value, delivered);
    EXPECT_TRUE(report->realmOre > 0);
    EXPECT_TRUE(!w.takeAway(a).has_value());
}

// A save from before mercenaries (schema 12) opens at 14 with none.
TEST(mercs_old_save_migrates) {
    const std::string path = savePath("mercs-v12");
    std::string why;
    std::vector<WorldStore::Migration> v12;
    for (const auto& s : WorldStore::migrations())
        if (s.version <= 12) v12.push_back(s);
    {
        auto store = WorldStore::open(path, "first", &why, v12);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(12));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress, gated, contracts, rig, stacked) "
                                "VALUES ('" + keyA + "', 40, 7, 'start', NULL, '', '', 1, '', '', 1)",
                                &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    EXPECT_TRUE(store->rows("SELECT mercs FROM pilots")[0][0].empty());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    EXPECT_TRUE(w.mercs(a).empty());
    EXPECT_EQ(w.progressMessage(a)->mercs.size(), size_t(0));
}

// E1: a mercenary whose field is mined out stands at it, unpaid (no ore
// owed), and goes back to work as its vein grows back: over a world day
// held it delivers the tenth the vein regrows, no more.
TEST(mercs_wait_at_a_dry_field) {
    {
        Mercs1 s(20, still());
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        ASSERT_TRUE(!w.hireMerc(a).has_value());
        const auto m0 = w.mercs(a)[0];
        Simulation& sim = w.region(m0.region).simulation();
        for (size_t k : w.fields(m0.region)[size_t(m0.field)]) sim.state.patches[k].remaining = 0;
        wait(w, a, 60 * 60);
        const auto m = w.mercs(a)[0];
        EXPECT_EQ(m.delivered, m0.delivered);
        EXPECT_TRUE(m.owed < 1e-9);
        const Unit* u = unitOf(w, m);
        ASSERT_TRUE(u != nullptr);
        EXPECT_TRUE(u->task != Unit::Task::mining);
    }
    Mercs1 s(20);
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ASSERT_TRUE(!w.hireMerc(a).has_value());
    const auto m0 = w.mercs(a)[0];
    Simulation& sim = w.region(m0.region).simulation();
    int64_t full = 0;
    for (size_t k : w.fields(m0.region)[size_t(m0.field)]) {
        full += sim.state.patches[k].initial;
        sim.state.patches[k].remaining = 0;
    }
    const int64_t wallet0 = w.wallet(a)->ore;
    const int64_t realm0 = w.realmWallet().ore;
    const int64_t hand0 = unitOf(w, m0)->carrying;
    for (int i = 0; i < int(WorldClock::dayLength * 2); i++) w.idle(0.5);
    const int64_t taken = (w.wallet(a)->ore - wallet0) + (w.realmWallet().ore - realm0) + (unitOf(w, w.mercs(a)[0])->carrying - hand0);
    int64_t left = 0;
    for (size_t k : w.fields(m0.region)[size_t(m0.field)]) left += sim.state.patches[k].remaining;
    std::printf("  a dry field of %lld over a world day: %lld regrown and taken, %lld left in it\n", (long long)full, (long long)taken,
                (long long)left);
    EXPECT_TRUE(taken > 0);
    EXPECT_NEAR(double(taken + left), double(full) * region::regrowPerDay, double(2 * WorldMercs::load));
}
