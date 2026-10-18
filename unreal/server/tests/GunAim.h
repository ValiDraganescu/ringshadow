// A gunner's aim for the server tests (C2, docs/guns-and-practice.md
// section 1): the yaw and pitch that put a corner gun's round on a unit, on
// the core's ballistic solve from the gun's own muzzle, led by the unit's
// velocity over the flight.
#pragma once

#include "Defence.h"
#include "Rules.h"
#include "Simulation.h"

#include <cmath>
#include <optional>

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

} // namespace ac::server::rig
