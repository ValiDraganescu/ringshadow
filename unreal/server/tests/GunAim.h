// A gunner's aim for the server tests (C2, docs/guns-and-practice.md
// section 1): the yaw and pitch that put a corner gun's round on a unit, on
// the core's ballistic solve from the gun's own muzzle, led by the unit's
// velocity over the flight.
#pragma once

#include "Defence.h"
#include "RegionFile.h"
#include "Rules.h"
#include "NavGrid.h"
#include "Simulation.h"

#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace ac::server::rig {

struct GunAim {
    double yaw = 0;
    double pitch = 0;
};

/// Corner gun `gun` of the Citadel at `citadel` aimed at `u` moving at
/// `velocity` (cells/s): at its body's middle. Nil: out of the round's
/// reach, or past the gun's arc or its pitch stops.
inline std::optional<GunAim> aimCornerGun(const Simulation& sim, Vec2 citadel, int gun, const Unit& u, Vec2 velocity = Vec2(0, 0)) {
    const UnitStats& st = Rules::stats(u.kind);
    const double h = st.height > 0 ? st.height : 1.5;
    const double z = sim.groundZ(u.position) + (u.airborne() ? (u.bug ? u.bug->height : st.hover) : h / 2);
    const Vec2 mount = Defence::cornerGun(citadel, gun);
    Vec2 at = u.position;
    GunAim a;
    for (int k = 0; k < 4; k++) {
        a.yaw = std::atan2(at.y - mount.y, at.x - mount.x);
        Vec3 m = Defence::gunMuzzle(citadel, gun, a.yaw, a.pitch);
        m.z += sim.groundZ(citadel);
        const double ahead = std::hypot(at.x - m.x, at.y - m.y);
        const auto pitch = Rules::ballisticPitch(ahead, z - m.z);
        if (!pitch) return std::nullopt;
        a.pitch = *pitch;
        const double flight = ahead / (Rules::roundSpeed * std::cos(a.pitch));
        at = u.position + velocity * flight;
    }
    if (std::abs(std::remainder(Defence::clampAim(gun, a.yaw) - a.yaw, 2 * pi)) > 1e-6) return std::nullopt;
    if (a.pitch < Defence::gunPitchMin || a.pitch > Defence::gunPitchMax) return std::nullopt;
    return a;
}

/// A gatling's place on a canyon's rim over path `from` of zone `z` (ground
/// cells): placeable now (snapped, `placementRefusal`, off the pads'
/// plates), in the zone, a level above the path, and of those the one whose
/// arc (`Simulation::gatlingPitch`) reaches the most points of the path,
/// half a cell apart, within 12 cells; the nearest the path on a tie. The
/// tests ask the rule, not a spot: the dev line's first place
/// (`canyon turrets`) is a sample of the rims that need not see the floor
/// (2026-10-19: the sculpted Cold Fork's cliffs are sheer where the old
/// ground had shelves). Nil: none reaches it.
/// A gatling gun at `at` stands back from its ground's lip: on the walking
/// grid a footprint blocks the ground under it whatever its height, so a
/// gun hanging over a rim's edge pinches the arm's floor below and walkers
/// jam there, under its barrels. Its square, grown by the walk's clearance
/// and half a cell, all at its own height.
inline bool offTheLip(const Simulation& sim, Vec2 at) {
    const double g = Rules::radius(StructureKind::gatling) + NavGrid::clearance + 0.5;
    for (double dx = -g; dx <= g; dx += 0.25)
        for (double dy = -g; dy <= g; dy += 0.25)
            if (!(sim.groundZ(at + Vec2(dx, dy)) > sim.groundZ(at) - 0.5)) return false;
    return true;
}

inline std::optional<Vec2> rimGunSpot(const Simulation& sim, const region::Zone& z, const std::vector<region::Place>& pads,
                                      const std::string& from) {
    std::vector<Vec2> path;
    for (const region::Zone::Path& p : z.paths)
        if (p.from == from)
            for (size_t k = 0; k + 1 < p.points.size(); k++) {
                const Vec2 a = p.points[k], e = p.points[k + 1] - a;
                const double len = std::hypot(e.x, e.y);
                for (double t = 0; t < len; t += 0.5) path.push_back(a + e * (t / len));
            }
    if (path.empty()) return std::nullopt;
    Vec2 lo = path[0], hi = path[0];
    for (const Vec2 q : path) {
        lo = Vec2(std::min(lo.x, q.x), std::min(lo.y, q.y));
        hi = Vec2(std::max(hi.x, q.x), std::max(hi.y, q.y));
    }
    const UnitStats& bug = Rules::stats(UnitKind::shardling);
    const double bugMiddle = (bug.height > 0 ? bug.height : 1.5) / 2;
    std::optional<Vec2> best;
    int bestHits = 0;
    double bestNear = 1e18;
    for (double y = std::floor(lo.y) - 12; y <= hi.y + 12; y += 1)
        for (double x = std::floor(lo.x) - 12; x <= hi.x + 12; x += 1) {
            const auto at = sim.snap(StructureKind::gatling, Vec2(x, y));
            // Within a cell's half diagonal: a 5x5 gun (C5) snaps to cell
            // centres, a 2x2 one to corners.
            if (!at || std::hypot(at->x - x, at->y - y) > 0.75 || signedDistance(*at, z.outline) > 0) continue;
            if (sim.placementRefusal(StructureKind::gatling, *at) ||
                region::touchesPadPlate(pads, *at, Rules::radius(StructureKind::gatling)))
                continue;
            if (!offTheLip(sim, *at)) continue;
            int hits = 0;
            double near = 1e18;
            for (const Vec2 q : path) {
                const double d = std::hypot(q.x - at->x, q.y - at->y);
                near = std::min(near, d);
                if (d > 12 || sim.groundZ(*at) < sim.groundZ(q) + 1) continue;
                Structure probe(0, StructureKind::gatling, Pilot::player, *at);
                probe.aim = std::atan2(q.y - at->y, q.x - at->x);
                if (sim.gatlingPitch(probe, q, sim.groundZ(q) + bugMiddle, bug.radius)) hits++;
            }
            if (hits > bestHits || (hits == bestHits && hits > 0 && near < bestNear)) {
                best = *at;
                bestHits = hits;
                bestNear = near;
            }
        }
    return best;
}

} // namespace ac::server::rig
