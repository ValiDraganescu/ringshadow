// The radial build wheel (C2, docs/guns-and-practice.md §5): B opens a ring
// of the buildings at the screen's centre, one slice a kind, the first at
// the top and the rest clockwise. The pointer stays hidden and captured:
// the mouse's motion since B moves a virtual stick, and past a dead zone the
// slice under the stick's direction is the pick, drawn lit with its name,
// its cost and its state. A click (or letting go of a B held over 0.25 s)
// confirms; Esc or B closes; the number keys still pick.
//
// `FAcBuildWheel` is the stick and the sectors (no engine, tested by
// Autocraft.Hud.BuildWheelPicksBySector); `Slices` reads what each kind's
// slice says; `UAcPilotBuildSubsystem` (AcPilotBuild.cpp) owns the open
// wheel and feeds it the mouse (`AAcPilotPawn::OnLookTaken`); the overlay
// (`SAcPilotOverlay::PaintWheel`) draws `FAcBuildWheelInfo`.
#pragma once

#include "CoreMinimal.h"

#include "Rules.h"

namespace ac { class Simulation; }
class FAcMmoClient;

/// One slice of the wheel.
struct FAcWheelSlice
{
	enum class EState : uint8
	{
		/// Picked, it goes to placement.
		Build,
		/// Not yet: it shows why (`State`) and does nothing.
		Locked,
		/// A kind the game does not have yet (shown, never picked).
		Soon,
	};
	/// Unset: a placeholder slice (`Soon`).
	TOptional<ac::StructureKind> Kind;
	/// Its number key (1…9), 0: none.
	int32 Number = 0;
	FString Title;
	/// The icon (`FAcIcons`).
	FString Icon;
	int32 Ore = 0;
	int32 Hydrogen = 0;
	EState State = EState::Build;
	/// Under the name: "×2 COPIES", "NO BLUEPRINT", "LV 8", "REALM LIMIT",
	/// "NEEDS 100 ORE"; empty: nothing to say.
	FString Tag;
	/// The refusal in full, for the note a locked pick shows.
	FString Why;
	bool operator==(const FAcWheelSlice&) const = default;
};

/// What the overlay draws while the wheel is open.
struct FAcBuildWheelInfo
{
	TArray<FAcWheelSlice> Slices;
	/// The virtual stick (points, x right, y up), for the hub's needle.
	FVector2D Stick = FVector2D::ZeroVector;
	/// The slice under the stick; INDEX_NONE inside the dead zone.
	int32 Pick = INDEX_NONE;
	bool operator==(const FAcBuildWheelInfo&) const = default;
};

struct AUTOCRAFT_API FAcBuildWheel
{
	/// The stick's dead zone and its reach (points: the mouse's raw counts).
	/// Held at the reach, a turn of the hand moves it round the ring at once.
	static constexpr double DeadZone = 40;
	static constexpr double Reach = 120;
	/// A B held longer than this confirms when it is let go; a tap leaves
	/// the wheel open.
	static constexpr double HoldToConfirm = 0.25;

	/// The slice of `N` (the first centred at the top, clockwise) that a
	/// stick at `Stick` (x right, y up) points into; INDEX_NONE inside the
	/// dead zone or with no slices.
	static int32 Sector(FVector2D Stick, int32 N);
	/// The middle of slice `I` of `N`, radians clockwise from the top.
	static double SliceAngle(int32 I, int32 N) { return N > 0 ? 2.0 * UE_DOUBLE_PI * I / N : 0.0; }

	/// The mouse moved (raw counts, y up): the stick follows, held inside
	/// its reach.
	void Move(FVector2D Delta);
	int32 Pick(int32 N) const { return Sector(Stick, N); }
	void Reset() { Stick = FVector2D::ZeroVector; }

	FVector2D Stick = FVector2D::ZeroVector;

	/// The wheel's slices for the driven pilot: the buildings of the build
	/// menu (`AcPilotText::BuildKinds`) and the Target Dummy, each with its
	/// price and what stands in its way.
	static TArray<FAcWheelSlice> Slices(const ac::Simulation& Sim, const FAcMmoClient* Mmo, bool bMmo);
};
