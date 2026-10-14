// `SAcSkillScreen`: the Prospector's skill screen in the MMORPG mode (K;
// docs/prospector-progression.md, "On screen"). Drawn by hand in the HUD's
// chrome, as `SAcWorldMap`:
//
//   PROSPECTOR LEVEL 34 [xp bar]  [3 POINTS]   SKILLS | BLUEPRINTS   RESPEC 460 MH
//   ┌ gates ┬ DEEP VEIN 14 ┬ QUICKSILVER 0 ┬ … ┬ HARDSHELL 2 ┐ ┌ the picked node ┐
//   │ TIER 1│ ▣ ▣ ▣        │ ▣ ▣ ▣         │   │             │ │ each rank, the  │
//   │ …     │ …            │               │   │             │ │ next, why not,  │
//   │ CAP   │     ▣        │     ▣         │   │             │ │ LEARN, UNDO     │
//   └───────┴──────────────┴───────────────┴───┴─────────────┘ └─────────────────┘
//   [Q Overcharge] [C opens at level 25] [G opens at level 45]          the keys
//
// SKILLS: five branch columns, five tier rows (the capstone last), a tile a
// node (its icon, name and rank pips: grey locked, outlined open, lit in the
// branch's colour learned). The picked node's ranks in the right panel.
// Click or Return learns a rank, the arrows move the pick, Backspace takes
// back a point spent in the last 60 s, Q/C/G puts a learned active in that
// slot (or drag it there), Tab switches to BLUEPRINTS: every building, found
// or not, its floor and where it came from.
//
// The screen only shows and asks: a learn, an undo, a slot or a respec goes
// to its owner (`OnSpend`, …), which asks the server and hands the answer
// back through `SetInfo` (a refusal through `SetRefusal`). The game goes on
// behind; the pilot's unit stands while it is open (AcGameFlow).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

#include "Progression.h"
#include "Rules.h"

#include <array>
#include <map>
#include <optional>

/// What the skill screen shows.
struct FAcSkillScreenInfo
{
	/// The pilot's progress (none yet: "Waiting for the server…").
	std::optional<ac::ProspectorProgress> Progress;
	/// Seconds left to take back the last point in a node free.
	std::map<ac::Skill, double> UndoLeft;
	/// MH the next respec costs, and whether it is free.
	int64 RespecPrice = 0;
	bool bRespecFree = true;
	/// Why a respec can't be made here (empty: it can: at the pilot's Citadel).
	FString RespecWhyNot;
	/// Where a found blueprint came from ("Landing crate, Prospect Landing").
	std::map<ac::StructureKind, FString> Sources;
	/// Sites the pilot knows of that hold a blueprint not found yet.
	std::map<ac::StructureKind, int32> SitesMarked;
	/// Levels the pilot's floors are cut by (Blueprint savant).
	int64 FloorCut = 0;

	bool operator==(const FAcSkillScreenInfo&) const = default;
};

DECLARE_DELEGATE_OneParam(FAcOnSkill, ac::Skill);
DECLARE_DELEGATE_TwoParams(FAcOnSkillSlot, int32 /*Slot 0..2*/, std::optional<ac::Skill>);

class AUTOCRAFT_API SAcSkillScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcSkillScreen) {}
		SLATE_EVENT(FAcOnSkill, OnSpend)
		SLATE_EVENT(FAcOnSkill, OnUndo)
		SLATE_EVENT(FAcOnSkillSlot, OnSlot)
		SLATE_EVENT(FSimpleDelegate, OnRespec)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	enum class ETab : uint8 { Skills, Blueprints };

	void Construct(const FArguments& InArgs);
	virtual ~SAcSkillScreen() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcSkillScreenInfo& Info);
	const FAcSkillScreenInfo& GetInfo() const { return Info; }
	/// Why the last ask was refused (empty: none). Shown under the picked node.
	void SetRefusal(const FString& Why);
	const FString& RefusalText() const { return Refusal; }

	ETab Tab() const { return CurrentTab; }
	void SetTab(ETab To);
	void NextTab();
	/// The node picked (the first open one at first).
	ac::Skill Picked() const { return Pick; }
	void PickSkill(ac::Skill To);
	/// The arrow keys: `DX` across (nodes, then branches), `DY` down the tiers.
	void MovePick(int32 DX, int32 DY);
	/// Learns a rank of the picked node (Return, a click), takes the last one
	/// back (Backspace), puts it in slot `Slot` (Q, C, G: 0..2).
	void Learn();
	void Undo();
	void SlotPicked(int32 Slot);
	void Respec();
	void Close();
	/// The keys the flow hands it: true when used.
	bool Key(const FKey& InKey);

	/// Why the picked node can't take a point (empty: it can).
	FString WhyNot(ac::Skill Skill) const;

	/// The icons' atlas (`Resources/Textures/Skills/skill_icons.png`, made by
	/// `Tools/hud/skill_icons.py`): a node's brush, or null when it did not load.
	static const FSlateBrush* Icon(ac::Skill Skill);
	/// The branch's colour on the screen and the HUD.
	static FLinearColor BranchColor(ac::Branch Branch);

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
		/// A node, a slot (0..2), a tab, a building.
		int32 Index = 0;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	void Press(const FTarget& T);
	/// Where a node sits: its branch column, tier row (0..4) and place in the row.
	struct FCell
	{
		int32 Column = 0;
		int32 Row = 0;
		int32 Place = 0;
	};
	static FCell CellOf(ac::Skill Skill);
	static std::optional<ac::Skill> SkillAt(int32 Column, int32 Row, int32 Place);
	static int32 RowSize(int32 Row) { return Row == 4 ? 1 : 3; }

	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;

	void PaintHeader(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintTree(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintTile(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, ac::Skill Skill, const FBox2f& Box) const;
	void PaintDetail(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintSlots(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintBlueprints(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FTarget& T, const FString& Label,
		bool bLit, const FLinearColor& Accent) const;

	FAcSkillScreenInfo Info;
	FString Refusal;
	ETab CurrentTab = ETab::Skills;
	ac::Skill Pick = ac::Skill::quickDrill;
	bool bPickSet = false;

	FAcOnSkill OnSpendDelegate;
	FAcOnSkill OnUndoDelegate;
	FAcOnSkillSlot OnSlotDelegate;
	FSimpleDelegate OnRespecDelegate;
	FSimpleDelegate OnCloseDelegate;

	/// When each node last took a rank (the learn flash), and when the screen opened.
	std::map<ac::Skill, double> Flashes;
	double OpenedAt = 0;
	TSharedPtr<FActiveTimerHandle> Timer;

	/// A learned active held by the mouse, on its way to a slot.
	std::optional<ac::Skill> Dragging;
	FVector2f PressAt = FVector2f::ZeroVector;
	FVector2f PointerAt = FVector2f::ZeroVector;

	mutable TArray<FTarget> Targets;
	mutable std::map<ac::Skill, FBox2f> Tiles;
	mutable FBox2f TreeBox;
	mutable FBox2f DetailBox;
	mutable FBox2f SlotBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
