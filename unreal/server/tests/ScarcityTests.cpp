// E1, scarcity (docs/regions.md "Scarcity"): a world region holds one or two
// ore veins of three to five mounds and one MH well, its ore in all by its
// brood level; the region tool refuses more. The veins grow back a tenth of
// their ore a world day, up to full, held or caught up; a starfall's deposit
// never does. A world saved before adopts the region files' new veins on
// load, its buildings and the wells under its Derricks kept. Isolated
// scenes; files in build/save-tests. Run from unreal/server (as `make test`
// does).

#include "test.h"

#include "RegionFile.h"
#include "Starfall.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cstdio>
#include <optional>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;

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
    return p;
}

std::string fresh(const std::string& name) {
    mkdir("build/save-tests", 0755);
    const std::string path = "build/save-tests/" + name + ".sqlite";
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
    return path;
}

const std::string key = "5ca7ce5ca7ce5ca7ce5ca7ce5ca7ce00";

/// The scarcity problems validate finds in `f` (its other checks aside).
std::vector<std::string> scarcity(const region::RegionFile& f) {
    std::vector<std::string> out;
    for (const region::Problem& p : region::validate(f, region::build(f)))
        for (const char* word : {"vein", "wells; a region", "ore in all", "drops the base"})
            if (p.text().find(word) != std::string::npos) {
                out.push_back(p.text());
                break;
            }
    return out;
}

region::RegionFile startFile() {
    for (const WorldRegion& g : first().regions)
        if (g.file.id == "start") return g.file;
    return {};
}

/// Open ground for a lone mound in `f`: more than 12 cells from every mound
/// and well (a vein of its own) and taking no landform's room (sculpt: a
/// vein keeps the forms 7 cells off it), so it adds no problem but its
/// vein's to validate. Found by the rule, not written: the regions are
/// re-sculpted from time to time. Nil: none.
std::optional<Vec2> openGround(const region::RegionFile& f) {
    region::RegionFile zero = f;
    zero.brood = 0;
    const size_t before = region::validate(zero, region::build(zero)).size();
    const Vec2 ext = f.extent();
    for (double y = 12; y < ext.y - 12; y += 6)
        for (double x = 12; x < ext.x - 12; x += 6) {
            const Vec2 q(x, y);
            bool far = true;
            for (const region::Deposit& d : f.deposits) far = far && distance(d.at, q) > 12;
            for (const region::WellSite& w : f.wells) far = far && distance(w.at, q) > 12;
            if (!far) continue;
            region::RegionFile g = zero;
            g.deposits.push_back(region::Deposit{q, 10, 0});
            if (region::validate(g, region::build(g)).size() == before) return q;
        }
    return std::nullopt;
}

bool has(const std::vector<std::string>& lines, const std::string& word) {
    for (const std::string& l : lines)
        if (l.find(word) != std::string::npos) return true;
    return false;
}

} // namespace

/// Every region of the first world: one or two veins of three to five
/// mounds, each mound within reach of its vein's middle, one well, its ore
/// in its brood level's range.
TEST(scarcity_world_regions_hold_their_veins) {
    for (const WorldRegion& g : first().regions) {
        const region::RegionFile& f = g.file;
        const auto veins = region::veins(f.deposits, f.wells);
        int64_t ore = 0;
        for (const region::Deposit& d : f.deposits) ore += d.amount;
        std::printf("  %-14s brood %lld: %zu veins, %zu mounds, %lld ore, %zu well\n", f.id.c_str(), (long long)f.brood, veins.size(),
                    f.deposits.size(), (long long)ore, f.wells.size());
        EXPECT_TRUE(!veins.empty() && int64_t(veins.size()) <= region::veinsMost);
        for (const auto& v : veins) EXPECT_TRUE(int64_t(v.size()) >= region::moundsFewest && int64_t(v.size()) <= region::moundsMost);
        EXPECT_EQ(int64_t(f.wells.size()), region::wellsMost);
        const auto range = region::oreRange(f.brood);
        ASSERT_TRUE(range.has_value());
        EXPECT_TRUE(ore >= range->fewest && ore <= range->most);
        EXPECT_TRUE(scarcity(f).empty());
    }
}

/// The region tool refuses a third vein, a vein of six, a mound off its
/// vein, a second well, ore over the level's range and the base's own ore.
TEST(scarcity_validate_refuses_plenty) {
    const region::RegionFile ok = startFile();
    ASSERT_TRUE(scarcity(ok).empty());
    const auto open = openGround(ok);
    ASSERT_TRUE(open.has_value());
    {
        // A third vein: a mound far from both.
        region::RegionFile f = ok;
        f.deposits.push_back(region::Deposit{*open, 10, 0});
        const auto p = scarcity(f);
        EXPECT_TRUE(has(p, "3 veins"));
        EXPECT_TRUE(has(p, "a vein of 1 mounds"));
    }
    {
        // A sixth mound in the first vein, and a mound off its middle.
        region::RegionFile f = ok;
        f.deposits.push_back(region::Deposit{f.deposits[0].at + Vec2(-6.5, 0), 10, 0});
        const auto p = scarcity(f);
        EXPECT_TRUE(has(p, "a vein of 6 mounds"));
    }
    {
        region::RegionFile f = ok;
        f.wells.push_back(region::WellSite{Vec2(36, 74), 2250});
        EXPECT_TRUE(has(scarcity(f), "2 wells"));
    }
    {
        region::RegionFile f = ok;
        f.deposits[0].amount += 5000;
        EXPECT_TRUE(has(scarcity(f), "ore in all; a brood level 1 region holds 6000 to 8000"));
    }
    {
        region::RegionFile f = ok;
        f.dropResources = false;
        EXPECT_TRUE(has(scarcity(f), "drops the base"));
    }
    {
        // Brood 0 (the example file, a generated region): no limit.
        region::RegionFile f = ok;
        f.brood = 0;
        f.deposits.push_back(region::Deposit{*open, 10, 0});
        EXPECT_TRUE(scarcity(f).empty());
    }
}

/// Mined out, a vein grows back a tenth of its ore a world day, held (no
/// pilot: the regions' games stand) or caught up after a stop, and stops at
/// full; a starfall's deposit never grows.
TEST(scarcity_veins_regrow_a_tenth_a_day) {
    WorldParams p = params();
    p.starfalls = true;
    WorldMap w(first(), p);
    GameState& st = w.start().simulation().state;
    ASSERT_TRUE(st.patches.size() >= size_t(2));
    const int64_t full0 = st.patches[0].initial, full1 = st.patches[1].initial;
    st.patches[0].remaining = 0;
    st.patches[1].remaining = full1 - 10;
    // A starfall's deposit in the region, half drilled.
    const auto fall = w.forceStarfall(0, w.regionNamed("start"));
    ASSERT_TRUE(fall.has_value());
    w.idle(0.5);
    const size_t hot = st.patches.size() - 1;
    ASSERT_TRUE(hot >= size_t(9));
    st.patches[hot].remaining = st.patches[hot].initial / 2;
    const int64_t hotHalf = st.patches[hot].remaining;
    // A world day, half a second at a time.
    for (int i = 0; i < int(WorldClock::dayLength * 2); i++) w.idle(0.5);
    std::printf("  a mound of %lld empty: %lld after a world day\n", (long long)full0, (long long)st.patches[0].remaining);
    EXPECT_TRUE(std::abs(double(st.patches[0].remaining) - double(full0) * region::regrowPerDay) <= 1.0);
    EXPECT_TRUE(!st.patches[0].depletedAt.has_value());
    EXPECT_EQ(st.patches[1].remaining, full1);
    EXPECT_EQ(st.patches[hot].remaining, hotHalf);
    // Caught up after a stop of five world days: half of it.
    w.catchUp(5 * WorldClock::dayLength);
    EXPECT_TRUE(std::abs(double(st.patches[0].remaining) - double(full0) * 6 * region::regrowPerDay) <= 1.0);
    // And no more than full, however long.
    w.catchUp(20 * WorldClock::dayLength);
    EXPECT_EQ(st.patches[0].remaining, full0);
    EXPECT_EQ(st.patches[hot].remaining, hotHalf);
}

/// A world saved before E1 (schema 24: deposits and wells by index of the
/// old layout) with buildings on it: on load the region file's veins come in
/// full and the old deposits are gone, every building stands, a Derrick on
/// an old well keeps a well under it, a mound under a building is left
/// empty; saved again (schema 26) and loaded, it is the same world.
TEST(scarcity_an_old_save_adopts_the_new_veins) {
    const std::string path = fresh("e1adopt");
    std::string why;
    // The steps before E1's (26).
    std::vector<WorldStore::Migration> old;
    for (const WorldStore::Migration& m : WorldStore::migrations())
        if (m.version < 26) old.push_back(m);
    ASSERT_TRUE(old.back().version == int64_t(24) || old.back().version == int64_t(25));
    // Ground cells of the start (144 x 96): region (x, z) less (72, 48).
    const auto g = [](double x, double z) { return Vec2(x - 72, z - 48); };
    const region::RegionFile file = startFile();
    const Vec2 mound = g(file.deposits[0].at.x, file.deposits[0].at.y);
    {
        auto store = WorldStore::open(path, "first", &why, old);
        ASSERT_TRUE(store != nullptr);
        ASSERT_TRUE(store->exec("INSERT INTO regions (region, time, next_id) VALUES ('start', 500, 30)", &why));
        // A Citadel, a Derrick on the old layout's south well (60, 61), a
        // Hab Dome where the new layout's first mound now lies.
        const auto row = [&](int64_t id, const char* kind, Vec2 at, double hp) {
            return "('start', " + std::to_string(id) + ", '" + kind + "', " + std::to_string(at.x) + ", " + std::to_string(at.y) + ", '" + key +
                   "', NULL, " + std::to_string(hp) + ")";
        };
        const bool built = store->exec("INSERT INTO buildings (region, id, kind, x, y, built_by, build_left, hp) VALUES " +
                                           row(4, "citadel", g(72, 60), 1500) + ", " + row(5, "derrick", g(60, 61), 100) + ", " +
                                           row(6, "habDome", mound, 200),
                                       &why);
        if (!built) std::printf("    %s\n", why.c_str());
        ASSERT_TRUE(built);
        // The old layout's 43 deposits and 5 wells, mined down.
        for (int k = 0; k < 43; k++)
            ASSERT_TRUE(store->exec("INSERT INTO deposits (region, idx, remaining, depleted_at) VALUES ('start', " + std::to_string(k) +
                                        ", 100, NULL)",
                                    &why));
        for (int k = 0; k < 5; k++)
            ASSERT_TRUE(store->exec("INSERT INTO wells (region, idx, remaining) VALUES ('start', " + std::to_string(k) + ", 700)", &why));
        ASSERT_TRUE(store->exec("INSERT INTO pilots (id, ore, hydrogen, region, unit, levels, progress) VALUES ('" + key +
                                    "', 40, 7, 'start', NULL, '', '')",
                                &why));
    }
    auto store = WorldStore::open(path, "first", &why);
    ASSERT_TRUE(store != nullptr);
    EXPECT_EQ(store->schemaVersion(), WorldStore::version());
    const auto loaded = store->load(&why);
    ASSERT_TRUE(loaded.has_value());
    WorldMap w(first(), params());
    const auto problems = w.restore(*loaded);
    for (const std::string& p : problems) std::printf("    %s\n", p.c_str());
    EXPECT_TRUE(problems.empty());
    const GameState& st = w.start().simulation().state;
    EXPECT_EQ(st.structures.size(), size_t(3));
    ASSERT_TRUE(st.patches.size() == file.deposits.size());
    for (size_t k = 0; k < st.patches.size(); k++) {
        if (k == 0) {
            // Under the Hab Dome: empty, for good.
            EXPECT_EQ(st.patches[k].remaining, int64_t(0));
            EXPECT_EQ(st.patches[k].initial, int64_t(0));
        } else {
            EXPECT_EQ(st.patches[k].remaining, file.deposits[k].amount);
        }
    }
    ASSERT_TRUE(st.wells && st.wells->size() == size_t(2));
    EXPECT_EQ((*st.wells)[0].remaining, file.wells[0].amount);
    EXPECT_TRUE(distance((*st.wells)[1].position, g(60, 61)) < 0.01);
    EXPECT_EQ((*st.wells)[1].remaining, Rules::wellHydrogen);
    // Mined a little, saved (schema 26: by place) and loaded again.
    w.start().simulation().state.patches[2].remaining = 333;
    (*w.start().simulation().state.wells)[1].remaining = 1200;
    ASSERT_TRUE(store->write(w.save(), &why).has_value());
    const auto again = store->load(&why);
    ASSERT_TRUE(again.has_value());
    for (const SavedRegion& r : again->regions)
        if (r.id == "start")
            for (const SavedDeposit& d : r.deposits) EXPECT_TRUE(d.at.has_value());
    WorldMap w2(first(), params());
    EXPECT_TRUE(w2.restore(*again).empty());
    const GameState& st2 = w2.start().simulation().state;
    EXPECT_EQ(st2.structures.size(), size_t(3));
    EXPECT_EQ(st2.patches[2].remaining, int64_t(333));
    EXPECT_EQ(st2.patches[0].remaining, int64_t(0));
    ASSERT_TRUE(st2.wells && st2.wells->size() == size_t(2));
    EXPECT_EQ((*st2.wells)[1].remaining, int64_t(1200));
}

/// Saved by place (schema 26): a deposit the region file no longer lays
/// out is gone, one it still does keeps its ore; a well with no Derrick on
/// it that the file dropped is gone.
TEST(scarcity_by_place_drops_what_the_file_dropped) {
    const region::RegionFile file = startFile();
    const auto g = [](Vec2 r) { return Vec2(r.x - 72, r.y - 48); };
    WorldSave s;
    SavedRegion r;
    r.id = "start";
    r.time = 50;
    r.nextId = 10;
    r.deposits.push_back(SavedDeposit{0, 5, std::nullopt, g(Vec2(10, 10))});
    r.deposits.push_back(SavedDeposit{1, 77, std::nullopt, g(file.deposits[3].at)});
    r.wells.push_back(SavedWell{0, 10, g(Vec2(104, 22))});
    r.wells.push_back(SavedWell{1, 900, g(file.wells[0].at)});
    s.regions.push_back(r);
    WorldMap w(first(), params());
    EXPECT_TRUE(w.restore(s).empty());
    const GameState& st = w.start().simulation().state;
    ASSERT_TRUE(st.patches.size() == file.deposits.size());
    for (size_t k = 0; k < st.patches.size(); k++) EXPECT_EQ(st.patches[k].remaining, k == 3 ? int64_t(77) : file.deposits[k].amount);
    ASSERT_TRUE(st.wells && st.wells->size() == size_t(1));
    EXPECT_EQ((*st.wells)[0].remaining, int64_t(900));
}
