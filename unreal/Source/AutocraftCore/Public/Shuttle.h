// The walk-in Dropship (C4, docs/haul-and-ferry.md section 3): its bay's
// shape in its own frame, and its pose through the flight. The state is
// `Shuttle` (Types.h), in `GameState::shuttles`; the simulation's half (the
// stages, the ramp, the walk in the bay, the refusals) is
// Simulation+Shuttle.members.h.
//
// The ship's frame: cells, +x the nose, +y its left, z up from the ground
// under the landed ship; the origin is `Shuttle::position` on the pad,
// turned by `Shuttle::heading`. A unit aboard keeps an ordinary position in
// the region, inside the frame (in flight too: the frame stays on the pad
// it lifted from until the switch to the target pad), and is drawn at the
// pose: `shuttlePose(ship)` carries the frame's point (`ShuttleBay::local`,
// at `ShuttleBay::floor`) up into the sky.
#pragma once

#include "AutocraftCoreApi.h"
#include "Flight.h"
#include "Rules.h"
#include "SimdMath.h"
#include "Types.h"

#include <algorithm>
#include <optional>
#include <string>

namespace ac {

/// The bay (concept B2) and the hull round it, in the ship's frame. The
/// numbers are c5-ship's model's (Tools/Editor/models/ferry.py: the bay 12
/// long and 4 wide, docs/haul-and-ferry.md section 3); *assumed* times are
/// the manager's, for the user to tune.
struct AUTOCRAFTCORE_API ShuttleBay {
    /// The deck's top (the bay's floor) and the ceiling's underside: 2.6
    /// tall clear inside.
    static constexpr double deck = 0.6;
    static constexpr double ceiling = 3.2;
    /// Inside, walkable: the bay x -6.8 … 5.2 (12 long), the cockpit nook
    /// up to the console's face at 6.6; 4 wide (y −2 … 2, the ribs' faces).
    static constexpr double rear = -6.8;
    static constexpr double front = 6.6;
    static constexpr double halfWide = 2.0;
    /// The hull seen from above (the tail's jambs at the back, the nose
    /// tip, the chines along the flanks at ±2.62): its bounding box. The
    /// walls from the clear bay out are solid, and the nose block from
    /// `noseFrom` (the console's back), narrowing to the tip (`noseSolids`).
    /// The nacelles on their pylons stand out to ±`nacelleHalfWide` from
    /// x `nacelleRear` to `nacelleFront`, high over a walker's head (from
    /// `nacelleUnder` up): only the cameras meet them.
    static constexpr double hullRear = -7.05;
    static constexpr double hullFront = 9.3;
    static constexpr double hullHalfWide = 2.62;
    static constexpr double noseFrom = 6.92;
    static constexpr double roofTop = 4.06;
    static constexpr double nacelleRear = -7.0;
    static constexpr double nacelleFront = -2.6;
    static constexpr double nacelleHalfWide = 4.56;
    static constexpr double nacelleUnder = 2.4;
    /// The ramp hinges at the opening's foot (x `rear`, z `deck`); 2.6 long
    /// and ±1.6 wide (a Mule with room on either side), down it lies on the
    /// ground at `rampFoot` (a 13.3° slope, gentle for wheels). The opening
    /// is the ramp's width; the jambs either side of it stand from
    /// `rampHalfWide` out to the walls.
    static constexpr double rampLength = 2.6;
    static constexpr double rampHalfWide = 1.6;
    static double rampFoot();
    /// Seconds from shut to down (*assumed*), and from how far down it is
    /// walkable (as a Citadel's door, `Simulation::doorOpen`).
    static constexpr double rampSeconds = 3.0;
    static constexpr double rampDown = 0.95;
    /// The orange ramp post inside by the opening (on the ship's right,
    /// beside the jamb), and the green take-off button on the console under
    /// the windshield (its top, z up).
    static constexpr Vec3 rampButton{-6.56, -1.84, 1.23};
    static constexpr Vec3 takeOffButton{6.75, 0, 1.30};
    /// Where a pilot stands in the bay to press each (the frontier pilots'
    /// walk, the dev lines, the tests): clear of the opening and of a Mule's
    /// way up the ramp.
    static constexpr Vec2 rampStand{-6.0, -1.4};
    static constexpr Vec2 helmStand{5.84, 0};
    /// It leaves once nobody has been in the bay or on the ramp this long
    /// (*assumed* 5 s), or `waitSeconds` after a call when nobody came
    /// aboard (*assumed*); it lifts away over `departSeconds`.
    static constexpr double leaveSeconds = 5.0;
    static constexpr double waitSeconds = 60.0;
    static constexpr double departSeconds = 6.0;
    /// The Landing Pad (C5, docs/building-sizes.md): the ship lands aligned
    /// with it, its hull centred, so the pad is the smallest odd square
    /// holding the hull's length and width (the nacelles' too) with
    /// `padMargin` round it. Half
    /// its side: a realm Landing Pad's `Rules::radius` (the MMO's sizes) and
    /// a region file pad's plate (`region::padReach`).
    static constexpr double padMargin = 0.25;
    static constexpr double padHalfSide() {
        const double need = std::max({(hullFront - hullRear) / 2, hullHalfWide, nacelleHalfWide}) + padMargin - 0.5;
        const double whole = double(int64_t(need));
        return (whole < need ? whole + 1 : whole) + 0.5;
    }
    /// On a pad it stands aligned with it (the manager, 2026-10-19): its
    /// ramp toward the pad's front (+y), its nose to the back, the hull
    /// centred on the pad (the ramp's foot lies off the pad's front edge).
    /// `hullMiddle` is the hull's middle along its axis, ahead of the frame's
    /// origin; `onPad` the frame's origin for a ship landed on the pad
    /// centred at `padCentre` with its nose along `heading`.
    static constexpr double padHeading = -pi / 2;
    static constexpr double hullMiddle() { return (hullFront + hullRear) / 2; }
    static Vec2 onPad(Vec2 padCentre, double heading = padHeading);

    /// Region point `p` in the frame of `s`, and back.
    static Vec2 local(const Shuttle& s, Vec2 p);
    static Vec2 world(const Shuttle& s, Vec2 local);
    /// `local` is in the bay (the cockpit nook too), the bay shrunk by
    /// `grow` on every side.
    static bool inBay(Vec2 local, double grow = 0);
    /// `local` is over the ramp's strip behind the opening (down or not,
    /// `rampHalfWide` across), grown by `grow`.
    static bool onRamp(Vec2 local, double grow = 0);
    /// `local` is within the hull seen from above grown by `grow`, or on the
    /// ramp's strip.
    static bool covers(Vec2 local, double grow = 0);
    /// The height a unit at `local` stands at, over the frame's ground: the
    /// deck in the bay, the ramp's slope on it (`rampOpen`, `Door::open`),
    /// else the ground (0).
    static double floor(Vec2 local, double rampOpen);
    /// The solid piece a walker of radius `grow` at `local` stands in (the
    /// hull's sides, the jambs and the nose, the console, the ramp post, the
    /// ramp when it is not down), nil when none: its box, for the slide's
    /// contact.
    static std::optional<Block> solidAt(Vec2 local, double grow, bool rampDown);
    /// The ship's solid parts for the cameras (as `Rules::footprint`): the
    /// hull's sides and nose, the deck slab and the ceiling as walls with
    /// their heights, the nacelles high on their pylons, `door` the shut
    /// ramp, `interior` the bay, `roof` over it, `furniture` the console and
    /// the ramp post. Its `radius` reaches the down ramp's foot.
    static const Footprint& footprint();
};

/// Where the ship is drawn: its frame's origin (cells, z up: the ground's
/// height when it stands on a pad) and its turn: `yaw` where the nose points
/// (radians, as `Shuttle::heading`), `pitch` the nose up (+), `roll` the
/// bank, + with the left side down (into a turn toward +y).
struct ShuttlePose {
    Vec3 at;
    double yaw = 0, pitch = 0, roll = 0;
    bool operator==(const ShuttlePose&) const = default;
};

/// The pose of `s` now, and at `clock` seconds into `stage` (the client's
/// own clock between two snapshots). Coming down onto the pad from high
/// behind it; on the pad; up off it, through two banking turns from its
/// heading onto the way to the target pad, the dash across the sky, the
/// approach over the target pad (the way F2's cinematic flight drew it);
/// lifting away.
AUTOCRAFTCORE_API ShuttlePose shuttlePose(const Shuttle& s);
AUTOCRAFTCORE_API ShuttlePose shuttlePose(const Shuttle& s, Shuttle::Stage stage, double clock);
/// The heading it lands with at the target pad: aligned with the pad
/// (`ShuttleBay::padHeading`), its nose to the pad's back.
AUTOCRAFTCORE_API double shuttleLandingHeading(const Shuttle& s);
/// How long `stage` lasts (the ground: no end of its own).
AUTOCRAFTCORE_API double shuttleStageSeconds(const Shuttle& s, Shuttle::Stage stage);

} // namespace ac
