// -AcFlowTest's pictures of the buildings' sizes (C5, docs/building-sizes.md),
// over the local server on the first world (build it first: make -C
// unreal/server). It starts a new world and waits to be seated in the
// Prospector on the start pad.
//
//   mmosize   the server's dev line `citadel here` puts a Citadel of the
//             realm ahead; then `street KIND` for each kind a pilot builds
//             (the Foundry first), one finished ahead of the pilot with its
//             Prospector at the building's front corner. With
//             -AcMmoShots=DIR, PNGs from a free camera before its front
//             corner, framed for the MMO's size (so the same picture taken
//             at the strategy game's sizes shows how much it grew): DIR/
//             citadel.png, foundry.png, habdome.png, garrison.png,
//             bastion.png, spacedock.png, lab.png, sentinel.png,
//             gatling.png, pad.png, derrick.png. Then `street all` (one of
//             each in a row beside the Citadel): DIR/street.png from before
//             the row and DIR/street-tps.png from the pilot's chase camera
//             at the Citadel's door. Checks: each kind placed and drawn.
//
// Run hidden in a game process, by day (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmosize -AcMmoShots={out} -AcMmoClockHour=11
#include "AcGameFlow.h"

#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcWorldRenderer.h"

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
	constexpr double AskSeconds = 10;
	/// How long each picture's moment is held before it is taken, and after.
	constexpr double HoldSeconds = 2.0;

	struct FSizeKind
	{
		const TCHAR* Name;
		ac::StructureKind Kind;
		/// The MMO's half side and height (C5's table), cells: the camera
		/// frames these in both runs.
		double Radius, Height;
	};
	const FSizeKind Kinds[] = {
		{TEXT("citadel"), ac::StructureKind::citadel, 6.0, 12.3},
		{TEXT("foundry"), ac::StructureKind::foundry, 3.5, 5.76},
		{TEXT("habdome"), ac::StructureKind::habDome, 2.5, 2.88},
		{TEXT("garrison"), ac::StructureKind::garrison, 3.5, 6.0},
		{TEXT("bastion"), ac::StructureKind::bastion, 3.5, 3.36},
		{TEXT("spacedock"), ac::StructureKind::spacedock, 3.5, 7.68},
		{TEXT("lab"), ac::StructureKind::lab, 2.5, 4.56},
		{TEXT("sentinel"), ac::StructureKind::sentinel, 2.5, 4.08},
		{TEXT("gatling"), ac::StructureKind::gatling, 2.5, 3.12},
		{TEXT("pad"), ac::StructureKind::landingPad, 5.5, 1.5},
		{TEXT("derrick"), ac::StructureKind::derrick, 3.5, 6.0},
	};
	constexpr int32 NumKinds = UE_ARRAY_COUNT(Kinds);

	struct FSizeShots
	{
		/// The kind being pictured (`Kinds`), then `NumKinds` for the street.
		int32 Kind = 0;
		bool bAsked = false;
		double AskedAt = -1;
		/// The highest building id before the line was sent: the new one is above.
		int64 Before = 0;
		int64 Building = -1;
		double ShotAt = -1;
		bool bShot = false;
		TWeakObjectPtr<ACameraActor> Cam;
	};
	FSizeShots Staged;

	/// -AcMmoShots=DIR: a PNG of the moment `Name` there (false: no directory).
	bool Picture(const FString& Name)
	{
		FString Dir;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty()) return false;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ false, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
		return true;
	}
}

void UAcGameFlowSubsystem::TickMmoSizeShots(const FString& Step)
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
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	auto ViewPilot = [&] {
		if (PC && Pilot && PC->GetViewTarget() != Pilot) PC->SetViewTarget(Pilot);
	};
	// Third person: the pilot's Prospector drawn (first person hides it).
	auto ThirdPerson = [&] {
		if (Pilot && !Pilot->ChaseEye().IsSet()) Pilot->ToggleView();
	};
	auto Finish = [&] {
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
		Staged = FSizeShots();
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Since() > 1.0) Advance();
		else Waited(TEXT("seated"), SeatSeconds);
		return;
	}

	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::Simulation* S = M ? &M->simulation() : nullptr;
	if (!Seated() || !S)
	{
		Waited(TEXT("still driving"), 6);
		return;
	}
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	auto Ground = [&](const ac::Vec2 P) { return Terrain ? Terrain->FieldHeight(P) : 0.0; };
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
	auto Shot = [&](const FString& Name) {
		if (Staged.ShotAt < 0) Staged.ShotAt = Now;
		if (!Staged.bShot && Now - Staged.ShotAt >= HoldSeconds)
		{
			Picture(Name);
			Staged.bShot = true;
		}
		return Staged.bShot && Now - Staged.ShotAt >= 2 * HoldSeconds;
	};
	auto NextKind = [&] {
		Staged.Kind += 1;
		Staged.bAsked = false;
		Staged.Building = -1;
		Staged.ShotAt = -1;
		Staged.bShot = false;
		T.PhaseAt = Now;
	};
	auto Newest = [&](const ac::StructureKind Kind) -> const ac::Structure* {
		const ac::Structure* Found = nullptr;
		for (const ac::Structure& B : S->state.structures)
			if (B.kind == Kind && B.id > Staged.Before && B.complete() && (!Found || B.id > Found->id)) Found = &B;
		return Found;
	};
	auto Ask = [&](const FString& Line) {
		Staged.Before = 0;
		for (const ac::Structure& B : S->state.structures) Staged.Before = FMath::Max<int64>(Staged.Before, B.id);
		Check(Mmo->DevClock(Line), FString::Printf(TEXT("`%s` sent (dev)"), *Line));
		Staged.bAsked = true;
		Staged.AskedAt = Now;
	};

	if (Staged.Kind < NumKinds)
	{
		const FSizeKind& K = Kinds[Staged.Kind];
		if (!Staged.bAsked)
		{
			ViewPilot();
			ThirdPerson();
			Ask(K.Kind == ac::StructureKind::citadel ? FString(TEXT("citadel here")) : FString::Printf(TEXT("street %s"), K.Name));
			return;
		}
		const ac::Structure* B = Newest(K.Kind);
		if (!B)
		{
			if (Now - Staged.AskedAt > AskSeconds)
			{
				Check(false, FString::Printf(TEXT("size: a %s placed (%s)"), K.Name, *Mmo->Message()));
				NextKind();
			}
			return;
		}
		// Drawn: the renderer has its model, stretched to its footprint.
		const UAcWorldRenderer* R = UAcWorldRenderer::Get(this);
		if (Staged.Building != B->id)
		{
			Staged.Building = B->id;
			const FAcModelInfo* Model = R ? R->ModelOf(B->id) : nullptr;
			Check(Model != nullptr, FString::Printf(TEXT("size: the %s drawn (%.1f cells across, %.2f tall)"), K.Name,
										 2 * ac::Rules::radius(K.Kind), ac::Rules::height(K.Kind)));
		}
		// Before its front corner (the door is on +y), framed for its MMO size.
		const ac::Vec2 Dir = ac::normalize(ac::Vec2(0.75, 1.0));
		const double Away = 2.4 * K.Radius + 0.9 * K.Height + 6.0;
		Camera(B->position + Dir * Away, 0.45 * K.Height + 2.0, B->position, 0.35 * K.Height + 0.4, 50.f);
		if (Shot(K.Name)) NextKind();
		return;
	}

	// The street: one of each in a row beside the Citadel.
	if (Staged.Kind == NumKinds)
	{
		if (!Staged.bAsked)
		{
			ViewPilot();
			Ask(TEXT("street all"));
			return;
		}
		int32 Placed = 0;
		ac::Vec2 Lo(1e9, 1e9), Hi(-1e9, -1e9);
		for (const ac::Structure& B : S->state.structures)
		{
			if (!(B.id > Staged.Before || B.kind == ac::StructureKind::citadel) || !B.complete()) continue;
			if (B.kind == ac::StructureKind::derrick) continue;  // on its well, maybe off the row
			if (B.kind != ac::StructureKind::citadel) Placed += 1;
			const double Rad = ac::Rules::radius(B.kind);
			Lo = ac::Vec2(FMath::Min(Lo.x, B.position.x - Rad), FMath::Min(Lo.y, B.position.y - Rad));
			Hi = ac::Vec2(FMath::Max(Hi.x, B.position.x + Rad), FMath::Max(Hi.y, B.position.y + Rad));
		}
		if (Placed < 3)
		{
			if (Now - Staged.AskedAt > AskSeconds)
			{
				Check(false, FString::Printf(TEXT("size: the street placed (%d buildings)"), Placed));
				Finish();
			}
			return;
		}
		const ac::Vec2 Mid = (Lo + Hi) * 0.5;
		const double Span = FMath::Max(Hi.x - Lo.x, Hi.y - Lo.y);
		Camera(Mid + ac::Vec2(0.15 * Span, 0.75 * Span + 6.0), 0.3 * Span + 4.0, Mid, 2.0, 55.f);
		if (Shot(TEXT("street")))
		{
			Check(true, FString::Printf(TEXT("size: the street, %d buildings beside the Citadel"), Placed));
			Staged.Kind += 1;
			Staged.ShotAt = -1;
			Staged.bShot = false;
		}
		return;
	}

	// The same from the pilot's chase camera at the Citadel's door.
	ViewPilot();
	ThirdPerson();
	if (Shot(TEXT("street-tps")))
	{
		Check(true, TEXT("the buildings' sizes pictured"));
		Finish();
	}
}
