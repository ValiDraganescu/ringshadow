// `UAcGameFlowSubsystem` (chunk F1, GAME-LAYER.md §2.14): the game's flow
// around `UAcSimSubsystem`, as Swift's `AppDelegate` window mode and
// `GameController` do it:
//
// - New game (⌘N, `ac.NewGame`): the dialog `SAcNewGame` (map, size,
//   players, teams, AI, fog, a preview), modal: the game pauses and the
//   pointer, camera and card keys are blocked while it is up. Start →
//   `UAcSimSubsystem::NewGame` (the old game is marked abandoned in the log).
//   On the first start with no map remembered it opens by itself (Swift
//   `startWindowMode`); cancelling then keeps the default map running
//   (Swift quits).
// - Restart (⌘R, `ac.Restart`): the same map from the start, teams, AI and
//   fog kept, the camera kept (`restartMap`).
// - The victory lap → next game is `UAcSimSubsystem` (30 s, then
//   `NextGame`); every system rebuilds on `OnGameStarted`.
// - Saving: `UAcSimSubsystem` saves every 15 s of real time on a worker
//   thread (one write at a time) and on quit; sessions are in
//   `AcSaves::Directory()`. The last map played is remembered
//   (`UAcSettings::LastMap`) and opened next time.
// - The MMORPG test (the home screen's MMORPG (TEST), -AcMmo; docs/mmorpg-m1.md
//   T5): `UAcSimSubsystem::StartMmo` on the first world; QUIT TO HOME
//   leaves it (the local server stops). New game, restart, import, the
//   toggles and save say no there (`SAcMmoBanner`, which also shows where
//   the local server is and why it failed, and the region the pilot is in
//   or flies to). M opens the world map (`SAcWorldMap`, docs/mmorpg-m2.md
//   R3) in place of the command map: a pad picked there and FLY ask the
//   server for the flight.
// - Import a Swift game: the dialog's button or `ac.Import [PATH]` (default
//   the Swift game's `window.json`), `-AcImport=PATH` at launch.
// - Toggles: `ac.AI [0|1]` (Swift's Game ▸ AI Commander), `ac.SessionFog
//   [0|1]` (`session.fog`: the core's vision, unlike B10's display-only
//   `ac.Fog`), both saved with the session; the dialog sets them for a new
//   game. `ac.Save` writes now.
//
// Checks (headless, seconds of play, never a whole game):
//   -AcFlowTest=STEP[,STEP...]  save, resume, import, autosave, fog,
//                               newgame, victory; each logs `flowtest: ...
//                               ok|FAILED`, then the run quits.
//   -AcNewGameDialog[=MAP[,TEAMS]]  open the dialog at start (shots hold
//                               until its preview is drawn).
//   -AcMmo                      the MMORPG test mode at once (no home screen).
//   -AcHomePlay=S               open the home screen with the staged game as
//                               the one to resume, and press RESUME GAME S
//                               seconds after it shows (clips: home, then play).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SAcMenu.h"
#include "SAcMmoBanner.h"
#include "SAcNewGame.h"
#include "SAcWorldMap.h"
#include "Session.h"

#include <optional>

#include "AcGameFlow.generated.h"

class UAcSimSubsystem;
class IInputProcessor;

UCLASS()
class AUTOCRAFT_API UAcGameFlowSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAcGameFlowSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/// Open the new-game dialog (`MapPicker.ask`), starting from `Initial`
	/// (unset: the game being played). False when there is no HUD to put it on.
	bool OpenNewGame(const FString& Message = TEXT("Pick a map and how big it is."),
		TOptional<FAcNewGameChoice> Initial = {});
	void CloseNewGame();
	bool IsNewGameOpen() const { return Dialog.IsValid(); }
	TSharedPtr<SAcNewGame> NewGameDialog() const { return Dialog; }

	/// A new game on `Choice` (the dialog's Start).
	void StartNewGame(const FAcNewGameChoice& Choice);
	/// The same map over again (⌘R).
	void Restart();
	/// Import a Swift session file as the window game.
	bool Import(const FString& Path);

	/// The choice that describes the game being played.
	FAcNewGameChoice CurrentChoice() const;

	/// The launcher: the home screen (Resume, New Game, Quit) and the pause
	/// menu (Escape in the game: Resume, Quit to home). The game waits behind.
	bool OpenMenu(EAcMenuMode Mode);
	void CloseMenu();
	bool IsMenuOpen() const { return Menu.IsValid(); }
	TSharedPtr<SAcMenu> MenuWidget() const { return Menu; }
	/// Escape in the game opens the pause menu when nothing else wants it
	/// (no selection, command map shut, not driving, the console shut).
	bool WantsPauseOnEscape() const;

	/// The MMORPG test mode on the first world (the home screen's item).
	void StartMmo();
	/// The MMORPG world map (M in the mode). False outside the mode or
	/// without a HUD.
	bool OpenWorldMap();
	void CloseWorldMap();
	void ToggleWorldMap();
	TSharedPtr<SAcWorldMap> WorldMapWidget() const { return WorldMap; }
	/// The world map's FLY: asks the server; the map closes when asked, or
	/// says why not.
	void FlyFromMap(const FString& Region, const FString& Pad);
	/// Back to the window game: the local server stops.
	void StopMmo();

private:
	void OnGameStarted(UAcSimSubsystem& Sim);
	void Remember(const UAcSimSubsystem& Sim);
	void LeaveDriving();
	void SetBlocked(bool bBlocked);
	void TickFlowTest();
	/// The MMORPG test mode's banner: the server's state, the refusals.
	void TickMmo();
	void OnRefused(const FString& Why);
	TSharedPtr<SAcMmoBanner> MmoBanner;
	TSharedPtr<SAcWorldMap> WorldMap;
	/// The world map's state from the client, each frame.
	void TickWorldMap();
	FDelegateHandle RefusedHandle;
	/// -AcMmo: start the mode once the game is up.
	bool bMmoWhenReady = false;

	TSharedPtr<SAcNewGame> Dialog;
	TSharedPtr<SAcMenu> Menu;
	/// A game to resume from home: a saved one loaded, or one started here.
	bool bHasGame = false;
	bool bHomeWhenReady = false;
	/// -AcHomePlay=S: press RESUME GAME this long after home shows (engine time); <0 off.
	double HomePlayAfter = -1;
	double HomeShownAt = 0;
	void OnMenuPick(EAcMenuAction Action);
	void RefreshMenuGame();
	/// The game waits and the world takes no input while a dialog or menu is up.
	void BeginModal();
	void EndModal();
	int32 ModalDepth = 0;
	TSharedPtr<IInputProcessor> Keys;
	FDelegateHandle StartedHandle;
	bool bPausedBefore = false;
	/// Open the dialog once the HUD is up (first start, -AcNewGameDialog).
	bool bOpenWhenReady = false;
	FString OpenMessage;
	TOptional<FAcNewGameChoice> OpenChoice;
	bool bShotHeld = false;

	/// -AcFlowTest.
	struct FFlowTest
	{
		TArray<FString> Steps;
		int32 Step = 0;
		int32 Phase = 0;
		double PhaseAt = 0;
		double GameAt = 0;
		int32 Failures = 0;
		int32 GamesStarted = 0;
		FString GameId;
		FDateTime FileTime;
		/// What was on disk before the game loaded (resume) or the file imported.
		std::optional<ac::Session> Before;
		std::optional<ac::Session> Expected;
	};
	TOptional<FFlowTest> Test;
	void Check(bool bOk, const FString& What);
	void NextStep();
};
