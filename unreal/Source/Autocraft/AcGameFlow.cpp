#include "AcGameFlow.h"
#include "AcMusicPlayer.h"

#include "AcAudioDirector.h"
#include "AcCommandMap.h"
#include "AcCommands.h"
#include "AcGameMode.h"
#include "AcHUD.h"
#include "AcLog.h"
#include "AcHudStyle.h"
#include "AcMissions.h"
#include "AcPilotPawn.h"
#include "AcPointer.h"
#include "AcProgressHud.h"
#include "AcRtsPawn.h"
#include "AcSaves.h"
#include "AcSettings.h"
#include "AcShot.h"
#include "AcSimSubsystem.h"
#include "SAcRoot.h"

#include "Engine/Console.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include "Commander.h"
#include "NavGrid.h"
#include "Pilot.h"
#include "Defence.h"
#include "Ringshadow.h"
#include "Rules.h"

namespace
{
	/// Over the command map (60), under the tip (90).
	constexpr int32 NewGameLayer = 80;
	/// The home screen and pause menu: under the new-game dialog it opens.
	constexpr int32 MenuLayer = 75;
	/// The MMORPG world map: over the command map, under the menu.
	constexpr int32 WorldMapLayer = 70;
	/// The MMORPG skill screen: beside the world map (one at a time).
	constexpr int32 SkillScreenLayer = 71;
	/// The MMORPG rig screen: the skill screen's sibling (one at a time).
	constexpr int32 RigScreenLayer = 72;
	/// The MMORPG Citadel screen (C1): the rig screen's sibling (one at a time).
	constexpr int32 CitadelScreenLayer = 73;
	constexpr int32 JournalScreenLayer = 73;

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	FString WindowFile() { return FPaths::Combine(AcSaves::Directory(), TEXT("window.json")); }

	std::optional<ac::Session> ReadSession(const FString& Path)
	{
		return ac::SessionStore::load(std::string(TCHAR_TO_UTF8(*Path)));
	}

	/// ⌘N, ⌘R (the menu's keys), and Return/Escape for the dialog when it
	/// has not got the keyboard.
	class FAcFlowKeys final : public IInputProcessor
	{
	public:
		explicit FAcFlowKeys(UAcGameFlowSubsystem* InFlow) : Flow(InFlow) {}
		virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}
		virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
		{
			UAcGameFlowSubsystem* F = Flow.Get();
			if (!F || Event.IsRepeat()) return false;
			const FKey K = Event.GetKey();
			if (TSharedPtr<SAcWorldMap> W = F->WorldMapWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::M && !Event.IsCommandDown())) { F->CloseWorldMap(); return true; }
				if (K == EKeys::Enter) { W->Fly(); return true; }
				// The arrows (and WASD) move the pad pick.
				if (!Event.IsCommandDown())
				{
					if (K == EKeys::Left || K == EKeys::A) { W->MovePick(-1, 0); return true; }
					if (K == EKeys::Right || K == EKeys::D) { W->MovePick(1, 0); return true; }
					if (K == EKeys::Up || K == EKeys::W) { W->MovePick(0, -1); return true; }
					if (K == EKeys::Down || K == EKeys::S) { W->MovePick(0, 1); return true; }
				}
				// The game's own keys wait while the map is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcRigScreen> S = F->RigScreenWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::J && !Event.IsCommandDown())) { F->CloseRigScreen(); return true; }
				if (K == EKeys::K && !Event.IsCommandDown()) { F->OpenSkillScreen(); return true; }
				if (!Event.IsCommandDown() && S->Key(K)) return true;
				// The game's own keys wait while the screen is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcCitadelScreen> S = F->CitadelScreenWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::H && !Event.IsCommandDown())) { F->CloseCitadelScreen(); return true; }
				if (!Event.IsCommandDown() && S->Key(K)) return true;
				// The game's own keys wait while the screen is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcTurretCard> S = F->TurretCardWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::U && !Event.IsCommandDown())) { F->CloseTurretCard(); return true; }
				if (!Event.IsCommandDown() && S->Key(K)) return true;
				// The game's own keys wait while the card is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcJournalScreen> S = F->JournalScreenWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::L && !Event.IsCommandDown())) { F->CloseJournalScreen(); return true; }
				if (!Event.IsCommandDown() && S->Key(K)) return true;
				// The game's own keys wait while the screen is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcSkillScreen> S = F->SkillScreenWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::K && !Event.IsCommandDown())) { F->CloseSkillScreen(); return true; }
				if (K == EKeys::J && !Event.IsCommandDown()) { F->OpenRigScreen(); return true; }
				if (!Event.IsCommandDown() && S->Key(K)) return true;
				// The game's own keys wait while the screen is up (⌘Q still quits).
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcNewGame> D = F->NewGameDialog())
			{
				if (K == EKeys::Enter) { D->Start(); return true; }
				if (K == EKeys::Escape) { D->Cancel(); return true; }
				// The game's own keys wait while the dialog is up.
				return !Event.IsCommandDown();
			}
			if (TSharedPtr<SAcMenu> M = F->MenuWidget())
			{
				if (K == EKeys::Up || K == EKeys::W) M->Move(-1);
				else if (K == EKeys::Down || K == EKeys::S || K == EKeys::Tab) M->Move(1);
				else if (K == EKeys::Left || K == EKeys::A) M->Adjust(-1);
				else if (K == EKeys::Right || K == EKeys::D) M->Adjust(1);
				else if (K == EKeys::Enter || K == EKeys::SpaceBar) M->PickHighlighted();
				else if (K == EKeys::Escape && M->Back()) {}
				else if (K == EKeys::Escape && M->Mode() == EAcMenuMode::Pause) F->CloseMenu();
				else if (K == EKeys::N && Event.IsCommandDown()) F->OpenNewGame();
				// Nothing else reaches the game while the menu is up (⌘Q still quits).
				return !(Event.IsCommandDown() && K == EKeys::Q);
			}
			if (K == EKeys::Escape && !Event.IsCommandDown() && F->WantsPauseOnEscape())
			{
				F->OpenMenu(EAcMenuMode::Pause);
				return true;
			}
			if (!Event.IsCommandDown() || Event.IsShiftDown() || Event.IsAltDown()) return false;
			if (K == EKeys::N) { F->OpenNewGame(); return true; }
			if (K == EKeys::R) { F->Restart(); return true; }
			return false;
		}
		virtual const TCHAR* GetDebugName() const override { return TEXT("AcFlowKeys"); }

	private:
		TWeakObjectPtr<UAcGameFlowSubsystem> Flow;
	};

	UAcGameFlowSubsystem* FlowOf(UWorld* World) { return World ? World->GetSubsystem<UAcGameFlowSubsystem>() : nullptr; }

	FAutoConsoleCommandWithWorldAndArgs CmdNewGame(TEXT("ac.NewGame"),
		TEXT("ac.NewGame: the new-game dialog. ac.NewGame MAP [TEAMS]: start that game now (badlands-large-8 4v4)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			UAcGameFlowSubsystem* F = FlowOf(World);
			if (!F) return;
			if (Args.Num() == 0)
			{
				F->OpenNewGame();
				return;
			}
			const std::optional<ac::MapChoice> Map = UAcSimSubsystem::ParseMap(Args[0]);
			if (!Map)
			{
				UE_LOG(LogAutocraft, Warning, TEXT("ac.NewGame: no map %s"), *Args[0]);
				return;
			}
			FAcNewGameChoice C = F->CurrentChoice();
			C.Map = *Map;
			C.Teams = Args.Num() > 1 ? Args[1] : SAcNewGame::TeamOptions(Map->players)[0];
			F->StartNewGame(C);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNewGameDialog(TEXT("ac.NewGameDialog"),
		TEXT("ac.NewGameDialog [MAP [TEAMS]]: open the new-game dialog on that choice; ac.NewGameDialog close."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			UAcGameFlowSubsystem* F = FlowOf(World);
			if (!F) return;
			if (Args.Num() > 0 && Args[0] == TEXT("close"))
			{
				F->CloseNewGame();
				return;
			}
			TOptional<FAcNewGameChoice> C;
			if (Args.Num() > 0)
			{
				if (const std::optional<ac::MapChoice> Map = UAcSimSubsystem::ParseMap(Args[0]))
				{
					FAcNewGameChoice N = F->CurrentChoice();
					N.Map = *Map;
					N.Teams = Args.Num() > 1 ? Args[1] : SAcNewGame::TeamOptions(Map->players)[0];
					C = N;
				}
			}
			if (F->IsNewGameOpen() && C) F->NewGameDialog()->SetChoice(*C);
			else F->OpenNewGame(TEXT("Pick a map and how big it is."), C);
		}));

	FAutoConsoleCommandWithWorld CmdRestart(TEXT("ac.Restart"), TEXT("The same map from the start (⌘R)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
			if (UAcGameFlowSubsystem* F = FlowOf(World)) F->Restart();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAI(TEXT("ac.AI"), TEXT("ac.AI [0|1]: the AI commanders off/on (toggles with no value); saved."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World);
			if (!Sim || !Sim->IsRunning()) return;
			Sim->SetAI(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Sim->IsAIOn());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSessionFog(TEXT("ac.SessionFog"),
		TEXT("ac.SessionFog [0|1]: the fog of war in the game (session.fog: what the player sees and hears), off/on; saved."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World);
			if (!Sim || !Sim->IsRunning()) return;
			Sim->SetFog(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Sim->IsFogOn());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdImport(TEXT("ac.Import"),
		TEXT("ac.Import [PATH]: play a Swift session JSON here (default: the Swift game's window.json)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) {
			if (UAcGameFlowSubsystem* F = FlowOf(World)) F->Import(Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : AcSaves::SwiftWindowFile());
		}));

	FAutoConsoleCommandWithWorld CmdSave(TEXT("ac.Save"), TEXT("Save the session now (in the background)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World) {
			if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World))
			{
				UE_LOG(LogAutocraft, Log, TEXT("save: %s"), Sim->Save() ? *WindowFile() : TEXT("not saved (a fixture or -AcNoSave)"));
			}
		}));
}

// MARK: - Subsystem

UAcGameFlowSubsystem* UAcGameFlowSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAcGameFlowSubsystem>() : nullptr;
}

bool UAcGameFlowSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UAcGameFlowSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAcGameFlowSubsystem, STATGROUP_Tickables);
}

void UAcGameFlowSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const TCHAR* Cmd = FCommandLine::Get();
	// Listen before the sim loads its session (in its OnWorldBeginPlay).
	UAcSimSubsystem* Sim = Collection.InitializeDependency<UAcSimSubsystem>();
	if (Sim) StartedHandle = Sim->OnGameStarted.AddUObject(this, &UAcGameFlowSubsystem::OnGameStarted);
	if (Sim) RefusedHandle = Sim->OnRefused.AddUObject(this, &UAcGameFlowSubsystem::OnRefused);

	FString Steps;
	if (FParse::Value(Cmd, TEXT("AcFlowTest="), Steps, false))
	{
		Test.Emplace();
		Steps.ParseIntoArray(Test->Steps, TEXT(","));
		UE_LOG(LogAutocraft, Log, TEXT("flowtest: %s in %s"), *Steps, *AcSaves::Directory());
		// What is on disk before the game opens, and what is imported.
		Test->Before = ReadSession(WindowFile());
		FString ImportPath;
		if (FParse::Value(Cmd, TEXT("AcImport="), ImportPath))
		{
			Test->Expected = ReadSession(FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), ImportPath));
		}
	}
	else if (FString ShotsDir; FParse::Value(Cmd, TEXT("AcMmoShots="), ShotsDir) && !ShotsDir.IsEmpty())
	{
		// The MMO moments in pictures (AcMmoShots.cpp): a flow step of its own.
		Test.Emplace();
		Test->Steps.Add(TEXT("mmoshots"));
		Shots.Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), ShotsDir);
		UE_LOG(LogAutocraft, Log, TEXT("mmoshots: pictures to %s, saves in %s, the pilot in %s"), *Shots.Dir, *AcSaves::Directory(),
			*FAcMmoClient::Directory());
	}
	// mmoshots as one of the listed steps (after flow tests, in one run).
	if (FString ShotsDir; Test && Test->Steps.Contains(TEXT("mmoshots")) && Shots.Dir.IsEmpty() && FParse::Value(Cmd, TEXT("AcMmoShots="), ShotsDir))
	{
		Shots.Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), ShotsDir);
	}
	// The MMORPG checks and pictures build at once: a new pilot has every
	// blueprint there (the server's --dev-blueprints), not only the crate's.
	// The first ten minutes' pictures (-AcMmoShotsO1) find theirs.
	if (FString MmoShotsDir; (Steps.Contains(TEXT("mmo")) || FParse::Value(Cmd, TEXT("AcMmoShots="), MmoShotsDir))
		&& !FParse::Param(Cmd, TEXT("AcMmoDevBlueprints")) && !FParse::Param(Cmd, TEXT("AcMmoShotsO1")))
	{
		FCommandLine::Append(TEXT(" -AcMmoDevBlueprints"));
	}
	// The perf panel as last left (Swift `showPerf`); the command line wins.
	static bool bAppliedPerf = false;
	if (!bAppliedPerf && UAcSettings::Remembers())
	{
		bAppliedPerf = true;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PerfPanel")))
		{
			V->Set(UAcSettings::Get().bShowPerf ? 1 : 0, ECVF_SetByGameSetting);
		}
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PerfPanelMmo")))
		{
			V->Set(UAcSettings::Get().bShowPerfMmo ? 1 : 0, ECVF_SetByGameSetting);
		}
	}
	if (FSlateApplication::IsInitialized())
	{
		Keys = MakeShared<FAcFlowKeys>(this);
		FSlateApplication::Get().RegisterInputPreProcessor(Keys);
	}
}

void UAcGameFlowSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const TCHAR* Cmd = FCommandLine::Get();
	FString Wanted;
	if (FParse::Param(Cmd, TEXT("AcMmo")))
	{
		// Straight into the MMORPG test (hidden test runs).
		bMmoWhenReady = true;
		bMmoNewWorldWhenReady = FParse::Param(Cmd, TEXT("AcMmoNewWorld"));
	}
	else if (FParse::Value(Cmd, TEXT("AcNewGameDialog="), Wanted, false) || FParse::Param(Cmd, TEXT("AcNewGameDialog")))
	{
		bOpenWhenReady = true;
		OpenMessage = TEXT("Pick a map and how big it is.");
		TArray<FString> Parts;
		Wanted.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() > 0)
		{
			if (const std::optional<ac::MapChoice> Map = UAcSimSubsystem::ParseMap(Parts[0]))
			{
				FAcNewGameChoice C = CurrentChoice();
				C.Map = *Map;
				C.Teams = Parts.Num() > 1 ? Parts[1] : SAcNewGame::TeamOptions(Map->players)[0];
				OpenChoice = C;
			}
		}
		if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this))
		{
			Shot->Hold();
			bShotHeld = true;
		}
	}
	else if (const AAcGameMode* RunMode = InWorld.GetAuthGameMode<AAcGameMode>(); !Test && !FParse::Param(Cmd, TEXT("AcNoHome"))
		&& (FParse::Param(Cmd, TEXT("AcHome")) || FParse::Value(Cmd, TEXT("AcHomePlay="), HomePlayAfter) || (RunMode && RunMode->GetRunMode() == EAcRunMode::Game && !FParse::Param(Cmd, TEXT("unattended")))))
	{
		// The launcher: the home screen over the paused game (-AcHome for
		// headless shots; -AcNoHome straight into the game).
		bHomeWhenReady = true;
	}
	else if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->NeedsMapChoice())
	{
		const AAcGameMode* Mode = InWorld.GetAuthGameMode<AAcGameMode>();
		if (!Mode || Mode->GetRunMode() == EAcRunMode::Game)
		{
			// Swift `startWindowMode`: no map remembered, so ask.
			bOpenWhenReady = true;
			OpenMessage = TEXT("Pick a map and how big it is.");
		}
	}
}

void UAcGameFlowSubsystem::Deinitialize()
{
	if (Keys && FSlateApplication::IsInitialized()) FSlateApplication::Get().UnregisterInputPreProcessor(Keys);
	Keys.Reset();
	CloseNewGame();
	CloseMenu();
	CloseWorldMap();
	CloseSkillScreen();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	CloseJournalScreen();
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		Sim->OnGameStarted.Remove(StartedHandle);
		Sim->OnRefused.Remove(RefusedHandle);
	}
	if (MmoBanner)
	{
		if (const AAcHUD* Hud = AAcHUD::Get(this); Hud && Hud->Root()) Hud->Root()->RemoveLayer(MmoBanner.ToSharedRef());
		MmoBanner.Reset();
	}
	if (UAcSettings::Remembers())
	{
		if (const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PerfPanel")))
		{
			UAcSettings::Get().bShowPerf = V->GetInt() != 0;
		}
		if (const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.PerfPanelMmo")))
		{
			UAcSettings::Get().bShowPerfMmo = V->GetInt() != 0;
		}
		UAcSettings::Store();
	}
	Super::Deinitialize();
}

void UAcGameFlowSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bOpenWhenReady && OpenNewGame(OpenMessage, OpenChoice)) bOpenWhenReady = false;
	if (bMmoWhenReady)
	{
		const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
		if (Sim && Sim->IsRunning() && AAcHUD::Get(this))
		{
			bMmoWhenReady = false;
			StartMmo(bMmoNewWorldWhenReady);
		}
	}
	TickMmo();
	if (bHomeWhenReady)
	{
		const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
		if (Sim && Sim->IsRunning())
		{
			bHasGame = bHasGame || Sim->WasResumed() || HomePlayAfter >= 0;
			if (OpenMenu(EAcMenuMode::Home))
			{
				bHomeWhenReady = false;
				HomeShownAt = FApp::GetCurrentTime();
				if (FParse::Param(FCommandLine::Get(), TEXT("AcPauseMenu"))) Menu->SetMode(EAcMenuMode::Pause);
				// -AcSettingsMenu: its Settings page (stills).
				if (FParse::Param(FCommandLine::Get(), TEXT("AcSettingsMenu")))
				{
					Menu->OpenSettings();
				}
			}
		}
	}
	if (HomePlayAfter >= 0 && Menu && Menu->Mode() == EAcMenuMode::Home && FApp::GetCurrentTime() - HomeShownAt >= HomePlayAfter)
	{
		// -AcHomePlay: RESUME GAME, as a click on it would.
		HomePlayAfter = -1;
		UE_LOG(LogAutocraft, Log, TEXT("menu: -AcHomePlay presses RESUME GAME"));
		OnMenuPick(EAcMenuAction::Resume);
	}
	if (Menu && Menu->Mode() == EAcMenuMode::Home)
	{
		// The home screen's song line (the music player ticks while paused).
		const UAcMusicPlayer* Music = GetWorld() && GetWorld()->GetGameInstance() ? GetWorld()->GetGameInstance()->GetSubsystem<UAcMusicPlayer>() : nullptr;
		const TOptional<FAcNowPlaying> Np = Music ? Music->NowPlaying() : TOptional<FAcNowPlaying>();
		Menu->SetNowPlaying(Np && !Np->bAd ? Np->Title : FString());
	}
	if (bShotHeld && Dialog && Dialog->PreviewReady())
	{
		bShotHeld = false;
		if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this)) Shot->Release();
	}
	if (Test) TickFlowTest();
}

// MARK: - The game

FAcNewGameChoice UAcGameFlowSubsystem::CurrentChoice() const
{
	FAcNewGameChoice C;
	const UAcSettings& S = UAcSettings::Get();
	C.bAI = S.bNewGameAI;
	C.bFog = S.bNewGameFog;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (Sim && Sim->IsRunning() && Sim->Choice())
	{
		C.Map = *Sim->Choice();
		C.Teams = AcSaves::TeamsText(Sim->State().teams());
		if (C.Map.players == 2) C.Teams = TEXT("1v1");
		C.bAI = Sim->IsAIOn();
		C.bFog = Sim->IsFogOn();
	}
	else if (const std::optional<ac::MapChoice> Last = UAcSimSubsystem::ParseMap(S.LastMap))
	{
		C.Map = *Last;
		C.Teams = S.LastTeams;
	}
	if (!SAcNewGame::TeamOptions(C.Map.players).Contains(C.Teams)) C.Teams = SAcNewGame::TeamOptions(C.Map.players)[0];
	return C;
}

void UAcGameFlowSubsystem::OnGameStarted(UAcSimSubsystem& Sim)
{
	Remember(Sim);
	if (Test) Test->GamesStarted += 1;
}

void UAcGameFlowSubsystem::Remember(const UAcSimSubsystem& Sim)
{
	if (!UAcSettings::Remembers() || Sim.IsFixture() || !Sim.Choice() || Sim.IsMmo()) return;
	UAcSettings& S = UAcSettings::Get();
	const FString Map = AcSaves::ShortName(*Sim.Choice());
	const FString Teams = AcSaves::TeamsText(Sim.State().teams());
	if (S.LastMap == Map && S.LastTeams == Teams) return;
	S.LastMap = Map;
	S.LastTeams = Teams;
	UAcSettings::Store();
}

void UAcGameFlowSubsystem::LeaveDriving()
{
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this); Pilot && Pilot->Driving()) Pilot->Leave();
}

void UAcGameFlowSubsystem::StartNewGame(const FAcNewGameChoice& Choice)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim) return;
	CloseNewGame();
	LeaveDriving();
	UE_LOG(LogAutocraft, Log, TEXT("new game: %s, %s, AI %s, fog %s"), *AcSaves::Title(Choice.Map), *SAcNewGame::TeamTitle(Choice.Teams),
		Choice.bAI ? TEXT("on") : TEXT("off"), Choice.bFog ? TEXT("on") : TEXT("off"));
	UAcSettings& S = UAcSettings::Get();
	S.bNewGameAI = Choice.bAI;
	S.bNewGameFog = Choice.bFog;
	Sim->NewGame(Choice.Map, Choice.Map.players > 2 ? AcSaves::ParseTeams(Choice.Teams) : std::nullopt, Choice.bAI, Choice.bFog);
	if (UAcSettings::Remembers()) UAcSettings::Store();
	bHasGame = true;
	if (Menu)
	{
		// From the home screen: into the new game, running.
		bPausedBefore = false;
		CloseMenu();
	}
}

void UAcGameFlowSubsystem::Restart()
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	if (Sim->IsMmo())
	{
		Sim->Refuse(TEXT("Restart is off in the MMORPG test"));
		return;
	}
	CloseNewGame();
	LeaveDriving();
	Sim->Restart();
}

bool UAcGameFlowSubsystem::Import(const FString& Path)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim) return false;
	if (Sim->IsMmo())
	{
		Sim->Refuse(TEXT("Import is off in the MMORPG test"));
		return false;
	}
	LeaveDriving();
	FString Error;
	if (!Sim->ImportSession(Path, &Error))
	{
		UE_LOG(LogAutocraft, Warning, TEXT("import: %s"), *Error);
		return false;
	}
	CloseNewGame();
	bHasGame = true;
	if (Menu)
	{
		bPausedBefore = false;
		CloseMenu();
	}
	return true;
}

// MARK: - The dialog

bool UAcGameFlowSubsystem::OpenNewGame(const FString& Message, TOptional<FAcNewGameChoice> Initial)
{
	if (Dialog)
	{
		if (Initial) Dialog->SetChoice(*Initial);
		return true;
	}
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->IsMmo())
	{
		Sim->Refuse(TEXT("New game is off in the MMORPG test"));
		return false;
	}
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	FString ImportNote;
	const FString Swift = AcSaves::SwiftWindowFile();
	if (IFileManager::Get().FileExists(*Swift))
	{
		const FDateTime When = IFileManager::Get().GetTimeStamp(*Swift);
		ImportNote = FString::Printf(TEXT("Swift game: %s, saved %s"), *Swift.Replace(*FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("")), TEXT("~/")),
			*When.ToString(TEXT("%Y-%m-%d %H:%M")));
	}
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(NewGameLayer)
	[
		SAssignNew(Dialog, SAcNewGame)
		.Initial(Initial ? *Initial : CurrentChoice())
		.Message(Message)
		.ImportNote(ImportNote)
		.OnStart_Lambda([Weak](const FAcNewGameChoice& C) { if (Weak.IsValid()) Weak->StartNewGame(C); })
		.OnCancel_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseNewGame(); })
		.OnImport_Lambda([Weak]() { if (Weak.IsValid()) Weak->Import(AcSaves::SwiftWindowFile()); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Dialog, EFocusCause::SetDirectly);
	// Modal: the game waits (an alert stops Swift's timer), nothing else takes input.
	BeginModal();
	UE_LOG(LogAutocraft, Log, TEXT("new game: dialog open"));
	return true;
}

void UAcGameFlowSubsystem::CloseNewGame()
{
	if (!Dialog) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(Dialog.ToSharedRef());
	}
	Dialog.Reset();
	EndModal();
}

void UAcGameFlowSubsystem::BeginModal()
{
	if (ModalDepth++ > 0) return;
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		bPausedBefore = Sim->IsPaused();
		Sim->SetPaused(true);
	}
	SetBlocked(true);
}

void UAcGameFlowSubsystem::EndModal()
{
	if (ModalDepth == 0 || --ModalDepth > 0) return;
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this)) Sim->SetPaused(bPausedBefore);
	SetBlocked(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

// MARK: - The launcher (home screen, pause menu)

bool UAcGameFlowSubsystem::OpenMenu(const EAcMenuMode Mode)
{
	if (Menu)
	{
		Menu->SetMode(Mode);
		RefreshMenuGame();
		return true;
	}
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	if (Mode == EAcMenuMode::Home) LeaveDriving();
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(MenuLayer)
	[
		SAssignNew(Menu, SAcMenu)
		.Mode(Mode)
		.OnPick_Lambda([Weak](EAcMenuAction A) { if (Weak.IsValid()) Weak->OnMenuPick(A); })
		.OnSound_Lambda([Weak](EAcMenuAction A)
		{
			// H1: the way into the Reach swells in with the ringshadow's
			// release (J1); every other pick ticks.
			UAcAudioDirector* Audio = Weak.IsValid() ? UAcAudioDirector::Get(Weak.Get()) : nullptr;
			if (!Audio) return;
			if (A == EAcMenuAction::Mmo) Audio->PlayMenu("rsrelease", AcSound::MenuEnterGain);
			else Audio->PlayMenu("pick", AcSound::MenuPickGain);
		})
	];
	RefreshMenuGame();
	BeginModal();
	UE_LOG(LogAutocraft, Log, TEXT("menu: %s"), Mode == EAcMenuMode::Home ? TEXT("home") : TEXT("paused"));
	return true;
}

void UAcGameFlowSubsystem::CloseMenu()
{
	if (!Menu) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(Menu.ToSharedRef());
	}
	Menu.Reset();
	EndModal();
	UE_LOG(LogAutocraft, Log, TEXT("menu: closed"));
}

void UAcGameFlowSubsystem::RefreshMenuGame()
{
	if (!Menu) return;
	// Under ENTER THE REACH: whether a saved world waits (H1).
	Menu->SetReach(IFileManager::Get().FileExists(*FAcMmoClient::SaveFile()) ? TEXT("ASHFALL REACH  ·  YOUR CLAIM WAITS")
	                                                                       : TEXT("ASHFALL REACH  ·  A FRONTIER WORLD ONLINE"));
	// S2: the world's server running on with the game away (home only).
	FString ServerLine;
	if (Menu->Mode() == EAcMenuMode::Home)
	{
		if (const std::optional<ac::net::RunFile> Running = FAcMmoClient::RunningServer())
		{
			const FDateTime Since = FDateTime::FromUnixTimestamp(Running->since) + (FDateTime::Now() - FDateTime::UtcNow());
			const bool bToday = Since.GetDate() == FDateTime::Now().GetDate();
			ServerLine = FString::Printf(TEXT("World server running  ·  since %s"),
				*Since.ToString(bToday ? TEXT("%H:%M") : TEXT("%d %b %H:%M")));
		}
	}
	Menu->SetServer(ServerLine);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!bHasGame || !Sim || !Sim->IsRunning() || !Sim->Choice())
	{
		Menu->SetGame(FString(), FString());
		return;
	}
	const FAcNewGameChoice C = CurrentChoice();
	const int32 Seconds = (int32)Sim->GameTime();
	const FString Played = Seconds >= 3600 ? FString::Printf(TEXT("%d:%02d:%02d"), Seconds / 3600, Seconds / 60 % 60, Seconds % 60)
	                                       : FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
	Menu->SetGame(AcSaves::Title(*Sim->Choice()),
		FString::Printf(TEXT("%s  ·  %s played  ·  AI %s  ·  fog %s"), *SAcNewGame::TeamTitle(C.Teams), *Played, C.bAI ? TEXT("on") : TEXT("off"),
			C.bFog ? TEXT("on") : TEXT("off")));
}

void UAcGameFlowSubsystem::OnMenuPick(const EAcMenuAction Action)
{
	switch (Action)
	{
	case EAcMenuAction::Resume:
		CloseMenu();
		break;
	case EAcMenuAction::NewGame:
		OpenNewGame();
		break;
	case EAcMenuAction::QuitToHome:
		if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->IsMmo()) StopMmo();
		else if (Sim) Sim->Save();
		bHasGame = true;
		OpenMenu(EAcMenuMode::Home);
		break;
	case EAcMenuAction::Settings:
		// The widget's own page.
		break;
	case EAcMenuAction::Mmo:
		StartMmo();
		break;
	case EAcMenuAction::MmoNewWorld:
		// Confirmed on the menu's own page.
		StartMmo(true);
		break;
	case EAcMenuAction::StopServer:
	{
		// S2: it saves and ends; the next ENTER THE REACH starts it again.
		FString Why;
		if (!FAcMmoClient::StopWorldServer(FAcMmoClient::SaveFile(), &Why))
		{
			UE_LOG(LogAutocraft, Warning, TEXT("menu: STOP: %s"), *Why);
		}
		else
		{
			UE_LOG(LogAutocraft, Log, TEXT("menu: STOP the world server"));
		}
		RefreshMenuGame();
		break;
	}
	case EAcMenuAction::Back:
		// The widget's own page.
		break;
	case EAcMenuAction::QuitApp:
		// The sim saves on the way out (Deinitialize).
		FPlatformMisc::RequestExit(false, TEXT("AcMenu"));
		break;
	}
}

bool UAcGameFlowSubsystem::WantsPauseOnEscape() const
{
	const UWorld* World = GetWorld();
	const AAcGameMode* Mode = World ? World->GetAuthGameMode<AAcGameMode>() : nullptr;
	if (!Mode || Mode->GetRunMode() != EAcRunMode::Game || Dialog || Menu) return false;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsRunning()) return false;
	// Escape's other jobs come first: shut the map, drop the selection, leave the unit.
	if (const UAcCommandMapSubsystem* Map = UAcCommandMapSubsystem::Get(this); Map && Map->IsOpen()) return false;
	if (const UAcPointer* Pointer = UAcPointer::Get(this); Pointer && Pointer->Selected().IsSet()) return false;
	if (const AAcPilotPawn* Pilot = AAcPilotPawn::Find(this); Pilot && Pilot->Driving()) return false;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->ViewportConsole && GEngine->GameViewport->ViewportConsole->ConsoleActive())
	{
		return false;
	}
	return true;
}

void UAcGameFlowSubsystem::SetBlocked(const bool bBlocked)
{
	if (UAcPointer* Pointer = UAcPointer::Get(this)) Pointer->SetBlocked(bBlocked);
	if (AAcRtsPawn* Pawn = AAcRtsPawn::Get(this)) Pawn->SetInputBlocked(bBlocked);
	if (UAcCommandsSubsystem* Commands = UAcCommandsSubsystem::Get(this)) Commands->SetBlocked(bBlocked);
}

// MARK: - The MMORPG test mode

void UAcGameFlowSubsystem::StartMmo(const bool bNewWorld)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	CloseNewGame();
	LeaveDriving();
	if (Menu)
	{
		// From the home screen: into the mode, running.
		bPausedBefore = false;
		CloseMenu();
	}
	UE_LOG(LogAutocraft, Log, TEXT("menu: %s on the first world"), bNewWorld ? TEXT("NEW WORLD") : TEXT("ENTER THE REACH"));
	Sim->StartMmo(bNewWorld);
	TickMmo();
}

void UAcGameFlowSubsystem::StopMmo()
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo()) return;
	CloseWorldMap();
	CloseSkillScreen();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	CloseJournalScreen();
	LeaveDriving();
	Sim->StopMmo();
	TickMmo();
}

void UAcGameFlowSubsystem::OnRefused(const FString& Why)
{
	TickMmo();
	if (MmoBanner) MmoBanner->AddNote(Why, FPlatformTime::Seconds());
}

void UAcGameFlowSubsystem::TickMmo()
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (Mmo && !MmoBanner && Root)
	{
		Root->AddLayer(MenuLayer + 1)[SAssignNew(MmoBanner, SAcMmoBanner)];
	}
	TickWorldMap();
	TickSkillScreen();
	TickRigScreen();
	TickCitadelScreen();
	TickTurretCard();
	TickJournalScreen();
	TickMissions();
	if (!MmoBanner) return;
	MmoBanner->Update(FPlatformTime::Seconds());
	if (Mmo)
	{
		MmoBanner->SetStatus(Mmo->Message(), Mmo->Phase() == FAcMmoClient::EPhase::Failed);
		// Where the pilot is: the region, or the flight.
		FString Place;
		if (const ac::net::WorldMessage::Region* To = Mmo->FlyingTo())
		{
			Place = FString::Printf(TEXT("Flying to %s…"), *Text(To->name));
		}
		else if (const ac::net::WorldMessage::Region* Here = Mmo->Here(); Here && Mmo->Phase() == FAcMmoClient::EPhase::Playing)
		{
			Place = FString::Printf(TEXT("%s  ·  %s"), *Text(Here->name), *Text(ac::title(Here->style)));
		}
		// The world map says where the pilot is in its own header.
		if (WorldMap) Place.Reset();
		MmoBanner->SetPlace(Place, Mmo->FlyingTo() != nullptr);
		// Arrived in a region (the start, a landing once the ship is down,
		// an edge crossing): its title over the view, once per arrival.
		const AAcPilotPawn* Pawn = AAcPilotPawn::Find(this);
		if (const ac::net::WorldMessage::Region* Here = Mmo->Here(); Here && Mmo->Phase() == FAcMmoClient::EPhase::Playing
			&& !Mmo->Travelling() && !(Pawn && Pawn->InFlight()) && Mmo->Visit() != MmoTitledVisit)
		{
			MmoTitledVisit = Mmo->Visit();
			MmoBanner->ShowTitle(Text(Here->name), Text(ac::title(Here->style)), FPlatformTime::Seconds());
		}
		// Small: the round trip only when it is high enough to feel (on this
		// machine it is mostly frame time), and the lag set on it (ac.MmoLag).
		FString Stats;
		bool bSlow = false;
		if (Mmo->Phase() == FAcMmoClient::EPhase::Playing)
		{
			const std::optional<double> Rtt = Mmo->RoundTrip();
			bSlow = Rtt && *Rtt > 0.12;
			if (bSlow) Stats = FString::Printf(TEXT("RTT %.0f ms"), *Rtt * 1000);
			const ac::net::LagSettings& Lag = Mmo->Lag();
			if (Lag.delay > 0 || Lag.jitter > 0 || Lag.loss > 0)
			{
				if (!Stats.IsEmpty()) Stats += TEXT("  ·  ");
				Stats += FString::Printf(TEXT("lag %.0f ms (+%.0f jitter) each way, %.0f%% lost"), Lag.delay * 1000, Lag.jitter * 1000, Lag.loss * 100);
			}
		}
		MmoBanner->SetStats(Stats, bSlow);
	}
	else
	{
		// Out of the mode: the banner goes.
		if (Root) Root->RemoveLayer(MmoBanner.ToSharedRef());
		MmoBanner.Reset();
		MmoTitledVisit = -1;
	}
}

// MARK: - The MMORPG world map

bool UAcGameFlowSubsystem::OpenWorldMap()
{
	if (WorldMap) return true;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	CloseSkillScreen();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	CloseJournalScreen();
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(WorldMapLayer)
	[
		SAssignNew(WorldMap, SAcWorldMap)
		.OnFly_Lambda([Weak](const FString& Region, const FString& Pad) { if (Weak.IsValid()) Weak->FlyFromMap(Region, Pad); })
		.OnTarget_Lambda([Weak](const ac::net::atlas::Item& It) { if (Weak.IsValid()) Weak->TargetFromMap(It); })
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseWorldMap(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(WorldMap, EFocusCause::SetDirectly);
	// The server goes on (nothing pauses); the world takes no clicks, the
	// pilot's unit stands and the pointer is the map's.
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	TickWorldMap();
	UE_LOG(LogAutocraft, Log, TEXT("world map: open"));
	return true;
}

void UAcGameFlowSubsystem::CloseWorldMap()
{
	if (!WorldMap) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(WorldMap.ToSharedRef());
	}
	WorldMap.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("world map: closed"));
}

void UAcGameFlowSubsystem::ToggleWorldMap()
{
	if (WorldMap) CloseWorldMap();
	else OpenWorldMap();
}

void UAcGameFlowSubsystem::FlyFromMap(const FString& Region, const FString& Pad)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo) return;
	FString Why;
	if (!Mmo->Fly(Region, Pad, &Why))
	{
		if (WorldMap) WorldMap->SetRefusal(Why);
		return;
	}
	// Asked: the banner says "Flying to …" once the server lifts off.
	CloseWorldMap();
}

void UAcGameFlowSubsystem::TargetFromMap(const ac::net::atlas::Item& It)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::WorldMessage* W = Mmo ? Mmo->World() : nullptr;
	if (!W || It.region < 0 || It.region >= (int64)W->regions.size()) return;
	const ac::net::atlas::Place* P = nullptr;
	const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(*W, {});
	P = ac::net::atlas::placeOf(Places, It.region);
	if (!P) return;
	FAcMmoClient::FMapTarget T;
	T.Region = UTF8_TO_TCHAR(P->id.c_str());
	T.At = P->toGround(It.at);
	const bool bMission = It.kind == ac::net::atlas::Kind::mission;
	T.Key = bMission ? FString::Printf(TEXT("mission:%lld"), (long long)It.ref) : FString(UTF8_TO_TCHAR(It.key.c_str()));
	// Clicked again: no target.
	if (const TOptional<FAcMmoClient::FMapTarget>& Was = Mmo->GetMapTarget(); Was && Was->Key == T.Key)
	{
		Mmo->SetMapTarget(NullOpt);
		UE_LOG(LogAutocraft, Log, TEXT("world map: no compass target"));
		return;
	}
	T.Label = FString(UTF8_TO_TCHAR(It.title.c_str())).ToUpper();
	T.Color = bMission ? AcMissions::TierColor(It.tier) : FAcHudStyle::LevelGold();
	Mmo->SetMapTarget(T);
	UE_LOG(LogAutocraft, Log, TEXT("world map: the compass points at %s in %s"), *T.Key, *T.Region);
}

void UAcGameFlowSubsystem::TickWorldMap()
{
	if (!WorldMap) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo)
	{
		CloseWorldMap();
		return;
	}
	FAcWorldMapInfo Info;
	if (const ac::net::WorldMessage* W = Mmo->World()) Info.World = *W;
	if (const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad())
	{
		Info.NearPad = Near->Name;
		Info.NearDistance = Near->Distance;
		Info.PadReach = Near->Reach;
		Info.bOwnPad = Near->Building.has_value();
	}
	else Info.PadReach = Mmo->PadReachNow();
	Info.RingwalkerIn = Mmo->RingwalkerIn();
	// What each flight costs (F1), and the wallet's MH.
	if (const ac::net::WorldMessage* W = Mmo->World())
	{
		for (const ac::net::WorldMessage::Region& G : W->regions)
		{
			if (const std::optional<int64> Price = Mmo->FlightPrice(Text(G.id))) Info.FlightPrice[Text(G.id)] = *Price;
		}
	}
	Info.Hydrogen = Mmo->HydrogenNow();
	if (const ac::net::WorldMessage::Region* To = Mmo->FlyingTo()) Info.FlyingTo = Text(To->name);
	// M2: the satellite map's overlays, from the mirror and the progression.
	const ac::net::ProgressMessage* Progress = Mmo->Progress();
	if (Progress) Info.Seen = Progress->regionsSeen;
	if (const ac::net::WorldMessage* W = Mmo->World())
	{
		const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(*W, Info.Seen);
		ac::net::atlas::Input In;
		In.world = W;
		In.progress = Progress;
		const ac::net::Mirror* M = Mmo->Mirror();
		In.here = M && !Mmo->Travelling() ? &M->simulation() : nullptr;
		In.driven = Mmo->Driven();
		In.sky = Mmo->Sky();
		In.sites = AcProgressHud::AtlasSites(*Mmo);
		for (const FAcWorldMapInfo::FMark& K : AcProgressHud::ContractMarks(*Mmo))
		{
			const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, std::string(TCHAR_TO_UTF8(*K.Region)));
			if (!P) continue;
			ac::net::atlas::ContractMark C{P->id, std::nullopt, TCHAR_TO_UTF8(*K.Title)};
			if (K.At) C.at = ac::Vec2(K.At->X * P->width, K.At->Y * P->depth);
			In.contracts.push_back(C);
		}
		Info.Items = ac::net::atlas::items(In, Places);
		// The ringshadow band over every region it crosses now.
		if (In.sky)
		{
			for (const ac::net::atlas::Place& P : Places)
			{
				const ac::GroundRect Ground{-P.width / 2, -P.depth / 2, P.width / 2, P.depth / 2};
				if (const std::optional<ac::Ringshadow::Band> B = ac::Ringshadow::band(In.sky->time, Ground, In.sky->dayLength))
				{
					if (std::vector<ac::Vec2> Poly = ac::net::atlas::band(P, *B); !Poly.empty()) Info.Bands.push_back(std::move(Poly));
				}
			}
		}
	}
	if (const TOptional<FAcMmoClient::FMapTarget>& T = Mmo->GetMapTarget()) Info.Target = T->Key;
	WorldMap->SetInfo(Info);
	if (!Mmo->FlightRefusal().IsEmpty()) WorldMap->SetRefusal(Mmo->FlightRefusal());
}

// MARK: - The MMORPG skill screen

bool UAcGameFlowSubsystem::OpenSkillScreen()
{
	if (SkillScreen) return true;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	CloseWorldMap();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	CloseJournalScreen();
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	// Each ask goes to the server; a refusal at once (not connected) shows here,
	// the server's own through `TickSkillScreen`.
	auto Ask = [Weak](TFunctionRef<bool(FAcMmoClient&, FString*)> Do)
	{
		if (!Weak.IsValid()) return;
		UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
		FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
		FString Why;
		if (!Mmo) Why = TEXT("Not in a world");
		else if (Do(*Mmo, &Why)) return;
		if (Weak->SkillScreen) Weak->SkillScreen->SetRefusal(Why);
	};
	Root->AddLayer(SkillScreenLayer)
	[
		SAssignNew(SkillScreen, SAcSkillScreen)
		.OnSpend_Lambda([Ask](ac::Skill Skill) { Ask([Skill](FAcMmoClient& M, FString* Why) { return M.Spend(Skill, Why); }); })
		.OnUndo_Lambda([Ask](ac::Skill Skill) { Ask([Skill](FAcMmoClient& M, FString* Why) { return M.Undo(Skill, Why); }); })
		.OnSlot_Lambda([Ask](int32 Slot, std::optional<ac::Skill> Skill)
			{ Ask([Slot, Skill](FAcMmoClient& M, FString* Why) { return M.Slot(Slot, Skill, Why); }); })
		.OnRespec_Lambda([Ask]() { Ask([](FAcMmoClient& M, FString* Why) { return M.Respec(Why); }); })
		.OnRig_Lambda([Weak]() { if (Weak.IsValid()) Weak->OpenRigScreen(); })
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseSkillScreen(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(SkillScreen, EFocusCause::SetDirectly);
	// As the world map: the world goes on, the pilot's unit stands, the pointer is free.
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	SkillRefusalShown = Sim->MmoClient() ? Sim->MmoClient()->SkillRefusal() : FString();
	TickSkillScreen();
	UE_LOG(LogAutocraft, Log, TEXT("skill screen: open"));
	return true;
}

void UAcGameFlowSubsystem::CloseSkillScreen()
{
	if (!SkillScreen) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(SkillScreen.ToSharedRef());
	}
	SkillScreen.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("skill screen: closed"));
}

void UAcGameFlowSubsystem::ToggleSkillScreen()
{
	if (SkillScreen) CloseSkillScreen();
	else OpenSkillScreen();
}

void UAcGameFlowSubsystem::TickSkillScreen()
{
	if (!SkillScreen) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo)
	{
		CloseSkillScreen();
		return;
	}
	FAcSkillScreenInfo Info;
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		ac::ProspectorProgress G;
		G.xp = Mmo->Xp().value_or(P->xp);
		G.ranks = P->ranks;
		G.slots = P->slots;
		G.blueprints = P->blueprints;
		G.sites = P->sitesFound;
		G.rig = P->rig;
		// The points that can still be taken back, and for how long (world time).
		const double Now = Mmo->Sky() ? Mmo->Sky()->time : 0.0;
		for (const ac::net::ProgressMessage::Undoable& U : P->undo)
		{
			G.recent.push_back({U.skill, U.until - ac::Progression::undoWindow});
			if (U.until > Now) Info.UndoLeft[U.skill] = FMath::Max(Info.UndoLeft[U.skill], U.until - Now);
		}
		AcProgressHud::BlueprintSites(*Mmo, Info.Sources, Info.SitesMarked);
		Info.RespecPrice = P->respecPrice;
		Info.bRespecFree = P->respecFree;
		// Blueprint savant: 2 levels a rank.
		Info.FloorCut = 2 * G.rank(ac::Skill::blueprintSavant);
		Info.Progress = MoveTemp(G);
		// The server checks the place; the screen says so ahead: the pilot's
		// own finished Citadel, where it unloads.
		bool bHome = false;
		if (const ac::net::Mirror* M = Mmo->Mirror(); M && M->driven() && !Mmo->Travelling())
		{
			if (const ac::Unit* U = M->unit(*M->driven()))
			{
				for (const ac::Structure& B : M->simulation().state.structures)
				{
					if (B.kind == ac::StructureKind::citadel && B.complete() && B.owner == U->owner
						&& ac::NavGrid::insideStructure(U->position, B, ac::Simulation::pilotDepositReach))
					{
						bHome = true;
						break;
					}
				}
			}
		}
		if (!bHome) Info.RespecWhyNot = TEXT("Respec at your Citadel: stand where you unload");
	}
	SkillScreen->SetInfo(Info);
	if (Mmo->SkillRefusal() != SkillRefusalShown)
	{
		SkillRefusalShown = Mmo->SkillRefusal();
		if (!SkillRefusalShown.IsEmpty()) SkillScreen->SetRefusal(SkillRefusalShown);
	}
}

// MARK: - The MMORPG rig screen

bool UAcGameFlowSubsystem::OpenRigScreen()
{
	if (RigScreen) return true;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	CloseWorldMap();
	CloseSkillScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	CloseJournalScreen();
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	// As the skill screen's: a refusal at once here, the server's own
	// through `TickRigScreen`.
	auto Ask = [Weak](TFunctionRef<bool(FAcMmoClient&, FString*)> Do)
	{
		if (!Weak.IsValid()) return;
		UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
		FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
		FString Why;
		if (!Mmo) Why = TEXT("Not in a world");
		else if (Do(*Mmo, &Why)) return;
		if (Weak->RigScreen) Weak->RigScreen->SetRefusal(Why);
	};
	Root->AddLayer(RigScreenLayer)
	[
		SAssignNew(RigScreen, SAcRigScreen)
		.OnMake_Lambda([Ask](ac::Module M, int64 Mark) { Ask([M, Mark](FAcMmoClient& C, FString* Why) { return C.Make(M, Mark, Why); }); })
		.OnResearch_Lambda([Ask](ac::Module M, int64 Mark) { Ask([M, Mark](FAcMmoClient& C, FString* Why) { return C.Research(M, Mark, Why); }); })
		.OnFit_Lambda([Ask](ac::Module M, int64 Mark) { Ask([M, Mark](FAcMmoClient& C, FString* Why) { return C.Fit(M, Mark, Why); }); })
		.OnTab_Lambda([Weak](int32 Tab)
			{
				if (!Weak.IsValid() || !Weak->OpenSkillScreen()) return;
				if (Weak->SkillScreen) Weak->SkillScreen->SetTab(Tab == 1 ? SAcSkillScreen::ETab::Blueprints : SAcSkillScreen::ETab::Skills);
			})
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseRigScreen(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(RigScreen, EFocusCause::SetDirectly);
	// As the skill screen: the world goes on, the pilot's unit stands, the pointer is free.
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	RigRefusalShown = Sim->MmoClient() ? Sim->MmoClient()->RigRefusal() : FString();
	TickRigScreen();
	UE_LOG(LogAutocraft, Log, TEXT("rig screen: open"));
	return true;
}

void UAcGameFlowSubsystem::CloseRigScreen()
{
	if (!RigScreen) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(RigScreen.ToSharedRef());
	}
	RigScreen.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("rig screen: closed"));
}

void UAcGameFlowSubsystem::ToggleRigScreen()
{
	if (RigScreen) CloseRigScreen();
	else OpenRigScreen();
}

void UAcGameFlowSubsystem::TickRigScreen()
{
	if (!RigScreen) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo)
	{
		CloseRigScreen();
		return;
	}
	FAcRigScreenInfo Info;
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		Info.Rig = P->rig;
		Info.bFoundry = P->foundry;
		Info.bLab = P->lab;
		Info.Now = Mmo->Sky() ? Mmo->Sky()->time : 0.0;
		// The wallet where the pilot is, and the place: the server checks
		// it; the screen says so ahead (its own Citadel or Hab Dome, where it
		// unloads).
		bool bHome = false;
		if (const ac::net::Mirror* M = Mmo->Mirror())
		{
			const ac::GameState& St = M->simulation().state;
			if (size_t(ac::Pilot::player) < St.players.size())
			{
				Info.Ore = St.players[ac::Pilot::player].ore;
				Info.Hydrogen = St.players[ac::Pilot::player].hydrogen;
			}
			const ac::Unit* U = M->driven() && !Mmo->Travelling() ? M->unit(*M->driven()) : nullptr;
			for (const ac::Structure& B : St.structures)
			{
				if (U && (B.kind == ac::StructureKind::citadel || B.kind == ac::StructureKind::habDome) && B.complete() && B.owner == U->owner
					&& ac::NavGrid::insideStructure(U->position, B, ac::Simulation::pilotDepositReach))
				{
					bHome = true;
					break;
				}
			}
		}
		if (!bHome) Info.FitWhyNot = TEXT("Fit modules at your Citadel or a Hab Dome: stand where you unload");
	}
	RigScreen->SetInfo(Info);
	if (Mmo->RigRefusal() != RigRefusalShown)
	{
		RigRefusalShown = Mmo->RigRefusal();
		if (!RigRefusalShown.IsEmpty()) RigScreen->SetRefusal(RigRefusalShown);
	}
}

// MARK: - The MMORPG Citadel screen

bool UAcGameFlowSubsystem::AtRealmCitadel(FString* Builder) const
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	if (!M || Mmo->Travelling()) return false;
	const ac::GameState& St = M->simulation().state;
	const ac::Unit* U = M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!U) return false;
	// The realm's finished Citadel (R1: every pilot's), where the Prospector unloads.
	for (const ac::Structure& B : St.structures)
	{
		if (B.kind != ac::StructureKind::citadel || !B.complete() || B.owner != U->owner) continue;
		if (!ac::NavGrid::insideStructure(U->position, B, ac::Simulation::pilotDepositReach)) continue;
		if (Builder) *Builder = AcProgressHud::BuiltBy(Mmo, B.id);
		return true;
	}
	return false;
}

bool UAcGameFlowSubsystem::OpenCitadelScreen()
{
	if (CitadelScreen) return true;
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	// Only at the realm's Citadel, where the Prospector unloads (C1).
	if (!AtRealmCitadel())
	{
		Sim->Refuse(TEXT("H opens the realm's Citadel: walk up to it, where you unload"));
		return false;
	}
	CloseWorldMap();
	CloseSkillScreen();
	CloseRigScreen();
	CloseJournalScreen();
	CloseTurretCard();
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(CitadelScreenLayer)
	[
		SAssignNew(CitadelScreen, SAcCitadelScreen)
		.OnDonate_Lambda([Weak](int64 Ore, int64 Hydrogen)
			{
				if (!Weak.IsValid()) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				FString Why;
				if (!Mmo) Why = TEXT("Not in a world");
				else if (Mmo->Donate(Ore, Hydrogen, &Why)) return;
				if (Weak->CitadelScreen) Weak->CitadelScreen->SetRefusal(Why);
			})
		.OnBuy_Lambda([Weak](bool bItem, int32 Index)
			{
				// C2, the shop: the screen's lines are the blueprints' order
				// (`Progression::blueprintKinds`) and the items' (`Shop::items`).
				if (!Weak.IsValid()) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				const auto& Kinds = ac::Progression::blueprintKinds();
				const auto& Items = ac::Shop::items();
				FString Why;
				if (!Mmo) Why = TEXT("Not in a world");
				else if (bItem && Index >= 0 && size_t(Index) < Items.size() && Mmo->BuyItem(Items[size_t(Index)].item, &Why)) return;
				else if (!bItem && Index >= 0 && size_t(Index) < Kinds.size() && Mmo->BuyBlueprint(Kinds[size_t(Index)], &Why)) return;
				if (Weak->CitadelScreen) Weak->CitadelScreen->SetRefusal(Why.IsEmpty() ? FString(TEXT("Nothing there")) : Why);
			})
		.OnTake_Lambda([Weak](int32 Index)
			{
				// K2, the board: the screen's offers are the board's order.
				if (!Weak.IsValid()) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
				FString Why;
				if (!Mmo || !P) Why = TEXT("Not in a world");
				else if (Index >= 0 && size_t(Index) < P->board.size() && Mmo->TakeMission(P->board[size_t(Index)].id, &Why)) return;
				if (Weak->CitadelScreen) Weak->CitadelScreen->SetRefusal(Why.IsEmpty() ? FString(TEXT("No offer there")) : Why);
			})
		.OnMerc_Lambda([Weak](int32 Act, int32 Merc, int32 Field)
			{
				// H2, the mercenaries: the screen's lines are the progress
				// message's order (`mercs`, `mercFields`).
				if (!Weak.IsValid()) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
				using Ask = ac::net::RequestMessage::Merc;
				FString Why;
				Ask A;
				A.act = Act == 0 ? Ask::Act::hire : Act == 1 ? Ask::Act::dismiss : Ask::Act::assign;
				if (!Mmo || !P) Why = TEXT("Not in a world");
				else if (Act != 0 && (Merc < 0 || size_t(Merc) >= P->mercs.size())) Why = TEXT("No mercenary there");
				else if (Act == 2 && (Field < 0 || size_t(Field) >= P->mercFields.size())) Why = TEXT("No field there");
				else
				{
					if (Act != 0) A.merc = P->mercs[size_t(Merc)].id;
					if (Act == 2)
					{
						A.region = P->mercFields[size_t(Field)].region;
						A.field = P->mercFields[size_t(Field)].field;
					}
					if (Mmo->Merc(A, &Why)) return;
				}
				if (Weak->CitadelScreen) Weak->CitadelScreen->SetRefusal(Why);
			})
		.OnUpgrade_Lambda([Weak](int32 Index)
			{
				// D1, the DEFENCE tab: the cards are the stats' order.
				if (!Weak.IsValid()) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				FString Why;
				if (!Mmo) Why = TEXT("Not in a world");
				else if (Index >= 0 && Index < 3 && Mmo->UpgradeCitadel(ac::DefenceStat(Index), &Why)) return;
				if (Weak->CitadelScreen) Weak->CitadelScreen->SetRefusal(Why.IsEmpty() ? FString(TEXT("Nothing there")) : Why);
			})
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseCitadelScreen(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(CitadelScreen, EFocusCause::SetDirectly);
	// As the rig screen: the world goes on, the pilot's unit stands, the pointer is free.
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	const FAcMmoClient* Mmo = Sim->MmoClient();
	CitadelRefusalShown = Mmo ? Mmo->CitadelRefusal() : FString();
	CitadelKeptShown = Mmo ? Mmo->CitadelKept() : 0;
	TickCitadelScreen();
	UE_LOG(LogAutocraft, Log, TEXT("citadel screen: open"));
	return true;
}

void UAcGameFlowSubsystem::CloseCitadelScreen()
{
	if (!CitadelScreen) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(CitadelScreen.ToSharedRef());
	}
	CitadelScreen.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("citadel screen: closed"));
}

void UAcGameFlowSubsystem::ToggleCitadelScreen()
{
	if (CitadelScreen) CloseCitadelScreen();
	else OpenCitadelScreen();
}

void UAcGameFlowSubsystem::TickCitadelScreen()
{
	if (!CitadelScreen) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo)
	{
		CloseCitadelScreen();
	CloseTurretCard();
		return;
	}
	FAcCitadelScreenInfo Info;
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		Info.bKnown = true;
		Info.RealmOre = P->realmWallet.ore;
		Info.RealmHydrogen = P->realmWallet.hydrogen;
		Info.TaxPercent = P->taxPercent;
		Info.TaxedOre = P->taxed.ore;
		Info.TaxedHydrogen = P->taxed.hydrogen;
		Info.DonatedOre = P->donated.ore;
		Info.DonatedHydrogen = P->donated.hydrogen;
	}
	if (const ac::net::Mirror* M = Mmo->Mirror())
	{
		const ac::GameState& St = M->simulation().state;
		if (size_t(ac::Pilot::player) < St.players.size())
		{
			Info.Ore = St.players[ac::Pilot::player].ore;
			Info.Hydrogen = St.players[ac::Pilot::player].hydrogen;
		}
	}
	// C2, the shop: every blueprint (copies held, the next copy's price, its
	// floor) and the four consumables (stacks, prices, keys in the field).
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		const std::optional<ac::ProspectorProgress> G = AcProgressHud::Progress(*Mmo);
		ac::ProspectorProgress Bought;
		Bought.bought = P->bought;
		for (const ac::StructureKind K : ac::Progression::blueprintKinds())
		{
			FAcShopLine X;
			X.Name = UTF8_TO_TCHAR(ac::Simulation::title(K).c_str());
			const auto Held = P->blueprints.find(K);
			X.Held = Held == P->blueprints.end() ? 0 : Held->second;
			const ac::ShopPrice Price = Bought.blueprintPrice(K);
			X.Ore = Price.ore;
			X.Hydrogen = Price.hydrogen;
			if (G && G->level() < G->floor(K)) X.OpensAt = G->floor(K);
			Info.Blueprints.Add(X);
		}
		int32 Key = 1;
		for (const ac::ItemInfo& I : ac::Shop::items())
		{
			FAcShopLine X;
			X.Name = UTF8_TO_TCHAR(I.title);
			X.Note = UTF8_TO_TCHAR(I.text);
			const auto Held = P->items.find(I.item);
			X.Held = Held == P->items.end() ? 0 : Held->second;
			X.Stack = I.stack;
			X.Ore = I.ore;
			X.Hydrogen = I.hydrogen;
			X.Key = FString::FromInt(Key++);
			Info.Items.Add(X);
		}
	}
	// K2, the board: the offers by tier, the missions taken.
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		for (const ac::net::CitadelMission& M : P->board)
		{
			FAcMissionLine X;
			X.Id = M.id;
			X.Tier = int32(M.tier);
			X.Kind = AcMissions::KindName(M.kind);
			X.Where = AcMissions::RegionName(*Mmo, M.region);
			X.Title = UTF8_TO_TCHAR(M.title.c_str());
			X.Instructions = UTF8_TO_TCHAR(M.instructions.c_str());
			X.Reward = AcMissions::Reward(M);
			Info.Offers.Add(X);
		}
		for (const ac::net::CitadelMission& M : P->journal) Info.Taken += M.state == ac::net::CitadelMission::State::taken ? 1 : 0;
	}
	// H2, the mercenaries: what each does, the fields, the terms.
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		using State = ac::net::ProgressMessage::Merc::State;
		auto FieldName = [&](const std::string& Region, int64 Field) {
			return FString::Printf(TEXT("%s · Field %lld"), *AcMissions::RegionName(*Mmo, Region), (long long)(Field + 1));
		};
		const double Now = Mmo->Sky() ? Mmo->Sky()->time : 0.0;
		for (const ac::net::ProgressMessage::Merc& M : P->mercs)
		{
			FAcMercLine X;
			X.Id = M.id;
			X.Name = UTF8_TO_TCHAR(M.name.c_str());
			X.State = int32(M.state);
			X.Region = UTF8_TO_TCHAR(M.region.c_str());
			X.Field = M.field;
			X.Delivered = M.delivered;
			switch (M.state)
			{
			case State::working: X.Doing = TEXT("Mining ") + FieldName(M.region, M.field); break;
			case State::down:
				X.Doing = FString::Printf(TEXT("Down: back at the Citadel, at work in %lld min"),
					(long long)FMath::Max(1.0, FMath::CeilToDouble((M.downUntil - Now) / 60.0)));
				break;
			case State::empty: X.Doing = FieldName(M.region, M.field) + TEXT(" is mined out: send it to another"); break;
			case State::noDepot: X.Doing = TEXT("No realm Citadel to haul to in ") + AcMissions::RegionName(*Mmo, M.region); break;
			}
			Info.Mercs.Add(X);
		}
		for (const ac::net::ProgressMessage::MercField& F : P->mercFields)
		{
			FAcMercField X;
			X.Region = UTF8_TO_TCHAR(F.region.c_str());
			X.Name = FieldName(F.region, F.field);
			X.Field = F.field;
			X.Deposits = F.deposits;
			X.Ore = F.ore;
			Info.MercFields.Add(X);
		}
		Info.MercLimit = P->mercLimit;
		Info.MercFeeOre = P->mercFee.ore;
		Info.MercFeeHydrogen = P->mercFee.hydrogen;
		Info.MercWage = P->mercWage;
		Info.MercRate = P->mercRate;
		Info.MercNextLevel = P->mercNextLevel;
		Info.Level = P->level;
	}
	// D1, the DEFENCE tab: the corner turrets' stats now and with the next
	// mark (the core's `Defence`), the next mark's price.
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		const ac::TurretMarks Marks = P->citadelMarks;
		const ac::UnitStats Now = ac::Defence::apply(ac::UnitStats::citadelGuns, Marks);
		for (int32 I = 0; I < 3; ++I)
		{
			const ac::DefenceStat Stat = ac::DefenceStat(I);
			ac::TurretMarks Up = Marks;
			Up.of(Stat) = FMath::Min<int64>(Up.of(Stat) + 1, ac::Defence::marksMax);
			const ac::UnitStats Next = ac::Defence::apply(ac::UnitStats::citadelGuns, Up);
			auto Value = [&](const ac::UnitStats& W) {
				const double V = Stat == ac::DefenceStat::rate ? 1.0 / W.cooldown : Stat == ac::DefenceStat::damage ? W.damage : W.range;
				return FString::Printf(TEXT("%.1f"), V);
			};
			FAcDefenceLine X;
			X.Name = FString(UTF8_TO_TCHAR(ac::Defence::title(Stat))).ToUpper();
			X.Mark = Marks.of(Stat);
			X.Most = ac::Defence::marksMax;
			X.Now = Value(Now);
			X.Next = Value(Next);
			const ac::ShopPrice Price = ac::Defence::citadelPrice(Stat, X.Mark + 1);
			X.Ore = Price.ore;
			X.Hydrogen = Price.hydrogen;
			Info.Defence.Add(X);
		}
	}
	// The server checks the place; the screen says so ahead.
	if (!AtRealmCitadel(&Info.Builder))
	{
		Info.DefenceWhyNot = TEXT("Upgrade at the realm's Citadel: stand where you unload");
		Info.DonateWhyNot = TEXT("Donate at the realm's Citadel: stand where you unload");
		Info.ShopWhyNot = TEXT("Buy at the realm's Citadel: stand where you unload");
		Info.MissionsWhyNot = TEXT("Sign up at the realm's Citadel: stand where you unload");
		Info.MercsWhyNot = TEXT("Hire at the realm's Citadel: stand where you unload");
	}
	CitadelScreen->SetInfo(Info);
	if (Mmo->CitadelRefusal() != CitadelRefusalShown)
	{
		CitadelRefusalShown = Mmo->CitadelRefusal();
		if (!CitadelRefusalShown.IsEmpty()) CitadelScreen->SetRefusal(CitadelRefusalShown);
	}
	if (Mmo->CitadelKept() != CitadelKeptShown)
	{
		CitadelKeptShown = Mmo->CitadelKept();
		CitadelScreen->SetKept();
	}
}

// MARK: - The Journal (K2)

bool UAcGameFlowSubsystem::OpenJournalScreen()
{
	if (JournalScreen) return true;
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	CloseWorldMap();
	CloseSkillScreen();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseTurretCard();
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(JournalScreenLayer)
	[
		SAssignNew(JournalScreen, SAcJournalScreen)
		.OnPlay_Lambda([Weak](int32 Index)
			{
				if (!Weak.IsValid() || !Weak->JournalScreen) return;
				const TArray<FAcJournalEntry>& E = Weak->JournalScreen->GetInfo().Entries;
				if (!E.IsValidIndex(Index)) return;
				Weak->PlayMissionNarration(E[Index].Narration, false);
				Weak->JournalScreen->SetNote(FString::Printf(TEXT("Playing the briefing: %s."), *E[Index].Title));
			})
		.OnAbandon_Lambda([Weak](int32 Index)
			{
				if (!Weak.IsValid() || !Weak->JournalScreen) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				const TArray<FAcJournalEntry>& E = Weak->JournalScreen->GetInfo().Entries;
				FString Why;
				if (!Mmo) Why = TEXT("Not in a world");
				else if (E.IsValidIndex(Index) && Mmo->AbandonMission(E[Index].Id, &Why)) return;
				Weak->JournalScreen->SetRefusal(Why.IsEmpty() ? FString(TEXT("No mission there")) : Why);
			})
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseJournalScreen(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(JournalScreen, EFocusCause::SetDirectly);
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	TickJournalScreen();
	UE_LOG(LogAutocraft, Log, TEXT("journal: open"));
	return true;
}

void UAcGameFlowSubsystem::CloseJournalScreen()
{
	if (!JournalScreen) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(JournalScreen.ToSharedRef());
	}
	JournalScreen.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("journal: closed"));
}

void UAcGameFlowSubsystem::ToggleJournalScreen()
{
	if (JournalScreen) CloseJournalScreen();
	else OpenJournalScreen();
}

void UAcGameFlowSubsystem::PlayMissionNarration(const FString& Key, const bool bAccepted)
{
	UAcAudioDirector* Audio = UAcAudioDirector::Get(this);
	if (!Audio || Key.IsEmpty()) return;
	if (bAccepted) Audio->PlayMenu(TEXT("missionaccept"), 0.8f);
	if (Audio->PlayNarration(Key))
	{
		NarrationKey = Key;
		NarrationUntil = FPlatformTime::Seconds() + FMath::Max(1.0, Audio->NarrationSeconds(Key));
	}
}

void UAcGameFlowSubsystem::TickMissions()
{
	if (!NarrationKey.IsEmpty() && FPlatformTime::Seconds() > NarrationUntil) NarrationKey.Reset();
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo) return;
	// Just signed up: the sting, and the narration once, as soon as the
	// Journal has it (the reply can come before the progress message).
	const double Now = FPlatformTime::Seconds();
	for (const uint32 Id : Mmo->TakeAccepted()) MissionsHeard.Add({Id, Now});
	const ac::net::ProgressMessage* P = Mmo->Progress();
	for (int32 I = MissionsHeard.Num() - 1; I >= 0; --I)
	{
		const ac::net::CitadelMission* Found = nullptr;
		if (P)
			for (const ac::net::CitadelMission& M : P->journal)
				if (M.id == MissionsHeard[I].Key) Found = &M;
		if (Found)
		{
			UE_LOG(LogAutocraft, Log, TEXT("mission: signed up for %s (%s)"), UTF8_TO_TCHAR(Found->title.c_str()), UTF8_TO_TCHAR(Found->key.c_str()));
			PlayMissionNarration(UTF8_TO_TCHAR(Found->key.c_str()), true);
			MissionsHeard.RemoveAt(I);
		}
		else if (Now - MissionsHeard[I].Value > 10) MissionsHeard.RemoveAt(I);
	}
}

void UAcGameFlowSubsystem::TickJournalScreen()
{
	if (!JournalScreen) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo)
	{
		CloseJournalScreen();
		return;
	}
	FAcJournalInfo Info;
	Info.Most = 3;
	Info.Playing = NarrationKey;
	const ac::net::WorldMessage* W = Mmo->World();
	if (W)
	{
		Info.Columns = int32(W->columns);
		Info.Rows = int32(W->rows);
		Info.Names.SetNum(Info.Columns * Info.Rows);
		for (const ac::net::WorldMessage::Region& R : W->regions)
		{
			const int32 I = int32(R.row) * Info.Columns + int32(R.column);
			if (Info.Names.IsValidIndex(I)) Info.Names[I] = UTF8_TO_TCHAR(R.name.c_str());
		}
	}
	// A region's cell, and a ground point's place in it (0…1).
	auto Cell = [&](const std::string& Id, ac::Vec2 At, int32& Col, int32& Row, FVector2f& Share, double& Width) {
		if (!W) return false;
		for (const ac::net::WorldMessage::Region& R : W->regions)
		{
			if (R.id != Id) continue;
			const ac::MapCells C = ac::cells(R.size);
			Col = int32(R.column);
			Row = int32(R.row);
			Width = double(C.width);
			Share = FVector2f(float(FMath::Clamp((At.x + C.width / 2.0) / C.width, 0.0, 1.0)),
				float(FMath::Clamp((At.y + C.depth / 2.0) / C.depth, 0.0, 1.0)));
			return true;
		}
		return false;
	};
	const ac::net::WorldMessage::Region* Here = Mmo->Here();
	std::optional<ac::Vec2> Me;
	if (const ac::net::Mirror* M = Mmo->Mirror())
	{
		if (const ac::Pilot* Pilot = M->simulation().pilot ? &*M->simulation().pilot : nullptr)
		{
			if (const auto U = M->simulation().state.unit(Pilot->unit)) Me = U->position;
		}
	}
	if (Here && Me)
	{
		double Width = 0;
		Info.bHere = Cell(Here->id, *Me, Info.HereColumn, Info.HereRow, Info.HereAt, Width);
	}
	if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this); Audio && Audio->IsNarrating() == false && FPlatformTime::Seconds() > NarrationUntil)
	{
		Info.Playing.Reset();
	}
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		Info.bKnown = true;
		// L1: KSTR's next goal on top.
		if (const std::optional<ac::net::Goal>& G = P->goal)
		{
			Info.bNext = true;
			Info.NextTitle = UTF8_TO_TCHAR(G->title.c_str());
			Info.NextWhy = UTF8_TO_TCHAR(G->why.c_str());
			Info.NextHint = UTF8_TO_TCHAR(G->hint.c_str());
			Info.NextPitch = UTF8_TO_TCHAR(G->pitch.c_str());
			if (G->xp > 0) Info.NextPay = FString::Printf(TEXT("+%lld XP"), (long long)std::llround(G->xp));
			if (G->hydrogen > 0) Info.NextPay += FString::Printf(TEXT("  ·  +%lld MH"), (long long)G->hydrogen);
			if (G->marked)
			{
				const FString Where = AcMissions::RegionName(*Mmo, G->region);
				Info.NextWhere = Here && Here->id == G->region ? FString::Printf(TEXT("Marked here, in %s."), *Where)
															   : FString::Printf(TEXT("Marked in %s, on your map."), *Where);
			}
		}
		const UAcAudioDirector* Audio = UAcAudioDirector::Get(this);
		for (const ac::net::CitadelMission& M : P->journal)
		{
			FAcJournalEntry E;
			E.Id = M.id;
			E.Title = UTF8_TO_TCHAR(M.title.c_str());
			E.Kind = AcMissions::KindName(M.kind);
			E.Tier = int32(M.tier);
			E.Where = AcMissions::RegionName(*Mmo, M.region);
			E.Briefing = UTF8_TO_TCHAR(M.briefing.c_str());
			E.Instructions = UTF8_TO_TCHAR(M.instructions.c_str());
			E.Progress = AcMissions::Progress(M);
			E.Share = AcMissions::Share(M);
			E.bDone = M.state == ac::net::CitadelMission::State::done;
			E.Reward = AcMissions::Reward(M);
			E.Narration = UTF8_TO_TCHAR(M.key.c_str());
			E.Seconds = Audio ? Audio->NarrationSeconds(E.Narration) : 0;
			double Width = 0;
			E.bMarked = Cell(M.region, M.at, E.Column, E.Row, E.At, Width);
			E.bWholeRegion = M.kind == ac::net::CitadelMission::Kind::hunt;
			E.Ring = Width > 0 ? float(M.radius / Width) : 0.f;
			// Where it is from here.
			if (Here && Me && Here->id == M.region && !E.bWholeRegion)
			{
				const ac::Vec2 D = M.at - *Me;
				// A cell is a metre (as the starfall's warning).
				const double Metres = std::sqrt(D.x * D.x + D.y * D.y);
				E.Bearing = FString::Printf(TEXT("Here, %s: %d m %s."), *E.Where, int32(FMath::RoundToDouble(Metres)),
					*AcMissions::Heading(D));
			}
			else if (Here && Here->id == M.region)
			{
				E.Bearing = FString::Printf(TEXT("Here, anywhere in %s."), *E.Where);
			}
			else if (E.bMarked)
			{
				E.Bearing = FString::Printf(TEXT("In %s: leave %s to get there."), *E.Where,
					Here ? *FString(UTF8_TO_TCHAR(Here->name.c_str())) : TEXT("here"));
			}
			Info.Entries.Add(E);
		}
	}
	JournalScreen->SetInfo(Info);
}

// MARK: - -AcFlowTest

void UAcGameFlowSubsystem::Check(const bool bOk, const FString& What)
{
	const FString Step = Test->Steps.IsValidIndex(Test->Step) ? Test->Steps[Test->Step] : TEXT("?");
	UE_LOG(LogAutocraft, Display, TEXT("flowtest: %s: %s %s"), *Step, *What, bOk ? TEXT("ok") : TEXT("FAILED"));
	if (!bOk) Test->Failures += 1;
}

void UAcGameFlowSubsystem::NextStep()
{
	Test->Step += 1;
	Test->Phase = 0;
	Test->PhaseAt = FPlatformTime::Seconds();
}

void UAcGameFlowSubsystem::TickFlowTest()
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsRunning()) return;
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	if (T.PhaseAt == 0) T.PhaseAt = Now;
	if (T.Step >= T.Steps.Num())
	{
		if (T.Step == T.Steps.Num())
		{
			UE_LOG(LogAutocraft, Display, TEXT("flowtest: done, %d failed"), T.Failures);
			T.Step += 1;
			FPlatformMisc::RequestExit(false, TEXT("AcFlowTest"));
		}
		return;
	}
	const FString Step = T.Steps[T.Step].TrimStartAndEnd();
	if (Step == TEXT("mmokeys") || Step == TEXT("mmohud"))
	{
		TickMmoHudFlow(Step);
		return;
	}
	if (Step == TEXT("mmoshots"))
	{
		TickMmoShots();
		return;
	}
	if (Step.StartsWith(TEXT("mmo")))
	{
		TickMmoFlow(Step);
		return;
	}
	const ac::GameState& S = Sim->State();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; T.GameAt = S.time; };
	auto SavedEqual = [&](const ac::Session& Want, const FString& What) {
		// The file holds exactly the session's JSON, and reads back to it
		// (dates are whole seconds in JSON, so compare the encodings).
		const std::optional<std::string> File = ac::readFile(std::string(TCHAR_TO_UTF8(*WindowFile())));
		const std::optional<ac::Session> Disk = ReadSession(WindowFile());
		const std::string A = ac::SessionStore::encode(Want);
		const bool bBytes = File && *File == A;
		const bool bBack = Disk && ac::SessionStore::encode(*Disk) == A && Disk->state == Want.state;
		Check(bBytes && bBack, FString::Printf(TEXT("%s: %s (%d bytes, game %s at %.2f s, %d units, %d buildings)"), *What,
			bBytes && bBack ? TEXT("the file is the session's JSON and reads back to the same state")
			: bBytes ? TEXT("the file is the session's JSON but reads back different") : TEXT("the file differs"), File ? (int32)File->size() : 0,
			*Text(Want.game.value_or("")), Want.state.time, (int32)Want.state.units.size(), (int32)Want.state.structures.size()));
	};

	if (Step == TEXT("save"))
	{
		// A few seconds of play, a save, the file read back.
		if (T.Phase == 0) { Advance(); return; }
		if (T.Phase == 1 && S.time - T.GameAt >= 3)
		{
			Check(Sim->Save(), TEXT("Save() queued on a worker"));
			T.Expected = Sim->Session();
			Advance();
			return;
		}
		if (T.Phase == 2 && Since() >= 1.0)
		{
			SavedEqual(*T.Expected, TEXT("save → load"));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("resume"))
	{
		// The window game on disk before launch is the one being played.
		const ac::Session& Now2 = Sim->Session();
		Check(T.Before.has_value(), TEXT("a saved window game was on disk"));
		if (T.Before)
		{
			Check(Now2.game == T.Before->game && Now2.mapName == T.Before->mapName,
				FString::Printf(TEXT("resumed game %s on %s"), *Text(Now2.game.value_or("")), *Text(Now2.mapName)));
			// What is played is the file's game as a fresh sim takes it in
			// (building a sim refreshes intel and re-plans walking units'
			// paths, so compare against that, not the raw file).
			const ac::Simulation Fresh(T.Before->state, Sim->Map(), {}, Sim->IsFogOn());
			const std::string Was = ac::SessionStore::encode(Fresh.state), Is = ac::SessionStore::encode(S);
			Check(Was == Is && S.time == T.Before->state.time,
				FString::Printf(TEXT("its state as saved, through the sim (time %.2f s, %d units, %d bytes)"), T.Before->state.time,
				(int32)T.Before->state.units.size(), (int32)Was.size()));
			if (Was != Is)
			{
				size_t At = 0;
				while (At < Was.size() && At < Is.size() && Was[At] == Is[At]) ++At;
				const size_t From = At > 200 ? At - 200 : 0;
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: resume: differs at %d: was ...%s... is ...%s..."), (int32)At,
					*Text(Was.substr(From, 300)), *Text(Is.substr(From, 300)));
			}
			Check(S.time >= T.Before->state.time, FString::Printf(TEXT("play goes on from %.2f s (now %.2f s)"), T.Before->state.time, S.time));
		}
		NextStep();
		return;
	}
	if (Step == TEXT("import"))
	{
		// -AcImport=PATH: the Swift game as the window game, then saved as ours.
		if (T.Phase == 0)
		{
			const ac::Session& Sess = Sim->Session();
			Check(T.Expected.has_value(), TEXT("the import file reads"));
			if (T.Expected)
			{
				Check(Sess.id == AcSaves::WindowId && Sess.signature == AcSaves::Signature(*Sim->Choice()),
					FString::Printf(TEXT("imported as \"%s\" (%s)"), *Text(Sess.id), *Text(Sess.signature)));
				// The file's game as a fresh sim takes it in (see resume).
				const ac::Simulation Fresh(T.Expected->state, Sim->Map(), {}, Sim->IsFogOn());
				Check(ac::SessionStore::encode(Fresh.state) == ac::SessionStore::encode(S) && Sess.game == T.Expected->game
						&& Sess.mapName == T.Expected->mapName && S.time == T.Expected->state.time,
					FString::Printf(TEXT("the same game: %s on %s at %.1f min, %d units, %d buildings, score %d:%d"),
						*Text(Sess.game.value_or("")), *Text(Sess.mapName), Sess.state.time / 60, (int32)Sess.state.units.size(),
						(int32)Sess.state.structures.size(), Sess.state.score.size() > 0 ? (int32)Sess.state.score[0] : -1,
						Sess.state.score.size() > 1 ? (int32)Sess.state.score[1] : -1));
				Check(Sim->IsAIOn() == T.Expected->ai.value_or(true), TEXT("its AI setting"));
			}
			Advance();
			return;
		}
		if (T.Phase == 1 && S.time - T.GameAt >= 3)
		{
			Check(S.time > T.Expected->state.time, FString::Printf(TEXT("plays on (%.2f → %.2f s)"), T.Expected->state.time, S.time));
			Sim->Save();
			T.Expected = Sim->Session();
			Advance();
			return;
		}
		if (T.Phase == 2 && Since() >= 1.0)
		{
			SavedEqual(*T.Expected, TEXT("saved as the Unreal game's"));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("autosave"))
	{
		// Every 15 s of real time, without being asked.
		if (T.Phase == 0)
		{
			T.FileTime = IFileManager::Get().GetTimeStamp(*WindowFile());
			Advance();
			return;
		}
		const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*WindowFile());
		if (Stamp > T.FileTime)
		{
			Check(Since() <= 16.5, FString::Printf(TEXT("autosaved after %.1f s"), Since()));
			NextStep();
		}
		else if (Since() > 20)
		{
			Check(false, TEXT("no autosave within 20 s"));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("fog"))
	{
		if (T.Phase == 0) { Advance(); return; }
		if (T.Phase == 1 && S.time - T.GameAt >= 1)
		{
			const int32 Total = (int32)S.units.size() + (int32)S.structures.size();
			Sim->SetFog(false);
			const ac::GameState& Shown = Sim->Shown();
			Check(!Sim->IsFogOn() && Sim->Session().fog == false, TEXT("session fog off: no vision in the core"));
			Check((int32)Shown.units.size() + (int32)Shown.structures.size() == Total,
				FString::Printf(TEXT("everything shown (%d of %d)"), (int32)(Shown.units.size() + Shown.structures.size()), Total));
			Check(T.GamesStarted >= 1 && Sim->State().time >= T.GameAt, TEXT("the same game goes on"));
			Advance();
			return;
		}
		if (T.Phase == 2 && S.time - T.GameAt >= 1)
		{
			Sim->SetFog(true);
			const ac::GameState& Shown = Sim->Shown();
			const int32 Total = (int32)S.units.size() + (int32)S.structures.size();
			Check(Sim->IsFogOn() && Sim->Session().fog == true, TEXT("session fog on again"));
			Check((int32)Shown.units.size() + (int32)Shown.structures.size() < Total,
				FString::Printf(TEXT("the others hidden again (%d of %d shown)"), (int32)(Shown.units.size() + Shown.structures.size()), Total));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("menu"))
	{
		// The launcher through the real key path: Escape pauses, Escape
		// resumes, Quit to home saves, New Game from home starts a game.
		auto Press = [&](const FKey& K) {
			if (!Keys || !FSlateApplication::IsInitialized()) return false;
			return Keys->HandleKeyDownEvent(FSlateApplication::Get(), FKeyEvent(K, FModifierKeysState(), 0, false, 0, 0));
		};
		if (T.Phase == 0)
		{
			if (UAcPointer* Pointer = UAcPointer::Get(this)) Pointer->Select({});
			CloseMenu();
			Check(!Sim->IsPaused() && Press(EKeys::Escape) && Menu && Menu->Mode() == EAcMenuMode::Pause && Sim->IsPaused(),
				TEXT("Escape opens the pause menu and the game waits"));
			Check(Press(EKeys::Escape) && !Menu && !Sim->IsPaused(), TEXT("Escape again resumes"));
			Press(EKeys::Escape);
			Press(EKeys::Down);  // Resume, Settings
			Press(EKeys::Enter);
			{
				UAcSettings& Set = UAcSettings::Get();
				const float Was = Set.SfxVolume;
				Press(EKeys::Down);  // Master, Music, Effects
				Press(EKeys::Down);
				Press(EKeys::Left);
				const float Lower = Set.SfxVolume;
				Press(EKeys::Right);
				Check(Menu && Menu->InSettings() && Lower < Was && FMath::IsNearlyEqual(Set.SfxVolume, Was, 0.051f),
					TEXT("Settings: Left/Right move the effects volume"));
				Set.SfxVolume = Was;
				Check(Press(EKeys::Escape) && Menu && !Menu->InSettings() && Menu->Mode() == EAcMenuMode::Pause,
					TEXT("Escape leaves the settings for the pause menu"));
			}
			Press(EKeys::Down);  // back on Settings: Quit to home
			T.FileTime = IFileManager::Get().GetTimeStamp(*WindowFile());
			Check(Press(EKeys::Enter) && Menu && Menu->Mode() == EAcMenuMode::Home && Sim->IsPaused(), TEXT("Quit to home: the home screen, paused"));
			Advance();
			return;
		}
		if (T.Phase == 1 && Since() >= 1.0)
		{
			SavedEqual(Sim->Session(), TEXT("quit to home saved the game"));
			const double At = S.time;
			Check(Sim->IsPaused() && FMath::IsNearlyEqual(At, T.GameAt), TEXT("the game waits behind the home screen"));
			Check(!Press(EKeys::Escape) || Menu, TEXT("Escape keeps the home screen"));
			Menu->Highlight(EAcMenuAction::NewGame);  // NEW SKIRMISH
			Menu->PickHighlighted();
			Check(Dialog.IsValid() && Menu.IsValid(), TEXT("New Game opens the dialog over home"));
			Press(EKeys::Escape);
			Check(!Dialog && Menu && Sim->IsPaused(), TEXT("Cancel goes back to home"));
			Menu->SetMode(EAcMenuMode::Home);
			Menu->Highlight(EAcMenuAction::NewGame);
			Menu->PickHighlighted();
			T.GameId = Text(Sim->Session().game.value_or(""));
			Press(EKeys::Enter);
			Check(!Dialog && !Menu && !Sim->IsPaused() && Text(Sim->Session().game.value_or("")) != T.GameId,
				TEXT("Start: a new game, running, menus gone"));
			Advance();
			return;
		}
		if (T.Phase == 2 && S.time - T.GameAt >= 1)
		{
			Check(true, TEXT("the new game runs"));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("newgame"))
	{
		if (T.Phase == 0)
		{
			FAcNewGameChoice C;
			C.Map = ac::MapChoice{ac::MapStyle::badlands, ac::MapSize::small, 4};
			C.Teams = TEXT("2v2");
			C.bAI = true;
			C.bFog = true;
			T.GameId = Text(Sim->Session().game.value_or(""));
			StartNewGame(C);
			const ac::GameState& N = Sim->State();
			Check(Sim->Map().name == AcSaves::Build(C.Map, false).name && N.players.size() == 4,
				FString::Printf(TEXT("new game on %s, %d players"), *Text(Sim->Map().name), (int32)N.players.size()));
			Check(AcSaves::TeamsText(N.teams()) == TEXT("2v2") && N.allied(0, 1) && N.hostile(0, 2),
				FString::Printf(TEXT("teams %s"), *AcSaves::TeamsText(N.teams())));
			Check(Text(Sim->Session().game.value_or("")) != T.GameId && N.time == 0, TEXT("a new game id, time 0"));
			Advance();
			return;
		}
		if (T.Phase == 1 && S.time - T.GameAt >= 2)
		{
			T.GameId = Text(Sim->Session().game.value_or(""));
			const std::string Map = Sim->Map().name;
			Restart();
			const ac::GameState& N = Sim->State();
			Check(Sim->Map().name == Map && AcSaves::TeamsText(N.teams()) == TEXT("2v2") && N.players.size() == 4,
				TEXT("restart: same map, teams kept"));
			Check(Text(Sim->Session().game.value_or("")) != T.GameId && N.time == 0, TEXT("restart: a new game id, time 0"));
			Advance();
			return;
		}
		if (T.Phase == 2 && Since() >= 1.0)
		{
			SavedEqual(Sim->Session(), TEXT("the new game is on disk"));
			NextStep();
		}
		return;
	}
	if (Step == TEXT("victory"))
	{
		// Stage the end (no game played out): player 0 won half a second
		// before the lap ends, so the next game follows at once.
		if (T.Phase == 0)
		{
			ac::GameState& W = Sim->Simulation().state;
			W.winner = 0;
			W.endedAt = W.time - ac::Rules::victoryLap + 0.5;
			T.GameId = Text(Sim->Session().game.value_or(""));
			T.GamesStarted = 0;
			Advance();
			return;
		}
		if (T.Phase == 1 && T.GamesStarted > 0)
		{
			const ac::GameState& N = Sim->State();
			Check(Text(Sim->Session().game.value_or("")) != T.GameId && !N.endedAt && !N.winner && N.time < 1.0,
				FString::Printf(TEXT("victory lap → next game %s at %.2f s"), *Text(Sim->Session().game.value_or("")), N.time));
			NextStep();
		}
		else if (T.Phase == 1 && Since() > 5)
		{
			Check(false, TEXT("no next game after the victory lap"));
			NextStep();
		}
		return;
	}
	Check(false, FString::Printf(TEXT("unknown step %s"), *Step));
	NextStep();
}
