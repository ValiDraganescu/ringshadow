// Starfall (docs/weather.md, "Starfall"; docs/mmorpg-m7.md E2): two or three
// times a world day a meteor streaks across the sky under the gas giant and
// lands somewhere in the world, leaving a crater with a rich deposit, first
// come first served. The server picks the times (seeded dice per world, a
// pure function of the world's seed and day: `impacts`) and the places (its
// own rules, WorldStarfall.cpp) and tells every pilot (`net::Sky::starfalls`);
// the client draws the streak (`head`), the impact and the crater.
#pragma once

#include "AutocraftCoreApi.h"
#include "SimdMath.h"
#include "WorldClock.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ac {

struct AUTOCRAFTCORE_API Starfall {
    /// Falls a world day: 2 or 3, by the day's dice.
    static constexpr int64_t fewest = 2;
    static constexpr int64_t most = 3;
    /// The radio tells every pilot this many seconds before the impact: the
    /// place is picked then.
    static constexpr double warning = 60;
    /// Seconds the streak shows before the impact, the speed it comes in at
    /// (cells a second) and how steeply it falls (degrees under level).
    static constexpr double streak = 10;
    static constexpr double speed = 70;
    static constexpr double descent = 16;
    /// It comes out of the giant's side of the sky (the giant hangs at
    /// bearing 280°), this many degrees either side.
    static constexpr double giantBearing = 280;
    static constexpr double spread = 40;
    /// The crater: its radius (cells), and how long it marks the ground
    /// after the impact (a world day). The deposit stays until emptied.
    static constexpr double craterRadius = 4;
    static constexpr double craterLife = WorldClock::dayLength;
    /// The deposit: this many times a normal deposit's ore (the median of
    /// the region's own), at least `minOre`; never grows back.
    static constexpr double richness = 3;
    static constexpr int64_t minOre = 3000;
    /// Ore drilled from it pays this many times the XP: the share over 1 at
    /// the drill, the plain XP when the load is unloaded.
    static constexpr double xp = 1.5;

    /// The impact times of world day `day` (`time / dayLength`, floored) of
    /// the world with `seed`, in order: one in each equal share of the day,
    /// somewhere in its middle 70%.
    static std::vector<double> impacts(uint64_t seed, int64_t day, double dayLength = WorldClock::dayLength);
    /// The first impact after `time`.
    static double next(uint64_t seed, double time, double dayLength = WorldClock::dayLength);
    /// The bearing a fall comes from (compass degrees, 0 north = -z, 90 east
    /// = +x), by its dice.
    static double bearingFrom(uint64_t dice);
    /// Where the streak's head is `before` seconds before its impact,
    /// coming from `bearing`: cells from the impact point, x and z on the
    /// ground and y up.
    static Vec3 head(double before, double bearing);
    /// The unit vector toward `bearing` on the ground (x, z).
    static Vec2 toward(double bearing);
    /// The compass bearing (0-360) from `from` to `to`, ground cells.
    static double bearing(Vec2 from, Vec2 to);
    /// A 64-bit mix (splitmix64), the dice everything here rolls.
    static uint64_t mix(uint64_t x);
};

} // namespace ac
