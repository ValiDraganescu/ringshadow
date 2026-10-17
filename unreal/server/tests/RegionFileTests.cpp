// Region files (AutocraftCore RegionFile.h, docs/regions.md) and the
// region tool's preview (region/Preview.h). Isolated: a map built and
// looked at, nothing stepped.

#include "test.h"

#include "NavGrid.h"
#include "Preview.h"
#include "RegionFile.h"
#include "TerrainField.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::region;

/// A clean flat field to craft on: the open field without its resources
/// and its rocks, with `layers` (JSON keys) added.
std::string flat(const std::string& layers = "") {
    return R"({"id": "t", "name": "T", "size": "medium", "base": {"style": "openField", "seed": 1, "drop": ["resources", "doodads"]})" +
           (layers.empty() ? std::string() : ", " + layers) + "}";
}

RegionFile parsed(const std::string& text) {
    const Parsed p = parse(text);
    for (const Problem& q : p.problems) std::printf("    unexpected: %s\n", q.text().c_str());
    return p.file ? *p.file : RegionFile{};
}

std::vector<Problem> parseProblems(const std::string& text) { return parse(text).problems; }

std::vector<Problem> problemsOf(const std::string& text) {
    const RegionFile f = parsed(text);
    return validate(f, build(f));
}

/// A problem whose place holds `where` and whose message holds `what`.
bool has(const std::vector<Problem>& problems, const std::string& where, const std::string& what) {
    for (const Problem& p : problems)
        if (p.where.find(where) != std::string::npos && p.message.find(what) != std::string::npos) return true;
    std::printf("    no \"%s\" / \"%s\" among:\n", where.c_str(), what.c_str());
    for (const Problem& p : problems) std::printf("      %s\n", p.text().c_str());
    return false;
}

} // namespace

TEST(RegionFile_roundTrip) {
    const std::string text = R"({
      "id": "round-trip", "name": "Round Trip", "size": "small",
      "base": {"style": "ridge", "seed": 42, "drop": ["doodads"]},
      "height": [
        {"op": "raise", "area": [[10, 10], [20, 10], [20, 20]], "level": 1, "slope": 3},
        {"op": "lower", "area": {"rect": [30, 30, 40, 40]}, "level": 0},
        {"op": "flatten", "area": {"circle": [50, 30, 5.5]}, "level": 1},
        {"op": "ridge", "line": [[60, 10], [70, 20]], "width": 2.5, "level": 2},
        {"op": "ramp", "from": [5, 50], "to": [5, 40], "width": 4, "fromLevel": 0, "level": 1}
      ],
      "deposits": [{"at": [12.5, 30], "amount": 1800, "angle": 45}],
      "wells": [{"at": [80, 40], "amount": 2250}],
      "doodads": [{"kind": "rockSpire", "at": [90, 60], "rotation": 30, "scale": 1.5}],
      "pads": [{"name": "Dock", "at": [48, 32]}],
      "spawns": [{"name": "Gate", "at": [48, 40]}],
      "openings": {"north": [[40, 56]], "west": [[20, 30], [40, 50]]}
    })";
    const Parsed first = parse(text);
    ASSERT_TRUE(first.file.has_value());
    const RegionFile& f = *first.file;
    EXPECT_EQ(f.id, std::string("round-trip"));
    EXPECT_TRUE(f.seed == std::optional<uint64_t>(42));
    EXPECT_TRUE(f.dropDoodads && !f.dropResources);
    EXPECT_EQ(f.height.size(), size_t(5));
    EXPECT_TRUE(f.height[2].area.shape == Area::Shape::circle);
    EXPECT_EQ(f.openings[static_cast<size_t>(Edge::west)].size(), size_t(2));
    const std::string written = write(f);
    const Parsed again = parse(written);
    ASSERT_TRUE(again.file.has_value());
    EXPECT_TRUE(*again.file == f);
    EXPECT_EQ(write(*again.file), written);
    // The starter is the base alone, and reads back the same.
    const RegionFile s = starter("a-1", MapStyle::badlands, 9, MapSize::large);
    const Parsed st = parse(write(s));
    ASSERT_TRUE(st.file.has_value());
    EXPECT_TRUE(*st.file == s);
}

TEST(RegionFile_parseProblems) {
    // Bad JSON: the line and column.
    EXPECT_TRUE(has(parseProblems("{\n  \"id\": \"x\",\n  \"name\": ]\n}"), "line 3", "not valid JSON"));
    EXPECT_TRUE(has(parseProblems(R"({"name": "N", "size": "medium", "base": {"style": "ridge"}})"), "", "missing \"id\""));
    EXPECT_TRUE(has(parseProblems(R"({"id": "Bad Id", "name": "N", "size": "medium", "base": {"style": "ridge"}})"), "id", "slug"));
    EXPECT_TRUE(has(parseProblems(flat(R"("deposit": [])")), "region", "did you mean \"deposits\"?"));
    EXPECT_TRUE(has(parseProblems(R"({"id": "x", "name": "N", "size": "tiny", "base": {"style": "ridge"}})"), "size", "one of small"));
    EXPECT_TRUE(has(parseProblems(R"({"id": "x", "name": "N", "size": "small", "base": {"style": "hills"}})"), "base.style", "one of highlands"));
    EXPECT_TRUE(has(parseProblems(flat(R"("deposits": [{"at": [150, 20], "amount": 900}])")), "deposits[0].at", "out of bounds"));
    EXPECT_TRUE(has(parseProblems(flat(R"("deposits": [{"at": [15, 20]}])")), "deposits[0]", "missing \"amount\""));
    EXPECT_TRUE(has(parseProblems(flat(R"("height": [{"op": "raise", "area": [[1, 1], [5, 5]], "level": 1}])")), "height[0].area", "at least 3"));
    EXPECT_TRUE(has(parseProblems(flat(R"("height": [{"op": "raise", "area": [[1, 1], [5, 5], [9, 9]], "level": 1}])")), "height[0].area", "encloses no ground"));
    EXPECT_TRUE(has(parseProblems(flat(R"("height": [{"op": "raise", "area": {"rect": [1, 1, 9, 9]}, "level": 5}])")), "height[0].level", "0…4"));
    EXPECT_TRUE(has(parseProblems(flat(R"("ground": "lava")")), "ground", "one of plain, rust"));
    EXPECT_TRUE(has(parseProblems(flat(R"("height": [{"op": "dig", "area": {"rect": [1, 1, 9, 9]}, "level": 0}])")), "height[0].op", "one of raise"));
    EXPECT_TRUE(has(parseProblems(flat(R"("doodads": [{"kind": "tree", "at": [9, 9]}])")), "doodads[0].kind", "one of rock"));
    EXPECT_TRUE(has(parseProblems(flat(R"("pads": [{"name": "A", "at": [9, 9]}, {"name": "A", "at": [29, 9]}])")), "pads[1].name", "used twice"));
    EXPECT_TRUE(has(parseProblems(flat(R"("openings": {"north": [[10, 12]]})")), "openings.north[0]", "shorter than 4"));
    EXPECT_TRUE(has(parseProblems(flat(R"("openings": {"east": [[90, 100]]})")), "openings.east[0]", "0…96"));
    EXPECT_TRUE(has(parseProblems(flat(R"("openings": {"south": [[10, 20], [15, 30]]})")), "openings.south[1]", "overlaps"));
    EXPECT_TRUE(has(parseProblems(flat(R"("openings": {"up": [[10, 20]]})")), "openings", "unknown key \"up\""));
    // A clean file has none, and its build has none either.
    EXPECT_TRUE(parseProblems(flat()).empty());
    EXPECT_TRUE(problemsOf(flat()).empty());
}

TEST(RegionFile_validationProblems) {
    // A plateau (level 1, cliffs) and a ridge to put things on.
    const std::string ground = R"("height": [{"op": "raise", "area": {"rect": [20, 20, 50, 44]}, "level": 1},
                                             {"op": "ridge", "line": [[90, 60], [120, 60]], "width": 3, "level": 2}])";
    // Deposits: on a cliff, in rock, on top of another, cut off.
    EXPECT_TRUE(has(problemsOf(flat(ground + R"(, "deposits": [{"at": [50, 32], "amount": 900}])")), "deposits[0]", "on a cliff face"));
    EXPECT_TRUE(has(problemsOf(flat(R"("deposits": [{"at": [60, 30], "amount": 900}], "doodads": [{"kind": "boulder", "at": [61, 31]}])")),
                    "deposits[0]", "in rock: a boulder"));
    EXPECT_TRUE(has(problemsOf(flat(R"("deposits": [{"at": [60, 30], "amount": 900}, {"at": [61, 30], "amount": 900}])")),
                    "deposits[1]", "on top of deposits[0]"));
    const std::string ring = R"("height": [{"op": "ridge", "line": [[66, 48], [72, 42], [78, 48], [72, 54], [66, 48]], "width": 2, "level": 2}])";
    EXPECT_TRUE(has(problemsOf(flat(ring + R"(, "deposits": [{"at": [72, 48], "amount": 900}])")), "deposits[0]", "cut off"));
    // Wells: on a cliff.
    EXPECT_TRUE(has(problemsOf(flat(ground + R"(, "wells": [{"at": [100, 61], "amount": 2000}])")), "wells[0]", "on a cliff face"));
    // Pads: on a cliff, on uneven ground, in rock.
    EXPECT_TRUE(has(problemsOf(flat(ground + R"(, "pads": [{"name": "Edge", "at": [50, 40]}])")), "pads[0] \"Edge\"", "on a cliff"));
    EXPECT_TRUE(has(problemsOf(flat(R"("height": [{"op": "raise", "area": {"rect": [60, 20, 90, 40]}, "level": 1, "slope": 12}],
                                       "pads": [{"name": "Slope", "at": [94, 30]}])")), "pads[0]", "uneven ground"));
    EXPECT_TRUE(has(problemsOf(flat(R"("pads": [{"name": "Rocky", "at": [60, 30]}], "doodads": [{"kind": "boulder", "at": [62, 30]}])")),
                    "pads[0]", "in rock"));
    // Spawn points: on a cliff, on a well, cut off.
    EXPECT_TRUE(has(problemsOf(flat(ground + R"(, "spawns": [{"name": "S", "at": [50, 30]}])")), "spawns[0]", "on a cliff face"));
    EXPECT_TRUE(has(problemsOf(flat(R"("wells": [{"at": [60, 30], "amount": 2000}], "spawns": [{"name": "S", "at": [60, 30]}])")),
                    "spawns[0]", "on top of wells[0]"));
    EXPECT_TRUE(has(problemsOf(flat(ring + R"(, "spawns": [{"name": "S", "at": [72, 48]}])")), "spawns[0]", "cut off"));
    // Ramps: an end on the wrong level, too steep.
    EXPECT_TRUE(has(problemsOf(flat(R"("height": [{"op": "raise", "area": {"rect": [20, 20, 50, 44]}, "level": 1},
                                                  {"op": "ramp", "from": [35, 52], "to": [35, 42], "width": 5, "fromLevel": 0, "level": 2}])")),
                    "height[1]", "\"to\" end meets level 1 ground, not level 2"));
    EXPECT_TRUE(has(problemsOf(flat(R"("height": [{"op": "raise", "area": {"rect": [20, 20, 50, 44]}, "level": 2},
                                                  {"op": "ramp", "from": [35, 46], "to": [35, 43], "width": 5, "fromLevel": 0, "level": 2}])")),
                    "height[1]", "too steep"));
    // Openings: a cliff across the lane; a lane walled off from the rest.
    EXPECT_TRUE(has(problemsOf(flat(R"("height": [{"op": "ridge", "line": [[70, 0], [70, 10]], "width": 2, "level": 2}],
                                       "openings": {"north": [[64, 76]]})")), "openings.north[0] 64…76", "a cliff"));
    EXPECT_TRUE(has(problemsOf(flat(R"("height": [{"op": "ridge", "line": [[60, 0], [60, 8], [80, 8], [80, 0]], "width": 2, "level": 2}],
                                       "openings": {"north": [[64, 76]]})")), "openings.north[0]", "cut off"));
    EXPECT_TRUE(has(problemsOf(flat(R"("doodads": [{"kind": "boulder", "at": [142, 50]}], "openings": {"east": [[44, 56]]})")),
                    "openings.east[0]", "a boulder"));
}

TEST(RegionFile_baseOnlyIsTheGeneratedMap) {
    // No seed: the game's own map.
    const RegionFile plain = parsed(R"({"id": "p", "name": "P", "size": "small", "base": {"style": "highlands"}})");
    MapDefinition game = WindowMaps::build(MapChoice{MapStyle::highlands, MapSize::small, 2});
    MapDefinition region = build(plain).map;
    EXPECT_EQ(region.seed, game.seed);
    // Only the name and the layout version differ.
    region.name = game.name;
    region.version = game.version;
    EXPECT_TRUE(region == game);
    // A seed: the generator's map of that style, size and seed.
    for (const MapStyle style : {MapStyle::highlands, MapStyle::openField, MapStyle::ridge, MapStyle::badlands}) {
        RegionFile f = starter("s", style, 7, MapSize::medium);
        MapDefinition built = build(f).map;
        const MapDefinition generated = WindowMaps::build(MapChoice{style, MapSize::medium, 2}, true, 7);
        built.name = generated.name;
        built.version = generated.version;
        EXPECT_TRUE(built == generated);
        EXPECT_TRUE(built.heightEdits.empty() && built.deposits.empty() && built.wells.empty());
        // Another seed lays it out anew.
        EXPECT_TRUE(!(generated.doodads == WindowMaps::build(MapChoice{style, MapSize::medium, 2}, true, 8).doodads));
    }
    EXPECT_TRUE(!(WindowMaps::build(MapChoice{MapStyle::ridge, MapSize::medium, 2}, true, 7).seed ==
                  WindowMaps::build(MapChoice{MapStyle::ridge, MapSize::medium, 2}).seed));
}

TEST(RegionFile_layersLandWhereWritten) {
    const RegionFile f = parsed(flat(R"(
        "height": [{"op": "raise", "area": {"rect": [20, 20, 50, 44]}, "level": 2},
                   {"op": "flatten", "area": {"rect": [30, 26, 40, 36]}, "level": 1},
                   {"op": "lower", "area": {"circle": [100, 48, 6]}, "level": 0}],
        "deposits": [{"at": [70, 30], "amount": 1234, "angle": 90}],
        "wells": [{"at": [80, 30], "amount": 777}],
        "doodads": [{"kind": "rockSpire", "at": [90, 70], "rotation": 90}],
        "pads": [{"name": "Dock", "at": [70, 60]}],
        "spawns": [{"name": "Gate", "at": [60, 60]}])"));
    const Built b = build(f);
    const MapDefinition& m = b.map;
    // Region (70, 30) is ground (-2, -18) on the 144 by 96 medium map.
    EXPECT_TRUE(b.ground(Vec2(70, 30)) == Vec2(-2, -18));
    ASSERT_TRUE(m.deposits.size() == 1 && m.wells.size() == 1);
    EXPECT_TRUE(m.deposits[0].position == Vec2(-2, -18));
    EXPECT_EQ(m.deposits[0].amount, int64_t(1234));
    EXPECT_NEAR(m.deposits[0].angle, 3.14159265358979 / 2, 1e-12);
    EXPECT_TRUE(m.wells[0].position == Vec2(8, -18));
    // The game state mines them.
    const GameState s = GameState::new_(m);
    bool deposit = false, well = false;
    for (const OreDeposit& o : s.patches) deposit = deposit || (o.position == Vec2(-2, -18) && o.remaining == 1234);
    if (s.wells) for (const Well& w : *s.wells) well = well || (w.position == Vec2(8, -18) && w.remaining == 777);
    EXPECT_TRUE(deposit && well);
    EXPECT_TRUE(m.doodads.back().position == Vec2(18, 22) && m.doodads.back().kind == Doodad::Kind::rockSpire);
    ASSERT_TRUE(b.pads.size() == 1 && b.spawns.size() == 1);
    EXPECT_TRUE(b.pads[0].at == Vec2(-2, 12) && b.pads[0].name == "Dock");
    EXPECT_TRUE(b.spawns[0].at == Vec2(-12, 12));
    // Heights: the flattened square is level 1 exactly, inside the level 2
    // plateau; the lowered circle is at most the low ground.
    const TerrainField field(m);
    for (double x = 30.5; x < 40; x += 1.5)
        for (double z = 26.5; z < 36; z += 1.5) {
            EXPECT_NEAR(field.height(b.ground(Vec2(x, z))), m.levelHeight, 1e-9);
            EXPECT_EQ(field.level(b.ground(Vec2(x, z))), int64_t(1));
        }
    EXPECT_NEAR(field.height(b.ground(Vec2(22, 22))), 2 * m.levelHeight, 0.35);
    EXPECT_EQ(field.level(b.ground(Vec2(22, 22))), int64_t(2));
    EXPECT_TRUE(field.height(b.ground(Vec2(100, 48))) <= 1e-9);
    // Without edits the field is the generated one, point for point.
    MapDefinition bare = m;
    bare.heightEdits.clear();
    const TerrainField plain(bare);
    EXPECT_EQ(plain.height(b.ground(Vec2(5, 5))), field.height(b.ground(Vec2(5, 5))));
}

TEST(RegionFile_groundAndLandmarks) {
    // The ground palette reads and writes back; plain is left out.
    const RegionFile f = parsed(flat(R"("ground": "slag",
        "height": [{"op": "raise", "area": {"rect": [20, 14, 56, 50]}, "level": 2, "slope": 6},
                   {"op": "raise", "area": {"circle": [35, 26, 6]}, "level": 4},
                   {"op": "ramp", "from": [35, 45], "to": [35, 31], "width": 4, "fromLevel": 2, "level": 4}],
        "doodads": [{"kind": "arch", "at": [80, 60], "rotation": 0},
                    {"kind": "sunkHauler", "at": [100, 30], "rotation": 30},
                    {"kind": "derrickRuin", "at": [110, 70]},
                    {"kind": "relayDish", "at": [60, 80], "rotation": 200, "scale": 1.2}],
        "spawns": [{"name": "Gate", "at": [72, 48]}])"));
    EXPECT_TRUE(f.ground == Ground::slag);
    EXPECT_TRUE(write(f).find("\"ground\": \"slag\"") != std::string::npos);
    EXPECT_TRUE(write(parsed(flat())).find("ground") == std::string::npos);
    EXPECT_TRUE(parsed(write(f)) == f);
    const Built b = build(f);
    for (const Problem& p : validate(f, b)) std::printf("    unexpected: %s\n", p.text().c_str());
    EXPECT_TRUE(validate(f, b).empty());
    // Ground above level 2: the mesa is level 4, 4 levels high.
    const TerrainField field(b.map);
    EXPECT_EQ(field.level(b.ground(Vec2(35, 24))), int64_t(4));
    EXPECT_NEAR(field.height(b.ground(Vec2(35, 24))), 4 * b.map.levelHeight, 0.35);
    // The arch blocks at its legs and leaves the lane under it open.
    NavGrid nav(b.map, field);
    EXPECT_TRUE(nav.walkable(b.ground(Vec2(80, 60))));
    EXPECT_TRUE(!nav.walkable(b.ground(Vec2(82.6, 60))));
    EXPECT_TRUE(!nav.walkable(b.ground(Vec2(77.4, 60))));
    // The others are solid at their middles.
    EXPECT_TRUE(!nav.walkable(b.ground(Vec2(100, 30))));
    EXPECT_TRUE(!nav.walkable(b.ground(Vec2(110, 70))));
    EXPECT_TRUE(!nav.walkable(b.ground(Vec2(60, 80))));
}

TEST(RegionFile_openingsAreDrivable) {
    // Highlands: the mains on plateaus, cliffs down the middle; openings on
    // every edge, the north one behind a crafted ridge to drive round.
    const RegionFile f = parsed(R"({"id": "o", "name": "O", "size": "medium", "base": {"style": "highlands", "seed": 3},
        "height": [{"op": "ridge", "line": [[60, 10], [84, 10]], "width": 4, "level": 1}],
        "spawns": [{"name": "Middle", "at": [72, 48]}],
        "openings": {"north": [[64, 80]], "south": [[64, 80]], "west": [[40, 56]], "east": [[40, 56]]}})");
    const Built b = build(f);
    EXPECT_TRUE(validate(f, b).empty());
    const TerrainField field(b.map);
    NavGrid nav(b.map, field);
    GameState s = GameState::new_(b.map);
    s.structures.clear();
    s.units.clear();
    nav.setDynamic(s);
    for (size_t e = 0; e < 4; e++) {
        for (const Span& span : f.openings[e]) {
            for (double along : {span.from + 1, (span.from + span.to) / 2, span.to - 1}) {
                const Vec2 target = b.ground(b.edgePoint(static_cast<Edge>(e), along, laneDepth));
                const auto way = nav.path(b.spawns[0].at, target, 0);
                ASSERT_TRUE(way.has_value() && !way->empty());
                EXPECT_TRUE(distance(way->back(), target) < 0.75);
            }
        }
    }
}

TEST(RegionFile_exampleIsClean) {
    // The example generated region (unreal/server/regions/); the tests run
    // from unreal/server.
    std::ifstream in("regions/cinder-steps.json");
    ASSERT_TRUE(in.good());
    std::stringstream text;
    text << in.rdbuf();
    const RegionFile f = parsed(text.str());
    EXPECT_EQ(f.id, std::string("cinder-steps"));
    EXPECT_TRUE(validate(f, build(f)).empty());
}

TEST(RegionFile_previewWritesPng) {
    const RegionFile f = parsed(flat(R"("deposits": [{"at": [50, 32], "amount": 900}], "pads": [{"name": "Dock", "at": [70, 60]}],
                                        "openings": {"north": [[64, 76]]},
                                        "height": [{"op": "raise", "area": {"rect": [20, 20, 50, 44]}, "level": 1}])"));
    const Built b = build(f);
    const auto problems = validate(f, b);
    EXPECT_EQ(problems.size(), size_t(1));
    const regiontool::Image image = regiontool::preview(f, b, problems, 4);
    EXPECT_TRUE(image.width > 144 * 4 && image.height > 96 * 4);
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "autocraft-region-preview-test.png";
    ASSERT_TRUE(regiontool::writePng(image, path.string()));
    std::ifstream png(path, std::ios::binary);
    char head[8] = {};
    png.read(head, 8);
    EXPECT_TRUE(std::string(head + 1, 3) == "PNG");
    EXPECT_TRUE(std::filesystem::file_size(path) > 1000);
    png.close();
    std::filesystem::remove(path);
}

// W4: a region's border. The outline of its playable land and the walls
// outside it, openings with a floor level, written back as read; the walls
// stand outside the outline, the gaps stay at the passage's level to the
// edge, and a unit stands past an open edge by the overhang only.
TEST(RegionFile_borderWallsAndPassages) {
    const std::string layers = R"("border": {"outline": [[10, 10], [134, 10], [134, 40], [144, 40], [144, 56], [134, 56], [134, 86], [10, 86]],
        "islands": [[[30, 60], [44, 60], [44, 74], [30, 74]]], "level": 4},
        "pads": [{"name": "landing", "at": [72, 48]}],
        "openings": {"east": [{"span": [41, 55], "level": 0}]})";
    const RegionFile f = parsed(flat(layers));
    ASSERT_TRUE(f.border.has_value());
    EXPECT_EQ(f.border->outline.size(), size_t(8));
    EXPECT_EQ(f.border->level, int64_t(4));
    EXPECT_EQ(f.border->islands.size(), size_t(1));
    ASSERT_TRUE(f.openings[size_t(Edge::east)].size() == 1);
    EXPECT_TRUE(f.openings[size_t(Edge::east)][0].level == std::optional<int64_t>(0));
    // Written and read back the same.
    const Parsed again = parse(write(f));
    ASSERT_TRUE(again.file.has_value());
    EXPECT_TRUE(*again.file == f);
    const Built b = build(f);
    ASSERT_TRUE(b.map.edges.has_value());
    EXPECT_EQ(b.map.edges->gaps.size(), size_t(1));
    EXPECT_TRUE(validate(f, b).empty());
    const TerrainField field(b.map);
    const double H = b.map.levelHeight;
    // Walls outside the outline, the land inside as it was, the gap at its floor.
    EXPECT_TRUE(field.height(b.ground(Vec2(4, 48))) >= 4 * H - 1e-9);
    // (Near the outline a terrace may step the face half way up; by the
    // edge it stands full.)
    EXPECT_TRUE(field.height(b.ground(Vec2(72, 4))) >= 2 * H - 1e-9);
    EXPECT_TRUE(field.height(b.ground(Vec2(72, 1))) >= 4 * H - 1e-9);
    EXPECT_TRUE(field.height(b.ground(Vec2(72, 48))) < 0.5 * H);
    // An island of rock: walled like the border.
    EXPECT_TRUE(field.height(b.ground(Vec2(37, 67))) >= 4 * H - 1e-9);
    EXPECT_NEAR(field.height(b.ground(Vec2(144, 48))), 0.0, 1e-9);
    // On a walled stretch of the edge: the wall's level, and the edge's own
    // profile over it (W4b), the same in a neighbour whose west edge has
    // the same gap; its level exactly near the corners.
    EXPECT_TRUE(field.height(b.ground(Vec2(144, 20))) >= 4 * H - 1e-9);
    EXPECT_TRUE(field.height(b.ground(Vec2(144, 20))) < 6 * H);
    EXPECT_NEAR(field.height(b.ground(Vec2(144, 1))), 4 * H, 1e-9);
    {
        const RegionFile g = parsed(flat(R"("border": {"outline": [[10, 10], [134, 10], [134, 86], [10, 86], [10, 56], [0, 56], [0, 40], [10, 40]], "level": 4},
        "pads": [{"name": "landing", "at": [72, 48]}],
        "openings": {"west": [{"span": [41, 55], "level": 0}]})"));
        const Built c = build(g);
        const TerrainField west(c.map);
        double worst = 0, rise = 0;
        for (double z = 1; z < 95; z += 0.5) {
            const double here = field.height(b.ground(Vec2(144, z))), there = west.height(c.ground(Vec2(0, z)));
            worst = std::max(worst, std::fabs(here - there));
            rise = std::max(rise, here - 4 * H);
        }
        EXPECT_TRUE(worst < 0.01);
        EXPECT_TRUE(rise > 0.5 * H);
    }
    // The walk: up to the edge in the gap, nowhere near it elsewhere.
    NavGrid nav(b.map, field);
    EXPECT_TRUE(nav.standable(b.ground(Vec2(143.8, 48))));
    EXPECT_TRUE(!nav.standable(b.ground(Vec2(143.8, 20))));
    // Past the open edge by the overhang only, and only in the gap.
    const RegionEdges& e = *b.map.edges;
    EXPECT_TRUE(e.standIn(b.map.bounds, b.ground(Vec2(144.6, 48))).has_value());
    EXPECT_TRUE(!e.standIn(b.map.bounds, b.ground(Vec2(145.5, 48))).has_value());
    EXPECT_TRUE(!e.standIn(b.map.bounds, b.ground(Vec2(144.6, 20))).has_value());
    // Rocks strewn over the walls.
    size_t onWalls = 0;
    for (const Doodad& d : b.map.doodads)
        if (signedDistance(b.regionAt(d.position), f.border->outline) > 2.5) onWalls += 1;
    std::printf("  %zu rocks on the walls\n", onWalls);
    EXPECT_TRUE(onWalls > 30);
}

TEST(RegionFile_borderProblems) {
    // A side crossing another.
    EXPECT_TRUE(has(parseProblems(flat(R"("border": {"outline": [[10, 10], [130, 80], [130, 10], [10, 80]]})")), "border.outline", "crosses"));
    EXPECT_TRUE(has(parseProblems(flat(R"("border": {"outline": [[10, 10], [130, 10], [130, 80]], "level": 9})")), "border.level", "must be"));
    EXPECT_TRUE(has(parseProblems(flat(R"("openings": {"east": [{"span": [41, 55], "level": 0, "wide": 1}]})")), "openings.east[0]", "unknown key"));
    // An island outside the outline.
    EXPECT_TRUE(has(parseProblems(flat(R"("border": {"outline": [[10, 10], [130, 10], [130, 80], [10, 80]], "islands": [[[50, 50], [140, 50], [60, 60]]]})")),
                    "border.islands[0]", "outside the border's outline"));
    // A pad on an island: off the land.
    EXPECT_TRUE(has(problemsOf(flat(R"("border": {"outline": [[10, 10], [134, 10], [134, 86], [10, 86]], "islands": [[[30, 30], [50, 30], [50, 50], [30, 50]]]},
        "pads": [{"name": "landing", "at": [72, 48]}, {"name": "rock", "at": [40, 40]}])")), "pads[1]", "outside the border's outline"));
    // An outline that shuts the passage, and a pad left outside it.
    const auto problems = problemsOf(flat(R"("border": {"outline": [[10, 10], [134, 10], [134, 86], [10, 86]]},
        "pads": [{"name": "landing", "at": [72, 48]}, {"name": "far", "at": [140, 20]}],
        "openings": {"east": [{"span": [41, 55], "level": 0}]})"));
    EXPECT_TRUE(has(problems, "openings.east[0]", "the border's outline closes the passage"));
    EXPECT_TRUE(has(problems, "pads[1]", "outside the border's outline"));
}

// docs/perf.md "Server" round 2: the terrain measures a region's outline
// by runs of edges (`TerrainField::PolygonRuns`) and knows the open land
// by its sign alone; each answer is the plain one bit for bit, in every
// region file, on a half-cell grid over the region and past its edges.
TEST(RegionFile_outlineRunsMeasureAsThePolygon) {
    auto bits = [](double v) {
        uint64_t b;
        std::memcpy(&b, &v, sizeof b);
        return b;
    };
    size_t regions = 0, points = 0, off = 0;
    for (const auto& entry : std::filesystem::directory_iterator("regions")) {
        if (entry.path().extension() != ".json") continue;
        std::ifstream in(entry.path());
        std::stringstream text;
        text << in.rdbuf();
        const RegionFile f = parsed(text.str());
        const Built b = build(f);
        if (!b.map.edges || b.map.edges->outline.empty()) continue;
        const TerrainField field(b.map);
        const RegionEdges& e = *b.map.edges;
        regions += 1;
        bool same = true;
        const GroundRect& r = b.map.bounds;
        for (double z = r.minZ - 6; z < r.maxZ + 6; z += 0.5)
            for (double x = r.minX - 6; x < r.maxX + 6; x += 0.5) {
                const Vec2 p(x + 0.013, z + 0.029);
                points += 1;
                const double plain = signedDistance(p, e.outline);
                same = same && bits(field.outlineRuns.signedDistance(p, e.outline)) == bits(plain);
                same = same && field.outlineRuns.inside(p, e.outline) == std::signbit(plain);
                same = same && bits(field.landOutside(p)) == bits(landOutside(p, e.outline, e.islands));
                const TerrainField::Wall w = field.wall(p), n = field.wallNear(p);
                same = same && bits(w.coverage) == bits(n.coverage) && bits(w.edge) == bits(n.edge);
                if (w.outside > 0 && w.outside < field.wallReach()) same = same && bits(w.outside) == bits(n.outside);
                else if (w.outside <= 0) same = same && n.outside <= 0;
                else same = same && n.outside >= field.wallReach();
                if (field.openLand(p)) {
                    off += 1;
                    same = same && w.coverage == 0;
                }
            }
        if (!same) std::printf("    %s: the runs differ\n", f.id.c_str());
        EXPECT_TRUE(same);
    }
    EXPECT_TRUE(regions >= 9);
    EXPECT_TRUE(off > 0 && off < points);
}
