// The Mule in a world (C4, WorldMule.cpp, docs/haul-and-ferry.md section 2):
// bought at a realm Foundry and rolled out of its door 20 s later, one a
// pilot; boarded and left by its owner alone; filled at its hopper with no
// XP; driven into the Citadel's hall it books its load to the driver's
// wallet with the unload's XP and the realm's cut; across a region edge with
// its rider; destroyed, forgotten; saved and loaded with its load. Isolated
// scenes of a few seconds; run from unreal/server.

#include "test.h"

#include "Mule.h"
#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId a = 91;
const std::string keyA = "e0e1e2e3e4e5e6e7e8e9eaebecedeeef";
constexpr PilotId b = 92;
const std::string keyB = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

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
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.brood = false;
    return p;
}

Simulation& simOf(WorldMap& w, PilotId p) { return w.regionOf(p)->simulation(); }

Unit* unitOf(WorldMap& w, PilotId p, int64_t id) {
    Simulation& sim = simOf(w, p);
    const auto i = sim.unitIndex(id);
    return i ? &sim.state.units[size_t(*i)] : nullptr;
}

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    return id ? unitOf(w, p, *id) : nullptr;
}

const Structure* structure(const Simulation& sim, int64_t id) {
    for (const Structure& s : sim.state.structures)
        if (s.id == id) return &s;
    return nullptr;
}

int64_t inputTick = 0;

/// `seconds` of steps with the pilot's input: its walk, its heading.
void drive(WorldMap& w, PilotId p, double seconds, Vec2 walk = Vec2(0, 0), std::optional<double> heading = std::nullopt) {
    const int ticks = int(std::llround(seconds / w.start().description().step));
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.walk = walk;
        if (const Unit* u = drivenUnit(w, p)) in.heading = heading.value_or(u->heading);
        w.input(p, in);
        w.step();
    }
}

/// The wallet where the pilot is (the side's bank), told to it.
void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

/// The id a dev line said after `head`.
std::optional<int64_t> said(const std::string& line, const std::string& head) {
    std::printf("  %s\n", line.c_str());
    if (line.rfind(head, 0) != 0) return std::nullopt;
    return std::atoll(line.c_str() + head.size());
}

/// A world with pilot A seated, the realm's Citadel (door open) ahead of
/// it, and A's Mule out beside its Prospector.
struct Yard {
    Yard() : w(first(), params()) {
        w.seat(a, keyA);
        w.step();
        citadel = said(w.devCitadel("citadel here"), "dev: citadel here: citadel ");
        if (citadel) {
            for (Structure& s : simOf(w, a).state.structures)
                if (s.id == *citadel) s.door = Door{1, true};
            centre = structure(simOf(w, a), *citadel)->position;
        }
        mule = said(w.devMule("mule here"), "dev: mule here: mule ");
        w.step();
    }
    WorldMap w;
    std::optional<int64_t> citadel, mule;
    Vec2 centre{0, 0};
};

std::string freshFile(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

} // namespace

/// Bought at a finished realm Foundry for 600 ore and 150 MH, it rolls out
/// of the door 20 s later; one a pilot; refused with too little in the wallet
/// or no Foundry.
TEST(mule_boughtAtTheFoundryRollsOutIn20s) {
    WorldMap w(first(), params());
    w.seat(a, keyA);
    w.step();
    const auto foundry = said(w.devMule("mule foundry"), "dev: mule foundry: foundry ");
    ASSERT_TRUE(foundry.has_value());
    fill(w, a, 500, 150);
    EXPECT_TRUE(w.buyMule(a, *foundry).value_or("") == "Not enough ore: you have 500");
    fill(w, a, 1000, 100);
    EXPECT_TRUE(w.buyMule(a, *foundry).value_or("") == "Not enough MH: you have 100");
    fill(w, a, 1000, 300);
    EXPECT_TRUE(w.buyMule(a, *foundry + 1000).value_or("") == "Buy the Mule at a finished Foundry");
    EXPECT_TRUE(!w.buyMule(a, *foundry));
    EXPECT_EQ(w.wallet(a)->ore, int64_t(400));
    EXPECT_EQ(w.wallet(a)->hydrogen, int64_t(150));
    const double bought = w.clock();
    auto m = w.progressMessage(a)->mule;
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->unit, int64_t(0));
    EXPECT_EQ(m->foundry, *foundry);
    EXPECT_NEAR(m->readyAt, bought + Mule::rollOut(), 1e-9);
    EXPECT_TRUE(w.buyMule(a, *foundry).value_or("") == "Your Mule is rolling out");
    drive(w, a, Mule::rollOut() - 0.5);
    EXPECT_TRUE(!w.mule(a).has_value());
    drive(w, a, 0.6);
    const auto out = w.mule(a);
    ASSERT_TRUE(out.has_value());
    const Unit* u = unitOf(w, a, out->second);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(u->kind == UnitKind::mule && u->haul == Haul{});
    EXPECT_TRUE(distance(u->position, Mule::door(structure(simOf(w, a), *foundry)->position)) < 1.0);
    m = w.progressMessage(a)->mule;
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->unit, out->second);
    EXPECT_EQ(m->readyAt, 0.0);
    EXPECT_TRUE(w.buyMule(a, *foundry).value_or("") == "You have a Mule already");
}

/// Its owner climbs on at the perch and drives it, and climbs off again
/// (Take on the Mule, Take on its Prospector, HandBack); another pilot can
/// neither board nor fill it.
TEST(mule_onlyItsOwnerBoardsIt) {
    Yard y;
    ASSERT_TRUE(y.mule.has_value());
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    Unit* m = unitOf(w, a, *y.mule);
    unitOf(w, a, prospector)->position = Mule::perch(*m) + Vec2(0, Mule::reach + 1);
    EXPECT_TRUE(w.take(a, *y.mule).value_or("") == "Get to the Mule's perch");
    unitOf(w, a, prospector)->position = Mule::perch(*m) + Vec2(0, 1);
    EXPECT_TRUE(!w.take(a, *y.mule));
    EXPECT_TRUE(w.drivesMule(a));
    EXPECT_TRUE(simOf(w, a).ridden(*unitOf(w, a, prospector)) == *y.mule);
    // Pilot B, on foot by the perch and the hopper: not its Mule.
    w.seat(b, keyB);
    w.step();
    ASSERT_TRUE(w.where(b) == w.where(a));
    const int64_t pb = *w.regionOf(b)->driving(b);
    unitOf(w, b, pb)->position = Mule::perch(*unitOf(w, a, *y.mule)) + Vec2(0, -1);
    EXPECT_TRUE(w.take(b, *y.mule).value_or("") == "Not your Mule");
    unitOf(w, b, pb)->carrying = 10;
    unitOf(w, b, pb)->position = Mule::hopper(*unitOf(w, a, *y.mule));
    EXPECT_TRUE(w.muleFill(b, *y.mule).value_or("") == "Not your Mule");
    // Off by HandBack: its Prospector on foot beside it, driven again.
    EXPECT_TRUE(!w.handBack(a));
    EXPECT_TRUE(!w.drivesMule(a));
    EXPECT_EQ(*w.regionOf(a)->driving(a), prospector);
    EXPECT_TRUE(unitOf(w, a, prospector)->task == Unit::Task::idle);
    // On and off again by Take.
    EXPECT_TRUE(!w.take(a, *y.mule));
    EXPECT_TRUE(!w.take(a, prospector));
    EXPECT_EQ(*w.regionOf(a)->driving(a), prospector);
    EXPECT_TRUE(!w.drivesMule(a));
}

/// Filled at the hopper (no wallet, no XP), driven through the open door
/// into the hall: the whole load to the driver's wallet less the realm's cut,
/// the unload's XP on all of it (1 an ore, 2 an MH), the cut in the realm
/// wallet and the pilot's tax, the Mule empty.
TEST(mule_fillsAndBooksInTheHall) {
    Yard y;
    ASSERT_TRUE(y.mule && y.citadel);
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    w.setProgress(a, [] {
        ProspectorProgress g;
        g.xp = Progression::xp(5);
        return g;
    }());
    // The Mule outside the door, facing in; the Prospector at its hopper.
    Unit* m = unitOf(w, a, *y.mule);
    m->position = y.centre + Vec2(0, 9.5);
    m->heading = -pi / 2;
    unitOf(w, a, prospector)->position = Mule::hopper(*m) + Vec2(1.0, 0);
    const auto before = *w.wallet(a);
    const double xp = w.progress(a)->xp;
    unitOf(w, a, prospector)->carrying = 25;
    EXPECT_TRUE(!w.muleFill(a, *y.mule));
    unitOf(w, a, prospector)->carrying = 20;
    unitOf(w, a, prospector)->hydrogen = true;
    EXPECT_TRUE(!w.muleFill(a, *y.mule));
    w.step();
    EXPECT_TRUE(unitOf(w, a, *y.mule)->haul == (Haul{25, 20}));
    EXPECT_EQ(w.wallet(a)->ore, before.ore);
    EXPECT_EQ(w.wallet(a)->hydrogen, before.hydrogen);
    EXPECT_NEAR(w.progress(a)->xp, xp, 1e-9);
    EXPECT_TRUE(w.muleFill(a, *y.mule).value_or("") == "Nothing to put in");
    // On, and in through the door.
    unitOf(w, a, prospector)->position = Mule::perch(*unitOf(w, a, *y.mule)) + Vec2(1.0, 0);
    ASSERT_TRUE(!w.take(a, *y.mule));
    const auto realm = w.realmWallet();
    drive(w, a, 7, Vec2(1, 0), -pi / 2);
    const Unit* in = unitOf(w, a, *y.mule);
    std::printf("  the Mule at %.1f cells from the hall's middle\n", distance(in->position, y.centre));
    EXPECT_TRUE(Rules::footprint(StructureKind::citadel).inside(in->position - y.centre));
    EXPECT_TRUE(in->haul == Haul{});
    const int64_t taxOre = Progression::realmTax(25), taxMh = Progression::realmTax(20);
    EXPECT_EQ(w.wallet(a)->ore, before.ore + 25 - taxOre);
    EXPECT_EQ(w.wallet(a)->hydrogen, before.hydrogen + 20 - taxMh);
    EXPECT_NEAR(w.progress(a)->xp - xp, 25 * WorldXp::ore + 20 * WorldXp::hydrogen + WorldXp::feat(WorldXp::hydrogenFeat), 1e-6);
    EXPECT_EQ(w.realmWallet().ore, realm.ore + taxOre);
    EXPECT_EQ(w.realmWallet().hydrogen, realm.hydrogen + taxMh);
    EXPECT_EQ(w.taxed(a)->ore, taxOre);
}

/// The flow's staging (`mule door`): the Mule out of the Citadel's door
/// facing in, the Prospector on foot within reach of its hopper; driven
/// straight ahead it goes in through the open door.
TEST(mule_doorLineStagesTheDrive) {
    Yard y;
    ASSERT_TRUE(y.mule && y.citadel);
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    EXPECT_TRUE(w.devMule("mule door").rfind("dev: mule door: mule ", 0) == 0);
    const Unit* m = unitOf(w, a, *y.mule);
    EXPECT_TRUE(distance(m->position, y.centre + Vec2(0, 9.5)) < 1e-9);
    EXPECT_TRUE(distance(unitOf(w, a, prospector)->position, Mule::hopper(*m)) < Mule::reach);
    unitOf(w, a, prospector)->position = Mule::perch(*m) + Vec2(1.0, 0);
    ASSERT_TRUE(!w.take(a, *y.mule));
    w.devMule("mule load 50 0");
    drive(w, a, 8, Vec2(1, 0), -pi / 2);
    EXPECT_TRUE(Rules::footprint(StructureKind::citadel).inside(unitOf(w, a, *y.mule)->position - y.centre));
    EXPECT_TRUE(unitOf(w, a, *y.mule)->haul == Haul{});
}

/// At or over 1000 nothing more goes in.
TEST(mule_fullRefuses) {
    Yard y;
    ASSERT_TRUE(y.mule.has_value());
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    w.devMule("mule load 1000 0");
    unitOf(w, a, prospector)->position = Mule::hopper(*unitOf(w, a, *y.mule));
    unitOf(w, a, prospector)->carrying = 25;
    EXPECT_TRUE(w.muleFill(a, *y.mule).value_or("") == Mule::full);
    EXPECT_EQ(unitOf(w, a, prospector)->carrying, int64_t(25));
}

/// Driven out of the start region's east passage, it crosses with its
/// rider and its load, and the pilot drives it on there.
TEST(mule_crossesAnEdgeWithItsRider) {
    Yard y;
    ASSERT_TRUE(y.mule.has_value());
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    const region::Built& places = *w.start().places();
    const auto& span = places.openings[size_t(region::Edge::east)][0];
    Unit* m = unitOf(w, a, *y.mule);
    m->position = places.ground(Vec2(places.extent().x - 2.5, (span.from + span.to) / 2));
    m->heading = 0;
    m->haul = Haul{300, 12};
    unitOf(w, a, prospector)->position = Mule::perch(*m);
    ASSERT_TRUE(!w.take(a, *y.mule));
    const size_t from = *w.where(a);
    drive(w, a, 5, Vec2(1, 0), 0.0);
    ASSERT_TRUE(*w.where(a) != from);
    EXPECT_TRUE(w.loaded()->regions[*w.where(a)].file.id == "tern-ridge");
    ASSERT_TRUE(w.drivesMule(a));
    const auto mine = w.mule(a);
    ASSERT_TRUE(mine.has_value());
    EXPECT_EQ(mine->first, *w.where(a));
    const Unit* there = unitOf(w, a, mine->second);
    ASSERT_TRUE(there != nullptr);
    EXPECT_TRUE(there->haul == (Haul{300, 12}));
    const auto rider = simOf(w, a).muleRider(mine->second);
    ASSERT_TRUE(rider.has_value());
    EXPECT_TRUE(unitOf(w, a, *rider)->task == Unit::Task::aboard);
    // Nothing of its own left behind.
    EXPECT_TRUE(w.region(from).simulation().state.units.empty());
    // It drives on there; off, its Prospector on foot.
    const Vec2 at = there->position;
    drive(w, a, 1, Vec2(1, 0), 0.0);
    EXPECT_TRUE(distance(unitOf(w, a, mine->second)->position, at) > 0.5);
    EXPECT_TRUE(!w.handBack(a));
    EXPECT_EQ(*w.regionOf(a)->driving(a), *rider);
}

/// Destroyed: its load gone, its rider driven again on foot; a new one may
/// be bought.
TEST(mule_destroyedIsForgotten) {
    Yard y;
    ASSERT_TRUE(y.mule.has_value());
    WorldMap& w = y.w;
    const int64_t prospector = *w.regionOf(a)->driving(a);
    unitOf(w, a, prospector)->position = Mule::perch(*unitOf(w, a, *y.mule));
    ASSERT_TRUE(!w.take(a, *y.mule));
    unitOf(w, a, *y.mule)->haul = Haul{500, 0};
    unitOf(w, a, *y.mule)->hp = 0;
    w.step();
    w.step();
    EXPECT_TRUE(!w.mule(a).has_value());
    EXPECT_TRUE(!w.progressMessage(a)->mule.has_value());
    ASSERT_TRUE(w.regionOf(a)->driving(a).has_value());
    EXPECT_EQ(*w.regionOf(a)->driving(a), prospector);
    EXPECT_TRUE(unitOf(w, a, prospector)->task == Unit::Task::idle);
    EXPECT_TRUE(w.devMule("mule here").rfind("dev: mule here: mule ", 0) == 0);
}

/// Saved and loaded: the Mule, its load, its owner; a rider is saved on its
/// feet. Off line, the Mule stays where it stands.
TEST(mule_savedWithItsLoad) {
    const std::string path = freshFile("mule");
    std::string why;
    int64_t mule = 0;
    {
        Yard y;
        ASSERT_TRUE(y.mule.has_value());
        mule = *y.mule;
        WorldMap& w = y.w;
        const int64_t prospector = *w.regionOf(a)->driving(a);
        unitOf(w, a, mule)->haul = Haul{640, 85};
        unitOf(w, a, prospector)->position = Mule::perch(*unitOf(w, a, mule));
        ASSERT_TRUE(!w.take(a, mule));
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        const WorldSave s = w.save();
        ASSERT_TRUE(store->write(s, &why).has_value());
        bool saved = false;
        for (const SavedRegion& r : s.regions)
            for (const SavedUnit& u : r.units)
                if (u.id == mule) {
                    saved = true;
                    EXPECT_TRUE(u.kind == UnitKind::mule && u.owner == keyA && u.haulOre == 640 && u.haulHydrogen == 85);
                }
        EXPECT_TRUE(saved);
        // Off line: it climbs off, the Mule stays.
        w.leave(a);
        w.step();
        EXPECT_TRUE(w.region(*w.regionNamed("start")).simulation().state.unit(mule).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    w.step();
    const auto mine = w.mule(a);
    ASSERT_TRUE(mine.has_value());
    EXPECT_EQ(mine->second, mule);
    const Unit* m = unitOf(w, a, mule);
    ASSERT_TRUE(m != nullptr);
    EXPECT_TRUE(m->kind == UnitKind::mule && m->haul == (Haul{640, 85}));
    // Its Prospector on its feet, driven, beside the perch.
    const Unit* p = drivenUnit(w, a);
    ASSERT_TRUE(p != nullptr && p->kind == UnitKind::prospector);
    EXPECT_TRUE(p->task != Unit::Task::aboard);
    EXPECT_TRUE(distance(p->position, Mule::perch(*m)) < Mule::reach);
}

/// A Mule still rolling out at the save is saved out of its door, paid.
TEST(mule_rollingOutIsSavedOut) {
    const std::string path = freshFile("mule-rolling");
    std::string why;
    {
        WorldMap w(first(), params());
        w.seat(a, keyA);
        w.step();
        const auto foundry = said(w.devMule("mule foundry"), "dev: mule foundry: foundry ");
        ASSERT_TRUE(foundry.has_value());
        fill(w, a, 1000, 300);
        ASSERT_TRUE(!w.buyMule(a, *foundry));
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    w.step();
    ASSERT_TRUE(w.mule(a).has_value());
    EXPECT_EQ(w.wallet(a)->ore, int64_t(400));
    EXPECT_TRUE(unitOf(w, a, w.mule(a)->second)->kind == UnitKind::mule);
}
