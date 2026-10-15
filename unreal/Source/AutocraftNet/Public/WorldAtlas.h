// The world map's model (M2, docs/mmorpg-m8.md; the game's `SAcWorldMap`):
// the satellite map of a world, engine-free so the net tests check it.
//
// - **World cells.** Every region file is laid on the world's grid at its
//   column and row, each column as wide as its widest region and each row as
//   deep as its deepest, a region in its cell's north-west corner (a world of
//   medium regions: 144 × 96 cells a region, seamless). x east, y south. A
//   region's cells (docs/regions.md: from its north-west corner) are world
//   cells less its corner; its ground (the map's, centred) is cells less half
//   its size.
// - **The view.** A point of the world in the middle of the screen and a
//   scale (screen points a cell); the wheel zooms about the pointer, a drag
//   pans, kept over the world.
// - **Items.** What the map draws on the satellite image, each with what its
//   tooltip says: the pilot, other pilots, mercenaries, the Brood in groups,
//   realm buildings, deposits and wells (the pilot's region: the mirror's),
//   blueprint sites the pilot knows of, pads, starfalls and their craters,
//   the Journal's missions and KSTR's places. `pick` finds the one under the
//   pointer.
// - **The ringshadow band** over a region, as a polygon on the map.
#pragma once

#include "AutocraftNetApi.h"
#include "NetProtocol.h"

#include "Progression.h"
#include "Ringshadow.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ac {
class Simulation;
}

namespace ac::net::atlas {

/// A region's place on the map, world cells.
struct Place {
    /// Index in the World message's `regions`.
    int64_t index = 0;
    std::string id;
    std::string name;
    double x = 0, y = 0, width = 0, depth = 0;
    /// The pilot has been there (else dark, its outline only).
    bool seen = false;
    Vec2 corner() const { return Vec2(x, y); }
    Vec2 centre() const { return Vec2(x + width / 2, y + depth / 2); }
    bool contains(Vec2 p) const { return p.x >= x && p.x <= x + width && p.y >= y && p.y <= y + depth; }
    /// Region cells → world cells, and region ground (centred) → world.
    Vec2 fromCells(Vec2 cells) const { return Vec2(x + cells.x, y + cells.y); }
    Vec2 fromGround(Vec2 ground) const { return Vec2(x + width / 2 + ground.x, y + depth / 2 + ground.y); }
    Vec2 toGround(Vec2 world) const { return Vec2(world.x - x - width / 2, world.y - y - depth / 2); }
    bool operator==(const Place&) const = default;
};

/// The regions on the grid. `seen`: region ids the pilot has been to.
AUTOCRAFTNET_API std::vector<Place> layout(const WorldMessage& world, const std::set<std::string>& seen);

struct Bounds {
    double minX = 0, minY = 0, maxX = 0, maxY = 0;
    double width() const { return maxX - minX; }
    double depth() const { return maxY - minY; }
    Vec2 centre() const { return Vec2((minX + maxX) / 2, (minY + maxY) / 2); }
    bool operator==(const Bounds&) const = default;
};
AUTOCRAFTNET_API Bounds bounds(const std::vector<Place>& places);
/// The place a world point lies in (nil: none).
AUTOCRAFTNET_API const Place* placeAt(const std::vector<Place>& places, Vec2 world);
AUTOCRAFTNET_API const Place* placeOf(const std::vector<Place>& places, int64_t index);
AUTOCRAFTNET_API const Place* placeOf(const std::vector<Place>& places, const std::string& id);

/// What part of the world the screen shows.
struct AUTOCRAFTNET_API View {
    /// The world point in the middle of the screen, and screen points a cell.
    Vec2 centre{0, 0};
    double scale = 1;
    /// The closest and the furthest the wheel zooms: points a cell.
    static constexpr double minScale = 0.5;
    static constexpr double maxScale = 24;
    /// A wheel notch zooms by this.
    static constexpr double notch = 1.25;

    Vec2 toScreen(Vec2 world, Vec2 screen) const;
    Vec2 toWorld(Vec2 point, Vec2 screen) const;
    /// Zooms by `factor` keeping the world point under `point` where it is
    /// (within `minScale`…`maxScale`, and never smaller than `floor`).
    void zoomAt(Vec2 point, Vec2 screen, double factor, double floor = 0);
    /// Moves the picture by `points` (a drag).
    void pan(Vec2 points);
    /// Keeps the centre over the world.
    void clamp(const Bounds& world);
    /// The whole world on the screen with `margin` points round it.
    static View fit(const Bounds& world, Vec2 screen, double margin);
    bool operator==(const View&) const = default;
};

/// What an item on the map is. The order is the tie-break of `pick`: the
/// first wins between two as near.
enum class Kind : uint8_t {
    you, mission, site, pad, merc, pilot, bugs, building, starfall, crater, deposit, well, contract,
    /// A canyon zone's game (Z2): its state over its prize.
    zone,
};

struct Item {
    Kind kind = Kind::deposit;
    /// Index in the World message's `regions`.
    int64_t region = 0;
    /// World cells.
    Vec2 at{0, 0};
    /// Its size on the ground, cells (a building's radius, a mission's
    /// area; 0: an icon only).
    double radius = 0;
    /// The tooltip: its name and kind, then what is worth knowing.
    std::string title;
    std::vector<std::string> lines;
    /// What it is in the game: a unit or a building id, a mission id; a
    /// site's id ("<region id>/<site name>"), a pad's name.
    int64_t ref = 0;
    std::string key;
    /// How big its icon is, 0…1 (a deposit's ore left of its first; a
    /// building's share built; the Brood's count); a building's kind; a mission's tier; whether it is the pilot's own (a
    /// mercenary, a pad), opened (a site), being eaten (a deposit).
    double size = 1;
    std::optional<StructureKind> building;
    CitadelMission::Tier tier = CitadelMission::Tier::easy;
    bool own = false;
    bool flag = false;
    /// A unit's heading, radians in the ground plane (the pilot's arrow).
    double heading = 0;
    bool operator==(const Item&) const = default;
};

/// A blueprint site the pilot knows of, read from its region file by the
/// game (the World message carries no sites).
struct KnownSite {
    std::string region;
    std::string name;
    SiteKind kind = SiteKind::crate;
    StructureKind blueprint = StructureKind::citadel;
    /// Region cells.
    Vec2 at{0, 0};
    bool found = false;
};

/// A KSTR contract's place (`AcProgressHud::ContractMarks`): its region and,
/// when it has one, where in it (region cells).
struct ContractMark {
    std::string region;
    std::optional<Vec2> at;
    std::string title;
};

/// A canyon zone (Z2) as the game reads it from its region file and the sky:
/// its region, its prize's middle (region cells) and reach, its name and
/// state; `alert` while a wave is out or the zone has fallen.
struct ZoneMark {
    std::string region;
    Vec2 at{0, 0};
    double radius = 0;
    std::string title;
    std::vector<std::string> lines;
    bool alert = false;
};

/// What the items are made from.
struct Input {
    const WorldMessage* world = nullptr;
    const ProgressMessage* progress = nullptr;
    /// The mirror of the pilot's region (`world->here`), its driven unit.
    const Simulation* here = nullptr;
    std::optional<int64_t> driven;
    /// The world's sky (its clock, the starfalls).
    std::optional<Sky> sky;
    std::vector<KnownSite> sites;
    std::vector<ContractMark> contracts;
    std::vector<ZoneMark> zones;
};

/// Everything to draw, in `Kind` order.
AUTOCRAFTNET_API std::vector<Item> items(const Input& in, const std::vector<Place>& places);

/// The item under screen point `point` (nil: none): the one whose edge is
/// nearest, within `reach` points of its icon (or inside its radius on the
/// ground); between two as near (2 points), the first `Kind`.
AUTOCRAFTNET_API std::optional<size_t> pick(const std::vector<Item>& items, const View& view, Vec2 screen, Vec2 point,
                                            double reach = 10);

/// The Brood near each other in groups: units within `link` cells of one
/// of a group join it. Returns each group's members (indices into `at`).
AUTOCRAFTNET_API std::vector<std::vector<size_t>> groups(const std::vector<Vec2>& at, double link);

/// The ringshadow band over a place: the polygon (world cells) of the
/// region's rectangle between the band's trailing and leading edges (empty:
/// the band is off the region).
AUTOCRAFTNET_API std::vector<Vec2> band(const Place& place, const Ringshadow::Band& band);

/// "1,240".
AUTOCRAFTNET_API std::string thousands(int64_t n);

} // namespace ac::net::atlas
