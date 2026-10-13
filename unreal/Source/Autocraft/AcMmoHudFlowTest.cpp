// -AcFlowTest's MMORPG HUD steps (AcGameFlow.h; the others are in
// AcMmoFlowTest.cpp), over the local server on the first world (build it
// first: make -C unreal/server). Each starts a new world and waits to be
// seated in the Prospector on the start pad.
//
//   mmokeys  the world map by its keys, through the flow's key processor:
//            M's map opens on the start pad, the arrows move the pick
//            (Right: Tern Ridge, Down, Left, Up), Return asks for the
//            flight and closes the map; landed in Tern Ridge, the Prospector
//            drives off its pad, and Return on a pick is refused there with
//            the reason on the map, which stays open; Esc closes it
//   mmohud   the HUD in the mode (docs/mmorpg-m4.md Q4): driving, then
//            top-down with a Citadel selected and with nothing: no command
//            card, no view switch, the wallet, the unit count, the minimap
//            with the pads. -AcMmoHudShots=DIR saves a PNG of each moment
//            there (driving.png, topdown-citadel.png, topdown.png).
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   ... -game -RenderOffscreen ... -AcSaveDir=DIR -AcFlowTest=mmokeys,mmohud
#include "AcGameFlow.h"

#include "AcCommands.h"
#include "AcConsole.h"
#include "AcHUD.h"
#include "AcLog.h"
#include "AcMinimap.h"
#include "AcPilotPawn.h"
#include "AcRtsPawn.h"
#include "AcSimSubsystem.h"
#include "SAcConsole.h"
#include "SAcMinimap.h"
#include "SAcResourceBar.h"
#include "SAcRoot.h"
#include "SAcScoreboard.h"

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	constexpr double SeatSeconds = 20;
	constexpr double TravelSeconds = 60;

	const ac::Structure* PilotCitadel(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// -AcMmoHudShots=DIR: a PNG of the HUD at `Name` there (none without it).
	void Shot(const TCHAR* Name)
	{
		static const FString Dir = [] {
			FString D;
			FParse::Value(FCommandLine::Get(), TEXT("AcMmoHudShots="), D);
			return D;
		}();
		if (Dir.IsEmpty()) return;
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
	}
}

bool UAcGameFlowSubsystem::PressFlowKey(const FKey& Key)
{
	if (!Keys || !FSlateApplication::IsInitialized()) return false;
	return Keys->HandleKeyDownEvent(FSlateApplication::Get(), FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
}

void UAcGameFlowSubsystem::TickMmoHudFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	auto Finish = [&] {
		if (Mmo) Mmo->TestReadEvery = 0;
		if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		CloseWorldMap();
		StopMmo();
		NextStep();
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven()
			&& PC && PC->GetPawn() == Pilot;
	};
	auto Waited = [&](const TCHAR* What, const double Seconds) {
		if (Since() <= Seconds) return false;
		Check(false, FString::Printf(TEXT("%s within %.0f s (%s)"), What, Seconds, Mmo ? *Mmo->Message() : TEXT("no client")));
		Finish();
		return true;
	};
	// Both steps start the same: a new world, seated on the start pad.
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			Check(Utf(Mmo->Here()->id) == TEXT("start"), FString::Printf(TEXT("seated in Prospector #%lld in %s"), (long long)*Mmo->Driven(),
				*Utf(Mmo->Here()->id)));
			MmoVisitWas = Mmo->Visit();
			Advance();
		}
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}

	if (Step == TEXT("mmokeys"))
	{
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		auto Picked = [&] { return Map ? Map->PickedRegion() : FString(TEXT("(no map)")); };
		switch (T.Phase)
		{
		case 2:
		{
			// M opens the map (the command map's key hands it over).
			ToggleWorldMap();
			const TSharedPtr<SAcWorldMap> Open = WorldMapWidget();
			Check(Open.IsValid(), TEXT("M opened the world map"));
			if (!Open)
			{
				Finish();
				return;
			}
			auto Key = [&](const FKey& K, const TCHAR* Want) {
				const bool bTaken = PressFlowKey(K);
				Check(bTaken && Open->PickedRegion() == Want,
					FString::Printf(TEXT("%s picks %s (picked %s)"), *K.ToString(), Want, *Open->PickedRegion()));
			};
			Check(Open->PickedRegion() == TEXT("start") && Open->PickedPad() == TEXT("landing"),
				FString::Printf(TEXT("the pick starts on the start pad (%s %s)"), *Open->PickedRegion(), *Open->PickedPad()));
			// Return on the pad it stands on: nothing to fly to.
			PressFlowKey(EKeys::Enter);
			Check(WorldMapWidget().IsValid() && !Mmo->FlyingTo() && Open->RefusalText().Contains(TEXT("pick another pad")),
				FString::Printf(TEXT("Return on its own pad stays (\"%s\")"), *Open->RefusalText()));
			Key(EKeys::Right, TEXT("tern-ridge"));
			Key(EKeys::Down, TEXT("slag-pockets"));
			Key(EKeys::Left, TEXT("brine-pans"));
			Key(EKeys::Up, TEXT("start"));
			Key(EKeys::W, TEXT("cold-fork"));
			Key(EKeys::A, TEXT("ashen-shelf"));
			Check(PressFlowKey(EKeys::Up) && Open->PickedRegion() == TEXT("ashen-shelf"), TEXT("Up at the top keeps the pick"));
			Key(EKeys::S, TEXT("rust-hollow"));
			Key(EKeys::D, TEXT("start"));
			Key(EKeys::Right, TEXT("tern-ridge"));
			// A click and the keys share the pick: picked as a click does, then moved.
			Check(Open->PickPad(TEXT("glass-flats"), TEXT("landing")) && PressFlowKey(EKeys::Down) && Open->PickedRegion() == TEXT("tern-ridge"),
				TEXT("a clicked pick moves with the keys (Glass Flats, Down: Tern Ridge)"));
			// Slow frames from here, as mmofly.
			Mmo->TestReadEvery = 0.2;
			PressFlowKey(EKeys::Enter);
			Check(!WorldMapWidget().IsValid(), TEXT("Return asked for the flight and closed the map"));
			Advance();
			return;
		}
		case 3:
			if (Mmo->FlyingTo())
			{
				Check(Utf(Mmo->FlyingTo()->id) == TEXT("tern-ridge"), FString::Printf(TEXT("flying to %s"), *Utf(Mmo->FlyingTo()->id)));
				Advance();
			}
			else if (Since() > 5)
			{
				Check(false, FString::Printf(TEXT("lifted off within 5 s (%s)"), *Mmo->FlightRefusal()));
				Finish();
			}
			return;
		case 4:
			if (Mmo->Visit() > MmoVisitWas && !Mmo->Travelling()) Mmo->TestReadEvery = 0;
			if (Mmo->Visit() > MmoVisitWas && Seated() && Since() > 0.5)
			{
				Check(Utf(Mmo->Here()->id) == TEXT("tern-ridge"), FString::Printf(TEXT("landed in %s"), *Utf(Mmo->Here()->id)));
				Advance();
			}
			else Waited(TEXT("landed and seated"), TravelSeconds);
			return;
		case 5:
		{
			// Off the pad: drive away until well out of its reach.
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			if (Near && Near->Distance > FAcMmoClient::PadReach + 3)
			{
				Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
				Advance();
				return;
			}
			Pilot->SetTestInput(FVector2D(0, -1), Pilot->Yaw());
			Waited(TEXT("drove off the pad"), 15);
			return;
		}
		case 6:
		{
			if (Since() < 0.5) return;
			ToggleWorldMap();
			const TSharedPtr<SAcWorldMap> Open = WorldMapWidget();
			Check(Open.IsValid(), TEXT("M opened the world map again"));
			if (!Open)
			{
				Finish();
				return;
			}
			Check(Open->PickedRegion() == TEXT("tern-ridge"), FString::Printf(TEXT("the pick starts on Tern Ridge's pad (%s)"), *Open->PickedRegion()));
			PressFlowKey(EKeys::Left);
			Check(Open->PickedRegion() == TEXT("start"), TEXT("Left picks the start pad"));
			PressFlowKey(EKeys::Enter);
			const FString Why = Open->RefusalText();
			Check(WorldMapWidget().IsValid() && !Mmo->FlyingTo(), TEXT("Return off a pad: no flight, the map stays"));
			Check(Why.StartsWith(TEXT("Drive onto a pad to fly")), FString::Printf(TEXT("the map says why: \"%s\""), *Why));
			Check(PressFlowKey(EKeys::Escape) && !WorldMapWidget().IsValid(), TEXT("Esc closes the map"));
			Finish();
			return;
		}
		default:
			Finish();
			return;
		}
	}

	// mmohud
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	UAcConsoleSubsystem* Consoles = UAcConsoleSubsystem::Get(this);
	const TSharedPtr<SAcConsole> Console = Consoles ? Consoles->Console() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::GameState* S = M ? &M->simulation().state : nullptr;
	if (!Root || !Console)
	{
		Check(false, TEXT("a HUD and a console"));
		Finish();
		return;
	}
	// What every MMO moment shares: the wallet without supply, the unit
	// count, no command card buttons that would only say "Orders are off".
	auto Common = [&](const TCHAR* Where) {
		Check(!Root->ResourceBar()->ShowsSupply(), FString::Printf(TEXT("%s: the wallet, no supply"), Where));
		const SAcScoreboard& Score = *Root->ScoreboardWidget();
		Check(Score.GetVisibility() != EVisibility::Collapsed && Score.ShownCount(Sim->LocalPlayer()) == 1 && !Score.ShowsGamesWon(),
			FString::Printf(TEXT("%s: the unit count (%lld), no games won"), Where, (long long)Score.ShownCount(Sim->LocalPlayer())));
		const UAcMinimapSubsystem* Minimaps = UAcMinimapSubsystem::Get(this);
		const TSharedPtr<SAcMinimap> Mini = Minimaps ? Minimaps->Minimap() : nullptr;
		Check(Mini && Mini->PadsShown().Num() == 1, FString::Printf(TEXT("%s: the minimap marks the region's pad (%d)"), Where,
			Mini ? Mini->PadsShown().Num() : -1));
	};
	// Top-down: no card grid, no view switch, no orders to press.
	auto TopDown = [&](const TCHAR* Where) {
		Check(!Console->ShowsCard() && !Console->ShowsViewSwitch(), FString::Printf(TEXT("%s: no card grid, no view switch"), Where));
		const UAcCommandsSubsystem* Commands = UAcCommandsSubsystem::Get(this);
		Check(Commands && Commands->State().Actions.IsEmpty(), FString::Printf(TEXT("%s: no card actions to press (%d)"), Where,
			Commands ? Commands->State().Actions.Num() : -1));
	};
	switch (T.Phase)
	{
	case 2:
		if (Since() < 1.0) return;
		Common(TEXT("driving"));
		Check(Console->ShowsCard() && Console->ShowsViewSwitch(), TEXT("driving: the Prospector's card and the view switch"));
		Shot(TEXT("driving"));
		Advance();
		return;
	case 3:
		if (Since() < 0.6) return;
		Check(MmoBuildCitadel(), TEXT("a Citadel asked for"));
		Pilot->Leave();
		Advance();
		return;
	case 4:
		if (PC && PC->GetPawn() == AAcRtsPawn::Get(this) && S && PilotCitadel(*S) && Since() > 2.0)
		{
			Consoles->Select(PilotCitadel(*S)->id);
			Advance();
		}
		else Waited(TEXT("top-down with the Citadel"), 15);
		return;
	case 5:
		if (Since() < 0.6) return;
		Common(TEXT("top-down, Citadel"));
		TopDown(TEXT("top-down, Citadel"));
		Check(Consoles->Selected().IsSet(), TEXT("top-down, Citadel: selected"));
		Shot(TEXT("topdown-citadel"));
		Advance();
		return;
	case 6:
		if (Since() < 0.6) return;
		Consoles->Select({});
		Advance();
		return;
	case 7:
		if (Since() < 0.6) return;
		Common(TEXT("top-down"));
		TopDown(TEXT("top-down"));
		Shot(TEXT("topdown"));
		Advance();
		return;
	case 8:
		if (Since() < 0.6) return;
		Finish();
		return;
	default:
		Finish();
		return;
	}
}
