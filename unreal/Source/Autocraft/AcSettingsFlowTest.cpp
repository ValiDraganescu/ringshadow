// -AcFlowTest's Settings steps (U2, docs/mmorpg-play.md "Settings"), over
// the local server on the first world, through the flow's key processor:
//
//   mmosettings      the home screen's SETTINGS (a first run's defaults
//                    with -AcSettingsFirstRun), VOICE to 55, SCREEN to
//                    WINDOW at the smallest resolution offered (the largest
//                    with -AcSettingsLargest), OK: applied and in
//                    GameUserSettings.ini; ENTER THE REACH, seated, the HUD
//                    at that size; out of the Prospector, Esc: the pause
//                    menu, its SETTINGS (the same values), the lag switch
//                    on and off, the KEY CARD, Esc back; QUIT TO HOME
//   mmosettingsback  after a restart on the same config: VOICE 55 and the
//                    WINDOW at its size read back, on the page too; ENTER
//                    THE REACH, seated, QUIT TO HOME
//
// -AcMmoShots=DIR: the pictures there (u2-01-settings-home,
// u2-02-settings-pause, u2-03-key-card, u2-04-field-WxH). The config is the
// run's own: hidden_run.py's -UserDir=DIR/ and -AcRemember (it stores).
//   uv run unreal/Tools/hidden_run.py --out unreal/server/build/u2/run1 -- -AcMap=highlands-small \
//     -UserDir={out}/user/ -AcRemember -AcSettingsFirstRun -AcFlowTest=mmosettings -AcMmoShots={out}
#include "AcGameFlow.h"

#include "AcAudioDirector.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcPointer.h"
#include "AcRtsPawn.h"
#include "AcSettings.h"
#include "AcSimSubsystem.h"
#include "SAcMenu.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	constexpr double SettingsSeatSeconds = 30;
	constexpr float WantVoice = 0.55f;

	FIntPoint ViewSize()
	{
		return GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ? GEngine->GameViewport->Viewport->GetSizeXY()
																				   : FIntPoint::ZeroValue;
	}

	/// The game's config on disk (GGameUserSettingsIni is only its name).
	FString IniFile()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::GeneratedConfigDir(), ANSI_TO_TCHAR(FPlatformProperties::PlatformName()), TEXT("GameUserSettings.ini")));
	}

	/// The size `mmosettings` chose, for `mmosettingsback` (beside the config).
	FString ChosenFile() { return FPaths::Combine(FPaths::ProjectUserDir(), TEXT("AcSettingsChosen.txt")); }

	FString IniText()
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *IniFile());
		return Text;
	}

	FString Lag()
	{
		const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.MmoLag"));
		return V ? V->GetString() : FString(TEXT("?"));
	}
}

void UAcGameFlowSubsystem::TickSettingsFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	UAcSettings& S = UAcSettings::Get();
	const bool bBack = Step == TEXT("mmosettingsback");
	auto Finish = [&] {
		if (Sim && Sim->IsMmo()) StopMmo();
		NextStep();
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven() && PC
			&& PC->GetPawn() == Pilot && Mmo->Here();
	};
	// A picture (with -AcMmoShots=DIR): asked for, then waited for (true once
	// it is on disk, or none was asked).
	auto Shoot = [&](const FString& Name) {
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		SettingsShot = FPaths::Combine(Dir, Name + TEXT(".png"));
		IFileManager::Get().Delete(*SettingsShot, false, true, true);
		FScreenshotRequest::RequestScreenshot(SettingsShot, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: %s: picture %s"), *Step, *SettingsShot);
	};
	auto ShotDone = [&] {
		if (SettingsShot.IsEmpty()) return Since() > 0.3;
		if (IFileManager::Get().FileSize(*SettingsShot) > 0 && Since() > 0.5)
		{
			SettingsShot.Empty();
			return true;
		}
		if (Since() > 15)
		{
			Check(false, FString::Printf(TEXT("the picture %s written within 15 s"), *SettingsShot));
			SettingsShot.Empty();
			return true;
		}
		return false;
	};
	auto Late = [&](const TCHAR* What, const double Seconds) {
		if (Since() <= Seconds) return false;
		Check(false, FString::Printf(TEXT("%s within %.0f s (%s)"), What, Seconds, Mmo ? *Mmo->Message() : TEXT("no client")));
		Finish();
		return true;
	};
	auto Near = [](const float A, const float B) { return FMath::Abs(A - B) < 0.001f; };

	switch (T.Phase)
	{
	case 0:
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Check(OpenMenu(EAcMenuMode::Home) && Menu, TEXT("the home screen"));
		if (!Menu)
		{
			Finish();
			return;
		}
		FString List, Window, Convenient;
		for (const FIntPoint& R : UAcSettings::Resolutions(true)) List += FString::Printf(TEXT(" %dx%d"), R.X, R.Y);
		for (const FIntPoint& R : UAcSettings::Resolutions(false)) Window += FString::Printf(TEXT(" %dx%d"), R.X, R.Y);
		TArray<FIntPoint> Conv;
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Conv);
		for (const FIntPoint& R : Conv) Convenient += FString::Printf(TEXT(" %dx%d"), R.X, R.Y);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: %s: the config %s, the display %dx%d; full screen:%s; a window:%s (the engine's windowed:%s)"),
			*Step, *IniFile(), UAcSettings::Desktop().X, UAcSettings::Desktop().Y, *List, *Window, *Convenient);
		if (FParse::Param(FCommandLine::Get(), TEXT("AcSettingsFirstRun")))
		{
			Check(Near(S.MusicVolume, 0.8f) && Near(S.VoiceVolume, 1.f) && Near(S.SfxVolume, 1.f) && Near(S.MasterVolume, 1.f) && !S.bMmoLag
					&& Lag().IsEmpty(),
				FString::Printf(TEXT("a first run's defaults: music %.2f, voice %.2f, effects %.2f, master %.2f, lag %s (\"%s\")"), S.MusicVolume,
					S.VoiceVolume, S.SfxVolume, S.MasterVolume, S.bMmoLag ? TEXT("on") : TEXT("off"), *Lag()));
			Check(S.GetFullscreenMode() == EWindowMode::WindowedFullscreen,
				FString::Printf(TEXT("a first run: full screen at the display's own resolution (mode %d)"), int32(S.GetFullscreenMode())));
		}
		if (bBack)
		{
			Check(Near(S.VoiceVolume, WantVoice), FString::Printf(TEXT("read back after the restart: voice %.2f"), S.VoiceVolume));
			// The size the first run chose (its note beside the config: the
			// display's modes can read differently from one run to the next).
			FString Chosen;
			FFileHelper::LoadFileToString(Chosen, *ChosenFile());
			FIntPoint Want = FIntPoint::ZeroValue;
			FString WantX, WantY;
			if (Chosen.TrimStartAndEnd().Split(TEXT("x"), &WantX, &WantY)) Want = FIntPoint(FCString::Atoi(*WantX), FCString::Atoi(*WantY));
			// The file the restart read (the engine writes the window's size
			// into the settings when a hidden run's -ResX sizes it, and saves
			// them at the quit; until then the file is the last run's).
			const FString Ini = IniText();
			Check(Ini.Contains(TEXT("FullscreenMode=2")) && Ini.Contains(FString::Printf(TEXT("ResolutionSizeX=%d"), Want.X))
					&& Ini.Contains(FString::Printf(TEXT("ResolutionSizeY=%d"), Want.Y)),
				FString::Printf(TEXT("read back after the restart: a window %dx%d in %s"), Want.X, Want.Y, *IniFile()));
			FString ResX;
			const bool bForced = FParse::Value(FCommandLine::Get(), TEXT("ResX="), ResX);
			Check(S.GetFullscreenMode() == EWindowMode::Windowed && (bForced || S.GetScreenResolution() == Want),
				FString::Printf(TEXT("the game reads it: a window (mode %d), %dx%d%s"), int32(S.GetFullscreenMode()), S.GetScreenResolution().X,
					S.GetScreenResolution().Y, bForced ? TEXT(" (the run's -ResX sizes this one)") : TEXT("")));
		}
		Check(Menu->Highlight(EAcMenuAction::Settings) && PressFlowKey(EKeys::Enter) && Menu->InSettings() && !Menu->InKeyCard(),
			TEXT("SETTINGS from the home screen"));
		Check(Menu->SettingsRow() == EAcSettingsRow::Master, TEXT("the page opens on MASTER"));
		if (bBack)
		{
			Check(!Menu->PendingFullScreen() && Menu->ChoiceTextFor(EAcSettingsRow::Voice) == TEXT("55"),
				FString::Printf(TEXT("the page reads VOICE %s, WINDOW %dx%d"), *Menu->ChoiceTextFor(EAcSettingsRow::Voice),
					Menu->PendingResolution().X, Menu->PendingResolution().Y));
			Check(PressFlowKey(EKeys::Escape) && !Menu->InSettings(), TEXT("Esc closes the page"));
			Check(Menu->Highlight(EAcMenuAction::Mmo), TEXT("ENTER THE REACH"));
			const TSharedPtr<SAcMenu> Held = Menu;
			Held->PickHighlighted();
			Check(Sim->IsMmo(), TEXT("the field"));
			T.Phase = 2;
			T.PhaseAt = Now;
			return;
		}
		Shoot(TEXT("u2-01-settings-home"));
		Advance();
		return;
	}
	case 1:
	{
		if (!ShotDone()) return;
		// VOICE: Down twice (MASTER, MUSIC), then nine steps down to 55.
		PressFlowKey(EKeys::Down);
		PressFlowKey(EKeys::Down);
		Check(Menu && Menu->SettingsRow() == EAcSettingsRow::Voice, TEXT("Down twice: VOICE"));
		for (int32 I = 0; I < 9; ++I) PressFlowKey(EKeys::Left);
		Check(Near(S.VoiceVolume, WantVoice) && Near(AcSound::SpeechGain(), WantVoice * S.MasterVolume),
			FString::Printf(TEXT("Left: VOICE %.2f, live (the voices' gain %.3f)"), S.VoiceVolume, AcSound::SpeechGain()));
		// SCREEN: WINDOW; RESOLUTION: the smallest (or largest) offered.
		Menu->HighlightRow(EAcSettingsRow::Screen);
		PressFlowKey(EKeys::Right);
		Check(!Menu->PendingFullScreen() && S.GetFullscreenMode() == EWindowMode::WindowedFullscreen,
			TEXT("SCREEN: WINDOW chosen, not yet applied"));
		const TArray<FIntPoint>& Offered = Menu->ResolutionChoices();
		const bool bLargest = FParse::Param(FCommandLine::Get(), TEXT("AcSettingsLargest"));
		SettingsWant = Offered.Num() == 0 ? FIntPoint::ZeroValue : bLargest ? Offered.Last() : Offered[0];
		Menu->HighlightRow(EAcSettingsRow::Resolution);
		for (int32 I = 0; I < Offered.Num() && Menu->PendingResolution() != SettingsWant; ++I) PressFlowKey(bLargest ? EKeys::Right : EKeys::Left);
		Check(Offered.Num() > 0 && Menu->PendingResolution() == SettingsWant,
			FString::Printf(TEXT("RESOLUTION: %dx%d, the %s of %d"), SettingsWant.X, SettingsWant.Y, bLargest ? TEXT("largest") : TEXT("smallest"),
				Offered.Num()));
		FFileHelper::SaveStringToFile(FString::Printf(TEXT("%dx%d"), SettingsWant.X, SettingsWant.Y), *ChosenFile());
		Menu->HighlightRow(EAcSettingsRow::Ok);
		Check(PressFlowKey(EKeys::Enter) && !Menu->InSettings(), TEXT("OK closes the page"));
		Check(S.GetFullscreenMode() == EWindowMode::Windowed && S.GetScreenResolution() == SettingsWant,
			FString::Printf(TEXT("OK: a window %dx%d"), S.GetScreenResolution().X, S.GetScreenResolution().Y));
		const FString Ini = IniText();
		Check(Ini.Contains(TEXT("VoiceVolume=0.55")) && Ini.Contains(TEXT("FullscreenMode=2"))
				&& Ini.Contains(FString::Printf(TEXT("ResolutionSizeX=%d"), SettingsWant.X)),
			FString::Printf(TEXT("stored in %s"), *IniFile()));
		Check(Menu->Highlight(EAcMenuAction::Mmo), TEXT("ENTER THE REACH"));
		const TSharedPtr<SAcMenu> Held = Menu;
		Held->PickHighlighted();
		Check(Sim->IsMmo(), TEXT("the field"));
		Advance();
		return;
	}
	case 2:
		if (!Seated() || Since() < 3.0)
		{
			Late(TEXT("seated"), SettingsSeatSeconds);
			return;
		}
		if (bBack)
		{
			Check(true, FString::Printf(TEXT("seated in the field, the view %dx%d"), ViewSize().X, ViewSize().Y));
			StopMmo();
			Check(OpenMenu(EAcMenuMode::Home) && !Sim->IsMmo(), TEXT("QUIT TO HOME"));
			NextStep();
			return;
		}
		Check(ViewSize() == SettingsWant,
			FString::Printf(TEXT("seated, the view %dx%d (%dx%d chosen)"), ViewSize().X, ViewSize().Y, SettingsWant.X, SettingsWant.Y));
		Shoot(FString::Printf(TEXT("u2-04-field-%dx%d"), ViewSize().X, ViewSize().Y));
		Advance();
		return;
	case 3:
		if (!ShotDone()) return;
		Pilot->Leave();
		Advance();
		return;
	case 4:
		if (!(PC && PC->GetPawn() == AAcRtsPawn::Get(this) && Since() > 1.5))
		{
			Late(TEXT("the top-down view"), 15);
			return;
		}
		if (UAcPointer* Pointer = UAcPointer::Get(this)) Pointer->Select({});
		Check(PressFlowKey(EKeys::Escape) && Menu && Menu->Mode() == EAcMenuMode::Pause, TEXT("Esc in the top-down view: the pause menu"));
		if (!Menu)
		{
			Finish();
			return;
		}
		Check(Menu->Highlight(EAcMenuAction::Settings) && PressFlowKey(EKeys::Enter) && Menu->InSettings(), TEXT("SETTINGS from the pause menu"));
		Check(Near(S.VoiceVolume, WantVoice) && !Menu->PendingFullScreen() && Menu->PendingResolution() == SettingsWant,
			TEXT("the same page: VOICE 55, WINDOW at its size"));
		// The lag simulator's switch: on (two_clients.py's preset), off.
		Menu->HighlightRow(EAcSettingsRow::Lag);
		PressFlowKey(EKeys::Right);
		Check(S.bMmoLag && Lag() == TEXT("150 20 5"), FString::Printf(TEXT("LAG SIMULATOR on: ac.MmoLag \"%s\""), *Lag()));
		PressFlowKey(EKeys::Right);
		Check(!S.bMmoLag && Lag().IsEmpty(), FString::Printf(TEXT("LAG SIMULATOR off: ac.MmoLag \"%s\""), *Lag()));
		Menu->HighlightRow(EAcSettingsRow::Voice);
		Shoot(TEXT("u2-02-settings-pause"));
		Advance();
		return;
	case 5:
	{
		if (!ShotDone()) return;
		Menu->HighlightRow(EAcSettingsRow::KeyCard);
		Check(PressFlowKey(EKeys::Enter) && Menu->InKeyCard(), TEXT("KEY CARD"));
		int32 Rows = 0;
		bool bMap = false;
		for (const AcPilotText::FKeyGroup& G : AcPilotText::KeyCard())
		{
			Rows += G.Rows.Num();
			for (const AcPilotText::FKeyRow& R : G.Rows) bMap |= R.Key == TEXT("M") && R.What.StartsWith(TEXT("world map"));
		}
		Check(Rows >= 15 && bMap, FString::Printf(TEXT("the key card: %d keys, M the world map"), Rows));
		Shoot(TEXT("u2-03-key-card"));
		Advance();
		return;
	}
	case 6:
		if (!ShotDone()) return;
		Check(PressFlowKey(EKeys::Escape) && Menu && Menu->InSettings() && !Menu->InKeyCard(), TEXT("Esc: the key card back to the page"));
		Check(PressFlowKey(EKeys::Escape) && Menu && !Menu->InSettings() && Menu->Mode() == EAcMenuMode::Pause, TEXT("Esc: the page closed"));
		Check(Menu && Menu->Highlight(EAcMenuAction::QuitToHome), TEXT("QUIT TO HOME"));
		if (Menu)
		{
			const TSharedPtr<SAcMenu> Held = Menu;
			Held->PickHighlighted();
		}
		Check(Menu && Menu->Mode() == EAcMenuMode::Home && !Sim->IsMmo(), TEXT("back on the home screen"));
		NextStep();
		return;
	default:
		Finish();
		return;
	}
}
