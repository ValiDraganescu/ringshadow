// A model at rest (`AAcFlightShip`), what is left of F2's Dropship flight
// show: the walk-in Dropship replaced it (C4, docs/haul-and-ferry.md:
// `ac::Shuttle`, drawn by AcShuttleShip.h and AcShuttleView.h). The night-haze
// picture (AcMmoShots.cpp, -AcMmoShotsNightHaze) draws a Dropship with it, at
// the flight's size (`ShipScale`) and approach height (`ApproachHigh`).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcPilotFlight.generated.h"

class UStaticMeshComponent;

namespace AcPilotFlight
{
	/// The ship drawn bigger than the strategy game's Dropship: it carries a
	/// Prospector, and fills a 3-cell pad.
	inline constexpr double ShipScale = 1.6;
	/// The cruise height over the target pad where the old flight's approach
	/// started (cells).
	inline constexpr double ApproachHigh = 40.0;
}

/// A model at rest (AcModelRow's way: static meshes with the model's
/// materials, its team colour), placed by hand: only the picture, no unit
/// behind it.
UCLASS()
class AUTOCRAFT_API AAcFlightShip : public AActor
{
	GENERATED_BODY()

public:
	AAcFlightShip();

	/// The one named `Name`, spawned on first ask.
	static AAcFlightShip* SpawnFor(UWorld* World, FName Name = TEXT("AcFlightShip"));

	/// The model `Model` of team `Team` placed at `Placement` (the renderer's
	/// placement: the ground point under the unit, turned to its heading),
	/// lifted `Lift` cm. False when the model is unknown.
	bool Show(FName Model, int32 Team, const FTransform& Placement, double Lift);
	/// Its size (1: the model's).
	void SetScale(double Scale);
	/// Builds the model's meshes now, hidden (a later `Show` does not build
	/// them in its frame).
	void Prepare(FName Model, int32 Team);
	/// Its lights' glow (the models' emission scale; 1 at rest).
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
