// -AcFlowTest's walk-in Dropship step (C4, docs/haul-and-ferry.md sections 3
// and 4), over the local server on the first world (build it first: make -C
// unreal/server). The pilot does it all with its own walk and act key:
//   mmoferry  the dev lines put a Landing Pad under the seated Prospector
//             and one in Cold Fork and fill the MH; on the pad E calls the
//             Dropship; it comes down, its ramp lowers; up the ramp into the
//             bay (the third-person camera stays inside its walls); at the
//             console with the ramp open the take-off button says "Close
//             the ramp first"; at the ramp post E closes the ramp; at the
//             console E opens the flight menu, Cold Fork's pad is flown to;
//             the lift, the dash and the approach seen through the
//             windshield in first person; landed, E at the post lowers the
//             ramp and the pilot walks out onto the pad.
//   mmoferrymule  the same rig; the Dropship called and down, the pilot's
//             Mule put on the ship's axis behind its ramp, the pilot at its
//             perch (`ferry mule`), onto it, driven up the ramp into the bay: the
//             Mule fits (inside the bay's walls, its rider aboard); from the
//             perch, the crosshair on the ramp post, E closes the ramp (still
//             driving); the crosshair off the buttons, E would climb off.
// With -AcMmoShots=DIR -AcMmoShotsFerry, PNGs of its moments (ferry-N-*.png).
// -AcMmoWeather=rain|storm for the windshield's rain and wipers.
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR -- -AcFlowTest=mmoferry
//       [-AcMmoShots={out} -AcMmoShotsFerry -AcMmoWeather=rain]
#include "AcGameFlow.h"

#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcShuttleAct.h"
#include "AcShuttleView.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "SAcWorldMap.h"

#include "Mule.h"
#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"

#include "Camera/PlayerCameraManager.h"
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
	constexpr double SeatSeconds = 40;
	constexpr double WalkSeconds = 25;
	constexpr double AskSeconds = 8;
	/// The Dropship's call to its ramp down (descend 3 s, the ramp 3 s), with room.
	constexpr double LandSeconds = 30;

	struct FFerryRun
	{
		int64 Ship = -1;
		bool bPressed = false;
		double Settled = -1;
		/// The stages seen in flight (pictures taken once each).
		bool bLiftShot = false, bDashShot = false, bApproachShot = false;
		/// The target pad's region, seen in flight.
		bool bFlew = false;
		/// Since when nothing is driven (the region switch in flight).
		double UnseatedAt = -1;
		/// The walk in: 0 out to the ship's side, 1 back along it, 2 to the
		/// ramp's foot, 3 up the ramp.
		int32 Leg = 0;
	};
	FFerryRun Run;

	/// -AcMmoShots=DIR with -AcMmoShotsFerry: a PNG of the moment `Name`.
	void Picture(const TCHAR* Name)
	{
		FString Dir;
		if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsFerry")) || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoShots="), Dir)
			|| Dir.IsEmpty())
		{
			return;
		}
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: shot %s"), *Path);
	}
}

void UAcGameFlowSubsystem::TickMmoFerryFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
		Run.bPressed = false;
		Run.Settled = -1;
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
		CloseWorldMap();
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
		return Mmo && Pilot && Mmo->Driven() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven() && PC && PC->GetPawn() == Pilot;
	};
	const bool bMule = Step == TEXT("mmoferrymule");
	if (Step != TEXT("mmoferry") && !bMule)
	{
		Check(false, FString::Printf(TEXT("no ferry step %s (mmoferry, mmoferrymule)"), *Step));
		NextStep();
		return;
	}
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		Run = FFerryRun();
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && !Mmo->Travelling() && Mmo->Here() && Since() > 1.0)
		{
			// A Landing Pad under the pilot, one in Cold Fork (seen), MH to fly.
			Check(Mmo->DevClock(TEXT("ferry pad here\nferry pad cold-fork\nferry mh 200")),
				TEXT("ferry: the server puts up two Landing Pads and fills the MH (dev)"));
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
		// The region switch in flight: a moment with nothing driven.
		if (Run.UnseatedAt < 0) Run.UnseatedAt = Now;
		if (Now - Run.UnseatedAt > 6)
		{
			Check(false, FString::Printf(TEXT("still driving within 6 s (%s)"), Mmo ? *Mmo->Message() : TEXT("no client")));
			Finish();
		}
		return;
	}
	Run.UnseatedAt = -1;
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	const TOptional<AcShuttleAct::FAct> Act = AcShuttleAct::Find(*S, *U);
	const ac::Shuttle* Ship = nullptr;
	for (const ac::Shuttle& Q : S->state.shuttles)
	{
		if (Run.Ship >= 0 ? Q.id == Run.Ship : ac::distance(Q.position, U->position) < 12) Ship = &Q;
	}
	const ac::Vec2 Local = Ship ? ac::ShuttleBay::local(*Ship, U->position) : ac::Vec2();
	// Walk to a point in the ship's frame (or the region's, with no ship
	// yet): the move keys forward, the mouse's yaw onto it.
	auto WalkTo = [&](const ac::Vec2 To, const double Near) {
		const ac::Vec2 Goal = Ship ? ac::ShuttleBay::world(*Ship, To) : To;
		const ac::Vec2 D = Goal - U->position;
		const double Yaw = std::atan2(D.y, D.x);
		if (ac::length(D) <= Near)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
			return true;
		}
		Pilot->SetTestInput(FVector2D(0, -1), Yaw);
		return false;
	};
	auto Face = [&](const double LocalYaw) { Pilot->SetTestInput(FVector2D::ZeroVector, (Ship ? Ship->heading : 0.0) + LocalYaw); };
	auto Settle = [&](const double Seconds) {
		if (Run.Settled < 0) Run.Settled = Now;
		return Now - Run.Settled >= Seconds;
	};
	auto Press = [&] {
		Pilot->Press(true);
		Pilot->Press(false);
		Run.bPressed = true;
	};
	// The look's pitch to `Want` (radians, up +), through the mouse.
	auto Pitch = [&](const double Want) { Pilot->Look(0.0, (Pilot->ViewPitch() - Want) / 0.0026); };
	auto FirstPerson = [&](const bool bFirst) {
		if (Pilot->ChaseEye().IsSet() == bFirst) Pilot->ToggleView();
	};
	// The camera in the ship's frame (cm): inside its bay's walls and under its ceiling.
	auto CameraInBay = [&](FString& Where) {
		const AcShuttleView::FShip* F = Ship ? AcShuttleView::Find(Ship->id) : nullptr;
		if (!F || !PC || !PC->PlayerCameraManager) return false;
		const FVector L = F->Placement.InverseTransformPosition(PC->PlayerCameraManager->GetCameraLocation());
		Where = L.ToCompactString();
		using B = ac::ShuttleBay;
		// (With the ramp down the boom may reach out of the open back, as far as the hull's end.)
		const double Back = Ship->ramp.open > 0.0 ? AcSpace::ToCm(B::hullRear) - 30 : AcSpace::ToCm(B::rear) - 5;
		return L.X > Back && L.X < AcSpace::ToCm(B::noseFrom) + 60 && FMath::Abs(L.Y) < AcSpace::ToCm(B::halfWide) + 6
			&& L.Z > AcSpace::ToCm(B::deck) && L.Z < AcSpace::ToCm(B::ceiling) + 8;
	};

	switch (T.Phase)
	{
	case 2:
	{
		// Onto the pad's middle, then the call.
		const ac::Structure* Pad = nullptr;
		for (const ac::Structure& B : S->state.structures)
		{
			if (B.kind != ac::StructureKind::landingPad || !B.complete()) continue;
			if (!Pad || ac::distance(B.position, U->position) < ac::distance(Pad->position, U->position)) Pad = &B;
		}
		if (Pad && ac::distance(Pad->position, U->position) < 12 && !WalkTo(Pad->position, 0.3))
		{
			Waited(TEXT("onto the Landing Pad"), WalkSeconds);
			return;
		}
		if (Act && Act->Kind == AcShuttleAct::FAct::EKind::Call && !Act->Why && Mmo->HydrogenNow().value_or(0) >= 100 && Settle(1.0))
		{
			Check(Prompt == TEXT("E · CALL THE DROPSHIP"), FString::Printf(TEXT("ferry: on the pad the prompt calls the Dropship (\"%s\")"), *Prompt));
			FirstPerson(false);
			Press();
			Advance();
		}
		else Waited(FString::Printf(TEXT("on a Landing Pad (%s), the call offered (%s), MH %lld"),
			Pad ? *FString::Printf(TEXT("%.1f away"), ac::distance(Pad->position, U->position)) : TEXT("none"),
			Act ? *Act->Prompt() : TEXT("no act"), (long long)Mmo->HydrogenNow().value_or(-1)), WalkSeconds);
		return;
	}
	case 3:
		// It comes down and lowers its ramp.
		if (Ship && Ship->grounded() && Ship->ramp.open >= ac::ShuttleBay::rampDown)
		{
			Run.Ship = Ship->id;
			Face(UE_DOUBLE_PI);
			if (Settle(1.5))
			{
				Check(Mmo->ShuttleRefusal().IsEmpty(), FString::Printf(TEXT("ferry: the call kept (%s)"), *Mmo->ShuttleRefusal()));
				Picture(TEXT("ferry-1-landed"));
				Advance();
				if (bMule) T.Phase = 20;
			}
		}
		else Waited(TEXT("the Dropship down with its ramp lowered"), LandSeconds);
		return;
	case 4:
	{
		// Round to the ramp's foot, up it, into the bay.
		if (!Ship) break;
		{
			static double LoggedAt = 0;
			if (Now - LoggedAt > 1.0)
			{
				LoggedAt = Now;
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: ferry: walking in, at (%.2f, %.2f) in the ship's frame, ramp %.2f"), Local.x, Local.y, Ship->ramp.open);
			}
		}
		// Pushed off the pad by the landing: out to its side, back along the
		// side, to the ramp's foot, up it (each leg once, in turn).
		if (Run.Leg == 0 && (Local.x < ac::ShuttleBay::rampFoot() - 0.5 || ac::ShuttleBay::onRamp(Local) || ac::ShuttleBay::inBay(Local))) Run.Leg = 2;
		const double Side = Local.y >= 0 ? 3.3 : -3.3;
		if (Run.Leg == 0)
		{
			if (FMath::Abs(Local.y) >= 3.0 || WalkTo(ac::Vec2(Local.x, Side), 0.3)) Run.Leg = 1;
			Waited(FString::Printf(TEXT("out to the Dropship's side (at %.2f, %.2f)"), Local.x, Local.y), WalkSeconds);
		}
		else if (Run.Leg == 1)
		{
			if (WalkTo(ac::Vec2(ac::ShuttleBay::rampFoot() - 1.2, Side), 0.4)) Run.Leg = 2;
			Waited(FString::Printf(TEXT("back along the Dropship's side (at %.2f, %.2f)"), Local.x, Local.y), WalkSeconds);
		}
		else if (Run.Leg == 2)
		{
			if (ac::ShuttleBay::onRamp(Local) || ac::ShuttleBay::inBay(Local) || WalkTo(ac::Vec2(ac::ShuttleBay::rampFoot() - 0.8, 0.0), 0.3)) Run.Leg = 3;
			Waited(FString::Printf(TEXT("to the ramp's foot (at %.2f, %.2f)"), Local.x, Local.y), WalkSeconds);
		}
		else if (WalkTo(ac::Vec2(-1.0, 0.0), 0.3))
		{
			FString Where;
			if (Settle(1.0))
			{
				Check(ac::ShuttleBay::inBay(Local), FString::Printf(TEXT("ferry: walked up the ramp into the bay (%.2f, %.2f)"), Local.x, Local.y));
				Check(CameraInBay(Where), FString::Printf(TEXT("ferry: the third-person camera inside the bay's walls (%s)"), *Where));
				Picture(TEXT("ferry-2-bay"));
				Advance();
			}
		}
		else Waited(TEXT("up the ramp into the bay"), WalkSeconds);
		return;
	}
	case 5:
		// At the console with the ramp down: refused.
		if (!Ship) break;
		if (WalkTo(ac::Vec2(1.9, 0.0), 0.25) && Settle(1.0))
		{
			Face(0.0);
			Check(Act && Act->Kind == AcShuttleAct::FAct::EKind::TakeOff && Act->Why && *Act->Why == TEXT("Close the ramp first"),
				FString::Printf(TEXT("ferry: with the ramp down the take-off button says close it first (\"%s\")"), *Prompt));
			Advance();
		}
		else Waited(TEXT("at the console"), WalkSeconds);
		return;
	case 6:
		// At the ramp post: close it.
		if (!Ship) break;
		if (!Run.bPressed)
		{
			if (WalkTo(ac::Vec2(-1.55, -0.4), 0.2) && Settle(1.0))
			{
				Check(Prompt == TEXT("E · CLOSE THE RAMP"), FString::Printf(TEXT("ferry: at the post the prompt closes the ramp (\"%s\")"), *Prompt));
				for (const ac::Unit& V : S->state.units)
				{
					const ac::Vec2 W = ac::ShuttleBay::local(*Ship, V.position);
					if (ac::ShuttleBay::onRamp(W, ac::Rules::radius(V.kind)))
						UE_LOG(LogAutocraft, Display, TEXT("flowtest: ferry: on the ramp's strip: #%lld %s at (%.2f, %.2f)"), (long long)V.id,
							*FString(std::string(ac::rawValue(V.kind)).c_str()), W.x, W.y);
				}
				Press();
			}
			else Waited(TEXT("at the ramp post"), WalkSeconds);
		}
		else if (Ship->ramp.open <= 0.0 && !Ship->ramp.opening)
		{
			Check(Mmo->ShuttleRefusal().IsEmpty(), FString::Printf(TEXT("ferry: the ramp closed (%s)"), *Mmo->ShuttleRefusal()));
			Face(UE_DOUBLE_PI);
			Picture(TEXT("ferry-3-ramp-shut"));
			Advance();
		}
		else Waited(FString::Printf(TEXT("the ramp shut (%.2f, %s)"), Ship->ramp.open, *Mmo->ShuttleRefusal()), AskSeconds);
		return;
	case 7:
		// At the console: E opens the flight menu; Cold Fork's pad.
		if (!Ship) break;
		if (!Run.bPressed)
		{
			if (WalkTo(ac::Vec2(1.9, 0.0), 0.25) && Settle(1.0))
			{
				Face(0.0);
				FirstPerson(true);
				Pitch(-0.35);
				if (Now - Run.Settled < 1.6) return;
				if (!Run.bLiftShot)
				{
					// The console and the windshield, the rain on it: before the press.
					Check(Prompt == TEXT("E · TAKE OFF"), FString::Printf(TEXT("ferry: at the console the prompt takes off (\"%s\")"), *Prompt));
					Picture(TEXT("ferry-4-console"));
					Run.bLiftShot = true;
					return;
				}
				if (Now - Run.Settled < 2.4) return;
				Run.bLiftShot = false;
				Press();
			}
			else Waited(TEXT("back at the console"), WalkSeconds);
		}
		else if (WorldMapWidget().IsValid() && Since() > 1.0)
		{
			Check(true, TEXT("ferry: the take-off button opened the flight menu"));
			Picture(TEXT("ferry-4-flight-menu"));
			FlyFromMap(TEXT("cold-fork"), TEXT("Landing Pad 1"));
			Advance();
		}
		else Waited(TEXT("the flight menu"), AskSeconds);
		return;
	case 8:
	{
		// The flight, a step back from the console, looking out of the windshield.
		WalkTo(ac::Vec2(1.25, 0.0), 0.15);
		Face(0.0);
		// High in the dash, a little down onto the land going by.
		Pitch(Ship && Ship->stage == ac::Shuttle::Stage::dash ? -0.22 : -0.08);
		if (!Ship)
		{
			Waited(TEXT("the Dropship through the region switch"), 10);
			return;
		}
		using EStage = ac::Shuttle::Stage;
		const AcShuttleView::FShip* F = AcShuttleView::Find(Ship->id);
		if (Ship->flying()) Run.bFlew = true;
		if (Ship->stage == EStage::lift && !Run.bLiftShot && Ship->clock > 1.5)
		{
			Run.bLiftShot = true;
			FString Where;
			Check(CameraInBay(Where), FString::Printf(TEXT("ferry: lifting off, the view still in the bay (%s)"), *Where));
			Picture(TEXT("ferry-5-lift"));
		}
		if (Ship->stage == EStage::dash && !Run.bDashShot && Ship->clock > 1.0)
		{
			Run.bDashShot = true;
			const double Up = F ? AcSpace::ToCells(F->Placement.GetLocation().Z) - Ship->ground : 0.0;
			Check(Up > 10.0, FString::Printf(TEXT("ferry: in the dash the ship high over the land (%.1f cells)"), Up));
			Picture(TEXT("ferry-6-dash"));
		}
		// Out of the side pane a moment into the dash.
		if (Ship->stage == EStage::dash && Run.bDashShot && Ship->clock > 2.2 && Ship->clock < 2.6) Face(0.75);
		if (Ship->stage == EStage::approach && !Run.bApproachShot && Ship->clock > 1.0)
		{
			Run.bApproachShot = true;
			Picture(TEXT("ferry-7-approach"));
		}
		if (Run.bFlew && Ship->grounded())
		{
			Check(Run.bLiftShot && Run.bDashShot && Run.bApproachShot, TEXT("ferry: the lift, the dash and the approach seen"));
			Run.bApproachShot = false;
			Advance();
			return;
		}
		Waited(TEXT("the flight to Cold Fork's pad"), Ship->plan.total() + 60);
		return;
	}
	case 9:
		// Landed: the ramp down from the post.
		if (!Ship) break;
		if (!Run.bPressed)
		{
			if (WalkTo(ac::Vec2(-1.55, -0.4), 0.2) && Settle(1.0))
			{
				Check(Prompt == TEXT("E · OPEN THE RAMP"), FString::Printf(TEXT("ferry: landed, the post opens the ramp (\"%s\")"), *Prompt));
				Press();
			}
			else Waited(TEXT("at the ramp post after the landing"), WalkSeconds);
		}
		else if (Ship->ramp.open >= ac::ShuttleBay::rampDown)
		{
			Face(UE_DOUBLE_PI);
			Picture(TEXT("ferry-8-ramp-down"));
			Advance();
		}
		else Waited(FString::Printf(TEXT("the ramp down (%.2f, %s)"), Ship->ramp.open, *Mmo->ShuttleRefusal()), AskSeconds + 3);
		return;
	case 10:
		// Down the ramp onto the pad.
		if (!Ship) break;
		if (WalkTo(ac::Vec2(ac::ShuttleBay::rampFoot() - 1.0, 0.0), 0.35))
		{
			FirstPerson(false);
			Face(0.0);
			if (Settle(1.5) && !Run.bApproachShot)
			{
				Check(!S->aboard(*U), TEXT("ferry: walked out of the Dropship"));
				Picture(TEXT("ferry-9-out"));
				Run.bApproachShot = true;
			}
			if (Settle(2.5)) Finish();
		}
		else Waited(TEXT("down the ramp"), WalkSeconds);
		return;
	case 20:
	{
		// mmoferrymule: the pilot's Mule on the ship's axis behind its ramp,
		// facing it, the pilot on its perch (the server's `ferry mule`).
		if (!Run.bPressed)
		{
			Check(Mmo->DevClock(TEXT("ferry mule")), TEXT("mule: the Mule behind the ramp, the pilot driving it (dev)"));
			Run.bPressed = true;
			T.PhaseAt = Now;
			return;
		}
		// On foot at its perch: onto it (a take, as E at the perch).
		if (const TOptional<int64> Own = Mmo->OwnMule(); Own && U->kind == ac::UnitKind::prospector && !Run.bFlew && Since() > 1.0)
		{
			Mmo->Want(*Own);
			Run.bFlew = true;
		}
		if (Ship && U->kind == ac::UnitKind::mule && Local.x < ac::ShuttleBay::rampFoot() - 2.0 && FMath::Abs(Local.y) < 0.3)
		{
			Run.Leg = 3;
			Advance();
			return;
		}
		Waited(FString::Printf(TEXT("mule: driving the Mule behind the ramp (a %s at %.2f, %.2f)"), U->kind == ac::UnitKind::mule ? TEXT("Mule") : TEXT("Prospector"),
			Local.x, Local.y), 20);
		return;
	}
	case 21:
	{
		// Round the ship to behind its ramp, then straight up into the bay,
		// the look along the hull (a vehicle turns toward the look).
		if (!Ship) break;
		const double Foot = ac::ShuttleBay::rampFoot();
		const double Side = Local.y >= 0 ? 4.2 : -4.2;
		auto Drive = [&](const ac::Vec2 To, const double Near) {
			const ac::Vec2 Goal = ac::ShuttleBay::world(*Ship, To);
			const ac::Vec2 D = Goal - U->position;
			if (ac::length(D) <= Near) return true;
			// A vehicle steers on the sideways keys (`pilotFace`: toward a
			// growing heading on the right key), the forward key drives.
			const double Err = std::remainder(std::atan2(D.y, D.x) - U->heading, 2.0 * UE_DOUBLE_PI);
			Pilot->SetTestInput(FVector2D(FMath::Clamp(Err * 2.0, -1.0, 1.0), FMath::Abs(Err) > 1.4 ? -0.3 : -1.0), U->heading);
			return false;
		};
		{
			static double LoggedAt = 0;
			if (Now - LoggedAt > 1.0)
			{
				LoggedAt = Now;
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: ferry: the Mule at (%.2f, %.2f) in the ship's frame, leg %d"), Local.x, Local.y, Run.Leg);
			}
		}
		// By where it is (a vehicle overshoots a point): out from beside the
		// ship, back along its side past the ramp's foot, then onto the axis
		// behind the ramp and up it, chasing a point 3 cells ahead on the axis.
		// From the perch: the crosshair on the ramp post, E closes the ramp
		// (still driving); the crosshair off the buttons, E would climb off.
		auto AimAt = [&](const ac::Vec3& B) {
			const AcShuttleView::FShip* F = AcShuttleView::Find(Ship->id);
			if (!F || !PC || !PC->PlayerCameraManager) return;
			const FVector To = F->Placement.TransformPosition(FVector(AcSpace::ToCm(B.x), AcSpace::ToCm(B.y), AcSpace::ToCm(B.z)));
			const FVector D = (To - PC->PlayerCameraManager->GetCameraLocation()).GetSafeNormal();
			Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(D.Y, D.X));
			Pitch(std::asin(FMath::Clamp(D.Z, -1.0, 1.0)));
		};
		if (Run.Leg == 4)
		{
			AimAt(ac::ShuttleBay::rampButton);
			if (!AcShuttleAct::DriverAim().bRampButton || Prompt != TEXT("E · CLOSE THE RAMP"))
			{
				Waited(FString::Printf(TEXT("mule: the crosshair on the ramp post, \"E · CLOSE THE RAMP\" (\"%s\")"), *Prompt), AskSeconds + 10);
				return;
			}
			Check(true, TEXT("mule: from the perch, the crosshair on the ramp post: \"E · CLOSE THE RAMP\""));
			Press();
			T.PhaseAt = Now;
			Run.Leg = 5;
			return;
		}
		if (Run.Leg == 5)
		{
			if (Ship->ramp.open <= 0.0 && !Ship->ramp.opening)
			{
				Check(U->kind == ac::UnitKind::mule, TEXT("mule: the ramp closed from the perch, still driving the Mule"));
				Picture(TEXT("ferry-mule-ramp-shut"));
				Run.Leg = 6;
				T.PhaseAt = Now;
			}
			else Waited(FString::Printf(TEXT("mule: the ramp shut from the perch (%.2f, %s)"), Ship->ramp.open, *Mmo->ShuttleRefusal()), AskSeconds + 10);
			return;
		}
		if (Run.Leg == 6)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Ship->heading + 0.6);
			Pitch(0.25);
			if (Prompt.Contains(TEXT("CLIMB OFF")) && Since() > 1.0)
			{
				Check(true, FString::Printf(TEXT("mule: the crosshair off the buttons, E climbs off (\"%s\")"), *Prompt));
				Finish();
			}
			else Waited(FString::Printf(TEXT("mule: off the buttons, \"E · CLIMB OFF\" (\"%s\")"), *Prompt), AskSeconds + 10);
			return;
		}
		// Straight up the ramp, chasing a point 3 cells ahead on the axis.
		const bool bBehind = Local.x < Foot - 3.0;
		const bool bAxis = Run.Leg >= 3 || ac::ShuttleBay::onRamp(Local) || ac::ShuttleBay::inBay(Local);
		// The legs only go forward (a turning vehicle crosses their bounds).
		if (bAxis) Run.Leg = 3;
		else if (bBehind) Run.Leg = FMath::Max(Run.Leg, 2);
		else if (FMath::Abs(Local.y) >= 3.6) Run.Leg = FMath::Max(Run.Leg, 1);
		bool bIn = false;
		if (Run.Leg == 0) Drive(ac::Vec2(Local.x - 2.0, Side), 0.0);
		else if (Run.Leg == 1) Drive(ac::Vec2(Foot - 5.0, Side), 0.0);
		else if (Run.Leg == 2 && (FMath::Abs(Local.y) > 0.25 || Local.x < Foot - 1.0)) Drive(ac::Vec2(FMath::Max(Local.x + 3.0, Foot - 3.5), 0.0), 0.0);
		else if (Local.x < -0.5) Drive(ac::Vec2(FMath::Max(Local.x + 3.0, Foot), 0.0), 0.0), Run.Leg = 3;
		else bIn = true;
		if (bIn)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, Ship->heading);
			if (Settle(1.5))
			{
				const ac::Unit* Rider = nullptr;
				for (const ac::Unit& V : S->state.units)
				{
					if (V.kind == ac::UnitKind::prospector && V.task == ac::Unit::Task::aboard && V.structure == U->id) Rider = &V;
				}
				Check(U->kind == ac::UnitKind::mule && ac::ShuttleBay::inBay(Local, 0.0),
					FString::Printf(TEXT("mule: driven up the ramp into the bay (at %.2f, %.2f)"), Local.x, Local.y));
				Check(ac::ShuttleBay::inBay(Local + ac::Vec2(-1.5, 0.0)) && ac::ShuttleBay::inBay(Local + ac::Vec2(1.5, 0.0))
						&& FMath::Abs(Local.y) + 0.79 <= ac::ShuttleBay::halfWide,
					FString::Printf(TEXT("mule: the whole Mule inside the bay's walls (at %.2f, %.2f)"), Local.x, Local.y));
				Check(Rider != nullptr, TEXT("mule: its driver on the perch"));
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: ferry: in the bay on the Mule the prompt reads \"%s\""), *Prompt);
				Picture(TEXT("ferry-mule-bay"));
				Run.Leg = 4;
			}
			return;
		}
		Waited(FString::Printf(TEXT("mule: driven into the bay (at %.2f, %.2f, leg %d)"), Local.x, Local.y, Run.Leg), WalkSeconds * 2);
		return;
	}
	default:
		Finish();
		return;
	}
	Waited(TEXT("the Dropship in the shown state"), AskSeconds);
}
