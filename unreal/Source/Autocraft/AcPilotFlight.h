// The MMORPG world's pad flight as the player sees it (docs/mmorpg-m4.md,
// Q3; a launch worth watching since F1, docs/mmorpg-m7.md). The server's
// flight is the rule (docs/worlds.md, "Flights": the Prospector leaves its
// region at lift-off, 4 s pass, it lands on the other pad, a RegionChange
// when that pad is in another region); this is only the picture, drawn by
// the pilot pawn (`AAcPilotPawn`, AcPilotFlight.cpp). The whole flight is
// framed toward the ringed giant (`FAcGasGiant::Direction`): the ship leaves
// toward it, the camera looks along it.
//
//   spool-up  the unit leaves the mirror at the server's lift-off: a copy of
//             its model (`AAcFlightShip`) stands on the pad and lights up:
//             its glow climbs, two jets of flame push down out of its belly
//             with pale heat over them, a ring of dust races out over the
//             ground and the pad is scorched (`SpoolSeconds`). The camera
//             leaves the pilot's view for a low spot on the ground behind
//             the pad, the giant beyond it.
//   lift-off  the ship rises, accelerating (`LiftHeight`), the jets and the
//             dust on under it; the camera stays low, rising a little,
//             looking up at it against the giant (`LiftSeconds` in all).
//   transit   the ship climbs on and heads off toward the giant
//             (`Drift`); the camera swings up behind it and levels out so
//             the giant fills the sky over it and the region falls away
//             below; near the server's landing time the picture dims. It
//             never stops moving: a late RegionChange holds this view until
//             it comes, at most `AAcPilotPawn::MaxHoldSeconds` (then the
//             top-down camera, as before). A RegionChange read before its
//             region's first snapshot: dark until that snapshot.
//   landing   the first snapshot shows the unit driven (a new region's, or
//             the same one back): the ship comes in along the same way and
//             down onto the unit's real place on the pad on its jets (retro
//             thrust, `LandHeight`, `LandSeconds`), dust blown out under it
//             and a ring of it at touch down; the picture brightens and the
//             camera comes from behind and beside it into the pilot's view.
//             The real unit is hidden meanwhile and the pilot's input held
//             (no walk, no act): the unit stands where the server put it,
//             so the prediction has nothing to fight. At touch down the
//             ship goes, the unit is drawn, control is back.
//
// Heights in cells over the ground; times in seconds since the stage began.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcPilotFlight.generated.h"

class UStaticMeshComponent;

enum class EAcFlightStage : uint8
{
	None,
	LiftOff,
	Transit,
	Landing,
};

namespace AcPilotFlight
{
	/// The spool-up: the ship lit on the pad, not yet off it.
	inline constexpr double SpoolSeconds = 0.7;
	/// The lift-off stage, the spool-up in it: the ship off the pad, the
	/// camera low behind it.
	inline constexpr double LiftSeconds = 2.0;
	/// The landing: down onto the pad, the camera into the pilot's view.
	inline constexpr double LandSeconds = 1.5;
	/// The server's flight (`WorldParams::flightTime`): when the landing is
	/// due, for the dimming. Only the picture's guess; the server decides.
	inline constexpr double ServerSeconds = 4.0;
	/// How high the landing starts (cells over the pad).
	inline constexpr double LandFrom = 16.0;
	/// The dimming at the landing's due time (0 none, 1 black): the view
	/// still shows, moving.
	inline constexpr double TransitDim = 0.8;

	/// How high the lift-off takes the ship (cells).
	inline constexpr double LiftTop = 8.0;

	/// How high the spool-up lifts the ship on its jets (cells): light on
	/// its legs.
	inline constexpr double SpoolLift = 0.15;

	/// The ship's height `T` seconds after lift-off: a hand's breadth in the
	/// spool-up, then accelerating up (`LiftTop` at the lift-off's end),
	/// faster in transit, then a slow drift up once the landing is a second
	/// overdue (a late RegionChange).
	inline double LiftHeight(const double T)
	{
		if (T <= 0) return 0;
		if (T <= SpoolSeconds) return SpoolLift * FMath::SmoothStep(0.0, SpoolSeconds, T);
		constexpr double Climb0 = LiftSeconds - SpoolSeconds;
		if (T <= LiftSeconds)
		{
			const double S = (T - SpoolSeconds) / Climb0;
			return SpoolLift + (LiftTop - SpoolLift) * S * S;
		}
		const double Late = ServerSeconds + 1.0;
		// On at the lift-off's last speed, speeding up.
		auto Climb = [](const double D) { return LiftTop + 2.0 * (LiftTop - SpoolLift) / (LiftSeconds - SpoolSeconds) * D + 3.0 * D * D; };
		if (T <= Late) return Climb(T - LiftSeconds);
		return Climb(Late - LiftSeconds) + 4.0 * (T - Late);
	}

	/// How far the ship has gone toward the giant `T` seconds after lift-off
	/// (cells, flat): nothing until the lift-off's end, then speeding up.
	inline double Drift(const double T)
	{
		const double D = FMath::Clamp(T - LiftSeconds, 0.0, ServerSeconds + 1.0 - LiftSeconds);
		const double Late = FMath::Max(0.0, T - (ServerSeconds + 1.0));
		return 2.0 * D * D + 4.0 * (ServerSeconds + 1.0 - LiftSeconds) * Late;
	}

	/// How far back along its way the landing ship still is `T` seconds into
	/// the landing (cells): coming in, slowing to stop over the pad.
	inline double LandDrift(const double T)
	{
		const double S = FMath::Clamp(T / LandSeconds, 0.0, 1.0);
		return 9.0 * (1.0 - S) * (1.0 - S) * (1.0 - S);
	}

	/// The jets' strength `T` seconds after lift-off (0..1): up through the
	/// spool-up, full in the climb, easing off once high.
	inline double LiftThrust(const double T)
	{
		return FMath::SmoothStep(0.0, SpoolSeconds, T) * (1.0 - 0.5 * FMath::SmoothStep(LiftSeconds, LiftSeconds + 1.5, T));
	}

	/// The ship's height `T` seconds into the landing: from `LandFrom`,
	/// slowing to touch down at `LandSeconds` with no speed left.
	inline double LandHeight(const double T)
	{
		const double S = FMath::Clamp(T / LandSeconds, 0.0, 1.0);
		return LandFrom * (1.0 - S) * (1.0 - S);
	}

	/// The dimming `T` seconds after lift-off, in transit (0..`TransitDim`).
	inline double TransitFade(const double T)
	{
		return TransitDim * FMath::SmoothStep(ServerSeconds - 0.8, ServerSeconds, T);
	}

	/// The dimming `T` seconds into the landing (from black to none).
	inline double LandFade(const double T) { return 1.0 - FMath::SmoothStep(0.0, 0.5, T); }

	/// Smootherstep: no jolt at either end.
	inline double Ease(const double T)
	{
		const double S = FMath::Clamp(T, 0.0, 1.0);
		return S * S * S * (S * (S * 6.0 - 15.0) + 10.0);
	}
}

/// The flight's copy of the driven unit's model, at rest (AcModelRow's way:
/// static meshes with the model's materials, its team colour). Only the
/// picture: the real unit is out of the region in flight, and hidden while
/// it lands.
UCLASS()
class AUTOCRAFT_API AAcFlightShip : public AActor
{
	GENERATED_BODY()

public:
	AAcFlightShip();

	static AAcFlightShip* SpawnFor(UWorld* World);

	/// The model `Model` of team `Team` placed at `Placement` (the renderer's
	/// placement: the ground point under the unit, turned to its heading),
	/// lifted `Lift` cm. False when the model is unknown.
	bool Show(FName Model, int32 Team, const FTransform& Placement, double Lift);
	/// Its lights' glow (the models' emission scale; 1 at rest): the engines
	/// lit for the flight.
	void SetGlow(double Glow);
	void Hide();
	bool IsShown() const { return bShown; }

private:
	void Build(FName Model, int32 Team);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	FName Key;
	int32 KeyTeam = -1;
	bool bShown = false;
};
