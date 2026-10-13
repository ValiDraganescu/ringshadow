// The MMORPG world's pad flight as the player sees it (docs/mmorpg-m4.md,
// Q3). The server's flight is the rule (docs/worlds.md, "Flights": the
// Prospector leaves its region at lift-off, 4 s pass, it lands on the other
// pad, a RegionChange when that pad is in another region); this is only the
// picture, drawn by the pilot pawn (`AAcPilotPawn`, AcPilotFlight.cpp):
//
//   lift-off  the unit leaves the mirror at the server's lift-off: a copy of
//             its model (`AAcFlightShip`) rises off the pad from where it
//             stood, accelerating (`LiftHeight`), dust and a thrust glow
//             under it; the camera goes from the pilot's view up behind it
//             and pulls back (`LiftSeconds`).
//   transit   the ship climbs on and the region falls away under it, the
//             camera behind and over it, tilting down and turning slowly
//             round it; near the server's landing time the picture dims. It
//             never stops moving: a late RegionChange holds this view until
//             it comes, at most `AAcPilotPawn::MaxHoldSeconds` (then the
//             top-down camera, as before). A RegionChange read before its
//             region's first snapshot: dark until that snapshot.
//   landing   the first snapshot shows the unit driven (a new region's, or
//             the same one back): the ship comes down onto the unit's real
//             place on the pad (`LandHeight`, `LandSeconds`), the picture
//             brightens and the camera settles into the pilot's own view.
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
	/// The lift-off: the ship off the pad, the camera up behind it.
	inline constexpr double LiftSeconds = 1.5;
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

	/// The ship's height `T` seconds after lift-off: accelerating up
	/// (`LiftTop` at the lift-off's end), faster in transit, then a slow
	/// drift up once the landing is a second overdue (a late RegionChange).
	inline double LiftHeight(const double T)
	{
		if (T <= 0) return 0;
		if (T <= LiftSeconds)
		{
			const double S = T / LiftSeconds;
			return LiftTop * S * S;
		}
		const double Late = ServerSeconds + 1.0;
		// On at the lift-off's last speed, speeding up.
		auto Climb = [](const double D) { return LiftTop + 2.0 * LiftTop / LiftSeconds * D + 3.0 * D * D; };
		if (T <= Late) return Climb(T - LiftSeconds);
		return Climb(Late - LiftSeconds) + 4.0 * (T - Late);
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
