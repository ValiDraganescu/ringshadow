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

/// The brood queen (docs/creatures.md, "The Matriarch"; N3): her attacks,
/// her roar, her mending and her nest. Her hp, armour, speed and footprint
/// are her `UnitStats`; the core steps her (`Simulation::stepMatriarch`),
/// the server's director keeps her nest and decides her roars.
struct Matriarch {
    /// The slam: both forelimbs rise for `slamWindup` s and crash down on
    /// every ground foe within `slamReach` cells of her edge in front of her
    /// (within `slamArc` rad of her heading): `slamUnit` to a unit,
    /// `slamBuilding` to a building, before armour; every `slamEvery` s.
    static constexpr double slamUnit = 40;
    static constexpr double slamBuilding = 60;
    static constexpr double slamReach = 3;
    static constexpr double slamArc = 1.1;
    static constexpr double slamWindup = 0.8;
    static constexpr double slamEvery = 4;
    /// The acid spit, at ground and air: `spitUnit` to a unit,
    /// `spitBuilding` to a building, before armour, within `spitRange` of
    /// her edge, every `spitEvery` s. It leaves a puddle `puddleRadius`
    /// wide that does `puddleDps` a second to every ground foe in it for
    /// `puddleFor` s.
    static constexpr double spitUnit = 18;
    static constexpr double spitBuilding = 30;
    static constexpr double spitRange = 12;
    static constexpr double spitEvery = 3;
    static constexpr double puddleRadius = 1;
    static constexpr double puddleDps = 4;
    static constexpr double puddleFor = 4;
    /// The roar: she rears for `roarFor` s; in a fight every `roarEvery` s,
    /// and once when a pilot first comes within `roarSight`. Every bug of
    /// her nest within `roarReach` turns on the roared-at foe and runs ×
    /// `rallySpeed` for `rallyFor` s; `hatch` Shardlings come out of her egg
    /// pods while the nest holds fewer than `nestShardlingsMax`.
    static constexpr double roarFor = 2;
    static constexpr double roarEvery = 30;
    static constexpr double roarSight = 25;
    static constexpr double roarReach = 40;
    static constexpr double rallySpeed = 1.3;
    static constexpr double rallyFor = 10;
    static constexpr int64_t hatch = 4;
    /// She mends `mendRate` hp a second once `mendAfter` s passed without a hit.
    static constexpr double mendRate = 20;
    static constexpr double mendAfter = 20;
    /// She never goes farther than `tether` cells from her nest's centre
    /// (out on a siege: from the siege's place).
    static constexpr double tether = 6;
    /// Her nest: a ring `nestRing` cells round her, where `nestShardlings`
    /// to `nestShardlingsMax` Shardlings and `nestGloamwings` to
    /// `nestGloamwingsMax` Gloamwings linger always; it refills one bug
    /// every `refill` s while it is below its count.
    static constexpr double nestRing = 30;
    static constexpr int64_t nestShardlings = 16;
    static constexpr int64_t nestShardlingsMax = 24;
    static constexpr int64_t nestGloamwings = 4;
    static constexpr int64_t nestGloamwingsMax = 8;
    static constexpr double refill = 20;
    /// Killing her: `rewardOre` and `rewardHydrogen` shared by the damage
    /// each pilot did her (at least `rewardFloor` of each share), and a
    /// rare blueprint copy to each; she comes back `returnDays` world days
    /// later, and the region's bugs stay away for `calmDays`.
    static constexpr int64_t rewardOre = 3000;
    static constexpr int64_t rewardHydrogen = 1500;
    static constexpr double rewardFloor = 0.05;
    static constexpr double returnDays = 3;
    static constexpr double calmDays = 1;
    /// What her last strike was, in `Bug::sac` (the Lodeback's field; she
    /// has no sac): the client plays the slam or the spit.
    static constexpr double struckSlam = 1;
    static constexpr double struckSpit = 2;
};

} // namespace ac
