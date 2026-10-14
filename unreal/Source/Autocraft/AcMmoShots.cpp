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
//   07 the level-up cards (none in the mode since P5: skipped; the run
//      gives the new pilot 1100 XP, -AcMmoDevXp, level 3 for the Derrick)
//   08 a Citadel going up               09 the Citadel finished
//   10 drilling ore                     11 unloading: before, 12 after
//   13 the world map, Cold Fork picked  14 the flight, mid-flight
//   15 landed at Cold Fork's pad        16 the edge crossing into the start
//                                          region, just after arrival
//   17 after leaving the mode and resuming
//   18 after a level-up pick (Z); before 01 and here the HUD's plates must
//      draw with their frames (FAcHudStyle::PlateProblem)
//   the MH loop, between 12 and 13 (docs/mmorpg-play.md "The loop"):
//   19 the Derrick's ghost on the south well    20 the Derrick built
//   21 pumping MH inside it                     22 the MH unloaded at the
//                                                  Citadel, the wallet's MH up
//
// -AcMmoShotsP5 (with -AcMmoDevXp=7100, level 12): only the progression
// (docs/prospector-progression.md "On screen"), DIR/p5-NN-moment.png:
//   01 the HUD: the points badge, the slot row   02 the skill screen (K), empty
//   03 points spent, Overcharge in Q              04 the BLUEPRINTS tab
//   05 Q used: the slot's cooldown ring           06 the level-up banner (its
//      words from AcProgressHud for 9 to 10, shown directly) and a find toast
//   07 the build menu from the blueprints
//
// -AcMmoShotsG2 (it adds -AcMmoDevRig: a finished Citadel, level 25, a
// full wallet): contracts, respawn, depot and own pads (docs/contracts.md,
// docs/prospector-progression.md "On the server (P6)"), DIR/g2-NN-moment.png:
//   01 the KSTR contract panel      02 a contract done: its toast (the news
//      shown directly, its words from AcProgressHud)
//   03 X: an abandon armed          04 slot 3 abandoned: back at dawn
//   05 the respawn screen (shown directly for a Citadel, 40 ore kept)
//   06 Homing beacon learned: the world map at the pilot's own Citadel pad,
//      the contracts' marks           07 the opening ring at the Lander wreck
//   08 the wreck opened: its news toast   09 the world map, the site opened
//   10 a Hab Dome built, a load drilled: the depot prompt
//   11 the load unloaded at the Hab Dome
//
// -AcMmoShotsRingshadow (with -AcMmoRingshadow=70: a sweep 70 s after the
// server starts; docs/weather.md "Ringshadow sweep"): only the sweep, third
// person on the start pad toward the giant, DIR/rs-NN-moment.png:
//   01 before: the radio's toast, the land in the sun
//   02 toward the giant: the band's edge 20-30 cells ahead over the berms
//   02b the same from a boom 4 times as long (ac.Pilot.ShotBoom), 6-19 off
//   03 its edge at the pilot, looking up: the ring's edge on the sun, god rays
//   04 under the band    05 under it, looking up: the sun through the ring
//   06 under it, first person at the nearest ore (the veins flaring)
//   07 after: the band gone east   08 looking east at it
// The giant's comet impacts are off meanwhile (ac.GasGiant.Events 0).
//
// -AcMmoShotsStarfall (with -AcMmoStarfall=40 -AcMmoClockHour=17: a
// starfall near the pilot 40 s after it comes, late in the afternoon;
// docs/weather.md "Starfall"): third person on the start pad toward the
// crater's spot, DIR/sf-NN-moment.png:
//   01 told: the radio's toast         02 the streak high in the sky
//   03 the streak low, over the land   04 the impact's flash
//   05 the dust ring                   06 the crater and its hot deposit by day
//   07 the same, first person          08 at night, third person
//   09 at night, first person
//
// -AcMmoShotsRig (with -AcMmoDevRig: the server gives the new pilot a
// finished Citadel, Foundry and Lab, the Prospector where it unloads, and a
// stocked rig with a job at each; docs/prospector-progression.md "Foundry
// and Lab"): the rig screen (J), DIR/rig-NN-moment.png:
//   01 home: unloading at the Citadel     02 the rig screen: fitting at home,
//      the Foundry making, the Lab researching
//   03 Drill head Mk III fitted from the stores, the Mk II back in them,
//      and the Foundry's refusal while busy   04 the skill screen's RIG tab
//   05 the Foundry's job done: the stores up, the Foundry idle
//   06 a Cargo pod Mk II asked for: the Foundry's new countdown
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
#include "AcDaylight.h"

#include "AcGasGiant.h"
#include "AcHudStyle.h"
#include "AcLeveling.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotBuild.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "SAcLevelUp.h"

#include "Pilot.h"
#include "Ringshadow.h"
#include "Rules.h"
#include "Starfall.h"
#include "Simulation.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
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

	/// -AcMmoShotsP5: the points it spends, in order (9 of level 12's 11),
	/// how many are in, and when it last asked.
	constexpr ac::Skill P5Spends[] = {ac::Skill::quickDrill, ac::Skill::quickDrill, ac::Skill::quickDrill, ac::Skill::quickDrill,
		ac::Skill::quickDrill, ac::Skill::overcharge, ac::Skill::lightFrame, ac::Skill::lightFrame, ac::Skill::plating};
	int32 P5Spent = 0;
	double P5AskedAt = 0;

	/// -AcMmoShotsG2: the Hab Dome it builds.
	int64 G2Dome = -1;

	const ac::Structure* HabDomeOf(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::habDome && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// -AcMmoShotsOnly=sf-02,sf-08 (names or numbers, comma-separated): the
	/// moments to shoot; the others are passed over (counted as skipped),
	/// their waits too where the set can. Unset: every moment.
	bool Wanted(const TCHAR* Name)
	{
		FString Only;
		if (!FParse::Value(FCommandLine::Get(), TEXT("AcMmoShotsOnly="), Only, false) || Only.IsEmpty()) return true;
		const FString N(Name);
		// The moment's number: the first piece that starts with a digit
		// ("02" in sf-02-streak-high, "02b" in rs-02b-edge-high).
		FString Number;
		TArray<FString> Pieces;
		N.ParseIntoArray(Pieces, TEXT("-"));
		for (const FString& P : Pieces)
			if (!P.IsEmpty() && FChar::IsDigit(P[0]))
			{
				Number = P;
				break;
			}
		TArray<FString> Asked;
		Only.ParseIntoArray(Asked, TEXT(","));
		for (FString A : Asked)
		{
			A.TrimStartAndEndInline();
			if (A.IsEmpty()) continue;
			if (N.Equals(A, ESearchCase::IgnoreCase) || N.StartsWith(A + TEXT("-"), ESearchCase::IgnoreCase)) return true;
			// A number alone ("8", "08", "02b"), or a set's prefix and number ("sf-8").
			FString Tail = A;
			if (int32 Dash; A.FindLastChar(TEXT('-'), Dash)) Tail = A.Mid(Dash + 1);
			if (Tail.IsEmpty() || Number.IsEmpty() || !FChar::IsDigit(Tail[0])) continue;
			if (A != Tail && !N.StartsWith(A.Left(A.Len() - Tail.Len()), ESearchCase::IgnoreCase)) continue;
			if (Tail.Equals(Number, ESearchCase::IgnoreCase) || (Tail.IsNumeric() && Number.IsNumeric() && FCString::Atoi(*Tail) == FCString::Atoi(*Number)))
				return true;
		}
		return false;
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
	const int64 Hydrogen = S && !S->players.empty() ? S->players[0].hydrogen : -1;

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
		return U ? FString::Printf(TEXT("at %.1f,%.1f in %s, %lld ore, %lld MH"), U->position.x, U->position.y, *Here, (long long)Ore,
					   (long long)Hydrogen)
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
		// The new pilot starts at level 3 (1100 XP): the Derrick's floor, so
		// the MH loop (19-22) can build one, unless the run says otherwise.
		if (double Xp = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp)) FCommandLine::Append(TEXT(" -AcMmoDevXp=1100"));
		// -AcMmoShotsG2: the dev rig's home (a Citadel, level 25, a full wallet),
		// for this step only (the flow steps before it in the run go without).
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsG2")) && !FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig")))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		// -AcMmoShotsStarfall: the fall 10 s after the pilot drives (the
		// streak shows its last 10 s), late in the afternoon, unless the run
		// says otherwise.
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsStarfall")))
		{
			if (double In = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoStarfall="), In)) FCommandLine::Append(TEXT(" -AcMmoStarfall=10"));
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=17"));
		}
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
			// -AcMmoShotsLook: only the world's look (100-111).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsLook"))) T.Phase = 100;
			// -AcMmoShotsP5: only the progression (200-215).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsP5"))) T.Phase = 200;
			// -AcMmoShotsRingshadow: only the ringshadow sweep (300-315).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsRingshadow"))) T.Phase = 300;
			// -AcMmoShotsStarfall: only a starfall (400-414).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsStarfall"))) T.Phase = 400;
			// -AcMmoShotsG2: contracts, respawn, depot, own pads (600-629).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsG2"))) T.Phase = 600;
			// -AcMmoShotsRig: only the rig screen (500-513).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsRig"))) T.Phase = 500;
		}
		else TimedOut(TEXT("seated"));
		return;
	case 2:
		if (Since() > 5.0)
		{
			const FString Problem = FAcHudStyle::PlateProblem();
			Check(Problem.IsEmpty(), FString::Printf(TEXT("the HUD plates draw with their frames (%s)"), *Problem));
			Shoot(TEXT("01-spawn-pad-third"), TEXT("on the start pad, third person"));
		}
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
			if (!Shots.bCardsShot && !bCards && Built(*C) >= 0.4)
			{
				// A world without picks (the skill tree replaces them): no cards to shoot.
				UE_LOG(LogAutocraft, Display, TEXT("mmoshots: 07-level-up-cards skipped: no cards"));
				Shots.bCardsShot = true;
				Shots.Skipped += 1;
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
			// Then the MH loop (50-58), and back to the pad (25).
			T.Phase = 50;
		}
		return;
	case 50:
	{
		// Round the Citadel's west side, south to stand east of the south
		// well (60, 61), facing it: a Derrick's reach (3 cells) away.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y + 5), Cell(63, 61)});
		Advance();
		return;
	}
	case 51:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the south well"));
		return;
	case 52:
		if (Since() > 1.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::derrick) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::derrick, FString::Printf(TEXT("B, %d held out a Derrick"), Number));
			const TOptional<ac::Vec2> At = Build ? Build->Spot(ac::StructureKind::derrick) : TOptional<ac::Vec2>();
			Check(At.IsSet(), TEXT("the Derrick's ghost on a well"));
			Advance();
		}
		return;
	case 53:
		if (Since() > 1.5) Shoot(TEXT("19-derrick-ghost"), TEXT("the Derrick's ghost on the south well"));
		return;
	case 54:
		Shots.OreBefore = Ore;
		Pilot->Press(true);
		Pilot->Press(false);
		Advance();
		return;
	case 55:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Derrick built, not refused: %s"), *Why));
		}
		for (const ac::Structure& B : S->structures)
		{
			if (B.kind != ac::StructureKind::derrick || B.owner != ac::Pilot::player) continue;
			Shots.Derrick = B.id;
			if (B.complete() && Since() > 2.0)
			{
				Check(Ore == Shots.OreBefore - ac::Rules::cost(ac::StructureKind::derrick),
					FString::Printf(TEXT("the Derrick paid: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
				Shoot(TEXT("20-derrick-built"), TEXT("the Derrick finished on the south well"));
				return;
			}
		}
		TimedOut(TEXT("the Derrick finished"), WeldLimit);
		return;
	case 56:
	{
		// Up to the Derrick until the act key would go in; then a click.
		const std::optional<ac::Structure> D = S->structure(Shots.Derrick);
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (D && Target && Target->is<ac::PilotTarget::Derrick>())
		{
			Stop(Face(D->position));
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
			return;
		}
		if (D && Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(D->position));
		TimedOut(TEXT("at the Derrick"), 15);
		return;
	}
	case 57:
		// Inside, pumping: a picture a moment in.
		if (U && U->task == ac::Unit::Task::inDerrick && Since() > 1.0)
		{
			Shoot(TEXT("21-pumping"), TEXT("pumping MH inside the Derrick"));
			return;
		}
		TimedOut(TEXT("inside the Derrick"), 10);
		return;
	case 58:
		if (U && U->carrying > 0 && U->hydrogen == true)
		{
			Shots.HydrogenBefore = Hydrogen;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load of MH pumped"), 15);
		return;
	case 59:
		if (U && U->carrying == 0)
		{
			Stop(Face(CitadelOf(*S)->position));
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("the MH unloaded at the Citadel"), 30);
		return;
	case 60:
		if (Since() > 1.0)
		{
			Check(Hydrogen > Shots.HydrogenBefore && Hydrogen > 0,
				FString::Printf(TEXT("the wallet's MH grew: %lld -> %lld"), (long long)Shots.HydrogenBefore, (long long)Hydrogen));
			Shoot(TEXT("22-mh-unloaded"), FString::Printf(TEXT("MH unloaded, wallet %lld -> %lld MH"), (long long)Shots.HydrogenBefore,
				(long long)Hydrogen));
		}
		return;
	case 61:
	{
		// Back round to the Citadel's west side, where 25 sets off for the pad.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x - 5, C->position.y + 5), FVector2D(C->position.x - 5, C->position.y)});
		Advance();
		return;
	}
	case 62:
		if (const int32 D = Drive(); D == 1) T.Phase = 25;
		else if (D < 0) Stalled(TEXT("back round the Citadel"));
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
	case 47:
		// A level-up pick (Z), then the HUD's plates must still draw with
		// their frames (Q5: they once drew as flat glass; FAcHudStyle::PlateProblem).
		if (!Leveling || !Leveling->DrivenOffer().IsSet())
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: 18-after-pick skipped: no cards"));
			Shots.Skipped += 1;
			T.Phase = 49;
			T.PhaseAt = Now;
			return;
		}
		Check(Leveling->PickKey(TEXT("z")), TEXT("Z picked a level-up card"));
		Advance();
		return;
	case 48:
		if (Since() > 1.5)
		{
			const FString Problem = FAcHudStyle::PlateProblem();
			Check(Problem.IsEmpty(), FString::Printf(TEXT("the HUD plates draw with their frames after the pick (%s)"), *Problem));
			Shoot(TEXT("18-after-pick"), TEXT("after a level-up pick (Z)"));
		}
		return;
	// The look (-AcMmoShotsLook, docs/mmorpg-m6.md V2): the spawn, the giant
	// from the cockpit, the ore up close and drilled, a well; the hour and
	// the weather from -AcMmoHour and -AcMmoWeather.
	case 100:
		if (Since() > 5.0) Shoot(TEXT("look-01-spawn-third"), TEXT("on the start pad, third person"));
		return;
	case 101:
	{
		Pilot->ToggleView();
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X));
		Pilot->Eye().Pitch = 0.12;
		Advance();
		return;
	}
	case 102:
		if (Since() > 2.5) Shoot(TEXT("look-02-first-giant"), TEXT("first person, facing the giant"));
		return;
	case 103:
	{
		Pilot->ToggleView();
		Pilot->Eye().Pitch = 0;
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("ore in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Patch = Best;
		const ac::Vec2 P = S->patches[Best].position;
		const ac::Vec2 Off = P - U->position;
		const double L = std::max(ac::length(Off), 1e-6);
		Route({FVector2D(P.x - Off.x / L * 4.0, P.y - Off.y / L * 4.0)});
		Advance();
		return;
	}
	case 104:
		if (const int32 D = Drive(); D != 0)
		{
			Stop(Face(S->patches[Shots.Patch].position));
			Advance();
		}
		return;
	case 105:
		if (Since() > 2.0) Shoot(TEXT("look-03-ore-near"), TEXT("a Stardust deposit, a few cells off"));
		return;
	case 106:
	{
		// Up to it until the act key would drill it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(P));
		TimedOut(TEXT("at the ore"), 15);
		return;
	}
	case 107:
		if (U && U->task == ac::Unit::Task::mining && Since() > 1.5)
		{
			Shoot(TEXT("look-04-drilling"), TEXT("drilling ore up close"));
			return;
		}
		TimedOut(TEXT("drilling"), 15);
		return;
	case 108:
	{
		Pilot->Press(false);
		static const std::vector<ac::Well> None;
		const std::vector<ac::Well>& Wells = S->wells ? *S->wells : None;
		int32 Best = -1;
		for (int32 I = 0; I < (int32)Wells.size(); ++I)
		{
			if (Best < 0 || ac::distance(Wells[I].position, U->position) < ac::distance(Wells[Best].position, U->position)) Best = I;
		}
		Check(Best >= 0, TEXT("a well in the start region"));
		if (Best < 0)
		{
			Finish();
			return;
		}
		Shots.Derrick = Best;
		const ac::Vec2 W = Wells[Best].position;
		const ac::Vec2 Off = W - U->position;
		const double L = std::max(ac::length(Off), 1e-6);
		Route({FVector2D(W.x - Off.x / L * 7.0, W.y - Off.y / L * 7.0)});
		Advance();
		return;
	}
	case 109:
		if (const int32 D = Drive(); D != 0)
		{
			// A little to its side, so the Prospector does not hide the pool.
			Stop(Face((*S->wells)[size_t(Shots.Derrick)].position) - 0.3);
			Advance();
		}
		return;
	case 110:
		if (Since() > 2.0) Shoot(TEXT("look-05-well"), TEXT("a Metallic Hydrogen well"));
		return;
	case 111:
		Check(Shots.Saved == 5, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsP5
	case 200:
		// The dev XP's progress in, first person.
		if (Mmo->Progress() && Since() > 4.0)
		{
			const ac::net::ProgressMessage& P = *Mmo->Progress();
			Check(P.level >= 10 && P.points >= (int64)UE_ARRAY_COUNT(P5Spends),
				FString::Printf(TEXT("the dev XP's progress: level %lld, %lld points"), (long long)P.level, (long long)P.points));
			Pilot->ToggleView();
			P5Spent = 0;
			P5AskedAt = 0;
			Advance();
		}
		else TimedOut(TEXT("the progress"));
		return;
	case 201:
		if (Since() > 2.0) Shoot(TEXT("p5-01-hud-points"), TEXT("the HUD: the points badge and the slot row"));
		return;
	case 202:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid(), TEXT("K opens the skill screen"));
		Advance();
		return;
	case 203:
		if (Since() > 1.5) Shoot(TEXT("p5-02-skill-screen-empty"), TEXT("the skill screen, nothing spent"));
		return;
	case 204:
	{
		// One point at a time through the screen, each once the last is in.
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P)
		{
			Check(false, TEXT("the skill screen stays open"));
			Finish();
			return;
		}
		if (P5Spent >= (int32)UE_ARRAY_COUNT(P5Spends))
		{
			Advance();
			return;
		}
		const ac::Skill Want = P5Spends[P5Spent];
		int64 Ranks = 0;
		for (int32 I = 0; I <= P5Spent; ++I) Ranks += P5Spends[I] == Want ? 1 : 0;
		const auto Has = P->ranks.find(Want);
		if (Has != P->ranks.end() && Has->second >= Ranks)
		{
			P5Spent += 1;
			P5AskedAt = 0;
			T.PhaseAt = Now;
			return;
		}
		if (!Mmo->SkillRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("spend %s: %s"), *Utf(std::string(ac::rawValue(Want))), *Mmo->SkillRefusal()));
			Finish();
			return;
		}
		// Asked, and again each second until the server has it.
		if (Now - P5AskedAt > 1.0)
		{
			W->PickSkill(Want);
			W->Learn();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("the points spent"), 20);
		return;
	}
	case 205:
	{
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->slots[0] == ac::Skill::overcharge)
		{
			W->PickSkill(ac::Skill::overcharge);
			Advance();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickSkill(ac::Skill::overcharge);
			W->SlotPicked(0);
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Overcharge in Q"), 10);
		return;
	}
	case 206:
		if (Since() > 1.5) Shoot(TEXT("p5-03-skill-screen-spent"), TEXT("points spent, Overcharge picked and in Q"));
		return;
	case 207:
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->SetTab(SAcSkillScreen::ETab::Blueprints);
		Advance();
		return;
	case 208:
		if (Since() > 1.0) Shoot(TEXT("p5-04-skill-screen-blueprints"), TEXT("the BLUEPRINTS tab"));
		return;
	case 209:
		// Out, and Q: Overcharge's cooldown on the slot row.
		CloseSkillScreen();
		if (ac::net::Mirror* Mine = Mmo->Mirror(); Mine && Mine->simulation().pilot) Mine->simulation().pilot->slot = 1;
		Advance();
		return;
	case 210:
		if (Since() > 1.5) Shoot(TEXT("p5-05-hud-slot-cooldown"), TEXT("Q used: the cooldown ring on the slot row"));
		return;
	case 211:
		// The banner as level 10 shows it (the real path's words), and a find.
		if (Leveling)
		{
			Leveling->ShowBanner(AcLeveling::BannerTitle(ac::UnitKind::prospector, 10), AcProgressHud::BannerSub(9, 10),
				AcProgressHud::BannerGate(9, 10));
		}
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this); Hud && Hud->Widget())
		{
			Hud->Widget()->Toast(TEXT("BLUEPRINT FOUND: FOUNDRY"), TEXT("Opens at level 15  ·  +750 XP"), FAcHudStyle::LevelGold(), Now);
		}
		Advance();
		return;
	case 212:
		if (Since() > 0.8) Shoot(TEXT("p5-06-level-banner"), TEXT("the level-up banner and a find toast"));
		return;
	case 213:
		// The banner and the toast gone, then B.
		if (Since() > 5.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Key(TEXT("b")) && Build->MenuOpen(), TEXT("B opened the build menu"));
			Advance();
		}
		return;
	case 214:
		if (Since() > 1.0) Shoot(TEXT("p5-07-build-menu"), TEXT("the build menu from the blueprints"));
		return;
	case 215:
		Check(Shots.Saved == 7, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsG2
	case 600:
		// The dev rig's Citadel, the contracts drawn; first person.
		if (Mmo->Progress() && Mmo->Progress()->contracts.size() == 3 && S && CitadelOf(*S) && Since() > 4.0)
		{
			// Away from the Citadel's wall (the dev rig stands it there).
			Pilot->ToggleView();
			Stop(Face(U->position * 2.0 - CitadelOf(*S)->position));
			P5AskedAt = 0;
			G2Dome = -1;
			Advance();
		}
		else TimedOut(TEXT("the contracts and the dev rig's Citadel"));
		return;
	case 601:
		if (Since() > 2.0) Shoot(TEXT("g2-01-contracts"), TEXT("the KSTR contract panel"));
		return;
	case 602:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			const ac::net::Contract& C = Mmo->Progress()->contracts[0];
			ac::net::ProgressMessage::News N;
			N.kind = ac::net::ProgressMessage::NewsKind::contract;
			N.id = std::to_string(C.id);
			N.xp = C.xp;
			N.value = C.hydrogen;
			Hud->PreviewNews(N);
		}
		Advance();
		return;
	case 603:
		if (Since() > 0.8) Shoot(TEXT("g2-02-contract-done"), TEXT("a contract done: its toast"));
		return;
	case 604:
		if (Since() > 5.0)
		{
			UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
			if (Hud) Hud->ToggleAbandon();
			Check(Hud && Hud->AbandonArmed(), TEXT("X armed an abandon"));
			Advance();
		}
		return;
	case 605:
		if (Since() > 0.6) Shoot(TEXT("g2-03-abandon-armed"), TEXT("X: which contract to drop"));
		return;
	case 606:
	{
		UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this);
		Check(Hud && Hud->Abandon(2), TEXT("3 asked to abandon slot 3"));
		Advance();
		return;
	}
	case 607:
		if (Mmo->Progress()->contracts.size() == 3 && Mmo->Progress()->contracts[2].state == ac::net::Contract::State::abandoned)
		{
			if (Since() > 1.0) Shoot(TEXT("g2-04-abandoned"), TEXT("slot 3 abandoned: back at dawn"));
			return;
		}
		TimedOut(TEXT("slot 3 abandoned"), 10);
		return;
	case 608:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			ac::net::ProgressMessage::Respawn R;
			R.at = (Mmo->Sky() ? Mmo->Sky()->time : 0) + 8;
			R.region = Mmo->Here() ? Mmo->Here()->id : std::string();
			R.home = ac::StructureKind::citadel;
			R.cargo = 40;
			Hud->PreviewRespawn(R);
		}
		Advance();
		return;
	case 609:
		if (Since() > 1.2) Shoot(TEXT("g2-05-respawn"), TEXT("the respawn screen"));
		return;
	case 610:
	{
		// Homing beacon (the Citadel a pad): its tier's points first.
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->PreviewRespawn(std::nullopt);
		const std::optional<ac::ProspectorProgress> G = AcProgressHud::Progress(*Mmo);
		if (!G)
		{
			TimedOut(TEXT("the progress"), 10);
			return;
		}
		if (G->rank(ac::Skill::homingBeacon) >= 1)
		{
			Advance();
			return;
		}
		if (Now - P5AskedAt > 0.7)
		{
			std::optional<ac::Skill> Want;
			if (!G->spendRefusal(ac::Skill::homingBeacon)) Want = ac::Skill::homingBeacon;
			const ac::Branch Branch = ac::Progression::node(ac::Skill::homingBeacon).branch;
			for (const ac::SkillNode& N : ac::Progression::nodes())
			{
				if (!Want && N.branch == Branch && N.tier <= 2 && !N.active && !G->spendRefusal(N.skill)) Want = N.skill;
			}
			if (Want) Mmo->Spend(*Want);
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Homing beacon learned"), 40);
		return;
	}
	case 611:
	{
		// The Citadel in the World message as the pilot's own pad.
		bool bOwn = false;
		if (const ac::net::WorldMessage::Region* R = Mmo->Here())
		{
			for (const ac::net::WorldMessage::Pad& Q : R->pads) bOwn |= Q.building == ac::StructureKind::citadel;
		}
		if (bOwn && Since() > 1.0)
		{
			ToggleWorldMap();
			Check(WorldMapWidget().IsValid(), TEXT("M opened the world map"));
			Advance();
			return;
		}
		TimedOut(TEXT("the Citadel a pad of the pilot's own"), 15);
		return;
	}
	case 612:
		if (Since() > 1.5)
		{
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			Check(Near && Near->Building && Near->Within(), FString::Printf(TEXT("at the own pad: %s %.1f of %.1f"),
				Near ? *Near->Name : TEXT("-"), Near ? Near->Distance : -1.0, Near ? Near->Reach : -1.0));
			Shoot(TEXT("g2-06-map-own-pad"), TEXT("the world map at the pilot's own Citadel pad, the contracts' marks"));
		}
		return;
	case 613:
		// To the Lander wreck (a 5 s hold opens it; a crate opens on touch).
		CloseWorldMap();
		Route({Cell(66, 34), Cell(54, 29.5)});
		Advance();
		return;
	case 614:
		if (const int32 D = Drive(); D == 1)
		{
			// Up to its hull, then hold the act key.
			const FVector2D Wreck = Cell(50, 30);
			const ac::Vec2 At(Wreck.X, Wreck.Y);
			if (U && ac::distance(U->position, At) <= 1.9)
			{
				Stop(Face(At));
				Pilot->Press(true);
				Advance();
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.5), Face(At));
			TimedOut(TEXT("at the Lander wreck"), 40);
		}
		else if (D < 0) Stalled(TEXT("to the Lander wreck"));
		return;
	case 615:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->opening && P->opening->since > 0 && P->opening->needs > 0)
		{
			const double Share = ((Mmo->Sky() ? Mmo->Sky()->time : 0) - P->opening->since) / P->opening->needs;
			if (Share > 0.35 && Share < 0.85)
			{
				Shoot(TEXT("g2-07-opening-ring"), FString::Printf(TEXT("the opening ring at the Lander wreck, %.0f%%"), Share * 100));
				return;
			}
		}
		else if (P && P->opening && !P->opening->refusal.empty())
		{
			Check(false, FString::Printf(TEXT("the wreck opens: %s"), *Utf(P->opening->refusal)));
			Finish();
			return;
		}
		TimedOut(TEXT("the wreck opening"), 20);
		return;
	case 616:
		if (Mmo->Progress()->sitesFound.count("start/Lander wreck"))
		{
			Pilot->Press(false);
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened"), 20);
		return;
	case 617:
		// The toast faded in.
		if (Since() > 0.8) Shoot(TEXT("g2-08-site-toast"), TEXT("the wreck opened: the news toast"));
		return;
	case 618:
		ToggleWorldMap();
		Advance();
		return;
	case 619:
		if (Since() > 1.5) Shoot(TEXT("g2-09-map-sites"), TEXT("the world map: the site opened, the contracts' marks"));
		return;
	case 620:
		// To the main run's build spot by the pad (its Citadel's ghost fits there).
		CloseWorldMap();
		Route({Cell(62, 34), Cell(69, 48)});
		Advance();
		return;
	case 621:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			if (Since() > 1.0) Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 622:
	{
		// A Hab Dome there (B, its number, the act key).
		UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
		const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::habDome) + 1;
		Check(Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet(),
			FString::Printf(TEXT("%d held out a Hab Dome"), Number));
		Advance();
		return;
	}
	case 623:
		if (Since() > 1.0)
		{
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 624:
		if (const ac::Structure* D = S ? HabDomeOf(*S) : nullptr; D && D->complete())
		{
			G2Dome = D->id;
			Route({Cell(66, 42), Cell(59, 43.5), Cell(58, 46)});
			Advance();
			return;
		}
		for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("the Hab Dome built, not refused: %s"), *Why));
		TimedOut(TEXT("the Hab Dome finished"), WeldLimit);
		return;
	case 625:
		if (const int32 D = Drive(); D == 1)
		{
			// Up to the deposit until the act key would drill it.
			const FVector2D Deposit = Cell(53, 46);
			const ac::Vec2 At(Deposit.X, Deposit.Y);
			const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
			if (Target && Target->is<ac::PilotTarget::Patch>())
			{
				Stop(Face(At));
				Pilot->Press(true);
				Advance();
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.6), Face(At));
			TimedOut(TEXT("at the ore"), 50);
		}
		else if (D < 0) Stalled(TEXT("to the ore"));
		return;
	case 626:
		if (U && U->carrying > 0 && U->task != ac::Unit::Task::mining)
		{
			Pilot->Press(false);
			if (const std::optional<ac::Structure> D = S->structure(G2Dome)) Stop(Face(D->position));
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 45);
		return;
	case 627:
		if (Since() > 1.5) Shoot(TEXT("g2-10-depot-prompt"), TEXT("a load, the Hab Dome as a depot: the prompt and the compass"));
		return;
	case 628:
		if (const std::optional<ac::Structure> D = S->structure(G2Dome))
		{
			if (U && U->carrying == 0)
			{
				Stop(Face(D->position));
				if (Since() > 0.4) Shoot(TEXT("g2-11-depot-unloaded"), TEXT("the load unloaded at the Hab Dome"));
				return;
			}
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.8), Face(D->position));
		}
		TimedOut(TEXT("unloaded at the Hab Dome"), 45);
		return;
	case 629:
		Check(Shots.Saved + Shots.Skipped == 11, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsRingshadow
	// The sweep from the start pad: the band comes out of the giant's side
	// of the sky (bearing 280°) and runs east over the pilot; the ring's arc
	// crosses the sun (late morning: 17° above the giant's limb).
	case 300:
	case 305:
	case 311:
	{
		// Third person toward the giant (300: low, the land ahead; 305: under
		// the band; 311: back from first person).
		if (T.Phase == 300)
		{
			// The giant's comet impacts are chance (one in 140-340 s, their shock
			// ring over the disc for ~20 s): none over these pictures.
			if (IConsoleVariable* Events = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.GasGiant.Events"))) Events->Set(0);
		}
		if (T.Phase == 311) Pilot->ToggleView();
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X));
		Pilot->Eye().Pitch = T.Phase == 305 ? 0.05 : -0.1;
		Advance();
		return;
	}
	case 303:
	case 307:
	{
		// Third person looking up at the sun, the giant's upper limb low in the
		// left of the picture.
		// The usual boom again after rs-02b.
		if (IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"))) Boom->Set(1.f);
		const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky();
		const FVector Sun = AAcDaylight::SunToward(AAcDaylight::HourAt(Sky ? Sky->Time : 0.0));
		Stop(std::atan2(Sun.Y, Sun.X) - 0.15);
		Pilot->Eye().Pitch = FMath::Clamp(std::asin(Sun.Z) - 0.42, 0.05, 0.75);
		Advance();
		return;
	}
	case 301:
		// The radio's toast, a minute before.
		if (const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky())
		{
			const double In = ac::Ringshadow::next(Sky->Time, Sky->DayLength).start - Sky->Time;
			if (In > 0 && In < ac::Ringshadow::warning - 2.0 && Since() > 1.0)
			{
				Shoot(TEXT("rs-01-before"), FString::Printf(TEXT("before the sweep (in %.0f s), the radio's toast"), In));
				return;
			}
		}
		TimedOut(TEXT("a ringshadow sweep coming (-AcMmoRingshadow=70)"), 240);
		return;
	case 302:
	case 304:
	case 306:
	case 312:
	{
		// The band where the pilot stands: 302 its leading edge coming
		// toward the pilot over the far ground (rs-02, rs-02b); 304 the edge at the pilot (the arc's
		// edge on the sun); 306 deep in it; 312 its trailing edge 6-20 cells past.
		const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky();
		const std::optional<ac::Ringshadow::Band> B =
			Sky && M ? ac::Ringshadow::band(Sky->Time, M->simulation().map->bounds, Sky->DayLength) : std::nullopt;
		if (B && U)
		{
			const double At = ac::dot(U->position, B->dir);
			const double Cover = ac::Ringshadow::cover(*B, U->position);
			if (T.Phase == 302)
			{
				// Toward the giant, the band's leading edge coming over the berms:
				// rs-02 from the usual boom 20-30 cells off, then rs-02b from a
				// boom 4 times as long (about 13 cells up) 6-19 cells off.
				IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"));
				const bool bHigh = Shots.Saved >= 2;
				if (bHigh && Boom && Boom->GetFloat() < 2.f)
				{
					Boom->Set(4.f);
					T.PhaseAt = Now;
				}
				const FVector G = FAcGasGiant::Direction();
				Stop(std::atan2(G.Y, G.X));
				Pilot->Eye().Pitch = bHigh ? -0.3 : -0.14;
				const double Off = At - B->lead;
				if (!bHigh && Off > 20 && Off < 30 && Since() > 1.0)
				{
					Shoot(TEXT("rs-02-edge-ahead"), FString::Printf(TEXT("toward the giant, the band's edge %.0f cells ahead"), Off));
					T.Phase -= 1;
					return;
				}
				if (bHigh && Off > 6 && Off < 19 && Since() > 1.0)
				{
					Shoot(TEXT("rs-02b-edge-high"), FString::Printf(TEXT("from up high, the band's edge %.0f cells ahead"), Off));
					return;
				}
			}
			if (T.Phase == 304 && At - B->lead < 1.5 && At - B->lead > -8 && Since() > 1.0)
			{
				Shoot(TEXT("rs-03-ring-edge-on-sun"), FString::Printf(TEXT("the band's edge at the pilot (%.1f cells), the ring's edge on the sun"), At - B->lead));
				return;
			}
			if (T.Phase == 306 && Cover > 0.99 && Since() > 1.0)
			{
				Shoot(TEXT("rs-04-under-band"), TEXT("under the band, toward the giant"));
				return;
			}
			if (T.Phase == 312 && B->trail - At > 6 && B->trail - At < 20 && Since() > 1.0)
			{
				Shoot(TEXT("rs-07-after"), FString::Printf(TEXT("after: the band %.0f cells past, east"), B->trail - At));
				return;
			}
		}
		else if (T.Phase == 312 && Sky && Since() > 2.0)
		{
			// The sweep over first: the land in the sun again.
			Shoot(TEXT("rs-07-after"), TEXT("after: the sweep over"));
			return;
		}
		TimedOut(T.Phase == 302 ? TEXT("the band's edge close")
			: T.Phase == 304   ? TEXT("the band's edge at the pilot")
			: T.Phase == 306   ? TEXT("the band over the pilot")
							   : TEXT("the band past"),
			150);
		return;
	}
	case 308:
		if (Since() > 1.5) Shoot(TEXT("rs-05-sun-through-ring"), TEXT("under the band, the sun through the ring"));
		return;
	case 309:
	{
		// First person at the nearest ore, its veins flaring in the shade.
		int64 Best = -1;
		for (int64 I = 0; I < (int64)S->patches.size(); ++I)
		{
			if (S->patches[I].remaining <= 0) continue;
			if (Best < 0 || ac::distance(S->patches[I].position, U->position) < ac::distance(S->patches[Best].position, U->position)) Best = I;
		}
		if (Best >= 0) Stop(Face(S->patches[Best].position));
		Pilot->ToggleView();
		Pilot->Eye().Pitch = -0.08;
		Advance();
		return;
	}
	case 310:
		if (Since() > 1.5) Shoot(TEXT("rs-06-under-ore-first"), TEXT("under the band, first person at the nearest ore"));
		return;
	case 313:
	{
		const FVector G = FAcGasGiant::Direction();
		Stop(std::atan2(G.Y, G.X) + UE_DOUBLE_PI);
		Pilot->Eye().Pitch = -0.1;
		Advance();
		return;
	}
	case 314:
		if (Since() > 1.5) Shoot(TEXT("rs-08-after-east"), TEXT("after, looking east: the band going away"));
		return;
	case 315:
		Check(Shots.Saved == 9, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
		if (IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"))) Boom->Set(1.f);
		if (IConsoleVariable* Events = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.GasGiant.Events"))) Events->Set(1);
		Finish();
		return;
	// MARK: -AcMmoShotsRig
	// The rig screen on --dev-rig's home: each ask through the screen.
	case 500:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->foundry && P->lab && P->rig.making && Since() > 4.0)
		{
			Check(P->rig.research.has_value(), TEXT("the dev rig: a Foundry, a Lab, a job at each"));
			Advance();
		}
		else TimedOut(TEXT("the dev rig"));
		return;
	case 501:
		if (Since() > 1.5) Shoot(TEXT("rig-01-home"), TEXT("home: where the Prospector unloads at the Citadel"));
		return;
	case 502:
		Check(OpenRigScreen() && RigScreenWidget().IsValid(), TEXT("J opens the rig screen"));
		Check(RigScreenWidget().IsValid() && RigScreenWidget()->GetInfo().FitWhyNot.IsEmpty(), TEXT("the screen has the pilot at home"));
		Advance();
		return;
	case 503:
		if (Since() > 1.5) Shoot(TEXT("rig-02-screen"), TEXT("the rig screen: fitting, the Foundry making, the Lab researching"));
		return;
	case 504:
	{
		// Drill head Mk III from the stores: the screen's own ask.
		const TSharedPtr<SAcRigScreen> W = RigScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (W && P && P->rig.fittedMark(ac::Module::drillHead) == 3)
		{
			Check(P->rig.stock(ac::Module::drillHead, 2) == 1, TEXT("Drill head Mk III fitted, the Mk II back in the stores"));
			// The Foundry is busy: its refusal, as the screen says it.
			W->PickCell({ac::Module::cargoPod, SAcRigScreen::EPanel::Foundry, 2});
			W->Act();
			Check(W->RefusalText().StartsWith(TEXT("The Foundry is busy")), FString::Printf(TEXT("the busy Foundry refuses: %s"), *W->RefusalText()));
			Advance();
			return;
		}
		if (!Mmo->RigRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("fit: %s"), *Mmo->RigRefusal()));
			Finish();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickCell({ac::Module::drillHead, SAcRigScreen::EPanel::Fitting, 3});
			W->Act();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Drill head Mk III fitted"), 10);
		return;
	}
	case 505:
		if (Since() > 1.0) Shoot(TEXT("rig-03-fitted-busy"), TEXT("Drill head Mk III fitted; the busy Foundry's refusal"));
		return;
	case 506:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid() && !RigScreenWidget().IsValid(), TEXT("K: the skill screen in the rig's place"));
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->SetTab(SAcSkillScreen::ETab::Blueprints);
		Advance();
		return;
	case 507:
		if (Since() > 1.0) Shoot(TEXT("rig-04-skill-tabs"), TEXT("the skill screen's tabs: SKILLS, BLUEPRINTS, RIG"));
		return;
	case 508:
		// Tab from BLUEPRINTS: the rig screen again.
		if (const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget()) W->NextTab();
		Check(RigScreenWidget().IsValid() && !SkillScreenWidget().IsValid(), TEXT("Tab on BLUEPRINTS opens the rig screen"));
		Advance();
		return;
	case 509:
		// The Foundry's Plate Mk II comes out (150 s after the pilot came).
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && !P->rig.making && Since() > 1.0)
		{
			Check(P->rig.stock(ac::Module::plate, 2) == 2, TEXT("Plate Mk II made, into the stores"));
			Advance();
		}
		else TimedOut(TEXT("the Foundry's job done"), 200);
		return;
	case 510:
		if (Since() > 2.0) Shoot(TEXT("rig-05-made"), TEXT("the Foundry's job done: the stores up, the Foundry idle"));
		return;
	case 511:
	{
		const TSharedPtr<SAcRigScreen> W = RigScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->rig.making && P->rig.making->module == ac::Module::cargoPod)
		{
			Advance();
			return;
		}
		if (!Mmo->RigRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("make: %s"), *Mmo->RigRefusal()));
			Finish();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickCell({ac::Module::cargoPod, SAcRigScreen::EPanel::Foundry, 2});
			W->Act();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("Cargo pod Mk II asked for"), 10);
		return;
	}
	case 512:
		if (Since() > 3.0) Shoot(TEXT("rig-06-making"), TEXT("Cargo pod Mk II at the Foundry: its countdown"));
		return;
	case 513:
		CloseRigScreen();
		Check(Shots.Saved == 6, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsStarfall
	// A starfall near the pilot (the server's --starfall): from the start
	// pad toward the crater's spot, the streak coming in over it.
	case 400:
	case 401:
	case 402:
	case 403:
	case 404:
	case 405:
	case 406:
	case 407:
	case 408:
	case 409:
	case 410:
	case 411:
	case 412:
	case 413:
	case 414:
	{
		const std::optional<ac::net::Sky>& Sky = Mmo ? Mmo->Sky() : std::optional<ac::net::Sky>();
		const ac::net::WorldMessage* Wm = Mmo ? Mmo->World() : nullptr;
		const ac::net::Sky::Starfall* F = nullptr;
		if (Sky && Wm)
			for (const ac::net::Sky::Starfall& X : Sky->starfalls)
				if (X.region == Wm->here) F = &X;
		if (!F || !U)
		{
			TimedOut(TEXT("a starfall told in the pilot's region (-AcMmoStarfall=40)"), 120);
			return;
		}
		const double In = F->impact - Sky->time;
		const double Dark = ac::Rules::dark(Sky->hour());
		switch (T.Phase)
		{
		case 400:
		{
			// Between the crater and the way the streak comes from: both in view.
			const double ToCrater = Face(F->at);
			const double ToSky = Face(F->at + ac::Starfall::toward(F->from) * 400.0);
			Stop(ToCrater + 0.5 * FMath::UnwindRadians(ToSky - ToCrater));
			Pilot->Eye().Pitch = 0.16;
			Advance();
			return;
		}
		case 401:
			if (Since() > 1.5) Shoot(TEXT("sf-01-told"), FString::Printf(TEXT("told: the radio's toast, impact in %.0f s"), In));
			return;
		case 402:
			if (In <= 5.0) Shoot(TEXT("sf-02-streak-high"), FString::Printf(TEXT("the streak in the sky, %.1f s to go"), In));
			else TimedOut(TEXT("the streak"), 90);
			return;
		case 403:
			if (In <= 1.6) Shoot(TEXT("sf-03-streak-low"), FString::Printf(TEXT("the streak low over the land, %.1f s to go"), In));
			return;
		case 404:
			Stop(Face(F->at));
			Pilot->Eye().Pitch = 0.02;
			Advance();
			return;
		case 405:
			if (-In >= 0.12) Shoot(TEXT("sf-04-impact-flash"), FString::Printf(TEXT("the impact's flash, %.2f s after"), -In));
			return;
		case 406:
			if (-In >= 2.5) Shoot(TEXT("sf-05-dust-ring"), TEXT("the dust ring thrown out"));
			return;
		case 407:
		{
			// Up to the crater's edge on foot (25 s at most: rocks may stand
			// in the way), then a look at it.
			static double Halted = -1.0;
			if (ac::distance(F->at, U->position) > 9.0 && Since() < 25.0)
			{
				Pilot->SetTestInput(FVector2D(0.0, -1.0), Face(F->at));
				Halted = -1.0;
				return;
			}
			if (Halted < 0.0) Halted = Now;
			Stop(Face(F->at));
			Pilot->Eye().Pitch = -0.12;
			if (-In >= 14.0 && Now - Halted > 1.5)
				Shoot(TEXT("sf-06-crater-day"), FString::Printf(TEXT("the crater and its hot deposit by day (%.0f cells off, deposit %s)"),
					ac::distance(F->at, U->position), F->patch ? TEXT("landed") : TEXT("missing")));
			return;
		}
		case 408:
			Pilot->ToggleView();
			Pilot->Eye().Pitch = -0.1;
			Advance();
			return;
		case 409:
			if (Since() > 1.5) Shoot(TEXT("sf-07-crater-day-first"), TEXT("the crater by day, first person"));
			return;
		case 410:
			// Night: back to third person, waiting for the dark.
			Pilot->ToggleView();
			Advance();
			return;
		case 411:
			Pilot->Eye().Pitch = -0.06;
			if (Dark > 0.97 && Since() > 1.5) Shoot(TEXT("sf-08-crater-night"), FString::Printf(TEXT("the crater at night (hour %.1f)"), Sky->hour()));
			else TimedOut(TEXT("night (the world's clock runs on)"), 600);
			return;
		case 412:
			Pilot->ToggleView();
			Pilot->Eye().Pitch = -0.1;
			Advance();
			return;
		case 413:
			if (Since() > 1.5) Shoot(TEXT("sf-09-crater-night-first"), TEXT("the crater at night, first person"));
			return;
		default:
			Check(Shots.Saved == 9, FString::Printf(TEXT("%d pictures in %s"), Shots.Saved, *Shots.Dir));
			Finish();
			return;
		}
	}
	default:
		Check(Shots.Saved + Shots.Skipped == 22, FString::Printf(TEXT("%d pictures (%d skipped) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	}
}
