#include "AcGameFlow.h"
#include "AcMusicPlayer.h"

#include "AcCommandMap.h"
#include "AcCommands.h"
#include "AcGameMode.h"
#include "AcHUD.h"
#include "AcLog.h"
#include "AcPilotPawn.h"
#include "AcPointer.h"
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
			if (TSharedPtr<SAcSkillScreen> S = F->SkillScreenWidget())
			{
				if (K == EKeys::Escape || (K == EKeys::K && !Event.IsCommandDown())) { F->CloseSkillScreen(); return true; }
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
	UE_LOG(LogAutocraft, Log, TEXT("menu: %s on the first world"), bNewWorld ? TEXT("MMORPG NEW WORLD (TEST)") : TEXT("MMORPG (TEST)"));
	Sim->StartMmo(bNewWorld);
	TickMmo();
}

void UAcGameFlowSubsystem::StopMmo()
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo()) return;
	CloseWorldMap();
	CloseSkillScreen();
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
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(WorldMapLayer)
	[
		SAssignNew(WorldMap, SAcWorldMap)
		.OnFly_Lambda([Weak](const FString& Region, const FString& Pad) { if (Weak.IsValid()) Weak->FlyFromMap(Region, Pad); })
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
	}
	Info.PadReach = FAcMmoClient::PadReach;
	if (const ac::net::WorldMessage::Region* To = Mmo->FlyingTo()) Info.FlyingTo = Text(To->name);
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
		// The points that can still be taken back, and for how long (world time).
		const double Now = Mmo->Sky() ? Mmo->Sky()->time : 0.0;
		for (const ac::net::ProgressMessage::Undoable& U : P->undo)
		{
			G.recent.push_back({U.skill, U.until - ac::Progression::undoWindow});
			if (U.until > Now) Info.UndoLeft[U.skill] = FMath::Max(Info.UndoLeft[U.skill], U.until - Now);
		}
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
			Menu->Move(1);  // Resume, New Game
			Menu->PickHighlighted();
			Check(Dialog.IsValid() && Menu.IsValid(), TEXT("New Game opens the dialog over home"));
			Press(EKeys::Escape);
			Check(!Dialog && Menu && Sim->IsPaused(), TEXT("Cancel goes back to home"));
			Menu->SetMode(EAcMenuMode::Home);
			Menu->Move(1);
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
