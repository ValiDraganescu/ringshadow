// `SAcRigScreen`: the Prospector's rig in the MMORPG mode (J, or the skill
// screen's RIG tab; docs/prospector-progression.md, "Foundry and Lab").
// Drawn by hand in the HUD's chrome, the skill screen's sibling:
//
//   PROSPECTOR THE RIG   ORE 4,820 · MH 3,610    SKILLS | BLUEPRINTS | RIG   ESC
//   ┌ FITTING ───────────────┐ ┌ FOUNDRY  making… ──────┐ ┌ LAB  researching ┐
//   │ ▣ DRILL HEAD  MK II    │ │ MK I    MK II   MK III │ │ MK II    MK III  │
//   │   Drill +16% [I][II][III]│ │ price, time, MAKE     │ │ price, time      │
//   │ … a row a module         │ │ …                      │ │ …                │
//   └────────────────────────┘ └────────────────────────┘ └──────────────────┘
//   the picked cell: what it does, or why not                       the keys
//
// A row a module across the three panels. FITTING: the slot (its mark and
// what it does) and a chip a mark with the stores' count; a chip in stock
// fits it, the fitted one takes it off (only at the pilot's Citadel or a Hab
// Dome, where it unloads). FOUNDRY: a cell a mark with its price and time,
// the job's countdown. LAB: Mk II and Mk III, the same. A panel the pilot
// has no building for is greyed with the server's words.
//
// The screen only shows and asks: a make, a research or a fit goes to its
// owner (`OnMake`, …), which asks the server and hands the answer back
// through `SetInfo` (a refusal through `SetRefusal`). The game goes on
// behind; the pilot's unit stands while it is open (AcGameFlow).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "Progression.h"

#include <optional>

/// What the rig screen shows.
struct FAcRigScreenInfo
{
	/// The pilot's rig (none yet: "Waiting for the server…").
	std::optional<ac::Rig> Rig;
	/// The pilot owns a finished Foundry, a finished Lab.
	bool bFoundry = false;
	bool bLab = false;
	/// The wallet where the pilot is.
	int64 Ore = 0;
	int64 Hydrogen = 0;
	/// The world's clock now (`Sky::time`): the jobs' countdowns.
	double Now = 0;
	/// Why a module can't be fitted here (empty: it can: at home).
	FString FitWhyNot;

	bool operator==(const FAcRigScreenInfo&) const = default;
};

DECLARE_DELEGATE_TwoParams(FAcOnRigAsk, ac::Module, int64 /*Mark*/);
DECLARE_DELEGATE_OneParam(FAcOnRigTab, int32 /*0 skills, 1 blueprints*/);

class AUTOCRAFT_API SAcRigScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcRigScreen) {}
		SLATE_EVENT(FAcOnRigAsk, OnMake)
		SLATE_EVENT(FAcOnRigAsk, OnResearch)
		SLATE_EVENT(FAcOnRigAsk, OnFit)
		SLATE_EVENT(FAcOnRigTab, OnTab)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	/// A cell's column: the fitting's chips (Mk I..III), the Foundry's
	/// marks, the Lab's (Mk II, III).
	enum class EPanel : uint8 { Fitting, Foundry, Lab };
	struct FCell
	{
		ac::Module Module = ac::Module::drillHead;
		EPanel Panel = EPanel::Fitting;
		int64 Mark = 1;
		bool operator==(const FCell&) const = default;
	};

	void Construct(const FArguments& InArgs);
	virtual ~SAcRigScreen() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcRigScreenInfo& Info);
	const FAcRigScreenInfo& GetInfo() const { return Info; }
	/// Why the last ask was refused (empty: none). Shown at the foot.
	void SetRefusal(const FString& Why);
	const FString& RefusalText() const { return Refusal; }

	/// The cell picked (the first that can act at first).
	FCell Picked() const { return Pick; }
	void PickCell(const FCell& To);
	/// The arrow keys: `DX` across the cells of a row, `DY` down the modules.
	void MovePick(int32 DX, int32 DY);
	/// The picked cell's ask (Return, a click): fit or take off, make, research.
	void Act();
	void Close();
	/// The keys the flow hands it: true when used.
	bool Key(const FKey& InKey);

	/// Why the cell can't act (empty: it can).
	FString WhyNot(const FCell& Cell) const;

	/// The icons' atlas (`Resources/Textures/Skills/module_icons.png`, made by
	/// `Tools/hud/module_icons.py`): a module's brush, the Foundry's and the
	/// Lab's, or null when it did not load.
	static const FSlateBrush* Icon(ac::Module Module);
	static const FSlateBrush* FoundryIcon();
	static const FSlateBrush* LabIcon();
	/// A mark's colour: Mk I steel, Mk II cyan, Mk III gold.
	static FLinearColor MarkColor(int64 Mark);
	/// "4:05", "59 s".
	static FString Countdown(double Seconds);

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
		/// A cell's index (`CellIndex`), a tab.
		int32 Index = 0;
	};

	/// Cells in a row: 3 chips, 3 marks, 2 research.
	static constexpr int32 RowCells = 8;
	static FCell CellAt(int32 Row, int32 Column);
	static int32 ColumnOf(const FCell& Cell);
	static int32 CellIndex(const FCell& Cell);

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	void Press(const FTarget& T);

	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;

	void PaintHeader(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintPanel(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, EPanel Panel) const;
	void PaintFitting(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, ac::Module Module, const FBox2f& Row) const;
	void PaintChip(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FCell& Cell, const FBox2f& B) const;
	void PaintMark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FCell& Cell, const FBox2f& B) const;
	void PaintFoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FTarget& T, const FString& Label,
		bool bLit, const FLinearColor& Accent) const;

	FAcRigScreenInfo Info;
	FString Refusal;
	FCell Pick;
	bool bPickSet = false;

	FAcOnRigAsk OnMakeDelegate;
	FAcOnRigAsk OnResearchDelegate;
	FAcOnRigAsk OnFitDelegate;
	FAcOnRigTab OnTabDelegate;
	FSimpleDelegate OnCloseDelegate;

	/// When a cell last changed for the better (made, researched, fitted):
	/// its flash; and when the screen opened.
	TMap<int32, double> Flashes;
	double OpenedAt = 0;
	TSharedPtr<FActiveTimerHandle> Timer;

	mutable TArray<FTarget> Targets;
	mutable TMap<int32, FBox2f> Cells;
	mutable TArray<FBox2f> Rows;
	mutable FBox2f PanelBoxes[3];
	mutable FBox2f FootBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
