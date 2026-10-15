// `SAcWorldMap`: the MMORPG world map (M in the MMORPG mode; docs/mmorpg-m2.md
// R3, docs/worlds.md; M2 made it a satellite map, docs/mmorpg-m8.md). Drawn
// by hand over the whole screen:
//
//   WORLD MAP · You are in Prospect Landing.          (top left)
//   [the world from above: every region the pilot has been to as its baked
//    satellite picture (/Game/UI/Map, AcMapBake.h) at its place on the
//    world's grid, seamless; the others dark, their outline and name only.
//    Over them, live: the pilot, other pilots, mercenaries, the Brood in
//    groups, the realm's buildings, deposits (sized by the ore left), wells,
//    known blueprint sites, pads, starfalls and craters, the Journal's
//    missions, KSTR's places, the ringshadow band while it sweeps]
//   a tooltip by the pointer: what is under it (`ac::net::atlas::Item`)
//   the state line: "At pad landing: pick a pad to fly to." / …  [CLOSE] [FLY]
//
// The wheel zooms about the pointer, a drag pans; it opens centred on the
// pilot. A click on a pad picks it, FLY asks for the flight (`OnFly`; the
// owner says why not with `SetRefusal`); a click on a mission or a site
// makes it the compass's target (`OnTarget`). The keys: the arrows (and
// WASD) move the pad pick to the nearest pad that way, starting on the pad
// of the pilot's region; Return is FLY. Esc, M, Return and the arrows come
// from the flow's key processor (AcGameFlow). The game goes on behind (the
// server does not wait); the pilot's unit stands while it is open.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "NetProtocol.h"
#include "Progression.h"
#include "WorldAtlas.h"

#include <map>
#include <optional>
#include <vector>

struct FSlateBrush;

/// What the world map shows (from `FAcMmoClient`).
struct FAcWorldMapInfo
{
	/// The world map (none yet: "Waiting for the world…").
	std::optional<ac::net::WorldMessage> World;
	/// The regions the pilot has been to (ids).
	std::set<std::string> Seen;
	/// What to draw over the pictures, and the ringshadow band over each
	/// region it crosses now (world cells).
	std::vector<ac::net::atlas::Item> Items;
	std::vector<std::vector<ac::Vec2>> Bands;
	/// The compass's target set from the map (a mission's id or a site's
	/// key; empty: none): ringed.
	FString Target;
	/// The Landing Pad of the pilot's region nearest its unit (F2: a Landing
	/// Pad or the Spacedock the realm built), and how far (cells); unset:
	/// none in this region.
	FString NearPad;
	std::optional<double> NearDistance;
	/// Within this of its centre the Dropship is called (Pad link's reach;
	/// the Spacedock its radius + 2).
	double PadReach = 4.0;
	/// The near pad is the Spacedock (half the MH).
	bool bSpacedock = false;
	/// Ringwalker learned: flights cost no MH.
	bool bRingwalker = false;
	/// In flight to this region (empty: on the ground).
	FString FlyingTo;
	/// What a flight to each region costs in MH, by region id (F1,
	/// `ac::Flight`), and the wallet's MH (unset: not known yet).
	std::map<FString, int64> FlightPrice;
	std::optional<int64> Hydrogen;
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
};

DECLARE_DELEGATE_TwoParams(FAcOnWorldFly, const FString& /*RegionId*/, const FString& /*Pad*/);
/// A mission or a site clicked: the compass's target.
DECLARE_DELEGATE_OneParam(FAcOnMapTarget, const ac::net::atlas::Item& /*Item*/);

class AUTOCRAFT_API SAcWorldMap : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcWorldMap) {}
		SLATE_EVENT(FAcOnWorldFly, OnFly)
		SLATE_EVENT(FAcOnMapTarget, OnTarget)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/// What it shows now (each frame from the flow).
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
	/// (`DX`, `DY`: -1, 0 or 1; y down = south) on the world. With nothing
	/// picked the first press picks the pilot's region's pad. False: no pad
	/// that way (the pick stays).
	bool MovePick(int32 DX, int32 DY);
	/// Pick a destination by region id and pad name (scripts, tests). False:
	/// no such pad.
	bool PickPad(const FString& RegionId, const FString& Pad);
	/// Presses FLY and CLOSE as the buttons do.
	void Fly();
	void Close();

	/// Scripts and pictures: the view (centred on a world point, points a
	/// cell), the whole world, a region filling the screen; the item under
	/// the pointer as if hovered (the first `Matches` takes; false: none),
	/// and what its tooltip says (empty: none).
	void SetView(ac::Vec2 Centre, double Scale);
	void ShowWorld();
	bool ShowRegion(const FString& RegionId, double Fill = 1.0);
	bool HoverWhere(TFunctionRef<bool(const ac::net::atlas::Item&)> Matches);
	FString TooltipText() const;
	/// The item hovered (null: none), and what the map shows now.
	const ac::net::atlas::Item* Hovered() const;
	const FAcWorldMapInfo& GetInfo() const { return Info; }
	const ac::net::atlas::View& View() const { return MapView; }
	/// Region pictures drawn last paint (seen and baked), for checks.
	int32 PicturesShown() const { return Pictures; }

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100, 100); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	/// A button.
	struct FTarget
	{
		FBox2f Box;
		/// 10 fly, 11 close.
		int32 Row = 0;
		FString Label;
	};

	void Layout(FVector2f ViewSize) const;
	int32 ButtonAt(FVector2f LocalPoint) const;
	/// The item under a point of the widget (none: INDEX_NONE).
	int32 ItemAt(FVector2f LocalPoint) const;
	/// The item shown hovered: the scripted one, else the pointer's.
	int32 HoveredItem() const;
	/// The view once the size is known: centred on the pilot.
	void EnsureView(FVector2f ViewSize) const;
	/// The furthest out the wheel goes: the whole world and a margin.
	double Floor(FVector2f ViewSize) const;
	/// The state line and its colour.
	FString StateLine(FLinearColor& Color) const;
	bool AtPad() const;
	/// The pick is the pad the pilot stands on (no flight to make).
	bool PickIsHere() const;
	/// A flight to region `Region`'s MH price ("15 MH"; empty: unknown), and
	/// whether the wallet is short of it.
	FString PriceText(int32 Region) const;
	bool Short(int32 Region) const;
	/// The Landing Pad of the pilot's region it stands nearest (none: no
	/// Landing Pad there).
	FPick HerePad() const;
	/// A pad the Dropship flies to (F2): a Landing Pad (or the Spacedock) the
	/// realm built, in a region the pilot has been to. The region files' pads
	/// are shown, not picked.
	bool Pickable(int32 Region, int32 Pad) const;
	/// A pad's place on the world, cells.
	ac::Vec2 PadPoint(int32 Region, int32 Pad) const;
	/// A region's picture (null: not baked).
	const FSlateBrush* Picture(const std::string& RegionId) const;

	FAcWorldMapInfo Info;
	std::vector<ac::net::atlas::Place> Places;
	ac::net::atlas::Bounds WorldBounds;
	FString Refusal;
	FPick Pick;
	FAcOnWorldFly OnFlyDelegate;
	FAcOnMapTarget OnTargetDelegate;
	FSimpleDelegate OnCloseDelegate;

	mutable TArray<FTarget> Targets;
	mutable FVector2f Size = FVector2f::ZeroVector;
	mutable ac::net::atlas::View MapView;
	mutable bool bViewSet = false;
	mutable int32 Pictures = 0;
	/// Pending from a script before the size was known.
	mutable TOptional<FString> RegionToShow;
	mutable double RegionFill = 1.0;
	mutable bool bWorldToShow = false;

	/// The pointer (widget points), inside or not; a drag under way.
	FVector2f Pointer = FVector2f::ZeroVector;
	bool bPointerIn = false;
	bool bDown = false;
	bool bDragging = false;
	FVector2f DownAt = FVector2f::ZeroVector;
	/// A scripted hover: the item's kind and what names it.
	TOptional<ac::net::atlas::Item> Scripted;

	int32 HoveredButton = INDEX_NONE;
	int32 PressedButton = INDEX_NONE;
};
