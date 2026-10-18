// What the cockpit says (chunk E6): the port of `PilotText`, `PilotInfo`,
// `SightInfo`, `HitMark` (Sources/Autocraft/PilotOverlay.swift:7-86,
// GameController+Pilot.swift:8-64) and of the controller's `pilotInfo`
// (:409), `sightInfo` (:377), `sightRefusal` (:391), `transit` (:403),
// `pilotHelp` (:515), `pilotHint` (:525), `pilotTips` (:532), and of
// `pilotBuildInfo`'s prompt (GameController+Command.swift:102) and
// `pilotConsole`/`soldierCard`/`status` (:486-580): the dashboard's card for
// the driven unit.
//
// Plain C++ over `ac::Simulation` (no widgets, no world): what the overlay
// (`SAcPilotOverlay`) draws and what the console (`SAcConsole`, cockpit and
// cab styles) shows are made here, from the sim and a few things only the
// game layer knows (`FAcPilotInputs`: the note, the hit mark, the build
// menu, the view). Kind-neutral: every kind fills the same struct.
#pragma once

#include "CoreMinimal.h"

#include "AcCab.h"
#include "AcConsoleInfo.h"
#include "Pilot.h"
#include "Simulation.h"

/// `PilotText`: what the cockpit says for each kind.
struct AUTOCRAFT_API FAcPilotText
{
	struct FAct
	{
		FString Title;
		FString Icon;
		FString Detail;
	};
	/// The act button on the dashboard (unset: the Prospector, whose card is
	/// its tools and the build menu).
	TOptional<FAct> Act;
	/// The prompt with something to act on under the sight.
	FString Aimed;
	/// What click (and R) do, and how the keys move it.
	FString Help;
	FString Move;
	/// The move line while the machine is held in place (anchored, or on its
	/// way in or out); unset: `Move`.
	TOptional<FString> Held;

	static FAcPilotText Of(ac::UnitKind Kind);
};

/// The enemy on a driven gun's line of fire (or a Dropship's patient).
struct FAcSightInfo
{
	FString Title;
	double Hp = 0;
	double MaxHp = 1;
	/// Edge to edge, and the reach at it.
	double Distance = 0;
	double Range = 0;
	double MinRange = 0;
	bool bInRange = false;
	/// One of its own side: green, not red, while in reach.
	bool bFriend = false;
};

/// One hit landed: `Serial` counts up, so each is marked once.
struct FAcHitMark
{
	int32 Serial = 0;
	double Damage = 0;
	bool bKill = false;
};

/// The compass's goal (the Citadel with a load, else the nearest ore).
struct FAcPilotGoal
{
	FString Label;
	ac::Vec2 At{0, 0};
	double Distance = 0;
	/// K2: a Citadel mission's place (its tier's colour), not KSTR's.
	FLinearColor Color = FLinearColor::White;
	bool bMission = false;
};

/// One line of the build menu (`PilotInfo.menu`).
struct FAcPilotMenuLine
{
	FString Line;
	bool bOk = true;
	bool operator==(const FAcPilotMenuLine&) const = default;
};

/// C1 (docs/citadel-interior.md §7): the live feed of the corner gun the
/// pilot mans, over the view (`SAcPilotOverlay`).
struct FAcTurretFeed
{
	/// The gun, 0 to 3 ("TURRET 1 · SE · CITADEL").
	int32 Gun = 0;
	FString Title;
	/// The aim from the arc's middle and the arc's half (radians; + is
	/// toward the stop on the right).
	double Off = 0;
	double ArcHalf = 1;
	/// The aim's pitch and its limits (radians).
	double Pitch = 0;
	double PitchMin = -0.35;
	double PitchMax = 0.87;
	/// The barrels' heat: pips lit of `HeatPips` (its shots of the last
	/// `HeatSeconds` against what it fires flat out in them).
	static constexpr int32 HeatPips = 6;
	static constexpr double HeatSeconds = 1.5;
	int32 Heat = 0;
	/// 0 just after a shot … 1 ready to fire again (the cooldown).
	double Ready = 1;
	/// The gun's marks ("RATE 2 · DAMAGE 1 · RANGE 0"; empty: none).
	FString Marks;
	bool operator==(const FAcTurretFeed&) const = default;
};

/// `PilotInfo`: what the cockpit shows for the driven unit this frame.
struct FAcPilotInfo
{
	FString Name;
	double Hp = 0;
	double MaxHp = 1;
	/// A load carried (a Prospector's ore or MH).
	int64 Cargo = 0;
	bool bHydrogen = false;
	/// What the action key does, or why it cannot.
	TOptional<FString> Prompt;
	EAcTone Tone = EAcTone::Idle;
	/// 0…1 through the work in hand (drilling, inside a Derrick, a jump...).
	TOptional<double> Progress;
	FString ProgressLabel;
	/// Inside a building: the view is dimmed.
	TOptional<FString> Inside;
	/// Facing, world radians (the compass).
	double Heading = 0;
	TOptional<FAcPilotGoal> Goal;
	/// The MMORPG mode: places KSTR contracts send the pilot to in this
	/// region (`AcProgressHud::ContractGoals`), green on the compass; and
	/// its Citadel missions' (`AcMissions::Goals`), in their tier's colour.
	TArray<FAcPilotGoal> Marks;
	ac::Vec2 Position{0, 0};
	FString Help;
	/// The build menu, when open (E8 fills it; the Swift controller never
	/// sets it: its build menu is the dashboard's card).
	TOptional<TArray<FAcPilotMenuLine>> Menu;
	/// The dashboard: the unit's status, cargo and command card.
	TOptional<FAcCardInfo> Console;
	/// Seen from a cab (first person in a Prospector, Firefly, Longbow, Hailstorm).
	bool bCab = false;
	EAcCabLamps Lamps = EAcCabLamps::Idle;
	bool bThirdPerson = false;
	/// A message flashed over the reticle.
	TOptional<FString> Note;
	TOptional<FAcSightInfo> Sight;
	TOptional<FAcHitMark> Hit;
	/// A vehicle's gun marker: off the view's centre in half the view's
	/// height (+y up). Unset: none (E4 fills it).
	TOptional<FVector2D> Gun;
	/// A soldier's spread cone (`Simulation::cone`) in half the view's
	/// height: the reticle's brackets open by it. Unset: none.
	TOptional<double> Cone;
	/// C1: at a corner gun, its live feed. Unset: not at one.
	TOptional<FAcTurretFeed> Feed;
};

/// What `GameCore` says about the driven unit this frame (`PilotFrame`).
struct AUTOCRAFT_API FAcPilotFrame
{
	ac::Unit Unit;
	std::optional<ac::PilotTarget> Target;
	std::optional<ac::PilotSight> Sight;
	std::optional<ac::PilotAbility> Ability;
	/// The unit it heals this step (the healer attacking it).
	std::optional<ac::Unit> Patient;

	/// The driven unit's frame; unset when nothing is driven.
	static TOptional<FAcPilotFrame> Of(const ac::Simulation& Sim);
};

/// What only the game layer knows, for `Info`.
struct FAcPilotInputs
{
	TOptional<FString> Note;
	TOptional<FAcHitMark> Hit;
	bool bThirdPerson = false;
	/// E8: the build menu is open / a building is held out (and where it
	/// would go: `ghostSpot`, unset when there is no spot).
	bool bBuildMenu = false;
	TOptional<ac::StructureKind> Placing;
	TOptional<ac::Vec2> PlacingSpot;
	/// The mode's own refusal of that spot, after the core's (the MMORPG
	/// test mode: on a pad's plate, `FAcMmoClient::PadPlateRefusal`).
	TOptional<FString> PlacingRefusal;
	/// The MMORPG test mode: tips for what is there, the build menu's list
	/// (`AcPilotText::BuildKinds`).
	bool bMmo = false;
	/// The MMORPG mode's client: the blueprints the pilot has, its level
	/// (`AcProgressHud::BlueprintRefusal`).
	const class FAcMmoClient* Mmo = nullptr;
};

struct AUTOCRAFT_API AcPilotText
{
	/// `Cockpit.cab`: seen from a cab in first person (the console frames the view).
	static bool HasCab(ac::UnitKind Kind);
	/// `Simulation.title(kind)` as an FString.
	static FString Title(ac::UnitKind Kind);
	static FString Title(ac::StructureKind Kind);
	/// `String(format: "%g")`: 5 → "5", 6.5 → "6.5".
	static FString FormatG(double V);

	/// `pilotHelp`: the help lines, bottom first (drawn upward), joined by "\n".
	static FString Help(const ac::Unit& U);
	/// `pilotHint`: the first of the help line.
	static FString Hint(const ac::Unit& U);
	/// `pilotTips`: the dashboard's tips, taking turns.
	static TArray<FString> Tips(const ac::Unit& U);
	/// The MMORPG test mode's tips: only what can be done there and then
	/// (a Derrick's line with a Derrick of the pilot's, the unload line with
	/// a Citadel, the pick keys with a pick waiting; no music keys).
	static TArray<FString> MmoTips(const ac::Simulation& Sim, const ac::Unit& U);
	/// U2: the key card (the Settings page's KEY CARD): the MMORPG
	/// Prospector's keys as its TIP line names them, every one at once, by
	/// group; each a key and what it does. Read only.
	struct FKeyRow
	{
		FString Key;
		FString What;
	};
	struct FKeyGroup
	{
		FString Title;
		TArray<FKeyRow> Rows;
	};
	static TArray<FKeyGroup> KeyCard();
	/// The build menu's buildings, in order (keys 1, 2, …): the MMORPG test
	/// mode's list there, else `PilotBuild.kinds`.
	static TArray<ac::StructureKind> BuildKinds(bool bMmo);

	/// `sightInfo`: the sight's target for the readout.
	static TOptional<FAcSightInfo> Sight(const ac::Simulation& Sim, const std::optional<ac::PilotSight>& S);
	/// `sightRefusal`: why the act key does nothing to what the sight is on.
	static TOptional<FString> Refusal(const ac::Unit& U, const FAcSightInfo& S);

	/// `pilotInfo` (the gun marker aside: E4's).
	static FAcPilotInfo Info(const ac::Simulation& Sim, const FAcPilotFrame& F, const FAcPilotInputs& In);
	/// `pilotConsole`: the driven unit's card on the dashboard.
	static FAcCardInfo Console(const ac::Simulation& Sim, const FAcPilotFrame& F, const FAcPilotInfo& P,
		const FAcPilotInputs& In);
	/// `LevelInfo`: the kind's level and XP.
	static FAcLevelInfo Level(const ac::Simulation& Sim, ac::UnitKind Kind);
	/// `status(_:)`: the dashboard's status line.
	static FString Status(const FAcPilotFrame& F);
	/// C1: the feed of gun `Gun` of `Citadel`, its look at `Yaw` and `Pitch`
	/// (radians), the game times of its shots `Shots` (any order), now `Now`.
	static FAcTurretFeed Feed(const ac::Structure& Citadel, int32 Gun, double Yaw, double Pitch, TConstArrayView<double> Shots,
		double Now);
};
