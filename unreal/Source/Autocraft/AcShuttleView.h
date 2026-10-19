// The walk-in Dropships as the game draws them (C4, docs/haul-and-ferry.md;
// the sim's half is `ac::Shuttle`, Shuttle.h). Each frame, on the game
// thread, `Update` reads the ships from the shown state and works out where
// each one is now: its pose (`ac::shuttlePose`, its stage's clock run on by
// the real seconds since the clock last changed, so the flight is smooth
// between snapshots), and the **carrier**: the transform from the ship's
// frame on the pad (where the sim keeps the bay and everyone in it, in
// flight too) to the ship where it is drawn. Everything aboard is drawn in
// the frame and carried: the riders (the world renderer, `Carry`), the
// pilot's cameras (the pawn works out its view in the frame, against the
// bay's walls, `Slabs`, and carries it), the ship's own model
// (`AAcShuttleShip`).
#pragma once

#include "CoreMinimal.h"

#include "AcPilotWalls.h"

#include "Shuttle.h"
#include "Types.h"

namespace AcShuttleView
{
	/// One Dropship as drawn this frame.
	struct FShip
	{
		ac::Shuttle Ship;
		/// Its pose now (the clock run on), and as Unreal transforms: the
		/// frame on the pad (its ground point, turned to its heading), the
		/// ship's root where it is drawn, and the carrier (frame space to
		/// the world: `Frame⁻¹ · Placement`).
		ac::ShuttlePose Pose;
		double Clock = 0;
		FTransform Frame = FTransform::Identity;
		FTransform Placement = FTransform::Identity;
		FTransform Carrier = FTransform::Identity;
	};

	/// The ships of `State` now (`Now`: real seconds). Game thread; the
	/// first call of a frame works them out and the others keep them, so the
	/// renderer, the pawn's camera and the ships' actors (whoever comes
	/// first) all draw one pose a frame.
	AUTOCRAFT_API void Update(const ac::GameState& State, double Now);
	/// The same from the running game's shown state (none out of it).
	AUTOCRAFT_API void Update(const UWorld* World);
	/// The ships drawn this frame (read-only between `Update`s: the
	/// renderer's pose threads read it).
	AUTOCRAFT_API TConstArrayView<FShip> Ships();
	AUTOCRAFT_API const FShip* Find(int64 Id);
	/// The ship unit `U` rides or stands in (its bay or ramp), as
	/// `ac::Simulation::aboard`; null: none.
	AUTOCRAFT_API const FShip* Aboard(const ac::Unit& U);

	/// The Unreal transforms of the sim's frame and pose.
	AUTOCRAFT_API FTransform FrameTransform(const ac::Shuttle& S);
	AUTOCRAFT_API FTransform PoseTransform(const ac::ShuttlePose& P);
	/// The height (cm, the frame's: the pad's ground plus the deck or the
	/// ramp under it) a unit at region point `P` stands at.
	AUTOCRAFT_API double StandCm(const ac::Shuttle& S, ac::Vec2 P);

	/// A unit's placement aboard: `Placement` (its ground point turned to its
	/// heading) put on the deck or the ramp (`Stand`) and carried to where
	/// the ship is (`Lift`). False (unchanged) when `U` is not aboard.
	AUTOCRAFT_API bool Stand(const ac::Unit& U, FTransform& Placement);
	AUTOCRAFT_API bool Lift(const ac::Unit& U, FTransform& Placement);

	/// The bay's solid slabs in frame space (`ac::ShuttleBay::footprint`:
	/// the hull's sides and nose, the ceiling, the shut ramp while it is not
	/// down, the console and the ramp post), for the cameras' sweeps.
	AUTOCRAFT_API void Slabs(const ac::Shuttle& S, TArray<AcPilotWalls::FSlab>& Out);
}
