// `AcPilotWalls`: the pilot's cameras against the buildings' walls
// (docs/citadel-interior.md §6, docs/mmorpg-shots.md #12). Buildings are not
// in the rays (AcRays.h), so the cameras sweep against their solid slabs
// instead: `ac::Rules::footprint` in world space, one truth with the pilot's
// walk and the shelter. The door's slab is solid while the door is not open
// (`open < 0.95`); a Citadel's roof is the hall's ceiling.
//
// The third-person boom (`FAcPilotChase::Follow`) sweeps a sphere of
// `ChaseRadius` from its pivot to the camera and stops it at the first
// slab; the first-person eye (`FAcPilotCamera`'s, through `ClampEye`) sweeps
// a smaller one from over the unit's centre to the eye.
#pragma once

#include "CoreMinimal.h"

#include "Types.h"

class UWorld;

namespace AcPilotWalls
{
	/// The third-person boom's sphere, cells.
	inline constexpr double ChaseRadius = 0.25;
	/// The first-person eye's (the near plane is 0.04 cells).
	inline constexpr double EyeRadius = 0.12;
	/// How far from the camera the structures are swept against, cells.
	inline constexpr double Range = 20.0;
	/// A slab no taller than this (cells over its ground) is a low wall the
	/// chase camera lifts to look over.
	inline constexpr double LowWall = 3.0;

	/// One solid box of a structure in the world (UE cm).
	struct FSlab
	{
		FBox Box;
		/// Under `LowWall` tall.
		bool bLow = false;
	};

	/// The solid slabs of `S` standing on ground `GroundCells` (its centre's
	/// height), sunk as the renderer sinks a building under construction:
	/// its walls, its door while not open, its roof.
	AUTOCRAFT_API void SlabsOf(const ac::Structure& S, double GroundCells, TArray<FSlab>& Out);
	/// The slabs of every structure within `Range` cells of `Near` in the
	/// running game (none out of the MMORPG mode).
	AUTOCRAFT_API void Gather(UWorld* World, ac::Vec2 Near, TArray<FSlab>& Out);

	/// How far along `From` → `To` (0 to 1) a sphere of `RadiusCm` goes
	/// before it touches a slab; 1 when it touches none. A slab the sphere
	/// already touches at `From` is swept with the point alone (so a camera
	/// beside a wall still stops at it); one `From` is inside is left out.
	/// `bOutLow`: whether what stopped it is a low slab.
	AUTOCRAFT_API double Sweep(const FVector& From, const FVector& To, double RadiusCm, TConstArrayView<FSlab> Slabs,
		bool* bOutLow = nullptr);

	/// The first-person eye `Eye` kept out of the walls: swept from over
	/// `Centre` (the unit's centre, at the eye's height) with `EyeRadius`.
	AUTOCRAFT_API FVector ClampEye(UWorld* World, const FVector& Centre, const FVector& Eye);
}
