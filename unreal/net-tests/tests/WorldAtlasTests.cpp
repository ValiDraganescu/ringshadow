// The world map's model (WorldAtlas.h, M2): the regions laid on the grid,
// the view's pan and zoom, what the map draws and what its tooltips say,
// and which item the pointer is on.

#include "test.h"

#include "NetMirror.h"
#include "NetProtocol.h"
#include "WorldAtlas.h"

#include "Pilot.h"
#include "Simulation.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace {

using namespace ac;
using namespace ac::net;
using namespace ac::net::atlas;

/// The First Reach's grid: 3 × 3 medium regions, the pilot in the middle.
WorldMessage firstReach() {
    WorldMessage w;
    w.columns = 3;
    w.rows = 3;
    const char* ids[] = {"ashen-shelf", "cold-fork", "glass-flats", "rust-hollow", "start", "tern-ridge", "dust-terraces", "brine-pans", "slag-pockets"};
    for (int i = 0; i < 9; i++) {
        WorldMessage::Region r;
        r.id = ids[i];
        r.name = ids[i];
        r.size = MapSize::medium;
        r.column = i % 3;
        r.row = i / 3;
        w.regions.push_back(r);
    }
    w.regions[4].name = "Prospect Landing";
    w.regions[4].pads = {WorldMessage::Pad{"landing", Vec2(72, 48), std::nullopt}};
    w.here = 4;
    return w;
}

std::string readText(const std::string& path) {
    std::ifstream f(path);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

bool has(const std::vector<std::string>& lines, const std::string& line) {
    for (const std::string& l : lines)
        if (l == line) return true;
    return false;
}

const atlas::Item* find(const std::vector<atlas::Item>& all, atlas::Kind kind) {
    for (const atlas::Item& it : all)
        if (it.kind == kind) return &it;
    return nullptr;
}

} // namespace

TEST(atlas_lays_the_regions_on_the_grid) {
    const WorldMessage w = firstReach();
    const std::vector<Place> places = layout(w, {"rust-hollow"});
    ASSERT_TRUE(places.size() == 9);
    // Medium regions, 144 × 96: the start in the middle, seamless.
    EXPECT_EQ(places[4].x, 144.0);
    EXPECT_EQ(places[4].y, 96.0);
    EXPECT_EQ(places[5].x, 288.0);
    EXPECT_EQ(places[8].y, 192.0);
    EXPECT_EQ(bounds(places), (Bounds{0, 0, 432, 288}));
    // Seen: the regions visited and the pilot's own; the rest dark.
    EXPECT_TRUE(places[3].seen && places[4].seen);
    EXPECT_TRUE(!places[0].seen && !places[5].seen);
    // A region's cells and its ground (centred) on the world.
    EXPECT_TRUE(places[4].fromCells(Vec2(72, 48)) == Vec2(216, 144));
    EXPECT_TRUE(places[4].fromGround(Vec2(0, 0)) == Vec2(216, 144));
    EXPECT_TRUE(places[4].toGround(Vec2(144, 96)) == Vec2(-72, -48));
    EXPECT_EQ(placeAt(places, Vec2(300, 10))->id, std::string("glass-flats"));
    EXPECT_TRUE(placeAt(places, Vec2(500, 10)) == nullptr);
}

TEST(atlas_columns_take_their_widest_region) {
    WorldMessage w = firstReach();
    w.regions[0].size = MapSize::large;  // 200 × 128 in the first column, first row
    const std::vector<Place> places = layout(w, {});
    EXPECT_EQ(places[1].x, 200.0);
    EXPECT_EQ(places[3].y, 128.0);
    // A small region sits in its cell's north-west corner.
    EXPECT_EQ(places[3].x, 0.0);
    EXPECT_EQ(places[4].x, 200.0);
}

TEST(atlas_view_zooms_about_the_pointer_and_pans) {
    const Vec2 screen(1600, 900);
    View v = View::fit(Bounds{0, 0, 432, 288}, screen, 40);
    EXPECT_TRUE(v.centre == Vec2(216, 144));
    EXPECT_NEAR(v.scale, (900.0 - 80) / 288, 1e-9);
    // A world point and its screen point round trip.
    const Vec2 p = v.toScreen(Vec2(100, 50), screen);
    const Vec2 back = v.toWorld(p, screen);
    EXPECT_NEAR(back.x, 100, 1e-9);
    EXPECT_NEAR(back.y, 50, 1e-9);
    // The wheel keeps the point under the pointer where it is.
    const Vec2 pointer(400, 300);
    const Vec2 under = v.toWorld(pointer, screen);
    v.zoomAt(pointer, screen, View::notch);
    const Vec2 still = v.toScreen(under, screen);
    EXPECT_NEAR(still.x, 400, 1e-9);
    EXPECT_NEAR(still.y, 300, 1e-9);
    // Within its limits.
    for (int i = 0; i < 40; i++) v.zoomAt(pointer, screen, View::notch);
    EXPECT_EQ(v.scale, View::maxScale);
    // A drag moves the picture with the pointer; the centre stays over the world.
    const double scale = v.scale;
    const Vec2 c = v.centre;
    v.pan(Vec2(48, -24));
    EXPECT_NEAR(v.centre.x, c.x - 48 / scale, 1e-9);
    EXPECT_NEAR(v.centre.y, c.y + 24 / scale, 1e-9);
    v.pan(Vec2(1e7, 1e7));
    v.clamp(Bounds{0, 0, 432, 288});
    EXPECT_TRUE(v.centre == Vec2(0, 0));
}

TEST(atlas_pick_takes_the_nearest_then_the_first_kind) {
    const Vec2 screen(1000, 1000);
    View v;
    v.centre = Vec2(0, 0);
    v.scale = 4;
    // Screen (500, 500) is world (0, 0).
    std::vector<atlas::Item> items(3);
    items[0].kind = atlas::Kind::mission;
    items[0].at = Vec2(2, 0);  // screen (508, 500)
    items[1].kind = atlas::Kind::deposit;
    items[1].at = Vec2(0.5, 0);  // screen (502, 500)
    items[2].kind = atlas::Kind::building;
    items[2].at = Vec2(20, 0);  // 80 points away, but 4 cells wide (16 points)
    items[2].radius = 4;
    // The deposit 3 points off, the mission 3: as near, the mission wins.
    EXPECT_EQ(*pick(items, v, screen, Vec2(505, 500)), size_t(0));
    // The deposit 2 points off, the mission 8: the deposit.
    EXPECT_EQ(*pick(items, v, screen, Vec2(500, 500)), size_t(1));
    // Inside the building's ground: the building.
    EXPECT_EQ(*pick(items, v, screen, Vec2(570, 500)), size_t(2));
    // Nothing within reach.
    EXPECT_TRUE(!pick(items, v, screen, Vec2(500, 700)).has_value());
}

TEST(atlas_groups_the_brood) {
    // A chain within 8 cells is one group; one far off is its own.
    const std::vector<Vec2> at{Vec2(0, 0), Vec2(6, 0), Vec2(12, 0), Vec2(60, 60)};
    const std::vector<std::vector<size_t>> g = groups(at, 8);
    ASSERT_TRUE(g.size() == 2);
    EXPECT_EQ(g[0].size(), size_t(3));
    EXPECT_EQ(g[1].size(), size_t(1));
}

TEST(atlas_band_clips_the_region) {
    Place p;
    p.x = 144;
    p.y = 96;
    p.width = 144;
    p.depth = 96;
    // A band moving east, from ground x -10 to 30: a strip 40 cells wide.
    Ringshadow::Band b;
    b.dir = Vec2(1, 0);
    b.lead = 30;
    b.trail = -10;
    const std::vector<Vec2> poly = band(p, b);
    ASSERT_TRUE(poly.size() == 4);
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    for (const Vec2& q : poly) {
        minX = std::min(minX, q.x);
        maxX = std::max(maxX, q.x);
        minY = std::min(minY, q.y);
        maxY = std::max(maxY, q.y);
    }
    EXPECT_NEAR(minX, 144 + 72 - 10, 1e-9);
    EXPECT_NEAR(maxX, 144 + 72 + 30, 1e-9);
    EXPECT_NEAR(minY, 96, 1e-9);
    EXPECT_NEAR(maxY, 192, 1e-9);
    // Off the region: nothing.
    b.lead = -200;
    b.trail = -300;
    EXPECT_TRUE(band(p, b).empty());
}

TEST(atlas_items_and_their_tooltips) {
    const WorldMessage w = firstReach();
    // The pilot's region, as the game mirrors it, from its region file.
    RegionDescription d;
    d.regionFile = readText("../server/regions/start.json");
    d.wild = true;
    d.fog = false;
    ASSERT_TRUE(!d.regionFile.empty());
    Mirror mirror(d);
    Simulation& sim = mirror.simulation();
    ASSERT_TRUE(sim.state.patches.size() >= 2);
    const OreDeposit deposit = sim.state.patches[0];
    sim.state.patches[1].remaining = 0;
    // The pilot, a mercenary of its own, another pilot's, a Lodeback at the
    // first deposit, a Citadel built by the pilot.
    sim.state.units.clear();
    Unit you(1, UnitKind::prospector, Pilot::player, Vec2(0, 0), 0, Unit::Task::idle);
    you.carrying = 7;
    sim.state.units.push_back(you);
    sim.state.units.push_back(Unit(2, UnitKind::prospector, Pilot::player, Vec2(10, 0), 0, Unit::Task::idle));
    sim.state.units.push_back(Unit(3, UnitKind::prospector, Pilot::player, Vec2(-10, 0), 0, Unit::Task::idle));
    sim.state.units.push_back(Unit(4, UnitKind::lodeback, 9, deposit.position + Vec2(1, 0), 0, Unit::Task::idle));
    sim.state.units.push_back(Unit(5, UnitKind::shardling, 9, deposit.position + Vec2(3, 0), 0, Unit::Task::idle));
    sim.state.structures.clear();
    Structure citadel(40, StructureKind::citadel, Pilot::player, Vec2(20, 20));
    citadel.hp = 900;
    sim.state.structures.push_back(citadel);

    ProgressMessage p;
    p.callsign = "ORBIT";
    p.builders = {{40, "ORBIT"}};
    p.mercs = {ProgressMessage::Merc{1, "Flint", ProgressMessage::Merc::State::working, "start", 0, 0, 1240, 2},
               ProgressMessage::Merc{2, "Kiln", ProgressMessage::Merc::State::down, "rust-hollow", 1, 130, 0, 0}};
    p.mercFields = {ProgressMessage::MercField{"rust-hollow", "Rust Hollow", 1, Vec2(30, 40), 3, 2400}};
    p.mercTags = {{2, "Flint"}, {3, "Wren"}};
    CitadelMission m;
    m.id = 7;
    m.kind = CitadelMission::Kind::survey;
    m.tier = CitadelMission::Tier::hard;
    m.state = CitadelMission::State::taken;
    m.title = "The Glass Echo";
    m.instructions = "Stand in the circle for 30 s";
    m.region = "start";
    m.at = Vec2(-20, 10);
    m.radius = 6;
    p.journal = {m};

    Input in;
    in.world = &w;
    in.progress = &p;
    in.here = &sim;
    in.driven = 1;
    in.sky = Sky{};
    in.sky->time = 100;
    in.sites = {KnownSite{"start", "Lander wreck", SiteKind::wreck, StructureKind::derrick, Vec2(50, 30), false}};
    const std::vector<Place> places = layout(w, {});
    const std::vector<atlas::Item> all = items(in, places);

    // In `atlas::Kind` order: the pilot first.
    ASSERT_TRUE(!all.empty());
    EXPECT_TRUE(all[0].kind == atlas::Kind::you);
    EXPECT_EQ(all[0].title, std::string("You · ORBIT"));
    EXPECT_TRUE(has(all[0].lines, "Carrying 7 ore"));
    EXPECT_TRUE(all[0].at == Vec2(216, 144));

    // The mercenaries: its own here (live), its own elsewhere (at its
    // field), another pilot's.
    int own = 0, others = 0;
    for (const atlas::Item& it : all) {
        if (it.kind != atlas::Kind::merc) continue;
        if (it.own) own++;
        else others++;
        if (it.title == "MERC · Flint") {
            EXPECT_TRUE(it.at == Vec2(226, 144));
            EXPECT_TRUE(has(it.lines, "1,240 ore delivered"));
            EXPECT_TRUE(has(it.lines, "Working its field"));
        }
        if (it.title == "MERC · Kiln") {
            EXPECT_TRUE(it.at == Vec2(30, 136));  // Rust Hollow's corner (0, 96) + its field
            EXPECT_TRUE(has(it.lines, "Down: back at work in 0:30"));
            EXPECT_TRUE(has(it.lines, "Field 2 in rust-hollow"));
        }
        if (it.title == "MERC · Wren") EXPECT_TRUE(has(it.lines, "Another pilot's mercenary"));
    }
    EXPECT_EQ(own, 2);
    EXPECT_EQ(others, 1);
    EXPECT_TRUE(find(all, atlas::Kind::pilot) == nullptr);

    // The Brood in one group by the deposit.
    const atlas::Item* bugs = find(all, atlas::Kind::bugs);
    ASSERT_TRUE(bugs != nullptr);
    EXPECT_EQ(bugs->title, std::string("Brood · 2 bugs"));
    EXPECT_TRUE(has(bugs->lines, "1 Shardling, 1 Lodeback"));

    // The Citadel.
    const atlas::Item* b = find(all, atlas::Kind::building);
    ASSERT_TRUE(b != nullptr);
    EXPECT_EQ(b->title, std::string("Citadel"));
    EXPECT_TRUE(b->own);
    EXPECT_TRUE(has(b->lines, "Built by you"));
    EXPECT_TRUE(has(b->lines, "HP 900 / " + thousands(int64_t(Rules::hp(StructureKind::citadel)))));

    // The deposits: ore left, eaten, mined out.
    int deposits = 0;
    bool eaten = false, out = false;
    for (const atlas::Item& it : all) {
        if (it.kind != atlas::Kind::deposit) continue;
        deposits++;
        if (it.ref == deposit.id) {
            eaten = it.flag && has(it.lines, "Being eaten by a Lodeback")
                && has(it.lines, thousands(deposit.remaining) + " of " + thousands(deposit.initial) + " ore left");
            EXPECT_NEAR(it.size, 1.0, 1e-9);
        }
        if (it.ref == sim.state.patches[1].id) out = has(it.lines, "Mined out: it grows back") && it.size == 0;
    }
    EXPECT_EQ(deposits, int(sim.state.patches.size()));
    EXPECT_TRUE(eaten);
    EXPECT_TRUE(out);

    // The site, the pad, the mission.
    const atlas::Item* site = find(all, atlas::Kind::site);
    ASSERT_TRUE(site != nullptr);
    EXPECT_EQ(site->key, std::string("start/Lander wreck"));
    EXPECT_TRUE(has(site->lines, "Wreck · Derrick blueprint"));
    EXPECT_TRUE(has(site->lines, "Not opened yet"));
    const atlas::Item* pad = find(all, atlas::Kind::pad);
    ASSERT_TRUE(pad != nullptr);
    EXPECT_EQ(pad->title, std::string("landing"));
    EXPECT_TRUE(has(pad->lines, "Where new pilots come down"));
    const atlas::Item* mission = find(all, atlas::Kind::mission);
    ASSERT_TRUE(mission != nullptr);
    EXPECT_EQ(mission->title, std::string("The Glass Echo"));
    EXPECT_TRUE(has(mission->lines, "Hard survey mission · Prospect Landing"));
    EXPECT_TRUE(mission->at == Vec2(196, 154));
    EXPECT_EQ(mission->radius, 6.0);
}

TEST(atlas_starfalls_incoming_and_craters) {
    const WorldMessage w = firstReach();
    RegionDescription d;
    d.regionFile = readText("../server/regions/start.json");
    d.wild = true;
    Mirror mirror(d);
    Simulation& sim = mirror.simulation();
    sim.state.units.clear();
    ProgressMessage p;
    Input in;
    in.world = &w;
    in.progress = &p;
    in.here = &sim;
    in.sky = Sky{};
    in.sky->time = 1000;
    Sky::Starfall coming{1, 0, Vec2(5, 5), 1042, 90, std::nullopt};
    Sky::Starfall landed{2, 4, sim.state.patches[0].position, 900, 90, int64_t(0)};
    in.sky->starfalls = {coming, landed};
    const std::vector<atlas::Item> all = items(in, layout(w, {}));
    const atlas::Item* s = find(all, atlas::Kind::starfall);
    ASSERT_TRUE(s != nullptr);
    EXPECT_TRUE(has(s->lines, "Lands in 0:42"));
    EXPECT_TRUE(s->at == Vec2(77, 53));  // Ashen Shelf's centre (72, 48) + (5, 5)
    const atlas::Item* c = find(all, atlas::Kind::crater);
    ASSERT_TRUE(c != nullptr);
    EXPECT_TRUE(has(c->lines, thousands(sim.state.patches[0].remaining) + " ore left"));
    // Its deposit is the crater's, not drawn twice.
    for (const atlas::Item& it : all) EXPECT_TRUE(!(it.kind == atlas::Kind::deposit && it.ref == sim.state.patches[0].id));
}

TEST(atlas_thousands) {
    EXPECT_EQ(thousands(0), std::string("0"));
    EXPECT_EQ(thousands(999), std::string("999"));
    EXPECT_EQ(thousands(1240), std::string("1,240"));
    EXPECT_EQ(thousands(-1234567), std::string("-1,234,567"));
}

/// A canyon zone (Z2) over its prize: its name and state, in its region's
/// place, drawn last of the kinds.
TEST(atlas_canyon_zones) {
    const WorldMessage w = firstReach();
    ProgressMessage p;
    Input in;
    in.world = &w;
    in.progress = &p;
    in.zones.push_back(ZoneMark{w.regions[1].id, Vec2(118, 27.5), 27, "THE FORK", {"Wave 3 of 10 · 7 bugs left"}, true});
    in.zones.push_back(ZoneMark{"nowhere", Vec2(1, 1), 5, "LOST", {}, false});
    const std::vector<atlas::Place> places = layout(w, {});
    const std::vector<atlas::Item> all = items(in, places);
    const atlas::Item* z = find(all, atlas::Kind::zone);
    ASSERT_TRUE(z != nullptr);
    EXPECT_TRUE(z->title == "THE FORK" && z->flag && z->radius == 27);
    EXPECT_TRUE(has(z->lines, "Wave 3 of 10 · 7 bugs left"));
    EXPECT_TRUE(z->at == placeOf(places, w.regions[1].id)->fromCells(Vec2(118, 27.5)));
    int zones = 0;
    for (const atlas::Item& it : all) zones += it.kind == atlas::Kind::zone;
    EXPECT_EQ(zones, 1);
}

/// P1: the frontier pilots on the map, each at its place in its region
/// (none while on its way), in its paint, its tooltip its callsign and
/// level, habit, what it is doing and where.
TEST(atlas_frontier_pilots) {
    const WorldMessage w = firstReach();
    const auto places = layout(w, {"start", "rust-hollow"});
    ProgressMessage p;
    p.frontier = {ProgressMessage::FrontierPilot{1, "Vesper", FrontierHabit::miner, 3},
                  ProgressMessage::FrontierPilot{2, "Kestrel", FrontierHabit::wanderer, 5}};
    Sky sky;
    Sky::Frontier vesper;
    vesper.id = 1;
    vesper.region = 3;
    vesper.at = Vec2(40, 30);
    vesper.level = 14;
    vesper.doing = FrontierDoing::building;
    Sky::Frontier kestrel;
    kestrel.id = 2;
    kestrel.level = 6;
    kestrel.doing = FrontierDoing::flying;
    sky.frontier = {vesper, kestrel};
    Input in;
    in.world = &w;
    in.progress = &p;
    in.sky = sky;
    const auto all = items(in, places);
    int64_t n = 0;
    for (const auto& it : all) n += it.kind == Kind::frontier ? 1 : 0;
    EXPECT_EQ(n, int64_t(1));
    const atlas::Item* it = find(all, Kind::frontier);
    ASSERT_TRUE(it != nullptr);
    EXPECT_EQ(it->title, std::string("VESPER · LV 14"));
    EXPECT_TRUE(has(it->lines, "Frontier pilot · Miner"));
    EXPECT_TRUE(has(it->lines, "Building in rust-hollow"));
    EXPECT_EQ(it->paint, int32_t(3));
    EXPECT_TRUE(it->region == 3 && distance(it->at, placeOf(places, 3)->fromCells(Vec2(40, 30))) < 1e-9);
}

/// The map's pick rule (the user, 2026-10-15: "I can't pick a landing on the
/// map"): a Landing Pad of the realm in a region been to is picked; a region
/// file's own pad, a region not discovered, a region with no Landing Pad, and
/// a realm with its only Landing Pad here are refused, each with its line.
TEST(atlas_pad_pick_rule_and_its_refusals) {
    WorldMessage w = firstReach();
    // The user's realm: one Landing Pad, here at Prospect Landing (a frontier
    // builder's), and every region's own pad.
    w.regions[4].pads.push_back(WorldMessage::Pad{"Landing Pad 1", Vec2(60, 44), StructureKind::landingPad});
    w.regions[3].pads = {WorldMessage::Pad{"landing", Vec2(72, 48), std::nullopt}};
    w.regions[1].pads = {WorldMessage::Pad{"landing", Vec2(72, 48), std::nullopt}};
    const std::set<std::string> seen{"start", "rust-hollow", "cold-fork"};
    PadPick p = padPick(w, seen, 3, 0);
    EXPECT_TRUE(!p.ok && p.why == std::string(onlyHere));
    EXPECT_TRUE(!padPick(w, seen, 4, 1).ok);
    // A Landing Pad in Rust Hollow (been to): picked; its own pad refused.
    w.regions[3].pads.push_back(WorldMessage::Pad{"Landing Pad 1", Vec2(40, 30), StructureKind::landingPad});
    EXPECT_TRUE(padPick(w, seen, 3, 1).ok);
    EXPECT_TRUE(padPick(w, seen, 4, 1).ok);
    p = padPick(w, seen, 3, 0);
    EXPECT_EQ(p.why, std::string("That is rust-hollow's own pad: the Dropship lands only on a Landing Pad"));
    // Cold Fork: been to, no Landing Pad.
    p = padPick(w, seen, 1, 0);
    EXPECT_EQ(p.why, std::string("No Landing Pad in cold-fork: build one there first"));
    // Glass Flats: a Landing Pad, never been there.
    w.regions[2].pads = {WorldMessage::Pad{"Landing Pad 1", Vec2(40, 30), StructureKind::landingPad}};
    p = padPick(w, seen, 2, 0);
    EXPECT_EQ(p.why, std::string("Not discovered yet: glass-flats. Go there on foot first"));
}
