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
//             the hall to gun 1's station and takes it (the act key); `dummy here` stands a
//             Target Dummy 20 cells out along the gun's aim. The look turned
//             onto it, pitched by the core's ballistic solve
//             (`Rules::ballisticPitch`), the trigger held: rounds fly
//             (`Fired`), strike the dummy, it counts its hits and falls; let
//             go, it stands again with its hits cleared. Every move through
//             the pawn's own hooks (`OnKey`, `OnLookTaken`, the act key, the
//             look, the move keys).
//             With -AcMmoShots=DIR -AcMmoShotsC2: a PNG of the wheel open on
//             the dummy (c2-wheel.png), the rounds in flight (c2-tracers.png),
//             their impacts on the plate (c2-impact.png) and the dummy down
//             (c2-dummy-down.png).
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
#include "AcSimSubsystem.h"

#include "Defence.h"
#include "Rules.h"
#include "Simulation.h"

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
	constexpr double AskSeconds = 8;
	constexpr double WalkSeconds = 25;
	/// Rounds at 1 a round on 150 hp, spun up: about six seconds.
	constexpr double FallSeconds = 20;

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
		int32 Rounds = 0;
		int32 Struck = 0;
		double FiringSince = -1;
		/// The way the pilot walks off the pad to put the dummy down.
		double Heading = 0;
		/// The pictures taken while firing.
		bool bTracers = false;
		bool bImpact = false;
		/// The command line before the dev rig was added (put back after).
		FString CommandLineWas;
	};
	FPractice Practice;

	bool Wanted() { return FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC2")); }

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
		// again until a spot takes it.
		if (Build->Placing())
		{
			const double Leg = std::fmod(Since(), 2.0);
			Pilot->SetTestInput(Leg < 1.2 ? FVector2D(0, -1) : FVector2D::ZeroVector, Practice.Heading);
			if (Leg < 1.7) Practice.bPressed = false;
			else if (!Practice.bPressed) Press();
		}
		else Pilot->SetTestInput(FVector2D::ZeroVector, Practice.Heading);
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
	case 6:
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
	case 7:
	case 8:
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
	case 9:
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
	case 10:
	{
		// A Target Dummy 20 cells out along the gun's aim (dev), the gun
		// first turned where the rounds' path to 20 cells is clear of the
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
			for (double Off = -ac::Defence::gunArcHalf + 0.15; Off <= ac::Defence::gunArcHalf - 0.15; Off += 0.1)
			{
				const double Yaw = Mid + Off;
				const ac::Vec2 Spot = Gun + ac::Vec2(std::cos(Yaw), std::sin(Yaw)) * 20.0;
				const ac::Vec3 Level = ac::Defence::gunMuzzle(C->position, 0, Yaw, 0.0);
				const std::optional<double> Pitch = ac::Rules::ballisticPitch(ac::length(Spot - ac::Vec2(Level.x, Level.y)),
					S->groundZ(Spot) + 1.1 - Level.z);
				if (!Pitch || *Pitch < ac::Defence::gunPitchMin || *Pitch > ac::Defence::gunPitchMax) continue;
				const ac::Vec3 Muzzle = ac::Defence::gunMuzzle(C->position, 0, Yaw, *Pitch);
				const TOptional<ac::Vec3> Hit = AcPilotText::BallisticImpact(*S, Muzzle, ac::Rules::roundVelocity(Yaw, *Pitch), C->id);
				const double Miss = Hit ? ac::distance(ac::Vec2(Hit->x, Hit->y), Spot) : 1e9;
				if (Miss < BestOff)
				{
					BestOff = Miss;
					Best = Yaw;
				}
			}
			// (Aimed 1.1 cells over the ground there, a clear round comes down
			// a little past it; a ledge in the way takes it cells short.)
			Check(BestOff < 4.0, FString::Printf(TEXT("practice: a clear line of fire 20 cells out (%.1f cells off)"), BestOff));
			Pilot->Look(std::remainder(Best - Pilot->Yaw(), 2 * UE_DOUBLE_PI) / 0.0032, 0.0);
			return;
		}
		if (!Practice.bAsked)
		{
			if (!Settle(1.0)) return;
			Practice.bAsked = true;
			Check(Mmo->DevClock(TEXT("dummy here")), TEXT("practice: a Target Dummy asked for along the gun's aim (dev)"));
			return;
		}
		for (const ac::Structure& B : S->state.structures)
		{
			if (B.kind == ac::StructureKind::dummy && B.id != Practice.Built && B.complete() && B.hp > 0 && !B.downUntil)
			{
				const double Dist = ac::distance(B.position, Gun);
				if (Dist > 12 && Dist < 30) Practice.Dummy = B.id;
			}
		}
		if (Practice.Dummy >= 0) Advance();
		else Waited(TEXT("practice: a standing Target Dummy 20 cells out"), AskSeconds);
		return;
	}
	case 11:
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
		const ac::Vec3 Muzzle = ac::Defence::gunMuzzle(C->position, 0, Pilot->Yaw(), Pilot->ViewPitch());
		const double Plate = S->groundZ(D->position) + ac::Rules::height(D->kind) * 0.6;
		const std::optional<double> Pitch = ac::Rules::ballisticPitch(ac::length(D->position - ac::Vec2(Muzzle.x, Muzzle.y)), Plate - Muzzle.z);
		if (Pitch) Pilot->Look(0.0, (Pilot->ViewPitch() - *Pitch) / 0.0026);
		if (!Settle(1.0)) return;
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
	case 12:
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
		Check(Info && Info->Dummies.ContainsByPredicate([](const FAcDummyTag& X) { return X.bDown; }),
			TEXT("practice: its tag reads DOWN on the screen"));
		Check(true, TEXT("practice: the dummy fell"));
		Advance();
		return;
	}
	case 13:
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
