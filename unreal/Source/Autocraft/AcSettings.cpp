#include "AcSettings.h"

#include "AcLog.h"
#include "PlayerSettings.h"

#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"

#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"

UAcSettings& UAcSettings::Get()
{
	if (GEngine)
	{
		if (UAcSettings* S = Cast<UAcSettings>(GEngine->GetGameUserSettings())) return *S;
	}
	// The ini does not name this class (or no engine yet): our own copy.
	static TStrongObjectPtr<UAcSettings> Own;
	if (!Own)
	{
		Own.Reset(NewObject<UAcSettings>(GetTransientPackage(), TEXT("AcSettings")));
		Own->LoadSettings(false);
	}
	return *Own;
}

void UAcSettings::Store()
{
	Get().SaveSettings();
}

bool UAcSettings::Remembers()
{
	return !FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("AcRemember"));
}

FIntPoint UAcSettings::Desktop()
{
	return Get().GetDesktopResolution();
}

TArray<FIntPoint> UAcSettings::Resolutions(const bool bFullScreen)
{
	TArray<FIntPoint> Modes;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Modes);
	std::vector<ac::Resolution> In;
	for (const FIntPoint& M : Modes) In.push_back({M.X, M.Y});
	const FIntPoint D = Desktop();
	TArray<FIntPoint> Out;
	for (const ac::Resolution& R : ac::ScreenChoices::list(In, {D.X, D.Y}, bFullScreen)) Out.Add(FIntPoint(R.width, R.height));
	return Out;
}

void UAcSettings::ApplyScreen()
{
	UAcSettings& S = Get();
	const FIntPoint D = Desktop();
	const FIntPoint Pick = S.GetScreenResolution();
	const ac::ScreenMode Mode =
		ac::ScreenChoices::mode(S.GetFullscreenMode() != EWindowMode::Windowed, {Pick.X, Pick.Y}, {D.X, D.Y});
	const EWindowMode::Type Want = Mode == ac::ScreenMode::window ? EWindowMode::Windowed
		: Mode == ac::ScreenMode::borderless                      ? EWindowMode::WindowedFullscreen
																  : EWindowMode::Fullscreen;
	S.SetFullscreenMode(Want);
	if (Mode == ac::ScreenMode::borderless && D.X > 0) S.SetScreenResolution(D);
	// A hidden run keeps its window off the display: a window is resized,
	// a full screen only logged (it would take the user's display).
	if (FApp::IsUnattended() && Mode != ac::ScreenMode::window)
	{
		UE_LOG(LogAutocraft, Log, TEXT("settings: screen %s %dx%d kept, not applied (a hidden run)"),
			Mode == ac::ScreenMode::borderless ? TEXT("full screen") : TEXT("full screen, the display switched"), S.GetScreenResolution().X,
			S.GetScreenResolution().Y);
		return;
	}
	S.ApplyResolutionSettings(false);
	UE_LOG(LogAutocraft, Log, TEXT("settings: screen %s %dx%d"),
		Mode == ac::ScreenMode::window ? TEXT("window") : Mode == ac::ScreenMode::borderless ? TEXT("full screen") : TEXT("full screen, the display switched"),
		S.GetScreenResolution().X, S.GetScreenResolution().Y);
}

void UAcSettings::ApplyLag()
{
	IConsoleVariable* Lag = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.MmoLag"));
	if (FString Flag; !Lag || FParse::Value(FCommandLine::Get(), TEXT("AcMmoLag="), Flag, false)) return;
	Lag->Set(UTF8_TO_TCHAR(ac::PlayerSettings::lagSetting(Get().bMmoLag).c_str()), ECVF_SetByGameSetting);
}
