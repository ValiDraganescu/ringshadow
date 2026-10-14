// `SAcWorldMap`: the MMORPG world map (M in the MMORPG test mode, in place
// of the command map; docs/mmorpg-m2.md R3, docs/worlds.md). Drawn by hand
// in the HUD's chrome, as `SAcNewGame`:
//
//   WORLD MAP                                  "You are in Prospect Landing."
//   [the world's grid, rows north to south: each region a tile with its
//    name, its style and size, its pads (a dot where the pad stands, its
//    name a button); the pilot's region ringed cyan, YOU ARE HERE; in
//    flight the destination ringed amber]
//   the state: "At pad landing: pick a pad to fly to." / "Not at a pad:
//   landing is 23 cells away. Drive onto a pad to fly." / "Flying to …" /
//   the last refusal
//   [CLOSE] [FLY]
//
// A click on a pad picks it, FLY asks for the flight (`OnFly`; the owner
// says why not with `SetRefusal`). The keys pick too: the arrows (and WASD)
// move the pick to the nearest pad that way, starting on the pad of the
// pilot's region; Return is FLY. One pick, ringed, whether clicked or keyed.
// Esc, M, Return and the arrows come from the flow's key processor
// (AcGameFlow). The game goes on behind (the server does not
// wait); the pilot's unit stands while it is open.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "NetProtocol.h"
#include "Progression.h"

#include <optional>

/// What the world map shows (from `FAcMmoClient`).
struct FAcWorldMapInfo
{
	/// The world map (none yet: "Waiting for the world…").
	std::optional<ac::net::WorldMessage> World;
	/// The pad of the pilot's region nearest its unit, and how far (cells).
	FString NearPad;
	std::optional<double> NearDistance;
	/// Within this of a pad's centre a flight can leave (the near pad's:
	/// Pad link's reach, a pad of the pilot's own its building's radius + 2).
	double PadReach = 4.0;
	/// The near pad is the pilot's own (a Spacedock, a Homing beacon).
	bool bOwnPad = false;
	/// Ringwalker learned: seconds until a flight from anywhere (0: now).
	std::optional<double> RingwalkerIn;
	/// In flight to this region (empty: on the ground).
	FString FlyingTo;
	/// The blueprint sites the pilot knows of (`AcProgressHud::KnownSites`):
	/// the region's id, where in it (0…1 across, 0…1 down), opened or not.
	struct FSite
	{
		FString Region;
		FVector2f At = FVector2f::ZeroVector;
		ac::SiteKind Kind = ac::SiteKind::crate;
		bool bFound = false;
		bool operator==(const FSite&) const = default;
	};
	TArray<FSite> Sites;
	/// KSTR contracts that point somewhere (open reach and drill ones,
	/// `AcProgressHud::ContractMarks`): the region's id, where in it (0…1;
	/// unset: the region as a whole), the contract's title.
	struct FMark
	{
		FString Region;
		TOptional<FVector2f> At;
		FString Title;
		bool operator==(const FMark&) const = default;
	};
	TArray<FMark> Marks;
};

DECLARE_DELEGATE_TwoParams(FAcOnWorldFly, const FString& /*RegionId*/, const FString& /*Pad*/);

class AUTOCRAFT_API SAcWorldMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcWorldMap) {}
		SLATE_EVENT(FAcOnWorldFly, OnFly)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcWorldMapInfo& Info);
	/// Why the flight was not asked for, or the server's refusal (empty: none).
	void SetRefusal(const FString& Why);
	/// The refusal shown (empty: none).
	const FString& RefusalText() const { return Refusal; }

	/// The pad picked (none: nothing picked yet).
	struct FPick
	{
		int32 Region = INDEX_NONE;
		int32 Pad = INDEX_NONE;
	};
	const FPick& Picked() const { return Pick; }
	/// The picked pad's region id and name (empty: none), for tests and logs.
	FString PickedRegion() const;
	FString PickedPad() const;
	/// The arrow keys: move the pick to the nearest pad in that direction
	/// (`DX`, `DY`: -1, 0 or 1; y down = south) on the world's grid. With
	/// nothing picked the first press picks the pilot's region's pad. False:
	/// no pad that way (the pick stays).
	bool MovePick(int32 DX, int32 DY);
	/// Pick a destination by region id and pad name (scripts, tests). False:
	/// no such pad.
	bool PickPad(const FString& RegionId, const FString& Pad);
	/// Presses FLY and CLOSE as the buttons do.
	void Fly();
	void Close();

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100, 100); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	/// Something clickable: a pad or a button.
	struct FTarget
	{
		FBox2f Box;
		/// 1 a pad, 10 fly, 11 close.
		int32 Row = 0;
		int32 Region = INDEX_NONE;
		int32 Pad = INDEX_NONE;
		FString Label;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	/// The state line and its colour.
	FString StateLine(FLinearColor& Color) const;
	bool AtPad() const;
	/// The pick is the pad the pilot stands on (no flight to make).
	bool PickIsHere() const;
	/// The pad of the pilot's region it stands nearest (else its first).
	FPick HerePad() const;
	/// A pad's place on the world's grid, in tiles (x east, y south).
	FVector2f GridPoint(int32 Region, int32 Pad) const;

	FAcWorldMapInfo Info;
	FString Refusal;
	FPick Pick;
	FAcOnWorldFly OnFlyDelegate;
	FSimpleDelegate OnCloseDelegate;

	mutable TArray<FTarget> Targets;
	/// Each region's tile (by index in the world's regions), panel points.
	mutable TArray<FBox2f> Tiles;
	mutable FBox2f GridBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
