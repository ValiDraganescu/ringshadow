// `SAcMenu` (the launcher, AcGameFlow): the home screen and the pause menu,
// drawn in HUD chrome like `SAcNewGame` over the dimmed (paused) game.
//
//   Home (H1):  RINGSHADOW (Exo 2, the brand's wordmark)
//               A lone prospector under a ringed giant
//               [ENTER THE REACH      ]  (the MMORPG: resumes the saved
//               [ ashfall reach · ... ]   world, or starts one; highlighted)
//               [NEW WORLD]              (asks first: it deletes the save)
//               [World server running · since 21:40   STOP]  (S2: when the
//                                        world server runs on with the game away)
//               THE STRATEGY GAME
//               [your game: map, teams, time played]
//               [RESUME SKIRMISH] (when there is one)
//               [NEW SKIRMISH]
//               [SETTINGS] [QUIT]
//
//   Pause:  PAUSED
//           [the game: map, teams, time]
//           [RESUME]
//           [SETTINGS]
//           [QUIT TO HOME]
//
// NEW WORLD opens a page that says what it deletes, with DELETE AND START
// OVER and BACK (highlighted): only the first picks it.
//
// Settings (from either): sliders for the master, music, effects and
// environment volumes and the mouse turn while driving, then BACK. They
// change `UAcSettings` live and store it on the way back.
//
// Modal: every click stays here. Up/Down move the highlight, Left/Right move
// a slider, Return picks (routed by the flow's key processor), the mouse
// hovers, clicks and drags.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class FAcLauncherArt;

enum class EAcMenuMode : uint8 { Home, Pause };
enum class EAcMenuAction : uint8 { Resume, NewGame, QuitToHome, QuitApp, Settings, Mmo, MmoNewWorld, Back, StopServer };

DECLARE_DELEGATE_OneParam(FAcOnMenuPick, EAcMenuAction);

class AUTOCRAFT_API SAcMenu : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcMenu) {}
		SLATE_ARGUMENT(EAcMenuMode, Mode)
		SLATE_EVENT(FAcOnMenuPick, OnPick)
		/// Every pick, the widget's own pages too (Settings, Back): its sound.
		SLATE_EVENT(FAcOnMenuPick, OnSound)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/// What the game card says: "Highlands · Medium", "2 v 2 · 12:04 played · AI on · fog on".
	/// Empty title: no game to resume (home shows no card and no Resume).
	void SetGame(const FString& Title, const FString& Detail);
	void SetMode(EAcMenuMode InMode);
	/// The line under ENTER THE REACH ("ASHFALL REACH · YOUR CLAIM WAITS").
	void SetReach(const FString& Line);
	/// S2: "World server running · since 21:40" when the world's server runs
	/// on its own (a STOP row on the home screen); empty: none.
	void SetServer(const FString& Line);
	/// X2: what the world did while the pilot was away, under ENTER THE
	/// REACH ("Your world ran 6 h 12 m." and the most telling things;
	/// empty: none). From the server's summary file beside the save.
	void SetAway(const FString& Ran, const FString& What);
	const FString& AwayRan() const { return AwayRanLine; }
	const FString& AwayWhat() const { return AwayWhatLine; }
	const FString& ServerText() const { return ServerLine; }
	/// The song title for the home screen's bottom-left line (empty: none).
	void SetNowPlaying(const FString& Title);
	/// Home with key art: the art fills the window and the panel sits left.
	bool HasArt() const;
	EAcMenuMode Mode() const { return MenuMode; }

	/// Keys (from the flow's key processor): -1 up, +1 down; Pick the highlighted.
	void Move(int32 Step);
	void PickHighlighted();
	/// Left/Right on the Settings page: the highlighted slider a step.
	void Adjust(int32 Step);
	/// Back from the Settings page (stores the settings); false when not on it.
	bool Back();
	bool InSettings() const { return bSettings; }
	/// The new world's page is up (MMORPG NEW WORLD picked, not confirmed).
	bool ConfirmingNewWorld() const { return bConfirmNewWorld; }
	/// Highlights the button for `Action` on the page shown; false: none.
	bool Highlight(EAcMenuAction Action);
	/// Opens the Settings page whatever buttons there are (stills).
	void OpenSettings();

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100, 100); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	struct FButton
	{
		EAcMenuAction Action;
		FString Label;
		/// The home screen's first entry: taller, larger, with the Reach line.
		bool bPrimary = false;
		/// Half the row: SETTINGS and QUIT share one.
		bool bHalf = false;
		/// The strategy game's group starts here (its heading and card above).
		bool bGroup = false;
		/// The world server's row: its line, STOP at the right.
		bool bServer = false;
	};
	TArray<FButton> Buttons() const;
	void Layout(FVector2f ViewSize) const;
	void PaintArt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f View) const;
	void PaintWordmark(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f Centre, float Width) const;
	EActiveTimerReturnType OnArtTimer(double Now, float Delta);
	bool ArtShowing() const { return MenuMode == EAcMenuMode::Home && HasArt(); }
	int32 ButtonAt(FVector2f LocalPoint) const;
	void Pick(int32 Index);
	/// The Settings page's sliders: how many, their names, their value as 0-1.
	static int32 SliderCount();
	static FString SliderName(int32 I);
	static FString SliderText(int32 I);
	static float SliderGet(int32 I);
	static void SliderSet(int32 I, float Fraction);
	void DragTo(FVector2f LocalPoint);

	EAcMenuMode MenuMode = EAcMenuMode::Home;
	FAcOnMenuPick OnPickDelegate;
	FAcOnMenuPick OnSoundDelegate;
	FString ReachLine;
	FString ServerLine;
	FString AwayRanLine;
	FString AwayWhatLine;
	FString GameTitle;
	FString GameDetail;
	int32 Highlighted = 0;
	int32 Pressed = INDEX_NONE;
	bool bSettings = false;
	bool bConfirmNewWorld = false;
	int32 Dragging = INDEX_NONE;
	FString NowPlaying;
	/// The key art (home only); the clock of its drift and crossfade starts at the first paint.
	mutable TSharedPtr<FAcLauncherArt> Art;
	mutable double ArtStart = -1;

	/// Laid out at paint (panel points).
	mutable TArray<FBox2f> Boxes;
	mutable FBox2f CardBox;
	/// The home screen's group heading (its top left; zero: none).
	mutable FVector2f GroupAt = FVector2f::ZeroVector;
	/// X2: where the away lines start (zero: none shown).
	mutable FVector2f AwayAt = FVector2f::ZeroVector;
	/// The sliders' tracks (Settings page; `Boxes` has their rows, then BACK).
	mutable TArray<FBox2f> Tracks;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;
};
