// -AcMmoShots=DIR: the MMORPG test mode's moments in pictures
// (docs/mmorpg-m4.md Q1, the review in docs/mmorpg-shots.md). A flow step of
// its own (`mmoshots`, AcGameFlow.h): it starts a new world on the local
// server (build it first: make -C unreal/server), plays it through the real
// pilot pawn, build menu, level cards and world map as AcMmoFlowTest.cpp
// does, and saves a PNG with the HUD at each moment, DIR/NN-moment.png, a
// log line each (`mmoshots: saved ...`):
//
//   01 spawn on the pad, third person   02 the same, first person
//   03 top-down over the start region   04 the world map
//   05 the build menu                   06 the Citadel's ghost near the ore
//   07 the level-up cards (the weld's XP levels the Prospector; the run
//      gives the new pilot 55 XP, -AcMmoDevXp, so it does)
//   08 a Citadel going up               09 the Citadel finished
//   10 drilling ore                     11 unloading: before, 12 after
//   13 the world map, Cold Fork picked  14 the flight, mid-flight
//   15 landed at Cold Fork's pad        16 the edge crossing into the start
//                                          region, just after arrival
//   17 after leaving the mode and resuming
//
// Its own folders (never the player's): the pilot key and the world's save
// in DIR/mmo, the window game's sessions in DIR/save (AcMmo.cpp, AcSaves.cpp).
//
// Run hidden (ringshadow-testing, "Hidden runs"):
//   "$BIN" "$PWD/unreal/Autocraft.uproject" /Game/Maps/Battlefield -game
//     -RenderOffscreen -windowed -ForceRes -ResX=1600 -ResY=1000 -unattended
//     -nosplash -nosound "-ini:Input:[/Script/Engine.InputSettings]:bCaptureMouseOnLaunch=False"
//     -abslog=DIR/run.log -AcMap=highlands-small -AcMmoShots=DIR
// Done when the log's last line reads `flowtest: done, 0 failed`.
#include "AcGameFlow.h"

#include "AcLeveling.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotBuild.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcSimSubsystem.h"
#include "SAcLevelUp.h"

#include "Pilot.h"
#include "Rules.h"
#include "Simulation.h"

#include "Engine/World.h"
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

	/// Seconds a phase may take before the run gives up on it.
	constexpr double PhaseLimit = 60;
	/// The Citadel's weld (71 s at twice the pace) and some.
	constexpr double WeldLimit = 90;

	const ac::Structure* CitadelOf(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// How far up a building is, 0 to 1.
	double Built(const ac::Structure& B)
	{
		if (!B.buildLeft) return 1;
		return 1 - *B.buildLeft / ac::Rules::buildTime(B.kind);
	}
}

void UAcGameFlowSubsystem::TickMmoShots()
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::GameState* S = M ? &M->simulation().state : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	const FString Here = Mmo && Mmo->Here() ? Utf(Mmo->Here()->id) : FString(TEXT("-"));
	const int64 Ore = S && !S->players.empty() ? S->players[0].ore : -1;

	auto Stop = [&](const double Yaw) {
		if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
	};
	auto Finish = [&] {
		if (Pilot)
		{
			Pilot->Press(false);
			Stop(Pilot->Yaw());
		}
		StopMmo();
		NextStep();
	};
	auto Where = [&] {
		return U ? FString::Printf(TEXT("at %.1f,%.1f in %s, %lld ore"), U->position.x, U->position.y, *Here, (long long)Ore)
				 : FString::Printf(TEXT("no unit, in %s"), *Here);
	};
	// A phase that never got there: say where it was, and stop.
	auto TimedOut = [&](const TCHAR* What, const double Limit = PhaseLimit) {
		if (Since() <= Limit) return false;
		Check(false, FString::Printf(TEXT("%s within %.0f s (%s; %s)"), What, Limit, *Where(), Mmo ? *Mmo->Message() : TEXT("no client")));
		Finish();
		return true;
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven()
			&& PC && PC->GetPawn() == Pilot && U;
	};

	// A picture asked for: wait until it is written (the writer is async).
	if (!Shots.Pending.IsEmpty())
	{
		++Shots.Waited;
		const int64 Size = IFileManager::Get().FileSize(*Shots.Pending);
		if (Size > 0 && Size == Shots.LastSize)
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: saved %s (%lld bytes; %s)"), *FPaths::GetCleanFilename(Shots.Pending), (long long)Size,
				*Where());
			Shots.Pending.Empty();
			Shots.Saved += 1;
			T.PhaseAt = Now;
			return;
		}
		Shots.LastSize = Size;
		if (Shots.Waited > 600)
		{
			Check(false, FString::Printf(TEXT("%s written"), *Shots.Pending));
			Shots.Pending.Empty();
			Finish();
		}
		return;
	}
	// Asks for a picture with the HUD; the phase moves on once it is written.
	auto Shoot = [&](const TCHAR* Name, const FString& What) {
		Shots.Pending = FPaths::Combine(Shots.Dir, FString(Name) + TEXT(".png"));
		IFileManager::Get().Delete(*Shots.Pending, false, true, true);
		Shots.LastSize = -1;
		Shots.Waited = 0;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: %s"), Name, *What);
		FScreenshotRequest::RequestScreenshot(Shots.Pending, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		Advance();
	};
	// Drives the route: 1 there, 0 on the way, -1 stalled (4 s without half a cell).
	auto Drive = [&]() -> int32 {
		if (!U || !Pilot) return 0;
		const FVector2D P(U->position.x, U->position.y);
		while (Shots.RouteAt < Shots.Route.Num() && FVector2D::Distance(P, Shots.Route[Shots.RouteAt]) < 1.0) Shots.RouteAt += 1;
		if (Shots.RouteAt >= Shots.Route.Num())
		{
			Stop(Pilot->Yaw());
			return 1;
		}
		const FVector2D W = Shots.Route[Shots.RouteAt];
		Pilot->SetTestInput(FVector2D(0, -1), std::atan2(W.Y - P.Y, W.X - P.X));
		if (FVector2D::Distance(P, Shots.StallAt) > 0.5)
		{
			Shots.StallAt = P;
			Shots.StallSince = Now;
		}
		return Now - Shots.StallSince > 4.0 ? -1 : 0;
	};
	auto Route = [&](std::initializer_list<FVector2D> Points) {
		Shots.Route = TArray<FVector2D>(Points);
		Shots.RouteAt = 0;
		Shots.StallAt = U ? FVector2D(U->position.x, U->position.y) : FVector2D::ZeroVector;
		Shots.StallSince = Now;
	};
	auto Stalled = [&](const TCHAR* What) {
		Check(false, FString::Printf(TEXT("%s: stalled %s, heading for %s"), What, *Where(),
			Shots.Route.IsValidIndex(Shots.RouteAt) ? *Shots.Route[Shots.RouteAt].ToString() : TEXT("-")));
		Finish();
	};
	// A spot in region-file cells (docs/regions.md: from the map's corner).
	auto Cell = [&](const double X, const double Y) {
		const ac::GroundRect* B = M ? &M->simulation().map->bounds : nullptr;
		return B ? FVector2D(B->minX + X, B->minZ + Y) : FVector2D(X, Y);
	};
	auto Face = [&](const ac::Vec2 At) {
		return U ? std::atan2(At.y - U->position.y, At.x - U->position.x) : 0.0;
	};

	// The level-up cards, the first time they are up (the weld's XP).
	UAcLevelingSubsystem* Leveling = UAcLevelingSubsystem::Get(this);
	const bool bCards = Leveling && Leveling->DrivenOffer().IsSet() && Leveling->Cards().IsValid()
		&& Leveling->Cards()->GetVisibility().IsVisible();

	switch (T.Phase)
	{
	case 0:
		if (Sim && Sim->IsMmo()) StopMmo();
		IFileManager::Get().MakeDirectory(*Shots.Dir, true);
		// A world's XP is a tenth of the game's: the new pilot starts with
		// 55 (5 short of level 2), so the weld levels it (the banner and the
		// cards) unless the run says otherwise.
		if (double Xp = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp)) FCommandLine::Append(TEXT(" -AcMmoDevXp=55"));
		StartMmo(true);
		Advance();
		return;
	case 1:
		// Seated on the start pad; a few seconds for the ground, the light
		// and the exposure to settle.
		if (Seated() && Mmo->Here() && Since() > 1.0)
		{
			Shots.Unit = U->id;
			Check(Here == TEXT("start"), FString::Printf(TEXT("seated in Prospector #%lld %s"), (long long)U->id, *Where()));
			if (!Pilot->ChaseEye()) Pilot->ToggleView();
			Advance();
		}
		else TimedOut(TEXT("seated"));
		return;
	case 2:
		if (Since() > 5.0) Shoot(TEXT("01-spawn-pad-third"), TEXT("on the start pad, third person"));
		return;
	case 3:
		Pilot->ToggleView();
		Advance();
		return;
	case 4:
		if (Since() > 2.0) Shoot(TEXT("02-spawn-pad-first"), TEXT("on the start pad, first person"));
		return;
	case 5:
		// Esc: out to the top-down camera (the unit handed back).
		Pilot->Leave();
		Advance();
		return;
	case 6:
		if (Since() > 3.0) Shoot(TEXT("03-topdown-start"), TEXT("top-down over the start region"));
		return;
	case 7:
		Pilot->TakeOver(Shots.Unit);
		Advance();
		return;
	case 8:
		if (Seated() && Since() > 2.5)
		{
			if (!Pilot->ChaseEye()) Pilot->ToggleView();
			Check(OpenWorldMap(), TEXT("M opened the world map"));
			Advance();
		}
		else TimedOut(TEXT("taken again"));
		return;
	case 9:
		if (Since() > 1.0) Shoot(TEXT("04-world-map"), TEXT("the world map (M)"));
		return;
	case 10:
		CloseWorldMap();
		// West toward the ore, to stand a Citadel's reach east of it.
		Route({Cell(69, 48)});
		Advance();
		return;
	case 11:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 12:
		if (Since() > 1.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("B opened the build menu"));
			Advance();
		}
		return;
	case 13:
		if (Since() > 1.0) Shoot(TEXT("05-build-menu"), TEXT("the build menu (B)"));
		return;
	case 14:
	{
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		// The Citadel's number in the mode's menu.
		const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::citadel) + 1;
		Check(Number > 0 && Build && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
			&& *Build->Placing() == ac::StructureKind::citadel, FString::Printf(TEXT("%d held out a Citadel"), Number));
		Advance();
		return;
	}
	case 15:
		if (Since() > 1.5) Shoot(TEXT("06-citadel-ghost"), TEXT("the Citadel's ghost near the ore"));
		return;
	case 16:
		// The act key places it (the Build request); paid from the wallet.
		Shots.OreBefore = Ore;
		Pilot->Press(true);
		Pilot->Press(false);
		Advance();
		return;
	case 17:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr)
		{
			Shots.Citadel = C->id;
			if (bCards && Shots.CardsSince < 0) Shots.CardsSince = Now;
			if (bCards && !Shots.bCardsShot && Now - Shots.CardsSince > 0.6)
			{
				// The banner is up for a moment: take it with the cards.
				Shots.bCardsShot = true;
				Shoot(TEXT("07-level-up-cards"), FString::Printf(TEXT("the level-up cards, the Citadel %.0f%% up"), Built(*C) * 100));
				T.Phase = 17;
				return;
			}
			if (Shots.bCardsShot && Built(*C) >= 0.4)
			{
				Check(Ore == Shots.OreBefore - ac::Rules::cost(ac::StructureKind::citadel),
					FString::Printf(TEXT("the Citadel paid: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
				Shoot(TEXT("08-citadel-going-up"), FString::Printf(TEXT("a Citadel going up, %.0f%%"), Built(*C) * 100));
				return;
			}
		}
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("built, not refused: %s"), *Why));
		}
		TimedOut(TEXT("the Citadel 40% up with the cards shown"), WeldLimit);
		return;
	case 18:
		if (const ac::Structure* C = S ? CitadelOf(*S) : nullptr; C && C->complete())
		{
			if (Since() > 2.0) Shoot(TEXT("09-citadel-finished"), TEXT("the Citadel finished"));
			return;
		}
		TimedOut(TEXT("the Citadel finished"), WeldLimit);
		return;
	case 19:
	{
		// Round the Citadel's north side to the ore field's middle patch.
		const ac::Structure* C = CitadelOf(*S);
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			const ac::OreDeposit& P = S->patches[I];
			if (P.remaining <= 0) continue;
			if (Best < 0 || ac::distance(P.position, C->position) < ac::distance(S->patches[Best].position, C->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore near the Citadel"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		const ac::Vec2 P = S->patches[Best].position;
		Route({FVector2D(C->position.x + 5, C->position.y - 6), FVector2D(C->position.x - 5, C->position.y - 6), FVector2D(P.x + 1, P.y)});
		Advance();
		return;
	}
	case 20:
	{
		// Up to the patch until the act key would drill it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1)
		{
			// At the last waypoint and not in reach yet: straight at it.
			Shots.Route.Add(FVector2D(P.x, P.y));
		}
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 21:
		// Held: drilling.
		if (U && U->task == ac::Unit::Task::mining && Since() > 1.5)
		{
			Shoot(TEXT("10-drilling-ore"), TEXT("drilling ore, the act key held"));
			return;
		}
		TimedOut(TEXT("drilling"), 15);
		return;
	case 22:
		if (U && U->carrying > 0)
		{
			Pilot->Press(false);
			Shots.OreBefore = Ore;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Shots.bShotBefore = false;
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 30);
		return;
	case 23:
	{
		// To the Citadel: a picture just before, then into it.
		const ac::Structure* C = CitadelOf(*S);
		const double Gap = U ? ac::distance(U->position, C->position) - ac::Rules::radius(C->kind) : 1e9;
		if (U && U->carrying > 0 && !Shots.bShotBefore && Gap < 3.0)
		{
			Shots.bShotBefore = true;
			Stop(Face(C->position));
			T.Phase = 30;
			T.PhaseAt = Now;
			return;
		}
		if (U && U->carrying == 0)
		{
			Stop(Face(C->position));
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("unloaded at the Citadel"), 30);
		return;
	}
	case 30:
		// (From 23.) Stood still a moment, the load still aboard.
		if (Since() > 1.0)
		{
			Shoot(TEXT("11-unload-before"), FString::Printf(TEXT("at the Citadel with %lld ore aboard, wallet %lld"),
				U ? (long long)U->carrying : -1LL, (long long)Ore));
			T.Phase = 23;
		}
		return;
	case 24:
		if (Since() > 1.0)
		{
			Check(Ore > Shots.OreBefore, FString::Printf(TEXT("the wallet grew: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
			Shoot(TEXT("12-unload-after"), FString::Printf(TEXT("unloaded, wallet %lld -> %lld"), (long long)Shots.OreBefore, (long long)Ore));
		}
		return;
	case 25:
	{
		// Back round the Citadel's north side to the start pad.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y - 6), FVector2D(C->position.x + 5, C->position.y - 6), Cell(76, 48)});
		Advance();
		return;
	}
	case 26:
	{
		const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
		if (Near && Near->Distance < 1.5)
		{
			Stop(Pilot->Yaw());
			Advance();
			return;
		}
		if (Drive() < 0) Stalled(TEXT("to the pad"));
		return;
	}
	case 27:
		if (Since() > 1.0)
		{
			Check(OpenWorldMap(), TEXT("M opened the world map on the pad"));
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			Check(Map && Map->PickPad(TEXT("cold-fork"), TEXT("landing")), TEXT("picked Cold Fork's pad"));
			Advance();
		}
		return;
	case 28:
		if (Since() > 1.0) Shoot(TEXT("13-world-map-pick"), TEXT("the world map on the pad, Cold Fork picked"));
		return;
	case 29:
		if (const TSharedPtr<SAcWorldMap> Map = WorldMapWidget())
		{
			Shots.VisitWas = Mmo->Visit();
			Map->Fly();
			T.Phase = 40;
			T.PhaseAt = Now;
			return;
		}
		Check(false, TEXT("the world map still open for FLY"));
		Finish();
		return;
	case 40:
		// The flight (4 s): a frame from its middle.
		if (Mmo->FlyingTo())
		{
			if (Since() > 2.0) Shoot(TEXT("14-flight-mid"), FString::Printf(TEXT("in flight to %s"), *Utf(Mmo->FlyingTo()->id)));
			return;
		}
		TimedOut(TEXT("lifted off"), 10);
		return;
	case 41:
		if (Mmo->Visit() > Shots.VisitWas && Seated() && Since() > 5.0)
		{
			Check(Here == TEXT("cold-fork"), FString::Printf(TEXT("landed %s"), *Where()));
			Shoot(TEXT("15-landing-cold-fork"), TEXT("landed on Cold Fork's pad"));
			return;
		}
		TimedOut(TEXT("landed"));
		return;
	case 42:
		// South off the plateau by its ramp, over the south edge into the
		// start region.
		Shots.VisitWas = Mmo->Visit();
		Route({Cell(82, 56), Cell(84, 62), Cell(84, 76), Cell(73, 84), Cell(73, 104)});
		Advance();
		return;
	case 43:
		if (Mmo->Visit() > Shots.VisitWas)
		{
			Stop(Pilot->Yaw());
			Advance();
			return;
		}
		if (Drive() < 0) Stalled(TEXT("to Cold Fork's south edge"));
		else TimedOut(TEXT("crossed into the start region"), 90);
		return;
	case 44:
		if (Seated() && Since() > 1.0)
		{
			Check(Here == TEXT("start"), FString::Printf(TEXT("crossed %s"), *Where()));
			Shots.UnitAt = FVector2D(U->position.x, U->position.y);
			Shoot(TEXT("16-edge-crossing"), TEXT("just after crossing into the start region"));
			return;
		}
		TimedOut(TEXT("seated after the crossing"), 15);
		return;
	case 45:
		// Stood still a while (where it stands, now and 4 s later: the
		// server's unit and the drawn one must agree), then QUIT TO HOME
		// and MMORPG (TEST): the world resumed.
		if (Since() < 4.0) return;
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: before leaving: %s (%.2f cells from 4 s ago)"), *Where(),
			FVector2D::Distance(FVector2D(U->position.x, U->position.y), Shots.UnitAt));
		Shots.Region = Here;
		Shots.UnitAt = FVector2D(U->position.x, U->position.y);
		StopMmo();
		StartMmo(false);
		Advance();
		return;
	case 46:
		if (Seated() && Mmo->Here() && Since() > 6.0)
		{
			const double Off = FVector2D::Distance(FVector2D(U->position.x, U->position.y), Shots.UnitAt);
			Check(Here == Shots.Region && Off < 1.0, FString::Printf(TEXT("resumed where it left (%.2f cells off) %s"), Off, *Where()));
			Shoot(TEXT("17-resumed"), TEXT("after leaving the mode and resuming"));
			return;
		}
		TimedOut(TEXT("seated after the resume"));
		return;
	default:
		Check(Shots.Saved == 17, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
		Finish();
		return;
	}
}
