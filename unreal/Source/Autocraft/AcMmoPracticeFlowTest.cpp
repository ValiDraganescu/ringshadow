// -AcFlowTest's practice step (C2, docs/guns-and-practice.md §7; the other
// MMORPG steps are in AcMmoFlowTest.cpp), over the local server on the first
// world with the dev rig (a finished Citadel, level 25, a full wallet;
// build the server first: make -C unreal/server). It starts a new world and
// waits to be seated in the Prospector on the start pad.
//
//   mmopractice  B opens the radial build wheel (the reticle gives way to
//             it, no build list on the console); the mouse's motion leans its
//             stick onto the Target Dummy's slice: that slice lit; a click
//             holds the dummy out and the act key puts it down (the server
//             builds it: no blueprint, 10 ore). Then the gun: the server's
//             dev line puts a complete Citadel ahead (`citadel here`); the
//             pilot opens its door (the act key at the doorstep), walks into
//             the hall to gun 1's station and takes it (the act key); `dummy here 15` stands a
//             Target Dummy 15 cells out along the gun's aim. A short burst
//             into the ground in front of it first; then the look turned
//             onto it, pitched by the core's ballistic solve
//             (`Rules::ballisticPitch`), the trigger held: rounds fly
//             (`Fired`), strike the dummy, it counts its hits and falls; let
//             go, it stands again with its hits cleared. Every move through
//             the pawn's own hooks (`OnKey`, `OnLookTaken`, the act key, the
//             look, the move keys).
//             Between them (C3, docs/guns-and-practice.md "Limits per region
//             and demolish"): eleven more dummies (dev) make the region's
//             twelve, the thirteenth refused "in this region"; the pilot walks
//             to the one it built, the sight on it: "HOLD E · DEMOLISH TARGET
//             DUMMY · +5 ORE"; E held 0.7 s and let go: nothing; held 1.5 s:
//             gone, 5 ore back, the toast; then a thirteenth goes up. With
//             -AcMmoShots=DIR -AcMmoShotsC3: the prompt (c3-demolish-prompt),
//             the ring part full (c3-demolish-ring) and the toast
//             (c3-demolish-toast).
//             With -AcMmoShots=DIR -AcMmoShotsC2: a PNG of the wheel open on
//             the dummy (c2-wheel.png), the burst into the ground
//             (c2-ground.png), the rounds in flight (c2-tracers.png), their
//             impacts on the plate (c2-impact.png) and the dummy down
//             (c2-dummy-down.png), all from gun 1's feed.
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR --headless -- -AcFlowTest=mmopractice
#include "AcGameFlow.h"

#include "AcBuildWheel.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotBuild.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"

#include "Defence.h"
#include "Rules.h"
#include "Simulation.h"

#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
	constexpr double SeatSeconds = 20;
	constexpr double AskSeconds = 8;
	constexpr double WalkSeconds = 25;
	/// Rounds at 1 a round on 150 hp, spun up: about six seconds.
	constexpr double FallSeconds = 20;
	/// How far out the dummy stands (close enough for the feed's pictures).
	constexpr double DummyCells = 15;

	/// The step's own memory.
	struct FPractice
	{
		int32 Slice = INDEX_NONE;
		bool bShot = false;
		double ShotAt = -1;
		double Settled = -1;
		bool bAsked = false;
		bool bPressed = false;
		int64 Citadel = -1;
		int64 Dummy = -1;
		/// The dummy the pilot put down from the wheel.
		int64 Built = -1;
		int64 Hits = 0;
		/// C3: dummies asked for, and the wallet's ore before the demolish.
		int32 Dummies = 0;
		/// How many stood when the last was asked for.
		int32 AskedAt = -1;
		int64 Ore = 0;
		/// Where the pilot's own dummy stood (the thirteenth goes there).
		ac::Vec2 BuiltAt{0, 0};
		int32 Rounds = 0;
		int32 Struck = 0;
		double FiringSince = -1;
		/// The way the pilot walks off the pad to put the dummy down.
		double Heading = 0;
		/// The burst into the ground before the dummy: 0 aiming, 1 firing,
		/// 2 spinning down, 3 done; and when its stage began.
		int32 Ground = 0;
		double GroundAt = -1;
		/// The pictures taken while firing.
		bool bTracers = false;
		bool bImpact = false;
		/// The command line before the dev rig was added (put back after).
		FString CommandLineWas;
	};
	FPractice Practice;

	bool Wanted() { return FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2")); }
	bool WantedC3() { return FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC3")); }

	/// -AcMmoShots=DIR with -AcMmoShotsC2: a PNG of the moment `Name` there.
	bool Picture(const TCHAR* Name)
	{
		FString Dir;
		if (!Wanted() || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return false;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
		return true;
	}

	/// -AcMmoShots=DIR with -AcMmoShotsC3: the demolish's pictures.
	bool Picture3(const TCHAR* Name)
	{
		FString Dir;
		if (!WantedC3() || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return false;
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
		Practice.Settled = -1;
		Practice.bAsked = false;
		Practice.bPressed = false;
	};
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UAcPilotOverlaySubsystem* Overlay = UAcPilotOverlaySubsystem::Get(this);
	UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
	auto Finish = [&] {
		if (Pilot)
		{
			if (Pilot->OnKey && Build && (Build->MenuOpen() || Build->Placing())) Pilot->OnKey(TEXT("esc"));
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			Pilot->Press(false);
		}
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
		const double Settle = Wanted() ? 8.0 : 1.0;
		if (Seated() && Mmo->Here() && Mmo->Progress() && Build && Since() > Settle) Advance();
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::Simulation* S = M ? &M->simulation() : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!Seated() || !Build || !Pilot->OnKey || !S || !U)
	{
		Waited(TEXT("still driving"), 4);
		return;
	}
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	auto Settle = [&](const double Seconds) {
		if (Practice.Settled < 0) Practice.Settled = Now;
		return Now - Practice.Settled >= Seconds;
	};
	auto Press = [&] {
		Pilot->Press(true);
		Pilot->Press(false);
		Practice.bPressed = true;
	};
	auto Structure = [&](const int64 Id) -> const ac::Structure* {
		for (const ac::Structure& B : S->state.structures)
			if (B.id == Id) return &B;
		return nullptr;
	};
	switch (T.Phase)
	{
	case 2:
	{
		// B: the wheel opens, nothing picked inside the dead zone.
		Check(Pilot->OnKey(TEXT("b")) && Build->MenuOpen(), TEXT("practice: B opens the build wheel"));
		Check(Build->WheelPick() == INDEX_NONE, TEXT("practice: the wheel opens with nothing picked"));
		const TArray<FAcWheelSlice>& Slices = Build->WheelSlices();
		Practice.Slice = Slices.IndexOfByPredicate([](const FAcWheelSlice& X) { return X.Kind == ac::StructureKind::dummy; });
		Check(Practice.Slice != INDEX_NONE && Slices[Practice.Slice].State == FAcWheelSlice::EState::Build,
			TEXT("practice: the Target Dummy's slice, free to build"));
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
		// A click: the dummy held out, the wheel gone; the act key puts it down.
		if (!Practice.bAsked)
		{
			Practice.bAsked = true;
			Pilot->Press(true);
			Pilot->Press(false);
			Check(!Build->MenuOpen() && Build->Placing() == ac::StructureKind::dummy, TEXT("practice: a click holds the Target Dummy out"));
			Practice.Heading = Pilot->Yaw();
			return;
		}
		// Off the start pad's plate: a short walk on, a stop, the act key;
		// again, turning a little each time (round a rock, away from a
		// building), until a spot takes it.
		if (Build->Placing())
		{
			const double Leg = std::fmod(Since(), 2.0);
			const double Way = Practice.Heading + 0.9 * std::floor(Since() / 2.0);
			Pilot->SetTestInput(Leg < 1.2 ? FVector2D(0, -1) : FVector2D::ZeroVector, Way);
			if (Leg < 1.7) Practice.bPressed = false;
			else if (!Practice.bPressed) Press();
		}
		else Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		bool bBuilt = false;
		for (const ac::Structure& B : S->state.structures)
		{
			if (B.kind == ac::StructureKind::dummy && B.owner == ac::Pilot::player && ac::distance(B.position, U->position) < 6)
			{
				bBuilt = true;
				Practice.Built = B.id;
			}
		}
		if (bBuilt && !Build->Placing())
		{
			Check(true, TEXT("practice: the act key put a Target Dummy down (no blueprint)"));
			// B, B: open and shut with nothing held.
			Pilot->OnKey(TEXT("b"));
			Check(Build->MenuOpen() && Build->WheelPick() == INDEX_NONE, TEXT("practice: B opens it again, the stick back in the middle"));
			Pilot->OnKey(TEXT("b"));
			Check(!Build->MenuOpen() && !Build->Placing(), TEXT("practice: B again closes it"));
			Advance();
		}
		else
		{
			Waited(FString::Printf(TEXT("practice: a Target Dummy built by the pilot (\"%s\")"), Info && Info->Note ? **Info->Note : TEXT("")),
				WalkSeconds);
		}
		return;
	}
	case 5:
	{
		// C3: more dummies here (dev) to the region's twelve, a thirteenth
		// refused ("in this region"). One dev line at a time (the control
		// file holds one), each once the last stands, along a line behind the
		// pilot (the Citadel goes up ahead after, where it always has),
		// turned a little when the ground runs out.
		int32 Here = 0;
		for (const ac::Structure& B : S->state.structures) Here += B.kind == ac::StructureKind::dummy && B.owner == ac::Pilot::player ? 1 : 0;
		if (!Practice.bAsked)
		{
			Practice.bAsked = true;
			Practice.Dummies = 0;
			Practice.ShotAt = -1;
			Practice.AskedAt = -1;
			Practice.Heading = U->heading;
		}
		Pilot->SetTestInput(FVector2D::ZeroVector, Practice.Heading + UE_DOUBLE_PI + 0.5 * (Practice.Dummies / 12));
		if (Here < 12)
		{
			if (Practice.Dummies >= 48)
			{
				Check(false, FString::Printf(TEXT("demolish: twelve dummies in the region (%d after %d asked)"), Here, Practice.Dummies));
				Finish();
				return;
			}
			// The last one seen first (or no ground for it), never one too many.
			if (!Settle(1.0) || (Here <= Practice.AskedAt && Now - Practice.ShotAt < 2.0)) return;
			Practice.ShotAt = Now;
			Practice.AskedAt = Here;
			if (!Mmo->DevClock(FString::Printf(TEXT("dummy here %d"), 6 + 3 * (Practice.Dummies % 12))))
			{
				Check(false, TEXT("demolish: a Target Dummy asked for (dev)"));
				Finish();
				return;
			}
			Practice.Dummies += 1;
			return;
		}
		if (Now - Practice.ShotAt < 1.0) return;
		const FString Why = AcProgressHud::BlueprintRefusal(Mmo, ac::StructureKind::dummy);
		Check(Why.Contains(TEXT("limit reached: 12 in this region")),
			FString::Printf(TEXT("demolish: at twelve the thirteenth is refused (\"%s\")"), *Why));
		Advance();
		return;
	}
	case 6:
	{
		// To the dummy the pilot built, the sight on it: the prompt.
		const ac::Structure* D = Structure(Practice.Built);
		if (!D)
		{
			Waited(TEXT("demolish: the pilot's dummy"), 4);
			return;
		}
		Practice.BuiltAt = D->position;
		const double Yaw = std::atan2(D->position.y - U->position.y, D->position.x - U->position.x);
		const bool bClose = ac::distance(D->position, U->position) < ac::Rules::radius(D->kind) + 1.0;
		Pilot->SetTestInput(bClose ? FVector2D::ZeroVector : FVector2D(0, -1), Yaw);
		if (!bClose || Pilot->DemolishTarget() != Practice.Built)
		{
			// Looking down onto the post a little at a time until the sight is on it.
			if (bClose && Pilot->ViewPitch() > -0.9) Pilot->Look(0.0, 20.0);
			Waited(FString::Printf(TEXT("demolish: the sight on the dummy within reach (%.1f cells, sight on #%lld)"),
				ac::distance(D->position, U->position), (long long)Pilot->DemolishTarget()), WalkSeconds);
			return;
		}
		if (!Settle(0.6)) return;
		const FString Want = TEXT("HOLD E · DEMOLISH TARGET DUMMY · +5 ORE");
		Check(Info && Info->Demolish && *Info->Demolish == Want,
			FString::Printf(TEXT("demolish: the prompt reads \"%s\" (\"%s\")"), *Want, Info && Info->Demolish ? **Info->Demolish : TEXT("")));
		Picture3(TEXT("c3-demolish-prompt"));
		Practice.Ore = S->state.players[size_t(ac::Pilot::player)].ore;
		Advance();
		return;
	}
	case 7:
	{
		// (A second for the picture's frame first: a hitch would run the hold's clock.)
		if (!Practice.bPressed)
		{
			if (Since() < 1.0) return;
			// E down: no act (the click's would strike the dummy), the hold starts.
			Pilot->Press(true, true);
			Check(Pilot->DemolishFill().IsSet(), TEXT("demolish: E on it starts the hold"));
			Practice.bPressed = true;
			Practice.Settled = Now;
			return;
		}
		// Let go at 0.7 s: nothing.
		if (Now - Practice.Settled < 0.7) return;
		if (!Practice.bShot)
		{
			Practice.bShot = true;
			const TOptional<double> Fill = Pilot->DemolishFill();
			Check(Fill && *Fill > 0.3 && *Fill < 0.7, FString::Printf(TEXT("demolish: the ring part full at 0.7 s (%.2f)"), Fill.Get(-1)));
			Check(Info && Info->DemolishFill.IsSet(), TEXT("demolish: the cockpit draws the ring"));
			Picture3(TEXT("c3-demolish-ring"));
			return;
		}
		// Let go a few frames after the picture (it is taken on the next frame).
		if (Pilot->DemolishFill().IsSet())
		{
			if (Now - Practice.Settled < 0.9) return;
			Pilot->Press(false);
			Check(!Pilot->DemolishFill().IsSet(), TEXT("demolish: letting go ends the hold"));
			return;
		}
		if (Now - Practice.Settled < 2.6) return;
		Check(Structure(Practice.Built) != nullptr, TEXT("demolish: let go early, the dummy stands"));
		Pilot->Press(true, true);
		Advance();
		return;
	}
	case 8:
	{
		// Held through: gone, 5 ore back, the toast.
		if (Structure(Practice.Built))
		{
			Waited(TEXT("demolish: the dummy gone after the hold"), ac::Rules::demolishHold + AskSeconds);
			return;
		}
		Pilot->Press(false);
		// (The shatter's flash gone first, for the picture.)
		if (!Settle(1.6)) return;
		const int64 Back = S->state.players[size_t(ac::Pilot::player)].ore - Practice.Ore;
		Check(Back == 5, FString::Printf(TEXT("demolish: 5 ore back in the wallet (%lld)"), (long long)Back));
		const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		const TArray<FString> Toasts = Hud && Hud->Widget() ? Hud->Widget()->ToastTitles() : TArray<FString>();
		Check(Toasts.Contains(TEXT("Target Dummy demolished · +5 ore")),
			FString::Printf(TEXT("demolish: the toast (%s)"), *FString::Join(Toasts, TEXT(" | "))));
		Picture3(TEXT("c3-demolish-toast"));
		Advance();
		return;
	}
	case 9:
	{
		// Eleven here: the thirteenth (counting all built) goes up.
		// Stepped back off the spot first (the server's "Step back first").
		if (!Practice.bAsked && ac::distance(U->position, Practice.BuiltAt) < 2.5)
		{
			const ac::Vec2 Off = Practice.BuiltAt - U->position;
			Pilot->SetTestInput(FVector2D(0, 1), std::atan2(Off.y, Off.x));
			Waited(TEXT("demolish: a step back from the spot"), WalkSeconds);
			return;
		}
		if (!Practice.bAsked)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			const FString Why = AcProgressHud::BlueprintRefusal(Mmo, ac::StructureKind::dummy);
			Check(Why.IsEmpty(), FString::Printf(TEXT("demolish: a dummy may be built again (\"%s\")"), *Why));
			// Where the demolished one stood: free, and out of the later walks' way.
			const std::optional<ac::Vec2> At = S->snap(ac::StructureKind::dummy, Practice.BuiltAt);
			if (At && !S->placementRefusal(ac::StructureKind::dummy, *At) && !Mmo->PadPlateRefusal(ac::StructureKind::dummy, *At))
				Practice.bAsked = Mmo->Build(ac::StructureKind::dummy, *At);
			Check(Practice.bAsked, TEXT("demolish: a spot for the thirteenth"));
			Practice.Dummies = 0;
			return;
		}
		int32 Here = 0;
		for (const ac::Structure& B : S->state.structures) Here += B.kind == ac::StructureKind::dummy && B.owner == ac::Pilot::player ? 1 : 0;
		if (Here >= 12)
		{
			// Turned back the way it faced, away from the line of dummies,
			// for the Citadel ahead.
			Pilot->SetTestInput(FVector2D::ZeroVector, Practice.Heading);
			if (!Settle(1.0)) return;
			Check(true, TEXT("demolish: the thirteenth dummy built, twelve in the region again"));
			Advance();
		}
		else Waited(FString::Printf(TEXT("demolish: the thirteenth built (%d here)"), Here), AskSeconds);
		return;
	}
	case 10:
		// A complete Citadel ahead (dev), its door opened.
		if (!Practice.bAsked)
		{
			Practice.bAsked = true;
			Check(Mmo->DevClock(TEXT("citadel here")), TEXT("practice: a Citadel asked for ahead of the pilot (dev)"));
			return;
		}
		for (const ac::Structure& B : S->state.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.complete() && B.door && B.guns && ac::distance(B.position, U->position) < 16)
				Practice.Citadel = B.id;
		}
		if (Practice.Citadel >= 0 && Since() > 1.0) Advance();
		else Waited(TEXT("practice: a Citadel with guns by the pilot"), AskSeconds);
		return;
	case 11:
	{
		// To the doorstep; the act key opens the door.
		const ac::Structure* C = Structure(Practice.Citadel);
		if (!C || !C->door)
		{
			Waited(TEXT("practice: the Citadel"), 4);
			return;
		}
		if (S->doorOpen(*C))
		{
			Check(true, TEXT("practice: the door opened on the act key"));
			Advance();
			return;
		}
		const ac::Vec2 D = C->position + ac::Vec2(0, 7.0) - U->position;
		if (ac::length(D) > 0.5 && !Practice.bPressed)
		{
			Pilot->SetTestInput(FVector2D(0, -1), std::atan2(D.y, D.x));
			Waited(TEXT("practice: at the doorstep"), WalkSeconds);
			return;
		}
		const ac::Vec2 In = C->position + ac::Vec2(0, 5.0) - U->position;
		Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(In.y, In.x));
		if (!Settle(0.6)) return;
		if (!Practice.bPressed) Press();
		Waited(FString::Printf(TEXT("practice: the door open (%s)"), *Mmo->HallRefusal()), WalkSeconds);
		return;
	}
	case 12:
	case 13:
	{
		// Into the hall through the doorway, then to gun 1's station.
		const ac::Structure* C = Structure(Practice.Citadel);
		if (!C || !C->door)
		{
			Waited(TEXT("practice: the Citadel"), 4);
			return;
		}
		const ac::Vec2 To = T.Phase == 7 ? C->position + ac::Vec2(0, 3.8) : ac::Defence::gunStation(C->position, 0);
		const ac::Vec2 D = To - U->position;
		const double Yaw = std::atan2(D.y, D.x);
		if (ac::length(D) > (T.Phase == 7 ? 0.5 : 0.7))
		{
			Pilot->SetTestInput(FVector2D(0, -1), Yaw);
			Waited(T.Phase == 7 ? TEXT("practice: in the hall") : TEXT("practice: at gun 1's station"), WalkSeconds);
			return;
		}
		Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
		if (!Settle(0.6)) return;
		if (T.Phase == 7) Check(S->sheltered(*U), TEXT("practice: in the hall, sheltered"));
		Advance();
		return;
	}
	case 14:
		// The act key takes the gun.
		if (!Practice.bPressed) Press();
		if (U->task != ac::Unit::Task::atGun || Pilot->AtGun() != 0)
		{
			Waited(FString::Printf(TEXT("practice: at gun 1 (\"%s\", %s)"), *Prompt, *Mmo->HallRefusal()), AskSeconds);
			return;
		}
		if (!Settle(0.5)) return;
		Check(true, TEXT("practice: gun 1 manned"));
		Advance();
		return;
	case 15:
	{
		// A Target Dummy `DummyCells` out along the gun's aim (dev), the gun
		// first turned where the rounds' path that far out is clear of the
		// ground (a ledge between would take them all).
		const ac::Structure* C = Structure(Practice.Citadel);
		if (!C)
		{
			Waited(TEXT("practice: the Citadel"), 4);
			return;
		}
		const ac::Vec2 Gun = ac::Defence::cornerGun(C->position, 0);
		if (!Practice.bPressed)
		{
			Practice.bPressed = true;
			double Best = Pilot->Yaw(), BestOff = 1e9;
			const double Mid = ac::Defence::cornerAngle(0);
			// Well inside the arc (the dummy may stand up to 0.2 rad aside of
			// where it is asked), the middle preferred.
			for (double Off = -ac::Defence::gunArcHalf + 0.45; Off <= ac::Defence::gunArcHalf - 0.45; Off += 0.1)
			{
				const double Yaw = Mid + Off;
				const ac::Vec2 Spot = Gun + ac::Vec2(std::cos(Yaw), std::sin(Yaw)) * DummyCells;
				const ac::Vec3 Base(0, 0, S->groundZ(C->position));
				const ac::Vec3 Level = ac::Defence::gunMuzzle(C->position, 0, Yaw, 0.0) + Base;
				const std::optional<double> Pitch = ac::Rules::ballisticPitch(ac::length(Spot - ac::Vec2(Level.x, Level.y)),
					S->groundZ(Spot) + 1.1 - Level.z);
				if (!Pitch || *Pitch < ac::Defence::gunPitchMin || *Pitch > ac::Defence::gunPitchMax) continue;
				const ac::Vec3 Muzzle = ac::Defence::gunMuzzle(C->position, 0, Yaw, *Pitch) + Base;
				const TOptional<ac::Vec3> Hit = AcPilotText::BallisticImpact(*S, Muzzle, ac::Rules::roundVelocity(Yaw, *Pitch), C->id);
				const double Miss = Hit ? ac::distance(ac::Vec2(Hit->x, Hit->y), Spot) : 1e9;
				if (Miss + 0.3 * std::abs(Off) < BestOff + 0.3 * std::abs(std::remainder(Best - Mid, 2 * UE_DOUBLE_PI)))
				{
					BestOff = Miss;
					Best = Yaw;
				}
			}
			// (Aimed 1.1 cells over the ground there, a clear round comes down
			// a little past it; a ledge in the way takes it cells short.)
			Check(BestOff < 4.0, FString::Printf(TEXT("practice: a clear line of fire %.0f cells out (%.1f cells off)"), DummyCells, BestOff));
			Pilot->Look(std::remainder(Best - Pilot->Yaw(), 2 * UE_DOUBLE_PI) / 0.0032, 0.0);
			return;
		}
		if (!Practice.bAsked)
		{
			if (!Settle(1.0)) return;
			Practice.bAsked = true;
			Check(Mmo->DevClock(FString::Printf(TEXT("dummy here %.0f"), DummyCells)), TEXT("practice: a Target Dummy asked for along the gun's aim (dev)"));
			return;
		}
		for (const ac::Structure& B : S->state.structures)
		{
			if (B.kind == ac::StructureKind::dummy && B.id != Practice.Built && B.complete() && B.hp > 0 && !B.downUntil)
			{
				const double Dist = ac::distance(B.position, Gun);
				if (Dist > DummyCells * 0.6 && Dist < DummyCells * 1.5) Practice.Dummy = B.id;
			}
		}
		if (Practice.Dummy >= 0) Advance();
		else Waited(FString::Printf(TEXT("practice: a standing Target Dummy %.0f cells out"), DummyCells), AskSeconds);
		return;
	}
	case 16:
	{
		// The look onto its plate: the yaw by the mouse, the pitch by the
		// core's ballistic solve; then the trigger held.
		const ac::Structure* C = Structure(Practice.Citadel);
		const ac::Structure* D = Structure(Practice.Dummy);
		if (!C || !D)
		{
			Waited(TEXT("practice: the gun and the dummy"), 4);
			return;
		}
		const ac::Vec2 Gun = ac::Defence::cornerGun(C->position, 0);
		const ac::Vec2 To = D->position - Gun;
		const double Yaw = std::atan2(To.y, To.x);
		Pilot->Look(std::remainder(Yaw - Pilot->Yaw(), 2 * UE_DOUBLE_PI) / 0.0032, 0.0);
		// First a burst into the ground 2.5 cells short of the dummy: the
		// impacts' plume, flash and sparks (c2-ground).
		if (Practice.Ground < 3)
		{
			const ac::Vec2 Short = D->position - ac::normalize(To) * 2.5;
			const ac::Vec3 From = ac::Defence::gunMuzzle(C->position, 0, Pilot->Yaw(), Pilot->ViewPitch()) + ac::Vec3(0, 0, S->groundZ(C->position));
			if (const std::optional<double> Down = ac::Rules::ballisticPitch(ac::length(Short - ac::Vec2(From.x, From.y)), S->groundZ(Short) - From.z))
			{
				Pilot->Look(0.0, (Pilot->ViewPitch() - *Down) / 0.0026);
			}
			if (Practice.Ground == 0)
			{
				if (!Settle(1.0)) return;
				Pilot->Press(true);
				Practice.Ground = 1;
				Practice.GroundAt = Now;
			}
			else if (Practice.Ground == 1 && Now - Practice.GroundAt > 1.6)
			{
				Picture(TEXT("c2-ground"));
				Pilot->Press(false);
				Practice.Ground = 2;
				Practice.GroundAt = Now;
			}
			else if (Practice.Ground == 2 && Now - Practice.GroundAt > 1.2)
			{
				Practice.Ground = 3;
				Practice.Settled = -1;
			}
			return;
		}
		// (The muzzle over the Citadel's ground, as the core fires it.)
		const ac::Vec3 Muzzle = ac::Defence::gunMuzzle(C->position, 0, Pilot->Yaw(), Pilot->ViewPitch()) + ac::Vec3(0, 0, S->groundZ(C->position));
		const double Plate = S->groundZ(D->position) + ac::Rules::height(D->kind) * 0.6;
		const std::optional<double> Pitch = ac::Rules::ballisticPitch(ac::length(D->position - ac::Vec2(Muzzle.x, Muzzle.y)), Plate - Muzzle.z);
		if (Pitch) Pilot->Look(0.0, (Pilot->ViewPitch() - *Pitch) / 0.0026);
		if (!Settle(1.0)) return;
		{
			const ac::Vec3 Now3 = ac::Defence::gunMuzzle(C->position, 0, Pilot->Yaw(), Pilot->ViewPitch()) + ac::Vec3(0, 0, S->groundZ(C->position));
			const TOptional<ac::Vec3> Lands =
				AcPilotText::BallisticImpact(*S, Now3, ac::Rules::roundVelocity(Pilot->Yaw(), Pilot->ViewPitch()), C->id);
			UE_LOG(LogAutocraft, Display,
				TEXT("flowtest: practice: aim: muzzle (%.2f, %.2f, %.2f), dummy (%.2f, %.2f) ground %.2f plate %.2f, solve %.4f, view %.4f, gun pitch %.4f, lands (%.2f, %.2f, %.2f)"),
				Now3.x, Now3.y, Now3.z, D->position.x, D->position.y, S->groundZ(D->position), Plate, Pitch.value_or(-9), Pilot->ViewPitch(),
				C->guns ? (*C->guns)[0].pitch : -9.0, Lands ? Lands->x : 0, Lands ? Lands->y : 0, Lands ? Lands->z : 0);
		}
		Check(Pitch.has_value(), TEXT("practice: the dummy within the rounds' reach"));
		Check(FMath::Abs(std::remainder(Pilot->Yaw() - Yaw, 2 * UE_DOUBLE_PI)) < 0.02,
			FString::Printf(TEXT("practice: the gun turned onto the dummy (%.3f off)"), std::remainder(Pilot->Yaw() - Yaw, 2 * UE_DOUBLE_PI)));
		Practice.Rounds = 0;
		Practice.Struck = 0;
		Practice.FiringSince = Now;
		Pilot->Press(true);
		Advance();
		return;
	}
	case 17:
	{
		// Held: rounds fly, strike the dummy, it falls.
		for (const ac::GameEvent& E : Sim->LastFrame().AllEvents)
		{
			if (const auto* Fi = E.as<ac::GameEvent::Fired>(); Fi && Fi->shooter == Practice.Citadel) ++Practice.Rounds;
			if (const auto* I = E.as<ac::GameEvent::Impact>(); I && I->victim == Practice.Dummy) ++Practice.Struck;
		}
		const ac::Structure* D = Structure(Practice.Dummy);
		if (!D)
		{
			Waited(TEXT("practice: the dummy"), 4);
			return;
		}
		Practice.Hits = FMath::Max(Practice.Hits, D->hits);
		if (!Practice.bTracers && Now - Practice.FiringSince > 1.2)
		{
			Practice.bTracers = true;
			Picture(TEXT("c2-tracers"));
		}
		if (!Practice.bImpact && Now - Practice.FiringSince > 2.6)
		{
			Practice.bImpact = true;
			Picture(TEXT("c2-impact"));
		}
		if (!D->downUntil)
		{
			Waited(FString::Printf(TEXT("practice: the dummy down (%d rounds fired, %d struck it, %lld hits)"), Practice.Rounds, Practice.Struck,
				(long long)Practice.Hits), FallSeconds);
			return;
		}
		if (!Practice.bShot)
		{
			Practice.bShot = true;
			Practice.ShotAt = Now;
			return;
		}
		if (Now - Practice.ShotAt < 0.35) return;
		Picture(TEXT("c2-dummy-down"));
		Pilot->Press(false);
		Check(Practice.Rounds > 30, FString::Printf(TEXT("practice: the gun fired rounds (%d)"), Practice.Rounds));
		Check(Practice.Struck > 0 && Practice.Hits > 0,
			FString::Printf(TEXT("practice: rounds struck the dummy (%d impacts, %lld hits counted)"), Practice.Struck, (long long)Practice.Hits));
		// (Headless there is no screen to put the tag on.)
		if (FApp::CanEverRender())
		{
			Check(Info && Info->Dummies.ContainsByPredicate([](const FAcDummyTag& X) { return X.bDown; }),
				TEXT("practice: its tag reads DOWN on the screen"));
		}
		Check(true, TEXT("practice: the dummy fell"));
		Advance();
		return;
	}
	case 18:
	{
		// Let go: it stands again, full, its hits cleared.
		const ac::Structure* D = Structure(Practice.Dummy);
		if (D && !D->downUntil && D->hp >= ac::Rules::hp(D->kind) - 0.5)
		{
			Check(D->hits == 0, FString::Printf(TEXT("practice: it stood again, its hits cleared (%lld)"), (long long)D->hits));
			Finish();
			return;
		}
		Waited(TEXT("practice: the dummy standing again"), ac::Rules::dummyDownTime + 6);
		return;
	}
	default:
		Finish();
		return;
	}
}
