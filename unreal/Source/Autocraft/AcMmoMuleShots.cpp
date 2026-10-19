// -AcFlowTest's Mule pictures (C4, docs/haul-and-ferry.md section 4; the
// Mule's flow `mmomule` is AcMmoMuleFlowTest.cpp), over the local server on
// the first world (build it first: make -C unreal/server). It starts a new
// world and waits to be seated in the Prospector on the start pad.
//
//   mmomuleshots  the server's dev lines stage the scene: `mule here` (the
//             pilot's Mule beside it on open ground) and `mule load 380 120`.
//             With -AcMmoShots=DIR, PNGs: the Mule beside the Prospector
//             (c4-mule-beside), the screen on the flank toward it
//             (c4-mule-screen); the pilot walks to the perch and climbs on
//             through the act key (E · CLIMB ON): the driver on the perch
//             (c4-mule-driver, third person: the rider is drawn), the
//             third-person view (c4-mule-tps) and the first-person one
//             standing on the perch (c4-mule-fp); it drives on (W): rolling,
//             its dust (c4-mule-dust). Then `citadel here` (a Citadel ahead)
//             and `mule door` (the Mule out of its open door, facing in, the
//             pilot on foot by the hopper); the pilot climbs on again and
//             drives into the doorway (c4-mule-doorway) and on into the
//             hall, where the load is booked (c4-mule-booked: the toast).
//             Checks: the prompts, the HUD's line, the boarding, the booking
//             (the haul empty).
//
// Run hidden in a game process, by day (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmomuleshots -AcMmoShots={out} -AcMmoClockHour=11
#include "AcGameFlow.h"

#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Drive.h"
#include "Mule.h"
#include "Rules.h"
#include "Simulation.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
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
	constexpr double WalkSeconds = 30;
	constexpr double AskSeconds = 8;
	/// How long each picture's moment is held before it is taken, and after.
	constexpr double HoldSeconds = 1.5;

	struct FMuleShots
	{
		int64 Citadel = -1;
		int64 Mule = -1;
		double Settled = -1;
		bool bPressed = false;
		bool bShot = false;
		double ShotAt = -1;
		TWeakObjectPtr<ACameraActor> Cam;
	};
	FMuleShots Staged;

	/// -AcMmoShots=DIR: a PNG of the moment `Name` there (false: no directory).
	bool Picture(const TCHAR* Name)
	{
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return false;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
		return true;
	}
}

void UAcGameFlowSubsystem::TickMmoMuleShots(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
		Staged.bPressed = false;
		Staged.Settled = -1;
		Staged.bShot = false;
		Staged.ShotAt = -1;
	};
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UAcPilotOverlaySubsystem* Overlay = UAcPilotOverlaySubsystem::Get(this);
	auto ViewPilot = [&] {
		if (PC && Pilot && PC->GetViewTarget() != Pilot) PC->SetViewTarget(Pilot);
	};
	auto Finish = [&] {
		if (Pilot)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			Pilot->Press(false);
		}
		ViewPilot();
		if (Staged.Cam.IsValid()) Staged.Cam->Destroy();
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
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Staged = FMuleShots();
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			Check(Mmo->DevClock(TEXT("mule here")), TEXT("a Mule asked for beside the pilot (dev)"));
			Advance();
		}
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}

	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::Simulation* S = M ? &M->simulation() : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!Seated() || !S || !U)
	{
		Waited(TEXT("still driving"), 6);
		return;
	}
	// The Citadel staged ahead of the Mule (from phase 10, not the realm's own nearby).
	const ac::Structure* C = nullptr;
	for (const ac::Structure& B : S->state.structures)
	{
		if (T.Phase < 10) break;
		if (B.kind != ac::StructureKind::citadel || !B.complete() || !B.door) continue;
		if (Staged.Citadel >= 0 ? B.id == Staged.Citadel : ac::distance(B.position, U->position) < 16) C = &B;
	}
	if (C) Staged.Citadel = C->id;
	const TOptional<int64> Own = Mmo->OwnMule();
	const ac::Unit* Mule = Own ? M->unit(*Own) : nullptr;
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	auto Ground = [&](const ac::Vec2 P) { return Terrain ? Terrain->FieldHeight(P) : 0.0; };

	// Walk (on foot) to a world point: the move keys forward, the mouse's yaw onto it.
	auto WalkTo = [&](const ac::Vec2 To, const double Near) {
		const ac::Vec2 D = To - U->position;
		const double Yaw = std::atan2(D.y, D.x);
		const bool bThere = ac::length(D) <= Near;
		Pilot->SetTestInput(bThere ? FVector2D::ZeroVector : FVector2D(0, -1), Yaw);
		return bThere;
	};
	auto Face = [&](const ac::Vec2 To) {
		const ac::Vec2 D = To - U->position;
		Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.y, D.x));
	};
	// Drive the Mule to a world point as a car (C4: W/S drive, A/D steer
	// while it rolls; `ac::steerTo` backs and fills for a point inside its
	// tightest circle).
	auto DriveTo = [&](const ac::Vec2 To, const double Near) {
		if (ac::distance(To, U->position) <= Near)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			return true;
		}
		const ac::SteerTo S = ac::steerTo(U->position, U->heading, To, ac::Pilot::turnRadius(U->kind));
		Pilot->SetTestInput(FVector2D(S.steer, -S.way), Pilot->Yaw());
		return false;
	};
	auto Settle = [&](const double Seconds) {
		if (Staged.Settled < 0) Staged.Settled = Now;
		return Now - Staged.Settled >= Seconds;
	};
	auto Press = [&] {
		Pilot->Press(true, true);
		Pilot->Press(false, true);
		Staged.bPressed = true;
	};
	// A free camera at `Eye` (world cells, `Up` over the ground there)
	// looking at `At` (`AtUp` over its ground); the view handed to it.
	auto Camera = [&](const ac::Vec2 Eye, const double Up, const ac::Vec2 At, const double AtUp, const float Fov) {
		if (!Staged.Cam.IsValid())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Staged.Cam = GetWorld()->SpawnActor<ACameraActor>(Params);
			Staged.Cam->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
		Staged.Cam->GetCameraComponent()->SetFieldOfView(Fov);
		const FVector E = AcSpace::ToWorld(Eye, Ground(Eye) + Up), A = AcSpace::ToWorld(At, Ground(At) + AtUp);
		Staged.Cam->SetActorLocationAndRotation(E, (A - E).Rotation());
		if (PC && PC->GetViewTarget() != Staged.Cam.Get()) PC->SetViewTarget(Staged.Cam.Get());
	};
	// Held a moment, the picture, a moment more: true once done.
	auto Shot = [&](const TCHAR* Name) {
		if (Staged.ShotAt < 0) Staged.ShotAt = Now;
		if (!Staged.bShot && Now - Staged.ShotAt >= HoldSeconds)
		{
			Picture(Name);
			Staged.bShot = true;
		}
		return Staged.bShot && Now - Staged.ShotAt >= 2 * HoldSeconds;
	};
	// The Mule's own frame: ahead along its heading, `Side` to its right as the screen shows it.
	auto MuleAt = [&](const ac::Unit& Mu, const double Ahead, const double Side) {
		const ac::Vec2 A(std::cos(Mu.heading), std::sin(Mu.heading)), R(-std::sin(Mu.heading), std::cos(Mu.heading));
		return Mu.position + A * Ahead + R * Side;
	};
	// The pilot's own side of the Mule (+1 its right as `MuleAt` counts it).
	auto SideOf = [&](const ac::Unit& Mu) {
		return ac::dot(U->position - Mu.position, ac::Vec2(-std::sin(Mu.heading), std::cos(Mu.heading))) >= 0 ? 1.0 : -1.0;
	};
	auto ThirdPerson = [&](const bool bOn) {
		if (bOn != Pilot->ChaseEye().IsSet()) Pilot->ToggleView();
	};

	switch (T.Phase)
	{
	case 2:
		if (!Mule)
		{
			Waited(TEXT("the pilot's Mule here"), AskSeconds);
			return;
		}
		Staged.Mule = Mule->id;
		if (!Staged.bPressed)
		{
			Check(Mmo->DevClock(TEXT("mule load 380 120")), TEXT("a load asked for in the Mule (dev)"));
			Staged.bPressed = true;
		}
		if (Mule->haul && Mule->haul->ore == 380 && Mule->haul->hydrogen == 120) Advance();
		else Waited(TEXT("the Mule's load of 380 ore, 120 MH"), AskSeconds);
		return;
	case 3:
		// Beside the Prospector (third person: its unit drawn), seen from ahead of both, on its side.
		if (!Mule) return;
		ThirdPerson(true);
		Camera(MuleAt(*Mule, 8.5, SideOf(*Mule) * 6.5), 2.6, MuleAt(*Mule, 0.0, SideOf(*Mule) * 1.3), 0.8, 38.f);
		if (Shot(TEXT("c4-mule-beside"))) Advance();
		return;
	case 4:
		// The screen on the flank toward the pilot, square on.
		if (!Mule) return;
		Camera(MuleAt(*Mule, -0.84, SideOf(*Mule) * 2.6), 0.95, MuleAt(*Mule, -0.84, 0.0), 0.93, 40.f);
		if (Shot(TEXT("c4-mule-screen")))
		{
			ViewPilot();
			ThirdPerson(false);
			Advance();
		}
		return;
	case 5:
	{
		// To the perch, and climb on (E · CLIMB ON).
		if (!Mule) return;
		if (!Staged.bPressed && !WalkTo(MuleAt(*Mule, ac::Mule::perchLocal.x, SideOf(*Mule) * 1.4), 0.4))
		{
			Waited(TEXT("at the Mule's perch"), WalkSeconds);
			return;
		}
		if (!Staged.bPressed)
		{
			Face(ac::Mule::perch(*Mule));
			if (!Settle(1.0)) return;
			Check(Info && Info->MuleLoad && Info->MuleLoad->Contains(TEXT("380 ore, 120 MH")),
				FString::Printf(TEXT("the HUD's line by the Mule (\"%s\")"), Info && Info->MuleLoad ? **Info->MuleLoad : TEXT("")));
			Check(Prompt.Contains(TEXT("CLIMB ON")), FString::Printf(TEXT("the perch's prompt (\"%s\")"), *Prompt));
			Press();
		}
		if (U->kind == ac::UnitKind::mule)
		{
			Check(Mmo->Driven() == std::optional<int64>(Staged.Mule), TEXT("driving the Mule"));
			Advance();
		}
		else Waited(TEXT("aboard the Mule"), AskSeconds);
		return;
	}
	case 6:
		// The driver on the perch (drawn in third person), from ahead and to its side.
		ThirdPerson(true);
		Camera(MuleAt(*U, 5.6, 3.6), 2.3, MuleAt(*U, 0.2, 0.0), 1.0, 40.f);
		if (Shot(TEXT("c4-mule-driver"))) Advance();
		return;
	case 7:
		ViewPilot();
		ThirdPerson(true);
		if (Staged.ShotAt < 0)
		{
			Check(Info && Info->MuleLoad && Info->MuleLoad->Contains(TEXT("In the Mule")), TEXT("the HUD's line while driving"));
			Check(Prompt.Contains(TEXT("CLIMB OFF")), FString::Printf(TEXT("the driver's prompt (\"%s\")"), *Prompt));
		}
		if (Shot(TEXT("c4-mule-tps"))) Advance();
		return;
	case 8:
		ThirdPerson(false);
		if (Shot(TEXT("c4-mule-fp"))) Advance();
		return;
	case 9:
		// On ahead: rolling, its dust from the side (third person: its driver drawn).
		ThirdPerson(true);
		Pilot->SetTestInput(FVector2D(0, -1), Pilot->Yaw());
		if (Since() < 3.0) return;
		Camera(MuleAt(*U, -6.5, -3.5), 1.6, MuleAt(*U, -1.0, 0.0), 0.5, 42.f);
		if (Shot(TEXT("c4-mule-dust")))
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			ViewPilot();
			Check(Mmo->DevClock(TEXT("citadel here")), TEXT("a Citadel asked for ahead of the Mule (dev)"));
			Advance();
		}
		return;
	case 10:
		// The Mule out of the Citadel's open door, facing in (`mule door`: the
		// pilot on foot by its hopper).
		if (!C)
		{
			Waited(TEXT("a complete Citadel by the Mule"), AskSeconds);
			return;
		}
		// Off the perch first (E · CLIMB OFF), as the server's line wants the pilot on foot.
		if (U->kind == ac::UnitKind::mule)
		{
			if (!Staged.bPressed)
			{
				Check(Prompt.Contains(TEXT("CLIMB OFF")), FString::Printf(TEXT("the driver's prompt (\"%s\")"), *Prompt));
				Press();
			}
			Waited(TEXT("off the Mule"), AskSeconds);
			return;
		}
		if (Staged.Settled < 0)
		{
			Check(Mmo->DevClock(TEXT("mule door")), TEXT("the Mule asked for at the Citadel's door (dev)"));
			Staged.Settled = Now;
		}
		if (Mule && U->kind == ac::UnitKind::prospector && C->door->open >= 0.99
			&& ac::distance(Mule->position, C->position + ac::Vec2(0, 9.5)) < 0.3)
		{
			Advance();
		}
		else Waited(TEXT("the Mule at the door, the door open, the pilot on foot"), AskSeconds);
		return;
	case 11:
		// Back on (E · CLIMB ON).
		if (!Mule) return;
		if (!Staged.bPressed && !WalkTo(MuleAt(*Mule, ac::Mule::perchLocal.x, SideOf(*Mule) * 1.4), 0.4))
		{
			Waited(TEXT("at the Mule's perch again"), WalkSeconds);
			return;
		}
		if (!Staged.bPressed)
		{
			Face(ac::Mule::perch(*Mule));
			if (!Settle(1.0)) return;
			Press();
		}
		if (U->kind == ac::UnitKind::mule) Advance();
		else Waited(TEXT("aboard the Mule again"), AskSeconds);
		return;
	case 12:
	{
		// Into the doorway, seen from outside (third person: its driver drawn).
		ThirdPerson(true);
		const ac::Vec2 Doorway = C->position + ac::Vec2(0, 6.0);
		if (!DriveTo(Doorway, 0.35))
		{
			Waited(TEXT("the Mule in the doorway"), 40);
			return;
		}
		Camera(Doorway + ac::Vec2(3.5, 7.0), 2.4, Doorway, 1.0, 45.f);
		if (Shot(TEXT("c4-mule-doorway")))
		{
			ViewPilot();
			Advance();
		}
		return;
	}
	case 13:
	{
		// On into the hall: the load is booked (the haul empty, the toast).
		ThirdPerson(false);
		const ac::Vec2 Hall = C->position + ac::Vec2(0, 1.5);
		if (!DriveTo(Hall, 0.5) && !(U->haul && U->haul->total() == 0))
		{
			Waited(TEXT("the Mule in the hall"), 40);
			return;
		}
		Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		if (U->haul && U->haul->total() > 0)
		{
			Waited(TEXT("the load booked"), 10);
			return;
		}
		if (Shot(TEXT("c4-mule-booked")))
		{
			Check(true, TEXT("the Mule's load booked in the hall"));
			Finish();
		}
		return;
	}
	default:
		Finish();
		return;
	}
}
