// The walk-in Dropship drawn (C4, docs/haul-and-ferry.md section 4): one
// actor per ship of the shown state (`AcShuttleView`), the `ferry_blue`
// model posed every frame (`AcPoseFerry`: the ramp, the buttons and their
// lamps, the wipers, the engines), its cabin lights, the rain on its
// windshield (`M_AcWindshield`, the visor's water in the glass's frame,
// driven by the ship's own `ac::Wipers`), and its sounds. The game flow's
// tick calls `Tick` once a frame after the world renderer (whose sync
// worked out where every ship is).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Types.h"
#include "Wipers.h"

#include "AcShuttleShip.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UPostProcessComponent;
class UStaticMeshComponent;
struct FAcModelInfo;

namespace AcShuttleShip
{
	/// The windshield's numbers (cm, the model's: Tools/Editor/models/ferry.py):
	/// the bottom line's middle, the glass's height up its slope and its tilt
	/// back from upright, the centre pane's half width, where the side panes
	/// start across and how far back their outer edges stand, the wipers'
	/// pivots across and their reach.
	inline constexpr double GlassBottomX = 320.0, GlassBottomZ = 148.0, GlassTopX = 262.0, GlassTopZ = 248.0;
	inline constexpr double PaneHalf = 56.0, SideFrom = 62.0, SideBack = 40.0, SideOut = 116.0;
	inline constexpr double WiperLeftU = -26.0, WiperRightU = 28.0, WiperPivotV = 3.0, WiperReach = 54.0;
	/// The windshield's rain is drawn for the ship nearest the camera within this (cm).
	inline constexpr double GlassRange = 2500.0;

	/// The engines' thrust (0 idle .. 1 full) for a ship at `Stage` `Clock`
	/// seconds in (of `Seconds`).
	AUTOCRAFT_API double Thrust(uint8 Stage, double Clock, double Seconds);

	/// Every ship drawn, made and unmade with the shown state's (game thread).
	AUTOCRAFT_API void Tick(UWorld* World);
	/// A Dropship is over the Landing Pad at `At` (coming down, standing,
	/// lifting): the pad's lamps alternate (`PoseLandingPad`).
	AUTOCRAFT_API bool PadBusy(const FVector& At);
	/// The wipers of the ship the driven unit is in (its bay, or riding it),
	/// for the dashboard's WIPERS plate; null: none.
	AUTOCRAFT_API const ac::Wipers* AboardWipers(UWorld* World);
	/// The last press of a ship's ramp or take-off button (the pawn's act,
	/// AcShuttleAct), for the button's press: the real time it was pressed.
	AUTOCRAFT_API void Pressed(int64 Ship, bool bTakeOff, double At);
}

UCLASS()
class AUTOCRAFT_API AAcShuttleShip : public AActor
{
	GENERATED_BODY()

public:
	AAcShuttleShip();

	int64 ShipId = 0;

	/// Builds the model's meshes and lights (once).
	bool Build(int32 Team);
	/// Poses it for this frame: where it is (`Placement`), its look.
	void Apply(const FTransform& Placement, const struct FAcFerryLook& Look);
	/// The windshield's frame in the world (for the rain): its bottom line's
	/// middle, across (the ship's +Y), up the glass, and the two side panes'
	/// bottom edges (outward).
	void Glass(FVector& Origin, FVector& Across, FVector& Up, FVector& Right, FVector& Left) const;

	/// Its flight's effects this frame: the lift jets, the lift-off's blast
	/// and scorch, the touch down's dust, the dust under it near the ground,
	/// the cloud streaks in the dash, the pad's lamps.
	void Effects(const ac::Shuttle& S, const FTransform& Placement, double Thrust, double Now, double Dt);

	/// The ship's own wipers (presentation, like the visor's).
	ac::Wipers Wiper;
	/// The stage last frame (-1: none yet), the last dust ring and cloud puff (real s).
	int32 LastStage = -1;
	double LastRing = -1, LastCloud = -1;
	/// The real times of the last presses.
	double RampPressAt = -1e9, TakeoffPressAt = -1e9;
	/// The ramp's way last frame (for the hydraulics' sound), and the wiper's passes heard.
	int32 LastRampWay = 0;
	int64 PassesHeard = 0;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> Rain;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RainMaterial;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	/// One per (part, mesh), with its part's index.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
	TArray<int32> MeshPart;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;
	TArray<int32> LightPart;
	const FAcModelInfo* Model = nullptr;
};
