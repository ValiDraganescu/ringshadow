// -AcFlowTest's MMORPG steps (AcGameFlow.h): the MMORPG test mode as the
// player plays it, through the real pilot pawn, its camera and the world map,
// over the local server on the first world (build it first: make -C
// unreal/server). Each step starts the mode, waits to be seated in the
// Prospector on the start pad, travels, and checks the pilot is still driving
// the new region's unit with the view on it, no hand back sent, the view not
// held; then leaves the mode.
//
//   mmocross  drive east (the move keys and the mouse's yaw, as the input
//             actions set them) over the start region's edge into Tern Ridge
//   mmofly    M's world map, Cold Fork's pad picked and FLY pressed, landing
//   mmohandback  Esc (a hand back) then a take again: the pawn never dives
//             back in on an old snapshot, nor drops a unit just taken
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   ... -game -RenderOffscreen ... -AcFlowTest=mmocross,mmofly
#include "AcGameFlow.h"

#include "AcHUD.h"
#include "SAcResourceBar.h"
#include "SAcRoot.h"
#include "SAcScoreboard.h"

#include "AcLog.h"
#include "AcPilotPawn.h"
#include "AcRtsPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

#include <cmath>

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	constexpr double SeatSeconds = 20;
	constexpr double TravelSeconds = 60;
	constexpr double ArriveSeconds = 8;
}

void UAcGameFlowSubsystem::TickMmoFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const bool bFly = Step == TEXT("mmofly");
	const FString Into = bFly ? TEXT("cold-fork") : TEXT("tern-ridge");
	auto Finish = [&] {
		if (Mmo) Mmo->TestReadEvery = 0;
		if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		StopMmo();
		NextStep();
	};
	const bool bHand = Step == TEXT("mmohandback");
	if (Step != TEXT("mmocross") && Step != TEXT("mmofly") && !bHand)
	{
		Check(false, FString::Printf(TEXT("no MMORPG step %s (mmocross, mmofly, mmohandback)"), *Step));
		NextStep();
		return;
	}
	// The pilot in the Prospector the server drives, the view on it.
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven()
			&& PC && PC->GetPawn() == Pilot;
	};
	// Where the driven unit is, and where the view is, cells apart.
	auto ViewOff = [&]() -> double {
		const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
		const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
		if (!U || !Pilot || !Pilot->Camera()) return 1e9;
		return FVector::Dist2D(Pilot->Camera()->GetComponentLocation(), AcSpace::ToWorld(U->position)) / AcSpace::ToCm(1.0);
	};

	if (bHand && T.Phase >= 2)
	{
		// Slow frames: a reply and a snapshot sent before it come together.
		if (Mmo) Mmo->TestReadEvery = 0.2;
		switch (T.Phase)
		{
		case 2:
			MmoVisitWas = Pilot ? Pilot->Driven() : -1;
			Pilot->Leave();
			Advance();
			return;
		case 3:
			// Two seconds: given back, and never driven again on its own.
			if (Pilot->Driving() || (Mmo->Wanted() && Since() > 0.1))
			{
				Check(false, FString::Printf(TEXT("stays given back (pilot %s, wanted #%lld)"), Pilot->Driving() ? TEXT("driving") : TEXT("out"),
					(long long)Mmo->Wanted().value_or(-1)));
				Finish();
				return;
			}
			if (Since() < 2.0) return;
			Check(!Mmo->Driven(), FString::Printf(TEXT("given back: the server drives nothing (#%lld)"), (long long)Mmo->Driven().value_or(-1)));
			Check(PC && PC->GetPawn() == AAcRtsPawn::Get(this), TEXT("on the top-down camera"));
			Pilot->TakeOver(MmoVisitWas);
			Advance();
			return;
		case 4:
			if (Seated() && Pilot->Driven() == MmoVisitWas)
			{
				Advance();
				return;
			}
			if (Since() > ArriveSeconds)
			{
				Check(false, FString::Printf(TEXT("taken again within %.0f s"), ArriveSeconds));
				Finish();
			}
			return;
		case 5:
			// Two seconds more: not dropped by an old snapshot.
			if (!Seated())
			{
				Check(false, TEXT("keeps the unit taken again"));
				Finish();
				return;
			}
			if (Since() < 2.0) return;
			Check(true, FString::Printf(TEXT("took #%lld again and kept it, %d hand back sent"), (long long)MmoVisitWas, Mmo->HandBacksSent()));
			Check(Mmo->HandBacksSent() == 1, TEXT("one hand back (the Esc)"));
			Finish();
			return;
		default:
			Finish();
			return;
		}
	}

	switch (T.Phase)
	{
	case 0:
		if (Sim && Sim->IsMmo()) StopMmo();
		StartMmo();
		Advance();
		return;
	case 1:
		if (Seated() && Mmo->Here())
		{
			Check(Utf(Mmo->Here()->id) == TEXT("start"), FString::Printf(TEXT("seated in Prospector #%lld in %s"), (long long)*Mmo->Driven(),
				*Utf(Mmo->Here()->id)));
			MmoVisitWas = Mmo->Visit();
			Check(Mmo->TakesSent() == 0, FString::Printf(TEXT("seated by the server, no take sent (%d)"), Mmo->TakesSent()));
			Advance();
		}
		else if (Since() > SeatSeconds)
		{
			Check(false, FString::Printf(TEXT("seated within %.0f s (%s)"), SeatSeconds, Mmo ? *Mmo->Message() : TEXT("no client")));
			Finish();
		}
		return;
	case 2:
		if (bFly)
		{
			// M, a pad picked, FLY: as the player does it.
			ToggleWorldMap();
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			Check(Map.IsValid(), TEXT("M opened the world map"));
			if (!Map)
			{
				Finish();
				return;
			}
			Check(Map->PickPad(Into, TEXT("landing")), TEXT("picked Cold Fork's pad"));
			// Slow frames from here: the RegionChange and the new region's
			// first snapshot are read in the same frame (the hand check's case).
			Mmo->TestReadEvery = 0.2;
			Map->Fly();
			Check(!WorldMapWidget().IsValid(), TEXT("FLY asked for the flight and closed the map"));
			Advance();
			return;
		}
		// East along the pad's row to the east edge's opening.
		if (const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr; M && M->driven() && Pilot)
		{
			if (const ac::Unit* U = M->unit(*M->driven()))
			{
				const ac::GroundRect& B = M->simulation().map->bounds;
				Pilot->SetTestInput(FVector2D(0, -1), std::atan2(B.minZ + 48 - U->position.y, B.maxX + 4 - U->position.x));
				// Slow frames near the edge (see mmofly).
				if (U->position.x > B.maxX - 8) Mmo->TestReadEvery = 0.2;
			}
		}
		if (Mmo && Mmo->Visit() > MmoVisitWas)
		{
			if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
			Advance();
		}
		else if (Since() > TravelSeconds)
		{
			Check(false, FString::Printf(TEXT("crossed the east edge within %.0f s"), TravelSeconds));
			Finish();
		}
		return;
	case 3:
		// The flight: lift-off, then the landing's RegionChange.
		if (bFly && Mmo && Mmo->FlyingTo())
		{
			Check(Utf(Mmo->FlyingTo()->id) == Into, FString::Printf(TEXT("flying to %s"), *Utf(Mmo->FlyingTo()->id)));
			Advance();
			return;
		}
		if (!bFly || Since() > 5)
		{
			if (bFly) Check(false, TEXT("lifted off within 5 s"));
			Advance();
		}
		return;
	case 4:
		if (bFly && Mmo && Mmo->Visit() <= MmoVisitWas && Since() < TravelSeconds) return;
		// Arrived: seated again, the view on the new unit (a moment to settle).
		if (Mmo && !Mmo->Travelling()) Mmo->TestReadEvery = 0;
		if (Seated() && Since() > 0.5 && ViewOff() < 12.0)
		{
			Advance();
			return;
		}
		if (Since() > ArriveSeconds)
		{
			const AAcRtsPawn* Rts = AAcRtsPawn::Get(this);
			Check(false, FString::Printf(TEXT("seated in the new region within %.0f s (pilot %s #%lld, server #%lld, travelling %d, pawn %s, view %.1f cells off)"),
				ArriveSeconds, Pilot && Pilot->Driving() ? TEXT("driving") : TEXT("not driving"), Pilot ? (long long)Pilot->Driven() : -1LL,
				Mmo && Mmo->Driven() ? (long long)*Mmo->Driven() : -1LL, Mmo ? (int32)Mmo->Travelling() : -1,
				PC && PC->GetPawn() ? *PC->GetPawn()->GetName() : TEXT("none"), ViewOff()));
			Check(Mmo && Mmo->HandBacksSent() == 0, FString::Printf(TEXT("no hand back sent (%d)"), Mmo ? Mmo->HandBacksSent() : -1));
			// Not seated: the top-down camera must work (pan and zoom).
			Check(Rts && PC && PC->GetPawn() == Rts && PC->GetViewTarget() == Rts && Rts->View() && !Rts->IsInputBlocked(),
				FString::Printf(TEXT("the top-down camera has the view (view target %s, input %s)"),
					PC && PC->GetViewTarget() ? *PC->GetViewTarget()->GetName() : TEXT("none"),
					Rts && Rts->IsInputBlocked() ? TEXT("blocked") : TEXT("free")));
			if (Rts && Rts->View())
			{
				AAcRtsPawn* R = const_cast<AAcRtsPawn*>(Rts);
				const double PpcWas = Rts->View()->pointsPerCell;
				R->ZoomBy(2.0);
				RtsTargetWas = FVector2D(Rts->View()->target.x, Rts->View()->target.y);
				R->Drag(Rts->ViewSize() / 2, Rts->ViewSize() / 2 + FVector2D(120, 0));
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: rts view %.0fx%.0f, %.3f -> %.3f points a cell (min %.3f), target %.1f,%.1f -> %.1f,%.1f, camera at %s"),
					Rts->ViewSize().X, Rts->ViewSize().Y, PpcWas, Rts->View()->pointsPerCell, Rts->View()->minPointsPerCell(), RtsTargetWas.X,
					RtsTargetWas.Y, Rts->View()->target.x, Rts->View()->target.y, *Rts->GetActorLocation().ToString());
			}
			T.Phase = 6;
			T.PhaseAt = Now;
		}
		return;
	case 5:
	{
		const ac::net::Client* Net = Mmo->Connection();
		Check(Net && Net->regionChange() && Utf(Net->regionChange()->regionId) == Into,
			FString::Printf(TEXT("a RegionChange into %s"), *Into));
		Check(Mmo->Here() && Utf(Mmo->Here()->id) == Into, TEXT("the world map says so"));
		Check(Seated(), FString::Printf(TEXT("still driving the new unit #%lld (pilot #%lld)"), (long long)Mmo->Driven().value_or(-1),
			(long long)Pilot->Driven()));
		Check(Mmo->HandBacksSent() == 0, FString::Printf(TEXT("no hand back sent (%d)"), Mmo->HandBacksSent()));
		Check(!Mmo->Travelling(), TEXT("not in the travel hold"));
		Check(ViewOff() < 12.0, FString::Printf(TEXT("the view follows the unit (%.1f cells off)"), ViewOff()));
		// The top bar: the pilot's units counted, no supply.
		if (const AAcHUD* Hud = AAcHUD::Get(this); Hud && Hud->Root())
		{
			const int64 Shown = Hud->Root()->ScoreboardWidget()->ShownCount(Sim->LocalPlayer());
			Check(Shown == 1, FString::Printf(TEXT("the HUD counts the pilot's 1 unit (shows %lld)"), (long long)Shown));
			Check(Hud->Root()->ScoreboardWidget()->GetVisibility() != EVisibility::Collapsed, TEXT("the unit count shows while driving"));
			Check(!Hud->Root()->ResourceBar()->ShowsSupply(), TEXT("no supply count"));
		}
		else
		{
			Check(false, TEXT("a HUD"));
		}
		if (bFly)
		{
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			Check(Near && Near->Distance <= FAcMmoClient::PadReach, TEXT("on Cold Fork's pad"));
		}
		Finish();
		return;
	}
	case 6:
	{
		// The top-down camera after a drag.
		if (Since() < 0.3) return;
		const AAcRtsPawn* Rts = AAcRtsPawn::Get(this);
		const bool bMoved = Rts && Rts->View() && FVector2D::Distance(RtsTargetWas, FVector2D(Rts->View()->target.x, Rts->View()->target.y)) > 0.5;
		Check(bMoved, TEXT("the top-down camera pans"));
		Finish();
		return;
	}
	default:
		Finish();
		return;
	}
}
