// Starfall (docs/weather.md, "Starfall"; docs/mmorpg-m7.md E2, m10.md E1):
// about once a world day a meteor streaks across the sky under the gas giant
// and lands somewhere in the world. Its blast destroys what it falls on and
// hurts what stands round it (`blastRadius`); it leaves a crater with a
// deposit of 100 to 1,000 ore, first come first served. The server picks
// the times (seeded dice per world, a pure function of the world's seed and
// day: `impacts`) and the places (its own rules, WorldStarfall.cpp), does the
// blast's damage and tells every pilot (`net::Sky::starfalls`); the client
// draws the streak (`head`), the flash, the shockwave and the crater.
#pragma once

#include "AutocraftCoreApi.h"
#include "SimdMath.h"
#include "WorldClock.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ac {

struct AUTOCRAFTCORE_API Starfall {
    /// Falls a world day, by the day's dice (E1: one, realm-wide).
    static constexpr int64_t fewest = 1;
    static constexpr int64_t most = 1;
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
    /// The deposit (E1): `oreFewest` to `oreMost` ore, by the fall's dice;
    /// never grows back.
    static constexpr int64_t oreFewest = 100;
    static constexpr int64_t oreMost = 1000;
    /// The blast (E1): about a quarter of the region's diameter across (its
    /// longer side's); everything within `blastKill` of that radius of the
    /// impact is destroyed, buildings and units alike, and the damage falls
    /// off from there to nothing at its edge. A thing is as near as its
    /// nearest edge.
    static constexpr double blastAcross = 0.25;
    static constexpr double blastKill = 0.4;
    /// The home's ground (E1): no blast reaches the start region's Citadel
    /// or this many cells round it.
    static constexpr double homeSpared = 25;
    /// The blast's radius in a region `extent` (width, depth) cells.
    static double blastRadius(Vec2 extent);
    /// The share of its whole hp the blast takes from a thing `distance`
    /// cells from the impact (past its edge) in a blast of `radius`: 1
    /// (destroyed) within `blastKill` of it, then down to 0 at the edge.
    static double blastShare(double distance, double radius);
    /// A fall's ore by its dice.
    static int64_t oreFrom(uint64_t dice);
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
