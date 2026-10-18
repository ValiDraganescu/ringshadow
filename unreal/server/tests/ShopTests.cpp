// The Citadel shop (C2, docs/prospector-progression.md "The Citadel shop"):
// blueprints and consumables bought from the pilot's own wallet at the
// realm's Citadel, each blueprint once a pilot at its first copy's price
// and learned once (docs/guns-and-practice.md section 4); building uses no
// copy; the four consumables used in the field, their stacks and
// cooldowns; the stacks saved. Isolated scenes of a few seconds on The First Reach; files
// in build/save-tests. Run from unreal/server.

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

constexpr PilotId a = 91;
const std::string keyA = "e0e1e2e3e4e5e6e7e8e9eaebecedeeef";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// No contracts drawn (they pay MH into the wallet).
WorldParams params() {
    WorldParams p;
    p.contracts = false;
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

/// Pilot A at level 10 knowing the Citadel, its Citadel welded east of
/// the start region's ore, a full wallet, and A standing where it unloads.
struct Shop1 {
    explicit Shop1(WorldParams p = params()) : w(first(), p) {
        w.seat(a, keyA);
        ProspectorProgress g;
        g.xp = Progression::xp(10);
        g.learned.insert(StructureKind::citadel);
        w.setProgress(a, g);
        fill(w, a, 20000, 3000);
        citadel = weld(w, a, StructureKind::citadel, rig::citadelStand(w, a, w.regionOf(a)->places()->ground(Vec2(72, 48))));
        if (citadel) besides(w, a, *citadel);
    }
    WorldMap w;
    std::optional<int64_t> citadel;
};

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

// A blueprint bought at the Citadel: the wallet pays the first copy's
// price, the pilot holds a copy; a second is refused while it holds one and
// once it has learned it, at the same price as ever; the Citadel it knows
// is refused outright.
TEST(shop_sellsABlueprintOncePerPilot) {
    Shop1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    EXPECT_EQ(w.progress(a)->copies(StructureKind::citadel), int64_t(0));
    const auto before = *w.wallet(a);
    const ShopPrice price = Shop::blueprintPrice(StructureKind::habDome, 0);
    EXPECT_TRUE(!w.buy(a, StructureKind::habDome, Item::repairKit).has_value());
    const auto after = *w.wallet(a);
    EXPECT_EQ(before.ore - after.ore, price.ore);
    EXPECT_EQ(before.hydrogen - after.hydrogen, price.hydrogen);
    EXPECT_EQ(w.progress(a)->copies(StructureKind::habDome), int64_t(1));
    // A copy held: learn it first; nothing taken.
    auto why = w.buy(a, StructureKind::habDome, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "You hold a Hab Dome blueprint: learn it (B)");
    EXPECT_TRUE(*w.wallet(a) == after);
    // Learned: never sold again.
    EXPECT_TRUE(!w.learn(a, StructureKind::habDome).has_value());
    why = w.buy(a, StructureKind::habDome, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "You know the Hab Dome");
    EXPECT_TRUE(*w.wallet(a) == after);
    EXPECT_EQ(w.progress(a)->copies(StructureKind::habDome), int64_t(0));
    why = w.buy(a, StructureKind::citadel, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "You know the Citadel");
    // Below its floor it sells too, at the first copy's price: the Sentinel.
    const auto beforeSentinel = *w.wallet(a);
    EXPECT_TRUE(!w.buy(a, StructureKind::sentinel, Item::repairKit).has_value());
    EXPECT_EQ(beforeSentinel.ore - w.wallet(a)->ore, Shop::blueprintPrice(StructureKind::sentinel, 0).ore);
    EXPECT_EQ(w.progress(a)->copies(StructureKind::sentinel), int64_t(1));
    // The tax does not apply: the realm wallet is untouched.
    EXPECT_EQ(w.realmWallet().ore, int64_t(0));
    // The client is told: copies, learned.
    const auto m = *w.progressMessage(a);
    EXPECT_EQ(m.blueprints.at(StructureKind::sentinel), int64_t(1));
    EXPECT_TRUE(!m.blueprints.count(StructureKind::habDome));
    EXPECT_TRUE(m.learned.count(StructureKind::habDome) && m.learned.count(StructureKind::citadel));
}

// Refused: away from the Citadel, the wallet short, the Garrison, a full
// stack. Nothing is taken.
TEST(shop_refusals) {
    Shop1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    auto why = w.buy(a, StructureKind::garrison, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "No Garrison blueprint in this world");
    // Five repair kits: the stack is full.
    for (int i = 0; i < 5; i++) EXPECT_TRUE(!w.buy(a, std::nullopt, Item::repairKit).has_value());
    EXPECT_EQ(w.progress(a)->held(Item::repairKit), int64_t(5));
    why = w.buy(a, std::nullopt, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Repair kit: you hold 5, the most");
    // The wallet short.
    fill(w, a, 100, 0);
    why = w.buy(a, StructureKind::habDome, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Not enough ore: 250, you have 100");
    fill(w, a, 5000, 10);
    why = w.buy(a, StructureKind::habDome, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Not enough MH: 40, you have 10");
    EXPECT_EQ(w.progress(a)->copies(StructureKind::habDome), int64_t(0));
    EXPECT_TRUE(*w.wallet(a) == (WorldMap::Wallet{5000, 10}));
    // Away from the Citadel.
    drivenUnit(w, a)->position = drivenUnit(w, a)->position + Vec2(12, 0);
    w.step();
    why = w.buy(a, std::nullopt, Item::surveyFlare);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Buy at a Citadel: stand where you unload");
    EXPECT_EQ(w.progress(a)->held(Item::surveyFlare), int64_t(0));
}

// Building uses no copy (C2): a Hab Dome learned, two built for their ore,
// no copy held; a Derrick never bought: "No blueprint".
TEST(shop_building_uses_no_copy) {
    Shop1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    ProspectorProgress g = *w.progress(a);
    g.xp = Progression::xp(8);
    w.setProgress(a, g);
    ASSERT_TRUE(!w.buy(a, StructureKind::habDome, Item::repairKit).has_value());
    ASSERT_TRUE(!w.learn(a, StructureKind::habDome).has_value());
    const Vec2 east = w.regionOf(a)->places()->ground(Vec2(72, 48));
    EXPECT_TRUE(weld(w, a, StructureKind::habDome, east + Vec2(0, 9)).has_value());
    EXPECT_TRUE(weld(w, a, StructureKind::habDome, east + Vec2(0, 18)).has_value());
    EXPECT_EQ(w.progress(a)->copies(StructureKind::habDome), int64_t(0));
    EXPECT_TRUE(w.progress(a)->knows(StructureKind::habDome));
    const auto why = w.build(a, StructureKind::derrick, drivenUnit(w, a)->position + Vec2(0, -8));
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "No blueprint: buy one at a Citadel (H)");
}

// The consumables: each bought, used in the field, its stack down by one,
// cooling down after; the repair kit mends, the canister adds MH, the
// shield takes all damage while it runs, the flare marks the sites near.
TEST(shop_consumables_use_stacks_and_cooldowns) {
    Shop1 s;
    ASSERT_TRUE(s.citadel.has_value());
    WorldMap& w = s.w;
    for (const ItemInfo& x : Shop::items()) ASSERT_TRUE(!w.buy(a, std::nullopt, x.item).has_value());
    ASSERT_TRUE(!w.buy(a, std::nullopt, Item::repairKit).has_value());
    // Repair kit: not hurt, refused; hurt, half its hp back.
    auto why = w.use(a, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Your Prospector is not hurt");
    Simulation& sim = simOf(w, a);
    const double full = sim.maxHP(*drivenUnit(w, a));
    drivenUnit(w, a)->hp = full * 0.2;
    EXPECT_TRUE(!w.use(a, Item::repairKit).has_value());
    EXPECT_NEAR(drivenUnit(w, a)->hp, full * 0.7, 1e-6);
    EXPECT_EQ(w.progress(a)->held(Item::repairKit), int64_t(1));
    why = w.use(a, Item::repairKit);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(why->find("Repair kit ready in ") == 0);
    EXPECT_TRUE(w.progressMessage(a)->itemReady.count(Item::repairKit) == 1);
    // MH canister: 25 MH into the wallet.
    const int64_t mh = w.wallet(a)->hydrogen;
    EXPECT_TRUE(!w.use(a, Item::hydrogenCanister).has_value());
    EXPECT_EQ(w.wallet(a)->hydrogen, mh + Shop::canisterHydrogen);
    EXPECT_EQ(w.progress(a)->held(Item::hydrogenCanister), int64_t(0));
    why = w.use(a, Item::hydrogenCanister);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "No MH canisters: buy them at the Citadel (H)");
    // Shield charge: no damage for 8 s, then damage again.
    EXPECT_TRUE(!w.use(a, Item::shieldCharge).has_value());
    const int64_t id = drivenUnit(w, a)->id;
    const double hp = drivenUnit(w, a)->hp;
    sim.hit(id, 5);
    EXPECT_NEAR(drivenUnit(w, a)->hp, hp, 1e-9);
    EXPECT_TRUE(w.progressMessage(a)->shieldUntil > w.clock());
    wait(w, a, int(9 * 60));
    sim.hit(id, 5);
    EXPECT_TRUE(drivenUnit(w, a)->hp < hp);
    EXPECT_NEAR(w.progressMessage(a)->shieldUntil, 0, 1e-9);
    // Survey flare: the sites within 40 cells known, the flare told.
    const size_t knownBefore = w.progress(a)->known.size();
    EXPECT_TRUE(!w.use(a, Item::surveyFlare).has_value());
    const auto m = *w.progressMessage(a);
    ASSERT_TRUE(m.flare.has_value());
    EXPECT_TRUE(m.flare->region == "start");
    EXPECT_NEAR(m.flare->radius, Shop::flareRadius, 1e-9);
    EXPECT_TRUE(m.flare->until > w.clock());
    EXPECT_TRUE(w.progress(a)->known.size() >= knownBefore);
    // Left: the second repair kit (the first's cooldown refused it).
    EXPECT_TRUE((w.progress(a)->items == std::map<Item, int64_t>{{Item::repairKit, 1}}));
}

// The stacks, the copies held and the kinds learned are kept by the save.
TEST(shop_stacks_saved_and_loaded) {
    const std::string path = savePath("shop");
    std::string why;
    ProspectorProgress kept;
    {
        Shop1 s;
        ASSERT_TRUE(s.citadel.has_value());
        WorldMap& w = s.w;
        ASSERT_TRUE(!w.buy(a, StructureKind::derrick, Item::repairKit).has_value());
        ASSERT_TRUE(!w.buy(a, StructureKind::habDome, Item::repairKit).has_value());
        ASSERT_TRUE(!w.learn(a, StructureKind::habDome).has_value());
        ASSERT_TRUE(!w.buy(a, std::nullopt, Item::shieldCharge).has_value());
        ASSERT_TRUE(!w.buy(a, std::nullopt, Item::shieldCharge).has_value());
        kept = *w.progress(a);
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(w.save(), &why).has_value());
        EXPECT_TRUE(store->rows("SELECT stacked, learned FROM pilots")[0] == (std::vector<std::string>{"1", "1"}));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(a, keyA) == WorldMap::Seated::restored);
    const ProspectorProgress g = *w.progress(a);
    // A copy bought and not learned stays a copy across the save (schema 28
    // adopts only older pilots).
    EXPECT_EQ(g.copies(StructureKind::derrick), int64_t(1));
    EXPECT_TRUE(!g.knows(StructureKind::derrick));
    EXPECT_TRUE(g.knows(StructureKind::habDome));
    EXPECT_EQ(g.held(Item::shieldCharge), int64_t(2));
    EXPECT_TRUE(g.blueprints == kept.blueprints && g.items == kept.items && g.learned == kept.learned);
}
