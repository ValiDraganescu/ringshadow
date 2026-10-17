// The kind table: one look record per unit kind (docs/kind-registry.md,
// "Game layer: one look record per kind"). Everything the game layer draws
// or plays for a kind (its model, cockpit, icon, sounds, muzzles, heights,
// view, minimap mark, death, bursts, outline) is a field of its row here, in
// `ac::allCases<ac::UnitKind>()` order; the switches that held it read the
// row. The names derive from the core raw name (`prospector` → model
// `prospector_blue`, cockpit `cockpit_prospector_blue`, icon `prospector`,
// sounds `prospectordeath`, `vprospector`, `prospectormove`); a row
// overrides what differs. `Autocraft.Kinds.Complete` walks every kind and
// names the field that is missing.
//
// Adding a kind: one row in `AcKindTable.cpp`, a pose registered with
// `FAcPoseRegistration`, the assets the test asks for.
#pragma once

#include "CoreMinimal.h"

#include "Types.h"

namespace AcDeaths
{
	enum class EFall : uint8;
}

/// A sound a kind's weapon plays: its gain and how many may start a second.
struct FAcKindSound
{
	FName Name;
	float Gain = 0.f;
	int32 PerSecond = 0;
	/// It carries twice the sight (`AcSound::Carry`'s far set).
	bool bFar = false;
};

/// Whose cockpit a kind drives with.
enum class EAcKindCockpit : uint8
{
	/// Its own (`cockpit_<base>_blue`); with a `StandIn`, the stand-in's
	/// while the catalog lacks it.
	Own,
	/// Always the `StandIn`'s.
	Borrowed,
	/// Never driven (the Brood).
	None,
};

/// What the cockpit state holds besides the cockpit model
/// (`AcCockpitKinds::Make`).
enum class EAcKindCockpitRig : uint8
{
	Plain,
	/// The Prospector's fork arm.
	Fork,
	/// Two held weapons, `HeldNode`/`HeldMuzzle` with `%d` for 0 and 1.
	Held,
	/// The unit's own model posed by its pose on the cockpit's `model_*` parts.
	UnitModel,
};

/// How a kind dies on screen.
enum class EAcKindDeath : uint8
{
	/// Blown apart (`AcEffectsShatter`, the `Shatter` spec).
	Shatter,
	/// The body falls (`AcDeaths`, the `Fall`).
	BodyFall,
	/// Its own pose plays the death (the Brood, AcPoseBrood.cpp).
	Pose,
};

/// The particles a unit's model streams every frame (`AcEffectsBursts`).
enum class EAcKindBurst : uint8
{
	None,
	/// Thruster plumes from `emitter_1/2`, by `BurstSpeed`/`BurstRate`.
	Thrusters,
	/// The Comet's jets.
	CometJets,
	/// The Juggernaut's muzzle smoke.
	MuzzleSmoke,
	/// The Firefly's flame, embers, smoke and exhaust.
	FireflyFire,
};

enum class EAcKindMinimap : uint8
{
	Disc,
	Diamond,
};

/// A machine's wreck (`AcEffectsShatter::FSpec` without its model).
struct FAcKindShatter
{
	int32 Chunks = 10;
	double Size = 1;
	int32 Ring = 8;
	/// Parts left out of the wreck (a prefix ends in `_`).
	TArray<FString> Strip;
	bool bFlyer = false;
	/// The Atlas: it kneels and topples before it bursts.
	bool bTopple = false;
};

struct FAcKindLook
{
	ac::UnitKind Kind = ac::UnitKind::prospector;

	// Names.
	/// `rawValue(kind)`.
	FString Base;
	/// `<base>_blue`.
	FString Model;
	/// `cockpit_<base>_blue`.
	FString CockpitModel;
	/// `<base>` (`/Game/UI/Icons/T_Icon_<icon>`); empty: no icon (the Brood).
	FString Icon;

	// Sounds.
	/// `<base>death`.
	FName DeathSound;
	/// The report-in line `v<base>`; none for the Brood.
	FName Voice;
	/// A driven unit's engine as it pulls away (`<base>move`); none on foot.
	FName MoveSound;
	float MoveGain = 0.f;
	/// The shot (`FAcAudioRules::WeaponOf`), and the alternate weapon's
	/// (an anchored Longbow's, a Kestrel's at a flier); none: the shot's.
	FAcKindSound Weapon;
	FAcKindSound AltWeapon;
	/// The Brood's strike, hurt cry and idle chitter (none: its idle loops).
	FName Strike, Hurt, Idle;

	// Shots.
	/// `Trooper.muzzles` as exported part names, in order.
	TArray<FName> Muzzles;
	/// The muzzles with the side's upgrade (the Ranger's Mini gun).
	TArray<FName> UpgradedMuzzles;
	/// It shoots from no part (the Prospector's weld, the Brood's bites):
	/// an empty `Muzzles` on purpose.
	bool bNoMuzzle = false;
	/// The muzzles are where the part's meshes end, not its pivot.
	bool bMuzzleReach = false;

	// Heights, cells.
	/// A flier's height over the ground (`Models.hover`); 0 on the ground.
	double Hover = 0;
	/// Where rounds meet it over its feet (`GameScene.chestHeight`).
	double ChestHeight = 0.62;

	// The driven view (`FAcPilotCamera`), SceneKit axes, cells.
	FVector Eye = FVector(0, 1, 0);
	/// What the eye turns about with the turret.
	FVector Pivot = FVector::ZeroVector;
	double StartPitch = -0.22;
	/// The third-person boom's lift (cells) and length (× the default).
	float BoomLift = 0.f, BoomLength = 1.f;
	/// The cockpit has a cab over the view (`AcPilotText::HasCab`).
	bool bCab = false;

	// The cockpit.
	EAcKindCockpit Cockpit = EAcKindCockpit::Own;
	TOptional<ac::UnitKind> StandIn;
	EAcKindCockpitRig Rig = EAcKindCockpitRig::Plain;
	FString HeldNode, HeldMuzzle;
	FVector HeldAxis = FVector::ZeroVector;

	// The minimap mark: radius (points at scale 1), shape, SpriteKit z.
	double MinimapRadius = 1.8;
	EAcKindMinimap MinimapShape = EAcKindMinimap::Disc;
	uint8 MinimapZ = 1;

	// The death.
	EAcKindDeath Death = EAcKindDeath::Shatter;
	FAcKindShatter Shatter;
	TOptional<AcDeaths::EFall> Fall;

	// The bursts its model streams.
	EAcKindBurst Burst = EAcKindBurst::None;
	/// Thrusters: plume speed and birth rate, moving and at rest.
	double BurstSpeed[2] = {0, 0}, BurstRate[2] = {0, 0};

	/// Outlined while buried (`AcOutline::StencilFor`): the Scorpion.
	bool bOutlineBuried = false;
};

namespace AcKinds
{
	/// The row of `Kind`.
	AUTOCRAFT_API const FAcKindLook& Look(ac::UnitKind Kind);

	/// A Brood kind: not the player's, not in the palette.
	AUTOCRAFT_API bool IsBrood(ac::UnitKind Kind);

	/// The kind whose raw name is `Name` (`ac::parse`).
	AUTOCRAFT_API TOptional<ac::UnitKind> Named(const FString& Name);

	/// The kind whose model's base is `Base` (`FAcModelInfo::Base`).
	AUTOCRAFT_API TOptional<ac::UnitKind> OfBase(FName Base);

	/// The kind whose cockpit, eye and cab `Kind` drives with: its own, or
	/// its stand-in's while the catalog has no cockpit of its own.
	AUTOCRAFT_API ac::UnitKind CockpitKind(ac::UnitKind Kind);

	/// The catalog holds the cockpit `Kind` drives with when it is its own.
	AUTOCRAFT_API bool HasCockpit(ac::UnitKind Kind);
}
