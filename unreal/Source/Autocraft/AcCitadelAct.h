// `AcCitadelAct`: the Citadel's side of the act key (C1,
// docs/citadel-interior.md §7): the guns' names, the gun a pilot mans and
// its look held in the gun's arc, the gun's eye, the door panel lit for the
// pose. What E does at a Citadel (the door from its panels or the door
// itself, a gun from its console) is `AcActPick`'s (C5, docs/acting.md);
// the pawn lights the panel on the pilot's side while E is on the door
// (`SetPanel`), `PoseCitadel` reads it.
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Simulation.h"

namespace AcCitadelAct
{
	/// Gun `Gun`'s name on the compass, in the core's order (+x+y, -x+y,
	/// -x-y, +x-y; north is -y, as on the HUD's compass): "SE", "SW", "NW", "NE".
	AUTOCRAFT_API const TCHAR* Compass(int32 Gun);
	/// "TURRET 1 · SE" style: the gun's number (1 to 4) and its compass.
	AUTOCRAFT_API FString GunName(int32 Gun);

	/// The gun `U` mans: its Citadel's id and the gun; unset: none.
	AUTOCRAFT_API TOptional<TPair<int64, int32>> Manned(const ac::Simulation& Sim, const ac::Unit& U);

	/// At gun `Gun`: the look's yaw `Was` turned by `Delta` (radians), held
	/// in the gun's arc (`Defence::cornerAngle` ± `gunArcHalf`) with a hard
	/// stop: measured from the arc's middle, never wrapped, so a turn past a
	/// stop stays at it however far the mouse goes.
	AUTOCRAFT_API double LookYaw(int32 Gun, double Was, double Delta);
	/// The look's pitch held to the gun's limits (`gunPitchMin`, `gunPitchMax`).
	AUTOCRAFT_API double LookPitch(double Pitch);
	/// Where gun `Gun` of `Citadel` (standing on ground `GroundCells`) looks
	/// from along `Yaw` and `Pitch` (`Defence::gunEye`: the eye rides the
	/// gun's cradle, its barrels in the lower middle), in the world (cm).
	AUTOCRAFT_API FVector Eye(const ac::Structure& Citadel, double GroundCells, int32 Gun, double Yaw, double Pitch);

	/// The door panel lit for the pose (0 outside, 1 inside) of Citadel
	/// `Citadel`, or INDEX_NONE. The pawn sets it each frame on the game
	/// thread (`SetPanel`; INDEX_NONE citadel 0 for none) before the poses.
	AUTOCRAFT_API void SetPanel(int64 Citadel, int32 Panel);
	AUTOCRAFT_API int32 Panel(int64 Citadel);
}
