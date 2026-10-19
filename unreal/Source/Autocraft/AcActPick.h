// `AcActPick`: the one rule for the act key E in the MMORPG mode (C5,
// docs/acting.md), on foot and driving, first and third person. E acts only
// when the crosshair's ray (the camera's line through the reticle) enters
// the box of the thing itself (`ac::Act`: a door panel or the door, a gun's
// console, the Mule's perch or hopper, a Foundry's or a Landing Pad's call
// post, the Dropship's ramp post or take-off button) before the world stops
// the ray, and the body's edge is within `ac::Act::reach` of that box. No
// box under the crosshair: a building of the realm the crosshair is on,
// within reach, takes the demolish hold (`AcDemolish`); else nothing, and
// E does nothing, except driving a Mule, where E climbs off. One thing at
// a time, the one the ray meets first, so two acts never compete; the
// prompt names it. Only E: the click and Space keep the core's acts (fire,
// drill, pump, weld, repair, enter a Derrick).
//
// The pawn gathers the boxes (`Targets`), picks with its crosshair's ray
// (`Pick`) and works out the act (`Act`) each frame; the cockpit shows its
// prompt, the key sends it. Plain C++ over `ac::Simulation` and Unreal's
// math (no world): what the world says comes in `FWorld`.
#pragma once

#include "CoreMinimal.h"

#include "AcMuleAct.h"

#include "Simulation.h"

namespace AcActPick
{
	enum class EKind : uint8
	{
		Door,
		Gun,
		Board,
		Fill,
		Buy,
		Call,
		Ramp,
		TakeOff,
		Demolish,
		Leave
	};

	/// How far past the world's first hit (cells) a box still counts: a
	/// panel on a wall, a console against the hall's corner.
	inline constexpr double Tolerance = 0.4;

	/// One thing E works and its box, in the world (cm).
	struct FTarget
	{
		EKind Kind = EKind::Door;
		/// The Citadel, the Mule, the Foundry, the Landing Pad, the ship.
		int64 Id = 0;
		/// Door: 0 the outer panel, 1 the inner, 2 the door; Gun: the gun.
		int32 Index = INDEX_NONE;
		/// The box's middle and turn, and its half sides (cm).
		FTransform Frame = FTransform::Identity;
		FVector Half = FVector::ZeroVector;
		/// From the driven body's edge to the box, across and up (cells).
		double Gap = 0;
	};

	/// What the world says.
	struct FWorld
	{
		/// The ground's height under a point (cells).
		TFunction<double(ac::Vec2)> Ground;
		/// A ship's frame drawn now (its cells, z from its ground, to the
		/// world in cm: `AcShuttleView::FShip::Placement`); unset: from the
		/// core (on its pad).
		TFunction<TOptional<FTransform>(int64)> Ship;
		/// The driven body's feet (cells, the world's height).
		double Feet = 0;
	};

	/// Every box near the driven unit `U` it may work: on foot (a
	/// Prospector), the door's, the gun consoles, every Mule's perch and
	/// hopper, the call posts, in a ship's bay its buttons; driving a Mule,
	/// the doors and the ship's buttons.
	AUTOCRAFT_API TArray<FTarget> Targets(const ac::Simulation& Sim, const ac::Unit& U, const FWorld& W);

	/// The crosshair's ray: from `Start` toward `End`, the world's first hit
	/// at `Blocked` (`End` when it meets nothing).
	struct FRay
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FVector Blocked = FVector::ZeroVector;
	};
	/// Where the segment `Start`-`End` enters `T`'s box: 0 to 1 along it (0
	/// from inside); unset: it misses.
	AUTOCRAFT_API TOptional<double> Enters(const FTarget& T, const FVector& Start, const FVector& End);
	/// The box the ray meets first before the world stops it (with
	/// `Tolerance`), if it is within reach; `bOnBox` says whether the ray is
	/// on a box at all (in reach or not).
	AUTOCRAFT_API TOptional<FTarget> Pick(TConstArrayView<FTarget> Targets, const FRay& Ray, bool* bOnBox = nullptr);

	/// The gap from `U`'s body (feet at `Feet`, cells) to the point `At`
	/// (world cm), across and up (cells).
	AUTOCRAFT_API double GapTo(const ac::Unit& U, double Feet, const FVector& At);

	struct FAct
	{
		EKind Kind = EKind::Door;
		int64 Id = 0;
		int32 Index = INDEX_NONE;
		/// Door, Ramp: open it (else close it).
		bool bOpen = true;
		/// Door: the pilot stands in the hall (the inner panel's side).
		bool bInside = false;
		/// Leave: the Prospector riding the Mule.
		int64 Rider = 0;
		/// Demolish: the building's kind.
		ac::StructureKind Building = ac::StructureKind::citadel;
		/// Why the key would be refused now: the prompt in the blocked
		/// tone, the key's note; nothing is sent.
		TOptional<FString> Why;

		/// "E · OPEN", "E · TAKE GUN 1 SE", "E · CLIMB ON", "E · FILL THE
		/// MULE", "E · BUY MULE · 600 ORE · 150 MH", "E · CALL THE DROPSHIP",
		/// "E · CLOSE THE RAMP", "E · TAKE OFF", "HOLD E · DEMOLISH …",
		/// "E · CLIMB OFF", or `Why`.
		AUTOCRAFT_API FString Prompt() const;
	};

	/// What E does for the driven `U`: on `Picked` (in reach); with no box
	/// under the crosshair (`bOnBox` false), the realm's building
	/// `Crosshair` (INDEX_NONE: none) hit `CrosshairGap` cells from the body
	/// takes the demolish hold; else, driving a Mule, climb off. Unset: E
	/// does nothing.
	/// The building whose call post's button the pilot's crosshair is on
	/// (0: none): the pawn sets it each frame, `AcBuildingPose::CallPost`
	/// lights that button brighter.
	AUTOCRAFT_API void SetLit(int64 Building);
	AUTOCRAFT_API int64 Lit();

	AUTOCRAFT_API TOptional<FAct> Act(const ac::Simulation& Sim, const ac::Unit& U, const AcMuleAct::FOwn& Own,
		const TOptional<FTarget>& Picked, bool bOnBox, int64 Crosshair, double CrosshairGap);
}
