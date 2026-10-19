// Solid bodies (C5, docs/haul-and-ferry.md "Solid bodies";
// Simulation+Pilot.members.h): nothing on the ground walks or drives
// through another, walkers, vehicles and the Brood alike. A Mule's body is
// its box (`Mule::length` by `Mule::width`, turned with its heading);
// everyone else's its walk's circle (`Rules::radius` x 0.8, as
// `pilotCanStand` takes the walker's).

#include "Mule.h"
#include "NavGrid.h"
#include "Simulation.h"

#include <cmath>

namespace ac {

namespace {

/// A body seen from above: a box of half sides `half` turned by `angle`
/// round `centre`, or (`half` zero) a circle of radius `r`.
struct SolidBody {
    Vec2 centre, half;
    double angle = 0, r = 0;
    bool box() const { return half.x > 0; }
};

SolidBody solidOf(const Unit& u, Vec2 p, double heading) {
    if (u.stats().hauler) return SolidBody{p, Vec2(Mule::length / 2, Mule::width / 2), heading, 0};
    return SolidBody{p, Vec2(0, 0), 0, Rules::radius(u.kind) * 0.8};
}

/// Past this, centre to centre along either axis, no two bodies touch
/// (twice the farthest any body reaches from its centre: the Matriarch's).
const double solidFar = [] {
    double most = std::hypot(Mule::length / 2, Mule::width / 2);
    for (const UnitStats& s : UnitStats::byKind) most = ac::max(most, s.radius * 0.8);
    return 2 * most + 0.01;
}();

/// How deep `a` reaches into `b` (at or under 0: apart, by that much),
/// and the way out for `a` (a unit vector, from `b` toward `a`).
struct SolidOverlap {
    double depth;
    Vec2 out;
};

Vec2 turnedBy(Vec2 v, double angle) {
    const double c = std::cos(angle), s = std::sin(angle);
    return Vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

/// Point `p` against box `b`: its signed distance from the box's edge
/// (under 0 inside) and the box's outward normal there.
SolidOverlap pointInBox(Vec2 p, const SolidBody& b) {
    const Vec2 o = turnedBy(p - b.centre, -b.angle);
    const double qx = std::abs(o.x) - b.half.x, qy = std::abs(o.y) - b.half.y;
    if (qx > 0 || qy > 0) {
        const Vec2 off(qx > 0 ? std::copysign(qx, o.x) : 0.0, qy > 0 ? std::copysign(qy, o.y) : 0.0);
        const double len = length(off);
        return {len, turnedBy(off / len, b.angle)};
    }
    // Inside: out by the nearer face.
    if (qx > qy) return {qx, turnedBy(Vec2(o.x < 0 ? -1 : 1, 0), b.angle)};
    return {qy, turnedBy(Vec2(0, o.y < 0 ? -1 : 1), b.angle)};
}

SolidOverlap solidOverlap(const SolidBody& a, const SolidBody& b) {
    if (!a.box() && !b.box()) {
        const Vec2 d = a.centre - b.centre;
        const double len = length(d);
        return {a.r + b.r - len, len > 1e-9 ? d / len : Vec2(1, 0)};
    }
    if (!a.box()) {
        const SolidOverlap e = pointInBox(a.centre, b);
        return {a.r - e.depth, e.out};
    }
    if (!b.box()) {
        const SolidOverlap e = pointInBox(b.centre, a);
        return {b.r - e.depth, -e.out};
    }
    // Two boxes: the axis of either along which they overlap least.
    const Vec2 axes[] = {turnedBy(Vec2(1, 0), a.angle), turnedBy(Vec2(0, 1), a.angle), turnedBy(Vec2(1, 0), b.angle),
                         turnedBy(Vec2(0, 1), b.angle)};
    const Vec2 d = a.centre - b.centre;
    auto extent = [&](const SolidBody& x, Vec2 n) {
        return x.half.x * std::abs(dot(turnedBy(Vec2(1, 0), x.angle), n)) + x.half.y * std::abs(dot(turnedBy(Vec2(0, 1), x.angle), n));
    };
    SolidOverlap best{INFINITY, axes[0]};
    for (const Vec2 n : axes) {
        const double along = dot(d, n);
        const double o = extent(a, n) + extent(b, n) - std::abs(along);
        if (o < best.depth) best = {o, along < 0 ? -n : n};
    }
    return best;
}

} // namespace

bool Simulation::bodyBlocks(const Unit& u, const Unit& v) const {
    if (v.id == u.id || !(v.hp > 0) || u.stats().air) return false;
    if (v.stats().air || v.airborne() || v.jumpFrom || v.burrowed()) return false;
    return v.task != Unit::Task::aboard && v.task != Unit::Task::inDerrick && v.task != Unit::Task::inBastion;
}

namespace {

/// The body solid to `u` that holds it out of `p` turned to `heading`: one
/// it reaches into there deeper than where it stands now.
const Unit* solidHolder(const Simulation& sim, const Unit& u, Vec2 p, double heading) {
    const SolidBody at = solidOf(u, p, heading), now = solidOf(u, u.position, u.heading);
    for (const Unit& v : sim.state.units) {
        if (std::abs(v.position.x - p.x) > solidFar || std::abs(v.position.y - p.y) > solidFar) continue;
        if (!sim.bodyBlocks(u, v)) continue;
        const SolidBody b = solidOf(v, v.position, v.heading);
        const double deep = solidOverlap(at, b).depth;
        if (deep > 1e-9 && deep > solidOverlap(now, b).depth + 1e-9) return &v;
    }
    return nullptr;
}

} // namespace

bool Simulation::bodyHeld(const Unit& u, Vec2 p, double heading) const {
    if (u.stats().air) return false;
    return solidHolder(*this, u, p, heading) != nullptr;
}

std::optional<Vec2> Simulation::bodyContact(const Unit& u, Vec2 from, Vec2 to) const {
    if (u.stats().air) return std::nullopt;
    const Unit* v = solidHolder(*this, u, to, u.heading);
    if (!v) return std::nullopt;
    return solidOverlap(solidOf(u, from, u.heading), solidOf(*v, v->position, v->heading)).out;
}

std::optional<Vec2> Simulation::bodyPush(const Unit& u, double most) const {
    if (u.stats().air || !(most > 0)) return std::nullopt;
    const SolidBody now = solidOf(u, u.position, u.heading);
    std::optional<SolidOverlap> deepest;
    for (const Unit& v : state.units) {
        if (std::abs(v.position.x - u.position.x) > solidFar || std::abs(v.position.y - u.position.y) > solidFar) continue;
        if (!bodyBlocks(u, v)) continue;
        const SolidOverlap o = solidOverlap(now, solidOf(v, v.position, v.heading));
        if (o.depth > 1e-6 && (!deepest || o.depth > deepest->depth)) deepest = o;
    }
    if (!deepest) return std::nullopt;
    // Straight out of it, else turned up to a right angle either way, onto
    // ground it can stand on (a wall or another body behind it).
    const Vec2 s = deepest->out * ac::min(most, deepest->depth + 0.01);
    for (const double a : {0.0, 30.0, -30.0, 60.0, -60.0, 90.0, -90.0}) {
        const Vec2 step = turnedBy(s, a * pi / 180);
        if (pilotCanStand(u, u.position + step)) return step;
    }
    return std::nullopt;
}

void Simulation::holdOff() {
    // Never moved by the AI's push: a driven unit (its own walk puts it out,
    // `bodyPush`) and a Mule, parked or driven.
    auto fixed = [&](const Unit& u) { return piloted(u.id) || u.stats().hauler; };
    auto walkable = [&](Vec2 p) { return (nav ? nav->walkable(p) && !nav->inRock(p) : true) && !NavGrid::solid(p, state); };
    // Out by `out` (a unit vector) and `by`, else turned up to a right angle
    // either way, onto walkable ground.
    auto put = [&](Unit& v, Vec2 out, double by) {
        for (const double a : {0.0, 45.0, -45.0, 90.0, -90.0}) {
            const Vec2 q = v.position + turnedBy(out, a * pi / 180) * by;
            if (walkable(q)) {
                v.position = q;
                return;
            }
        }
    };
    for (size_t k = 0; k < state.units.size(); k++) {
        if (!(fixed(state.units[k]) && state.units[k].hp > 0)) continue;
        const Unit f = state.units[k];
        const SolidBody body = solidOf(f, f.position, f.heading);
        for (Unit& v : state.units) {
            if (std::abs(v.position.x - f.position.x) > solidFar || std::abs(v.position.y - f.position.y) > solidFar) continue;
            if (fixed(v) || !bodyBlocks(v, f) || !bodyBlocks(f, v)) continue;
            const SolidOverlap o = solidOverlap(solidOf(v, v.position, v.heading), body);
            if (o.depth > 1e-6) put(v, o.out, o.depth + 0.01);
        }
    }
}

} // namespace ac
