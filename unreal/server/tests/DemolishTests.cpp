// Demolish in a world (C3, WorldDemolish.cpp, docs/guns-and-practice.md
// "Limits per region and demolish"): a realm building in the pilot's region,
// finished or a scaffold, taken down within the repair reach (or from the
// top-down view), half its price back to the pilot's own wallet, the
// Demolished event to that pilot alone, the save and the region's limit
// count without it; the realm's last Citadel and a Citadel with someone in
// it stay. Isolated scenes on The First Reach; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 71;
constexpr PilotId other = 72;
const std::string key = "00112233445566778899aabbccddeeff";
const std::string otherKey = "ffeeddccbbaa99887766554433221100";

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
    p.devBlueprints = true;
    p.contracts = false;
    return p;
}

ProspectorProgress at(int64_t level) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    for (const StructureKind k : Progression::blueprintKinds()) g.learned.insert(k);
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

void fill(WorldMap& w, PilotId p, int64_t ore, int64_t mh) {
    simOf(w, p).state.players[0].ore = ore;
    simOf(w, p).state.players[0].hydrogen = mh;
    w.step();
}

void moveTo(WorldMap& w, PilotId p, Vec2 at) {
    drivenUnit(w, p)->position = at;
    w.step();
}

void goTo(WorldMap& w, PilotId p, const std::string& id) {
    const size_t r = *w.regionNamed(id);
    ASSERT_TRUE(w.devTransfer(p, r, first().regions[r].built.pads[0].at));
    w.step();
}

Vec2 startPad() { return first().regions[first().start].built.pads[first().startPad].at; }

/// A `kind` of the pilot's put up round its Prospector, off the pads'
/// plates; finished at once unless `scaffold`. Nil: no spot, or refused.
std::optional<int64_t> put(WorldMap& w, PilotId p, StructureKind kind, bool scaffold = false, std::string* why = nullptr) {
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
            if (!scaffold)
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

template <class E> std::vector<E> eventsOf(WorldMap& w, PilotId p) {
    std::vector<E> out;
    for (const GameEvent& e : w.snapshot(p).events)
        if (const E* x = e.as<E>()) out.push_back(*x);
    return out;
}

bool saved(const WorldMap& w, int64_t id) {
    const WorldSave s = w.save();
    for (const SavedRegion& r : s.regions)
        if (r.id == first().regions[first().start].file.id)
            for (const SavedBuilding& b : r.buildings)
                if (b.id == id) return true;
    return false;
}

} // namespace

// A finished Gatling Gun: 62 ore and 12 MH back (half of 125 and 25), gone
// at once, the Demolished event to the demolisher only (the other pilot here
// sees it shatter), out of the save, and out of the region's count.
TEST(demolish_a_finished_building_pays_half_back) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.seat(other, otherKey);
    w.setProgress(me, at(5));
    fill(w, me, 2000, 500);
    moveTo(w, me, startPad() + Vec2(0, 12));
    const auto gun = put(w, me, StructureKind::gatling);
    ASSERT_TRUE(gun.has_value());
    EXPECT_TRUE(saved(w, *gun));
    EXPECT_EQ(w.limitCount(StructureKind::gatling, first().start), int64_t(1));
    w.snapshot(me);
    w.snapshot(other);
    const auto before = *w.wallet(me);
    const auto why = w.demolish(me, *gun);
    if (why) std::printf("  refused: %s\n", why->c_str());
    ASSERT_TRUE(!why.has_value());
    EXPECT_TRUE(!simOf(w, me).state.structure(*gun).has_value());
    EXPECT_EQ(w.wallet(me)->ore - before.ore, int64_t(62));
    EXPECT_EQ(w.wallet(me)->hydrogen - before.hydrogen, int64_t(12));
    EXPECT_EQ(w.limitCount(StructureKind::gatling, first().start), int64_t(0));
    w.step();
    const auto mine = eventsOf<GameEvent::Demolished>(w, me);
    ASSERT_TRUE(mine.size() == size_t(1));
    EXPECT_TRUE(mine[0] == (GameEvent::Demolished{*gun, 62, 12}));
    // The other pilot: the shatter, not the refund.
    w.step();
    bool shattered = false, told = false;
    for (const GameEvent& e : w.snapshot(other).events) {
        if (const auto* d = e.as<GameEvent::Destroyed>(); d && d->structure == *gun) shattered = true;
        told = told || e.is<GameEvent::Demolished>();
    }
    EXPECT_TRUE(shattered);
    EXPECT_TRUE(!told);
    EXPECT_TRUE(!saved(w, *gun));
    // Gone: a second ask is refused.
    EXPECT_TRUE(w.demolish(me, *gun).has_value());
}

// A scaffold: the same half of the full price (it was paid in full when it
// was placed); its welder stops.
TEST(demolish_a_scaffold_pays_the_same_half) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 2000, 500);
    moveTo(w, me, startPad() + Vec2(0, 12));
    const int64_t paid = w.wallet(me)->ore;
    const auto gun = put(w, me, StructureKind::gatling, true);
    ASSERT_TRUE(gun.has_value());
    ASSERT_TRUE(!simOf(w, me).state.structure(*gun)->complete());
    EXPECT_EQ(paid - w.wallet(me)->ore, int64_t(125));
    const int64_t before = w.wallet(me)->ore;
    ASSERT_TRUE(!w.demolish(me, *gun).has_value());
    EXPECT_EQ(w.wallet(me)->ore - before, int64_t(62));
    EXPECT_TRUE(drivenUnit(w, me)->task != Unit::Task::building);
}

// Six Gatling Guns here: a seventh is refused; one demolished, the next is
// built.
TEST(demolish_frees_the_regions_limit) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 5000, 500);
    Simulation& sim = simOf(w, me);
    const Vec2 from = startPad() + Vec2(0, 30);
    int64_t placed = 0;
    for (int k = 0; k < 96 && placed < 5; k++) {
        const auto s = sim.snap(StructureKind::gatling, from + Vec2(std::cos(k * 0.9), std::sin(k * 0.9)) * (6 + k / 2.0));
        if (!s || sim.placementRefusal(StructureKind::gatling, *s)) continue;
        Structure g(sim.state.nextID++, StructureKind::gatling, Pilot::player, *s);
        g.hp = Rules::hp(StructureKind::gatling);
        sim.state.structures.push_back(g);
        sim.reindex();
        placed++;
    }
    ASSERT_TRUE(placed == 5);
    moveTo(w, me, startPad() + Vec2(0, 12));
    const auto sixth = put(w, me, StructureKind::gatling);
    ASSERT_TRUE(sixth.has_value());
    std::string why;
    EXPECT_TRUE(!put(w, me, StructureKind::gatling, false, &why).has_value());
    EXPECT_EQ(why, std::string("Gatling Gun limit reached: 6 in this region"));
    ASSERT_TRUE(!w.demolish(me, *sixth).has_value());
    EXPECT_EQ(w.limitCount(StructureKind::gatling, first().start), int64_t(5));
    EXPECT_TRUE(put(w, me, StructureKind::gatling).has_value());
}

// The reach: past the repair reach (and its slack) it is refused; from the
// top-down view (nothing driven) anywhere in the region.
TEST(demolish_within_reach_or_from_above) {
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 2000, 500);
    moveTo(w, me, startPad() + Vec2(0, 12));
    const auto dummy = put(w, me, StructureKind::dummy);
    ASSERT_TRUE(dummy.has_value());
    const Vec2 d = simOf(w, me).state.structure(*dummy)->position;
    moveTo(w, me, d + Vec2(0, 8));
    EXPECT_EQ(w.demolish(me, *dummy), std::optional<std::string>("Too far away"));
    EXPECT_EQ(w.demolish(me, 999999), std::optional<std::string>("No building of the realm's here"));
    w.regionOf(me)->handBack(me);
    w.step();
    ASSERT_TRUE(!w.regionOf(me)->driving(me).has_value());
    const int64_t before = w.wallet(me)->ore;
    ASSERT_TRUE(!w.demolish(me, *dummy).has_value());
    EXPECT_EQ(w.wallet(me)->ore - before, int64_t(5));
}

// A home must remain: the realm's only finished Citadel stays; with a second
// in Cold Fork either may go, but not while someone stands in its hall.
TEST(demolish_keeps_the_last_citadel_and_one_with_someone_in_it) {
    const bool was = Rules::citadelHall();
    Rules::setCitadelHall(true);
    WorldMap w(first(), params());
    w.seat(me, key);
    w.setProgress(me, at(5));
    fill(w, me, 5000, 500);
    const auto home = put(w, me, StructureKind::citadel);
    ASSERT_TRUE(home.has_value());
    const Structure c = *simOf(w, me).state.structure(*home);
    // Outside its +y wall, within reach.
    moveTo(w, me, c.position + Vec2(0, Rules::radius(StructureKind::citadel) + 1));
    EXPECT_EQ(w.demolish(me, *home), std::optional<std::string>("The realm's last Citadel stays: a home must remain"));
    goTo(w, me, "cold-fork");
    fill(w, me, 5000, 500);
    const auto second = put(w, me, StructureKind::citadel);
    ASSERT_TRUE(second.has_value());
    const Structure c2 = *simOf(w, me).state.structure(*second);
    // In its hall: refused.
    moveTo(w, me, c2.position + Vec2(1, 1));
    EXPECT_EQ(w.demolish(me, *second), std::optional<std::string>("Someone is inside"));
    moveTo(w, me, c2.position + Vec2(0, Rules::radius(StructureKind::citadel) + 1));
    const int64_t before = w.wallet(me)->ore;
    const auto why = w.demolish(me, *second);
    if (why) std::printf("  refused: %s\n", why->c_str());
    EXPECT_TRUE(!why.has_value());
    EXPECT_EQ(w.wallet(me)->ore - before, int64_t(200));
    EXPECT_EQ(w.realmCount(StructureKind::citadel), int64_t(1));
    Rules::setCitadelHall(was);
}
