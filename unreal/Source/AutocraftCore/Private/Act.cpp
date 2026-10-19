// The act key's targets (Act.h; C5, docs/acting.md).

#include "Act.h"

#include "Defence.h"
#include "Mule.h"
#include "Shuttle.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ac {

namespace {

std::array<Vec2, 4> corners(const Block& b) {
    const double c = std::cos(b.angle), s = std::sin(b.angle);
    const Vec2 x(b.half.x * c, b.half.x * s), y(-b.half.y * s, b.half.y * c);
    return {b.centre + x + y, b.centre + x - y, b.centre - x - y, b.centre - x + y};
}

/// The two boxes' shadows on `axis` keep apart (a separating axis).
bool apart(const Block& a, const Block& b, Vec2 axis) {
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    const Block* both[2] = {&a, &b};
    for (int k = 0; k < 2; k++)
        for (const Vec2 p : corners(*both[k])) {
            const double d = p.x * axis.x + p.y * axis.y;
            lo[k] = std::min(lo[k], d);
            hi[k] = std::max(hi[k], d);
        }
    return hi[0] < lo[1] || hi[1] < lo[0];
}

/// A box of a building, from its centre: `at` and `half` in its frame, the
/// building's ground under it.
Block onBuilding(const Structure& s, Vec2 at, Vec2 half, double z0, double z1, double angle = 0) {
    return Block{s.position + at, half, angle, z0, z1};
}

/// A box of the Mule `m`, in its own frame (x ahead along its heading).
Block onMule(const Unit& m, Vec2 at, Vec2 half, double z0, double z1) {
    return Block{Mule::point(m, at), half, m.heading, z0, z1};
}

/// A box round a button at `b` in the ship's frame.
Block button(Vec3 b, double half) { return Block{Vec2(b.x, b.y), Vec2(half, half), 0, b.z - half, b.z + half}; }

/// A ship-frame box in the world's frame.
Block inWorld(const Shuttle& s, Block b) {
    b.centre = ShuttleBay::world(s, b.centre);
    b.angle += s.heading;
    return b;
}

} // namespace

Block Act::body(const Unit& u) {
    if (u.stats().hauler) return Block{u.position, Vec2(Mule::length / 2, Mule::width / 2), u.heading, 0, bodyTop(u)};
    const double r = Rules::radius(u.kind);
    return Block{u.position, Vec2(r, r), u.heading, 0, bodyTop(u)};
}

double Act::bodyTop(const Unit& u) { return u.stats().hauler ? Mule::deck + 1.5 : 1.5; }

double Act::gap(const Block& a, const Block& b) {
    // Overlapping (no separating axis among the four sides' normals): 0.
    bool separate = false;
    for (const Block* k : {&a, &b}) {
        const double c = std::cos(k->angle), s = std::sin(k->angle);
        separate = separate || apart(a, b, Vec2(c, s)) || apart(a, b, Vec2(-s, c));
    }
    if (!separate) return 0;
    // Apart, two convex boxes are nearest at a corner of one.
    double best = 1e300;
    for (const Vec2 p : corners(a)) best = std::min(best, length(b.nearest(p) - p));
    for (const Vec2 p : corners(b)) best = std::min(best, length(a.nearest(p) - p));
    return best;
}

std::optional<Block> Act::doorLeaf(const Structure& citadel) {
    const Footprint& f = Rules::footprint(citadel.kind);
    if (!f.door) return std::nullopt;
    const Slab& d = *f.door;
    return onBuilding(citadel, (d.min + d.max) * 0.5, (d.max - d.min) * 0.5, d.z0, d.z1);
}

std::optional<Block> Act::doorPanel(const Structure& citadel, bool inside) {
    // Right of the doorway (+x), 0.75 out from the jamb, at chest height:
    // on the wall's outer face and its inner face (citadel.py's
    // `doorPanels_k`, the footprint's inner panel).
    const Footprint& f = Rules::footprint(citadel.kind);
    if (!f.door) return std::nullopt;
    const Slab& d = *f.door;
    const Vec2 at(d.max.x + 0.75, inside ? d.min.y - 0.07 : d.max.y + 0.07);
    return onBuilding(citadel, at, Vec2(0.25, 0.12), 0.85, 1.55);
}

Block Act::console(const Structure& citadel, int gun) {
    // The station's console sits on the corner's diagonal, its middle 0.98
    // out past the gunner's point (the footprint's furniture).
    const int i = std::clamp(gun, 0, 3);
    const double sx = (i == 0 || i == 3) ? 1 : -1, sy = i < 2 ? 1 : -1;
    const Vec2 station = Defence::gunStation(Vec2(0, 0), i);
    return onBuilding(citadel, station + Vec2(sx, sy) * 0.693, Vec2(0.47, 0.585), 0.05, 1.75, std::atan2(sy, sx));
}

Block Act::perch(const Unit& mule) { return onMule(mule, Mule::perchLocal, Vec2(0.35, 0.7), Mule::deck, Mule::rim + 0.15); }

Block Act::hopper(const Unit& mule) { return onMule(mule, Mule::hopperLocal, Vec2(0.75, 0.75), 0.6, Mule::rim + 0.15); }

Block Act::rampPostLocal() { return button(ShuttleBay::rampButton, 0.2); }
Block Act::takeOffLocal() { return button(ShuttleBay::takeOffButton, 0.2); }
Block Act::rampPost(const Shuttle& s) { return inWorld(s, rampPostLocal()); }
Block Act::takeOff(const Shuttle& s) { return inWorld(s, takeOffLocal()); }

Vec2 Act::padPostAt(double radius) { return Vec2(radius + 0.35, radius + 0.35); }
Vec2 Act::foundryPostAt(double radius) { return Vec2(0.7 * radius + 0.3, radius + 0.4); }

Block Act::padPost(const Structure& pad) {
    return onBuilding(pad, padPostAt(Rules::radius(pad.kind)), Vec2(postHalf, postHalf) + Vec2(0.09, 0.09), postButtonLow, postTop);
}

Block Act::foundryPost(const Structure& foundry) {
    return onBuilding(foundry, foundryPostAt(Rules::radius(foundry.kind)), Vec2(postHalf, postHalf) + Vec2(0.09, 0.09), postButtonLow,
                      postTop);
}

std::optional<Block> Act::postFoot(const Structure& s) {
    if (!s.complete() || (s.kind != StructureKind::landingPad && s.kind != StructureKind::foundry)) return std::nullopt;
    const Vec2 at = s.kind == StructureKind::landingPad ? padPostAt(Rules::radius(s.kind)) : foundryPostAt(Rules::radius(s.kind));
    return Block{s.position + at, Vec2(postHalf, postHalf), 0, 0, postTop};
}

} // namespace ac
