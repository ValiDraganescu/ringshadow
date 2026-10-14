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
// -AcMmoShotsW2 (it adds -AcMmoDevRig as G2 does): the blueprint sites on
// the ground and the compact contract panel (docs/mmorpg-m7.md W2),
// DIR/w2-NN-moment.png. 03-06 and 10-12 stand staged sites beside the
// region's own (AAcSites::Stage): the start region has a crate and a wreck
// only. 07-09 are the start region's own Lander wreck.
//   01 the contract panel, compact     02 T: the whole panel
//   03-06 crate, wreck, cache, vault near: shut (left), opened (right)
//   07 the Lander wreck, shut, known: its lamp and column
//   08 the opening ring on the wreck   09 the wreck opened: its lights out
//   10 four shut sites known, 18-44 cells off, by day: their columns
//   11 the same at dusk                12 the four shut, near, at dusk
//
// -AcMmoShotsO1 (a new pilot, no dev XP: the real levels): the first ten
// minutes (docs/contracts.md "First ten minutes", docs/mmorpg-m7.md O1)
// played through, a picture as each step opens (its "KSTR:" toast, the
// gold step on the panel, the tip line), DIR/o1-NN-moment.png:
//   01 Touchdown: the panel opened (T), the Landing crate's column
//   02 A Roof over Your Head: the crate opened, the Citadel's blueprint
//   03 First Haul: the Citadel finished   04 The Wreck on the Rise: a load
//      home, the Lander wreck's column ahead
//   05 Liquid Gold: the wreck opened, the Derrick's blueprint, level 3
//   06 Make It Yours: the Derrick on the north well, pumped once
//   07 a point spent on the skill screen  08 the daily three take over
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
// The giant's comet impacts are off meanwhile (ac.GasGiant.Events 0). The clock
// jumps to just before each moment (S1), about 40 s in all.
//
// -AcMmoShotsStarfall (it adds -AcMmoStarfall=10 -AcMmoClockHour=17 unless
// the run gives them: a starfall near the pilot 10 s after it comes, late in
// the afternoon; docs/weather.md "Starfall"): third person on the start pad
// toward the crater's spot, then at the crater; for the night the clock jumps
// on to 19:00 (FAcMmoClient::DevClock), about 40 s in all, DIR/sf-NN-moment.png:
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
// -AcMmoShotsFlight (F1, AcPilotFlight.h): a pad flight from the start pad
// to Cold Fork's, each moment on the flight's own clock, DIR/fl-NN-moment.png
// (-AcMmoHour=12 for noon, 19 for dusk):
//   01 the spool-up on the pad: the jets lit, the dust ring
//   02 the lift-off, from low behind the pad, the giant beyond
//   03 high over the region, heading for the giant
//   04 the descent onto Cold Fork's pad on retro thrust
//   05 the touch down: the dust thrown out     06 down, the pilot's view back
//
// -AcMmoShotsB1 (it adds -AcMmoDevRig: a Citadel to unload at): the drill's
// sparks stopping and the AI's walk (docs/mmorpg-m8.md B1),
// DIR/b1-NN-moment.png. Each stop logs how long sparks lived on after it
// (`mmoshots: b1: ... sparks gone N s after`); the AI's walk logs the drawn
// unit's speed jitter (`mmoshots: b1: AI walk ...`):
//   01 drilling, third person    02 0.3 s after the key let go   03 1 s after
//   04 drilling, first person    05 0.3 s after the key let go   06 1 s after
//   07 0.3 s after walking off the vein, first person, the key still held
//   then handed back: 15 s of the AI mining, top-down, measured, no picture.
//
// -AcMmoShotsC1 (it adds -AcMmoDevRig: the realm's Citadel to unload at
// and a full wallet): the Citadel's tax and the realm wallet
// (docs/mmorpg-m8.md C1), DIR/c1-NN-moment.png:
//   01 a load unloaded at the Citadel: the toast with the realm's cut
//      ("+22 ORE", "3 TO THE REALM"), the realm wallet by the wallet
//   02 the Citadel screen (H): the realm wallet, the tax, what the pilot
//      gave, the donate buttons
//   03 after a donation of 1,000 ore: the realm wallet up, the foot's thanks
//
// -AcMmoShotsOnly=sf-02,sf-08 (names, numbers or a prefix and a number,
// comma-separated): only those moments are shot; the others are passed over
// (logged, counted as skipped), and in the Starfall set their waits too.
// A run that waits on the world's clock moves it at once instead
// (FAcMmoClient::DevClock: the server's --dev-control file, shot runs only).
//
// -AcMmoShotsClip (with -AcShot=PATH -AcShotRecord=S -AcShotRaw=FILE
// -AcShotUI, AcShot.h; the ringshadow-video skill): motion instead of
// pictures. The recorder is held until the first moment -AcMmoShotsOnly
// names; from there it records S seconds of the run as it goes on, every
// later moment played (and its waits), none of them shot, and the recorder
// ends the run. Each moment logs its world time
// (`mmoshots: clip: NAME at world T s`) against the recorder's
// `shot: recording starts at world T s`, to find it in the clip. Leave out
// -UseFixedTimeStep: the server runs on the wall clock, so the client must
// too; the clip's rate is the frames over the recording's wall seconds.
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
#include "AcEffects.h"
#include "AcEffectsBursts.h"
#include "AcParticles.h"

#include "AcGasGiant.h"
#include "AcHudStyle.h"
#include "AcLeveling.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotBuild.h"
#include "AcPilotPawn.h"
#include "AcPilotText.h"
#include "AcProgressHud.h"
#include "AcShot.h"
#include "AcSimSubsystem.h"
#include "AcSites.h"
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
	/// -AcMmoShotsW2: where the staged sites are stood from (ground, the
	/// heading), and the kind shown near.
	ac::Vec2 W2From;
	double W2Yaw = 0;
	int32 W2Kind = 0;
	/// Ground `Ahead` cells along `Yaw` from `From` and `Right` to its right.
	ac::Vec2 W2At(const ac::Vec2 From, const double Yaw, const double Ahead, const double Right)
	{
		return ac::Vec2(From.x + Ahead * std::cos(Yaw) - Right * std::sin(Yaw), From.y + Ahead * std::sin(Yaw) + Right * std::cos(Yaw));
	}

	const ac::Structure* HabDomeOf(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::habDome && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// -AcMmoShotsB1: when the drill was last stopped, when sparks were
	/// last alive after it, and the drawn unit's walk under its AI (wall
	/// seconds; the drawn position and the newest snapshot's).
	double B1StoppedAt = -1;
	double B1SparksAt = -1;
	/// Into first person before drilling again (after the third person's).
	bool B1ToFirst = false;
	/// -AcMmoShotsC1: the realm wallet's ore before the unload, and the
	/// pilot's donations before the gift.
	int64 C1RealmBefore = 0;
	int64 C1DonatedBefore = 0;
	/// The load it brought home.
	int64 C1Load = 0;
	struct FB1Sample
	{
		double At;
		ac::Vec2 Drawn;
		ac::Vec2 Told;
	};
	TArray<FB1Sample> B1Walk;

	/// The AI's walk as drawn: over the frames where the server moved it
	/// steadily (its newest snapshot 0.1 cell on in every 0.3 s, 0.3 s
	/// either side), the speed each frame against the mean of the 0.5 s
	/// around it. Returns the line to log; `Jitter` the rms, a fraction.
	FString B1WalkStats(const TArray<FB1Sample>& W, double& Jitter, int32& Frames, int32& Stalls)
	{
		Jitter = 0;
		Frames = 0;
		Stalls = 0;
		const int32 N = W.Num();
		TArray<bool> Moving;
		Moving.SetNum(N);
		for (int32 I = 0, J = 0; I < N; ++I)
		{
			while (J < I && W[J + 1].At <= W[I].At - 0.3) ++J;
			Moving[I] = W[I].At - W[J].At >= 0.25 && ac::distance(W[I].Told, W[J].Told) > 0.1;
		}
		TArray<double> Speed;
		Speed.SetNum(N);
		for (int32 I = 1; I < N; ++I)
		{
			const double Dt = W[I].At - W[I - 1].At;
			Speed[I] = Dt > 1e-4 ? ac::distance(W[I].Drawn, W[I - 1].Drawn) / Dt : 0.0;
		}
		double Sum = 0, Sq = 0, Worst = 0, Mean = 0;
		for (int32 I = 1; I < N; ++I)
		{
			bool bSteady = true;
			double WSum = 0, WTime = 0;
			for (int32 K = I; K >= 1 && W[K].At >= W[I].At - 0.3 && bSteady; --K) bSteady = Moving[K];
			for (int32 K = I; K < N && W[K].At <= W[I].At + 0.3 && bSteady; ++K) bSteady = Moving[K];
			if (!bSteady) continue;
			for (int32 K = 1; K < N; ++K)
			{
				if (FMath::Abs(W[K].At - W[I].At) > 0.25) continue;
				WSum += ac::distance(W[K].Drawn, W[K - 1].Drawn);
				WTime += W[K].At - W[K - 1].At;
			}
			if (WTime <= 0 || WSum <= 0) continue;
			const double Around = WSum / WTime;
			const double Off = (Speed[I] - Around) / Around;
			Frames += 1;
			Sum += Speed[I];
			Sq += Off * Off;
			Worst = FMath::Max(Worst, FMath::Abs(Off));
			if (Speed[I] < 0.2 * Around) Stalls += 1;
		}
		if (Frames > 0)
		{
			Jitter = FMath::Sqrt(Sq / Frames);
			Mean = Sum / Frames;
		}
		return FString::Printf(TEXT("%d steady frames of %d, %.2f cells/s, speed jitter %.1f%% rms (worst %.0f%%), %d stalled frames"), Frames, N,
			Mean, 100 * Jitter, 100 * Worst, Stalls);
	}

	/// -AcMmoShotsClip: whether the recorder is held for the first moment,
	/// and whether it records (every moment wanted from then on).
	bool ClipHeld = false;
	bool Clipping = false;
	/// The set ended while recording: the recorder ends the run.
	bool ClipEnded = false;

	/// -AcMmoShotsOnly=sf-02,sf-08 (names or numbers, comma-separated): the
	/// moments to shoot; the others are passed over (counted as skipped),
	/// their waits too where the set can. Unset: every moment.
	bool Wanted(const TCHAR* Name)
	{
		FString Only;
		if (Clipping) return true;
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
		// -AcMmoShotsClip: the world goes on until the recorder ends the run.
		if (Clipping)
		{
			ClipEnded = true;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: the set is over; the recorder ends the run"));
			return;
		}
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

	if (ClipEnded) return;
	// -AcMmoShotsB1: after a drill stop, when sparks were last alive (every
	// frame, a picture being written or not).
	if (B1StoppedAt >= 0 && Sim)
	{
		const UAcEffects* Fx = UAcEffects::Get(this);
		const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
		if (P && P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) > 0) B1SparksAt = Now;
	}
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
		if (!Wanted(Name))
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: passed over (-AcMmoShotsOnly)"), Name);
			Shots.Skipped += 1;
			Advance();
			return;
		}
		if (ClipHeld || Clipping)
		{
			UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this);
			if (!Clipping && Shot) Shot->Release();
			Clipping = true;
			ClipHeld = false;
			Shots.Saved += 1;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: %s at world %.3f s: %s"), Name, GetWorld()->GetTimeSeconds(), *What);
			Advance();
			return;
		}
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
		// -AcMmoShotsClip: the recorder waits for the first moment wanted.
		Clipping = false;
		ClipHeld = false;
		ClipEnded = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsClip")) && UAcShotSubsystem::IsRecordRun())
		{
			if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this))
			{
				Shot->Hold();
				ClipHeld = true;
				UE_LOG(LogAutocraft, Display, TEXT("mmoshots: clip: the recorder held for the first moment wanted"));
			}
		}
		// -AcMmoShotsO1: the new pilot earns its levels (no dev XP).
		if (double Xp = 0; FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsO1")) && !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp))
		{
			FCommandLine::Append(TEXT(" -AcMmoDevXp=0"));
		}
		// The new pilot starts at level 3 (1100 XP): the Derrick's floor, so
		// the MH loop (19-22) can build one, unless the run says otherwise.
		if (double Xp = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), Xp)) FCommandLine::Append(TEXT(" -AcMmoDevXp=1100"));
		// -AcMmoShotsG2: the dev rig's home (a Citadel, level 25, a full wallet),
		// for this step only (the flow steps before it in the run go without).
		if ((FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsG2")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW2"))
				|| FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsB1")) || FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1")))
			&& !FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig")))
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
			// -AcMmoShotsW2: the sites on the ground, the compact panel (700-724).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsW2"))) T.Phase = 700;
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsO1"))) T.Phase = 800;
			// -AcMmoShotsRig: only the rig screen (500-513).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsRig"))) T.Phase = 500;
			// -AcMmoShotsFlight: only a pad flight (520-527).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsFlight"))) T.Phase = 520;
			// -AcMmoShotsB1: only the drill's sparks and the AI's walk (900-919).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsB1"))) T.Phase = 900;
			// -AcMmoShotsC1: only the Citadel's tax and the realm wallet (1000-1012).
			if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoShotsC1"))) T.Phase = 1000;
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
		Check(Shots.Saved + Shots.Skipped == 5, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
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
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
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
		// After the last picture: the news no run brings, for their sounds
		// (J1; -AcAudioLog shows blueprint, made and research).
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			using K = ac::net::ProgressMessage::NewsKind;
			for (const auto& [NewsKind, NewsId] : {std::pair{K::blueprint, std::string("habDome")}, std::pair{K::made, std::string("drillHead")},
					 std::pair{K::researched, std::string("drillHead")}})
			{
				ac::net::ProgressMessage::News N;
				N.kind = NewsKind;
				N.id = NewsId;
				N.value = 2;
				Hud->PreviewNews(N);
			}
		}
		Advance();
		return;
	case 630:
		if (Since() < 1.5) return;
		Check(Shots.Saved + Shots.Skipped == 11, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsW2
	case 700:
		// The dev rig's home, the contracts drawn, noon; first person away from the Citadel.
		if (Mmo->Progress() && Mmo->Progress()->contracts.size() == 3 && S && CitadelOf(*S) && Since() > 4.0)
		{
			Mmo->DevClock(TEXT("hour 11"));
			Pilot->ToggleView();
			Stop(Face(U->position * 2.0 - CitadelOf(*S)->position));
			Advance();
		}
		else TimedOut(TEXT("the contracts and the dev rig's Citadel"));
		return;
	case 701:
		if (Since() > 2.5) Shoot(TEXT("w2-01-panel-compact"), TEXT("the contract panel, compact: one line a contract"));
		return;
	case 702:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		Advance();
		return;
	case 703:
		if (Since() > 0.6) Shoot(TEXT("w2-02-panel-expanded"), TEXT("T: the whole contract panel"));
		return;
	case 704:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		// Third person, a little down, for the sites.
		Pilot->ToggleView();
		Pilot->Eye().Pitch = -0.12;
		W2From = U->position;
		W2Yaw = Pilot->Yaw();
		W2Kind = 0;
		Advance();
		return;
	case 705:
		// Kind W2Kind near: shut on the left, opened on the right, facing the pilot.
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			const ac::SiteKind Kind = ac::SiteKind(W2Kind);
			const bool bBig = Kind == ac::SiteKind::vault || Kind == ac::SiteKind::wreck;
			const double Ahead = bBig ? 9.5 : 6.0, Side = bBig ? 4.2 : 2.4;
			Sites->Stage({{Kind, W2At(W2From, W2Yaw, Ahead, -Side), W2From, false, false},
				{Kind, W2At(W2From, W2Yaw, Ahead, Side), W2From, true, false}});
		}
		Stop(W2Yaw);
		Advance();
		return;
	case 706:
		if (Since() > 1.5)
		{
			static const TCHAR* const Names[] = {TEXT("w2-03-crate-near"), TEXT("w2-04-wreck-near"), TEXT("w2-05-cache-near"), TEXT("w2-06-vault-near")};
			Shoot(Names[W2Kind], FString::Printf(TEXT("staged %s: shut (left), opened (right)"), Names[W2Kind] + 6));
			if (++W2Kind < 4) T.Phase = 705;
		}
		return;
	case 707:
		// The start region's own Lander wreck: up to 8 cells off.
		if (AAcSites* Sites = AAcSites::Find(GetWorld())) Sites->Stage({});
		Route({Cell(66, 34), Cell(58, 33)});
		Advance();
		return;
	case 708:
		if (const int32 D = Drive(); D == 1)
		{
			const FVector2D Wreck = Cell(50, 30);
			Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
			if (Since() > 2.0)
			{
				const AAcSites* Sites = AAcSites::Find(GetWorld());
				Check(Sites && Sites->Where(TEXT("start/Lander wreck")).IsSet(), TEXT("the Lander wreck stands in the start region"));
				Check(Mmo->Progress()->sitesKnown.count("start/Lander wreck") > 0, TEXT("the Lander wreck is known"));
				Advance();
			}
		}
		else if (D < 0) Stalled(TEXT("to the Lander wreck's view"));
		return;
	case 709:
		Shoot(TEXT("w2-07-wreck-shut"), TEXT("the Lander wreck, shut and known: its lamp and column"));
		return;
	case 710:
	{
		// Up to its hull, then hold the act key (a 5 s hold opens it).
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
		return;
	}
	case 711:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->opening && P->opening->since > 0 && P->opening->needs > 0)
		{
			const double Share = ((Mmo->Sky() ? Mmo->Sky()->time : 0) - P->opening->since) / P->opening->needs;
			if (Share > 0.4 && Share < 0.85)
			{
				Shoot(TEXT("w2-08-opening-ring"), FString::Printf(TEXT("the opening ring on the wreck, %.0f%%"), Share * 100));
				return;
			}
		}
		TimedOut(TEXT("the wreck opening"), 20);
		return;
	case 712:
		if (Mmo->Progress()->sitesFound.count("start/Lander wreck"))
		{
			Pilot->Press(false);
			Route({Cell(58, 33)});
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened"), 20);
		return;
	case 713:
		if (const int32 D = Drive(); D == 1)
		{
			const FVector2D Wreck = Cell(50, 30);
			Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
			if (Since() > 1.5)
			{
				Shoot(TEXT("w2-09-wreck-opened"), TEXT("the Lander wreck opened: the hatch open, its lights out"));
			}
		}
		else if (D < 0) Stalled(TEXT("back to the wreck's view"));
		return;
	case 714:
	{
		// Four shut, known sites far off toward the east, a fan 18-44 cells away.
		const FVector2D East = Cell(118, 40);
		W2From = U->position;
		W2Yaw = Face(ac::Vec2(East.X, East.Y));
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			TArray<AAcSites::FStaged> Far;
			const double Ahead[] = {18, 26, 34, 44}, Across[] = {-9, 6, -3, 10};
			for (int32 K = 0; K < 4; ++K) Far.Add({ac::SiteKind(K), W2At(W2From, W2Yaw, Ahead[K], Across[K]), W2From, false, true});
			Sites->Stage(Far);
		}
		Stop(W2Yaw);
		Pilot->Eye().Pitch = 0.0;
		Advance();
		return;
	}
	case 715:
		if (Since() > 2.0)
		{
			const AAcSites* Sites = AAcSites::Find(GetWorld());
			Check(Sites && Sites->Columns() >= 4, FString::Printf(TEXT("the four columns (%d)"), Sites ? Sites->Columns() : -1));
			Shoot(TEXT("w2-10-far-day"), TEXT("four shut sites known, 18-44 cells off, by day: their columns"));
		}
		return;
	case 716:
		Mmo->DevClock(TEXT("hour 18"));
		Advance();
		return;
	case 717:
		if (Since() > 3.0) Shoot(TEXT("w2-11-far-dusk"), TEXT("the same at dusk"));
		return;
	case 718:
		// The four shut, near, in a row.
		if (AAcSites* Sites = AAcSites::Find(GetWorld()))
		{
			TArray<AAcSites::FStaged> Row;
			// Left of the line (the dev rig's base stands to the right), the cache nearest.
			const double Across[] = {-4.5, -10.5, 2.0, -17.0}, Ahead[] = {8, 11, 7, 14};
			for (int32 K = 0; K < 4; ++K) Row.Add({ac::SiteKind(K), W2At(W2From, W2Yaw, Ahead[K], Across[K]), W2From, false, true});
			Sites->Stage(Row);
		}
		Pilot->Eye().Pitch = -0.1;
		Advance();
		return;
	case 719:
		if (Since() > 1.6) Shoot(TEXT("w2-12-near-dusk"), TEXT("the four shut, near, at dusk: lamps, the vault's seam"));
		return;
	case 720:
		if (Since() < 1.0) return;
		if (AAcSites* Sites = AAcSites::Find(GetWorld())) Sites->Stage({});
		Check(Shots.Saved + Shots.Skipped == 12, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsO1
	case 800:
		// Step 1 open, the panel whole (T) for the radio's line; toward the crate.
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 1 && Since() > 1.0)
		{
			if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
			const FVector2D Crate = Cell(70, 42);
			Stop(Face(ac::Vec2(Crate.X, Crate.Y)));
			Advance();
		}
		else TimedOut(TEXT("the first ten minutes' step 1"));
		return;
	case 801:
		if (Since() > 1.2) Shoot(TEXT("o1-01-touchdown"), TEXT("step 1, Touchdown: the toast, the gold step (T), the crate's column"));
		return;
	case 802:
		if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this)) Hud->ToggleContracts();
		Route({Cell(70, 42)});
		Advance();
		return;
	case 803:
	{
		// Into the crate: it opens on touch, step 2 opens.
		const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo);
		if (Step && Step->step == 2)
		{
			Stop(PI);
			Advance();
			return;
		}
		if (Drive() < 0) Stalled(TEXT("to the Landing crate"));
		TimedOut(TEXT("the crate opened, step 2"), 40);
		return;
	}
	case 804:
		if (Since() > 1.5) Shoot(TEXT("o1-02-a-roof"), TEXT("step 2: the crate opened, the Citadel's blueprint; the toast and the panel"));
		return;
	case 805:
		// The build spot east of the ore (as 10-16), the Citadel placed.
		Route({Cell(69, 48)});
		Advance();
		return;
	case 806:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Shots.bShotBefore = false;
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the build spot"));
		return;
	case 807:
		// Held out (B, its number), placed a moment later, once its ghost has a spot.
		if (!Shots.bShotBefore && Since() > 1.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::citadel) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::citadel, FString::Printf(TEXT("B, %d held out a Citadel"), Number));
			Shots.bShotBefore = true;
		}
		else if (Shots.bShotBefore && Since() > 2.5)
		{
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 808:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Citadel built, not refused: %s"), *Why));
		}
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 3 && S && CitadelOf(*S))
		{
			Shots.Citadel = CitadelOf(*S)->id;
			Advance();
			return;
		}
		TimedOut(TEXT("the Citadel finished, step 3"), WeldLimit);
		return;
	case 809:
		if (Since() > 1.5) Shoot(TEXT("o1-03-first-haul"), TEXT("step 3: the Citadel finished; the toast, the tip line"));
		return;
	case 810:
	{
		// To the nearest ore round the Citadel's north side (as 19).
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
	case 811:
	{
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Shots.Route.Add(FVector2D(P.x, P.y));
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 812:
		if (U && U->carrying > 0)
		{
			Pilot->Press(false);
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 30);
		return;
	case 813:
		// Home: unloaded, step 4 open; then toward the wreck on the rise.
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 4 && U && U->carrying == 0)
		{
			Advance();
			return;
		}
		Drive();
		TimedOut(TEXT("the load home, step 4"), 30);
		return;
	case 814:
	{
		// Off the Citadel's wall a few steps toward the wreck, then the picture.
		const FVector2D Wreck = Cell(50, 30);
		if (Since() < 1.6)
		{
			if (Pilot) Pilot->SetTestInput(FVector2D(0, -0.8), Face(ac::Vec2(Wreck.X, Wreck.Y)));
			return;
		}
		Stop(Face(ac::Vec2(Wreck.X, Wreck.Y)));
		if (Since() > 2.1) Shoot(TEXT("o1-04-the-wreck"), TEXT("step 4: a load home; the toast, the Lander wreck's column ahead"));
		return;
	}
	case 815:
	{
		// Over to the wreck (as 707-710), then hold the act key on it.
		const ac::Structure* C = CitadelOf(*S);
		Route({FVector2D(C->position.x + 5, C->position.y - 6), Cell(66, 34), Cell(58, 33)});
		Advance();
		return;
	}
	case 816:
		if (const int32 D = Drive(); D == 1) Advance();
		else if (D < 0) Stalled(TEXT("to the Lander wreck"));
		return;
	case 817:
	{
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
		return;
	}
	case 818:
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 5)
		{
			Pilot->Press(false);
			const int64 Level = ac::Progression::level(Mmo->Progress()->xp);
			Check(Level >= ac::Progression::blueprintFloor(ac::StructureKind::derrick),
				FString::Printf(TEXT("at the Derrick's floor by step 5: level %lld"), (long long)Level));
			// Back a little, to see the opened wreck.
			Pilot->SetTestInput(FVector2D(0, 0.6), Pilot->Yaw());
			Advance();
			return;
		}
		TimedOut(TEXT("the wreck opened, step 5"), 15);
		return;
	case 819:
		if (Since() > 0.7) Stop(Pilot->Yaw());
		if (Since() > 1.8) Shoot(TEXT("o1-05-liquid-gold"), TEXT("step 5: the wreck opened, the Derrick's blueprint; the toast, the panel"));
		return;
	case 820:
		// East of the north well (60, 36), facing it: a Derrick's reach away.
		Route({Cell(58, 33), Cell(63, 33), Cell(63, 36)});
		Advance();
		return;
	case 821:
		if (const int32 D = Drive(); D == 1)
		{
			Stop(PI);
			Shots.bShotBefore = false;
			Advance();
		}
		else if (D < 0) Stalled(TEXT("to the north well"));
		return;
	case 822:
		if (!Shots.bShotBefore && Since() > 1.0)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			const int32 Number = AcPilotText::BuildKinds(true).IndexOfByKey(ac::StructureKind::derrick) + 1;
			Check(Number > 0 && Build && Build->Key(TEXT("b")) && Build->Key(FString::FromInt(Number)) && Build->Placing().IsSet()
				&& *Build->Placing() == ac::StructureKind::derrick, FString::Printf(TEXT("B, %d held out a Derrick"), Number));
			Shots.bShotBefore = true;
		}
		else if (Shots.bShotBefore && Since() > 2.5)
		{
			UAcPilotBuildSubsystem* Build = UAcPilotBuildSubsystem::Get(this);
			Check(Build && Build->Spot(ac::StructureKind::derrick).IsSet(), TEXT("the Derrick's ghost on the north well"));
			Pilot->Press(true);
			Pilot->Press(false);
			Advance();
		}
		return;
	case 823:
		if (Mmo)
		{
			for (const FString& Why : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("Derrick built, not refused: %s"), *Why));
		}
		for (const ac::Structure& B : S->structures)
		{
			if (B.kind != ac::StructureKind::derrick || B.owner != ac::Pilot::player || !B.complete()) continue;
			Shots.Derrick = B.id;
			Advance();
			return;
		}
		TimedOut(TEXT("the Derrick finished"), WeldLimit);
		return;
	case 824:
	{
		// Into it (a click), pumping until step 6 opens.
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
	case 825:
		if (const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo); Step && Step->step == 6 && U && U->task != ac::Unit::Task::inDerrick)
		{
			if (const std::optional<ac::Structure> D = S->structure(Shots.Derrick)) Stop(Face(D->position));
			Advance();
			return;
		}
		TimedOut(TEXT("pumped once, step 6"), 25);
		return;
	case 826:
		if (Since() > 1.5) Shoot(TEXT("o1-06-make-it-yours"), TEXT("step 6: the Derrick pumped once; the toast, the panel"));
		return;
	case 827:
		Check(OpenSkillScreen() && SkillScreenWidget().IsValid(), TEXT("K opens the skill screen"));
		P5AskedAt = 0;
		Advance();
		return;
	case 828:
	{
		// A point in Quick drill, asked each second until the server has it.
		const TSharedPtr<SAcSkillScreen> W = SkillScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->ranks.count(ac::Skill::quickDrill))
		{
			Advance();
			return;
		}
		if (W && Now - P5AskedAt > 1.0)
		{
			W->PickSkill(ac::Skill::quickDrill);
			W->Learn();
			P5AskedAt = Now;
		}
		TimedOut(TEXT("a point spent"), 15);
		return;
	}
	case 829:
		if (Since() > 1.0) Shoot(TEXT("o1-07-point-spent"), TEXT("a point spent on the skill screen: the chain's last step"));
		return;
	case 830:
		CloseSkillScreen();
		Advance();
		return;
	case 831:
	{
		// The chain over: the daily three, the panel whole a while.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (P && P->contracts.size() == 3 && !AcProgressHud::FirstStep(*Mmo) && Since() > 1.5)
		{
			bool bDaily = true;
			for (const ac::net::Contract& C : P->contracts) bDaily &= C.step == 0;
			Check(bDaily, TEXT("the daily three after the chain"));
			const int64 Level = ac::Progression::level(P->xp);
			Check(Level >= 3, FString::Printf(TEXT("level %lld after the first ten minutes (%.0f XP)"), (long long)Level, P->xp));
			Shoot(TEXT("o1-08-dailies"), FString::Printf(TEXT("the daily three take over, level %lld"), (long long)Level));
			return;
		}
		TimedOut(TEXT("the daily contracts"), 15);
		return;
	}
	case 832:
		if (Since() < 1.0) return;
		Check(Shots.Saved + Shots.Skipped == 8, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
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
			// S1: the clock on to the radio's minute, not a wait for it.
			static bool bAhead = false;
			if (!bAhead && In > ac::Ringshadow::warning - 2.0 && Since() > 0.5 && Mmo)
			{
				bAhead = Mmo->DevClock(FString::Printf(TEXT("ahead %.2f"), In - (ac::Ringshadow::warning - 3.0)));
				T.PhaseAt = Now;
			}
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
		// S1: the clock on to 1.2 s before the phase's moment (the band
		// where it wants it), once a phase, instead of waiting for it.
		static int32 JumpedFor = -1;
		auto JumpTo = [&](TFunctionRef<bool(const ac::Ringshadow::Band&, double At)> Moment) {
			if (JumpedFor == T.Phase || !Sky || !M || !U || !Mmo) return;
			JumpedFor = T.Phase;
			for (double Tm = Sky->Time; Tm < Sky->Time + 600.0; Tm += 0.1)
			{
				const std::optional<ac::Ringshadow::Band> Bt = ac::Ringshadow::band(Tm, M->simulation().map->bounds, Sky->DayLength);
				if (!Bt || !Moment(*Bt, ac::dot(U->position, Bt->dir))) continue;
				if (Tm - 1.2 > Sky->Time + 2.0 && Mmo->DevClock(FString::Printf(TEXT("clock %.2f"), Tm - 1.2))) T.PhaseAt = Now;
				return;
			}
		};
		if (T.Phase == 302 && Shots.Saved + Shots.Skipped < 2)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return At - Bt.lead > 20 && At - Bt.lead < 30; });
		if (T.Phase == 304)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return At - Bt.lead < 1.5 && At - Bt.lead > -8; });
		if (T.Phase == 306)
			JumpTo([&](const ac::Ringshadow::Band& Bt, double) { return ac::Ringshadow::cover(Bt, U->position) > 0.99; });
		if (T.Phase == 312)
			JumpTo([](const ac::Ringshadow::Band& Bt, const double At) { return Bt.trail - At > 6 && Bt.trail - At < 20; });
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
				const bool bHigh = Shots.Saved + Shots.Skipped >= 2;
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
		Check(Shots.Saved + Shots.Skipped == 9, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		if (IConsoleVariable* Boom = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Pilot.ShotBoom"))) Boom->Set(1.f);
		if (IConsoleVariable* Events = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.GasGiant.Events"))) Events->Set(1);
		Finish();
		return;
	// MARK: -AcMmoShotsFlight
	// A pad flight to Cold Fork, its moments on the flight's clock.
	case 520:
		if (Since() > 2.0)
		{
			FString Why;
			Check(Mmo->Fly(TEXT("cold-fork"), TEXT("landing"), &Why), FString::Printf(TEXT("the flight to Cold Fork asked for (%s, %s)"),
				Why.IsEmpty() ? TEXT("ok") : *Why, *Where()));
			if (!Why.IsEmpty()) Finish();
			else Advance();
		}
		return;
	case 521:
		if (Pilot->FlightSeconds() >= 0.6) Shoot(TEXT("fl-01-spool"), TEXT("the spool-up: the jets lit on the pad, the dust ring"));
		else if (!Mmo->FlightRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("flight refused: %s"), *Mmo->FlightRefusal()));
			Finish();
		}
		else TimedOut(TEXT("the lift-off"), 10);
		return;
	case 522:
		if (Pilot->FlightSeconds() >= 1.5) Shoot(TEXT("fl-02-liftoff"), TEXT("the lift-off from low behind the pad, the giant beyond"));
		else TimedOut(TEXT("the lift-off's climb"), 10);
		return;
	case 523:
		if (Pilot->FlightSeconds() >= 2.95 || Pilot->FlightStage() == EAcFlightStage::Landing)
		{
			Check(Pilot->FlightStage() == EAcFlightStage::Transit, TEXT("in transit for the high picture"));
			Shoot(TEXT("fl-03-cruise"), TEXT("high over the region, heading for the giant"));
		}
		else TimedOut(TEXT("the transit"), 10);
		return;
	case 524:
		if (Pilot->LandingSeconds() >= 0.55) Shoot(TEXT("fl-04-descent"), TEXT("the descent onto Cold Fork's pad, retro thrust"));
		else TimedOut(TEXT("the landing"), 20);
		return;
	case 525:
		if (Pilot->LandingSeconds() >= 1.3 || !Pilot->InFlight()) Shoot(TEXT("fl-05-touchdown"), TEXT("the touch down: dust thrown out"));
		else TimedOut(TEXT("the touch down"), 10);
		return;
	case 526:
		if (Seated() && !Pilot->InFlight() && Since() > 0.6)
		{
			Check(Here == TEXT("cold-fork"), FString::Printf(TEXT("landed %s"), *Where()));
			Shoot(TEXT("fl-06-landed"), TEXT("down on Cold Fork's pad, the pilot's view back"));
		}
		else TimedOut(TEXT("down on Cold Fork's pad"), 15);
		return;
	case 527:
		Check(Shots.Saved + Shots.Skipped == 6, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
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
		Check(Shots.Saved + Shots.Skipped == 6, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
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
		// The streak in frame wherever it comes from: its head in the left
		// third (the contracts panel covers the right, the radio's toast the
		// middle), looking up half its height.
		auto AimAtStreak = [&] {
			const ac::Vec3 H = ac::Starfall::head(FMath::Clamp(In, 0.3, ac::Starfall::streak), F->from);
			const ac::Vec2 Head = F->at + ac::Vec2(H.x, H.z);
			Stop(Face(Head) + 0.45);
			Pilot->Eye().Pitch = FMath::Clamp(0.5 * std::atan2(H.y, FMath::Max(ac::distance(Head, U->position), 1.0)), 0.04, 0.35);
		};
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
			AimAtStreak();
			if (Since() > 1.5 || !Wanted(TEXT("sf-01-told")))
				Shoot(TEXT("sf-01-told"), FString::Printf(TEXT("told: the radio's toast, impact in %.0f s"), In));
			return;
		case 402:
			// None of the fall's own moments wanted: the clock on past the
			// impact and the dust (S1), straight to the crater.
			if (!Wanted(TEXT("sf-02-streak-high")) && !Wanted(TEXT("sf-03-streak-low")) && !Wanted(TEXT("sf-04-impact-flash"))
				&& !Wanted(TEXT("sf-05-dust-ring")))
			{
				if (In > 0 && Mmo->DevClock(FString::Printf(TEXT("ahead %.2f"), In + 12.0)))
				{
					Shots.Skipped += 4;
					T.Phase = 407;
					T.PhaseAt = Now;
				}
				else TimedOut(TEXT("the clock moved past the impact"), 10);
				return;
			}
			AimAtStreak();
			if (In <= 5.0) Shoot(TEXT("sf-02-streak-high"), FString::Printf(TEXT("the streak in the sky, %.1f s to go"), In));
			else TimedOut(TEXT("the streak"), 90);
			return;
		case 403:
			AimAtStreak();
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
			if ((-In >= 14.0 && Now - Halted > 1.5) || !Wanted(TEXT("sf-06-crater-day")))
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
			if (Since() > 1.5 || !Wanted(TEXT("sf-07-crater-day-first"))) Shoot(TEXT("sf-07-crater-day-first"), TEXT("the crater by day, first person"));
			return;
		case 410:
			// Night: back to third person, the clock on to 19:00 at once (S1:
			// it used to wait three minutes for the dark), or to the end when
			// no night moment is wanted.
			if (!Wanted(TEXT("sf-08-crater-night")) && !Wanted(TEXT("sf-09-crater-night-first")))
			{
				Shots.Skipped += 2;
				T.Phase = 414;
				return;
			}
			Pilot->ToggleView();
			if (Dark < 0.97) Mmo->DevClock(TEXT("hour 19"));
			Advance();
			return;
		case 411:
			// A few seconds for the sky to take the jump and the eye to adapt.
			Pilot->Eye().Pitch = -0.06;
			if ((Dark > 0.97 && Since() > 3.0) || !Wanted(TEXT("sf-08-crater-night"))) Shoot(TEXT("sf-08-crater-night"), FString::Printf(TEXT("the crater at night (hour %.1f)"), Sky->hour()));
			else TimedOut(TEXT("night (the clock moved on to 19:00)"), 30);
			return;
		case 412:
			Pilot->ToggleView();
			Pilot->Eye().Pitch = -0.1;
			Advance();
			return;
		case 413:
			if (Since() > 1.5 || !Wanted(TEXT("sf-09-crater-night-first"))) Shoot(TEXT("sf-09-crater-night-first"), TEXT("the crater at night, first person"));
			return;
		default:
			Check(Shots.Saved + Shots.Skipped == 9, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
			Finish();
			return;
		}
	}
	// MARK: -AcMmoShotsB1
	case 900:
	{
		// The dev rig's home; the nearest ore with some left.
		if (!(S && CitadelOf(*S) && Since() > 3.0))
		{
			TimedOut(TEXT("the dev rig's Citadel"));
			return;
		}
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
		Route({FVector2D(S->patches[Best].position.x, S->patches[Best].position.y)});
		Advance();
		return;
	}
	case 901:
	case 906:
	case 911:
	{
		// Up to the vein until the act key would drill it, then hold it.
		if (B1ToFirst)
		{
			B1ToFirst = false;
			Pilot->ToggleView();
		}
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Route({FVector2D(P.x, P.y)});
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 902:
	case 907:
	case 912:
	{
		auto Sparks = [&] {
			const UAcEffects* Fx = UAcEffects::Get(this);
			const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
			return P && Sim ? P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) : -1;
		};
		// Drilling a moment: the sparks fly.
		if (!(U && U->task == ac::Unit::Task::mining && Since() > 1.5))
		{
			TimedOut(TEXT("drilling"), 15);
			return;
		}
		const int32 Alive = Sparks();
		Check(Alive > 0, FString::Printf(TEXT("b1: sparks fly while drilling (%d alive)"), Alive));
		if (T.Phase == 912)
		{
			// Walked off the vein, the key still held: that stops the drill.
			Stop(Face(S->patches[Shots.Patch].position));
			Pilot->SetTestInput(FVector2D(0, 1), Pilot->Yaw());
			B1StoppedAt = Now;
			B1SparksAt = Now;
			T.Phase = 915;
			T.PhaseAt = Now;
			return;
		}
		Shoot(T.Phase == 902 ? TEXT("b1-01-drilling-third") : TEXT("b1-04-drilling-first"),
			FString::Printf(TEXT("drilling, %d sparks alive"), Alive));
		return;
	}
	case 903:
	case 908:
		// The key let go.
		Pilot->Press(false);
		B1StoppedAt = Now;
		B1SparksAt = Now;
		Advance();
		return;
	case 904:
	case 905:
	case 909:
	case 910:
	case 915:
	case 916:
	{
		const UAcEffects* Fx = UAcEffects::Get(this);
		const FAcParticles* P = Fx ? AcEffectsBursts::ParticlesOf(Fx) : nullptr;
		const int32 Alive = P && Sim ? P->Alive(EAcParticle::WeldSparks, Sim->LastFrame().Time) : -1;
		const bool bFirst = T.Phase >= 909;
		const bool bLate = T.Phase == 905 || T.Phase == 910 || T.Phase == 916;
		const double At = bLate ? 1.0 : 0.3;
		if (T.Phase == 915 && Now - B1StoppedAt > 0.4) Stop(Pilot->Yaw());
		if (Now - B1StoppedAt < At) return;
		const TCHAR* What = T.Phase == 904 ? TEXT("b1-02-stopped-third") : T.Phase == 905 ? TEXT("b1-03-stopped-third-1s")
			: T.Phase == 909 ? TEXT("b1-05-stopped-first") : T.Phase == 910 ? TEXT("b1-06-stopped-first-1s")
			: T.Phase == 915 ? TEXT("b1-07-walked-off-first") : nullptr;
		const TCHAR* How = T.Phase >= 915 ? TEXT("walked off the vein") : TEXT("let go of the key");
		if (bLate)
		{
			// Sparks die in 0.35-0.45 s (WeldSparks' life), after the server has the stop: gone by 0.7 s.
			const double Gone = B1SparksAt - B1StoppedAt;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: b1: %s, %s: sparks gone %.2f s after (task %d, paused clock %.1f s)"),
				bFirst ? TEXT("first person") : TEXT("third person"), How, Gone, U ? int32(U->task) : -1, Sim ? Sim->Clock() - Sim->GameTime() : -1.0);
			Check(Gone < 0.7, FString::Printf(TEXT("b1: %s, %s: the sparks stopped within 0.7 s (%.2f s)"),
				bFirst ? TEXT("first person") : TEXT("third person"), How, Gone));
			if (T.Phase == 916)
			{
				Pilot->Press(false);
				Stop(Pilot->Yaw());
				Advance();
				return;
			}
			// First person next (906), after the picture.
			if (T.Phase == 905) B1ToFirst = true;
		}
		if (T.Phase == 915)
		{
			Shoot(What, FString::Printf(TEXT("%s, 0.3 s on: %d sparks alive"), How, Alive));
			T.Phase = 916;
			return;
		}
		Shoot(What, FString::Printf(TEXT("%s, %.1f s on: %d sparks alive"), How, At, Alive));
		return;
	}
	case 917:
		// Handed back: its AI drives it (top-down), measured, no picture.
		if (Since() < 1.0) return;
		Pilot->Leave();
		B1StoppedAt = -1;
		B1Walk.Reset();
		Advance();
		return;
	case 918:
	{
		const ac::Unit* Told = M ? M->unit(Shots.Unit) : nullptr;
		const ac::Unit* Drawn = nullptr;
		if (Sim)
			for (const ac::Unit& V : Sim->State().units)
				if (V.id == Shots.Unit) Drawn = &V;
		if (Drawn && Told && !Mmo->Driven()) B1Walk.Add({Now, Drawn->position, Told->position});
		if (Since() < 16.0) return;
		double Jitter = 0;
		int32 Frames = 0, Stalls = 0;
		const FString Line = B1WalkStats(B1Walk, Jitter, Frames, Stalls);
		UE_LOG(LogAutocraft, Display, TEXT("mmoshots: b1: AI walk: %s"), *Line);
		Check(Frames >= 60, FString::Printf(TEXT("b1: the AI walked it (%d steady frames)"), Frames));
		Check(Stalls == 0 && Jitter < 0.15, FString::Printf(TEXT("b1: the AI's walk drawn smoothly (%s)"), *Line));
		Advance();
		return;
	}
	case 919:
		Check(Shots.Saved + Shots.Skipped == 7, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	// MARK: -AcMmoShotsC1
	case 1000:
	{
		// The dev rig's home (the realm's Citadel); the nearest ore with some left.
		if (!(S && CitadelOf(*S) && Mmo->Progress() && Since() > 3.0))
		{
			TimedOut(TEXT("the dev rig's Citadel"));
			return;
		}
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
		Check(Mmo->Progress()->taxPercent == ac::Progression::realmTaxPercent,
			FString::Printf(TEXT("c1: the tax told: %lld%%"), (long long)Mmo->Progress()->taxPercent));
		Shots.Patch = Best;
		Route({FVector2D(S->patches[Best].position.x, S->patches[Best].position.y)});
		Advance();
		return;
	}
	case 1001:
	{
		// Up to the vein until the act key would drill it, then hold it.
		const ac::Vec2 P = S->patches[Shots.Patch].position;
		const std::optional<ac::PilotTarget> Target = U ? M->simulation().pilotTarget(*U) : std::nullopt;
		if (Target && Target->is<ac::PilotTarget::Patch>())
		{
			Stop(Face(P));
			Pilot->Press(true);
			Advance();
			return;
		}
		if (Drive() == 1) Route({FVector2D(P.x, P.y)});
		if (Now - Shots.StallSince > 4.0) Stalled(TEXT("to the ore"));
		return;
	}
	case 1002:
		// A load: home to the Citadel with it.
		if (U && U->carrying > 0 && U->task != ac::Unit::Task::mining)
		{
			Pilot->Press(false);
			Shots.OreBefore = Ore;
			C1Load = U->carrying;
			C1RealmBefore = Mmo->Progress()->realmWallet.ore;
			const ac::Structure* C = CitadelOf(*S);
			Route({FVector2D(C->position.x, C->position.y)});
			Advance();
			return;
		}
		TimedOut(TEXT("a load drilled"), 45);
		return;
	case 1003:
	{
		const ac::Structure* C = CitadelOf(*S);
		if (U && U->carrying == 0)
		{
			Stop(Face(C->position));
			// The toast: up at once, held 1.2 s.
			if (Since() > 0.35)
			{
				const int64 Tax = ac::Progression::realmTax(C1Load);
				Check(Ore == Shots.OreBefore + C1Load - Tax, FString::Printf(TEXT("c1: the wallet got the load less the tax: %lld -> %lld (load %lld, tax %lld)"),
					(long long)Shots.OreBefore, (long long)Ore, (long long)C1Load, (long long)Tax));
				Shoot(TEXT("c1-01-unload-tax"), FString::Printf(TEXT("unloaded %lld ore: +%lld, %lld to the realm"), (long long)C1Load,
					(long long)(C1Load - Tax), (long long)Tax));
			}
			return;
		}
		Drive();
		TimedOut(TEXT("unloaded at the Citadel"), 40);
		return;
	}
	case 1004:
		// The server's realm wallet has the cut.
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->realmWallet.ore > C1RealmBefore)
		{
			Check(P->realmWallet.ore - C1RealmBefore == ac::Progression::realmTax(C1Load),
				FString::Printf(TEXT("c1: the realm wallet got the cut: %lld -> %lld ore"), (long long)C1RealmBefore, (long long)P->realmWallet.ore));
			Check(P->taxed.ore >= ac::Progression::realmTax(C1Load), FString::Printf(TEXT("c1: the pilot's tax counted: %lld ore"), (long long)P->taxed.ore));
			Advance();
			return;
		}
		TimedOut(TEXT("the realm wallet's cut"), 10);
		return;
	case 1005:
		Check(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("c1: H opens the Citadel screen at the Citadel"));
		Check(CitadelScreenWidget().IsValid() && CitadelScreenWidget()->GetInfo().DonateWhyNot.IsEmpty(), TEXT("c1: the screen has the pilot at the Citadel"));
		Advance();
		return;
	case 1006:
		if (Since() > 1.5) Shoot(TEXT("c1-02-citadel-screen"), TEXT("the Citadel screen: the realm wallet, the tax, the donate buttons"));
		return;
	case 1007:
	{
		// 1,000 ore given: the screen's own ask (key 2).
		const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!W || !P)
		{
			Check(false, TEXT("c1: the Citadel screen stays open"));
			Finish();
			return;
		}
		C1RealmBefore = P->realmWallet.ore;
		C1DonatedBefore = P->donated.ore;
		Shots.OreBefore = Ore;
		Check(W->Key(EKeys::Two), TEXT("c1: key 2 gives 1,000 ore"));
		Advance();
		return;
	}
	case 1008:
		if (const ac::net::ProgressMessage* P = Mmo->Progress(); P && P->donated.ore >= C1DonatedBefore + 1000 && Since() > 1.0)
		{
			Check(P->realmWallet.ore == C1RealmBefore + 1000, FString::Printf(TEXT("c1: the realm wallet took the gift: %lld -> %lld ore"),
				(long long)C1RealmBefore, (long long)P->realmWallet.ore));
			Check(Ore == Shots.OreBefore - 1000, FString::Printf(TEXT("c1: the wallet gave it: %lld -> %lld ore"), (long long)Shots.OreBefore, (long long)Ore));
			Check(CitadelScreenWidget() && !CitadelScreenWidget()->KeptText().IsEmpty(), TEXT("c1: the foot thanks the pilot"));
			Shoot(TEXT("c1-03-donated"), TEXT("after a donation of 1,000 ore: the realm wallet up"));
			return;
		}
		if (!Mmo->CitadelRefusal().IsEmpty())
		{
			Check(false, FString::Printf(TEXT("c1: donate: %s"), *Mmo->CitadelRefusal()));
			Finish();
			return;
		}
		TimedOut(TEXT("the donation kept"), 10);
		return;
	case 1009:
		CloseCitadelScreen();
		Check(Shots.Saved + Shots.Skipped == 3, FString::Printf(TEXT("%d pictures (%d passed over) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	default:
		Check(Shots.Saved + Shots.Skipped == 22, FString::Printf(TEXT("%d pictures (%d skipped) in %s"), Shots.Saved, Shots.Skipped, *Shots.Dir));
		Finish();
		return;
	}
}
