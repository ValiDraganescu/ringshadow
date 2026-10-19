// The ringshadow sweep (docs/weather.md, "Ringshadow sweep"; docs/mmorpg-m7.md
// E1): twice a world day the gas giant's rings throw a band of shadow that
// crosses every region of the world: the shadow of the ring's arc as it
// slides across the sun. A pure function of the world clock (`WorldClock`)
// and a ground point: the server pays the drilling bonus from it and the
// client draws the band from it, with nothing new on the wire.
#pragma once

#include "AutocraftCoreApi.h"
#include "Projection.h"
#include "SimdMath.h"
#include "WorldClock.h"

#include <cstdint>
#include <optional>

namespace ac {

struct AUTOCRAFTCORE_API Ringshadow {
    /// The world hours a sweep starts at: late morning and afternoon, both
    /// in daylight (the band is the sun's shadow).
    static constexpr double hours[2] = {11.5, 16.0};
    /// Seconds from the band's leading edge `margin` short of the region to
    /// its trailing edge `margin` past it.
    static constexpr double duration = 120;
    /// The band's width across, cells (its soft edges inside it): a point
    /// is in the shadow about 40 s on a medium region (144 × 96).
    static constexpr double width = 110;
    /// Each edge fades over this many cells.
    static constexpr double soft = 16;
    /// The band starts and ends this far outside the region, cells.
    static constexpr double margin = 25;
    /// Where the giant hangs, its middle: compass degrees (0 north = -z, 90
    /// east = +x) and degrees above the horizon. The sky draws it there
    /// (`FAcGasGiant::Direction`).
    static constexpr double giantBearing = 280, giantElevation = 12;
    /// An ore load drilled under the band (cover at least `bonusCover`)
    /// carries this many times the ore, and so the XP when it is unloaded.
    static constexpr double oreBonus = 1.5;
    static constexpr double bonusCover = 0.5;
    /// The radio warns this many seconds before a sweep starts.
    static constexpr double warning = 60;

    struct Sweep {
        double start = 0, end = 0;
        /// Which of the day's sweeps: its hour is `hours[index]`.
        int index = 0;
        bool operator==(const Sweep&) const = default;
    };
    /// The band at a moment: the way it moves (unit, cells), its leading and
    /// trailing edges along that way (cells, `dot(p, dir)`), its soft edge.
    struct Band {
        Vec2 dir;
        double lead = 0, trail = 0;
        double soft = Ringshadow::soft;
    };

    /// Toward the giant's middle, unit, sim axes (x east, y up, z south).
    static Vec3 giant();
    /// Toward the sun as sweep `index` starts (`Rules::sunToward` at its hour).
    static Vec3 sun(int index);
    /// The unit vector sweep `index`'s band moves along, ground cells (x, z):
    /// the shadow of the ring's arc. The arc runs along the great circle
    /// through the giant `g` and the sun `s`; its edge, crossing the sun,
    /// runs along that circle's tangent `t` there, so the edge's shadow on
    /// the ground lies in the plane of `s` and `t`, along
    /// e = normalize((s × t) × up), and the band moves across e. The sky
    /// draws the arc sliding over the sun along the circle's normal: the
    /// band moves the same way on the ground (Ringshadow.cpp has the sign).
    /// 11:30: toward 186° (the sun at 301°, 53° up); 16:00: toward 169°
    /// (the sun at 20°, 27° up).
    static Vec2 direction(int index);
    /// The same for any sun `s` and giant `g` (unit, sim axes, apart).
    static Vec2 direction(Vec3 s, Vec3 g);
    /// Its compass bearing, degrees (0 north = -z, 90 east = +x).
    static double bearing(int index);
    /// The sweep running at `time` (nil: none).
    static std::optional<Sweep> at(double time, double day = WorldClock::dayLength);
    /// The first sweep that has not ended by `time` (the running one, or the next).
    static Sweep next(double time, double day = WorldClock::dayLength);
    /// The band over a region with `bounds` at `time` (nil: no sweep).
    static std::optional<Band> band(double time, const GroundRect& bounds, double day = WorldClock::dayLength);
    /// How deep in the shadow ground point `p` is: 0 in the sun, 1 inside.
    static double cover(const Band& band, Vec2 p);
    static double cover(double time, Vec2 p, const GroundRect& bounds, double day = WorldClock::dayLength);
    /// An ore load drilled at `p` at `time`: what it carries with the bonus.
    static int64_t load(int64_t carried, double time, Vec2 p, const GroundRect& bounds,
                        double day = WorldClock::dayLength);
};

} // namespace ac
