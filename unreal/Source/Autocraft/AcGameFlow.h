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
//   server for the flight. The world lasts (docs/mmorpg-m3.md S4): MMORPG
//   (TEST) resumes the saved world with the game's own pilot; MMORPG NEW
//   WORLD (TEST) asks first, then deletes the save and starts over with the
//   same pilot.
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
//   -AcMmo                      the MMORPG test mode at once (no home screen),
//                               resumed; with -AcMmoNewWorld a new world.
//                               -AcMmoDir=PATH: its pilot key and save there.
//   -AcHomePlay=S               open the home screen with the staged game as
//                               the one to resume, and press RESUME GAME S
//                               seconds after it shows (clips: home, then play).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SAcMenu.h"
#include "SAcMmoBanner.h"
#include "SAcNewGame.h"
#include "SAcCitadelScreen.h"
#include "SAcTurretCard.h"
#include "SAcJournalScreen.h"
#include "SAcRigScreen.h"
#include "SAcSkillScreen.h"
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

	/// The MMORPG test mode on the first world (the home screen's items):
	/// resumed, or (`bNewWorld`) its save deleted and started over.
	void StartMmo(bool bNewWorld = false);
	/// The MMORPG world map (M in the mode). False outside the mode or
	/// without a HUD.
	bool OpenWorldMap();
	void CloseWorldMap();
	void ToggleWorldMap();
	TSharedPtr<SAcWorldMap> WorldMapWidget() const { return WorldMap; }
	/// The world map's FLY: asks the server; the map closes when asked, or
	/// says why not.
	/// A mission or a site clicked on the world map: the compass's target
	/// (clicked again: none).
	void TargetFromMap(const ac::net::atlas::Item& It);
	void FlyFromMap(const FString& Region, const FString& Pad);
	/// The Prospector's skill screen (K in the mode; docs/prospector-progression.md).
	/// False outside the mode or without a HUD.
	bool OpenSkillScreen();
	void CloseSkillScreen();
	void ToggleSkillScreen();
	TSharedPtr<SAcSkillScreen> SkillScreenWidget() const { return SkillScreen; }
	/// The Prospector's rig screen (J in the mode, or the skill screen's RIG
	/// tab; docs/prospector-progression.md "Foundry and Lab"). False outside
	/// the mode or without a HUD.
	bool OpenRigScreen();
	void CloseRigScreen();
	void ToggleRigScreen();
	TSharedPtr<SAcRigScreen> RigScreenWidget() const { return RigScreen; }
	/// The realm's Citadel screen (H in the mode, at the realm's Citadel; C1,
	/// docs/prospector-progression.md "The realm's tax"): the realm wallet,
	/// the tax, donations. False outside the mode, without a HUD, or away
	/// from the Citadel (the HUD says so).
	bool OpenCitadelScreen();
	void CloseCitadelScreen();
	void ToggleCitadelScreen();
	TSharedPtr<SAcCitadelScreen> CitadelScreenWidget() const { return CitadelScreen; }
	/// A placed turret's card (U in the mode; D3, docs/mmorpg-m8.md): the
	/// Sentinel or Gatling Gun of the realm under the crosshair within
	/// `Defence::turretReach`, else the nearest that close; its marks, bought
	/// from the pilot's wallet. False outside the mode, without a HUD, or
	/// with no turret near (the HUD says so). AcGameFlowTurret.cpp.
	bool OpenTurretCard();
	void CloseTurretCard();
	void ToggleTurretCard();
	TSharedPtr<SAcTurretCard> TurretCardWidget() const { return TurretCard; }
	/// The realm's turret the card would open on now (nil: none near).
	std::optional<int64> NearTurret() const;
	/// The pilot's Journal (L in the mode; K2, docs/missions.md): the Citadel
	/// missions it signed up for, their briefings, places and narrations.
	/// False outside the mode or without a HUD.
	bool OpenJournalScreen();
	void CloseJournalScreen();
	void ToggleJournalScreen();
	TSharedPtr<SAcJournalScreen> JournalScreenWidget() const { return JournalScreen; }
	/// Plays a mission's narration (its template key), the sting first when
	/// `bAccepted` (just signed up). Logged in a silent run.
	void PlayMissionNarration(const FString& Key, bool bAccepted);
	/// The driven Prospector stands where it unloads at the realm's finished
	/// Citadel (the client's mirror); `Builder`: who built it.
	bool AtRealmCitadel(FString* Builder = nullptr) const;
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
	/// The arrival whose region title was shown (`FAcMmoClient::Visit`; -1: none yet).
	int64 MmoTitledVisit = -1;
	TSharedPtr<SAcWorldMap> WorldMap;
	/// The world map's state from the client, each frame.
	void TickWorldMap();
	TSharedPtr<SAcSkillScreen> SkillScreen;
	/// The skill screen's state from the client's progress, each frame.
	void TickSkillScreen();
	/// The server's last skill refusal already handed to the screen.
	FString SkillRefusalShown;
	TSharedPtr<SAcRigScreen> RigScreen;
	/// The rig screen's state from the client's progress, each frame.
	void TickRigScreen();
	/// The server's last rig refusal already handed to the screen.
	FString RigRefusalShown;
	TSharedPtr<SAcCitadelScreen> CitadelScreen;
	/// The Citadel screen's state from the client's progress, each frame.
	void TickCitadelScreen();
	/// The server's last Citadel refusal, and the kept asks, already handed to the screen.
	FString CitadelRefusalShown;
	int32 CitadelKeptShown = 0;
	TSharedPtr<SAcTurretCard> TurretCard;
	/// The turret the card shows; its state each frame; the server's last
	/// turret refusal and kept asks already handed to it.
	int64 TurretCardId = 0;
	void TickTurretCard();
	FString TurretRefusalShown;
	int32 TurretKeptShown = 0;
	TSharedPtr<SAcJournalScreen> JournalScreen;
	/// The Journal's state from the client's progress, each frame; and the
	/// missions just signed up for (their sting and narration).
	void TickJournalScreen();
	void TickMissions();
	/// The narration playing (its key; empty: none) and until when (engine time).
	FString NarrationKey;
	double NarrationUntil = 0;
	/// Sign-ups the server kept whose Journal entry has not come yet (the
	/// reply can beat the progress message), and since when.
	TArray<TPair<uint32, double>> MissionsHeard;
	FDelegateHandle RefusedHandle;
	/// -AcMmo: start the mode once the game is up.
	bool bMmoWhenReady = false;
	/// -AcMmoNewWorld: that start is a new world.
	bool bMmoNewWorldWhenReady = false;

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
	/// The MMORPG steps (`mmocross`, `mmofly`, `mmoresume`; AcMmoFlowTest.cpp): the real
	/// pilot pawn and world map over the local server.
	void TickMmoFlow(const FString& Step);
	int64 MmoVisitWas = 0;
	/// mmokeys (F2): the Landing Pads asked of the server.
	bool bMmoPadsAsked = false;
	/// `mmoresume`: what was left (the Citadel, the wallet, the unit, the key).
	struct FMmoLeft
	{
		int64 Citadel = -1;
		FVector2D CitadelAt = FVector2D::ZeroVector;
		int64 Ore = -1;
		int64 Unit = -1;
		FVector2D UnitAt = FVector2D::ZeroVector;
		FString Region;
		FString Key;
		/// `mmoreenter`: the buildings and units the client had in view.
		int32 Buildings = -1;
		int32 Units = -1;
	};
	FMmoLeft MmoLeft;
	/// Builds a Citadel next to the driven Prospector (once); its spot.
	bool MmoBuildCitadel();
	/// `mmokeys` (the world map by its keys) and `mmohud` (the HUD in the
	/// mode, top-down and driving): AcMmoHudFlowTest.cpp.
	void TickMmoHudFlow(const FString& Step);
	/// A key through the flow's key processor, as the player presses it.
	bool PressFlowKey(const struct FKey& Key);
	/// U2: `mmosettings` (the Settings page from the home screen and the
	/// pause menu, a volume and the screen changed and stored, the key card)
	/// and `mmosettingsback` (a restart: read back); AcSettingsFlowTest.cpp.
	void TickSettingsFlow(const FString& Step);
	/// The picture asked for (-AcMmoShots=DIR), waited for before the next.
	FString SettingsShot;
	FIntPoint SettingsWant = FIntPoint::ZeroValue;
	FVector2D RtsTargetWas = FVector2D::ZeroVector;
	/// -AcMmoShots=DIR (AcMmoShots.cpp): the MMO moments staged in a new
	/// world, a PNG of each (HUD on) in DIR; run as the flow step `mmoshots`.
	void TickMmoShots();
	/// `mmotwo` (W5, AcMmoTwo.cpp): two humans on one world, this game one
	/// of them (-AcMmoTwo=A|B), the other a second game process
	/// (unreal/Tools/two_clients.py); pictures with -AcMmoShots=DIR
	/// -AcMmoShotsW5.
	void TickMmoTwo();
	struct FMmoShots
	{
		FString Dir;
		/// The picture asked for and not written yet, and its size last frame.
		FString Pending;
		int64 LastSize = -1;
		int32 Waited = 0;
		int32 Saved = 0;
		/// The drive: waypoints (cells), the next one, and the stall check.
		TArray<FVector2D> Route;
		int32 RouteAt = 0;
		FVector2D StallAt = FVector2D::ZeroVector;
		double StallSince = 0;
		int64 Unit = -1;
		int64 Citadel = -1;
		int64 Patch = -1;
		int64 OreBefore = -1;
		/// The MH detour: the Derrick, and the wallet's MH before the unload.
		int64 Derrick = -1;
		int64 HydrogenBefore = -1;
		int64 VisitWas = 0;
		/// F2: the Landing Pads asked of the server (the default set's flight).
		bool PadsAsked = false;
		bool bCardsShot = false;
		/// Moments a world without level-up cards skips (07, 18), counted
		/// with the pictures.
		int32 Skipped = 0;
		/// When the cards were first up (the banner is shot once it is in).
		double CardsSince = -1;
		bool bShotBefore = false;
		FString Region;
		FVector2D UnitAt = FVector2D::ZeroVector;
	};
	FMmoShots Shots;
	void Check(bool bOk, const FString& What);
	void NextStep();
};
