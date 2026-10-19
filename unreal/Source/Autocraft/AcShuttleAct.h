// `AcShuttleAct`: what the act key does at and in a walk-in Dropship (C4,
// docs/haul-and-ferry.md sections 3 and 4) when nothing else takes it:
// standing on a realm Landing Pad with no Dropship on it, call one
// ("E · CALL THE DROPSHIP"); in a landed one's bay at the orange ramp post,
// lower or raise the ramp ("E · OPEN THE RAMP", "E · CLOSE THE RAMP"); at
// the console, the green button: the flight menu ("E · TAKE OFF"). The
// core's refusals (`Simulation::rampRefusal`, `takeOffRefusal`: "Someone is
// on the ramp", "Close the ramp first", "Wait for the landing") are the
// prompt in the blocked tone, and the key sends nothing. The pawn sends the
// MMORPG client's requests on it; the buttons' press is drawn
// (`AcShuttleShip::Pressed`).
//
// Driving a Mule (the manager's rule, 2026-10-19), E climbs off unless the
// crosshair is on a button in reach (the ramp post, the take-off button and
// its console) or on a Citadel's door: then E works that. The pawn works out
// what the crosshair is on each frame (`SetDriverAim`); `Find` here and
// `AcCitadelAct::Find` follow it for a Mule.
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Types.h"

namespace ac
{
	class Simulation;
}

namespace AcShuttleAct
{
	struct FAct
	{
		enum class EKind : uint8
		{
			Call,
			Ramp,
			TakeOff
		};
		EKind Kind = EKind::Call;
		/// The ship (Ramp, TakeOff), or the pad called at (Call).
		int64 Ship = 0;
		int64 Pad = 0;
		/// Ramp: lower it (else raise it).
		bool bOpen = true;
		/// Why the key would be refused now: the prompt says it, the key sends nothing.
		TOptional<FString> Why;

		/// "E · CALL THE DROPSHIP", "E · OPEN THE RAMP", "E · CLOSE THE
		/// RAMP", "E · TAKE OFF", or `Why`.
		AUTOCRAFT_API FString Prompt() const;
	};

	/// What the crosshair of a driven Mule is on this frame (the pawn sets it
	/// on the game thread before the prompt and the key read it).
	struct FDriverAim
	{
		bool bRampButton = false;
		bool bTakeOffButton = false;
		bool bDoor = false;
	};
	AUTOCRAFT_API void SetDriverAim(const FDriverAim& Aim);
	AUTOCRAFT_API FDriverAim DriverAim();
	/// The crosshair's ray (`Eye`, along `Dir`, a unit vector) is on the point
	/// `At` (world cm): within `ConeDeg` of it and `Within` cm.
	AUTOCRAFT_API bool Aims(const FVector& Eye, const FVector& Dir, const FVector& At, double ConeDeg = 9.0, double Within = 700.0);

	/// The act for unit `U` (a Prospector on foot, or driving its Mule: then
	/// only the button its crosshair is on, `DriverAim`), or unset. The caller
	/// checks first that nothing else takes the key.
	AUTOCRAFT_API TOptional<FAct> Find(const ac::Simulation& Sim, const ac::Unit& U);
}
