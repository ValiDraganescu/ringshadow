// Blueprint sites, survey chips and building gates in a world
// (docs/prospector-progression.md, "Blueprint sites" and "Buildings in the
// MMO"; WorldSites.cpp, WorldMap::build): sites seen and opened once per
// pilot, a known blueprint's ore and MH instead, the act key held at a
// wreck and a vault, a Scanner pulse, every build refusal, the chips' dice
// with a fixed seed, and the finds kept by the save. Isolated scenes of a
// few seconds on The First Reach; run from unreal/server.

#include "test.h"

#include "Rules.h"
#include "Session.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using NewsKind = net::ProgressMessage::NewsKind;

constexpr PilotId me = 41;
const std::string key = "00112233445566778899aabbccddeeff";

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
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

/// Steps `ticks` standing still, the act key held or not.
void wait(WorldMap& w, PilotId p, int ticks, bool hold = false) {
    for (int i = 0; i < ticks; i++) {
        PilotInput in;
        in.tick = ++inputTick;
        in.visit = w.visit(p);
        in.hold = hold;
        if (const Unit* u = drivenUnit(w, p)) in.heading = u->heading;
        w.input(p, in);
        w.step();
    }
}

/// Ticks in `seconds`.
int ticks(const WorldMap& w, double seconds) { return int(std::ceil(seconds / w.start().description().step)); }

/// A site of the world by region id and name: its region's index and its
/// index there.
std::pair<size_t, size_t> site(const std::string& region, const std::string& name) {
    for (size_t r = 0; r < first().regions.size(); r++) {
        if (first().regions[r].file.id != region) continue;
        const auto& s = first().regions[r].built.sites;
        for (size_t i = 0; i < s.size(); i++)
            if (s[i].name == name) return {r, i};
    }
    std::printf("    no site %s/%s\n", region.c_str(), name.c_str());
    return {0, 0};
}

Vec2 siteAt(const std::string& region, const std::string& name) {
    const auto [r, i] = site(region, name);
    return first().regions[r].built.sites[i].at;
}

/// The driven Prospector put `off` cells east of a site in its region.
void standAt(WorldMap& w, PilotId p, const std::string& region, const std::string& name, double off = 0.5) {
    drivenUnit(w, p)->position = siteAt(region, name) + Vec2(off, 0);
}

/// Flies from the start pad to `region`'s landing and waits it out.
bool flyTo(WorldMap& w, PilotId p, const std::string& region) {
    if (w.fly(p, region, "landing")) return false;
    for (int i = 0; i < ticks(w, 10) && w.flying(p); i++) wait(w, p, 1);
    return !w.flying(p) && w.regionOf(p) == &w.region(*w.regionNamed(region));
}

int count(const std::vector<net::ProgressMessage::News>& news, NewsKind kind) {
    int n = 0;
    for (const auto& x : news) n += x.kind == kind ? 1 : 0;
    return n;
}

const net::ProgressMessage::News* find(const std::vector<net::ProgressMessage::News>& news, NewsKind kind) {
    for (const auto& x : news)
        if (x.kind == kind) return &x;
    return nullptr;
}

ProspectorProgress at(int64_t level) {
    ProspectorProgress g;
    g.xp = Progression::xp(level);
    return g;
}

/// A spot on open ground round the driven Prospector for a `kind` (off the
/// pad's plate), or nil.
std::optional<Vec2> spot(WorldMap& w, PilotId p, StructureKind kind) {
    Simulation& sim = w.regionOf(p)->simulation();
    const Unit u = *drivenUnit(w, p);
    const double reach = Region::buildReach(kind);
    for (double d = reach; d <= reach + Region::buildSlack; d += 0.5)
        for (int k = 0; k < 16; k++) {
            const double h = k * 2 * pi / 16;
            const auto at = sim.snap(kind, u.position + Vec2(std::cos(h), std::sin(h)) * d);
            if (!at || distance(*at, u.position) > reach + Region::buildSlack || sim.placementRefusal(kind, *at) ||
                region::touchesPadPlate(first().regions[first().start].built.pads, *at, Rules::radius(kind)))
                continue;
            return at;
        }
    return std::nullopt;
}

} // namespace

/// No first-ten-minutes pay in the sums (FirstTenTests.cpp).
WorldParams unchained() {
    WorldParams p;
    p.firstStep = 0;
    return p;
}

// The Citadel's crate by Prospect Landing's pad: seen from the pad, opened
// by walking into it: the blueprint (50 × floor 1) and its news. Stepping
// off and back opens nothing again.
TEST(sites_crate_opens_on_reaching_it_once) {
    WorldMap w(first(), unchained());
    ASSERT_TRUE(w.join(me));
    const std::string crate = "start/Landing crate";
    wait(w, me, 1);
    const auto g0 = *w.progress(me);
    EXPECT_TRUE(g0.known.count(crate) == 1);
    EXPECT_TRUE(g0.sites.empty());
    EXPECT_TRUE(g0.blueprints.empty());
    w.takeNews(me);
    standAt(w, me, "start", "Landing crate", 1.0);
    wait(w, me, 1);
    auto g = *w.progress(me);
    EXPECT_TRUE(g.sites.count(crate) == 1);
    EXPECT_TRUE(g.blueprints.count(StructureKind::citadel) == 1);
    EXPECT_NEAR(g.xp, WorldXp::blueprintPerFloor * 1, 1e-9);
    const auto news = w.takeNews(me);
    const auto* s = find(news, NewsKind::site);
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(s->id == crate);
    EXPECT_EQ(s->value, int64_t(0));
    const auto* b = find(news, NewsKind::blueprint);
    ASSERT_TRUE(b != nullptr);
    EXPECT_TRUE(b->id == "citadel");
    EXPECT_EQ(b->value, int64_t(1));
    // The message says so.
    const auto m = *w.progressMessage(me);
    EXPECT_TRUE(m.sitesFound.count(crate) == 1);
    EXPECT_TRUE(m.sitesKnown.count(crate) == 1);
    // Off and back on: nothing more.
    standAt(w, me, "start", "Landing crate", 6.0);
    wait(w, me, 2);
    standAt(w, me, "start", "Landing crate", 1.0);
    wait(w, me, 2);
    EXPECT_NEAR(w.progress(me)->xp, WorldXp::blueprintPerFloor, 1e-9);
    EXPECT_EQ(count(w.takeNews(me), NewsKind::site), 0);
}

// A crate out of reach (2 cells) does not open; the second pilot opens its
// own: the site stays in the world for the next pilot.
TEST(sites_each_pilot_opens_its_own) {
    WorldMap w(first(), unchained());
    ASSERT_TRUE(w.join(me));
    constexpr PilotId you = 42;
    standAt(w, me, "start", "Landing crate", WorldSites::reach + 0.3);
    wait(w, me, 2);
    EXPECT_TRUE(w.progress(me)->sites.empty());
    standAt(w, me, "start", "Landing crate", 1.0);
    wait(w, me, 1);
    EXPECT_TRUE(w.progress(me)->sites.size() == 1);
    // One drives a region at a time: the first leaves, the next comes.
    w.leave(me);
    ASSERT_TRUE(w.join(you));
    EXPECT_TRUE(w.progress(you)->sites.empty());
    // It walks up to the same crate: its own find.
    Region& r = *w.regionOf(you);
    if (const auto id = r.driving(you)) {
        const auto i = r.simulation().unitIndex(*id);
        r.simulation().state.units[size_t(*i)].position = siteAt("start", "Landing crate") + Vec2(-1, 0);
    }
    wait(w, you, 1);
    EXPECT_TRUE(w.progress(you)->blueprints.count(StructureKind::citadel) == 1);
}

// A wreck opens after 5 s of the act key held within reach; letting go
// starts over; the Opening in the message counts from when it began. The
// Derrick's blueprint is known already: 100 ore and 20 MH instead, no XP.
TEST(sites_wreck_held_5_s_and_a_known_blueprint_pays_ore_and_mh) {
    WorldMap w(first());
    ASSERT_TRUE(w.join(me));
    ASSERT_TRUE(w.findBlueprint(me, StructureKind::derrick, Progression::blueprintFloor(StructureKind::derrick)));
    w.takeNews(me);
    const double xp = w.progress(me)->xp;
    const auto wallet = *w.wallet(me);
    const std::string wreck = "start/Lander wreck";
    standAt(w, me, "start", "Lander wreck");
    wait(w, me, ticks(w, 4), true);
    EXPECT_TRUE(w.progress(me)->sites.empty());
    const auto m = *w.progressMessage(me);
    ASSERT_TRUE(m.opening.has_value());
    EXPECT_TRUE(m.opening->site == wreck);
    EXPECT_NEAR(m.opening->needs, WorldSites::wreck, 1e-9);
    EXPECT_TRUE(m.opening->refusal.empty());
    // Let go: the hold starts over.
    wait(w, me, 2, false);
    EXPECT_TRUE(!w.progressMessage(me)->opening.has_value());
    wait(w, me, ticks(w, 4), true);
    EXPECT_TRUE(w.progress(me)->sites.empty());
    wait(w, me, ticks(w, 1.2), true);
    const auto g = *w.progress(me);
    EXPECT_TRUE(g.sites.count(wreck) == 1);
    EXPECT_NEAR(g.xp, xp, 1e-9);
    const auto after = *w.wallet(me);
    EXPECT_EQ(after.ore - wallet.ore, net::kSiteOre);
    EXPECT_EQ(after.hydrogen - wallet.hydrogen, net::kSiteHydrogen);
    const auto news = w.takeNews(me);
    const auto* s = find(news, NewsKind::site);
    ASSERT_TRUE(s != nullptr);
    EXPECT_EQ(s->value, int64_t(1));
    EXPECT_EQ(count(news, NewsKind::blueprint), 0);
    // Held on: nothing more.
    wait(w, me, ticks(w, 6), true);
    EXPECT_EQ(w.wallet(me)->ore, after.ore);
}

// A vault under its blueprint's floor: holding does nothing and the
// Opening says why; at the floor less Blueprint savant it opens in 10 s.
TEST(sites_vault_opens_only_at_its_floor_less_blueprint_savant) {
    WorldParams fuel;
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    ASSERT_TRUE(w.join(me));
    ASSERT_TRUE(flyTo(w, me, "tern-ridge"));
    standAt(w, me, "tern-ridge", "Tern vault");
    wait(w, me, ticks(w, 11), true);
    EXPECT_TRUE(w.progress(me)->sites.empty());
    auto m = *w.progressMessage(me);
    ASSERT_TRUE(m.opening.has_value());
    EXPECT_TRUE(m.opening->refusal == "The Sentinel vault opens at level 55");
    // Level 49 with Blueprint savant at 3: the floor is 49.
    ProspectorProgress g = at(49);
    g.ranks[Skill::blueprintSavant] = 3;
    g.blueprints = {StructureKind::citadel};
    EXPECT_EQ(g.floor(StructureKind::sentinel), int64_t(49));
    w.setProgress(me, g);
    w.takeNews(me);
    standAt(w, me, "tern-ridge", "Tern vault");
    wait(w, me, ticks(w, 9), true);
    EXPECT_TRUE(w.progress(me)->sites.empty());
    m = *w.progressMessage(me);
    ASSERT_TRUE(m.opening.has_value());
    EXPECT_TRUE(m.opening->refusal.empty());
    wait(w, me, ticks(w, 1.5), true);
    EXPECT_TRUE(w.progress(me)->sites.count("tern-ridge/Tern vault") == 1);
    EXPECT_TRUE(w.progress(me)->blueprints.count(StructureKind::sentinel) == 1);
    const auto news = w.takeNews(me);
    const auto* b = find(news, NewsKind::blueprint);
    ASSERT_TRUE(b != nullptr);
    EXPECT_NEAR(b->xp, WorldXp::blueprintPerFloor * 55, 1e-9);
    EXPECT_EQ(b->value, int64_t(49));
}

// A buried cache is seen within 6 cells only; a wreck within sight; a
// Scanner pulse at rank 1 marks a region's wrecks on arriving, not its
// caches.
TEST(sites_seen_near_and_marked_by_a_scanner_pulse) {
    WorldParams fuel;
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    WorldMap w(first(), fuel);
    ASSERT_TRUE(w.join(me));
    ProspectorProgress g = at(20);
    g.ranks[Skill::scannerPulse] = 1;
    w.setProgress(me, g);
    ASSERT_TRUE(flyTo(w, me, "brine-pans"));
    auto known = w.progress(me)->known;
    EXPECT_TRUE(known.count("brine-pans/Barge wreck") == 1);
    EXPECT_TRUE(known.count("brine-pans/Salt cache") == 0);
    standAt(w, me, "brine-pans", "Salt cache", 8.0);
    wait(w, me, 1);
    EXPECT_TRUE(w.progress(me)->known.count("brine-pans/Salt cache") == 0);
    standAt(w, me, "brine-pans", "Salt cache", 5.0);
    wait(w, me, 1);
    EXPECT_TRUE(w.progress(me)->known.count("brine-pans/Salt cache") == 1);
    EXPECT_TRUE(w.progressMessage(me)->sitesKnown.count("brine-pans/Salt cache") == 1);
}

// Every build refusal in a world, in order: no blueprint (the Garrison has
// none at all), under the floor, at the limit. Blueprint savant lowers the
// floor; Overbuild raises the Hab Dome, Derrick and Sentinel limits.
TEST(sites_builds_refused_without_blueprint_under_floor_past_limit) {
    WorldMap w(first());
    ASSERT_TRUE(w.join(me));
    auto why = w.build(me, StructureKind::citadel, *spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "No Citadel blueprint yet: find it at a site");
    why = w.build(me, StructureKind::garrison, drivenUnit(w, me)->position);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "No Garrison blueprint in this world");
    ProspectorProgress g = at(1);
    g.blueprints = {StructureKind::citadel, StructureKind::derrick, StructureKind::habDome};
    w.setProgress(me, g);
    why = w.build(me, StructureKind::derrick, drivenUnit(w, me)->position);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "The Derrick opens at level 3");
    why = w.build(me, StructureKind::habDome, drivenUnit(w, me)->position);
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "The Hab Dome opens at level 8");
    // One Citadel, then its limit.
    EXPECT_TRUE(!w.build(me, StructureKind::citadel, *spot(w, me, StructureKind::citadel)).has_value());
    EXPECT_EQ(w.realmCount(StructureKind::citadel), int64_t(1));
    why = w.build(me, StructureKind::citadel, *spot(w, me, StructureKind::citadel));
    ASSERT_TRUE(why.has_value());
    EXPECT_TRUE(*why == "Citadel limit reached: 1");
    // Savant at 1: the Derrick opens at level 1, so the gate lets it by
    // (the ground refuses it here: no well).
    g = *w.progress(me);
    g.ranks[Skill::blueprintSavant] = 1;
    w.setProgress(me, g);
    why = w.build(me, StructureKind::derrick, drivenUnit(w, me)->position);
    EXPECT_TRUE(!why || why->find("opens at level") == std::string::npos);
    // The limits.
    ProspectorProgress o;
    EXPECT_EQ(o.limit(StructureKind::derrick), int64_t(4));
    EXPECT_EQ(o.limit(StructureKind::habDome), int64_t(3));
    EXPECT_EQ(o.limit(StructureKind::citadel), int64_t(1));
    EXPECT_EQ(o.limit(StructureKind::garrison), int64_t(0));
    o.ranks[Skill::overbuild] = 2;
    EXPECT_EQ(o.limit(StructureKind::derrick), int64_t(6));
    EXPECT_EQ(o.limit(StructureKind::sentinel), int64_t(6));
    EXPECT_EQ(o.limit(StructureKind::foundry), int64_t(1));
    o.xp = Progression::xp(60);
    o.blueprints = {StructureKind::habDome};
    EXPECT_TRUE(!o.buildRefusal(StructureKind::habDome, 4));
    EXPECT_TRUE(*o.buildRefusal(StructureKind::habDome, 5) == "Hab Dome limit reached: 5 (Overbuild allows more)");
}

// `--dev-blueprints`: a new pilot has every blueprint (shots and tests).
TEST(sites_dev_blueprints_give_every_blueprint) {
    WorldParams params;
    params.devBlueprints = true;
    WorldMap w(first(), params);
    ASSERT_TRUE(w.join(me));
    EXPECT_EQ(w.progress(me)->blueprints.size(), Progression::blueprintKinds().size());
    EXPECT_TRUE(!w.build(me, StructureKind::citadel, *spot(w, me, StructureKind::citadel)).has_value());
}

// The chips' dice with a fixed seed: the same seed gives the same chips;
// at most one every 5 minutes of the world's clock; Core sampler and Lucky
// strike bring more; each pays 25 and marks a site not known, here or next
// door.
TEST(sites_survey_chips_rolled_with_a_fixed_seed) {
    auto run = [](const ProspectorProgress& g, std::vector<double>* times) {
        WorldParams params;
        params.chipSeed = 1234;
        WorldMap w(first(), params);
        w.join(me);
        w.setProgress(me, g);
        int64_t chips = 0;
        // 4 loads a minute for 50 hours.
        for (int i = 0; i < 4 * 60 * 50; i++) {
            w.setClock(i * 15.0);
            if (w.oreLoad(me)) {
                chips += 1;
                if (times) times->push_back(w.clock());
            }
        }
        return chips;
    };
    std::vector<double> times, again;
    const int64_t base = run(at(1), &times);
    EXPECT_EQ(run(at(1), &again), base);
    EXPECT_TRUE(times == again);
    // About 0.5% of 12,000 loads: 60, less the 5-minute gap.
    EXPECT_TRUE(base >= 30 && base <= 90);
    for (size_t i = 1; i < times.size(); i++) EXPECT_TRUE(times[i] - times[i - 1] >= WorldSites::chipEvery - 1e-9);
    ProspectorProgress lucky = at(1);
    lucky.ranks[Skill::coreSampler] = 3;
    lucky.ranks[Skill::luckyStrike] = 5;
    EXPECT_NEAR(lucky.chipChance(), 0.035 * 1.5, 1e-12);
    EXPECT_NEAR(at(1).chipChance(), 0.005, 1e-12);
    EXPECT_TRUE(run(lucky, nullptr) > base * 3);

    // One chip: 25 XP, a site marked in the start region or a neighbour.
    WorldParams params;
    params.chipSeed = 99;
    WorldMap w(first(), params);
    w.join(me);
    w.takeNews(me);
    w.findChip(me);
    const auto news = w.takeNews(me);
    const auto* c = find(news, NewsKind::chip);
    ASSERT_TRUE(c != nullptr);
    EXPECT_NEAR(c->xp, WorldXp::chip, 1e-9);
    EXPECT_EQ(c->value, int64_t(1));
    if (c->id != "blueprint") {
        EXPECT_TRUE(w.progress(me)->known.count(c->id) == 1);
        const std::string region = c->id.substr(0, c->id.find('/'));
        EXPECT_TRUE(region == "start" || region == "cold-fork" || region == "brine-pans" || region == "rust-hollow" ||
                    region == "tern-ridge" || region == "glass-flats" || region == "ashen-shelf" ||
                    region == "dust-terraces" || region == "slag-pockets");
    }
    // Many chips: some are blueprints, only ones the pilot lacks within
    // its level + 10; marks run out and chips still pay.
    int blueprints = 0;
    for (int i = 0; i < 200; i++) {
        const int64_t level = w.progress(me)->level();
        const auto had = w.progress(me)->blueprints;
        w.findChip(me);
        for (const auto& n : w.takeNews(me))
            if (n.kind == NewsKind::blueprint) {
                blueprints += 1;
                const auto k = parse<StructureKind>(n.id);
                ASSERT_TRUE(k.has_value());
                EXPECT_TRUE(!had.count(*k));
                EXPECT_TRUE(Progression::blueprintFloor(*k) <= level + WorldSites::chipBlueprintReach);
            }
    }
    // About one in 20 of 201, fewer as the reachable ones run out.
    EXPECT_TRUE(blueprints >= 2 && blueprints <= 8);
    EXPECT_EQ(w.progress(me)->chips, int64_t(201));
}

// The finds kept: sites found and known, blueprints, chips and the chip
// clock through the save and back.
TEST(sites_finds_kept_through_the_save) {
    WorldMap w(first());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::fresh);
    standAt(w, me, "start", "Landing crate", 1.0);
    wait(w, me, 1);
    w.setClock(1000);
    w.findChip(me);
    const ProspectorProgress g = *w.progress(me);
    EXPECT_TRUE(g.chipAt.has_value());
    EXPECT_TRUE(!g.known.empty());
    const WorldSave saved = w.save();
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/sites.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    {
        auto store = WorldStore::open(path, "first", &why);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->write(saved, &why).has_value());
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    const auto back = store->load(&why);
    ASSERT_TRUE(back.has_value());
    ASSERT_TRUE(back->pilots.size() == 1);
    EXPECT_TRUE(back->pilots[0].gated);
    WorldMap again(first());
    EXPECT_TRUE(again.restore(*back).empty());
    ASSERT_TRUE(again.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE(*again.progress(me) == g);
}

// A pilot saved by schema 3 (before blueprints gated building) with a
// Hab Dome and a Derrick of its own: on load it has their blueprints, its
// buildings stand, and it is saved gated.
TEST(sites_v3_pilot_keeps_the_blueprints_of_its_buildings) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/v3gated.sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    std::string why;
    const std::vector<WorldStore::Migration> v3(WorldStore::migrations().begin(), WorldStore::migrations().begin() + 3);
    ProspectorProgress g = at(4);
    {
        auto store = WorldStore::open(path, "first", &why, v3);
        ASSERT_TRUE(store != nullptr);
        EXPECT_EQ(store->schemaVersion(), int64_t(3));
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 50, 9)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO buildings (region, id, kind, x, y, owner, build_left, hp) VALUES "
                                "('start', 4, 'habDome', 60, 40, '" + key + "', NULL, 600), "
                                "('start', 5, 'derrick', 70, 40, '" + key + "', 12.5, 100)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress) VALUES ('" + key +
                                "', 10, 3, 'start', NULL, '', '" + SessionStore::encode(g, false) + "')", &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), int64_t(9));
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    ASSERT_TRUE(loaded->pilots.size() == 1);
    EXPECT_TRUE(!loaded->pilots[0].gated);
    WorldMap w(first());
    EXPECT_TRUE(w.restore(*loaded).empty());
    ASSERT_TRUE(w.seat(me, key) == WorldMap::Seated::restored);
    EXPECT_TRUE((w.progress(me)->blueprints == std::set<StructureKind>{StructureKind::derrick, StructureKind::habDome}));
    EXPECT_EQ(w.realmCount(StructureKind::habDome), int64_t(1));
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    EXPECT_TRUE(store->rows("SELECT gated FROM pilots")[0][0] == "1");
}
