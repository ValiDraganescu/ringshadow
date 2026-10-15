// The Matriarch and her nest in a world (docs/creatures.md, "The Matriarch";
// WorldNest.cpp): the nest keeps its horde, her roar rallies it, killing her
// pays every pilot who hurt her by its share, she comes back after the rule,
// and the siege hook fells every turret in a zone and takes her home.
// Isolated scenes in Slag Pockets of a few seconds to a few minutes, never a
// game; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "Rules.h"
#include "WorldMap.h"

#include <cmath>
#include <cstdio>

namespace {

using namespace ac;
using namespace ac::server;

constexpr PilotId me = 71;
constexpr PilotId other = 72;

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
    p.broodSeed = 0x9a7e1ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(seconds * 60);
    for (int i = 0; i < n; i++) w.step();
}

size_t nestRegion(const WorldMap& w) { return *w.nestRegion(); }
Simulation& nestSim(WorldMap& w) { return w.region(nestRegion(w)).simulation(); }

Unit* unitOf(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

/// The pilot taken to Slag Pockets, `d` cells from the nest toward its pad.
void toNest(WorldMap& w, PilotId p, double d) {
    const size_t r = nestRegion(w);
    const Vec2 home = *w.nestAt(r);
    const Vec2 pad = w.region(r).places()->pads.front().at;
    const Vec2 at = home + normalize(pad - home) * d;
    if (w.where(p) != r) ASSERT_TRUE(w.devTransfer(p, r, at));
    Unit* u = unitOf(w, p);
    ASSERT_TRUE(u != nullptr);
    Simulation& sim = nestSim(w);
    u->position = sim.nav ? sim.nav->nearestFree(at).value_or(at) : at;
    u->goal = std::nullopt;
    u->waypoints = std::nullopt;
}

Unit* queen(WorldMap& w) {
    const auto q = w.matriarch();
    if (!q) return nullptr;
    const auto i = w.region(q->first).simulation().unitIndex(q->second);
    return i ? &w.region(q->first).simulation().state.units[size_t(*i)] : nullptr;
}

/// A finished turret of the realm at `at` in Slag Pockets.
int64_t turretAt(WorldMap& w, StructureKind kind, Vec2 at) {
    Simulation& sim = nestSim(w);
    const Vec2 p = sim.nav ? sim.nav->nearestFree(at).value_or(at) : at;
    const int64_t id = sim.state.nextID++;
    sim.state.structures.push_back(Structure(id, kind, Pilot::player, p));
    sim.reindex();
    sim.refreshNav();
    return id;
}

} // namespace

/// Slag Pockets has the nest (on its east plateau); no other region does.
TEST(matriarch_nest_in_slag_pockets) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.nestRegion().has_value());
    EXPECT_EQ(w.nestRegion(), w.regionNamed("slag-pockets"));
    for (size_t r = 0; r < w.regionCount(); r++) EXPECT_EQ(w.nestAt(r).has_value(), r == nestRegion(w));
}

/// With a pilot in her region she is at her nest with 16 to 24 Shardlings
/// and 4 to 8 Gloamwings round her, on top of the region's cap; bugs killed
/// come back one every 20 s.
TEST(matriarch_nest_keeps_its_horde) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 45);
    stepFor(w, 0.5);
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    EXPECT_EQ(q->hp, Rules::hp(UnitKind::matriarch));
    EXPECT_TRUE(distance(q->position, *w.nestAt(nestRegion(w))) < 2);
    const auto [packs, flyers] = w.nestHorde();
    EXPECT_TRUE(packs >= Matriarch::nestShardlings && packs <= Matriarch::nestShardlingsMax);
    EXPECT_TRUE(flyers >= Matriarch::nestGloamwings && flyers <= Matriarch::nestGloamwingsMax);
    // Five Shardlings fall: one comes back every 20 s.
    int killed = 0;
    for (Unit& u : nestSim(w).state.units)
        if (killed < 5 && u.kind == UnitKind::shardling && u.bug && u.hp > 0) {
            u.hp = 0;
            killed++;
        }
    stepFor(w, 1);
    EXPECT_EQ(w.nestHorde().first, packs - 5);
    stepFor(w, Matriarch::refill * 2 + 1);
    EXPECT_TRUE(w.nestHorde().first >= packs - 3 && w.nestHorde().first <= packs - 2);
    stepFor(w, Matriarch::refill * 3 + 1);
    EXPECT_EQ(w.nestHorde().first, packs);
    // The region's own groups still come: the horde is not counted in its cap.
    EXPECT_TRUE(w.broodAlive(nestRegion(w)) > WorldBrood::cap(4) - 22 + packs);
}

/// A pilot coming within 25 cells: she roars (told on the wire), the nest's
/// bugs within reach turn on it and run faster, Shardlings hatch.
TEST(matriarch_roar_rallies_the_nest) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 45);
    stepFor(w, 0.5);
    const int64_t before = w.nestHorde().first;
    ASSERT_TRUE(w.sky() && w.sky()->roars.empty());
    toNest(w, me, 22);
    stepFor(w, 0.5);
    ASSERT_TRUE(w.sky().has_value());
    ASSERT_TRUE(w.sky()->roars.size() == 1);
    EXPECT_EQ(w.sky()->roars.front().region, int64_t(nestRegion(w)));
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    EXPECT_TRUE(q->bug->state == Bug::State::roar);
    const int64_t prey = unitOf(w, me)->id;
    const double t = nestSim(w).state.time;
    int rallied = 0, onIt = 0;
    for (const auto& g : w.broodGroups(nestRegion(w))) {
        if (!g.nest) continue;
        if (g.prey == prey) onIt++;
        for (const int64_t id : g.bugs)
            if (const auto u = nestSim(w).state.unit(id); u && u->bug->rallyUntil > t) rallied++;
    }
    EXPECT_TRUE(onIt >= 3);
    EXPECT_TRUE(rallied >= 10);
    EXPECT_EQ(w.nestHorde().first, std::min<int64_t>(before + Matriarch::hatch, Matriarch::nestShardlingsMax));
    // Once only on first sight: the next roar waits for the fight's 30 s.
    stepFor(w, 5);
    EXPECT_TRUE(w.sky()->roars.empty());
}

/// Her slam and her spit reach a pilot at her nest: it loses hp to both.
TEST(matriarch_slams_and_spits_at_a_pilot) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 45);
    stepFor(w, 0.5);
    // The horde held off, to see her strikes alone.
    for (Unit& u : nestSim(w).state.units)
        if (u.bug && u.kind != UnitKind::matriarch) u.hp = 0;
    stepFor(w, 0.2);
    toNest(w, me, Rules::radius(UnitKind::matriarch) + 9);
    stepFor(w, 3);
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    EXPECT_TRUE(q->bug->sac == Matriarch::struckSpit);
    toNest(w, me, Rules::radius(UnitKind::matriarch) + 1.5);
    bool slammed = false;
    for (int i = 0; i < 6 * 60 && !slammed; i++) {
        w.step();
        if (queen(w) && queen(w)->bug->sac == Matriarch::struckSlam) slammed = true;
        // The pilot stands its ground, facing her.
        if (Unit* u = unitOf(w, me)) u->hp = std::max(u->hp, 30.0);
    }
    EXPECT_TRUE(slammed);
}

/// Killing her pays everyone who hurt her by its share: XP as it hit, then
/// ore, MH and a rare blueprint copy; the world hears it; the nest scatters.
TEST(matriarch_killing_her_pays_all_who_fought) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    ASSERT_TRUE(w.join(other));
    w.step();
    const size_t start = *w.where(other);
    // The other pilot hurts her a quarter, and leaves.
    toNest(w, other, 30);
    stepFor(w, 0.5);
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    const int64_t qid = q->id;
    const double xpOther = w.progress(other)->xp;
    {
        const Unit shooter = *unitOf(w, other);
        nestSim(w).hit(qid, 1500, false, &shooter);
    }
    w.step();
    EXPECT_NEAR(w.progress(other)->xp - xpOther, Brood::xp(UnitKind::matriarch) * 0.25, 1e-6);
    // Elsewhere (one pilot drives a region at a time: this one is at the start).
    const size_t away = *w.regionNamed("cold-fork");
    ASSERT_TRUE(away != start);
    ASSERT_TRUE(w.devTransfer(other, away, w.region(away).places()->pads.front().at));
    // This pilot does the rest.
    toNest(w, me, 30);
    stepFor(w, 0.2);
    const auto mine = *w.wallet(me), theirs = *w.wallet(other);
    auto copies = [&](PilotId p) {
        int64_t n = 0;
        const ProspectorProgress progress = *w.progress(p);
        for (const auto& [k, c] : progress.blueprints) n += c;
        return n;
    };
    const int64_t bpMine = copies(me);
    w.takeNews(me);
    w.takeNews(other);
    q = queen(w);
    ASSERT_TRUE(q != nullptr);
    {
        const Unit shooter = *unitOf(w, me);
        nestSim(w).hit(qid, q->hp + 10, false, &shooter);
    }
    w.step();
    EXPECT_TRUE(!w.matriarch().has_value());
    ASSERT_TRUE(w.matriarchBack().has_value());
    const double share = w.wallet(me)->ore - mine.ore;
    const double theirShare = w.wallet(other)->ore - theirs.ore;
    EXPECT_TRUE(share > theirShare && theirShare > 0);
    EXPECT_NEAR(share + theirShare, double(Matriarch::rewardOre), 2.0);
    EXPECT_NEAR(theirShare / double(Matriarch::rewardOre), 0.25, 0.02);
    EXPECT_TRUE(w.wallet(me)->hydrogen > mine.hydrogen && w.wallet(other)->hydrogen > theirs.hydrogen);
    EXPECT_EQ(copies(me), bpMine + 1);
    // The news: her share to each, "down" to all.
    bool down = false, mineShare = false;
    for (const auto& n : w.takeNews(me)) {
        if (n.kind != net::ProgressMessage::NewsKind::matriarch) continue;
        if (n.id == "down") { down = true; EXPECT_EQ(n.value, int64_t(2)); }
        if (n.id.rfind("share/", 0) == 0) mineShare = true;
    }
    EXPECT_TRUE(down && mineShare);
    bool theirsDown = false;
    for (const auto& n : w.takeNews(other))
        if (n.kind == net::ProgressMessage::NewsKind::matriarch && n.id == "down") theirsDown = true;
    EXPECT_TRUE(theirsDown);
    // The horde scatters.
    for (const auto& g : w.broodGroups(nestRegion(w))) EXPECT_TRUE(!g.nest && g.leaving);
}

/// She comes back three world days after her fall (saved across a
/// restart), and the nest grows back from empty.
TEST(matriarch_returns_after_the_rule) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 45);
    stepFor(w, 0.5);
    ASSERT_TRUE(queen(w) != nullptr);
    {
        Unit* q = queen(w);
        const Unit shooter = *unitOf(w, me);
        nestSim(w).hit(q->id, q->hp + 1, false, &shooter);
    }
    w.step();
    const double fell = w.clock();
    ASSERT_TRUE(w.matriarchBack().has_value());
    EXPECT_NEAR(*w.matriarchBack(), fell + Matriarch::returnDays * w.params().dayLength, 0.1);
    // Saved and restored: still down, on the same clock.
    const WorldSave saved = w.save();
    ASSERT_TRUE(saved.nests.size() == 1);
    WorldMap again(first(), params());
    EXPECT_TRUE(again.restore(saved).empty());
    EXPECT_NEAR(*again.matriarchBack(), *w.matriarchBack(), 1e-9);
    // A world day on: still gone.
    stepFor(w, 7);
    EXPECT_TRUE(!w.matriarch().has_value());
    w.setClock(*w.matriarchBack() - 1);
    stepFor(w, 0.5);
    EXPECT_TRUE(!w.matriarch().has_value());
    w.setClock(*w.matriarchBack() + 0.1);
    stepFor(w, 0.5);
    ASSERT_TRUE(w.matriarch().has_value());
    EXPECT_TRUE(!w.matriarchBack().has_value());
    EXPECT_EQ(queen(w)->hp, Rules::hp(UnitKind::matriarch));
    // The nest grows back one bug at a time.
    const auto [packs, flyers] = w.nestHorde();
    EXPECT_TRUE(packs + flyers <= 1);
    stepFor(w, Matriarch::refill * 2 + 1);
    EXPECT_TRUE(w.nestHorde().first + w.nestHorde().second >= 2);
    EXPECT_TRUE(w.save().nests.empty());
}

/// The siege hook (Z2's): she leaves her nest with an escort, fells every
/// turret of the realm in the zone (and none outside it), then goes home.
TEST(matriarch_siege_fells_every_turret_in_a_zone_and_goes_home) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 45);
    stepFor(w, 0.5);
    const size_t r = nestRegion(w);
    const Vec2 home = *w.nestAt(r);
    // The horde off the field, to watch her and her escort.
    for (Unit& u : nestSim(w).state.units)
        if (u.bug && u.kind != UnitKind::matriarch) u.hp = 0;
    stepFor(w, 0.2);
    // A zone 18 cells west of the nest, on her plateau's way down.
    const Vec2 zone = home + Vec2(-18, 0);
    const int64_t a = turretAt(w, StructureKind::gatling, zone + Vec2(0, -3));
    const int64_t b = turretAt(w, StructureKind::sentinel, zone + Vec2(2, 4));
    const int64_t outside = turretAt(w, StructureKind::gatling, zone + Vec2(-4, 22));
    ASSERT_TRUE(w.matriarchSiege(r, zone, 8, 4));
    EXPECT_TRUE(!w.matriarchSiege(r, zone, 8, 4));  // one at a time
    ASSERT_TRUE(w.siege().has_value());
    bool sawHome = false;
    for (int i = 0; i < 300 * 60 && w.siege(); i++) {
        w.step();
        if (w.siege() && w.siege()->home) sawHome = true;
        // The pilot stays far off and whole.
        if (Unit* u = unitOf(w, me)) u->hp = Rules::hp(UnitKind::prospector);
    }
    EXPECT_TRUE(!w.siege().has_value());
    EXPECT_TRUE(sawHome);
    EXPECT_TRUE(!nestSim(w).state.structure(a).has_value());
    EXPECT_TRUE(!nestSim(w).state.structure(b).has_value());
    EXPECT_TRUE(nestSim(w).state.structure(outside).has_value());
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    EXPECT_TRUE(distance(q->position, home) <= Matriarch::tether + 1);
    EXPECT_TRUE(distance(q->bug->site, home) < 0.01);
}

/// A dev line takes the pilot to her and stages each scene.
TEST(matriarch_dev_lines) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    EXPECT_TRUE(w.devMatriarch("matriarch here 40").find("40 cells") != std::string::npos);
    EXPECT_EQ(w.where(me), w.nestRegion());
    EXPECT_TRUE(queen(w) != nullptr);
    EXPECT_EQ(w.devMatriarch("matriarch roar"), "dev: matriarch roars");
    EXPECT_TRUE(w.sky()->roars.size() == 1);
    EXPECT_EQ(w.devMatriarch("matriarch down"), "dev: matriarch down");
    w.step();
    EXPECT_TRUE(w.matriarchBack().has_value());
    EXPECT_EQ(w.devMatriarch("matriarch back"), "dev: matriarch back");
    stepFor(w, 0.5);
    EXPECT_TRUE(w.matriarch().has_value());
}

/// A Matriarch hunt (a perilous hunt for her alone) is done when she falls,
/// for every pilot who fought her; a Shardling killed doesn't count for it.
TEST(matriarch_hunt_done_when_she_falls) {
    WorldMap w(first(), params());
    ASSERT_TRUE(w.join(me));
    w.step();
    toNest(w, me, 30);
    stepFor(w, 0.5);
    const size_t r = nestRegion(w);
    net::CitadelMission m;
    m.id = 900;
    m.kind = net::CitadelMission::Kind::hunt;
    m.tier = net::CitadelMission::Tier::perilous;
    m.state = net::CitadelMission::State::taken;
    m.title = "The Matriarch";
    m.region = w.region(r).places() ? first().regions[r].file.id : "";
    m.target = WorldMissions::queenTarget(first().regions[r].file.id);
    m.need = 1;
    w.setJournal(me, {m});
    const Unit shooter = *unitOf(w, me);
    // A Shardling of her horde first: not her.
    for (const Unit& u : nestSim(w).state.units)
        if (u.kind == UnitKind::shardling && u.hp > 0) {
            nestSim(w).hit(u.id, u.hp + 1, false, &shooter);
            break;
        }
    w.step();
    EXPECT_TRUE(w.journal(me).front().state == net::CitadelMission::State::taken);
    Unit* q = queen(w);
    ASSERT_TRUE(q != nullptr);
    nestSim(w).hit(q->id, q->hp + 1, false, &shooter);
    w.step();
    EXPECT_TRUE(w.journal(me).front().state == net::CitadelMission::State::done);
}
