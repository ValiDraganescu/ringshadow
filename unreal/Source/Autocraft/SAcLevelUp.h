// The leveling UI's widgets (chunk E9; see AcLeveling.h):
//   - `SAcLevelBanner`: `HUD.applyLevelUp`, "PROSPECTOR  LEVEL 4" on a plate
//     in the upper middle of the view (pops in at ×1.15 over 0.18 s, holds
//     2.4 s, fades 0.6 s; a new one replaces it);
//   - `SAcPickCards`: `PickCards.node` over the dashboard's centre, flat (not
//     on the warped faces), placed as `Console.showPicks` places it;
//   - `AcLevelPaint::Pilots`: `CommandMap.pilots`, the PILOTS rows.
// All laid out in Swift points, y up from the view's bottom, over the whole
// view (the HUD root's layer).
#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

#include "AcBakedArt.h"
#include "AcCab.h"
#include "AcLeveling.h"
#include "HudLanes.h"

class UTexture2D;
struct FAcCommandMapColumn;
struct FSlateBrush;

/// Plates baked by `AcCabArt::BakePlate`, one per size, kept: a banner
/// sized to its words bakes a plate the first time a size is asked for, and
/// a bake while playing costs the render thread a GPU frame on Metal when it
/// is read back (FAcBakedArt), so a size seen before is drawn as it was (K3).
struct FAcLevelPlate
{
	TMap<FIntPoint, FAcBakedArt> Arts;

	const FAcBakedArt& Get(FVector2D Size, double TL, double TR, double BR, double BL, int32 Seed);
};

class AUTOCRAFT_API SAcLevelBanner : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcLevelBanner) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/// `levelUp(kind, level, hold)`: `Now` real seconds; `bHold` keeps it up,
	/// unanimated. `bKeys` (the MMORPG test mode): the line under it says
	/// the keys that pick, on a darker plate, held a little longer.
	void Show(ac::UnitKind Kind, int32 Level, double Now, bool bHold, bool bKeys = false);
	/// Bakes the plate these lines would be shown on, now (K3: a region's
	/// arrival banner, baked as the region is made ahead, so the crossing
	/// into it draws a plate already baked).
	void WarmPlate(const FString& InTitle, const FString& InSub, const FString& InGate) const;
	/// The plate's size for these lines (points).
	static FVector2D PlateSize(const FString& InTitle, const FString& InSub, const FString& InGate, bool bInKeyed);
	/// The MMORPG mode's banner slot (U1, docs/mmorpg-play.md "The HUD"): a
	/// level reached (docs/prospector-progression.md, "The level-up"), a
	/// raid's warning, the Matriarch down. `InTitle` in `InAccent`, `InSub`
	/// ("+1 SKILL POINT · K") and `InGate` ("TIER 2 OPEN") under it in cyan.
	/// One at a time at the top of the view; the next waits its turn
	/// (`ac::BannerQueue`).
	void ShowLines(const FString& InTitle, const FString& InSub, const FString& InGate, double Now, bool bHold = false,
		const FLinearColor& InAccent = FLinearColor(0, 0, 0, 0));
	/// The slot's top edge (points from the view's top: under the compass,
	/// or under the boss bar), and whether a screen covers the view (the
	/// banner waits, its clock stopped).
	void Place(float InTop, bool bInHeld);
	bool IsShowing(double Now) const;
	/// The banner in the slot now ("": none), and how many wait.
	FString SlotTitle() const;
	int32 SlotWaiting() const { return int32(Queue.waiting.size()); }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	FString Title;
	FString Sub;
	FString Gate;
	double Since = -1e9;
	bool bHeld = false;
	bool bOn = false;
	bool bKeyed = false;
	/// The MMORPG mode's slot: its queue, where it stands, held under a screen.
	ac::BannerQueue Queue;
	bool bQueued = false;
	bool bQueueHeld = false;
	float SlotTop = 142;
	mutable FAcLevelPlate Plate;
	TSharedPtr<FActiveTimerHandle> Timer;

	EActiveTimerReturnType Animate(double CurrentTime, float DeltaTime);
};

DECLARE_DELEGATE_OneParam(FAcOnPickCard, ac::Perk);

class AUTOCRAFT_API SAcPickCards : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAcPickCards) {}
		SLATE_EVENT(FAcOnPickCard, OnPick)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	/// `showPicks`: the offer (unset: none) over the dashboard of `Layout`,
	/// framed in `Accent`; `bKeys`: the keys that pick (driving). `bSide`
	/// (the MMORPG test mode): the cards stacked at the view's left edge over
	/// the dashboard, the middle of the view (the unit, what it works on)
	/// left clear; the header names the level picked for and the levels
	/// waiting ("PICK FOR LEVEL 2", "3 WAITING · LV 2-4").
	void Show(const TOptional<FAcPickOffer>& Offer, const FAcCabLayout& Layout, const FLinearColor& Accent, bool bKeys, bool bSide = false);
	bool Side() const { return bSide; }
	const TOptional<FAcPickOffer>& Offer() const { return Shown; }
	/// The panel (Swift points, y up), zero with none.
	FAcRect Panel() const { return PanelRect; }
	/// The card under a point (Swift points, y up), as last laid out.
	TOptional<ac::Perk> CardAt(FVector2D P) const;
	/// Card `Slot`'s rectangle (Swift points, y up).
	TOptional<FAcRect> CardRect(int32 Slot) const;
	/// A press and release at a view point (points, y down) through the mouse path.
	bool Click(FVector2D ViewPoint);

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
	{
		return OnMouseButtonDown(Geometry, Event);
	}

private:
	void Relayout();

	FAcOnPickCard OnPick;
	TOptional<FAcPickOffer> Shown;
	FAcCabLayout Cab;
	FLinearColor Accent = FLinearColor::White;
	bool bKeys = true;
	bool bSide = false;
	FAcRect PanelRect;
	double CardH = 66;
	TArray<TArray<FString>> Lines;
	TArray<TPair<FAcRect, ac::Perk>> Frames;
	TOptional<ac::Perk> Pressed;
	mutable FAcLevelPlate Plate;
};

namespace AcLevelPaint
{
	/// `PerkReach.mark(s, color)` centred on `At` (Swift points, y up).
	void Mark(FSlateWindowElementList& Out, const FGeometry& Geometry, int32& Layer, double ViewHeight, EAcPerkReach Reach,
		FVector2D At, double S, const FLinearColor& Color);
	/// `CommandMap.pilots`: the PILOTS rows from `Col.Y` down; adds a pick
	/// hit per button and records them in `OutButtons`.
	void Pilots(FAcCommandMapColumn& Col, const TArray<FAcDrivenRow>& Rows, TArray<TPair<FAcRect, ac::Perk>>& OutButtons);
}
