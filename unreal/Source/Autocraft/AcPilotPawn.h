// `AAcPilotPawn`: driving one unit by hand, in first person (GAME-LAYER.md
// §2.12, chunk E2). The port of `GameController+Pilot.swift` (`drive`,
// `takeOver`, `leavePilot`, `nextPilot`, `look`, `pilotAbilityPress`,
// `steerPilot`, `followPilot`, `castCrosshair`, `pilotFacing`,
// `pilotSounds`) and of `PilotCamera`'s camera, sky and skirt (the eye math
// is `FAcPilotCamera`, AcPilotCamera.h).
//
// The simulation owns the driven unit (`sim.pilot`, `ac::Pilot`): this pawn
// only feeds it the keys and the mouse before each step and puts the eye on
// the unit after it. No `AController` possesses units: the player
// controller possesses this pawn while driving and the RTS pawn otherwise
// (the RTS pawn stays in the world, its view kept for the way back).
//
// Kind-neutral (the user drives every kind): what a kind does is the core's
// (`pilotTarget`, `pilotAct`, `pilotAbility`); here a kind is only its eye
// (`FAcPilotCamera::Eye`/`Pivot`) and whether it `steers` (a vehicle with a
// turret looks with the mouse, one without stays on the hull).
//
// Take over: a click on a drivable unit or F under the pointer (D4's
// `UAcPointer::OnTakeOver`), `TakeOver(id)`. Leave: Esc or F (offered first
// to `OnKey`, for the build menu, E8), or when the unit is gone. Tab: the
// nearest other unit of the same kind.
//
// Input (`IMC_Pilot`, Tools/Editor/make_pilot_input.py; the mouse captured,
// the cursor hidden, `FInputModeGameOnly`):
//   IA_PilotMove  Axis2D  WASD and the arrows (x right, y back): walk, or
//                         drive and turn the hull for `steers` kinds
//   IA_PilotLook  Axis2D  the mouse: yaw 0.0032, pitch 0.0026 rad a point
//   IA_PilotAct   Bool    left button, E, Space (press and hold)
//   IA_PilotAbility Bool  R (press: the ability; held: a Prospector mends)
//   IA_PilotView  Bool    V (first/third person: E3)
//   IA_PilotNext  Bool    Tab
//   IA_PilotLeave Bool    Esc, F
//   IA_PilotKey   Axis1D  B (-1), Z (-2), X (-3), 1-9 (1-9): `OnKey` (E8, E9)
//   IA_PilotFree  Bool    Option held: the pointer out for the HUD while
//                         `Pointable()` says there is something to point at
//   IA_PilotZoom  Axis1D  the mouse wheel (up +): third person, the chase
//                         camera's zoom (`FAcPilotChase::FZoom`), kept in
//                         `UAcSettings::ChaseZoom`; nothing in first person
//
// Each frame, at the sim frame's `Hud` stage (after the renderer posed the
// frame): the eye onto the unit, the sky skirt, the hooks of the other
// chunks: `UAcWorldRenderer::SetHiddenUnit`/`SetDrivenUnit`,
// `AAcLifeBars::SetPilot`/`SetHiddenUnit`, `AAcFog::SetSuppressed`,
// `UAcEffects::SetPilot`, `UAcAudioDirector::SetEars`/`Blocked` and the
// in-ear sounds of `FAcAudioRules` (step, breath, lock-on, pull-away, heal),
// `UAcPointer::SetThirdPerson`, `UAcMinimapSubsystem::SetSight` (the sight
// wedge), `UAcLampPool::SetPilotLamp` (the headlamp on the crosshair). The crosshair is cast from the eye with
// E1's rays (`UAcRaysSubsystem::SightHit`, 60 cells, the driven unit
// skipped) before each step, as `steerPilot` does.
//
// Hooks for later chunks:
//   E3  `Chase` (AcPilotChase.h): V toggles `FAcPilotCamera::bThirdPerson`, kept
//       between drives; `-AcPilotThird` starts in it. `ChaseEye()`, `AimPoint()`.
//   E4  `GunMarker()`; held guns (renderer `SetPilotAim`), `AAcPilotAids` (range
//       ring, fog veil), renderer `SetVeiled` (AcPilotAim.h, AcPilotAids.h).
//   E4  `Crosshair()`; the fog veil.
//   E5  `CockpitMount()` (a child of the camera: x ahead, z up), `Camera()`,
//       `Eye().SetLean`, `Eye().Turn/Speed`, `OnTookOver`/`OnLeft`.
//   E6  `OnNote`, `Driven()`, `Sight()`; E8/E9 `OnKey`, `Pointable`, `bPlacing`.
//   C2  `OnLookTaken` (the build wheel's stick); the act key offers
//       "confirm" to `OnKey` first (the wheel's pick).
//
// Scripted driving (headless; no desktop input):
//   -AcPilot=KIND        take over the player's first KIND once the game runs
//                        (a fresh one by its Citadel if it has none)
//   -AcPilotStage        stage it as `windowshot --pilot KIND` does: a
//                        Prospector at its main's nearest ore field facing it,
//                        any other kind fresh on the Citadel's open side
//                        facing out, a Ranger ahead in reach (a gun on foot
//                        aims at its chest); the game is then paused
//   -AcPilotPath=STEPS   a scripted drive, `key:seconds` joined by commas
//                        (keys joined by +: w a s d, act, right left (yaw 0.6 rad/s),
//                        up down (pitch), ability, next, view, leave, wait,
//                        aim: the view on the nearest enemy's chest within
//                        25 cells, every frame it is held, e.g. aim+act:2);
//                        default "w:1.5,w+right:1.5,d:1,act:1"
//   -AcPilotVariant=V    with -AcPilotStage: Swift's `--pilot KIND-V` (fire,
//                        far, miss, minigun[+fire], anchored, anchoring,
//                        close, jump, heal, cargo; a Prospector's citadel,
//                        enemy, wide); "fire"/"miss"/"heal" step until the
//                        round leaves, then hold (AcPilotStage.cpp);
//                        joined by +, any staging: "shield" (the Aegis
//                        Shield), "flyer" (an enemy Kestrel off to the
//                        left past the target; `aim` holds a flyer at its
//                        hover)
//   -AcPilotDiveAt=S     in a recording (-AcShotRecord): take over S seconds
//                        into it, so the clip shows the top-down view and the
//                        dive into the unit
//   -AcPilotDive=S       the dive's length (ac.PilotDive)
//   -AcPilotAt=enemy     a fresh KIND above the nearest enemy base instead:
//                        on the walkable approach 20-26 cells out whose
//                        ground falls the most toward its Citadel over the
//                        first 6 cells (a slope down to it), facing it
//   -AcPilotAt=slope     on the top of the player's own ramp (its top
//                        nearest the player's Citadel), facing down it
//                        (with -AcPilotVariant=anchored: a Longbow anchored;
//                        with -AcPilotDiveAt: put down as the game starts,
//                        so the top-down view has it before the dive)
//   -AcPilotAt=ridge     on high ground over a base (the player's first), above
//                        it toward an edge, the gas giant behind the base
//                        as seen from there, facing the Citadel
//                        (-AcPilotMiners=N: N more Prospectors mining below)
//   -AcPilotAt=X,Y       a fresh KIND at that ground point (cells), facing
//                        -AcPilotYaw (radians, default east)
//   -AcPilotPullOut=S[,L] in a recording: S seconds after the take over the
//                        camera leaves the eye and rises behind the unit,
//                        turned to the gas giant, over L seconds (5); the
//                        recording's UI (-AcShotUI) fades out over the first
//                        0.5 s; the camera ejects fast (eased out), and once
//                        it is out of the body the unit is drawn and the
//                        cockpit and the pilot's HUD are off (`PulledOut()`).
//                        The drive goes on underneath.
//   -AcPilotFor=S        hold an -AcShot until S game seconds after the take
//                        over (the path runs meanwhile)
//   -AcPilotYaw=RAD -AcPilotPitch=RAD   the view at the start
//   -AcPilotFoeHp=N      the staged enemy ahead starts with N hp, so the
//                        driven unit wins the duel (a clip that goes on)
//   ac.Pilot KIND|ID|off, ac.PilotLook DX DY (console)
//
// The dive: a take-over from the top-down view flies the camera from where
// the RTS camera is into the unit's eye (`ac.PilotDive` seconds, 1.2; 0 cuts
// as before), the place eased (smootherstep), the view turned onto the unit
// and levelled into the eye's at the end, the field of view with it. The
// unit stays drawn and the cockpit hidden until the camera is 2.5 cells from
// the eye or 80% of the way (`InDive()`). Stills (-AcShot without -AcShotRecord, a
// staged -AcPilotStage) cut.
//
// The eject: leaving blends the view from the eye, where it was, up to the
// top-down view (`ac.PilotEject` seconds, 0.9, eased; 0 cuts), the unit
// drawn again. A recording's pull-out fades the HUD out over its first
// `PullOutFade` seconds and drops the cockpit at its end.
#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"

#include "AcActPick.h"
#include "AcPilotCamera.h"
#include "AcPilotChase.h"
#include "AcDemolish.h"
#include "Types.h"

#include <optional>

#include "AcPilotPawn.generated.h"

namespace ac { class Simulation; }

class UInputAction;
class UInputMappingContext;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UAcSimSubsystem;
struct FAcFrame;

/// The pilot's camera: the Swift near plane (0.04 cells) for this view only.
UCLASS()
class AUTOCRAFT_API UAcPilotCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FAcPilotEvent, int64 /*UnitId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FAcPilotNote, const FString& /*Text*/);

UCLASS()
class AUTOCRAFT_API AAcPilotPawn : public APawn
{
	GENERATED_BODY()

public:
	AAcPilotPawn();

	static AAcPilotPawn* SpawnFor(UWorld* World);
	static AAcPilotPawn* Find(const UObject* WorldContext);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;

	/// True while a unit is driven.
	bool Driving() const { return DrivenId != INDEX_NONE; }
	int64 Driven() const { return DrivenId; }
	/// A staged still (-AcPilotStage) is being set up or held: E6 marks the
	/// driven unit's hit at the shot, as Swift's stage (`markPilotHit`).
	bool Staged() const { return Stage != EStage::None; }
	/// Take over the player's unit `Id` (`takeOver`); false if the sim refuses.
	bool TakeOver(int64 Id);
	/// The player's drivable unit nearest a ground point (`drive(near:)`).
	bool DriveNear(ac::Vec2 Ground);
	/// Back to the top-down camera over where the unit stands (`leavePilot`).
	/// In the MMORPG test it hands the unit back to the server, unless
	/// `bHandBack` is false (the server's doing: it took the unit, or it
	/// is gone).
	void Leave(bool bHandBack = true);
	/// Tab: the nearest other unit of the same kind (`nextPilot`).
	void Next();
	/// The mouse turned the view by (DX right, DY down) points (`look`).
	void Look(double DX, double DY);
	/// The look's gain on the mouse's raw counts over a frame of `Dt`
	/// seconds: points for `Look`, 0.0032 rad each across (the top-down
	/// view's right-drag turn uses it too).
	static double LookGain(FVector2D Raw, float Dt);
	static constexpr double YawPerPoint = 0.0032, PitchPerPoint = 0.0026;
	/// The act key went down or up (`pilotPress`). `bKeyE`: it was E. In the
	/// MMORPG mode E works only what the crosshair is on in reach (C5,
	/// AcActPick.h: `CurrentAct`), the demolish hold included, and nothing
	/// else; the click and Space keep the core's acts (fire, drill, weld,
	/// repair, enter). At a gun every key fires.
	void Press(bool bDown, bool bKeyE = false);
	/// C5: what E does now (unset: nothing), and the act's box under the
	/// crosshair, worked out each frame from the crosshair's ray.
	const TOptional<AcActPick::FAct>& CurrentAct() const { return ActNow; }
	const TOptional<AcActPick::FTarget>& CurrentActBox() const { return ActBox; }
	/// Scripted tests: turn the look so the crosshair's ray (from the
	/// camera's eye as last drawn) runs through `Target` (world cm).
	void TestLookAt(const FVector& Target);
	/// The same at the middle of an act's box (the picker's own: `Kind`,
	/// its thing `Id` (0: any), `Index` (INDEX_NONE: any)); false: no such
	/// box near the driven unit.
	bool TestLookAtAct(AcActPick::EKind Kind, int64 Id = 0, int32 Index = INDEX_NONE);
	/// C3: the building E would demolish now (INDEX_NONE: none), and the
	/// hold's fill while E is held on it (unset: not held).
	int64 DemolishTarget() const;
	TOptional<double> DemolishFill() const;
	/// R (`pilotAbilityPress`).
	void AbilityPress();
	/// V: first or third person, kept for the next ride (needs E3).
	void ToggleView();
	/// A screen over the view (the MMORPG world map): the pointer is free and
	/// the unit stands until it closes.
	void SetScreenOpen(bool bOpen);
	bool ScreenOpen() const { return bScreenOpen; }
	/// Scripted tests (-AcFlowTest mmo steps): the move keys (x right, y
	/// down the screen: (0, -1) is forward) and the mouse's yaw, as the input
	/// actions set them.
	void SetTestInput(FVector2D Move, double Yaw)
	{
		MoveInput = Move;
		PilotYaw = Yaw;
	}

	/// C1: the corner gun the driven unit mans (0 to 3), else INDEX_NONE.
	/// At a gun the view is always first person from the gun's eye, the
	/// look held in its arc; V does nothing; Esc or F disengages.
	int32 AtGun() const { return GunManned; }
	/// Esc or F at a gun: leave it (the server's Man -1); false when not at one.
	bool Disengage();
	/// The gun's Citadel while at one (0: none).
	int64 GunCitadel() const { return GunCitadelId; }
	/// The look's pitch (radians; at a gun, its aim's).
	double ViewPitch() const { return Cam.Pitch; }
	/// Third person: the chase camera's zoom wanted (`FAcPilotChase::FZoom`,
	/// 1 the old boom). `SetChaseZoom` clamps it; `bSnap`: no easing (pictures).
	float ChaseZoom() const { return Chase.Zoom.Target; }
	void SetChaseZoom(float Zoom, bool bSnap = false);
	/// The wheel turned by `Notches` (+ in): third person only.
	void ZoomNotch(float Notches);

	/// The camera is still flying down into the unit (the dive): the unit
	/// drawn, no cockpit.
	bool InDive() const { return (Dive.IsSet() && !Dive->bInside) || PulledOut(); }
	/// -AcPilotPullOut: the camera is out of the unit, on its way to the gas giant.
	bool PulledOut() const { return PullOut.IsSet() && PullOut->bOutside; }
	/// The eye (pitch, lean, third person) and the camera.
	FAcPilotCamera& Eye() { return Cam; }
	UCameraComponent* Camera() const { return CameraComponent; }
	/// Where E5 hangs a cockpit (follows the view: x ahead, z up).
	USceneComponent* CockpitMount() const { return Mount; }
	/// What the crosshair is on this frame: the point (world cm) and the
	/// unit or building (INDEX_NONE: ground, scenery, or the sky 60 cells out).
	struct FCrosshair
	{
		FVector Point = FVector::ZeroVector;
		int64 Target = INDEX_NONE;
		/// The ray cast (C5's acts use it): its start past the driven
		/// unit and its far end (zero: a staged crosshair, no ray).
		FVector From = FVector::ZeroVector, To = FVector::ZeroVector;
	};
	const TOptional<FCrosshair>& Crosshair() const { return Cross; }
	/// The yaw the mouse gives (world radians, as `heading`).
	double Yaw() const { return PilotYaw; }

	FAcPilotEvent OnTookOver;
	FAcPilotEvent OnLeft;
	/// A short note for the overlay (E6): "Anchored: R for tank mode"...
	FAcPilotNote OnNote;
	/// Keys offered first (E8 build menu, E9 picks): "esc", "b", "z", "x",
	/// "1"…"9"; true = used.
	TFunction<bool(const FString& Key)> OnKey;
	/// The mouse's motion offered first (raw counts, +y up; C2's build
	/// wheel takes it while open); true = taken, the view does not turn.
	TFunction<bool(FVector2D Raw)> OnLookTaken;
	/// Option frees the pointer only when something wants it (E9's pick cards).
	TFunction<bool()> Pointable;
	/// A building is held out to place (E8): the act key places, not acts.
	bool bPlacing = false;
	/// E3 installed the chase camera: V switches views.
	bool bChaseCamera = true;

	/// E4: where a vehicle's gun really points, off the view's centre in
	/// half the view's height (+x right, +y up; `gunMarker`), for the
	/// overlay (E6). Unset: not a vehicle, or behind the camera.
	const TOptional<FVector2D>& GunMarker() const { return GunMark; }
	/// The right-drag look-around (third person): the crosshair, where the
	/// aim is, off the view's centre as `GunMarker` (unset: at the centre).
	/// `bReticleHidden`: the aim is off the view, no crosshair.
	const TOptional<FVector2D>& ReticleMarker() const { return ReticleMark; }
	bool ReticleHidden() const { return bReticleHidden; }
	/// The camera swung off the aim by the right button (or easing back).
	bool LookingAround() const { return Cam.bThirdPerson && Chase.Orbit.Off(); }
	/// E3: third person, the unit's own eye (world cm; its shots start
	/// there: `chaseEye`). Unset in first person.
	TOptional<FVector> ChaseEye() const { return Cam.bThirdPerson ? Chase.Eye() : TOptional<FVector>(); }
	/// E3: third person, what the middle of the view is on (`aimPoint`).
	TOptional<FVector> AimPoint() const;

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;

private:
	void OnFrame(const FAcFrame& Frame);
	void OnGameStarted(UAcSimSubsystem& Sim);
	void BindPointer();
	/// The keys and the mouse into `sim.pilot`, before the step (`steerPilot`).
	void Steer();
	/// After the step: the eye on the unit, the hooks (`followPilot`).
	void Follow(const FAcFrame& Frame);
	/// The MMORPG test mode: dive in once the server gives the unit asked
	/// for (`FAcMmoClient::Wanted`), out when it no longer drives it.
	void SyncMmo();
	/// The model root's world (UE cm): the renderer's placement × part 0.
	FTransform RootOf(const ac::Unit& U) const;
	/// What the middle of the view is on. `bPredict`: from the view as the
	/// mouse has turned it since the last `Follow` (`PredictView`), not as
	/// last followed.
	void CastCrosshair(bool bPredict = true);
	/// The last followed eye (`LastEye`, `LastRotation`) turned by the mouse
	/// since: the yaw about the unit (when the view follows the mouse) and
	/// the pitch about the view's own right.
	void PredictView(FVector& OutEye, FQuat& OutRotation) const;
	void ApplyView();
	void Hooks(bool bOn);
	void Sounds(const ac::Unit& U);
	void SetCapture(bool bCapture);
	void ShowSkirt(bool bShow);
	void MakeSkirt();
	void AddContext(bool bPilot);

	// Input.
	void OnMove(const FInputActionValue& V);
	void OnMoveEnd(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnActStart(const FInputActionValue& V);
	void OnActEnd(const FInputActionValue& V);
	void OnAbilityStart(const FInputActionValue& V);
	void OnAbilityEnd(const FInputActionValue& V);
	void OnView(const FInputActionValue& V);
	void OnNext(const FInputActionValue& V);
	void OnLeave(const FInputActionValue& V);
	void OnKeyAction(const FInputActionValue& V);
	void OnFreeStart(const FInputActionValue& V);
	void OnFreeEnd(const FInputActionValue& V);
	void OnZoom(const FInputActionValue& V);
	/// The zoom into `UAcSettings` once the ride ends, if the wheel moved it.
	void StoreZoom();
	bool bZoomDirty = false;

	// The scripted drive.
	void Script(double Dt);
	bool StageScripted();
	// The staged stills (AcPilotStage.cpp, `GameController+PilotStage.swift`).
	bool StageFor(ac::UnitKind Kind);
	bool StageProspector(ac::Simulation& S, ac::Vec2 Home);
	bool StageUnit(ac::Simulation& S, ac::UnitKind Kind, ac::Vec2 Home);
	/// The staged still after the set-up: settle, then the act ("fire",
	/// "miss", "heal") step by step until the round leaves; true once done.
	bool StageTick(UAcSimSubsystem& Sim);
	/// Swift's stage draws its last frame 2 s on (`world.sync(time: clock + 2)`),
	/// so a staged flyer is up off its pad: forget the renderer's `BornAt`.
	void StageLifted();
	void AimAt(const FVector& Target);
	/// -AcPilotPath's `aim` key: `ScriptAimTarget` is the nearest enemy.
	void ScriptAimNearest();
	/// The dive's start: the RTS camera as it was drawn (before the possess).
	void StartDive(const APlayerController* PC);
	/// The dive's camera this frame, from the eye (`At`, `Rot`) it ends on.
	void DiveView(FVector& At, FQuat& Rot);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAcPilotCameraComponent> CameraComponent;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Mount;
	/// A wide plain just under the map's lowest edge (`makeSkirt`).
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Skirt;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkirtMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> PilotContext;
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RtsContext;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UInputAction>> Actions;

	FAcPilotCamera Cam;
	/// E3: the third-person camera.
	FAcPilotChase Chase;
	TOptional<FVector2D> GunMark;
	TOptional<FVector2D> ReticleMark;
	bool bReticleHidden = false;
	int64 DrivenId = INDEX_NONE;
	double PilotYaw = 0.0;
	/// C1: `AtGun`, `GunCitadel` as the last `Follow` saw them.
	int32 GunManned = INDEX_NONE;
	int64 GunCitadelId = 0;
	/// C1: the view from gun `GunManned`'s eye (`Defence::gunEye`).
	bool GunView(const ac::Simulation& S, FVector& OutLocation, FQuat& OutRotation) const;
	bool bHold = false;
	/// C3: E held on a building to demolish it.
	AcDemolish::FHold DemolishHold;
	/// C5: E's act and its box this frame (`UpdateAct`, after the crosshair).
	TOptional<AcActPick::FAct> ActNow;
	TOptional<AcActPick::FTarget> ActBox;
	void UpdateAct(const ac::Simulation& S, const ac::Unit& U, double Feet);
	/// What the picker needs of the world (the ground, the ships drawn).
	AcActPick::FWorld ActWorld(double Feet) const;
	bool bAbilityHeld = false;
	bool bThird = false;
	/// The console whose view switch is bound to `ToggleView`.
	TWeakPtr<class SAcConsole> ViewSwitchBound;
	bool bLeaveWanted = false;
	bool bCaptured = false;
	bool bPointing = false;
	/// `SetScreenOpen`.
	bool bScreenOpen = false;
	/// The MMORPG world: the view held for a unit in travel since (`Clock`;
	/// < 0: not held), at most this long.
	double HoldSince = -1;
	static constexpr double MaxHoldSeconds = 40.0;
	FVector2D MoveInput = FVector2D::ZeroVector;
	/// Third person on foot: the heading's turn off the camera's yaw, eased
	/// (`Steer`), and its real seconds (< 0: none).
	double FacingOffset = 0;
	double FacingAt = -1;
	TOptional<FCrosshair> Cross;
	TOptional<FVector> LastEye;
	FQuat LastRotation = FQuat::Identity;
	/// What the last `Follow` turned the view by: the mouse's yaw and pitch,
	/// and the unit's root (the eye turns about it).
	double LastViewYaw = 0.0, LastViewPitch = 0.0;
	FVector LastPivot = FVector::ZeroVector;
	/// The view turns with the mouse's yaw (third person, a turret, or a
	/// unit that faces where the mouse says), not with a hull.
	bool bViewOnMouse = false;
	/// The driven unit took the heading it was sent on the last sim step:
	/// it faces the mouse, so the renderer poses it at the mouse's heading
	/// every frame (`UAcWorldRenderer::SetDrivenHeading`).
	bool bFollowsMouse = false;
	struct FDive
	{
		FVector From = FVector::ZeroVector;
		FQuat FromRot = FQuat::Identity;
		float FromFov = 90.f;
		double Start = 0;
		double Seconds = 1;
		/// Near the eye: the unit hidden, the cockpit on.
		bool bInside = false;
	};
	TOptional<FDive> Dive;
	/// -AcPilotPullOut: when (seconds after the take over) and how long; the
	/// camera's way out once it starts.
	double ScriptPullOutAt = -1;
	double ScriptPullOutFor = 5;
	struct FPullOut
	{
		double Start = 0;
		double Seconds = 5;
		/// Away from the eye: the unit drawn, the cockpit and HUD off.
		bool bOutside = false;
	};
	TOptional<FPullOut> PullOut;
	/// The HUD's fade at the pull-out's start, seconds; the cockpit goes once
	/// the camera is out of the body, at the latest at its end.
	static constexpr double PullOutFade = 0.5;
	void PullOutView(FVector& At, FQuat& Rot, const FVector& Body);
	/// -AcPilotAt: a fresh unit of the kind at a staged place.
	bool StageAt(ac::UnitKind Kind, const FString& Where);
	/// Puts that unit down (not taken over yet); its id, or INDEX_NONE.
	int64 PlaceAt(ac::UnitKind Kind, const FString& Where);
	/// The -AcPilotAt unit, once put down (before the dive, so the top-down
	/// view shows it).
	int64 PlacedId = INDEX_NONE;
	ac::Vec2 PlacedAt;
	double PlacedHeading = 0;

	// `pilotSounds`' memory.
	TOptional<int64> LastStep;
	TOptional<int64> LastLocked;
	double LastBreath = -10.0;
	bool bWasMoving = false;
	double Clock = 0.0;

	const void* SkirtMap = nullptr;
	FDelegateHandle FrameHandle, StartedHandle;
	bool bPointerBound = false;

	// The scripted drive.
	struct FStep
	{
		FString Key;
		double Seconds = 0;
	};
	TArray<FStep> Path;
	FString ScriptKind;
	bool bScriptStage = false;
	bool bScriptStarted = false;
	bool bShotHeld = false;
	double ScriptFor = -1.0;
	/// -AcPilotDiveAt: seconds into the recording to take over (-1: at once).
	double ScriptDiveAt = -1.0;
	/// -AcPilotStage: the still's phases (`StageTick`).
	enum class EStage : uint8 { None, Settle, Act, Done };
	EStage Stage = EStage::None;
	/// -AcPilotVariant: Swift's `--pilot KIND-VARIANT` ("fire", "far",
	/// "miss", "minigun+fire", "anchored", "jump", "cargo"...; a
	/// Prospector's "citadel", "enemy", "wide").
	FString StageVariant;
	bool bStageActs = false;
	bool bStageHeals = false;
	/// The driven unit's round left (a Shot or Missed event) in the act.
	bool bStageFired = false;
	/// The units the stage put down round the driven one (target, cargo).
	TArray<int64> StagedIds;
	/// "citadel": the load goes aboard after the cockpit has posed once
	/// empty, as Swift's `takeOver` seeds the fork shut before the load.
	int32 StageLoad = 0;
	/// "jump": taken over at the cliff's edge, put half way across once the
	/// view has stood there (`StageTick`); the crosshair stays the one cast
	/// from the edge, as Swift's stage steers from `takeOver`'s camera.
	TOptional<TPair<ac::Vec2, ac::Vec2>> StageJump;
	ac::Vec2 StageJumpAt;
	TOptional<FCrosshair> StageCross;
	int32 StageFrames = 0;
	int32 StageActSteps = 0;
	int32 StageAimSteps = 0;
	double StageDoneAt = -1.0;
	double ScriptStart = 0.0;
	double ScriptLogAt = 0.0;
	int32 ScriptFrames = 0;
	int32 PathIndex = 0;
	double PathAt = 0.0;
	FString PathKeys;
	TOptional<int64> ScriptAimTarget;
	int32 ScriptAimRounds = 0;
};
