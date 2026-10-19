// The act key's targets (C5, docs/acting.md): each thing E works is a small
// box where it is drawn (a door panel, a gun's console, the Mule's perch and
// hopper, the Dropship's ramp post and take-off button, the call posts of a
// Landing Pad and a Foundry). The client acts only with the crosshair's ray
// on the box and the body's edge within `reach` of it (AcActPick); the
// server's refusals measure the same boxes from above with `serverReach`, a
// little looser, so an act the client allows is never refused.
//
// A box is a `Block` in the world's frame (cells; the ship's buttons in the
// ship's frame), its heights from the ground under what it belongs to (the
// building's, the Mule's, the landed ship's).
#pragma once

#include "AutocraftCoreApi.h"
#include "Rules.h"
#include "SimdMath.h"
#include "Types.h"

#include <optional>

namespace ac {

struct Shuttle;

struct AUTOCRAFTCORE_API Act {
    /// The client's reach: from the body's edge to the box (cells).
    static constexpr double reach = 1.5;
    /// The server's: the same boxes, seen from above, a little looser.
    static constexpr double serverReach = reach + 0.5;

    /// `u`'s body seen from above: a hauler's box (`Mule::length` by
    /// `width`, along its heading), else its walk's circle (as a square of
    /// that half side, so the gap is never more than the circle's).
    static Block body(const Unit& u);
    /// How tall the body stands over its ground: a Prospector's 1.5; a
    /// hauler's driver on its perch's deck.
    static double bodyTop(const Unit& u);
    /// The gap between two boxes seen from above (0 when they touch).
    static double gap(const Block& a, const Block& b);
    /// From `u`'s body to box `b` (the world's frame), seen from above.
    static double gap(const Unit& u, const Block& b) { return gap(body(u), b); }

    /// A Citadel's door: its leaf while it stands (the door slab), and its
    /// panels beside the doorway, outside (`inside` false) and in.
    static std::optional<Block> doorLeaf(const Structure& citadel);
    static std::optional<Block> doorPanel(const Structure& citadel, bool inside);
    /// Gun `gun`'s station console in the hall (its furniture, turned onto
    /// the corner's diagonal).
    static Block console(const Structure& citadel, int gun);
    /// The Mule's perch (the levers and the rail at its front) and its hopper.
    static Block perch(const Unit& mule);
    static Block hopper(const Unit& mule);
    /// The walk-in Dropship's ramp post's button and its console's take-off
    /// button, in the ship's frame (heights from the landed ship's ground),
    /// and in the world's.
    static Block rampPostLocal();
    static Block takeOffLocal();
    static Block rampPost(const Shuttle& s);
    static Block takeOff(const Shuttle& s);
    /// The call posts: a Landing Pad's, just off its front-right corner
    /// (calls the Dropship), and a Foundry's, beside its door (buys a Mule).
    /// Where they stand from the building's centre (cells; front is +y), and
    /// their button's box.
    static Vec2 padPostAt(double radius);
    static Vec2 foundryPostAt(double radius);
    static Block padPost(const Structure& pad);
    static Block foundryPost(const Structure& foundry);
    /// The post itself, solid to a pilot's walk: a complete Landing Pad's or
    /// Foundry's (nil: any other building, or one still going up).
    static std::optional<Block> postFoot(const Structure& s);
    /// A call post's size: the post's half side, its button's top (the box
    /// covers the post's head and the button).
    static constexpr double postHalf = 0.16;
    static constexpr double postButtonLow = 0.75, postTop = 1.25;
};

} // namespace ac
