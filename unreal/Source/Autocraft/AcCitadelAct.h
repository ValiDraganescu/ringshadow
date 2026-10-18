// `AcCitadelAct`: what the act key does in and at a Citadel (C1,
// docs/citadel-interior.md §7) when nothing else takes it (the drill, a
// strike, a repair, a Derrick, a building held out come first): a free gun
// station within `Defence::stationReach` ("E · TAKE GUN 1 NE"), else the
// door within `Rules::doorReach` of its slab ("E · OPEN" on either side,
// "E · CLOSE" inside while it opens). The pawn sends the MMORPG client's
// Door and Man requests on it; the cockpit shows its prompt; `PoseCitadel`
// lights the door panel on the pilot's side while it is in reach.
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Simulation.h"

namespace AcCitadelAct
{
	/// Gun `Gun`'s name on the compass, in the core's order (+x+y, -x+y,
	/// -x-y, +x-y; +y is north): "NE", "NW", "SW", "SE".
	AUTOCRAFT_API const TCHAR* Compass(int32 Gun);
	/// "TURRET 1 · NE" style: the gun's number (1 to 4) and its compass.
	AUTOCRAFT_API FString GunName(int32 Gun);

	struct FAct
	{
		enum class EKind : uint8
		{
			Man,
			Door
		};
		EKind Kind = EKind::Door;
		int64 Citadel = 0;
		/// Man: the gun (0 to 3).
		int32 Gun = INDEX_NONE;
		/// Door: open (else close).
		bool bOpen = true;
		/// The pilot stands in the hall (the inner door panel's side).
		bool bInside = false;
		/// Why the key would be refused now (someone in the doorway): the
		/// prompt says it and the key sends nothing.
		TOptional<FString> Why;

		/// "E · TAKE GUN 1 NE", "E · OPEN", "E · CLOSE", or `Why`.
		AUTOCRAFT_API FString Prompt() const;
	};

	/// The act at a Citadel for unit `U`, or unset (also while it mans a
	/// gun). The caller checks first that nothing else takes the key.
	AUTOCRAFT_API TOptional<FAct> Find(const ac::Simulation& Sim, const ac::Unit& U);

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
