// The world (world/WorldMap.h): the world file and its checks, the pilot at
// the start pad, edges, flights, the wallet, and a client following its
// pilot into another region. Isolated scenes of a few seconds; run from
// unreal/server (as `make test` does).

#include "test.h"

#include "Flight.h"
#include "HeadlessClient.h"
#include "Progression.h"
#include "NetPredictor.h"
#include "Server.h"
#include "TerrainField.h"
#include "WorldMap.h"
#include "rig.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

namespace {

using namespace ac;
using namespace ac::server;
using region::Edge;

constexpr PilotId me = 7;

const LoadedWorld& first() {
    static const LoadedWorld w = [] {
        WorldLoad l = loadWorld("worlds/first.json");
        for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
        return *l.world;
    }();
    return w;
}

/// The world's own regions in a grid of our own.
WorldLoad gridOf(const std::string& grid, const std::string& start = R"({"region": "start", "pad": "landing"})") {
    return loadWorld(R"({"id": "t", "name": "T", "grid": )" + grid + R"(, "start": )" + start + "}", "regions");
}

bool mentions(const WorldLoad& l, const std::string& what) {
    for (const std::string& p : l.problems)
        if (p.find(what) != std::string::npos) return true;
    return false;
}

/// A scratch folder for broken region files.
std::string scratch(const std::string& name, const std::string& text) {
    mkdir("build/world-tests", 0755);
    const std::string path = "build/world-tests/" + name;
    if (FILE* f = std::fopen(path.c_str(), "w")) {
        std::fputs(text.c_str(), f);
        std::fclose(f);
    }
    return path;
}

Unit* drivenUnit(WorldMap& w, PilotId p) {
    Region* r = w.regionOf(p);
    if (!r) return nullptr;
    const auto id = r->driving(p);
    if (!id) return nullptr;
    const auto i = r->simulation().unitIndex(*id);
    return i ? &r->simulation().state.units[size_t(*i)] : nullptr;
}

Vec2 regionPos(WorldMap& w, PilotId p) { return w.regionOf(p)->places()->regionAt(drivenUnit(w, p)->position); }

/// Puts the driven unit at a region position, facing `heading`.
void put(WorldMap& w, PilotId p, Vec2 at, double heading) {
    Unit* u = drivenUnit(w, p);
    u->position = w.regionOf(p)->places()->ground(at);
    u->heading = heading;
}

int64_t inputTick = 0;

/// Drives `ticks` ticks with the walk at `heading`, or until `done`.
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

double headingOut(Edge e) {
    switch (e) {
    case Edge::east: return 0;
    case Edge::south: return pi / 2;
    case Edge::west: return pi;
    case Edge::north: return -pi / 2;
    }
    return 0;
}

/// The unit's distance to an edge, and where along it, region cells.
std::pair<double, double> edgeGap(Vec2 at, Vec2 ext, Edge e) {
    switch (e) {
    case Edge::north: return {at.y, at.x};
    case Edge::south: return {ext.y - at.y, at.x};
    case Edge::west: return {at.x, at.y};
    case Edge::east: return {ext.x - at.x, at.y};
    }
    return {0, 0};
}

/// A point `depth` cells in from an edge at `along`.
Vec2 inFrom(Vec2 ext, Edge e, double along, double depth) {
    switch (e) {
    case Edge::north: return Vec2(along, depth);
    case Edge::south: return Vec2(along, ext.y - depth);
    case Edge::west: return Vec2(depth, along);
    case Edge::east: return Vec2(ext.x - depth, along);
    }
    return Vec2(0, 0);
}

Edge opposite(Edge e) {
    return e == Edge::north ? Edge::south : e == Edge::south ? Edge::north : e == Edge::east ? Edge::west : Edge::east;
}

/// The ground point of the first Landing Pad in region `r` (F2).
Vec2 padIn(const WorldMap& w, size_t r) {
    for (const LandingPad& o : w.landingPads())
        if (o.region == r) return o.at;
    return Vec2(0, 0);
}

size_t unitsEverywhere(const WorldMap& w) {
    size_t n = 0;
    for (size_t i = 0; i < w.regionCount(); i++) n += w.region(i).simulation().state.units.size();
    return n;
}

} // namespace

// MARK: - The world file

TEST(world_first_loads) {
    WorldLoad l = loadWorld("worlds/first.json");
    for (const std::string& p : l.problems) std::printf("  %s\n", p.c_str());
    ASSERT_TRUE(l.world.has_value());
    EXPECT_EQ(l.world->regions.size(), size_t(9));
    EXPECT_TRUE(l.world->regions[l.world->start].file.id == "start");
    EXPECT_TRUE(l.world->regions[l.world->start].file.pads[l.world->startPad].name == "landing");
    EXPECT_EQ(l.world->regions[l.world->start].column, int64_t(1));
    EXPECT_EQ(l.world->regions[l.world->start].row, int64_t(1));
}

TEST(world_load_refuses_each_problem) {
    EXPECT_TRUE(mentions(loadWorld("{nope", "regions"), "not a JSON object"));
    EXPECT_TRUE(gridOf(R"([["start.json"]])").world.has_value());
    EXPECT_TRUE(mentions(loadWorld(R"({"id": "t", "grid": [["start.json"]], "start": {"region": "start", "pad": "landing"}, "colour": 1})", "regions"),
                         "colour: unknown key"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "nowhere.json"]])"), "nowhere.json: cannot read"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "glass-flats.json"], ["brine-pans.json"]])"), "row 1 is not as long"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "start.json"]])"), "used twice"));
    EXPECT_TRUE(mentions(gridOf(R"([["glass-flats.json"]])"), "no region start"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json"]])", R"({"region": "start", "pad": "dock"})"), "has no pad dock"));
    // A region file that does not parse, one with a problem, one without a pad.
    scratch("broken.json", R"({"id": "broken", "name": "B", "size": "medium", "base": {"style": "openField", "seed": 3}, "padz": []})");
    scratch("rocky.json", R"({"id": "rocky", "name": "R", "size": "medium", "base": {"style": "openField", "seed": 3},
        "doodads": [{"kind": "boulder", "at": [72, 48], "scale": 3}], "pads": [{"name": "landing", "at": [72, 48]}]})");
    scratch("padless.json", R"({"id": "padless", "name": "P", "size": "medium", "base": {"style": "openField", "seed": 3}})");
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "../build/world-tests/broken.json"]])"), "padz"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "../build/world-tests/rocky.json"]])"), "pads[0]"));
    EXPECT_TRUE(mentions(gridOf(R"([["start.json", "../build/world-tests/padless.json"]])"), "no pad"));
    // Openings that miss each other: the neighbour opens its west edge low.
    scratch("missing.json", R"({"id": "missing", "name": "M", "size": "medium", "base": {"style": "openField", "seed": 3},
        "pads": [{"name": "landing", "at": [72, 48]}], "openings": {"west": [[70, 90]]}})");
    const WorldLoad miss = gridOf(R"([["start.json", "../build/world-tests/missing.json"]])");
    EXPECT_TRUE(mentions(miss, "start east opening [41, 55] meets no opening of missing's west edge"));
    EXPECT_TRUE(mentions(miss, "missing west opening [70, 90] meets no opening of start's east edge"));
    EXPECT_TRUE(!miss.world.has_value());
    // W4: beside a region with walls, a passage lines up exactly (span and
    // floor level), and the ground meets along the whole shared edge.
    scratch("askew.json", R"({"id": "askew", "name": "A", "size": "medium", "base": {"style": "openField", "seed": 3},
        "pads": [{"name": "landing", "at": [72, 48]}], "openings": {"west": [[44, 58]]}})");
    const WorldLoad askew = gridOf(R"([["start.json", "../build/world-tests/askew.json"]])");
    EXPECT_TRUE(mentions(askew, "start east opening [41, 55] does not line up with askew's west openings"));
    EXPECT_TRUE(mentions(askew, "start east edge and askew's west edge do not meet"));
    EXPECT_TRUE(!askew.world.has_value());
}

// MARK: - Pilots

TEST(world_pilot_starts_at_the_pad_in_a_prospector_with_600_ore) {
    WorldMap w(first());
    ASSERT_TRUE(w.join(me));
    EXPECT_EQ(*w.where(me), first().start);
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(u->kind == UnitKind::prospector);
    EXPECT_EQ(u->owner, Pilot::player);
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    EXPECT_TRUE(distance(u->position, pad) < 1.0);
    EXPECT_EQ(w.wallet(me)->ore, int64_t(600));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(0));
    EXPECT_EQ(w.start().simulation().state.players[0].ore, int64_t(600));
}

TEST(world_no_units_but_the_pilots_anywhere) {
    WorldMap w(first());
    w.join(me);
    for (int i = 0; i < 120; i++) w.step();
    EXPECT_EQ(unitsEverywhere(w), size_t(1));
    for (size_t i = 0; i < w.regionCount(); i++) {
        const GameState& s = w.region(i).simulation().state;
        EXPECT_TRUE(s.structures.empty());
        // The pilots' side, and the Brood's where the region has a brood level.
        EXPECT_EQ(s.players.size(), size_t(first().regions[i].file.brood > 0 ? 2 : 1));
        EXPECT_TRUE(!s.endedAt.has_value());
        for (const Unit& u : s.units) EXPECT_TRUE(u.kind == UnitKind::prospector);
    }
}

TEST(world_edges_hand_the_prospector_to_the_neighbour) {
    const std::pair<Edge, const char*> cases[] = {
        {Edge::east, "tern-ridge"}, {Edge::west, "rust-hollow"}, {Edge::north, "cold-fork"}, {Edge::south, "brine-pans"}};
    for (const auto& [edge, name] : cases) {
        WorldMap w(first());
        w.join(me);
        const Vec2 ext = w.start().places()->extent();
        const auto& span = w.start().places()->openings[size_t(edge)][0];
        const double along = (span.from + span.to) / 2 + 1.3;
        put(w, me, inFrom(ext, edge, along, 4), headingOut(edge));
        Unit* u = drivenUnit(w, me);
        u->carrying = 6;
        u->hp = 31;
        Player& side = w.start().simulation().state.players[0];
        side.ore = 777;
        side.pilot = PilotRecord{};
        side.pilot->kinds[UnitKind::prospector].xp = 123;
        const bool crossed = drive(w, me, headingOut(edge), 180, [&] { return w.visit(me) == 1; });
        std::printf("  %s: crossed %s\n", std::string(region::edgeName(edge)).c_str(), crossed ? "yes" : "no");
        ASSERT_TRUE(crossed);
        ASSERT_TRUE(w.where(me) == w.regionNamed(name));
        const Unit* v = drivenUnit(w, me);
        ASSERT_TRUE(v != nullptr);
        const Vec2 there = regionPos(w, me);
        const auto [gap, at] = edgeGap(there, w.regionOf(me)->places()->extent(), opposite(edge));
        // W4: at the same point of the world, just past the edge it crossed.
        EXPECT_TRUE(gap >= 0 && gap < 0.2);
        EXPECT_NEAR(at, along, 0.3);
        EXPECT_EQ(v->carrying, int64_t(6));
        EXPECT_NEAR(v->hp, 31.0, 1e-9);
        EXPECT_TRUE(v->kind == UnitKind::prospector);
        // The wallet and the levels came along; the old region keeps nothing.
        EXPECT_EQ(w.wallet(me)->ore, int64_t(777));
        const Player& now = w.regionOf(me)->simulation().state.players[0];
        EXPECT_EQ(now.ore, int64_t(777));
        ASSERT_TRUE(now.pilot.has_value());
        EXPECT_NEAR(now.pilot->kinds.at(UnitKind::prospector).xp, 123.0, 1e-9);
        EXPECT_EQ(w.start().simulation().state.players[0].ore, int64_t(0));
        EXPECT_TRUE(w.start().simulation().state.units.empty());
        EXPECT_TRUE(!w.start().joined(me));
        EXPECT_EQ(unitsEverywhere(w), size_t(1));
        const auto moves = w.takeMoves();
        ASSERT_TRUE(moves.size() == 1);
        ASSERT_TRUE(moves[0].change.has_value());
        EXPECT_TRUE(moves[0].change->regionId == name);
        EXPECT_EQ(moves[0].change->visit, int64_t(1));
        EXPECT_TRUE(moves[0].change->driven == w.regionOf(me)->driving(me));
        EXPECT_TRUE(!moves[0].change->region.regionFile.empty());
        // And it drives on there.
        const Vec2 from = regionPos(w, me);
        drive(w, me, headingOut(edge), 30, [] { return false; });
        EXPECT_TRUE(distance(regionPos(w, me), from) > 1);
    }
}

// W4: a seamless crossing. Driven out of each edge's passage with the walk
// held, its inputs still stamped with the visit before (the client has not
// heard yet), the unit crosses at the same point of the world and walks on at
// the same pace: every tick's step on the world's grid within 2% of the
// steps before, no jump, no stop, its heading kept.
TEST(world_crossing_keeps_position_and_speed) {
    for (const Edge edge : {Edge::east, Edge::west, Edge::north, Edge::south}) {
        WorldMap w(first());
        w.join(me);
        const Vec2 ext = w.start().places()->extent();
        const auto& span = w.start().places()->openings[size_t(edge)][0];
        put(w, me, inFrom(ext, edge, (span.from + span.to) / 2, 6), headingOut(edge));
        // A point of the world: the region's place on the grid plus its cells.
        const auto world = [&] {
            const WorldRegion& g = first().regions[*w.where(me)];
            return Vec2(double(g.column) * ext.x, double(g.row) * ext.y) + regionPos(w, me);
        };
        std::vector<double> steps;
        Vec2 last = world();
        int64_t crossedAt = -1;
        for (int i = 0; i < 150 && (crossedAt < 0 || i < crossedAt + 40); i++) {
            PilotInput in;
            in.tick = ++inputTick;
            in.visit = 0;
            in.walk = Vec2(1, 0);
            in.heading = headingOut(edge);
            w.input(me, in);
            w.step();
            if (crossedAt < 0 && w.visit(me) == 1) crossedAt = int64_t(steps.size());
            const Vec2 now = world();
            steps.push_back(distance(now, last));
            last = now;
        }
        ASSERT_TRUE(crossedAt > 10);
        // The pace before the crossing, and every step round it and after.
        double pace = 0;
        for (int64_t i = crossedAt - 10; i < crossedAt; i++) pace += steps[size_t(i)] / 10;
        double lo = 1e9, hi = 0;
        for (int64_t i = crossedAt - 5; i < int64_t(steps.size()); i++) {
            lo = std::min(lo, steps[size_t(i)]);
            hi = std::max(hi, steps[size_t(i)]);
        }
        std::printf("  %s: crossed at tick %lld, pace %.4f cells a tick, steps %.4f…%.4f round and after it\n",
                    std::string(region::edgeName(edge)).c_str(), (long long)crossedAt, pace, lo, hi);
        EXPECT_TRUE(pace > 0.01);
        EXPECT_TRUE(lo > pace * 0.98 && hi < pace * 1.02);
        EXPECT_NEAR(drivenUnit(w, me)->heading, headingOut(edge), 1e-6);
    }
}

TEST(world_closed_edges_stop_it) {
    // Start and Glass Flats side by side: Start's north edge is the world's
    // border (its passage there leads nowhere: the unit stands in it, past the
    // edge by the overhang at most), and its east edge is walled outside its
    // passage (W4: the wall stops it well before the edge).
    WorldLoad l = gridOf(R"([["start.json", "glass-flats.json"]])");
    ASSERT_TRUE(l.world.has_value());
    struct Case {
        Edge edge;
        double along;
    };
    for (const Case c : {Case{Edge::north, 72}, Case{Edge::east, 22}}) {
        WorldMap w(*l.world);
        w.join(me);
        const Vec2 ext = w.start().places()->extent();
        put(w, me, inFrom(ext, c.edge, c.along, 24), headingOut(c.edge));
        double closest = 99;
        drive(w, me, headingOut(c.edge), 420, [&] {
            closest = std::min(closest, edgeGap(regionPos(w, me), ext, c.edge).first);
            return w.visit(me) > 0;
        });
        std::printf("  %s at %.0f: came within %.2f cells of the edge\n", std::string(region::edgeName(c.edge)).c_str(), c.along, closest);
        EXPECT_EQ(w.visit(me), int64_t(0));
        EXPECT_EQ(*w.where(me), l.world->start);
        EXPECT_TRUE(closest >= -RegionEdges::overhang - 1e-9);
        if (c.edge == Edge::east) EXPECT_TRUE(closest > 2);
        else EXPECT_TRUE(closest < 0);
    }
}

// A flight's MH (F1, docs/prospector-progression.md "MH sinks"): a base and
// a share a grid step, taken at the call; refused with the price and the
// wallet when it is short, nothing taken and nothing flown (F2: between
// Landing Pads, the target region seen).
TEST(world_flight_costs_mh) {
    WorldParams fuel;
    fuel.startHydrogen = 14;
    fuel.devSeen = {"cold-fork"};
    WorldMap w(first(), fuel);
    w.join(me);
    const size_t fork = *w.regionNamed("cold-fork");
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    const auto here = w.devLandingPad(first().start, pad + Vec2(0, 10));
    ASSERT_TRUE(here.has_value() && w.devLandingPad(fork, first().regions[fork].built.pads[0].at + Vec2(0, 10)).has_value());
    const Vec2 hereAt = padIn(w, first().start);
    put(w, me, w.start().places()->regionAt(hereAt), 0);
    const WorldRegion& from = first().regions[first().start];
    const WorldRegion& to = first().regions[fork];
    const int64_t steps = Flight::steps(from.column, from.row, to.column, to.row);
    EXPECT_EQ(steps, int64_t(1));
    EXPECT_EQ(Flight::hydrogen(steps), int64_t(15));
    EXPECT_TRUE(w.fly(me, "cold-fork", "Landing Pad 1") == std::optional<std::string>("Not enough MH to fly: 15 MH, you have 14"));
    EXPECT_TRUE(!w.flying(me));
    EXPECT_EQ(w.start().simulation().state.players[0].hydrogen, int64_t(14));
    // Paid at the call, the rest lands with it.
    w.start().simulation().state.players[0].hydrogen = 40;
    ASSERT_TRUE(!w.fly(me, "cold-fork", "Landing Pad 1"));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(25));
    for (int i = 0; i < 60 * 30 && w.flying(me); i++) w.step();
    ASSERT_TRUE(!w.flying(me));
    EXPECT_EQ(w.regionOf(me)->simulation().state.players[0].hydrogen, int64_t(25));
}

/// F1b: with Cliff hop the pilot's jet pack burns 2 MH a jump from the
/// region's wallet; short, it stays at the cliff.
TEST(world_cliff_hop_burns_mh) {
    WorldMap w(first());
    w.join(me);
    ProspectorProgress pr;
    pr.xp = Progression::xp(Progression::cap);
    pr.ranks = {{Skill::cliffHop, 1}};
    w.setProgress(me, pr);
    Simulation& sim = w.regionOf(me)->simulation();
    Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u && sim.nav);
    // A spot on walkable ground just short of a cliff face it can jump.
    std::optional<std::pair<Vec2, double>> edge;
    const GroundRect b = sim.map->bounds;
    for (double x = b.minX + 2; x < b.maxX - 2 && !edge; x += 1)
        for (double y = b.minZ + 2; y < b.maxZ - 2 && !edge; y += 1) {
            const Vec2 p(x, y);
            if (!sim.pilotCanStand(*u, p)) continue;
            for (int j = 0; j < 16 && !edge; ++j) {
                const double a = j / 16.0 * 2 * pi;
                const Vec2 dir(std::cos(a), std::sin(a));
                Vec2 q = p;
                while (sim.pilotCanStand(*u, q) && distance(q, p) < 1) q += dir * 0.05;
                if (!(sim.nav->cliff(q) && distance(q, p) > 0.4)) continue;
                const auto land = sim.landing(q, q + dir * Simulation::pilotJumpReach);
                if (land && sim.pilotCanStand(*u, *land)) edge = std::make_pair(q - dir * 0.3, a);
            }
        }
    ASSERT_TRUE(edge.has_value());  // no cliff in the start region
    u->position = edge->first;
    u->heading = edge->second;
    sim.state.players[0].hydrogen = 1;
    const auto jumped = [&] { return drivenUnit(w, me)->jumpFrom.has_value(); };
    EXPECT_TRUE(!drive(w, me, edge->second, 20, jumped));
    EXPECT_EQ(sim.state.players[0].hydrogen, int64_t(1));
    EXPECT_TRUE(sim.pilotJumpDry.has_value());
    drivenUnit(w, me)->position = edge->first;
    sim.state.players[0].hydrogen = 5;
    EXPECT_TRUE(drive(w, me, edge->second, 20, jumped));
    EXPECT_EQ(sim.state.players[0].hydrogen, int64_t(3));
}

// F2: the Dropship from a Landing Pad to one in another region: the cargo
// and the wallet go with the pilot, the Prospector out of every region
// aboard, the region change at the approach (nothing driven yet), the
// Prospector on the pad at the touch down.
TEST(world_flight_from_pad_to_pad) {
    WorldParams fuel;
    fuel.startHydrogen = 100;  // a flight costs MH (F1)
    fuel.devSeen = {"cold-fork"};
    WorldMap w(first(), fuel);
    w.join(me);
    const size_t fork = *w.regionNamed("cold-fork");
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    ASSERT_TRUE(w.devLandingPad(first().start, pad + Vec2(0, 10)).has_value());
    ASSERT_TRUE(w.devLandingPad(fork, first().regions[fork].built.pads[0].at + Vec2(0, 10)).has_value());
    const Vec2 there = padIn(w, fork);
    // Away from the pad: refused; a region file's pad is no Landing Pad.
    put(w, me, w.start().places()->regionAt(padIn(w, first().start)) + Vec2(-12, 0), 0);
    EXPECT_TRUE(w.fly(me, "cold-fork", "Landing Pad 1") ==
                std::optional<std::string>("Not at a Landing Pad: stand on one to call the Dropship"));
    put(w, me, w.start().places()->regionAt(padIn(w, first().start)) + Vec2(-2.5, 0), 0);
    EXPECT_TRUE(w.fly(me, "nowhere", "Landing Pad 1").has_value());
    EXPECT_TRUE(w.fly(me, "cold-fork", "landing").has_value());
    drivenUnit(w, me)->carrying = 4;
    w.start().simulation().state.players[0].ore = 555;
    w.step();
    w.takeMoves();
    ASSERT_TRUE(!w.fly(me, "cold-fork", "Landing Pad 1").has_value());
    EXPECT_TRUE(w.flying(me));
    EXPECT_EQ(unitsEverywhere(w), size_t(0));
    EXPECT_TRUE(w.fly(me, "cold-fork", "Landing Pad 1") == std::optional<std::string>("Aboard the Dropship already"));
    const Ferry::Plan plan = Ferry::plan(1);
    const int64_t switchTicks = std::llround(plan.approachAt() * 60), landTicks = std::llround(plan.total() * 60);
    for (int64_t i = 0; i < switchTicks - 1; i++) w.step();
    EXPECT_EQ(*w.where(me), first().start);
    w.step();
    EXPECT_TRUE(w.flying(me));
    ASSERT_TRUE(w.where(me) == w.regionNamed("cold-fork"));
    EXPECT_EQ(w.visit(me), int64_t(1));
    EXPECT_TRUE(drivenUnit(w, me) == nullptr);
    EXPECT_EQ(w.regionOf(me)->simulation().state.players[0].ore, int64_t(555));
    EXPECT_EQ(w.start().simulation().state.players[0].ore, int64_t(0));
    for (int64_t i = switchTicks; i < landTicks; i++) w.step();
    EXPECT_TRUE(!w.flying(me));
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(distance(u->position, there) < 1.5);
    EXPECT_EQ(u->carrying, int64_t(4));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(555));
    EXPECT_EQ(unitsEverywhere(w), size_t(1));
    // The call, the region change (at the approach) and the touch down told.
    const auto moves = w.takeMoves();
    ASSERT_TRUE(moves.size() == 3);
    EXPECT_TRUE(!moves[0].change.has_value());
    ASSERT_TRUE(moves[1].change.has_value());
    EXPECT_TRUE(moves[1].change->regionId == "cold-fork" && !moves[1].change->driven);
    EXPECT_TRUE(!moves[2].change.has_value());
}

TEST(world_message_has_the_grid_and_where_the_pilot_is) {
    WorldMap w(first());
    w.join(me);
    const auto m = w.worldMessage(me);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->columns, int64_t(3));
    EXPECT_EQ(m->rows, int64_t(3));
    EXPECT_EQ(m->regions.size(), size_t(9));
    EXPECT_TRUE(m->regions[size_t(m->here)].id == "start");
    EXPECT_TRUE(m->regions[size_t(m->here)].pads[0].name == "landing");
    EXPECT_TRUE(!m->flyingTo.has_value());
    // The M1 server's one region is no world.
    EXPECT_TRUE(!WorldMap(RegionConfig{}).worldMessage(me).has_value());
}

// MARK: - Over the network

TEST(world_client_follows_a_region_change) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    net::LagSimulator lag(clientPort, 5);
    net::LagSettings rough;
    rough.delay = 0.15;
    rough.jitter = 0.02;
    rough.loss = 0.05;
    rough.reorder = 0.05;
    lag.setDefault(rough);
    ServerOptions o;
    o.world = first();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(lag, nettest::clientConfig());
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
    client.connect(now);
    client.join(net::newPilotKey());
    ASSERT_TRUE(until([&] { return client.mirror() && client.mirror()->driven() && client.world(); }, 10));
    EXPECT_TRUE(client.mirror()->matches());
    EXPECT_TRUE(client.world()->regions[size_t(client.world()->here)].id == "start");
    const PilotId pilot = server.world().start().pilots().front();
    // Out of the east opening.
    WorldMap& w = server.world();
    const Vec2 ext = w.start().places()->extent();
    put(w, pilot, inFrom(ext, Edge::east, 48, 3), 0);
    client.steer.walk = Vec2(1, 0);
    client.steer.heading = 0;
    ASSERT_TRUE(until([&] { return client.visit() == 1; }, 10));
    client.steer.walk = Vec2(0, 0);
    EXPECT_TRUE(client.regionChange()->regionId == "tern-ridge");
    const Region& there = *w.regionOf(pilot);
    EXPECT_TRUE(client.mirror()->region() == there.description());
    EXPECT_TRUE(client.mirror()->matches());
    // The wallet the client may show (docs/mmorpg-m5.md, E3): the new
    // mirror's players only once a snapshot brought them, and then the
    // pilot's 600, never the map's start values.
    int64_t lowest = std::numeric_limits<int64_t>::max();
    until([&] {
        const net::Mirror& m = *client.mirror();
        if (m.playersKnown()) lowest = std::min(lowest, m.simulation().state.players[0].ore);
        return false;
    }, 1);
    EXPECT_EQ(lowest, int64_t(600));
    // The mirror becomes the new region's: its one unit, driven, where it is.
    const auto same = [&] {
        const net::Mirror& m = *client.mirror();
        const GameState& truth = there.simulation().state;
        if (!m.driven() || m.driven() != there.driving(pilot) || m.simulation().state.units.size() != truth.units.size()) return false;
        const Unit* u = m.unit(*m.driven());
        return u && *u == net::quantised(truth.units[0]) && m.simulation().state.players[0].ore == truth.players[0].ore;
    };
    EXPECT_TRUE(until(same, 5));
    EXPECT_TRUE(until([&] { return client.world()->regions[size_t(client.world()->here)].id == "tern-ridge"; }, 3));
    EXPECT_EQ(client.mirror()->simulation().state.players[0].ore, int64_t(600));
}

// Problem 9 of docs/mmorpg-shots.md: crossing an edge with the walk held, on
// a lagged link and a client reading only every 0.2 s (a busy machine's
// frames). W4: the inputs it sent before it heard of the RegionChange still
// walk the unit on (no input lost at a seamless edge); once the client hears
// of it, it lets go and the Prospector stops. The client's prediction starts
// over on the new region's unit (its visit), so it draws it where the server
// has it, and nothing drifts after.
TEST(world_crossing_with_the_walk_held_walks_on_then_stops) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    net::LagSimulator lag(clientPort, 9);
    net::LagSettings rough;
    rough.delay = 0.15;
    rough.jitter = 0.02;
    lag.setDefault(rough);
    ServerOptions o;
    o.world = first();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(lag, nettest::clientConfig());
    net::Predictor predictor;
    double now = 100;
    int64_t frame = 0;
    int readEvery = 1;
    WorldMap& w = server.world();
    std::optional<Vec2> arrival;
    PilotId pilot = 0;
    // The game's client tick: what came (only on a frame it reads), the
    // prediction put right, the input sent and predicted at once.
    auto tick = [&] {
        now += 1.0 / 60;
        server.tick(now);
        if (pilot && !arrival && w.visit(pilot) == 1) arrival = regionPos(w, pilot);
        if (++frame % readEvery == 0) {
            client.receive(now);
            if (client.visit() >= 1) client.steer.walk = Vec2(0, 0);
            if (client.mirror()) predictor.reconcile(*client.mirror());
        }
        if (const auto in = client.sendInput(); in && client.mirror()) predictor.input(*client.mirror(), *in);
        client.flush(now);
        predictor.ease(1.0 / 60);
    };
    auto until = [&](auto&& done, double seconds) {
        for (int i = 0; i < int(seconds * 60); i++) {
            tick();
            if (done()) return true;
        }
        return false;
    };
    client.connect(now);
    client.join(net::newPilotKey());
    ASSERT_TRUE(until([&] { return client.mirror() && client.mirror()->driven() && client.world(); }, 10));
    pilot = w.start().pilots().front();
    // Out of the east opening, the walk held; slow frames from near the edge.
    const Vec2 ext = w.start().places()->extent();
    put(w, pilot, inFrom(ext, Edge::east, 48, 4), 0);
    client.steer.walk = Vec2(1, 0);
    client.steer.heading = 0;
    readEvery = 12;
    ASSERT_TRUE(until([&] { return client.visit() == 1; }, 10));
    ASSERT_TRUE(arrival.has_value());
    // Settled: the server's unit where it came in, the client's drawn there.
    until([] { return false; }, 1.5);
    const Vec2 there = regionPos(w, pilot);
    const Unit* truth = drivenUnit(w, pilot);
    ASSERT_TRUE(truth != nullptr && predictor.predicted() != nullptr);
    const double drift = distance(there, *arrival);
    const double drawn = distance(predictor.shown()->position, truth->position);
    std::printf("  came in at %.2f,%.2f; %.3f cells later; drawn %.4f cells from the server's\n", arrival->x, arrival->y, drift, drawn);
    EXPECT_TRUE(drift > 0.05);
    EXPECT_TRUE(drawn < 0.01);
    EXPECT_TRUE(distance(predictor.predicted()->position, truth->position) < 0.01);
    // And no drift follows.
    until([] { return false; }, 1);
    EXPECT_TRUE(distance(regionPos(w, pilot), there) < 1e-9);
    EXPECT_TRUE(distance(predictor.shown()->position, drivenUnit(w, pilot)->position) < 0.01);
}

// W4: the client's side of a seamless crossing. The walk held east out of
// Prospect Landing into Tern Ridge on a lagged link: the neighbour's mirror
// made ahead (`Client::prepare`) is the one the RegionChange swaps in, the
// prediction carries over (`Predictor::carryOver`), and the unit as drawn,
// on the world's grid, moves on every frame at the same pace: no jump, no
// stop, through the change and the new region's first snapshot.
TEST(world_client_draws_the_crossing_without_a_seam) {
    nettest::MemoryNet network;
    nettest::MemoryPort serverPort(network, nettest::kServerAddress), clientPort(network, nettest::kClientAddress);
    net::LagSimulator lag(clientPort, 9);
    net::LagSettings link;
    link.delay = 0.03;
    link.jitter = 0.005;
    lag.setDefault(link);
    ServerOptions o;
    o.world = first();
    o.token = nettest::kToken;
    o.secret = nettest::serverConfig().secret;
    Server server(serverPort, o);
    HeadlessClient client(lag, nettest::clientConfig());
    net::Predictor predictor;
    double now = 100;
    const auto tern = std::find_if(first().regions.begin(), first().regions.end(), [](const WorldRegion& g) { return g.file.id == "tern-ridge"; });
    const Vec2 ext = first().regions[first().start].built.extent();
    int64_t visitSeen = 0;
    std::vector<Vec2> drawn;
    size_t changedAt = 0;
    auto tick = [&] {
        now += 1.0 / 60;
        server.tick(now);
        client.receive(now);
        if (client.visit() != visitSeen && client.mirror()) {
            // East into the next column: its ground is this one's less a width.
            visitSeen = client.visit();
            predictor.carryOver(*client.mirror(), *client.regionChange()->driven, Vec2(-ext.x, 0), client.visit());
            changedAt = drawn.size();
        }
        if (client.mirror()) predictor.reconcile(*client.mirror());
        if (const auto in = client.sendInput(); in && client.mirror()) predictor.input(*client.mirror(), *in);
        client.flush(now);
        predictor.ease(1.0 / 60);
        if (client.mirror()) predictor.draw(*client.mirror());
        if (const auto u = predictor.shown()) drawn.push_back(u->position + Vec2(visitSeen > 0 ? ext.x : 0, 0));
    };
    client.connect(now);
    client.join(net::newPilotKey());
    for (int i = 0; i < 600 && !(client.mirror() && client.mirror()->driven() && client.world()); i++) tick();
    ASSERT_TRUE(client.mirror() && client.mirror()->driven());
    // Tern Ridge's mirror made ahead, as the game does on the worker thread.
    client.prepare(std::make_unique<net::Mirror>(net::Mirror::describe(client.mirror()->region(), tern->text)));
    WorldMap& w = server.world();
    const PilotId pilot = w.start().pilots().front();
    const auto& span = w.start().places()->openings[size_t(Edge::east)][0];
    put(w, pilot, inFrom(ext, Edge::east, (span.from + span.to) / 2, 5), 0);
    // Settle there, then walk east through the passage.
    for (int i = 0; i < 30; i++) tick();
    client.steer.walk = Vec2(1, 0);
    client.steer.heading = 0;
    drawn.clear();
    for (int i = 0; i < 360 && (changedAt == 0 || drawn.size() < changedAt + 90); i++) tick();
    ASSERT_TRUE(client.visit() == 1);
    EXPECT_TRUE(client.tookPrepared());
    ASSERT_TRUE(changedAt > 40 && drawn.size() > changedAt + 60);
    // The pace walking up to it, then every frame from 20 before the change
    // to 60 after.
    std::vector<double> steps;
    for (size_t i = 1; i < drawn.size(); i++) steps.push_back(distance(drawn[i], drawn[i - 1]));
    double pace = 0;
    for (size_t i = changedAt - 40; i < changedAt - 20; i++) pace += steps[i] / 20;
    double lo = 1e9, hi = 0;
    for (size_t i = changedAt - 20; i < changedAt + 60; i++) {
        lo = std::min(lo, steps[i]);
        hi = std::max(hi, steps[i]);
    }
    if (std::getenv("W4_STEPS"))
        for (size_t i = changedAt - 20; i < changedAt + 60; i++) std::printf("    %+lld %.4f\n", (long long)(int64_t(i) - int64_t(changedAt)), steps[i]);
    std::printf("  changed at frame %zu (prepared mirror: %s); pace %.4f cells a frame, %.4f…%.4f round the change\n", changedAt,
                client.tookPrepared() ? "yes" : "no", pace, lo, hi);
    EXPECT_TRUE(pace > 0.05);
    EXPECT_TRUE(lo > pace * 0.95 && hi < pace * 1.05);
}

// A new pilot's Prospector faces the start region's nearest ore (problem 14:
// it faced away, nothing to do in view).
TEST(world_new_pilot_faces_the_nearest_ore) {
    WorldMap w(first());
    ASSERT_TRUE(w.join(me));
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    const GameState& s = w.start().simulation().state;
    ASSERT_TRUE(!s.patches.empty());
    const OreDeposit* near = &s.patches.front();
    for (const OreDeposit& d : s.patches)
        if (distance(d.position, u->position) < distance(near->position, u->position)) near = &d;
    const Vec2 to = near->position - u->position;
    const double off = std::abs(std::remainder(u->heading - std::atan2(to.y, to.x), 2 * pi));
    std::printf("  heading %.3f rad, the ore %.1f cells off at %.3f rad\n", u->heading, length(to), std::atan2(to.y, to.x));
    EXPECT_TRUE(off < 0.2);
    // The seat's held input faces there too: it stands facing the ore.
    const double heading = u->heading;
    for (int i = 0; i < 30; i++) w.step();
    EXPECT_TRUE(std::abs(std::remainder(drivenUnit(w, me)->heading - heading, 2 * pi)) < 1e-9);
}

// The landing pads are what the ground shows: a plate under each pad of every
// region, none under the generated base's start bases (a pilot drove onto
// one of those 50 cells from the pad and could not fly).
TEST(world_plates_are_the_pads) {
    for (const WorldRegion& g : first().regions) {
        const TerrainField field(g.built.map);
        for (const region::Place& p : g.built.pads) {
            EXPECT_TRUE(field.materials(p.at).z > 0.9);
            EXPECT_TRUE(field.materials(p.at + Vec2(region::padReach - 0.3, 0)).z > 0.9);
            EXPECT_TRUE(field.materials(p.at + Vec2(region::padReach + 0.5, 0)).z < 0.1);
        }
        for (const int64_t b : g.built.map.starts) {
            const Vec2 c = g.built.map.bases[size_t(b)].center;
            bool onPad = false;
            for (const region::Place& p : g.built.pads) onPad = onPad || distance(p.at, c) < region::padReach + 1;
            if (!onPad) EXPECT_TRUE(field.materials(c).z < 0.1);
        }
    }
}

// "At a pad" is one rule on both ends: the client's distance (the World
// message's pads on its mirror's ground) is the server's, and the Dropship
// is called on a Landing Pad's centre and 3.9 cells out, not at 4.1.
TEST(world_pad_distance_is_the_same_on_client_and_server) {
    for (const double off : {0.0, 3.9, 4.1}) {
        WorldParams fuel;
        fuel.startHydrogen = 100;  // a flight costs MH (F1)
        fuel.devSeen = {"cold-fork"};
        WorldMap w(first(), fuel);
        w.join(me);
        const size_t fork = *w.regionNamed("cold-fork");
        const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
        ASSERT_TRUE(w.devLandingPad(first().start, pad + Vec2(0, 12)).has_value());
        ASSERT_TRUE(w.devLandingPad(fork, first().regions[fork].built.pads[0].at + Vec2(0, 10)).has_value());
        const Vec2 mine = padIn(w, first().start);
        put(w, me, w.start().places()->regionAt(mine) + Vec2(off, 0), 0);
        const auto msg = w.worldMessage(me);
        ASSERT_TRUE(msg.has_value());
        std::vector<region::Place> built;
        for (const region::Place& q : net::padsOnGround(msg->regions[size_t(msg->here)], w.start().simulation().map->bounds))
            if (q.name.rfind("Landing Pad", 0) == 0) built.push_back(q);
        const auto client = region::nearestPad(built, drivenUnit(w, me)->position);
        ASSERT_TRUE(client.has_value());
        EXPECT_NEAR(client->distance, off, 1e-9);
        EXPECT_NEAR(distance(mine, drivenUnit(w, me)->position), client->distance, 1e-9);
        const auto refused = w.fly(me, "cold-fork", "Landing Pad 1");
        EXPECT_TRUE(refused.has_value() == (off > region::padReach));
    }
}
