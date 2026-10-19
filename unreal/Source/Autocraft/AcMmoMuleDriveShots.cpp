// -AcFlowTest's pictures of the Mule's drive (C4, docs/haul-and-ferry.md
// "The drive"), over the local server on the first world (build it first:
// make -C unreal/server). It starts a new world and waits to be seated in
// the Prospector on the start pad.
//
//   mmomuledrive  the server's dev line `mule here` puts the pilot's Mule
//             beside it; the pilot climbs on (E · CLIMB ON). With
//             -AcMmoShots=DIR, PNGs: full right lock with W held from a
//             camera straight over its turning circle, a frame every 2 s
//             (c4-drive-turn-0..5: it rolls round the circle, never turning
//             on the spot), and from the chase camera (c4-drive-turn-tps);
//             D held standing still (c4-drive-standstill: the heading as it
//             was). Then `mule slope` (6 cells short of a slope it climbs,
//             facing up it): W, and half way up from its side
//             (c4-drive-slope: pitched to the ground, its wheels on it), and
//             from behind (c4-drive-slope-back: rolled to it too); `mule
//             steep` (short of a face too steep for its wheels): W until it
//             stands, from its side (c4-drive-steep) and the chase camera
//             with the HUD's refusal (c4-drive-steep-tps). Checks: no turn
//             at a standstill, the turn's radius, the refusal.
//
// Run hidden in a game process, by day (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmomuledrive -AcMmoShots={out} -AcMmoClockHour=11
#include "AcGameFlow.h"

#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

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
	/// The turn's frames: how many, how far apart (seconds).
	constexpr int32 TurnFrames = 6;
	constexpr double TurnEvery = 2.0;

	struct FDriveShots
	{
		int64 Mule = -1;
		double Settled = -1;
		bool bPressed = false;
		bool bShot = false;
		double ShotAt = -1;
		int32 Frame = 0;
		/// Where the turn began: the Mule's place and heading, and the
		/// circle's middle.
		ac::Vec2 From{0, 0};
		double FromHeading = 0;
		ac::Vec2 Centre{0, 0};
		TWeakObjectPtr<ACameraActor> Cam;
	};
	FDriveShots Staged;

	/// -AcMmoShots=DIR: a PNG of the moment `Name` there (false: no directory).
	bool Picture(const FString& Name)
	{
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return false;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
		return true;
	}
}

void UAcGameFlowSubsystem::TickMmoMuleDriveShots(const FString& Step)
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
		Staged.Frame = 0;
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
		Staged = FDriveShots();
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
	const TOptional<int64> Own = Mmo->OwnMule();
	const ac::Unit* Mule = Own ? M->unit(*Own) : nullptr;
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	auto Ground = [&](const ac::Vec2 P) { return Terrain ? Terrain->FieldHeight(P) : 0.0; };

	auto WalkTo = [&](const ac::Vec2 To, const double Near) {
		const ac::Vec2 D = To - U->position;
		const bool bThere = ac::length(D) <= Near;
		Pilot->SetTestInput(bThere ? FVector2D::ZeroVector : FVector2D(0, -1), std::atan2(D.y, D.x));
		return bThere;
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
		// Straight down: the yaw along the Mule's start heading, so the
		// frames line up.
		const FRotator R = (A - E).Rotation();
		Staged.Cam->SetActorLocationAndRotation(E, Up > 10 ? FRotator(-90.0, FMath::RadiansToDegrees(Staged.FromHeading), 0.0) : R);
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
	// The unit's own frame: ahead along its heading, `Side` to its right.
	auto At = [&](const ac::Unit& Mu, const double Ahead, const double Side) {
		const ac::Vec2 A(std::cos(Mu.heading), std::sin(Mu.heading)), R(-std::sin(Mu.heading), std::cos(Mu.heading));
		return Mu.position + A * Ahead + R * Side;
	};
	auto ThirdPerson = [&](const bool bOn) {
		if (bOn != Pilot->ChaseEye().IsSet()) Pilot->ToggleView();
	};
	auto Turned = [&](const ac::Unit& Mu) { return std::remainder(Mu.heading - Staged.FromHeading, 2.0 * UE_DOUBLE_PI); };

	switch (T.Phase)
	{
	case 2:
	{
		// To the perch, and climb on (E · CLIMB ON).
		if (!Mule)
		{
			Waited(TEXT("the pilot's Mule here"), AskSeconds);
			return;
		}
		Staged.Mule = Mule->id;
		const double Side = ac::dot(U->position - Mule->position, ac::Vec2(-std::sin(Mule->heading), std::cos(Mule->heading))) >= 0 ? 1.0 : -1.0;
		if (U->kind != ac::UnitKind::mule && !Staged.bPressed && !WalkTo(At(*Mule, ac::Mule::perchLocal.x, Side * 1.4), 0.4))
		{
			Waited(TEXT("at the Mule's perch"), WalkSeconds);
			return;
		}
		if (U->kind != ac::UnitKind::mule && !Staged.bPressed)
		{
			const ac::Vec2 D = ac::Mule::perch(*Mule) - U->position;
			Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.y, D.x));
			if (!Settle(1.0)) return;
			Press();
		}
		if (U->kind == ac::UnitKind::mule) Advance();
		else Waited(TEXT("aboard the Mule"), AskSeconds);
		return;
	}
	case 3:
	{
		// D held standing still: the heading stays (measured once it has
		// settled from the boarding, a second after).
		if (Since() < 1.0) return;
		if (Staged.Settled < 0)
		{
			Staged.FromHeading = U->heading;
			Staged.From = U->position;
		}
		Pilot->SetTestInput(FVector2D(1, 0), Pilot->Yaw());
		if (!Settle(2.0)) return;
		Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		if (Staged.ShotAt < 0)
		{
			Check(std::abs(Turned(*U)) < 1e-6 && ac::distance(U->position, Staged.From) < 1e-6,
				FString::Printf(TEXT("drive: no turn at a standstill (turned %.4f, moved %.4f)"), Turned(*U),
					ac::distance(U->position, Staged.From)));
		}
		ThirdPerson(true);
		ViewPilot();
		if (Shot(TEXT("c4-drive-standstill"))) Advance();
		return;
	}
	case 4:
	{
		// Full right lock and W: round its turning circle, from straight above
		// it, a frame every `TurnEvery`.
		if (Staged.Frame == 0 && Staged.Settled < 0)
		{
			Staged.From = U->position;
			Staged.FromHeading = U->heading;
			Staged.Centre = At(*U, 0.0, ac::Rules::muleTurnRadius);
			Staged.Settled = Now;
		}
		Camera(Staged.Centre, 30.0, Staged.Centre, 0.0, 50.f);
		Pilot->SetTestInput(FVector2D(1, -1), Pilot->Yaw());
		if (Staged.Frame < TurnFrames && Now - Staged.Settled >= Staged.Frame * TurnEvery)
		{
			Picture(FString::Printf(TEXT("c4-drive-turn-%d"), Staged.Frame));
			Staged.Frame += 1;
			return;
		}
		if (Staged.Frame < TurnFrames || Now - Staged.Settled < TurnFrames * TurnEvery) return;
		const double Off = ac::distance(U->position, Staged.Centre);
		Check(FMath::Abs(Off - ac::Rules::muleTurnRadius) < 0.3 && Turned(*U) > 0.5,
			FString::Printf(TEXT("drive: round its turning circle (%.2f cells from its middle, %.2f wanted; turned %.2f)"), Off,
				ac::Rules::muleTurnRadius, Turned(*U)));
		Advance();
		return;
	}
	case 5:
		// The same turn from the chase camera.
		ViewPilot();
		ThirdPerson(true);
		Pilot->SetTestInput(FVector2D(1, -1), Pilot->Yaw());
		if (Shot(TEXT("c4-drive-turn-tps")))
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			Advance();
		}
		return;
	case 6:
		// Short of a slope it climbs.
		if (!Staged.bPressed)
		{
			Check(Mmo->DevClock(TEXT("mule slope")), TEXT("the Mule set short of a slope (dev)"));
			Staged.bPressed = true;
			Staged.Settled = Now;
			return;
		}
		if (Now - Staged.Settled < 1.0) return;
		Staged.FromHeading = U->heading;
		Advance();
		return;
	case 7:
	{
		// Up it: half way, from its side and from behind.
		const ac::Vec2 Ahead(std::cos(U->heading), std::sin(U->heading));
		const double Up = S->groundPitch(*U, U->position, Ahead);
		if (Staged.ShotAt < 0 && Up < std::tan(10.0 * UE_DOUBLE_PI / 180.0))
		{
			Pilot->SetTestInput(FVector2D(0, -1), Pilot->Yaw());
			Waited(TEXT("the Mule up the slope"), 30);
			return;
		}
		Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		if (Staged.ShotAt < 0) Check(!Prompt.Contains(TEXT("Too steep")), TEXT("drive: up a slope it climbs, not refused"));
		Camera(At(*U, 0.0, 7.5), 0.9, At(*U, 0.0, 0.0), 0.7, 40.f);
		if (Shot(TEXT("c4-drive-slope"))) Advance();
		return;
	}
	case 8:
		Camera(At(*U, -7.5, 0.0), 1.6, At(*U, 0.0, 0.0), 0.7, 40.f);
		if (Shot(TEXT("c4-drive-slope-back"))) Advance();
		return;
	case 9:
		// Short of a face too steep for its wheels.
		if (!Staged.bPressed)
		{
			ViewPilot();
			Check(Mmo->DevClock(TEXT("mule steep")), TEXT("the Mule set short of a steep face (dev)"));
			Staged.bPressed = true;
			Staged.Settled = Now;
			return;
		}
		if (Now - Staged.Settled < 1.0) return;
		Advance();
		return;
	case 10:
	{
		// W until it stands, refused: from its side, then the chase camera
		// with the HUD's refusal.
		Pilot->SetTestInput(FVector2D(0, -1), Pilot->Yaw());
		const bool bRefused = Prompt.Contains(TEXT("Too steep"));
		if (Staged.ShotAt < 0 && !bRefused)
		{
			Waited(TEXT("the Mule refused at the steep face"), 30);
			return;
		}
		Camera(At(*U, 0.0, 7.5), 0.9, At(*U, 0.0, 0.0), 0.7, 40.f);
		if (Shot(TEXT("c4-drive-steep"))) Advance();
		return;
	}
	case 11:
		Pilot->SetTestInput(FVector2D(0, -1), Pilot->Yaw());
		ViewPilot();
		ThirdPerson(true);
		if (Staged.ShotAt < 0)
		{
			Check(Prompt.Contains(TEXT("Too steep for its wheels")), FString::Printf(TEXT("drive: the refusal (\"%s\")"), *Prompt));
		}
		if (Shot(TEXT("c4-drive-steep-tps")))
		{
			Check(true, TEXT("the Mule's drive pictured"));
			Finish();
		}
		return;
	default:
		Finish();
		return;
	}
}
