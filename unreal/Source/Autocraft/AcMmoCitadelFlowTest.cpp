// -AcFlowTest's Citadel step (C1, docs/citadel-interior.md §10; the other
// MMORPG steps are in AcMmoFlowTest.cpp), over the local server on the first
// world (build it first: make -C unreal/server). It starts a new world and
// waits to be seated in the Prospector on the start pad.
//
//   mmocitadel  the server's dev line `citadel here` puts a complete realm
//             Citadel ahead of the pilot, its door shut; the pilot walks to
//             the door ("E · OPEN"), opens it on the act key, walks into the
//             hall (sheltered), closes it from inside ("E · CLOSE"), walks to
//             gun 1's station ("E · TAKE GUN 1 SE") and takes it: the view
//             first person at the gun's eye, the look held at the arc's hard
//             stop, the feed up; fires (the Citadel's shots or misses come,
//             the feed heats); Esc disengages; opens the door again and walks
//             out (no longer sheltered). Every move through the real act key,
//             the move keys and the mouse's yaw, the Door and Man requests.
//             With -AcMmoShots=DIR -AcMmoShotsC1: a PNG of the door shut
//             (c1-door-shut.png) and open (c1-door-open.png) from 7 cells
//             out, in the hall (c1-hall.png) and at the gun (c1-feed.png).
//             With -AcMmoShots=DIR -AcMmoShotsTps: before the door opens, the
//             third-person view from 7 cells out at the wheel's zoom 0.45, 1
//             and 2.6 (tps-zoom-0.45.png ...), then the same with the build
//             menu's first building held out (tps-place-0.45.png ...; a
//             refusal noted in the log instead); then up against the
//             nearest rock, seen from the side (tps-rock.png, the gap from
//             its footprint circle logged); then first person again.
//             At the gun the wheel must leave the zoom alone.
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR --headless -- -AcFlowTest=mmocitadel
#include "AcGameFlow.h"

#include "AcCitadelAct.h"
#include "AcLog.h"
#include "AcPilotBuild.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Defence.h"
#include "Rules.h"
#include "Simulation.h"

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
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	constexpr double SeatSeconds = 20;
	constexpr double WalkSeconds = 25;
	constexpr double AskSeconds = 6;

	/// The step's own memory.
	struct FHall
	{
		int64 Citadel = -1;
		bool bAsked = false;
		bool bPressed = false;
		double Settled = -1;
		int32 ShotsSeen = 0;
		bool bShot = false;
		/// -AcMmoShotsTps: the next of the third-person pictures, and when it began.
		int32 Tps = 0;
		double TpsAt = -1;
		bool bTpsToggled = false;
	};
	/// -AcMmoShotsTps: the wheel's zoom of each picture (then again, placing).
	constexpr float TpsZooms[] = {0.45f, 1.f, 2.6f};
	FHall Hall;

	/// -AcMmoShots=DIR with -AcMmoShotsC1: a PNG of the moment `Name` there.
	bool Picture(const TCHAR* Name)
	{
		FString Dir;
		const bool bWanted = FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsTps"));
		if (!bWanted || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir) || Dir.IsEmpty())
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

void UAcGameFlowSubsystem::TickMmoCitadelFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
		Hall.bPressed = false;
		Hall.Settled = -1;
		Hall.bShot = false;
	};
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UAcPilotOverlaySubsystem* Overlay = UAcPilotOverlaySubsystem::Get(this);
	auto Finish = [&] {
		if (Pilot)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			Pilot->Press(false);
		}
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
	if (Step != TEXT("mmocitadel"))
	{
		Check(false, FString::Printf(TEXT("no Citadel step %s (mmocitadel)"), *Step));
		NextStep();
		return;
	}
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Hall = FHall();
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			Check(ac::Rules::citadelHall(), TEXT("the MMORPG mode has the Citadel's hall"));
			Check(Mmo->DevClock(TEXT("citadel here")), TEXT("a Citadel asked for ahead of the pilot (dev)"));
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
		if (!Waited(TEXT("still driving"), 4)) return;
		return;
	}
	const ac::Structure* C = nullptr;
	for (const ac::Structure& B : S->state.structures)
	{
		if (B.kind != ac::StructureKind::citadel || !B.complete() || !B.door) continue;
		if (Hall.Citadel >= 0 ? B.id == Hall.Citadel : ac::distance(B.position, U->position) < 16) C = &B;
	}
	if (!C)
	{
		Waited(TEXT("a complete Citadel with a door by the pilot"), AskSeconds);
		return;
	}
	Hall.Citadel = C->id;
	const ac::Footprint& F = ac::Rules::footprint(C->kind);
	const ac::Vec2 Local = U->position - C->position;
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	// Walk to a point in the Citadel's frame: the move keys forward, the
	// mouse's yaw onto it; stopped within `Near`. True once there.
	auto WalkTo = [&](const ac::Vec2 To, const double Near) {
		const ac::Vec2 D = C->position + To - U->position;
		const double Yaw = std::atan2(D.y, D.x);
		if (ac::length(D) <= Near)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
			return true;
		}
		Pilot->SetTestInput(FVector2D(0, -1), Yaw);
		return false;
	};
	// Stand still a moment (the snapshots and the prompt catch up).
	auto Settle = [&](const double Seconds) {
		if (Hall.Settled < 0) Hall.Settled = Now;
		return Now - Hall.Settled >= Seconds;
	};
	auto Press = [&] {
		Pilot->Press(true);
		Pilot->Press(false);
		Hall.bPressed = true;
	};
	const ac::Vec2 Doorstep(0, 7.0), InsideDoor(0, 3.8), Apron(0, 13.0);
	const bool bTps = FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsTps"));
	const bool bPictures = FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1")) || bTps;
	auto Pictures = [bPictures] { return bPictures; };
	auto FaceDoor = [&] {
		const ac::Vec2 D = C->position + ac::Vec2(0, 5.0) - U->position;
		Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.y, D.x));
	};
	switch (T.Phase)
	{
	case 2:
		// The door shut, a picture of it from out on the field, then to the doorstep.
		// (-AcMmoShotsTps's walk to a rock leaves the apron.)
		if (!Hall.bShot && Pictures() && Hall.Tps <= 2 * int32(UE_ARRAY_COUNT(TpsZooms)))
		{
			// A few cells before the apron (the open ramp reaches 3 out),
			// facing the door; the new Citadel and the ground streamed in.
			if (!WalkTo(Apron, 0.5)) return;
			FaceDoor();
			if (!Settle(10.0)) return;
		}
		// -AcMmoShotsTps: third person at each zoom, then a building held out.
		if (bTps && Hall.Tps <= 2 * int32(UE_ARRAY_COUNT(TpsZooms)) + 2)
		{
			constexpr int32 Zooms = int32(UE_ARRAY_COUNT(TpsZooms));
			if (Hall.Tps == 2 * Zooms)
			{
				// The last picture taken (a frame or more later): the building
				// dropped, the zoom back to 1.
				if (Now - Hall.TpsAt < 1.0) return;
				if (Pilot->OnKey) Pilot->OnKey(TEXT("esc"));
				Pilot->SetChaseZoom(1.f, true);
				Hall.Tps += 1;
				Hall.TpsAt = -1;
				T.PhaseAt = Now;
				return;
			}
			if (Hall.Tps == 2 * Zooms + 1)
			{
				// Up against the nearest rock (a single circle), then seen from its side.
				TOptional<ac::NavGrid::Circle> Rock;
				for (const ac::Doodad& D : S->map ? S->map->doodads : std::vector<ac::Doodad>())
				{
					const std::vector<ac::NavGrid::Circle> K = ac::NavGrid::circles(D);
					if (D.kind != ac::Doodad::Kind::rock || K.size() != 1 || ac::distance(K[0].first, U->position) > 14) continue;
					if (!Rock || ac::distance(K[0].first, U->position) < ac::distance(Rock->first, U->position)) Rock = K[0];
				}
				if (!Rock)
				{
					UE_LOG(LogAutocraft, Display, TEXT("flowtest: tps: no rock within 14 cells"));
					Hall.Tps += 1;
					Hall.TpsAt = Now;
					return;
				}
				const ac::Vec2 D = Rock->first - U->position;
				const double Keep = Rock->second + ac::NavGrid::clearance;
				if (Hall.TpsAt < 0)
				{
					if (ac::length(D) > Keep + 0.08 && Since() < 12)
					{
						Pilot->SetTestInput(FVector2D(0, -1), std::atan2(D.y, D.x));
						return;
					}
					Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.y, D.x) + UE_DOUBLE_PI / 2);
					Hall.TpsAt = Now;
					return;
				}
				if (Now - Hall.TpsAt < 1.5) return;
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: tps: against the rock: the unit's centre %.3f cells off the footprint circle (r %.2f; the walk keeps %.2f)"),
					ac::length(D) - Rock->second, Rock->second, ac::NavGrid::clearance);
				Picture(TEXT("tps-rock"));
				Hall.Tps += 1;
				Hall.TpsAt = Now;
				return;
			}
			if (Hall.Tps == 2 * Zooms + 2)
			{
				// The picture taken: first person again.
				if (Now - Hall.TpsAt < 1.0) return;
				if (Hall.bTpsToggled) Pilot->ToggleView();
				Hall.Tps += 1;
				T.PhaseAt = Now;
				return;
			}
			const bool bPlace = Hall.Tps >= Zooms;
			const float Zoom = TpsZooms[Hall.Tps % Zooms];
			if (Hall.TpsAt < 0)
			{
				if (!Pilot->Eye().bThirdPerson)
				{
					Pilot->ToggleView();
					Hall.bTpsToggled = true;
				}
				if (Hall.Tps == Zooms && Pilot->OnKey)
				{
					// The menu's first building the pilot may put down.
					Pilot->OnKey(TEXT("b"));
					const UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
					for (int32 K = 1; K <= 9 && Build && !Build->Placing(); ++K) Pilot->OnKey(FString::FromInt(K));
				}
				Pilot->SetChaseZoom(Zoom, true);
				Hall.TpsAt = Now;
				return;
			}
			if (Now - Hall.TpsAt < 1.5) return;
			if (const UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this); bPlace && (!Build || !Build->Placing()))
			{
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: tps: no building held out (\"%s\")"), Info && Info->Note ? **Info->Note : TEXT(""));
			}
			Picture(*FString::Printf(TEXT("tps-%s-%.2g"), bPlace ? TEXT("place") : TEXT("zoom"), Zoom));
			Hall.TpsAt = ++Hall.Tps == 2 * Zooms ? Now : -1;
			return;
		}
		if (!Hall.bShot)
		{
			Check(C->door->open == 0 && !C->door->opening, FString::Printf(TEXT("Citadel #%lld put down, its door shut"), (long long)C->id));
			Hall.bShot = true;
			Hall.Settled = -1;
			if (Picture(TEXT("c1-door-shut")))
			{
				T.PhaseAt = Now;
				return;
			}
		}
		if (Since() < 1.0) return;
		if (WalkTo(Doorstep, 0.6) && Settle(0.6))
		{
			Check(Prompt == TEXT("E · OPEN"), FString::Printf(TEXT("at the door: \"%s\""), *Prompt));
			Advance();
		}
		else Waited(TEXT("at the doorstep"), WalkSeconds);
		return;
	case 3:
		// The act key opens it.
		if (!Hall.bPressed) Press();
		if (S->doorOpen(*C))
		{
			if (!Hall.bShot)
			{
				Check(Mmo->HallRefusal().IsEmpty(), TEXT("the door opened on the act key"));
				Hall.bShot = true;
				Hall.Settled = -1;
			}
			// The picture from where the shut one was: the ramp down, the
			// hall through the opening.
			if (Pictures())
			{
				if (!WalkTo(Apron, 0.5)) return;
				FaceDoor();
				if (!Settle(1.5)) return;
				Picture(TEXT("c1-door-open"));
			}
			Advance();
		}
		else Waited(FString::Printf(TEXT("the door open (%.2f, %s)"), C->door->open, *Mmo->HallRefusal()), AskSeconds);
		return;
	case 4:
		// Into the hall, through the doorway.
		if (Since() < 1.0) return;
		// (Straight in along x 0, through the doorway's middle.)
		if (WalkTo(InsideDoor, 0.5))
		{
			if (!Settle(0.8)) return;
			Check(F.inside(Local) && S->sheltered(*U), FString::Printf(TEXT("in the hall, sheltered (%.1f, %.1f)"), Local.x, Local.y));
			Check(Prompt == TEXT("E · CLOSE"), FString::Printf(TEXT("inside by the open door: \"%s\""), *Prompt));
			Advance();
		}
		else
		{
			Waited(FString::Printf(TEXT("in the hall (at %.2f, %.2f, the door %.2f%s)"), Local.x, Local.y, C->door->open,
				C->door->opening ? TEXT(" opening") : TEXT(" shutting")), WalkSeconds);
		}
		return;
	case 5:
		// Shut behind it, from inside.
		if (!Hall.bPressed) Press();
		if (C->door->open <= 0.01 && !C->door->opening)
		{
			// The picture looks into the hall, its stations and lamps.
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1")))
			{
				const ac::Vec2 D = C->position + ac::Vec2(0, -4) - U->position;
				Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.y, D.x));
				if (Hall.Settled < 0) Hall.Settled = Now;
				if (Now - Hall.Settled < 1.5) return;
			}
			Check(true, TEXT("the door closed from inside"));
			Picture(TEXT("c1-hall"));
			Advance();
		}
		else Waited(FString::Printf(TEXT("the door shut (%.2f, %s)"), C->door->open, *Mmo->HallRefusal()), AskSeconds);
		return;
	case 6:
	{
		// To gun 1's station, in the hall's NE corner.
		if (Since() < 1.0) return;
		const ac::Vec2 Station = ac::Defence::gunStation(C->position, 0) - C->position;
		if (WalkTo(Station, 0.7) && Settle(0.6))
		{
			Check(Prompt == TEXT("E · TAKE GUN 1 SE"), FString::Printf(TEXT("at the station: \"%s\""), *Prompt));
			Advance();
		}
		else Waited(TEXT("at gun 1's station"), WalkSeconds);
		return;
	}
	case 7:
	{
		if (!Hall.bPressed) Press();
		if (U->task != ac::Unit::Task::atGun || Pilot->AtGun() != 0 || !Settle(0.5))
		{
			Waited(FString::Printf(TEXT("at the gun (%s)"), *Mmo->HallRefusal()), AskSeconds);
			return;
		}
		const std::optional<ac::Structure> Cs = S->state.structure(C->id);
		Check(Cs && Cs->guns && (*Cs->guns)[0].gunner == U->id, TEXT("gun 1 manned by the pilot"));
		// The view: first person from the gun's eye, the cockpit and the unit hidden.
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		const FVector Eye = AcCitadelAct::Eye(*C, Terrain ? Terrain->FieldHeight(C->position) : 0.0, 0, Pilot->Yaw(), Pilot->ViewPitch());
		const double Off = Pilot->Camera() ? AcSpace::ToCells(FVector::Dist(Pilot->Camera()->GetComponentLocation(), Eye)) : 1e9;
		Check(Off < 0.3, FString::Printf(TEXT("the view at the gun's eye (%.2f cells off)"), Off));
		// The wheel is the chase camera's: at a gun it does nothing.
		const float Zoomed = Pilot->ChaseZoom();
		Pilot->ZoomNotch(-1.f);
		Check(Pilot->ChaseZoom() == Zoomed, TEXT("the wheel leaves the zoom alone at a gun"));
		Check(Info && Info->Feed && Info->Feed->Gun == 0, TEXT("the feed is up: TURRET 1 · SE"));
		// The look: a long turn right holds at the hard stop, then back in.
		Pilot->Look(5000.0, 0.0);
		const double Mid = ac::Defence::cornerAngle(0);
		Check(FMath::IsNearlyEqual(std::remainder(Pilot->Yaw() - Mid, 2 * UE_DOUBLE_PI), ac::Defence::gunArcHalf, 1e-6),
			FString::Printf(TEXT("the look held at the right stop (%.3f)"), std::remainder(Pilot->Yaw() - Mid, 2 * UE_DOUBLE_PI)));
		Pilot->Look(-ac::Defence::gunArcHalf / 0.0032, 0.0);
		Check(FMath::Abs(std::remainder(Pilot->Yaw() - Mid, 2 * UE_DOUBLE_PI)) < 1e-6, TEXT("and back to the arc's middle"));
		Hall.ShotsSeen = 0;
		Pilot->Press(true);
		Advance();
		return;
	}
	case 8:
		// Held: it fires rounds at the weapon's rate (C2: `Fired`, flying
		// until they strike something).
		for (const ac::GameEvent& E : Sim->LastFrame().AllEvents)
		{
			if (const ac::GameEvent::Fired* Fi = E.as<ac::GameEvent::Fired>(); Fi && Fi->shooter == C->id) Hall.ShotsSeen += 1;
		}
		if (Hall.ShotsSeen >= 3 && Info && Info->Feed && Info->Feed->Heat > 0 && Since() > 1.0)
		{
			if (!Hall.bShot)
			{
				Hall.bShot = true;
				Picture(TEXT("c1-feed"));
				return;
			}
			Pilot->Press(false);
			Check(true, FString::Printf(TEXT("the gun fired %d rounds, the feed hot (%d pips)"), Hall.ShotsSeen, Info->Feed->Heat));
			Check(Pilot->Disengage(), TEXT("Esc disengages"));
			Advance();
		}
		else Waited(FString::Printf(TEXT("the gun firing (%d rounds seen)"), Hall.ShotsSeen), AskSeconds);
		return;
	case 9:
		if (U->task != ac::Unit::Task::atGun && Pilot->AtGun() == INDEX_NONE)
		{
			Check(Pilot->Driving() && S->sheltered(*U), TEXT("off the gun, still driving, in the hall"));
			Advance();
		}
		else Waited(TEXT("off the gun"), AskSeconds);
		return;
	case 10:
		// Back to the door, open it from inside...
		if (WalkTo(InsideDoor, 0.5) && Settle(0.6))
		{
			if (!Hall.bPressed)
			{
				Check(Prompt == TEXT("E · OPEN"), FString::Printf(TEXT("inside by the shut door: \"%s\""), *Prompt));
				Press();
			}
			if (S->doorOpen(*C)) Advance();
			else Waited(TEXT("the door open from inside"), WalkSeconds);
		}
		else Waited(TEXT("back at the door"), WalkSeconds);
		return;
	case 11:
		// ...and out.
		if (WalkTo(ac::Vec2(0, 9.0), 0.6))
		{
			if (!Settle(0.5)) return;
			Check(!F.inside(Local) && !S->sheltered(*U), FString::Printf(TEXT("out of the Citadel (%.1f, %.1f)"), Local.x, Local.y));
			Finish();
		}
		else Waited(FString::Printf(TEXT("out of the Citadel (at %.2f, %.2f)"), Local.x, Local.y), WalkSeconds);
		return;
	default:
		Finish();
		return;
	}
}
