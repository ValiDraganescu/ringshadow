// The world (world/WorldMap.h): the world file and its checks, the pilot at
// the start pad, edges, flights, the wallet, and a client following its
// pilot into another region. Isolated scenes of a few seconds; run from
// unreal/server (as `make test` does).

#include "test.h"

#include "HeadlessClient.h"
#include "Server.h"
#include "TerrainField.h"
#include "WorldMap.h"
#include "rig.h"

#include <cmath>
#include <cstdio>
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
    EXPECT_TRUE(mentions(miss, "start east opening [39, 57] meets no opening of missing's west edge"));
    EXPECT_TRUE(mentions(miss, "missing west opening [70, 90] meets no opening of start's east edge"));
    EXPECT_TRUE(!miss.world.has_value());
}

// MARK: - Pilots

TEST(world_pilot_starts_at_the_pad_in_a_prospector_with_400_ore) {
    WorldMap w(first());
    ASSERT_TRUE(w.join(me));
    EXPECT_EQ(*w.where(me), first().start);
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    EXPECT_TRUE(u->kind == UnitKind::prospector);
    EXPECT_EQ(u->owner, Pilot::player);
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    EXPECT_TRUE(distance(u->position, pad) < 1.0);
    EXPECT_EQ(w.wallet(me)->ore, int64_t(400));
    EXPECT_EQ(w.wallet(me)->hydrogen, int64_t(0));
    EXPECT_EQ(w.start().simulation().state.players[0].ore, int64_t(400));
}

TEST(world_no_units_but_the_pilots_anywhere) {
    WorldMap w(first());
    w.join(me);
    for (int i = 0; i < 120; i++) w.step();
    EXPECT_EQ(unitsEverywhere(w), size_t(1));
    for (size_t i = 0; i < w.regionCount(); i++) {
        const GameState& s = w.region(i).simulation().state;
        EXPECT_TRUE(s.structures.empty());
        EXPECT_EQ(s.players.size(), size_t(1));
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
        EXPECT_NEAR(gap, w.params().arriveDepth, 0.3);
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

TEST(world_closed_edges_stop_it) {
    // Start and Glass Flats side by side: Start's north edge is the world's
    // border, and its east edge is closed outside its opening.
    WorldLoad l = gridOf(R"([["start.json", "glass-flats.json"]])");
    ASSERT_TRUE(l.world.has_value());
    struct Case {
        Edge edge;
        double along;
    };
    for (const Case c : {Case{Edge::north, 72}, Case{Edge::east, 20}}) {
        WorldMap w(*l.world);
        w.join(me);
        const Vec2 ext = w.start().places()->extent();
        put(w, me, inFrom(ext, c.edge, c.along, 4), headingOut(c.edge));
        double closest = 99;
        drive(w, me, headingOut(c.edge), 180, [&] {
            closest = std::min(closest, edgeGap(regionPos(w, me), ext, c.edge).first);
            return w.visit(me) > 0;
        });
        std::printf("  %s at %.0f: came within %.2f cells of the edge\n", std::string(region::edgeName(c.edge)).c_str(), c.along, closest);
        EXPECT_EQ(w.visit(me), int64_t(0));
        EXPECT_EQ(*w.where(me), l.world->start);
        EXPECT_TRUE(closest <= w.params().crossBand);
    }
}

TEST(world_flight_from_pad_to_pad) {
    WorldMap w(first());
    w.join(me);
    const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
    // Away from the pad: refused.
    put(w, me, w.start().places()->regionAt(pad) + Vec2(-12, 0), 0);
    EXPECT_TRUE(w.fly(me, "cold-fork", "landing") == std::optional<std::string>("not at a pad"));
    put(w, me, w.start().places()->regionAt(pad) + Vec2(-2.5, 0), 0);
    EXPECT_TRUE(w.fly(me, "nowhere", "landing").has_value());
    EXPECT_TRUE(w.fly(me, "cold-fork", "dock").has_value());
    drivenUnit(w, me)->carrying = 4;
    w.start().simulation().state.players[0].ore = 555;
    ASSERT_TRUE(!w.fly(me, "cold-fork", "landing").has_value());
    EXPECT_TRUE(w.flying(me));
    EXPECT_EQ(unitsEverywhere(w), size_t(0));
    EXPECT_TRUE(w.fly(me, "cold-fork", "landing") == std::optional<std::string>("already flying"));
    const int ticks = int(std::llround(w.params().flightTime * 60));
    for (int i = 0; i < ticks - 1; i++) w.step();
    EXPECT_TRUE(w.flying(me));
    EXPECT_EQ(*w.where(me), first().start);
    w.step();
    EXPECT_TRUE(!w.flying(me));
    ASSERT_TRUE(w.where(me) == w.regionNamed("cold-fork"));
    EXPECT_EQ(w.visit(me), int64_t(1));
    const Unit* u = drivenUnit(w, me);
    ASSERT_TRUE(u != nullptr);
    const Vec2 there = first().regions[*w.regionNamed("cold-fork")].built.pads[0].at;
    EXPECT_TRUE(distance(u->position, there) < 1.5);
    EXPECT_EQ(u->carrying, int64_t(4));
    EXPECT_EQ(w.wallet(me)->ore, int64_t(555));
    EXPECT_EQ(w.regionOf(me)->simulation().state.players[0].ore, int64_t(555));
    EXPECT_EQ(w.start().simulation().state.players[0].ore, int64_t(0));
    EXPECT_EQ(unitsEverywhere(w), size_t(1));
    // Lift-off and landing each told; the landing with the region change.
    const auto moves = w.takeMoves();
    ASSERT_TRUE(moves.size() == 2);
    EXPECT_TRUE(!moves[0].change.has_value());
    ASSERT_TRUE(moves[1].change.has_value());
    EXPECT_TRUE(moves[1].change->regionId == "cold-fork");
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
    EXPECT_EQ(client.mirror()->simulation().state.players[0].ore, int64_t(400));
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
// message's pads on its mirror's ground) is the server's, and a flight is
// accepted on the pad's centre and 3.9 cells out, refused at 4.1.
TEST(world_pad_distance_is_the_same_on_client_and_server) {
    for (const double off : {0.0, 3.9, 4.1}) {
        WorldMap w(first());
        w.join(me);
        const Vec2 pad = first().regions[first().start].built.pads[first().startPad].at;
        put(w, me, w.start().places()->regionAt(pad) + Vec2(off, 0), 0);
        const auto msg = w.worldMessage(me);
        ASSERT_TRUE(msg.has_value());
        const auto client = region::nearestPad(net::padsOnGround(msg->regions[size_t(msg->here)], w.start().simulation().map->bounds),
                                               drivenUnit(w, me)->position);
        const auto server = region::nearestPad(first().regions[first().start].built.pads, drivenUnit(w, me)->position);
        ASSERT_TRUE(client.has_value() && server.has_value());
        EXPECT_NEAR(client->distance, off, 1e-9);
        EXPECT_NEAR(server->distance, client->distance, 1e-12);
        EXPECT_TRUE(client->name == server->name);
        const auto refused = w.fly(me, "cold-fork", "landing");
        EXPECT_TRUE(refused.has_value() == (off > region::padReach));
    }
}
