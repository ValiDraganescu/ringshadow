// The Mule (C4, docs/haul-and-ferry.md section 2): the MMO's hauler, a six
// wheeled cart the pilot drives standing at two levers on its front perch,
// with an open ore hopper behind it and MH tanks along its flanks. A
// Prospector on foot puts its load in at the hopper (`Simulation::muleFill`);
// driven into a Citadel's hall, the Mule books its whole load to its driver's
// wallet (`Simulation::stepMules`, `GameEvent::Booked`). Bought at a realm
// Foundry by a pilot (the server, `WorldMap::buyMule`), one each.
//
// Its numbers beside its row in `UnitStats::byKind` (`UnitKind::mule`). Its
// own frame: x ahead along its heading, y to its left (the world's turn).
#pragma once

#include "AutocraftCoreApi.h"
#include "Rules.h"
#include "SimdMath.h"
#include "Types.h"

#include <cstdint>
#include <string>

namespace ac {

struct AUTOCRAFTCORE_API Mule {
    /// What it holds, ore and MH together, one a unit. The load that fills
    /// it may go over; at or over this no new load goes in.
    static constexpr int64_t capacity = 1000;
    /// Its body: long along its heading, wide, its top (the hopper's rim and
    /// the tanks) over the ground; the perch's deck its driver stands on. It
    /// fits the Citadel's doorway (3 wide, 3 tall) and the Dropship's bay
    /// (2 wide, 4 long, 2 tall) with its driver aboard (the driver's top
    /// under 1.9: the deck and a Prospector's 1.5).
    static constexpr double length = 3.0;
    static constexpr double width = 1.6;
    static constexpr double height = 1.4;
    static constexpr double deck = 0.35;
    static constexpr double rim = 1.3;
    /// The perch (the front rail, the two levers) and the hopper's middle,
    /// in its own frame.
    static constexpr Vec2 perchLocal{1.0, 0};
    static constexpr Vec2 hopperLocal{-0.55, 0};
    /// Its price from the buyer's wallet, and how long it takes to roll out
    /// of the Foundry's door (its row's `ore`, `hydrogen`, `trainTime`).
    static int64_t ore() { return Rules::cost(UnitKind::mule); }
    static int64_t hydrogen() { return Rules::hydrogenCost(UnitKind::mule); }
    static double rollOut() { return Rules::trainTime(UnitKind::mule); }

    /// A point of Mule `m` given in its own frame, in the world.
    static Vec2 point(const Unit& m, Vec2 local);
    static Vec2 perch(const Unit& m) { return point(m, perchLocal); }
    static Vec2 hopper(const Unit& m) { return point(m, hopperLocal); }
    /// Room left before it is full (0 when full or over).
    static int64_t room(const Unit& m);
    /// Where a Mule rolls out of Foundry `foundry` at `at` (its centre),
    /// facing out of the door (+y, as a trained vehicle leaves).
    static Vec2 door(Vec2 foundry);
    static constexpr double doorHeading = 1.5707963267948966;

    /// The refusals the game shows as they are.
    static constexpr const char* full = "The Mule is full";
    /// The toast of a booking: "Booked · +N ore, +M MH" (either left out
    /// when none).
    static std::string bookedToast(int64_t ore, int64_t hydrogen);
};

} // namespace ac
