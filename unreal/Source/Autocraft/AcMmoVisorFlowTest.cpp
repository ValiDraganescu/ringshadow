// -AcFlowTest's visor step (docs/weather.md, "Rain on the visor and the
// wipers"), over the local server on the first world (build it first: make
// -C unreal/server). It starts a new world, waits to be seated in the
// Prospector on the start pad, goes to first person and, in the weather the
// server holds (-AcMmoWeather=rain|storm), checks the glass is in view and
// the wiper runs. With -AcMmoShots=DIR, PNGs at points of a sweep
// (visor-<weather>-N-<moment>.png, the weather's name from -AcMmoWeather):
//   1-sweep    the blade straight up on its way out, its ridge ahead of it
//   2-clean    on its way back: the glass right behind it clear
//   3-half     the middle of the glass half way to its next wipe
//   4-full     the blade coming again: the middle built up since
//   5-nowipe   the wiper held parked (ac.Wipers 0) for 10 s: the glass
//              as the rain leaves it unwiped
//   6-wiping   the wiper let go again: its first sweep half way through that
//   7-cleared  and back: behind it the glass clear again
// Then what the glass costs: the GPU frame with it drawn and not
// (ac.Weather.Lens), in turns of 3 s, four of each, logged as
// "flowtest: visor cost".
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmovisor
//       -AcMmoWeather=storm -AcMmoShots={out}
#include "AcGameFlow.h"

#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcWeather.h"

#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
	constexpr double SeatSeconds = 20;
	/// Rain landing and the wiper going before the first picture.
	constexpr double WetSeconds = 8;
	/// The wiper held parked this long for the unwiped glass.
	constexpr double HeldSeconds = 10;
	/// The longest a picture's moment may take to come.
	constexpr double MomentSeconds = 30;

	/// The cost's turns: the glass drawn and not, each this long, this many.
	constexpr double TurnSeconds = 3;
	constexpr int32 Turns = 8;

	struct FVisor
	{
		int32 Shot = 0;
		double ShotAt = -1;
		double HeldAt = -1;
		/// The cost: the turn now and when it began; GPU frame ms summed
		/// with the glass drawn (0) and not (1), and the frames.
		int32 Turn = -1;
		double TurnAt = -1;
		double Gpu[2] = {0, 0};
		int32 Frames[2] = {0, 0};
	};
	FVisor Visor;

	void SetLens(const bool bOn)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Weather.Lens")))
		{
			V->Set(bOn ? 1 : 0, ECVF_SetByCode);
		}
	}

	void SetWipers(const bool bOn)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Wipers")))
		{
			V->Set(bOn ? 1 : 0, ECVF_SetByCode);
		}
	}

	/// -AcMmoShots=DIR: a PNG of the moment `Name` there.
	void Picture(const FString& Name)
	{
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
	}
}

void UAcGameFlowSubsystem::TickMmoVisorFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
	};
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const AAcWeather* Weather = AAcWeather::Find(GetWorld());
	auto Finish = [&] {
		SetWipers(true);
		SetLens(true);
		StopMmo();
		NextStep();
	};
	auto Waited = [&](const FString& What, const double Seconds) {
		if (Since() <= Seconds) return false;
		Check(false, FString::Printf(TEXT("%s within %.0f s (%s)"), *What, Seconds, Mmo ? *Mmo->Message() : TEXT("no client")));
		Finish();
		return true;
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven() && PC
			&& PC->GetPawn() == Pilot;
	};
	if (Step != TEXT("mmovisor"))
	{
		Check(false, FString::Printf(TEXT("no visor step %s (mmovisor)"), *Step));
		NextStep();
		return;
	}
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Visor = FVisor();
		SetWipers(true);
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			if (Pilot->ChaseEye().IsSet()) Pilot->ToggleView();
			Advance();
		}
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}
	if (!Seated() || !Weather)
	{
		Waited(TEXT("still driving, the weather up"), 4);
		return;
	}
	const ac::Wipers& W = Weather->Wipers();
	if (T.Phase == 2)
	{
		if (Since() < WetSeconds) return;
		Check(Weather->GlassShown(), TEXT("the visor's glass in view, driving in first person"));
		Check(W.rate() > 0.1, FString::Printf(TEXT("rain on the glass (rate %.2f; -AcMmoWeather=rain|storm)"), W.rate()));
		Check(W.mode() != ac::WiperMode::off, FString::Printf(TEXT("the wiper running in the rain (mode %d)"), int32(W.mode())));
		Check(W.passes() >= 2, FString::Printf(TEXT("the wiper has swept (%lld passes)"), (long long)W.passes()));
		if (W.rate() <= 0.1 || W.mode() == ac::WiperMode::off)
		{
			Finish();
			return;
		}
		Advance();
		return;
	}

	FString Held;
	FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), Held);
	const FString Tag = Held.IsEmpty() ? FString::Printf(TEXT("%.0f"), W.rate() * 100) : Held.ToLower();
	const double A = W.angle();
	const int M = W.moving();
	// How long since the blade last crossed the middle of the glass, and the
	// longest it goes uncrossed (from the back pass to the next way out).
	const double Mid = W.now() - W.lastWipe(0.0);
	const double Gap = FMath::Max(ac::Wipers::cadenceOf(W.mode()) - W.latest().each, 0.2);
	auto Shoot = [&](const TCHAR* Moment) {
		Picture(FString::Printf(TEXT("visor-%s-%d-%s"), *Tag, Visor.Shot + 1, Moment));
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: visor %s: mode %d, rate %.2f, blade %.2f (%d), middle wiped %.2f s ago, %.2f covered"),
			Moment, int32(W.mode()), W.rate(), A, M, Mid, W.coverage());
		Visor.Shot += 1;
		Visor.ShotAt = Now;
		T.PhaseAt = Now;
	};
	if (Visor.ShotAt >= 0 && Now - Visor.ShotAt < 0.5) return;
	switch (Visor.Shot)
	{
	case 0:
		if (M == 1 && FMath::Abs(A) < 0.12) Shoot(TEXT("sweep"));
		break;
	case 1:
		if (M == -1 && A > -0.75 && A < -0.55) Shoot(TEXT("clean"));
		break;
	case 2:
		if (M != -1 && Mid >= Gap * 0.5 && Mid < Gap * 0.5 + 0.06) Shoot(TEXT("half"));
		break;
	case 3:
		if (M == 1 && A > -0.5 && A < -0.3) Shoot(TEXT("full"));
		break;
	case 4:
		if (Visor.HeldAt < 0)
		{
			// Held parked: let the running sweep end first.
			SetWipers(false);
			Visor.HeldAt = Now;
		}
		if (Now - Visor.HeldAt >= HeldSeconds)
		{
			Check(W.parked(), TEXT("held parked"));
			Shoot(TEXT("nowipe"));
			SetWipers(true);
		}
		break;
	case 5:
		if (M == 1 && FMath::Abs(A) < 0.1) Shoot(TEXT("wiping"));
		break;
	case 6:
		if (M == -1 && A > -0.75 && A < -0.55) Shoot(TEXT("cleared"));
		break;
	case 7:
	{
		// The cost: turns of the glass drawn and not; the GPU timer lags a
		// few frames, so each turn's first half second is not counted.
		if (Visor.Turn < 0 || Now - Visor.TurnAt >= TurnSeconds)
		{
			Visor.Turn += 1;
			Visor.TurnAt = Now;
			T.PhaseAt = Now;
			if (Visor.Turn >= Turns)
			{
				SetLens(true);
				const double On = Visor.Gpu[0] / FMath::Max(Visor.Frames[0], 1), Off = Visor.Gpu[1] / FMath::Max(Visor.Frames[1], 1);
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: visor cost: GPU frame %.2f ms with the glass, %.2f ms without: %.2f ms (%d and %d frames, %s)"),
					On, Off, On - Off, Visor.Frames[0], Visor.Frames[1], *Tag);
				Visor.Shot += 1;
				Visor.ShotAt = Now;
				return;
			}
			SetLens(Visor.Turn % 2 == 0);
			return;
		}
		if (Now - Visor.TurnAt > 0.5)
		{
			Visor.Gpu[Visor.Turn % 2] += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
			Visor.Frames[Visor.Turn % 2] += 1;
		}
		return;
	}
	default:
		if (Now - Visor.ShotAt > 1.0) Finish();
		return;
	}
	Waited(FString::Printf(TEXT("the visor's moment %d"), Visor.Shot + 1), MomentSeconds + (Visor.Shot == 4 ? HeldSeconds : 0));
}
