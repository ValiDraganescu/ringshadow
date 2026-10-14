// The Brood's numbers (docs/creatures.md): the bugs' side of the MMO, past
// their `UnitStats` (Rules.h). The core steps each bug by its orders
// (`Simulation::stepBug`); the server's brood director gives the orders.
#pragma once

#include "AutocraftCoreApi.h"
#include "Rules.h"

#include <cstdint>

namespace ac {

struct AUTOCRAFTCORE_API Brood {
    /// Cells a second wandering in its site (a Gloamwing hovering): a share
    /// of its top speed is `Bug::pace`.
    static double wanderSpeed(UnitKind k);
    /// Cells a second on its way to its site or out of the region.
    static double marchSpeed(UnitKind k);
    /// Damage a bite, spit or slam does to a building, before its armour
    /// (0: it never attacks buildings).
    static double buildingDamage(UnitKind k);
    /// XP a kill is worth (docs/creatures.md, "XP and rewards"), standing
    /// in for a unit's cost: the damage dealt's share of its hp × this.
    static double xp(UnitKind k);

    /// A Gloamwing's hover, cells above the ground, and its dive's: at torch
    /// height. The dive: `diveWindup` s down, the spit, then `diveAfter` s
    /// low before it climbs (`Unit::airborne` is false from the windup to
    /// then). It eases between heights at `climbRate` cells a second.
    static constexpr double hoverHeight = 2.5;
    static constexpr double diveHeight = 0.6;
    static constexpr double diveWindup = 0.5;
    static constexpr double diveAfter = 0.6;
    static constexpr double climbRate = 5.0;

    /// A Lodeback eats this much ore a second, up to `sacFull`, for at most
    /// `eatFor` s; it flees at `fleeSpeed`; it is cornered when it has fled
    /// `corneredAfter` s and was hit within the last `stillHit` s.
    static constexpr double eatRate = 1.5;
    static constexpr double sacFull = 60;
    static constexpr double eatFor = 40;
    static constexpr double fleeSpeed = 3.0;
    static constexpr double corneredAfter = 10;
    static constexpr double stillHit = 2;
    /// It eats standing this far from the deposit's centre (past its radius).
    static constexpr double eatReach = 1.6;

    /// A bug back in its site, unhit for this long, mends to full.
    static constexpr double mendAfter = 8;
    /// A scared bug (a starfall's impact) runs at full speed this long.
    static constexpr double scareFor = 10;
    /// A dead bug's body lies this long (its death pose), then goes.
    static constexpr double bodyStays = 6;
    /// A wandering bug picks a new spot in its site every 3 to 8 s.
    static constexpr double wanderMin = 3;
    static constexpr double wanderMax = 8;
};

} // namespace ac
