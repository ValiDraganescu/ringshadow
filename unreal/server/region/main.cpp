// autocraft-region: make, check, build and draw region files
// (docs/regions.md). Engine-free.
//
//   autocraft-region new --style S --seed N --size Z [--id ID] [-o FILE]
//   autocraft-region validate FILE
//   autocraft-region build FILE
//   autocraft-region preview FILE -o OUT.png [--scale PX]
//   autocraft-region edges FILE [--depth D]
//   autocraft-region levels FILE [--bare]
//   autocraft-region format FILE

#include "Preview.h"

#include "NavGrid.h"
#include "RegionFile.h"
#include "TerrainField.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ac;
using namespace ac::region;

int usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  autocraft-region new --style highlands|openField|ridge|badlands --seed N --size small|medium|large|huge\n"
                 "                       [--id ID] [-o FILE]      a region file with only the base\n"
                 "  autocraft-region validate FILE                problems, one a line (exit 1 if any)\n"
                 "  autocraft-region build FILE                   build the map, print a summary and the problems\n"
                 "  autocraft-region preview FILE -o OUT.png [--scale PX]\n"
                 "                                                draw it from above, problems marked\n"
                 "  autocraft-region edges FILE [--depth D]       the ground's level along each edge, D cells in\n"
                 "                                                (default 6): where a passage's floor can meet\n"
                 "  autocraft-region levels FILE [--bare]         the ground's level a cell at a time (# a cliff, o rock\n"
                 "                                                in the way), --bare without the border's walls\n"
                 "  autocraft-region format FILE                  rewrite it in the tool's own layout\n"
                 "Format: docs/regions.md\n");
    return 2;
}

std::optional<std::string> option(const std::vector<std::string>& args, const std::string& name) {
    for (size_t i = 0; i + 1 < args.size(); i++) if (args[i] == name) return args[i + 1];
    return std::nullopt;
}

std::optional<std::string> readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

/// The file parsed, or its problems printed (nil).
std::optional<RegionFile> load(const std::string& path) {
    const auto text = readFile(path);
    if (!text) { std::fprintf(stderr, "cannot read %s\n", path.c_str()); return std::nullopt; }
    const Parsed parsed = parse(*text);
    for (const Problem& p : parsed.problems) std::printf("%s: %s\n", path.c_str(), p.text().c_str());
    if (parsed.problems.empty() && parsed.file) return parsed.file;
    std::printf("%zu problem%s; nothing built\n", parsed.problems.size(), parsed.problems.size() == 1 ? "" : "s");
    return std::nullopt;
}

template <class E> std::optional<E> named(const std::string& s) {
    const auto& names = EnumInfo<E>::names;
    for (size_t i = 0; i < names.size(); i++) if (names[i] == s) return static_cast<E>(i);
    return std::nullopt;
}

std::string point(Vec2 p) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "(%.4g, %.4g)", p.x, p.y);
    return buf;
}

int newFile(const std::vector<std::string>& args) {
    const auto style = option(args, "--style"), seed = option(args, "--seed"), size = option(args, "--size");
    if (!style || !seed || !size) return usage();
    const auto s = named<MapStyle>(*style);
    const auto z = named<MapSize>(*size);
    char* end = nullptr;
    const unsigned long long n = std::strtoull(seed->c_str(), &end, 10);
    if (!s || !z || seed->empty() || *end != '\0') return usage();
    const std::string id = option(args, "--id").value_or(*style + "-" + *seed);
    const std::string text = write(starter(id, *s, n, *z));
    if (const auto out = option(args, "-o")) {
        std::ofstream f(*out, std::ios::binary);
        f << text;
        if (!f) { std::fprintf(stderr, "cannot write %s\n", out->c_str()); return 1; }
        std::printf("wrote %s\n", out->c_str());
    } else {
        std::fputs(text.c_str(), stdout);
    }
    return 0;
}

void printProblems(const std::string& path, const std::vector<Problem>& problems) {
    for (const Problem& p : problems) std::printf("%s: %s\n", path.c_str(), p.text().c_str());
}

int summary(const std::string& path) {
    const auto file = load(path);
    if (!file) return 1;
    const Built built = build(*file);
    const MapDefinition& m = built.map;
    const Vec2 x = built.extent();
    std::printf("%s: %s\n", file->id.c_str(), file->name.c_str());
    std::printf("size: %s, %g x %g cells; base: %s, %s%s\n", std::string(rawValue(file->size)).c_str(), x.x, x.y,
                std::string(rawValue(file->style)).c_str(),
                file->seed ? ("seed " + std::to_string(*file->seed)).c_str() : "the game's own map",
                file->dropResources || file->dropDoodads
                    ? (std::string(", dropped") + (file->dropResources ? " resources" : "") + (file->dropDoodads ? " doodads" : "")).c_str()
                    : "");
    std::printf("ground: %zu plateaus, %zu ramps (base); %zu height edits\n", m.plateaus.size(), m.ramps.size(),
                m.heightEdits.size());
    size_t baseOre = 0, baseWells = 0;
    int64_t ore = 0, mh = 0;
    for (const BaseSite& b : m.bases) {
        baseOre += b.ore.size();
        baseWells += b.wells.size();
        for (const OreSpec& o : b.ore) ore += o.amount;
    }
    for (const OreSpec& o : m.deposits) ore += o.amount;
    const GameState state = GameState::new_(m);
    if (state.wells) for (const Well& w : *state.wells) mh += w.remaining;
    std::printf("ore: %zu deposits (%zu base, %zu crafted), %lld ore; MH: %zu wells (%zu base, %zu crafted), %lld MH\n",
                baseOre + m.deposits.size(), baseOre, m.deposits.size(), static_cast<long long>(ore),
                baseWells + m.wells.size(), baseWells, m.wells.size(), static_cast<long long>(mh));
    std::printf("doodads: %zu (%zu crafted; %lld base ones cleared for crafted things)\n", m.doodads.size(),
                file->doodads.size(), static_cast<long long>(built.cleared));
    for (const Place& p : file->pads) std::printf("pad \"%s\" at %s\n", p.name.c_str(), point(p.at).c_str());
    for (const Place& p : file->spawns) std::printf("spawn \"%s\" at %s\n", p.name.c_str(), point(p.at).c_str());
    const auto reach = siteReach(built);
    for (size_t i = 0; i < file->sites.size(); i++) {
        const Site& t = file->sites[i];
        std::printf("site \"%s\": %s, %s, at %s, reached %s\n", t.name.c_str(), std::string(rawValue(t.kind)).c_str(),
                    std::string(rawValue(t.blueprint)).c_str(), point(t.at).c_str(), std::string(siteReachName(reach[i])).c_str());
    }
    for (size_t e = 0; e < 4; e++)
        for (const Span& s : file->openings[e])
            std::printf("opening: %s edge %g..%g\n", std::string(edgeName(static_cast<Edge>(e))).c_str(), s.from, s.to);
    const TerrainField field(m);
    NavGrid nav(m, field);
    GameState ground = state;
    ground.structures.clear();
    ground.units.clear();
    nav.setDynamic(ground);
    int64_t free = 0;
    for (int64_t i = 0; i < nav.width * nav.height; i++) free += nav.free(i) ? 1 : 0;
    const double all = static_cast<double>(nav.width * nav.height);
    const int64_t main = nav.mainRegion >= 0 ? nav.regionCells[static_cast<size_t>(nav.mainRegion)] : 0;
    std::printf("open ground: %.0f%% of the region; the main open area %.0f%%\n", 100.0 * static_cast<double>(free) / all,
                100.0 * static_cast<double>(main) / all);
    const auto problems = validate(*file, built);
    printProblems(path, problems);
    std::printf("%zu problem%s\n", problems.size(), problems.size() == 1 ? "" : "s");
    return problems.empty() ? 0 : 1;
}

int check(const std::string& path) {
    const auto file = load(path);
    if (!file) return 1;
    const Built built = build(*file);
    const auto problems = validate(*file, built);
    printProblems(path, problems);
    // How each vault is reached: by a ramp, or only by a cliff hop.
    const auto reach = siteReach(built);
    for (size_t i = 0; i < file->sites.size(); i++)
        if (file->sites[i].kind == SiteKind::vault && reach[i] != SiteReach::none)
            std::printf("%s: sites[%zu] \"%s\" (vault) is reached %s\n", path.c_str(), i, file->sites[i].name.c_str(),
                        std::string(siteReachName(reach[i])).c_str());
    if (problems.empty()) std::printf("%s: ok\n", path.c_str());
    else std::printf("%zu problem%s\n", problems.size(), problems.size() == 1 ? "" : "s");
    return problems.empty() ? 0 : 1;
}

int draw(const std::string& path, const std::vector<std::string>& args) {
    const auto out = option(args, "-o");
    if (!out) return usage();
    const int scale = option(args, "--scale") ? std::atoi(option(args, "--scale")->c_str()) : 0;
    const auto file = load(path);
    if (!file) return 1;
    const Built built = build(*file);
    const auto problems = validate(*file, built);
    printProblems(path, problems);
    const regiontool::Image image = regiontool::preview(*file, built, problems, scale);
    if (!regiontool::writePng(image, *out)) { std::fprintf(stderr, "cannot write %s\n", out->c_str()); return 1; }
    std::printf("wrote %s (%dx%d), %zu problem%s marked\n", out->c_str(), image.width, image.height, problems.size(),
                problems.size() == 1 ? "" : "s");
    return 0;
}

/// Each edge's ground, `depth` cells in: a level digit every 2 cells (`#`:
/// a cliff), the openings under it, so a passage's floor level can be
/// chosen to meet the neighbour's (W4).
int edges(const std::string& path, const std::vector<std::string>& args) {
    const auto file = load(path);
    if (!file) return 1;
    const double depth = option(args, "--depth") ? std::atof(option(args, "--depth")->c_str()) : 6.0;
    const Built built = build(*file);
    const TerrainField field(built.map);
    const double H = built.map.levelHeight;
    for (size_t e = 0; e < 4; e++) {
        const Edge edge = static_cast<Edge>(e);
        const double length = e % 2 == 0 ? built.extent().x : built.extent().y;
        std::string levels, opens;
        for (double a = 1; a < length; a += 2) {
            const Vec2 p = built.ground(built.edgePoint(edge, a, depth));
            const double h = field.height(p);
            const double dx = field.height(p + Vec2(0.5, 0)) - field.height(p - Vec2(0.5, 0));
            const double dz = field.height(p + Vec2(0, 0.5)) - field.height(p - Vec2(0, 0.5));
            const bool cliff = std::sqrt(dx * dx + dz * dz) > NavGrid::maxSlope;
            levels += cliff ? '#' : static_cast<char>('0' + std::clamp<long>(std::lround(h / H), 0, 9));
            bool open = false;
            for (const Span& s : file->openings[e]) open = open || (a >= s.from && a <= s.to);
            opens += open ? '^' : ' ';
        }
        std::printf("%-5s %s\n      %s\n", std::string(edgeName(edge)).c_str(), levels.c_str(), opens.c_str());
    }
    std::printf("a digit every 2 cells from the west (north, south) or the north (east, west) end, %g cells in; ^ an opening\n", depth);
    return 0;
}

/// The ground a cell at a time, a row a line from the north: its level
/// digit, `#` a cliff, `o` rock in the way of a unit (W4b: the region
/// shapes' tool reads it to keep the land joined). `--bare`: as if the
/// file had no border.
int levels(const std::string& path, const std::vector<std::string>& args) {
    auto file = load(path);
    if (!file) return 1;
    if (std::find(args.begin(), args.end(), "--bare") != args.end()) file->border.reset();
    const Built built = build(*file);
    const TerrainField field(built.map);
    NavGrid nav(built.map, field);
    GameState ground = GameState::new_(built.map);
    ground.structures.clear();
    ground.units.clear();
    nav.setDynamic(ground);
    const double H = built.map.levelHeight;
    const Vec2 x = built.extent();
    for (int z = 0; z < static_cast<int>(x.y); z++) {
        std::string row;
        for (int i = 0; i < static_cast<int>(x.x); i++) {
            const Vec2 p = built.ground(Vec2(i + 0.5, z + 0.5));
            const double dx = field.height(p + Vec2(0.5, 0)) - field.height(p - Vec2(0.5, 0));
            const double dz = field.height(p + Vec2(0, 0.5)) - field.height(p - Vec2(0, 0.5));
            const auto cell = nav.index(p);
            if (std::sqrt(dx * dx + dz * dz) > NavGrid::maxSlope) row += '#';
            else if (!cell || !nav.free(*cell) || nav.inRock(p)) row += 'o';
            else row += static_cast<char>('0' + std::clamp<long>(std::lround(field.height(p) / H), 0, 9));
        }
        std::printf("%s\n", row.c_str());
    }
    return 0;
}

int format(const std::string& path) {
    const auto file = load(path);
    if (!file) return 1;
    std::ofstream f(path, std::ios::binary);
    f << write(*file);
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
    std::printf("wrote %s\n", path.c_str());
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage();
    const std::string& command = args[0];
    if (command == "new") return newFile(args);
    if (args.size() < 2) return usage();
    if (command == "validate") return check(args[1]);
    if (command == "build") return summary(args[1]);
    if (command == "preview") return draw(args[1], args);
    if (command == "edges") return edges(args[1], args);
    if (command == "levels") return levels(args[1], args);
    if (command == "format") return format(args[1]);
    return usage();
}
