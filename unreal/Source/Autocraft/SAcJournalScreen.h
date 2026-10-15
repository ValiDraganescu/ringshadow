// `SAcJournalScreen`: the pilot's Journal in the MMORPG mode (L; K2,
// docs/missions.md): the Citadel missions it signed up for, each with the
// officer's briefing, what to do, how far along, its place on the map, the
// reward, and the narration to play again. Drawn by hand in the HUD's
// chrome, the Citadel screen's sibling:
//
//   THE PROSPECTOR'S JOURNAL   2/3 missions                                ESC
//   ┌ NEXT · KSTR ──────────────────────────────────────────────────────────┐
//   │ Guard Your Ore   B  BUILD A GATLING GUN BY YOUR ORE        +240 XP …  │
//   │ why it matters · the radio's line                                     │
//   └───────────────────────────────────────────────────────────────────────┘
//   ┌ MISSIONS ─────────┐ ┌ SURVEY · HARD · Rust Hollow ──────────────────────┐
//   │▸ Beyond the Ridge  │ │ Beyond the Ridge                        [▶ PLAY]  │
//   │  8 / 15 s ▬▬▬      │ │ BRIEFING  the officer's words, wrapped …          │
//   │  Into the Nest     │ │ WHAT TO DO  the instructions                       │
//   │  DONE              │ │ PROGRESS ▬▬▬▬▬▬▬▬▬     ┌ the world, the marker ┐  │
//   │                    │ │ REWARD   1,500 XP · …  └───────────────────────┘  │
//   └────────────────────┘ └──────────────────────────────────── [ABANDON] ────┘
//   what happened, or why not                                        the keys
//
// The screen only shows and asks: a play or an abandon goes to its owner
// (`OnPlay`, `OnAbandon`), which plays the narration or asks the server. The
// game goes on behind; the pilot's unit stands while it is open (AcGameFlow).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/// A mission in the Journal.
struct FAcJournalEntry
{
	uint32 Id = 0;
	FString Title;
	/// "SURVEY", the tier (0 easy, 1 hard, 2 perilous), the region's name.
	FString Kind;
	int32 Tier = 0;
	FString Where;
	FString Briefing;
	FString Instructions;
	/// "8 / 15 s scanned", and the share done.
	FString Progress;
	float Share = 0;
	bool bDone = false;
	FString Reward;
	/// Its place on the world's grid (`bMarked`): the region's cell, and the
	/// marker's point within it (0…1 across, 0…1 down) and ring (a share of
	/// the region's width; 0: a point; the whole region: a hunt).
	bool bMarked = false;
	int32 Column = 0;
	int32 Row = 0;
	FVector2f At = FVector2f(0.5f, 0.5f);
	float Ring = 0;
	bool bWholeRegion = false;
	/// "Here: 120 m north-east", "Rust Hollow, west of here".
	FString Bearing;
	/// Its narration (empty: none) and how long it is.
	FString Narration;
	double Seconds = 0;

	bool operator==(const FAcJournalEntry&) const = default;
};

/// What the Journal shows.
struct FAcJournalInfo
{
	bool bKnown = false;
	TArray<FAcJournalEntry> Entries;
	int32 Most = 3;
	/// The world's grid, its regions' names by cell (row × Columns + column;
	/// empty: none there), and where the pilot is (its cell and point).
	int32 Columns = 0;
	int32 Rows = 0;
	TArray<FString> Names;
	bool bHere = false;
	int32 HereColumn = 0;
	int32 HereRow = 0;
	FVector2f HereAt = FVector2f(0.5f, 0.5f);
	/// The narration playing now (its key; empty: none).
	FString Playing;
	/// L1: KSTR's next goal (`bNext`), on top: its title, why it matters,
	/// what to do (the HUD's line), the radio's line, the pay, where.
	bool bNext = false;
	FString NextTitle;
	FString NextWhy;
	FString NextHint;
	FString NextPitch;
	FString NextPay;
	FString NextWhere;

	bool operator==(const FAcJournalInfo&) const = default;
};

DECLARE_DELEGATE_OneParam(FAcOnJournalAsk, int32 /*Index*/);

class AUTOCRAFT_API SAcJournalScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcJournalScreen) {}
		SLATE_EVENT(FAcOnJournalAsk, OnPlay)
		SLATE_EVENT(FAcOnJournalAsk, OnAbandon)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SAcJournalScreen() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcJournalInfo& Info);
	const FAcJournalInfo& GetInfo() const { return Info; }
	/// The picked mission (its index in the entries), kept on its id as the
	/// list changes.
	int32 Picked() const { return Pick; }
	void SetPicked(int32 Index);
	/// Why the last ask was refused (empty: none), and a line of what was
	/// done ("Playing the briefing…"). Shown at the foot.
	void SetRefusal(const FString& Why);
	void SetNote(const FString& Note);
	const FString& RefusalText() const { return Refusal; }

	/// Play the picked mission's narration again (P, Enter, the button).
	void Play();
	/// Abandon the picked mission: the first press arms, the second asks
	/// (X twice, or the button twice).
	void Abandon();
	bool AbandonArmed() const { return bArmed; }
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
		int32 Row = 0;
		int32 Index = 0;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	void Press(const FTarget& T);
	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;

	void PaintHeader(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintNext(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintList(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintPage(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintMap(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FAcJournalEntry& E, const FBox2f& B) const;
	void PaintFoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FTarget& T, const FString& Label,
		bool bLit, const FLinearColor& Accent) const;

	FAcJournalInfo Info;
	int32 Pick = 0;
	uint32 PickedId = 0;
	bool bArmed = false;
	FString Refusal;
	FString Note;

	FAcOnJournalAsk OnPlayDelegate;
	FAcOnJournalAsk OnAbandonDelegate;
	FSimpleDelegate OnCloseDelegate;
	double OpenedAt = 0;
	TSharedPtr<FActiveTimerHandle> Timer;

	mutable TArray<FTarget> Targets;
	mutable FBox2f ListBox;
	mutable FBox2f PageBox;
	mutable FBox2f FootBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
