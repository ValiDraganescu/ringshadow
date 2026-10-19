// The tests' rigs put the realm's Citadel up near a spot of their choice;
// since C1 it is 12x12 (Rules::radius 6, docs/citadel-interior.md) and the
// old spots lie on the pad, a crate or the ore. `citadelSpot` finds the
// nearest one where it places now, so every rig asks the core's own rule.

#pragma once

#include "RegionFile.h"
#include "Simulation.h"
#include "WorldMap.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ac::server::rig {

/// The snapped spot nearest `near` (simulation cells) where a `kind`
/// places now (`Simulation::placementRefusal`, off the plates of `pads` as
/// `WorldMap::build` keeps it), out ring by ring a cell apart up to
/// `reach`. Nil: none.
inline std::optional<Vec2> placeableNear(const Simulation& sim, const std::vector<region::Place>& pads, Vec2 near,
                                         StructureKind kind = StructureKind::citadel, double reach = 40) {
    for (double rad = 0; rad <= reach; rad += 1) {
        const int around = rad == 0 ? 1 : std::max(16, int(rad * 6));
        for (int k = 0; k < around; k++) {
            const double t = k * 6.283185307179586 / around;
            const auto spot = sim.snap(kind, near + Vec2(std::cos(t), std::sin(t)) * rad);
            if (spot && !sim.placementRefusal(kind, *spot) && !region::touchesPadPlate(pads, *spot, Rules::radius(kind)))
                return spot;
        }
    }
    return std::nullopt;
}

/// The Citadel's nearest spot to `near` in pilot `p`'s region.
inline std::optional<Vec2> citadelSpot(WorldMap& w, PilotId p, Vec2 near, double reach = 40) {
    const Region* r = w.regionOf(p);
    if (!r || !w.loaded() || !w.where(p)) return std::nullopt;
    return placeableNear(r->simulation(), w.loaded()->regions[*w.where(p)].built.pads, near, StructureKind::citadel, reach);
}

/// Where the rigs' `weld` stands its Prospector to put a `kind` up at
/// `spot`: east of it by the build reach (weld builds west of the stand).
inline Vec2 weldStand(StructureKind kind, Vec2 spot) { return spot + Vec2(Region::buildReach(kind), 0); }

/// The stand for a Citadel at its nearest spot to `near` (`near` itself
/// when there is none: the weld is refused and says why).
inline Vec2 citadelStand(WorldMap& w, PilotId p, Vec2 near) {
    const auto spot = citadelSpot(w, p, near);
    return spot ? weldStand(StructureKind::citadel, *spot) : near;
}

/// The same for any `kind` (C5: every building of the MMO 2.4 times the
/// strategy game's, docs/building-sizes.md; the rigs' old fixed spots
/// overlap): the stand for its nearest spot to where the rig's stand
/// `stand` would have put it.
inline Vec2 standFor(WorldMap& w, PilotId p, StructureKind kind, Vec2 stand, double reach = 40) {
    const Region* r = w.regionOf(p);
    if (!r || !w.loaded() || !w.where(p)) return stand;
    const auto spot = placeableNear(r->simulation(), w.loaded()->regions[*w.where(p)].built.pads,
                                    stand - Vec2(Region::buildReach(kind), 0), kind, reach);
    return spot ? weldStand(kind, *spot) : stand;
}

} // namespace ac::server::rig
