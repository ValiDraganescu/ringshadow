// `SAcCitadelScreen`: the realm's Citadel in the MMORPG mode (H, at the
// realm's Citadel; docs/prospector-progression.md "The realm's tax", C1).
// Drawn by hand in the HUD's chrome, the rig screen's sibling:
//
//   THE REALM'S CITADEL  built by Pilot 045B        REALM                 ESC
//   ┌ REALM WALLET ───────────────┐ ┌ DONATE ─────────────────────────────┐
//   │ STARDUST ORE   1,240        │ │ YOUR WALLET  ORE 4,820 · MH 610     │
//   │ METALLIC HYDROGEN  56       │ │ ORE  [ 100 ] [ 1,000 ] [ ALL ]      │
//   │ TAX 10% of every unload …   │ │ MH   [ 100 ] [ 1,000 ] [ ALL ]      │
//   │ YOU GAVE  tax · donated     │ │                                     │
//   └─────────────────────────────┘ └─────────────────────────────────────┘
//   what was given, or why not                                  the keys
//
// Built with tabs in mind: REALM is the first; the Citadel's shop (C2) and
// its upgrades (D1) add theirs to `ETab`, each its own body under the same
// header and foot.
//
// The screen only shows and asks: a donation goes to its owner (`OnDonate`),
// which asks the server and hands the answer back through `SetInfo` (a
// refusal through `SetRefusal`, a kept one through `SetKept`). The game goes
// on behind; the pilot's unit stands while it is open (AcGameFlow).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/// What the Citadel screen shows.
struct FAcCitadelScreenInfo
{
	/// The server has told the realm's numbers (none yet: "Waiting…").
	bool bKnown = false;
	/// The realm wallet, and the tax in percent.
	int64 RealmOre = 0;
	int64 RealmHydrogen = 0;
	int64 TaxPercent = 0;
	/// What this pilot has given in all: in tax, and donated.
	int64 TaxedOre = 0;
	int64 TaxedHydrogen = 0;
	int64 DonatedOre = 0;
	int64 DonatedHydrogen = 0;
	/// The pilot's wallet where it is.
	int64 Ore = 0;
	int64 Hydrogen = 0;
	/// Who built the Citadel ("Pilot 045B", "you"; empty: not known).
	FString Builder;
	/// Why a donation can't go from here (empty: it can: at the Citadel).
	FString DonateWhyNot;

	bool operator==(const FAcCitadelScreenInfo&) const = default;
};

DECLARE_DELEGATE_TwoParams(FAcOnDonate, int64 /*Ore*/, int64 /*Hydrogen*/);

class AUTOCRAFT_API SAcCitadelScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcCitadelScreen) {}
		SLATE_EVENT(FAcOnDonate, OnDonate)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	/// The screen's tabs: the realm's wallet now; the shop (C2) and the
	/// upgrades (D1) come after it.
	enum class ETab : uint8 { Realm };

	/// The donate buttons: 100, 1,000 and all, of ore (0-2) and of MH (3-5).
	static constexpr int32 DonateButtons = 6;

	void Construct(const FArguments& InArgs);
	virtual ~SAcCitadelScreen() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcCitadelScreenInfo& Info);
	const FAcCitadelScreenInfo& GetInfo() const { return Info; }
	/// Why the last ask was refused (empty: none). Shown at the foot.
	void SetRefusal(const FString& Why);
	const FString& RefusalText() const { return Refusal; }
	/// The server kept the last ask: the foot says what was given.
	void SetKept();
	const FString& KeptText() const { return Kept; }

	ETab Tab() const { return CurrentTab; }

	/// What button `Index` gives now (ore, MH): "all" is the wallet's.
	TPair<int64, int64> Amount(int32 Index) const;
	/// Why button `Index` can't give (empty: it can).
	FString WhyNot(int32 Index) const;
	/// Button `Index`'s ask (a click, keys 1-6).
	void Donate(int32 Index);
	void Close();
	/// The keys the flow hands it: true when used.
	bool Key(const FKey& InKey);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100, 100); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	/// Something clickable.
	struct FTarget
	{
		FBox2f Box;
		/// What it is (ERow in the .cpp).
		int32 Row = 0;
		/// A donate button's index, a tab's.
		int32 Index = 0;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	void Press(const FTarget& T);
	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;

	void PaintHeader(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintRealm(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintFoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FTarget& T, const FString& Label,
		bool bLit, const FLinearColor& Accent) const;

	FAcCitadelScreenInfo Info;
	ETab CurrentTab = ETab::Realm;
	FString Refusal;
	FString Kept;
	/// What the last ask gave (ore, MH), for the foot once kept.
	TPair<int64, int64> Asked{0, 0};

	FAcOnDonate OnDonateDelegate;
	FSimpleDelegate OnCloseDelegate;
	double OpenedAt = 0;
	/// When the realm wallet last grew (its flash).
	double GrewAt = -10;
	TSharedPtr<FActiveTimerHandle> Timer;

	mutable TArray<FTarget> Targets;
	mutable FBox2f WalletBox;
	mutable FBox2f DonateBox;
	mutable FBox2f FootBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
