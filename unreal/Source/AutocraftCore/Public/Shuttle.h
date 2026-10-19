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

#include <optional>
#include <string>

namespace ac {

/// The bay (concept B2) and the hull round it, in the ship's frame. The
/// numbers are c4-ship-look's model's; *assumed* times are the manager's,
/// for the user to tune.
struct AUTOCRAFTCORE_API ShuttleBay {
    /// The deck's top (the bay's floor) and the ceiling's underside: 2 tall
    /// clear inside.
    static constexpr double deck = 0.5;
    static constexpr double ceiling = 2.5;
    /// Inside, walkable: the bay x -2.5 … 1.5 (4 long), the cockpit nook
    /// up to the console's face at 2.36; 2 wide (y −1 … 1, the ribs' faces).
    static constexpr double rear = -2.5;
    static constexpr double front = 2.36;
    static constexpr double halfWide = 1.0;
    /// The hull seen from above (the opening's jambs at the back, the nose
    /// tip, the nacelles out to ±2.48): nothing but the open ramp stands
    /// outside it. The walls from the clear bay out are solid; the nose
    /// block behind the console from `noseFrom`.
    static constexpr double hullRear = -2.72;
    static constexpr double hullFront = 4.1;
    static constexpr double hullHalfWide = 2.48;
    static constexpr double noseFrom = 2.62;
    static constexpr double roofTop = 2.83;
    /// The ramp hinges at the opening's foot (x `rear`, z `deck`); 2 long,
    /// down it lies on the ground at `rampFoot` (a 14.5° slope).
    static constexpr double rampLength = 2.0;
    static double rampFoot();
    /// Seconds from shut to down (*assumed*), and from how far down it is
    /// walkable (as a Citadel's door, `Simulation::doorOpen`).
    static constexpr double rampSeconds = 3.0;
    static constexpr double rampDown = 0.95;
    /// The orange ramp post inside by the opening, and the green take-off
    /// button on the console under the windshield (its top, z up).
    static constexpr Vec3 rampButton{-2.26, -0.84, 1.13};
    static constexpr Vec3 takeOffButton{2.50, 0, 1.44};
    /// How near (seen from above, cells) a unit presses either (*assumed*).
    static constexpr double reach = 1.5;
    /// It leaves once nobody has been in the bay or on the ramp this long
    /// (*assumed* 5 s), or `waitSeconds` after a call when nobody came
    /// aboard (*assumed*); it lifts away over `departSeconds`.
    static constexpr double leaveSeconds = 5.0;
    static constexpr double waitSeconds = 60.0;
    static constexpr double departSeconds = 6.0;

    /// Region point `p` in the frame of `s`, and back.
    static Vec2 local(const Shuttle& s, Vec2 p);
    static Vec2 world(const Shuttle& s, Vec2 local);
    /// `local` is in the bay (the cockpit nook too), the bay shrunk by
    /// `grow` on every side.
    static bool inBay(Vec2 local, double grow = 0);
    /// `local` is over the ramp's strip behind the opening (down or not),
    /// grown by `grow`.
    static bool onRamp(Vec2 local, double grow = 0);
    /// `local` is within the hull seen from above grown by `grow`, or on the
    /// ramp's strip.
    static bool covers(Vec2 local, double grow = 0);
    /// The height a unit at `local` stands at, over the frame's ground: the
    /// deck in the bay, the ramp's slope on it (`rampOpen`, `Door::open`),
    /// else the ground (0).
    static double floor(Vec2 local, double rampOpen);
    /// The solid piece a walker of radius `grow` at `local` stands in (the
    /// hull's sides and nose, the console, the ramp post, the ramp when it
    /// is not down), nil when none: its box, for the slide's contact.
    static std::optional<Block> solidAt(Vec2 local, double grow, bool rampDown);
    /// The ship's solid parts for the cameras (as `Rules::footprint`): the
    /// hull's sides and nose, the deck slab and the ceiling as walls with
    /// their heights, `door` the shut ramp, `interior` the bay, `roof` over
    /// it, `furniture` the console and the ramp post.
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
/// approach over the target pad (the way the client drew it since F2,
/// `AcPilotFlight.h`); lifting away.
AUTOCRAFTCORE_API ShuttlePose shuttlePose(const Shuttle& s);
AUTOCRAFTCORE_API ShuttlePose shuttlePose(const Shuttle& s, Shuttle::Stage stage, double clock);
/// The heading it lands with at the target pad: along the way from `from`
/// to `to` (its own heading for a short hop).
AUTOCRAFTCORE_API double shuttleLandingHeading(const Shuttle& s);
/// How long `stage` lasts (the ground: no end of its own).
AUTOCRAFTCORE_API double shuttleStageSeconds(const Shuttle& s, Shuttle::Stage stage);

} // namespace ac
