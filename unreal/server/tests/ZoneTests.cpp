// Canyon zones (Z1, docs/regions.md "zones"): the `cut` height edit, a
// zone's entrances, paths and prize in a region file (Z4: no build spots; parse,
// write, validate), Cold Fork's canyon, a group of bugs walking a path to the
// prize, and a save from before the canyon. Isolated scenes: a map built and
// looked at, or one group stepped for a minute or two; run from unreal/server.

#include "test.h"

#include "Brood.h"
#include "NavGrid.h"
#include "RegionFile.h"
#include "TerrainField.h"
#include "WorldMap.h"
#include "WorldStore.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::region;
using namespace ac::server;

/// A flat field with a level 2 mesa over its middle and a canyon cut into
/// it along a bend, its floor at level 0 (open to the low ground at its
/// south end), with `zone` added (JSON) and `more` keys.
std::string mesa(const std::string& zone = "", const std::string& more = "") {
    return R"({"id": "t", "name": "T", "size": "medium", "base": {"style": "openField", "seed": 1, "drop": ["resources", "doodads"]},
      "height": [
        {"op": "raise", "area": {"rect": [30, 10, 110, 70]}, "level": 2},
        {"op": "cut", "line": [[60, 6], [60, 30], [80, 45], [80, 80]], "width": 6, "level": 0},
        {"op": "cut", "line": [[79.5, 54], [80.5, 54]], "width": 14, "level": 0},
        {"op": "ramp", "from": [100, 82], "to": [100, 66], "width": 4, "fromLevel": 0, "level": 2},
        {"op": "ramp", "from": [40, 82], "to": [40, 66], "width": 4, "fromLevel": 0, "level": 2}
      ],
      "deposits": [{"at": [76, 52], "amount": 6000, "angle": 90}, {"at": [84, 52], "amount": 6000, "angle": 90}],
      "wells": [{"at": [80, 58], "amount": 6000}])" +
           (zone.empty() ? std::string() : R"(, "zones": [)" + zone + "]") + (more.empty() ? std::string() : ", " + more) + "}";
}

/// The canyon's zone: its mouth at the north end of the cut, one path down
/// the floor to the prize; `spots` as an old file has them (Z4: read and
/// ignored).
std::string zone(const std::string& spots = R"([])", const std::string& path = R"([[60, 14], [60, 30], [80, 45], [80, 49]])",
                 const std::string& entrance = R"([60, 12])") {
    return R"({"id": "bend", "name": "The Bend", "outline": [[36, 4], [104, 4], [104, 84], [36, 84]],
      "entrances": [{"name": "Mouth", "at": )" + entrance + R"(, "facing": -90}],
      "paths": [{"from": "Mouth", "line": )" + path + R"(}],
      "prize": {"deposits": [0, 1], "wells": [0]},
      "spots": )" + spots + "}";
}

RegionFile parsed(const std::string& text) {
    const Parsed p = parse(text);
    for (const Problem& q : p.problems) std::printf("    unexpected: %s\n", q.text().c_str());
    return p.file ? *p.file : RegionFile{};
}

std::vector<Problem> problemsOf(const std::string& text) {
    const RegionFile f = parsed(text);
    return validate(f, build(f));
}

bool has(const std::vector<Problem>& problems, const std::string& where, const std::string& what) {
    for (const Problem& p : problems)
        if (p.where.find(where) != std::string::npos && p.message.find(what) != std::string::npos) return true;
    std::printf("    no \"%s\" / \"%s\" among:\n", where.c_str(), what.c_str());
    for (const Problem& p : problems) std::printf("      %s\n", p.text().c_str());
    return false;
}

std::string read(const std::string& path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

/// Distance from `p` to a polyline.
double toLine(Vec2 p, const std::vector<Vec2>& line) {
    double d = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i + 1 < line.size(); i++) {
        const Vec2 a = line[i], e = line[i + 1] - a;
        const double t = std::clamp(dot(p - a, e) / std::max(dot(e, e), 1e-9), 0.0, 1.0);
        d = std::min(d, length(p - a - e * t));
    }
    return d;
}

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

constexpr PilotId me = 71;

WorldParams params() {
    WorldParams p;
    p.contracts = false;
    p.starfalls = false;
    p.firstStep = 0;
    p.brood = false;  // no director's groups: only the ones sent
    p.broodSeed = 0xc4a7ULL;
    return p;
}

void stepFor(WorldMap& w, double seconds) {
    const int n = int(seconds * 60);
    for (int i = 0; i < n; i++) w.step();
}

/// A world with the pilot's Prospector in Cold Fork's valley south of the
/// canyon, out of the prize's site.
size_t inColdFork(WorldMap& w) {
    w.join(me);
    w.step();
    const size_t r = *w.regionNamed("cold-fork");
    const Vec2 valley = w.region(r).places()->ground(Vec2(100, 52));
    w.devTransfer(me, r, valley);
    w.step();
    return r;
}

} // namespace

/// A cut is a canyon: its floor at its level along the line, the ground
/// beside it as it was, its walls a cliff; where it is two levels deep its
/// walls step in stretches (a shelf half way up); its floor a darker tone.
TEST(Zone_cutIsACanyon) {
    const RegionFile f = parsed(mesa());
    const Built b = build(f);
    const TerrainField field(b.map);
    const double lh = b.map.levelHeight;
    int shelves = 0, samples = 0;
    for (double z = 12; z <= 28; z += 0.5) {
        const Vec2 floor = b.ground(Vec2(60, z));
        EXPECT_TRUE(std::fabs(field.height(floor)) < 0.3);
        EXPECT_EQ(field.level(floor), int64_t(0));
        EXPECT_TRUE(std::fabs(field.height(b.ground(Vec2(60 + 10, z))) - 2 * lh) < 0.3);
        EXPECT_TRUE(field.materials(floor).w > 0.2);
        // Past the floor and its cliff, before the rim: a shelf, or the rim.
        for (const double side : {1.0, -1.0}) {
            const double h = field.height(b.ground(Vec2(60 + side * (3 + 1.6 + 0.6), z)));
            samples += 1;
            if (std::fabs(h - lh) < 0.4) shelves += 1;
        }
    }
    std::printf("  a shelf at %d of %d points along the wall\n", shelves, samples);
    EXPECT_TRUE(shelves > 0 && shelves < samples);
    // Not on the open ground it runs out onto (no cut there).
    EXPECT_TRUE(std::fabs(field.height(b.ground(Vec2(80, 78)))) < 0.3);
}

/// Zones go through the file and back as they were.
TEST(Zone_roundTrip) {
    const RegionFile f = parsed(mesa(zone(R"([[50, 30], [70, 26]])")));
    ASSERT_TRUE(f.zones.size() == size_t(1));
    const Zone& z = f.zones[0];
    EXPECT_EQ(z.id, std::string("bend"));
    EXPECT_EQ(z.entrances.size(), size_t(1));
    EXPECT_EQ(z.entrances[0].facing, -90.0);
    EXPECT_EQ(z.paths[0].from, std::string("Mouth"));
    EXPECT_EQ(z.paths[0].points.size(), size_t(4));
    EXPECT_EQ(z.prizeDeposits, (std::vector<int64_t>{0, 1}));
    EXPECT_EQ(z.prizeWells, (std::vector<int64_t>{0}));
    // Z4: an old file's build spots are read past and not written back.
    EXPECT_TRUE(write(f).find("spots") == std::string::npos);
    EXPECT_EQ(f.height[1].op, HeightEdit::Kind::cut);
    const Parsed again = parse(write(f));
    ASSERT_TRUE(again.file.has_value());
    EXPECT_TRUE(*again.file == f);
    // Built: in ground coordinates, a tunnel's mouth at the entrance.
    const Built b = build(f);
    ASSERT_TRUE(b.zones.size() == size_t(1));
    EXPECT_TRUE(distance(b.zones[0].entrances[0].at, b.ground(Vec2(60, 12))) < 1e-9);
    int tunnels = 0;
    for (const Doodad& d : b.map.doodads)
        if (d.kind == Doodad::Kind::broodTunnel) {
            tunnels += 1;
            EXPECT_TRUE(distance(d.position, b.ground(Vec2(60, 12))) < 1e-9);
        }
    EXPECT_EQ(tunnels, 1);
    // No build spot markers (Z4).
    for (const Doodad& d : b.map.doodads) EXPECT_TRUE(d.kind != Doodad::Kind::buildSpot);
    // The tunnel is walked through (no rock where the bugs come out).
    EXPECT_TRUE(NavGrid::footprint(Doodad::Kind::broodTunnel).empty());
}

/// What the parser refuses in a zone.
TEST(Zone_parseProblems) {
    const auto problems = [](const std::string& text) { return parse(text).problems; };
    EXPECT_TRUE(has(problems(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrances": [{"name": "A", "at": [60, 12]}], "paths": [{"from": "B", "line": [[60, 14], [60, 30]]}],
        "prize": {"deposits": [0]}})")), "zones[0].paths[0].from", "names no entrance"));
    EXPECT_TRUE(has(problems(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrances": [{"name": "A", "at": [60, 12]}], "paths": [{"from": "A", "line": [[60, 14], [60, 30]]}],
        "prize": {"deposits": [7]}})")), "zones[0].prize.deposits", "must be an index"));
    EXPECT_TRUE(has(problems(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrances": [{"name": "A", "at": [60, 12]}, {"name": "B", "at": [62, 12]}], "paths": [{"from": "A", "line": [[60, 14], [60, 30]]}],
        "prize": {"wells": [0]}})")), "zones[0].paths", "no path leaves entrance \"B\""));
    EXPECT_TRUE(has(problems(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrances": [{"name": "A", "at": [60, 12]}], "paths": [{"from": "A", "line": [[60, 14], [60, 30]]}],
        "prize": {}})")), "zones[0].prize", "needs a deposit or a well"));
    EXPECT_TRUE(has(problems(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrance": [], "paths": [], "prize": {"wells": [0]}})")), "zones[0]", "did you mean \"entrances\""));
    const std::string one = R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 64]],
        "entrances": [{"name": "A", "at": [60, 12]}], "paths": [{"from": "A", "line": [[60, 14], [60, 30]]}], "prize": {"wells": [0]}})";
    EXPECT_TRUE(has(problems(mesa(one + ", " + one)), "zones[1].id", "used twice"));
}

/// A clean zone validates; what the validator finds wrong in one.
TEST(Zone_validationProblems) {
    for (const Problem& p : problemsOf(mesa(zone()))) std::printf("    unexpected: %s\n", p.text().c_str());
    EXPECT_TRUE(problemsOf(mesa(zone())).empty());
    // Z4: an old file's spots, wherever they are, are no problem.
    EXPECT_TRUE(problemsOf(mesa(zone(R"([[45, 40], [78.5, 44], [63.6, 20]])"))).empty());
    // A path over the rim, not along the floor.
    EXPECT_TRUE(has(problemsOf(mesa(zone(R"([])", R"([[60, 14], [70, 20], [80, 49]])"))), "paths[0]", "not walkable end to end"));
    // A path that starts far from its entrance, or ends far from the prize.
    EXPECT_TRUE(has(problemsOf(mesa(zone(R"([])", R"([[60, 22], [60, 30], [80, 45], [80, 49]])"))), "paths[0]", "cells from its entrance"));
    EXPECT_TRUE(has(problemsOf(mesa(zone(R"([])", R"([[60, 14], [60, 30]])"))), "paths[0]", "from the prize's middle"));
    // An entrance out in the open.
    EXPECT_TRUE(has(problemsOf(mesa(zone(R"([])", R"([[92, 78], [80, 72], [80, 49]])", R"([92, 78])"))), "entrances[0]", "stands in the open"));
    // The prize outside the zone.
    EXPECT_TRUE(has(problemsOf(mesa(R"({"id": "z", "name": "Z", "outline": [[40, 4], [100, 4], [100, 40], [40, 40]],
        "entrances": [{"name": "Mouth", "at": [60, 12]}], "paths": [{"from": "Mouth", "line": [[60, 14], [60, 30], [70, 37]]}],
        "prize": {"deposits": [0]}})")), "zones[0] \"z\".prize", "outside the zone's outline"));
}

/// Cold Fork's canyon: The Fork, two tunnels whose paths wind down the
/// floor to the prize at the junction, a field richer than any other ore
/// in the region, all valid; the region's pad still reaches it.
TEST(Zone_coldForkHasTheFork) {
    const RegionFile f = parsed(read("regions/cold-fork.json"));
    ASSERT_TRUE(f.zones.size() == size_t(1));
    const Zone& z = f.zones[0];
    EXPECT_EQ(z.id, std::string("the-fork"));
    EXPECT_TRUE(z.entrances.size() >= 2 && z.entrances.size() <= 3);
    EXPECT_EQ(z.paths.size(), z.entrances.size());
    int64_t prizeLeast = std::numeric_limits<int64_t>::max(), elsewhereMost = 0;
    for (size_t i = 0; i < f.deposits.size(); i++) {
        const bool prize = std::find(z.prizeDeposits.begin(), z.prizeDeposits.end(), int64_t(i)) != z.prizeDeposits.end();
        if (prize) prizeLeast = std::min(prizeLeast, f.deposits[i].amount);
        else elsewhereMost = std::max(elsewhereMost, f.deposits[i].amount);
    }
    EXPECT_TRUE(z.prizeDeposits.size() >= 4);
    EXPECT_EQ(z.prizeWells.size(), size_t(1));
    EXPECT_TRUE(prizeLeast > elsewhereMost);
    const Built b = build(f);
    const std::vector<Problem> problems = validate(f, b);
    for (const Problem& p : problems) std::printf("    %s\n", p.text().c_str());
    EXPECT_TRUE(problems.empty());
}

/// A group told to walk a path follows it along the canyon's floor, from
/// its tunnel to the prize, and lingers there: both of The Fork's paths.
TEST(Zone_aGroupWalksItsPathToThePrize) {
    for (size_t path = 0; path < 2; path++) {
        WorldMap w(first(), params());
        const size_t r = inColdFork(w);
        const region::Zone& z = w.region(r).places()->zones.at(0);
        const std::vector<Vec2>& line = z.paths[path].points;
        const auto id = w.sendAlongPath(r, "the-fork", path, UnitKind::shardling, 5);
        ASSERT_TRUE(id.has_value());
        Vec2 prize(0, 0);
        const auto& m = w.region(r).places()->map;
        for (int64_t i : z.prizeDeposits) prize = prize + m.deposits[size_t(i)].position / double(z.prizeDeposits.size());
        double farthest = 0;
        double arrived = -1;
        const double from = w.clock();
        for (int s = 0; s < 150 * 4 && arrived < 0; s++) {
            stepFor(w, 0.25);
            int there = 0, alive = 0;
            for (const Unit& u : w.region(r).simulation().state.units) {
                if (!u.bug || u.bug->group != *id || u.hp <= 0) continue;
                alive += 1;
                farthest = std::max(farthest, toLine(u.position, line));
                if (distance(u.position, prize) <= region::prizeReach) there += 1;
            }
            ASSERT_TRUE(alive == 5);
            if (there == alive) arrived = w.clock() - from;
        }
        double length = 0;
        for (size_t k = 0; k + 1 < line.size(); k++) length += distance(line[k], line[k + 1]);
        std::printf("  path %zu (%s, %.0f cells): all within %.0f cells of the prize after %.1f world s, at most %.1f cells off the path\n",
                    path, z.paths[path].from.c_str(), length, region::prizeReach, arrived, farthest);
        EXPECT_TRUE(arrived > 0);
        EXPECT_TRUE(farthest < 4.0);
        // They linger there: still at the prize a while later.
        stepFor(w, 10);
        for (const Unit& u : w.region(r).simulation().state.units)
            if (u.bug && u.bug->group == *id) EXPECT_TRUE(distance(u.position, prize) <= region::prizeReach + 4);
    }
}

/// A Lodeback sent down a path mines the prize for itself.
TEST(Zone_aLodebackMinesThePrize) {
    WorldMap w(first(), params());
    const size_t r = inColdFork(w);
    const auto id = w.sendAlongPath(r, "the-fork", 1, UnitKind::lodeback, 1);
    ASSERT_TRUE(id.has_value());
    const GameState& st = w.region(r).simulation().state;
    int64_t before = 0;
    for (const OreDeposit& d : st.patches) before += d.remaining;
    bool ate = false;
    for (int s = 0; s < 150 && !ate; s++) {
        stepFor(w, 1);
        for (const Unit& u : st.units)
            if (u.bug && u.bug->group == *id && u.bug->sac > 5) ate = true;
    }
    int64_t after = 0;
    for (const OreDeposit& d : st.patches) after += d.remaining;
    std::printf("  ore in the region %lld -> %lld\n", (long long)before, (long long)after);
    EXPECT_TRUE(ate);
    EXPECT_TRUE(after < before);
}

/// A save from before the canyon: the north-east vein's deposits keep their
/// indices and their ore (moved down into the valley), the prize comes in
/// full, a Prospector saved where the canyon's wall now stands is put on
/// open ground.
TEST(Zone_aSaveFromBeforeTheCanyonLoads) {
    const std::string key = "00112233445566778899aabbccddeef0";
    WorldSave s;
    SavedRegion r;
    r.id = "cold-fork";
    r.time = 10;
    r.nextId = 20;
    // Region (116, 20): before the canyon low ground, now the rim's wall.
    r.units.push_back(SavedUnit{7, UnitKind::prospector, Vec2(116 - 72, 20 - 48), 0, 60, 0, false, key});
    r.deposits.push_back(SavedDeposit{33, 700, std::nullopt});
    r.wells.push_back(SavedWell{3, 1000});
    s.regions.push_back(r);
    s.pilots.push_back(SavedPilot{key, 100, 0, "cold-fork", 7, "", ""});
    WorldMap w(first());
    const auto problems = w.restore(s);
    for (const std::string& p : problems) std::printf("    %s\n", p.c_str());
    EXPECT_TRUE(problems.empty());
    const size_t cf = *w.regionNamed("cold-fork");
    const GameState& st = w.region(cf).simulation().state;
    ASSERT_TRUE(st.patches.size() > 33);
    EXPECT_EQ(st.patches[33].remaining, int64_t(700));
    EXPECT_TRUE(distance(st.patches[33].position, Vec2(113.9 - 72, 39.1 - 48)) < 12);
    const region::Zone& z = w.region(cf).places()->zones.at(0);
    for (int64_t i : z.prizeDeposits) {
        const Vec2 at = w.region(cf).places()->map.deposits[size_t(i)].position;
        for (const OreDeposit& d : st.patches)
            if (distance(d.position, at) < 0.01) EXPECT_EQ(d.remaining, int64_t(6000));
    }
    EXPECT_EQ((*st.wells)[3].remaining, int64_t(1000));
    EXPECT_EQ((*st.wells)[size_t(z.prizeWells[0])].remaining, int64_t(6000));
    ASSERT_TRUE(w.seat(3, key) == WorldMap::Seated::restored);
    const Simulation& sim = w.region(cf).simulation();
    for (const Unit& u : sim.state.units)
        if (u.id == 7) {
            std::printf("  saved at a wall now; stands %.1f cells off, on open ground\n", distance(u.position, Vec2(116 - 72, 20 - 48)));
            EXPECT_TRUE(sim.pilotCanStand(u, u.position));
        }
}
