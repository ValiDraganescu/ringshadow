// C++ only: a region of the MMORPG world as a file (docs/regions.md). A
// generated base (today's map from a style, a size and a seed) and crafted
// layers on top: height edits, deposits, wells, doodads, pads, spawn points
// and the edges' openings. AI agents write these files; `autocraft-region`
// (unreal/server/region) checks them and draws them.
#pragma once

#include "AutocraftCoreApi.h"

#include "Progression.h"
#include "SimdMath.h"
#include "Types.h"
#include "WindowMaps.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ac::region {

/// A region file's positions are cells from the region's north-west
/// corner: x east, z south (down the preview), 0…width and 0…depth.

/// An area of ground: an outline, a rectangle or a circle.
struct Area {
    enum class Shape : uint8_t { polygon, rect, circle };
    Shape shape = Shape::polygon;
    /// polygon: its corners; rect: two opposite corners; circle: the centre.
    std::vector<Vec2> points;
    double radius = 0;
    /// The outline (a circle as 32 sides).
    std::vector<Vec2> outline() const;
    bool operator==(const Area&) const = default;
};

/// One edit of the `height` layer.
struct Edit {
    HeightEdit::Kind op = HeightEdit::Kind::raise;
    /// raise, lower, flatten.
    Area area;
    /// ridge, cut: its line; ramp: `from` then `to`.
    std::vector<Vec2> line;
    int64_t level = 0;
    /// ramp: the `from` end's level (`level` is the `to` end's).
    int64_t fromLevel = 0;
    double width = 0;
    double slope = 0;
    bool operator==(const Edit&) const = default;
};

struct Deposit {
    Vec2 at;
    int64_t amount = 0;
    /// Degrees, the patch's long axis from east toward south.
    double angle = 0;
    bool operator==(const Deposit&) const = default;
};

struct WellSite {
    Vec2 at;
    int64_t amount = 0;
    bool operator==(const WellSite&) const = default;
};

struct Prop {
    Doodad::Kind kind = Doodad::Kind::rock;
    Vec2 at;
    double rotation = 0;
    double scale = 1;
    bool operator==(const Prop&) const = default;
};

/// A named landing pad or spawn point.
struct Place {
    std::string name;
    Vec2 at;
    bool operator==(const Place&) const = default;
};

/// A blueprint site (docs/prospector-progression.md, "Blueprint sites"):
/// one blueprint, which each pilot finds for themselves.
struct Site {
    std::string name;
    SiteKind kind = SiteKind::crate;
    Vec2 at;
    /// The building it teaches (never the Garrison: held back).
    StructureKind blueprint = StructureKind::citadel;
    bool operator==(const Site&) const = default;
};

enum class Edge : uint8_t { north, east, south, west };

/// The ground's look (docs/regions.md, "ground"): the palette the game
/// paints the region's ground with. `plain` is the game's own brown dirt.
enum class Ground : uint8_t { plain, rust, salt, ash, glass, slag, frost, ochre };

/// A passable stretch of an edge: cells along it (x on north and south,
/// z on east and west), and the level its passage's floor meets the edge at
/// (nil: the ground as it is there).
struct Span {
    double from = 0, to = 0;
    std::optional<int64_t> level;
    bool operator==(const Span&) const = default;
};

/// The region's natural border (docs/regions.md, "border"; W4): the outline
/// of its playable land, and the walls' level outside it.
struct Border {
    std::vector<Vec2> outline;
    /// Islands of high rock inside the outline (W4b), walled the same.
    std::vector<std::vector<Vec2>> islands;
    int64_t level = 4;
    /// The rim's rise past the outline, cells (0: a cliff; S1,
    /// docs/world-sculpting.md).
    double slope = 0;
    /// Each island's own level (0: `level`) and kind (S2), one per island
    /// (an island written as a bare outline: 0 and a wall).
    std::vector<int64_t> islandLevels;
    std::vector<IslandKind> islandKinds;
    bool operator==(const Border&) const = default;
};

/// A canyon zone (Z1, docs/regions.md "zones"): where the Brood comes in
/// (its tunnels' mouths), and the ways it walks from each to the prize (a
/// field of rich ore and an MH well). Z4: no build spots; pilots build
/// anywhere standable in it under the ordinary rules (an old file's `spots`
/// are read and ignored).
struct Zone {
    struct Entrance {
        std::string name;
        Vec2 at;
        /// Degrees: the way the mouth faces (into the canyon), from east
        /// toward north, as a doodad's rotation turns its +x.
        double facing = 0;
        /// Z3: a Gloamwing roost on a ledge over the canyon, not a tunnel:
        /// only flyers come out of it (and fly its paths over the walls); its
        /// mark is a small nest of chitin, not a tunnel's mouth.
        bool roost = false;
        bool operator==(const Entrance&) const = default;
    };
    /// From an entrance (its name) along the canyon's floor to the prize.
    struct Path {
        std::string from;
        std::vector<Vec2> points;
        bool operator==(const Path&) const = default;
    };
    std::string id;
    std::string name;
    std::vector<Vec2> outline;
    std::vector<Entrance> entrances;
    std::vector<Path> paths;
    /// The prize: indices into the file's `deposits` and `wells`.
    std::vector<int64_t> prizeDeposits, prizeWells;
    /// Z3: a flooded floor (brine): the Brood's walkers wade through it at
    /// `floodSlow` of their speed; flyers pass over. Empty: none.
    std::vector<Vec2> flood;
    double floodSlow = 0.5;
    bool operator==(const Zone&) const = default;
};

struct RegionFile {
    std::string id;
    std::string name;
    MapSize size = MapSize::medium;
    MapStyle style = MapStyle::highlands;
    Ground ground = Ground::plain;
    /// How many bugs of the Brood the region draws (docs/creatures.md,
    /// "Brood level"): 0 none, 1 Prospect Landing's few … 4 the Matriarch's.
    int64_t brood = 0;
    /// nil: the game's own map of that style and size.
    std::optional<uint64_t> seed;
    /// The base without its ore and wells, without its rocks and plants.
    bool dropResources = false;
    bool dropDoodads = false;
    std::vector<Edit> height;
    /// nil: no walls (the rectangle's edges stop units).
    std::optional<Border> border;
    std::vector<Deposit> deposits;
    std::vector<WellSite> wells;
    std::vector<Prop> doodads;
    std::vector<Place> pads;
    std::vector<Place> spawns;
    std::vector<Site> sites;
    /// The Matriarch's nest (docs/creatures.md, "The Matriarch"; N3): her
    /// home and her horde's, in a brood level 4 region. Nil: none.
    std::optional<Vec2> nest;
    /// By `Edge`.
    std::array<std::vector<Span>, 4> openings;
    /// Canyon zones (Z1).
    std::vector<Zone> zones;

    /// Width and depth, cells.
    Vec2 extent() const;
    bool operator==(const RegionFile&) const = default;
};

/// Something wrong with a region file: where in it, what, and the region
/// position it is about (if any).
struct Problem {
    std::string where;
    std::string message;
    std::optional<Vec2> at;
    /// "deposits[2] at (40, 12.5): on a cliff face".
    std::string text() const;
};

struct Parsed {
    std::optional<RegionFile> file;
    std::vector<Problem> problems;
};

/// Reads a region file. Any problem (bad JSON, a missing or unknown key, a
/// wrong type or name, a value out of range) leaves `file` nil.
AUTOCRAFTCORE_API Parsed parse(std::string_view json);

/// The file as JSON, in the reference's key order.
AUTOCRAFTCORE_API std::string write(const RegionFile& file);

/// A file with only the base.
AUTOCRAFTCORE_API RegionFile starter(std::string id, MapStyle style, uint64_t seed, MapSize size);

/// The highest level a height edit reaches (one level is 2.4 high).
constexpr int64_t maxLevel = 4;
/// The highest a free-standing island stands, levels (S2).
constexpr int64_t maxIslandLevel = 6;

constexpr double padRadius = 3.0;
/// The nest's mound (N3): rocks and plants within this many cells of it
/// give way; no pad within `nestClear` of it (the horde's ring).
constexpr double nestMound = 6.0;
constexpr double nestClear = 30.0;
/// A path starts within this of its entrance and ends within `prizeReach`
/// of the prize's middle; an entrance stands within `mouthReach` of a wall.
constexpr double pathStart = 3.0;
constexpr double prizeReach = 8.0;
constexpr double mouthReach = 4.0;
/// A flight leaves from within this many cells of a pad's centre: the
/// foundation plate drawn on each pad is this wide a disc (the server's and
/// the client's one rule, `nearestPad`).
constexpr double padReach = 4.0;
/// How deep into the region an opening's lane is checked, cells.
constexpr double laneDepth = 2.0;
/// An opening with a level: its passage's floor is flat at that level this
/// deep in from the edge, then joins the ground over `mouthSlope` (W4).
constexpr double mouthDepth = 5.0;
constexpr double mouthSlope = 5.0;
/// The walls' rocks: about one every this many cells each way (W4).
constexpr double wallRockSpacing = 3.4;

/// Scarcity (E1; docs/regions.md, "Scarcity"): a region of a world (brood
/// level 1 or more) holds at most `veinsMost` ore veins and `wellsMost` MH
/// well. A vein is a tight cluster of `moundsFewest` to `moundsMost`
/// deposits (mounds), each within `veinReach` cells of the vein's middle;
/// deposits within `veinLink` cells of each other are one vein.
constexpr int64_t veinsMost = 2;
constexpr int64_t moundsFewest = 3;
constexpr int64_t moundsMost = 5;
constexpr int64_t wellsMost = 1;
constexpr double veinLink = 5.5;
constexpr double veinReach = 4.5;
/// A deposit grows back this share of its full ore a world day, up to full
/// (the server's regions, on the world's clock: `WorldMap::regrowVeins`).
constexpr double regrowPerDay = 0.1;
/// A region's ore in all, by brood level: 1 about 6,000-8,000, 2
/// 10,000-12,000, 3 and 4 15,000-20,000. Nil: no limit (level 0).
struct OreRange {
    int64_t fewest = 0;
    int64_t most = 0;
};
AUTOCRAFTCORE_API std::optional<OreRange> oreRange(int64_t brood);
/// The deposits in veins (indices into `deposits`), each vein in the order
/// of its first deposit.
AUTOCRAFTCORE_API std::vector<std::vector<size_t>> veins(const std::vector<Deposit>& deposits);

/// The region built: its map (ground coordinates, centred) and its places.
struct AUTOCRAFTCORE_API Built {
    MapDefinition map;
    /// Ground coordinates.
    std::vector<Place> pads, spawns;
    std::vector<Site> sites;
    /// The Matriarch's nest, ground coordinates (nil: none).
    std::optional<Vec2> nest;
    std::array<std::vector<Span>, 4> openings;
    /// Canyon zones, ground coordinates; their prizes as indices into the
    /// built map's `deposits` and `wells`.
    std::vector<Zone> zones;
    /// Base rocks and plants taken away because they stood on something
    /// crafted (a deposit, a well, a pad, a spawn point, a site, an
    /// opening's lane).
    int64_t cleared = 0;

    Vec2 extent() const;
    /// Region position to ground, and back.
    Vec2 ground(Vec2 r) const;
    Vec2 regionAt(Vec2 g) const;
    /// The point `depth` cells into the region at `along` on an edge,
    /// region coordinates.
    Vec2 edgePoint(Edge e, double along, double depth) const;
};

/// The map: the base generated, then the layers in order. `file` must have
/// parsed.
AUTOCRAFTCORE_API Built build(const RegionFile& file);

/// What the built region gets wrong: deposits, wells, pads and spawn points
/// on cliffs, in rock, on top of each other or cut off; ramps that do not
/// meet their levels or are too steep; openings blocked or cut off from
/// the region's open ground; sites on a cliff, in rock, on ore, a well or a
/// pad's plate, or cut off (a vault: off high ground, or out of reach of
/// both a ramp and a cliff hop); a nest off open ground, outside a brood
/// level 4 region, or with a pad in its ring. Empty: fine.
AUTOCRAFTCORE_API std::vector<Problem> validate(const RegionFile& file, const Built& built);

/// How a pilot reaches a site: across the open ground, up a ramp to high
/// ground joined to it, only with Cliff hop (a vault), or not at all.
enum class SiteReach : uint8_t { walk, ramp, cliffHop, none };
AUTOCRAFTCORE_API std::string_view siteReachName(SiteReach r);
/// By `built.sites`.
AUTOCRAFTCORE_API std::vector<SiteReach> siteReach(const Built& built);
/// `validate`, and `siteReach` from the same walk (the world server keeps
/// it from the load, not from a tick).
AUTOCRAFTCORE_API std::vector<Problem> validate(const RegionFile& file, const Built& built, std::vector<SiteReach>& reach);

AUTOCRAFTCORE_API std::string_view edgeName(Edge e);

/// The pad nearest `at` and how far its centre is (cells; `pads` and `at` in
/// the same space). Nil: no pads. At a pad: `distance <= padReach`.
struct PadNear {
    std::string name;
    double distance = 0;
};
AUTOCRAFTCORE_API std::optional<PadNear> nearestPad(const std::vector<Place>& pads, Vec2 at);

/// Whether a building's square footprint (centre `at`, half side `halfSide`
/// as `Rules::radius`) touches a pad's plate, the disc `padReach` round its
/// centre (`pads` and `at` in the same space). A world's server refuses such
/// a building with `padPlateRefusal`, and the client's ghost shows it red
/// with that reason: the one rule on both ends, like `nearestPad`.
AUTOCRAFTCORE_API bool touchesPadPlate(const std::vector<Place>& pads, Vec2 at, double halfSide);
inline constexpr const char* padPlateRefusal = "Too close to a landing pad";

} // namespace ac::region

namespace ac {
template <> struct EnumInfo<region::Ground> {
    static constexpr std::array<std::string_view, 8> names{"plain", "rust", "salt", "ash", "glass", "slag", "frost", "ochre"};
};
} // namespace ac
