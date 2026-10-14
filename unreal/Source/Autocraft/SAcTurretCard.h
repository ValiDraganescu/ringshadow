// `SAcTurretCard`: a placed turret of the realm in the MMORPG mode (U, near
// a Sentinel or a Gatling Gun, or with one under the crosshair that close;
// D3, docs/mmorpg-m8.md). Drawn by hand in the HUD's chrome, the Citadel
// screen's small sibling:
//
//   GATLING GUN  ground and close air      HP ████████░ 280/300  KILLS 17  ESC
//   built by Pilot 045B · every pilot's
//   ┌ FIRE RATE ▲▲△ ─┐ ┌ DAMAGE △△△ ──┐ ┌ RANGE ▲△△ ───┐ ┌ AIR REACH ▲▲▲ ┐
//   │ NOW 4.8 /s      │ │ NOW 4.0       │ │ NOW 7.0 cells │ │ NOW 6.0 cells │
//   │ MARK 3 5.6 /s   │ │ MARK 1 5.0    │ │ MARK 2 8.0    │ │ LAST MARK     │
//   │ 800 ore · 500 MH│ │ 100 ore · 50  │ │ 300 · 200 MH  │ │               │
//   │ [  UPGRADE  ]   │ │ [  UPGRADE  ] │ │ [  UPGRADE  ] │ │ [   DONE   ]  │
//   └─────────────────┘ └───────────────┘ └───────────────┘ └───────────────┘
//   what was upgraded, or why not                                  the keys
//
// A card per stat the turret takes (air reach only the Gatling Gun's), its
// marks as chevrons (the ones lit on the turret's base), what it does now
// and with the next mark, the next mark's price from the pilot's own wallet.
// The card only shows and asks: an upgrade goes to its owner (`OnUpgrade`),
// which asks the server and hands the answer back through `SetInfo` (a
// refusal through `SetRefusal`, a kept one through `SetKept`).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "SAcCitadelScreen.h"

/// What the turret card shows.
struct FAcTurretCardInfo
{
	/// The server has told the pilot's progress (none yet: "Waiting…").
	bool bKnown = false;
	/// The turret's id in the region, its name ("GATLING GUN") and what it
	/// hits ("GROUND AND CLOSE AIR").
	int64 Id = 0;
	FString Name;
	FString Role;
	/// Its hp now and whole, the bugs it has killed, who built it ("Pilot
	/// 045B", "you"; empty: not known).
	double Hp = 0;
	double MaxHp = 1;
	int64 Kills = 0;
	FString Builder;
	/// A line per stat it takes (`Defence::takes`), in `DefenceStat`'s
	/// order; the price is the pilot's (`Defence::turretPrice`).
	TArray<FAcDefenceLine> Lines;
	/// Each line's stat (`ac::DefenceStat` as a number).
	TArray<int32> Stats;
	/// The pilot's wallet where it is.
	int64 Ore = 0;
	int64 Hydrogen = 0;
	/// Why nothing can be upgraded from here (empty: it can).
	FString WhyNot;

	bool operator==(const FAcTurretCardInfo&) const = default;
};

class AUTOCRAFT_API SAcTurretCard : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcTurretCard) {}
		/// Line `Index`'s stat takes its next mark.
		SLATE_EVENT(FAcOnUpgrade, OnUpgrade)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAcTurretCard() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcTurretCardInfo& Info);
	const FAcTurretCardInfo& GetInfo() const { return Info; }
	/// Why the last ask was refused (empty: none). Shown at the foot.
	void SetRefusal(const FString& Why);
	const FString& RefusalText() const { return Refusal; }
	/// The server kept the last ask: the foot says what was upgraded.
	void SetKept();
	const FString& KeptText() const { return Kept; }

	/// Why line `Index` can't take its next mark now (empty: it can), and
	/// its ask (a click; keys 1-4).
	FString UpgradeWhyNot(int32 Index) const;
	void Upgrade(int32 Index);
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
	struct FTarget
	{
		FBox2f Box;
		/// 0: close; 1: a line's UPGRADE (`Index`).
		int32 Row = 0;
		int32 Index = 0;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, int32 Target, const FString& Label, bool bLit,
		const FLinearColor& Accent) const;

	FAcTurretCardInfo Info;
	FString Refusal;
	FString Kept;
	/// What the last ask upgraded ("air reach, mark 2"; empty: none).
	FString Upgraded;

	FAcOnUpgrade OnUpgradeDelegate;
	FSimpleDelegate OnCloseDelegate;
	double OpenedAt = 0;
	TSharedPtr<FActiveTimerHandle> Timer;

	mutable TArray<FTarget> Targets;
	mutable TArray<FBox2f> Cards;
	mutable FBox2f FootBox;
	mutable float PanelW = 0;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
