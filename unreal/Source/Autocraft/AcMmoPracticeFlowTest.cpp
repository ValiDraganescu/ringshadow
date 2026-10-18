// -AcFlowTest's practice step (C2, docs/guns-and-practice.md §7; the other
// MMORPG steps are in AcMmoFlowTest.cpp), over the local server on the first
// world with the dev rig (a finished Citadel, level 25, a full wallet;
// build the server first: make -C unreal/server). It starts a new world and
// waits to be seated in the Prospector on the start pad.
//
//   mmopractice  B opens the radial build wheel (the reticle gives way to
//             it); the mouse's motion leans its stick onto the first slice
//             the pilot may build: that slice lit; a click holds the kind out
//             to place; Esc drops it; B again and B again close the wheel
//             with nothing held. Every move through the pawn's own hooks
//             (`OnKey`, `OnLookTaken`, the act key).
//             With -AcMmoShots=DIR -AcMmoShotsC2: a PNG of the wheel open on
//             that slice (c2-wheel.png).
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR --headless -- -AcFlowTest=mmopractice
#include "AcGameFlow.h"

#include "AcBuildWheel.h"
#include "AcLog.h"
#include "AcPilotBuild.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"

#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
	constexpr double SeatSeconds = 20;

	/// The step's own memory.
	struct FPractice
	{
		int32 Slice = INDEX_NONE;
		bool bShot = false;
		/// The command line before the dev rig was added (put back after).
		FString CommandLineWas;
		double ShotAt = -1;
	};
	FPractice Practice;

	/// -AcMmoShots=DIR with -AcMmoShotsC2: a PNG of the moment `Name` there.
	bool Picture(const TCHAR* Name)
	{
		FString Dir;
		if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2")) || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir)
			|| Dir.IsEmpty())
		{
			return false;
		}
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
		return true;
	}
}

void UAcGameFlowSubsystem::TickMmoPracticeFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
		Practice.bShot = false;
		Practice.ShotAt = -1;
	};
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UAcPilotOverlaySubsystem* Overlay = UAcPilotOverlaySubsystem::Get(this);
	UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
	auto Finish = [&] {
		if (Pilot && Pilot->OnKey && Build && (Build->MenuOpen() || Build->Placing())) Pilot->OnKey(TEXT("esc"));
		StopMmo();
		// The steps after this one go without the dev rig.
		if (!Practice.CommandLineWas.IsEmpty()) FCommandLine::Set(*Practice.CommandLineWas);
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
	if (Step != TEXT("mmopractice"))
	{
		Check(false, FString::Printf(TEXT("no practice step %s (mmopractice)"), *Step));
		NextStep();
		return;
	}
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Practice = FPractice();
		// The dev rig: a finished Citadel, level 25, a full wallet.
		if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig")))
		{
			Practice.CommandLineWas = FCommandLine::Get();
			FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		// With pictures, the world streamed in first.
		const double Settle = FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2")) ? 8.0 : 1.0;
		if (Seated() && Mmo->Here() && Mmo->Progress() && Build && Since() > Settle) Advance();
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}
	if (!Seated() || !Build || !Pilot->OnKey)
	{
		Waited(TEXT("still driving"), 4);
		return;
	}
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	switch (T.Phase)
	{
	case 2:
	{
		// B: the wheel opens, nothing picked inside the dead zone.
		Check(Pilot->OnKey(TEXT("b")) && Build->MenuOpen(), TEXT("practice: B opens the build wheel"));
		Check(Build->WheelPick() == INDEX_NONE, TEXT("practice: the wheel opens with nothing picked"));
		const TArray<FAcWheelSlice>& Slices = Build->WheelSlices();
		Check(!Slices.IsEmpty(), TEXT("practice: the wheel has slices"));
		for (int32 I = 0; I < Slices.Num() && Practice.Slice == INDEX_NONE; ++I)
		{
			if (Slices[I].State == FAcWheelSlice::EState::Build && Slices[I].Kind) Practice.Slice = I;
		}
		Check(Practice.Slice != INDEX_NONE, TEXT("practice: a slice the rig's pilot may build"));
		if (Practice.Slice == INDEX_NONE)
		{
			Finish();
			return;
		}
		// The mouse leans the stick onto it (in a few motions, as a hand does).
		const double A = FAcBuildWheel::SliceAngle(Practice.Slice, Slices.Num());
		const FVector2D To = FVector2D(std::sin(A), std::cos(A)) * 90;
		for (int32 K = 0; K < 3 && Pilot->OnLookTaken; ++K) Pilot->OnLookTaken(To / 3);
		Check(Build->WheelPick() == Practice.Slice,
			FString::Printf(TEXT("practice: the stick picks slice %d (%s), not %d"), Practice.Slice, *Slices[Practice.Slice].Title,
				Build->WheelPick()));
		Advance();
		return;
	}
	case 3:
		// The overlay draws it (and no reticle), lit on the pick; a picture.
		if (!Info || !Info->Wheel || Info->Wheel->Pick != Practice.Slice)
		{
			Waited(TEXT("practice: the overlay shows the wheel on the pick"), 3);
			return;
		}
		if (!Practice.bShot)
		{
			Practice.bShot = true;
			Practice.ShotAt = Picture(TEXT("c2-wheel")) ? Now : Now - 10;
		}
		if (Now - Practice.ShotAt < 1.0) return;
		Check(true, TEXT("practice: the overlay shows the wheel lit on the pick"));
		Check(!(Info->Console && Info->Console->bBuildList), TEXT("practice: no build list on the console beside the wheel"));
		Advance();
		return;
	case 4:
	{
		// A click: the kind held out, the wheel gone.
		const TOptional<ac::StructureKind> Want = Build->WheelSlices()[Practice.Slice].Kind;
		Pilot->Press(true);
		Pilot->Press(false);
		Check(!Build->MenuOpen() && Build->Placing() == Want, TEXT("practice: a click holds the picked kind out to place"));
		Check(Pilot->OnKey(TEXT("esc")) && !Build->Placing(), TEXT("practice: Esc drops it"));
		// B, B: open and shut with nothing held.
		Pilot->OnKey(TEXT("b"));
		Check(Build->MenuOpen() && Build->WheelPick() == INDEX_NONE, TEXT("practice: B opens it again, the stick back in the middle"));
		Pilot->OnKey(TEXT("b"));
		Check(!Build->MenuOpen() && !Build->Placing(), TEXT("practice: B again closes it"));
		Advance();
		return;
	}
	case 5:
		if (Info && Info->Wheel)
		{
			Waited(TEXT("practice: the wheel gone from the overlay"), 3);
			return;
		}
		Check(true, TEXT("practice: the wheel gone from the overlay"));
		Finish();
		return;
	default:
		Finish();
		return;
	}
}
