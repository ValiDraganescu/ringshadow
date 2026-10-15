// -AcFlowTest=mmotwo (W5, docs/mmorpg-play.md "Two pilots"): two humans on
// one world. This game is one of them (-AcMmoTwo=A or B); the other is a
// second game process beside it, on the same Mac, started by
// unreal/Tools/two_clients.py under the one hidden-run slot. A starts the
// world server and names it in a share file (-AcMmoShare=FILE); B joins it
// (-AcMmoJoin=FILE), through the lag simulator. Each has its own save
// folder and pilot key. They keep in step through marks in a folder of
// their own (-AcMmoTwoSync=DIR: a file per mark, `A.seated`, `B.hidden`).
// With -AcMmoShots=DIR -AcMmoShotsW5 each takes its pictures,
// DIR/w5-NN-{a,b}-moment.png; without (headless) only what needs no HUD
// is checked.
//
//   1 each sees the other's Prospector, tagged with its callsign ("PILOT
//     2B3C"); A backs off, B drives behind the realm's Citadel: from A's
//     view no tag (TagSight's rules)
//   2 A lights the Beacon at the Citadel (`defence wallet` fills the realm
//     wallet): RUNNING on B's REALM tab, B's fare to Cold Fork halved
//   3 A builds a Hab Dome: B sees it rise, the deed's toast "PILOT 443E
//     BUILT A HAB DOME", its dot and tooltip on the world map (built by);
//     B drills a load and unloads at it: B's gain with the realm's tax,
//     the realm wallet up on A's HUD
//   4 A flies the Dropship from a Landing Pad by the Citadel to Tern Ridge:
//     B on the ground sees the ship come down, A's Prospector walk aboard,
//     the lift and the way out, A's tag gone while aboard
//   5 both at The Fork (`brood zone here`; B's line `as 2 …`), wave 1:
//     both strike its bugs, the wave beaten, each paid its share (the
//     toast, the wallet)
//   6 wave 2 out, B leaves (its game ends: Stop disconnects it); A drives
//     on undisturbed, B's tag gone; B's game again (-AcMmoTwoBack): back in
//     Cold Fork where it left, its wallet as it was, A sees its tag again
#include "AcGameFlow.h"

#include "AcHUD.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotFlight.h"
#include "AcPilotPawn.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcWorldRenderer.h"
#include "SAcCitadelScreen.h"
#include "SAcWorldMap.h"

#include "Flight.h"
#include "Progression.h"
#include "RealmProjects.h"
#include "Rules.h"
#include "Simulation.h"
#include "WorldAtlas.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

#include <cmath>

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	constexpr double PhaseLimit = 60;
	constexpr double WeldLimit = 90;

	struct FTwo
	{
		FString Role;
		FString Other;
		FString Sync;
		bool bBack = false;
		FString OtherCallsign;
		int64 Citadel = -1;
		int64 Dome = -1;
		/// Driving: the point it heads for, and the stall check.
		TOptional<ac::Vec2> To;
		ac::Vec2 StallAt;
		double StallSince = 0;
		int64 RealmOre = -1;
		int64 Ore = -1;
		int64 Hydrogen = -1;
		double Struck = 0;
		double StillAt = -1;
		double QuitSeen = 0;
		double DoneAt = -1;
		double FightLog = 0;
		double DescendSeen = -1;
		double ToastSeen = -1;
		double GoneAt = -1;
		ac::Vec2 ForkAt{0, 0};
		double LastNow = 0;
		bool bSent = false;
		bool bShotStage[8] = {};
		bool bTagWhileAboard = false;
		bool bStagesInOrder = true;
		uint8 LastStage = 0;
		int32 Saved = 0;
		FString Pending;
		int64 LastSize = -1;
		int32 Waited = 0;
		int64 TickWas = -1;
		double TickSince = 0;
		bool bOtherGone = false;
		ac::Vec2 Left;
		FString LeftRegion;
		int64 LeftOre = -1;
		int64 LeftHydrogen = -1;
	};
	FTwo Two;

	FString MarkPath(const FString& Role, const FString& Name) { return FPaths::Combine(Two.Sync, Role + TEXT(".") + Name); }
	void SyncMark(const FString& Name, const FString& Body = FString())
	{
		FFileHelper::SaveStringToFile(Body, *MarkPath(Two.Role, Name));
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo %s: mark %s %s"), *Two.Role, *Name, *Body);
	}
	bool SyncSeen(const FString& Name, FString* Body = nullptr)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *MarkPath(Two.Other, Name))) return false;
		if (Body) *Body = Text;
		return true;
	}

	const ac::Structure* OfKind(const ac::GameState& S, const ac::StructureKind K, const int64 Id = -1)
	{
		for (const ac::Structure& B : S.structures)
			if (B.kind == K && B.owner == ac::Pilot::player && (Id < 0 || B.id == Id)) return &B;
		return nullptr;
	}
}

void UAcGameFlowSubsystem::TickMmoTwo()
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] { T.Phase += 1; T.PhaseAt = Now; Two.To.Reset(); Two.bSent = false; };
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::GameState* S = M ? &M->simulation().state : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	const FString Here = Mmo && Mmo->Here() ? Utf(Mmo->Here()->id) : FString(TEXT("-"));
	const int64 Ore = S && !S->players.empty() ? S->players[0].ore : -1;
	const int64 Hydrogen = S && !S->players.empty() ? S->players[0].hydrogen : -1;
	const bool bA = Two.Role == TEXT("A");
	const bool bPictures = !Shots.Dir.IsEmpty();
	const TCHAR* R = bA ? TEXT("a") : TEXT("b");
	const double Dt = Two.LastNow > 0 ? FMath::Min(Now - Two.LastNow, 0.2) : 0.0;
	Two.LastNow = Now;

	auto Stop = [&](const double Yaw) {
		if (Pilot) Pilot->SetTestInput(FVector2D::ZeroVector, Yaw);
	};
	auto Face = [&](const ac::Vec2 At) { return U ? std::atan2(At.y - U->position.y, At.x - U->position.x) : 0.0; };
	auto Finish = [&] {
		if (Pilot)
		{
			Pilot->Press(false);
			Stop(Pilot->Yaw());
		}
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo %s: %d pictures in %s"), *Two.Role, Two.Saved, *Shots.Dir);
		StopMmo();
		NextStep();
	};
	auto Where = [&] {
		return U ? FString::Printf(TEXT("at %.1f,%.1f in %s, %lld ore, %lld MH"), U->position.x, U->position.y, *Here, (long long)Ore, (long long)Hydrogen)
				 : FString::Printf(TEXT("no unit, in %s"), *Here);
	};
	auto Say = [&](const bool bOk, const FString& What) { Check(bOk, FString::Printf(TEXT("mmotwo %s: %s"), *Two.Role, *What)); };
	auto TimedOut = [&](const TCHAR* What, const double Limit = PhaseLimit) {
		if (Since() <= Limit) return false;
		Say(false, FString::Printf(TEXT("%s within %.0f s (%s; %s)"), What, Limit, *Where(), Mmo ? *Mmo->Message() : TEXT("no client")));
		SyncMark(TEXT("failed"));
		Finish();
		return true;
	};
	auto Seated = [&] {
		return Mmo && Pilot && Mmo->Driven() && !Mmo->Travelling() && Pilot->Driving() && Pilot->Driven() == *Mmo->Driven()
			&& PC && PC->GetPawn() == Pilot && U;
	};
	// The other game gave up: so does this one.
	if (T.Phase > 1 && SyncSeen(TEXT("failed")))
	{
		Say(false, TEXT("the other game failed"));
		SyncMark(TEXT("failed"));
		Finish();
		return;
	}
	// A picture asked for: wait until it is written.
	if (!Two.Pending.IsEmpty())
	{
		++Two.Waited;
		const int64 Size = IFileManager::Get().FileSize(*Two.Pending);
		if (Size > 0 && Size == Two.LastSize)
		{
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: saved %s (%lld bytes; %s)"), *FPaths::GetCleanFilename(Two.Pending), (long long)Size, *Where());
			Two.Pending.Empty();
			Two.Saved += 1;
			T.PhaseAt = Now;
			return;
		}
		Two.LastSize = Size;
		if (Two.Waited > 600)
		{
			Say(false, FString::Printf(TEXT("%s written"), *Two.Pending));
			Two.Pending.Empty();
		}
		return;
	}
	// A picture with the HUD (pictures only; the phase moves on either way).
	auto Shoot = [&](const FString& Name, const FString& What) {
		if (bPictures)
		{
			Two.Pending = FPaths::Combine(Shots.Dir, Name + TEXT(".png"));
			IFileManager::Get().Delete(*Two.Pending, false, true, true);
			Two.LastSize = -1;
			Two.Waited = 0;
			UE_LOG(LogAutocraft, Display, TEXT("mmoshots: %s: %s"), *Name, *What);
			FScreenshotRequest::RequestScreenshot(Two.Pending, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		}
		Advance();
	};
	// Straight at `At` until within `Near` cells: 1 there, 0 on the way, -1 stalled.
	auto DriveTo = [&](const ac::Vec2 At, const double Near) -> int32 {
		if (!U || !Pilot) return 0;
		if (!Two.To || ac::distance(*Two.To, At) > 0.01)
		{
			Two.To = At;
			Two.StallAt = U->position;
			Two.StallSince = Now;
		}
		if (ac::distance(U->position, At) < Near)
		{
			Stop(Face(At));
			return 1;
		}
		Pilot->SetTestInput(FVector2D(0, -1), Face(At));
		if (ac::distance(U->position, Two.StallAt) > 0.5)
		{
			Two.StallAt = U->position;
			Two.StallSince = Now;
		}
		return Now - Two.StallSince > 5.0 ? -1 : 0;
	};
	auto OtherHuman = [&]() -> const ac::net::Sky::Human* {
		if (!Mmo || !Mmo->Sky()) return nullptr;
		for (const ac::net::Sky::Human& H : Mmo->Sky()->pilots)
			if (Utf(H.callsign) == Two.OtherCallsign) return &H;
		return nullptr;
	};
	auto OtherUnit = [&]() -> const ac::Unit* {
		const ac::net::Sky::Human* H = OtherHuman();
		const ac::net::WorldMessage* W = Mmo ? Mmo->World() : nullptr;
		return H && W && H->unit && H->region == W->here && M ? M->unit(H->unit) : nullptr;
	};
	auto Tagged = [&](const FString& Callsign) {
		const AAcHUD* Hud = AAcHUD::Get(this);
		return Hud && Hud->TagsShown().Contains(Callsign.ToUpper());
	};
	auto Toasted = [&](const FString& Start) {
		const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> W = H ? H->Widget() : nullptr;
		if (W)
			for (const FString& X : W->ToastTitles())
				if (X.StartsWith(Start)) return true;
		return false;
	};
	auto Toasts = [&] {
		const UAcProgressHudSubsystem* H = UAcProgressHudSubsystem::Get(this);
		const TSharedPtr<SAcProgressHud> W = H ? H->Widget() : nullptr;
		return W ? FString::Join(W->ToastTitles(), TEXT(" | ")) : FString();
	};
	// The ground of the region here built (the terrain's sets complete).
	auto GroundReady = [&] {
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		return !bPictures || (Terrain && Terrain->NumSets() >= 4 && Terrain->NumSetsComplete() == Terrain->NumSets());
	};
	auto Hostiles = [&](const ac::Vec2 From, const double Reach) -> const ac::Unit* {
		const ac::Unit* Best = nullptr;
		if (S)
			for (const ac::Unit& V : S->units)
				if (V.brood() && V.hp > 0 && ac::distance(V.position, From) < Reach && (!Best || ac::distance(V.position, From) < ac::distance(Best->position, From)))
					Best = &V;
		return Best;
	};
	// Strikes the nearest bug: up to it, the act key held while it is the foe.
	auto Fight = [&] {
		if (!U || !Pilot) return;
		const ac::Unit* Bug = Hostiles(U->position, 40);
		if (Now - Two.FightLog > 5.0)
		{
			Two.FightLog = Now;
			const ac::Unit* Any = Hostiles(U->position, 1e9);
			const std::optional<ac::PilotTarget> T0 = M->simulation().pilotTarget(*U);
			UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo %s: fighting: nearest bug %s, target %s, task %d, struck %.1f s, %s"), *Two.Role,
				Any ? *FString::Printf(TEXT("#%lld %.1f cells off at %.1f,%.1f"), (long long)Any->id, ac::distance(Any->position, U->position), Any->position.x, Any->position.y) : TEXT("none"),
				!T0 ? TEXT("none") : T0->is<ac::PilotTarget::Enemy>() ? TEXT("enemy") : TEXT("other"), int32(U->task), Two.Struck, *Where());
		}
		if (!Bug)
		{
			Pilot->Press(false);
			Stop(Pilot->Yaw());
			return;
		}
		const std::optional<ac::PilotTarget> Target = M->simulation().pilotTarget(*U);
		if (Target && Target->is<ac::PilotTarget::Enemy>())
		{
			Stop(Face(Bug->position));
			Pilot->Press(true);
			Two.Struck += Dt;
			return;
		}
		Pilot->Press(false);
		// Close, but a mound in the way (E1: a bug gnawing a prize ring's
		// mound): step round it until the bug is what the act key strikes.
		const bool bClose = ac::distance(U->position, Bug->position) <= 1.6;
		Pilot->SetTestInput(FVector2D(bClose ? 1 : 0, bClose ? 0 : -1), Face(Bug->position));
	};

	switch (T.Phase)
	{
	case 0:
	{
		// The game ticking first: a join sent while the engine still loads
		// waits out its welcome before a packet leaves.
		static uint64 FirstFrame = 0;
		if (FirstFrame == 0) FirstFrame = GFrameCounter;
		if (GFrameCounter < FirstFrame + 60) return;
		if (Sim && Sim->IsMmo()) StopMmo();
		Two = FTwo();
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoTwo="), Two.Role);
		Two.Role = Two.Role.ToUpper();
		Two.Other = Two.Role == TEXT("A") ? TEXT("B") : TEXT("A");
		Two.bBack = FParse::Param(FCommandLine::Get(), TEXT("AcMmoTwoBack"));
		FParse::Value(FCommandLine::Get(), TEXT("AcMmoTwoSync="), Two.Sync);
		Two.Sync = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Two.Sync);
		FString JoinFile;
		const bool bJoining = FParse::Value(FCommandLine::Get(), TEXT("AcMmoJoin="), JoinFile) && !JoinFile.IsEmpty();
		Say((Two.Role == TEXT("A") || Two.Role == TEXT("B")) && !Two.Sync.IsEmpty() && (Two.Role == TEXT("A")) != bJoining,
			FString::Printf(TEXT("-AcMmoTwo=%s, sync in %s, %s"), *Two.Role, *Two.Sync, bJoining ? TEXT("joining") : TEXT("its server")));
		if (Two.Sync.IsEmpty()) return Finish();
		IFileManager::Get().MakeDirectory(*Two.Sync, true);
		if (bPictures) IFileManager::Get().MakeDirectory(*Shots.Dir, true);
		// The server's world as the canyon and the map pictures want it:
		// late morning, clear; the dev rig's Citadel for A; every region seen.
		if (Two.Role == TEXT("A"))
		{
			if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig"))) FCommandLine::Append(TEXT(" -AcMmoDevRig"));
			if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevBlueprints"))) FCommandLine::Append(TEXT(" -AcMmoDevBlueprints"));
			if (double Hour = 0; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour)) FCommandLine::Append(TEXT(" -AcMmoClockHour=11"));
			if (FString W; !FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), W)) FCommandLine::Append(TEXT(" -AcMmoWeather=clear"));
		}
		StartMmo(!Two.bBack);
		Advance();
		return;
	}
	case 1:
		if (Seated() && Mmo->Here() && P && Since() > 2.0 && GroundReady())
		{
			Say(true, FString::Printf(TEXT("seated as %s in Prospector #%lld %s"), *Utf(P->callsign), (long long)U->id, *Where()));
			if (!Pilot->ChaseEye()) Pilot->ToggleView();
			if (Two.bBack)
			{
				T.Phase = 600;
				T.PhaseAt = Now;
				return;
			}
			SyncMark(TEXT("seated"), Utf(P->callsign));
			Advance();
			return;
		}
		TimedOut(TEXT("seated"), 200);
		return;
	case 2:
		if (FString Body; SyncSeen(TEXT("seated"), &Body) && S)
		{
			Two.OtherCallsign = Body.TrimStartAndEnd();
			if (const ac::Structure* C = OfKind(*S, ac::StructureKind::citadel)) Two.Citadel = C->id;
			Say(!Two.OtherCallsign.IsEmpty() && Two.OtherCallsign != Utf(P->callsign),
				FString::Printf(TEXT("the other pilot is %s (this one %s)"), *Two.OtherCallsign, *Utf(P->callsign)));
			Say(Two.Citadel >= 0, FString::Printf(TEXT("the realm's Citadel (the dev rig's) is #%lld"), (long long)Two.Citadel));
			Advance();
			return;
		}
		TimedOut(TEXT("the other pilot seated"), 240);
		return;

	// MARK: 1 the tags
	case 3:
	{
		const ac::Unit* O = OtherUnit();
		// Turned a little off it: the chase camera sits behind this one's
		// own Prospector, which would hide the other one standing dead ahead.
		// A short step toward it first: the chase camera swings behind a unit
		// that moves (turned on the spot, it keeps the seat's view).
		if (O && bPictures && Since() < 0.8 && ac::distance(U->position, O->position) > 2.5) Pilot->SetTestInput(FVector2D(0, -1), Face(O->position) + 0.35);
		else if (O) Stop(Face(O->position) + (bPictures ? 0.35 : 0.0));
		if (!(O && Since() > 3.0)) return void(TimedOut(TEXT("the other's Prospector here")));
		const ac::net::Sky::Human* H = OtherHuman();
		Say(H && H->unit == O->id && !H->ferry, FString::Printf(TEXT("the sky names %s driving #%lld here"), *Two.OtherCallsign, (long long)O->id));
		if (bPictures)
		{
			if (!Tagged(Two.OtherCallsign) && Since() < 10.0) return;
			Say(Tagged(Two.OtherCallsign), FString::Printf(TEXT("the tag \"%s\" over its Prospector, %.1f cells off"), *Two.OtherCallsign.ToUpper(), ac::distance(U->position, O->position)));
			if (const UAcWorldRenderer* Rn = UAcWorldRenderer::Get(this))
			{
				const TConstArrayView<FTransform> Parts = Rn->PartWorld(O->id);
				const ac::Vec2 Drawn = Parts.Num() ? AcSpace::ToSim(Parts[0].GetLocation()) : ac::Vec2(-999, -999);
				UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo %s: the other's #%lld in the mirror at %.1f,%.1f (task %d), drawn at %.1f,%.1f, this one at %.1f,%.1f"), *Two.Role,
					(long long)O->id, O->position.x, O->position.y, int32(O->task), Drawn.x, Drawn.y, U->position.x, U->position.y);
			}
		}
		Shoot(FString::Printf(TEXT("w5-01-%s-sees-other"), R), FString::Printf(TEXT("%s's Prospector in the realm's blue, its tag over it"), *Two.OtherCallsign));
		return;
	}
	case 4:
		SyncMark(TEXT("tags"));
		Advance();
		return;
	case 5:
	{
		// A backs off from the Citadel, away from where B is; B waits.
		if (!SyncSeen(TEXT("tags"))) return void(TimedOut(TEXT("the other's tags")));
		const ac::Structure* C = S ? OfKind(*S, ac::StructureKind::citadel, Two.Citadel) : nullptr;
		if (!C || !U) return void(TimedOut(TEXT("the Citadel")));
		if (bA)
		{
			// Straight out from where it unloads (B goes round to the far side).
			ac::Vec2 Away = U->position - C->position;
			Away = ac::length(Away) > 1e-3 ? ac::normalize(Away) : ac::Vec2(1, 0);
			if (!Two.To) Two.To = C->position + Away * (ac::Rules::radius(C->kind) + 9.0);
			const ac::Vec2 To = *Two.To;
			const int32 D = DriveTo(To, 1.0);
			if (D == 0) return void(TimedOut(TEXT("backed off from the Citadel")));
			Stop(Face(C->position));
			Say(D == 1, FString::Printf(TEXT("A backed off from the Citadel at %.1f,%.1f to %.1f,%.1f: %.1f cells out (%s)"), C->position.x,
				C->position.y, To.x, To.y, ac::distance(U->position, C->position), *Where()));
			SyncMark(TEXT("apart"), FString::Printf(TEXT("%.3f %.3f"), U->position.x, U->position.y));
		}
		Advance();
		return;
	}
	case 6:
	{
		const ac::Structure* C = S ? OfKind(*S, ac::StructureKind::citadel, Two.Citadel) : nullptr;
		if (!C || !U) return;
		if (bA)
		{
			// From here B goes behind the Citadel: no tag over it.
			Stop(Face(C->position));
			if (!SyncSeen(TEXT("hidden"))) return void(TimedOut(TEXT("the other behind the Citadel"), 90));
			if (Since() < 2.0) return;
			if (bPictures)
			{
				const bool bTag = Tagged(Two.OtherCallsign);
				if (bTag && Since() < 6.0) return;
				Say(!bTag, FString::Printf(TEXT("%s behind the Citadel: no tag (TagSight: a building in the way)"), *Two.OtherCallsign));
			}
			Shoot(TEXT("w5-02-a-other-behind-citadel"), TEXT("the other pilot behind the Citadel: its tag hidden"));
			return;
		}
		// B: behind the Citadel from A, close by its wall (where it unloads).
		FString Body;
		if (!SyncSeen(TEXT("apart"), &Body)) return void(TimedOut(TEXT("A apart")));
		TArray<FString> Xy;
		Body.ParseIntoArray(Xy, TEXT(" "));
		const ac::Vec2 Apos(Xy.Num() > 1 ? FCString::Atod(*Xy[0]) : 0, Xy.Num() > 1 ? FCString::Atod(*Xy[1]) : 0);
		const ac::Vec2 Back = ac::normalize(C->position - Apos);
		const double Rad = ac::Rules::radius(C->kind);
		const ac::Vec2 Behind = C->position + Back * (Rad + 0.4);
		// Round the Citadel on the side B is on.
		const ac::Vec2 Side(-Back.y, Back.x);
		const double Sgn = ac::dot(U->position - C->position, Side) >= 0 ? 1.0 : -1.0;
		const ac::Vec2 Round = C->position + Side * Sgn * (Rad + 2.5) + Back * (Rad * 0.5);
		const bool bRound = ac::dot(U->position - C->position, Back) < Rad * 0.3;
		const int32 D = DriveTo(bRound ? Round : Behind, bRound ? 1.2 : 0.4);
		// Up against its wall, where it unloads (a stall there is the wall).
		const bool bThere = !bRound && (D == 1 || AtRealmCitadel());
		if (D < 0 && !bThere)
		{
			Say(false, FString::Printf(TEXT("stalled on the way behind the Citadel (%s)"), *Where()));
			SyncMark(TEXT("failed"));
			return Finish();
		}
		if (!bThere || Since() < 1.0) return void(TimedOut(TEXT("behind the Citadel")));
		Stop(Face(C->position));
		SyncMark(TEXT("hidden"));
		Advance();
		return;
	}
	case 7:
		if (bA) SyncMark(TEXT("hidden-seen"));
		Advance();
		return;

	// MARK: 2 the Beacon
	case 8:
	{
		const ac::Structure* C = S ? OfKind(*S, ac::StructureKind::citadel, Two.Citadel) : nullptr;
		if (!C || !U) return;
		if (!bA)
		{
			Stop(Face(C->position));
			if (!SyncSeen(TEXT("hidden-seen"))) return void(TimedOut(TEXT("A's look behind the Citadel"), 90));
			Advance();
			return;
		}
		// A back at the Citadel's wall, where it unloads; the realm wallet filled.
		const ac::Vec2 Out = ac::normalize(U->position - C->position);
		const int32 D = DriveTo(C->position + Out * (ac::Rules::radius(C->kind) + 0.4), 0.4);
		if (!AtRealmCitadel() || (D == 0 && Since() < 2.0)) return void(TimedOut(TEXT("A where it unloads at the Citadel")));
		Stop(Face(C->position));
		Say(Mmo->DevClock(TEXT("defence wallet")), TEXT("the server fills the realm wallet"));
		Advance();
		return;
	}
	case 9:
		if (bA)
		{
			if (!(P && P->realmWallet.ore >= 3000 && Since() > 1.0)) return void(TimedOut(TEXT("the realm wallet filled"), 20));
			FString Why;
			Say(Mmo->StartProject(ac::RealmProject::Kind::beacon, &Why), FString::Printf(TEXT("A lights the Beacon %s"), *Why));
			Advance();
			return;
		}
		// B: stands where it unloads at the Citadel (it is behind it).
		Advance();
		return;
	case 10:
		if (!(Mmo->ProjectOn(ac::RealmProject::Kind::beacon) && Since() > 0.5)) return void(TimedOut(TEXT("the Beacon lit"), 20));
		if (bPictures && !Toasted(TEXT("THE REALM LIT THE BEACON")) && Since() < 5.0) return;
		if (bPictures) Say(Toasted(TEXT("THE REALM LIT THE BEACON")), FString::Printf(TEXT("the realm's toast: %s"), *Toasts()));
		if (bA)
		{
			SyncMark(TEXT("beacon"));
			Shoot(TEXT("w5-03-a-beacon-lit"), TEXT("A lit the Beacon: the realm-wide toast, the bar under the realm wallet"));
			return;
		}
		Shoot(TEXT("w5-03-b-beacon-toast"), TEXT("B hears of the Beacon A lit: the realm-wide toast"));
		return;
	case 11:
		if (bA)
		{
			if (!SyncSeen(TEXT("realm"))) return void(TimedOut(TEXT("B's REALM tab"), 90));
			Advance();
			return;
		}
		if (!AtRealmCitadel())
		{
			// Up against its wall.
			if (const ac::Structure* C = S ? OfKind(*S, ac::StructureKind::citadel, Two.Citadel) : nullptr; C && Pilot)
				Pilot->SetTestInput(FVector2D(0, -0.5), Face(C->position));
			return void(TimedOut(TEXT("B where it unloads at the Citadel"), 15));
		}
		Stop(Pilot->Yaw());
		Say(OpenCitadelScreen() && CitadelScreenWidget().IsValid(), TEXT("B opens the Citadel screen"));
		if (const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget()) W->SetTab(SAcCitadelScreen::ETab::Realm);
		Advance();
		return;
	case 12:
		if (!bA)
		{
			const TSharedPtr<SAcCitadelScreen> W = CitadelScreenWidget();
			if (!(W && W->GetInfo().Projects.Num() == ac::RealmProject::count && Since() > 1.5)) return void(TimedOut(TEXT("B's REALM tab"), 15));
			const auto& Pr = W->GetInfo().Projects[0];
			Say(Pr.bRunning, FString::Printf(TEXT("B's REALM tab: the Beacon RUNNING, started by %s"), *Pr.By));
			Shoot(TEXT("w5-04-b-realm-tab"), TEXT("B at the Citadel, its REALM tab: the Beacon A lit running"));
			return;
		}
		Advance();
		return;
	case 13:
		if (!bA)
		{
			CloseCitadelScreen();
			const std::optional<int64> Fare = Mmo->FlightPrice(TEXT("cold-fork"));
			int64 Steps = -1;
			if (const ac::net::WorldMessage* Wm = Mmo->World(); Wm && Mmo->Here())
				for (const ac::net::WorldMessage::Region& Rg : Wm->regions)
					if (Utf(Rg.id) == TEXT("cold-fork")) Steps = ac::Flight::steps(Mmo->Here()->column, Mmo->Here()->row, Rg.column, Rg.row);
			const int64 Whole = Steps >= 0 ? ac::Flight::hydrogen(Steps) : -1;
			Say(Fare.has_value() && Whole > 0 && *Fare == (Whole + 1) / 2,
				FString::Printf(TEXT("B's fare to Cold Fork %lld MH with A's Beacon (%lld without)"), (long long)Fare.value_or(-1), (long long)Whole));
			SyncMark(TEXT("realm"));
		}
		Advance();
		return;

	// MARK: 3 the Hab Dome
	case 14:
	{
		const ac::Structure* C = S ? OfKind(*S, ac::StructureKind::citadel, Two.Citadel) : nullptr;
		if (!C || !U) return;
		if (!bA)
		{
			// B: out from behind the Citadel, to where A builds.
			FString Body;
			if (!SyncSeen(TEXT("dome-at"), &Body)) return void(TimedOut(TEXT("A's Hab Dome placed"), 90));
			TArray<FString> Xy;
			Body.ParseIntoArray(Xy, TEXT(" "));
			const ac::Vec2 At(Xy.Num() > 1 ? FCString::Atod(*Xy[0]) : 0, Xy.Num() > 1 ? FCString::Atod(*Xy[1]) : 0);
			Two.Dome = Xy.Num() > 2 ? FCString::Atoi64(*Xy[2]) : -1;
			Stop(Face(At));
			Advance();
			return;
		}
		// A: out past its earlier spot, the Hab Dome on open ground there.
		const ac::Vec2 Out = ac::normalize(U->position - C->position + ac::Vec2(0.01, 0));
		if (!Two.To) Two.To = C->position + Out * (ac::Rules::radius(C->kind) + 12.0);
		if (!Two.bSent)
		{
			// (Stalled: it builds where it stands.)
			const int32 D = DriveTo(*Two.To, 1.0);
			if (D == 0) return void(TimedOut(TEXT("A to the Hab Dome's spot")));
			Stop(Pilot->Yaw());
			// Still for a moment first: the walk inputs still on their way
			// would walk it off the weld.
			if (Two.StillAt < 0) Two.StillAt = Now;
			if (Now - Two.StillAt < 1.5) return;
			const ac::Simulation& Sm = M->simulation();
			const double Reach = 3.0;
			for (int32 K = 0; K < 24 && !Two.bSent; ++K)
			{
				const double H = U->heading + K * 2.0 * PI / 24;
				const std::optional<ac::Vec2> Spot = Sm.snap(ac::StructureKind::habDome, U->position + ac::Vec2(std::cos(H), std::sin(H)) * Reach);
				if (!Spot || Sm.placementRefusal(ac::StructureKind::habDome, *Spot) || Mmo->PadPlateRefusal(ac::StructureKind::habDome, *Spot)) continue;
				if (ac::distance(*Spot, C->position) < ac::Rules::radius(C->kind) + ac::Rules::radius(ac::StructureKind::habDome) + 1.5) continue;
				Two.bSent = Mmo->Build(ac::StructureKind::habDome, *Spot);
				Two.Left = *Spot;
			}
			Say(Two.bSent, TEXT("A asks for a Hab Dome beside it"));
			if (!Two.bSent) return Finish();
			return;
		}
		for (const FString& Why : Mmo->TakeBuildRefusals()) Say(false, FString::Printf(TEXT("the Hab Dome built, not refused: %s"), *Why));
		const ac::Structure* Dm = OfKind(*S, ac::StructureKind::habDome);
		if (!Dm) return void(TimedOut(TEXT("the Hab Dome placed"), 20));
		Two.Dome = Dm->id;
		Stop(Face(Dm->position));
		SyncMark(TEXT("dome-at"), FString::Printf(TEXT("%.3f %.3f %lld"), Dm->position.x, Dm->position.y, (long long)Dm->id));
		Advance();
		return;
	}
	case 15:
	{
		const ac::Structure* Dm = S ? OfKind(*S, ac::StructureKind::habDome, Two.Dome) : nullptr;
		if (!Dm) return void(TimedOut(TEXT("the Hab Dome in the mirror"), 20));
		if (!bA)
		{
			// B: it rises (a picture half way up).
			Stop(Face(Dm->position));
			if (!Dm->complete() && Since() > 4.0)
			{
				Say(true, FString::Printf(TEXT("B sees A's Hab Dome rise: %.0f hp of %.0f"), double(Dm->hp), double(ac::Rules::hp(ac::StructureKind::habDome))));
				Shoot(TEXT("w5-05-b-dome-rising"), TEXT("A's Hab Dome rising, seen by B, A welding it"));
				return;
			}
			if (Dm->complete())
			{
				Say(Since() > 4.0, TEXT("B saw the Hab Dome before it was finished"));
				Advance();
			}
			return;
		}
		Stop(Face(Dm->position));
		Advance();
		return;
	}
	case 16:
	{
		const ac::Structure* Dm = S ? OfKind(*S, ac::StructureKind::habDome, Two.Dome) : nullptr;
		if (bA && Dm && U && Now - Two.TickSince > 5.0)
		{
			Two.TickSince = Now;
			UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo A: welding: task %d on #%lld, the dome's builder #%lld, %.0f hp, %.1f s left, %.1f cells off"),
				int32(U->task), (long long)U->structure.value_or(-1), (long long)Dm->builder.value_or(-1), double(Dm->hp), Dm->buildLeft.value_or(-1),
				ac::distance(U->position, Dm->position));
		}
		if (!(Dm && Dm->complete())) return void(TimedOut(TEXT("the Hab Dome finished"), WeldLimit));
		if (bA)
		{
			SyncMark(TEXT("dome"));
			Advance();
			return;
		}
		// B: the deed's toast (its news comes beside the snapshots, through
		// the lag: the wait counts from the Dome seen finished).
		if (Two.DoneAt < 0) Two.DoneAt = Now;
		Stop(Face(Dm->position));
		const FString Want = FString::Printf(TEXT("%s BUILT A HAB DOME"), *Two.OtherCallsign.ToUpper());
		if (bPictures && !Toasted(Want) && Now - Two.DoneAt < 6.0) return;
		// The toast faded in, B facing the Dome.
		if (bPictures && Toasted(Want) && Two.ToastSeen < 0) Two.ToastSeen = Now;
		if (bPictures && Two.ToastSeen >= 0 && Now - Two.ToastSeen < 0.8) return;
		if (bPictures) Say(Toasted(Want), FString::Printf(TEXT("B's toast \"%s\": %s"), *Want, *Toasts()));
		Shoot(TEXT("w5-06-b-deed-toast"), FString::Printf(TEXT("the deed's toast on B: \"%s\", in Prospect Landing"), *Want));
		return;
	}
	case 17:
		if (bA)
		{
			Advance();
			return;
		}
		Say(OpenWorldMap() && WorldMapWidget().IsValid(), TEXT("B opens the world map"));
		Advance();
		return;
	case 18:
	{
		if (bA)
		{
			Advance();
			return;
		}
		const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
		if (!Map) return void(TimedOut(TEXT("the world map")));
		if (Since() < 1.0) return;
		if (!Map->Hovered())
		{
			const int64 Id = Two.Dome;
			const bool bHover = Map->HoverWhere([Id](const ac::net::atlas::Item& It) {
				return It.kind == ac::net::atlas::Kind::building && It.building == ac::StructureKind::habDome && It.ref == Id;
			});
			Say(bHover, TEXT("B hovers the Hab Dome's dot on the map"));
			if (const ac::net::atlas::Item* It = Map->Hovered()) Map->SetView(It->at + ac::Vec2(8, 3), 13);
			if (!bHover) Advance();
			return;
		}
		if (Since() < 2.0) return;
		const FString Tip = Map->TooltipText();
		Say(Tip.StartsWith(TEXT("Hab Dome")) && Tip.Contains(TEXT("Built by")) && Tip.Contains(Two.OtherCallsign),
			FString::Printf(TEXT("the Hab Dome's tooltip: %s"), *Tip.Replace(TEXT("\n"), TEXT(" / "))));
		Shoot(TEXT("w5-07-b-map-tooltip"), TEXT("B's world map: the Hab Dome's dot, its tooltip built by A"));
		return;
	}
	case 19:
		if (!bA) CloseWorldMap();
		Advance();
		return;
	case 20:
	{
		if (bA)
		{
			// A: the realm wallet when B sets off with its load.
			if (!SyncSeen(TEXT("loaded"))) return void(TimedOut(TEXT("B's load drilled"), 120));
			Two.RealmOre = P ? P->realmWallet.ore : -1;
			Advance();
			return;
		}
		// B: to the nearest deposit, the act key until a load is in.
		if (!U || !S) return;
		const ac::OreDeposit* Near = nullptr;
		for (const ac::OreDeposit& Dp : S->patches)
			if (Dp.remaining > 50 && (!Near || ac::distance(Dp.position, U->position) < ac::distance(Near->position, U->position))) Near = &Dp;
		if (!Near) return void(TimedOut(TEXT("a deposit near B")));
		const std::optional<ac::PilotTarget> Target = M->simulation().pilotTarget(*U);
		if (Now - Two.TickSince > 4.0)
		{
			Two.TickSince = Now;
			UE_LOG(LogAutocraft, Display, TEXT("flowtest: mmotwo B: drilling: deposit at %.1f,%.1f (%lld left, miner #%lld), %.1f cells off, target %d, task %d, carrying %lld"),
				Near->position.x, Near->position.y, (long long)Near->remaining, (long long)Near->miner.value_or(-1), ac::distance(U->position, Near->position),
				!Target ? -1 : Target->is<ac::PilotTarget::Patch>() ? 1 : Target->is<ac::PilotTarget::Full>() ? 3 : Target->is<ac::PilotTarget::Scaffold>() ? 4 : 0,
				int32(U->task), (long long)U->carrying);
		}
		if (U->carrying > 0 && U->task != ac::Unit::Task::mining)
		{
			Pilot->Press(false);
			Two.Ore = Ore;
			SyncMark(TEXT("loaded"), FString::Printf(TEXT("%lld"), (long long)U->carrying));
			Advance();
			return;
		}
		// Drilling (the key held: let go, it loses the load).
		if ((Target && Target->is<ac::PilotTarget::Patch>()) || U->task == ac::Unit::Task::mining)
		{
			Stop(Face(Near->position));
			Pilot->Press(true);
		}
		else
		{
			Pilot->Press(false);
			if (DriveTo(Near->position, 1.2) < 0) Pilot->SetTestInput(FVector2D(0, 1), Pilot->Yaw());
			else if (ac::distance(U->position, Near->position) < 1.2) Pilot->SetTestInput(FVector2D(0, -0.6), Face(Near->position));
		}
		TimedOut(TEXT("a load drilled"), 90);
		return;
	}
	case 21:
	{
		if (bA)
		{
			FString Body;
			if (!SyncSeen(TEXT("unloaded"), &Body)) return void(TimedOut(TEXT("B's unload"), 120));
			const int64 Tax = FCString::Atoi64(*Body);
			if (!(P && P->realmWallet.ore >= Two.RealmOre + Tax && Since() > 1.0)) return void(TimedOut(TEXT("the realm's tax in A's wallet strip"), 20));
			Say(Tax > 0, FString::Printf(TEXT("B's tax %lld ore: the realm wallet on A's HUD %lld -> %lld"), (long long)Tax, (long long)Two.RealmOre,
				(long long)P->realmWallet.ore));
			Shoot(TEXT("w5-09-a-realm-tax"), TEXT("A's HUD: the realm wallet up by B's tax at A's Hab Dome"));
			return;
		}
		// B: home to the Hab Dome with it.
		const ac::Structure* Dm = S ? OfKind(*S, ac::StructureKind::habDome, Two.Dome) : nullptr;
		if (!Dm || !U) return;
		if (U->carrying > 0)
		{
			const ac::Vec2 Edge = Dm->position + ac::normalize(U->position - Dm->position) * (ac::Rules::radius(Dm->kind) + 0.4);
			if (DriveTo(Edge, 0.4) == 1) Pilot->SetTestInput(FVector2D(0, -0.6), Face(Dm->position));
			return void(TimedOut(TEXT("unloaded at the Hab Dome"), 60));
		}
		Stop(Face(Dm->position));
		const int64 Gained = Ore - Two.Ore;
		const int64 Tax = Gained > 0 ? ac::Progression::realmTax(Gained * 100 / (100 - ac::Progression::realmTaxPercent)) : 0;
		if (bPictures && !Toasted(TEXT("+")) && Since() < 3.0) return;
		Say(Gained > 0, FString::Printf(TEXT("B unloaded at A's Hab Dome: +%lld ore in its wallet, about %lld to the realm (%s)"), (long long)Gained,
			(long long)Tax, *Toasts()));
		SyncMark(TEXT("unloaded"), FString::Printf(TEXT("%lld"), (long long)FMath::Max<int64>(Tax - 1, 1)));
		Shoot(TEXT("w5-08-b-unloaded-at-dome"), TEXT("B's load into A's Hab Dome: its gain, the realm's tax"));
		return;
	}
	case 22:
		if (bA) SyncMark(TEXT("taxed"));
		else if (!SyncSeen(TEXT("taxed"))) return void(TimedOut(TEXT("A's tax seen"), 60));
		Advance();
		return;

	// MARK: 4 the Dropship
	case 23:
		if (bA)
		{
			Say(Mmo->DevClock(TEXT("ferry pad here\nferry pad tern-ridge")), TEXT("Landing Pads under A and in Tern Ridge"));
			Advance();
			return;
		}
		Advance();
		return;
	case 24:
	{
		const ac::net::WorldMessage* W = Mmo->World();
		if (!bA)
		{
			// B faces A on its pad.
			if (!SyncSeen(TEXT("pads"))) return void(TimedOut(TEXT("A's Landing Pads"), 60));
			if (const ac::Unit* O = OtherUnit())
			{
				Two.Left = O->position;
				Stop(Face(O->position));
			}
			if (Since() < 2.0) return;
			SyncMark(TEXT("watching"));
			Advance();
			return;
		}
		FString Pad;
		if (W)
			for (const ac::net::WorldMessage::Region& Rg : W->regions)
				if (Utf(Rg.id) == TEXT("tern-ridge"))
					for (const ac::net::WorldMessage::Pad& Pd : Rg.pads)
						if (Pd.building) Pad = Utf(Pd.name);
		if (Pad.IsEmpty() || Since() < 2.0) return void(TimedOut(TEXT("a Landing Pad in Tern Ridge")));
		Stop(Pilot->Yaw());
		SyncMark(TEXT("pads"));
		Shots.Region = Pad;
		Advance();
		return;
	}
	case 25:
		if (bA)
		{
			if (!SyncSeen(TEXT("watching"))) return void(TimedOut(TEXT("B watching"), 60));
			FString Why;
			Say(Mmo->Fly(TEXT("tern-ridge"), Shots.Region, &Why), FString::Printf(TEXT("A flies to Tern Ridge's %s %s"), *Shots.Region, *Why));
			SyncMark(TEXT("flying"));
			Advance();
			return;
		}
		Advance();
		return;
	case 26:
	{
		if (bA)
		{
			// A: down in Tern Ridge, driving again.
			if (!(Seated() && Here == TEXT("tern-ridge") && Since() > 2.0)) return void(TimedOut(TEXT("A down in Tern Ridge"), 90));
			Say(true, FString::Printf(TEXT("A landed in Tern Ridge %s"), *Where()));
			SyncMark(TEXT("landed"));
			Advance();
			return;
		}
		// B: the ship come down, A aboard, the lift, the way out.
		EAcFlightStage Stage = EAcFlightStage::None;
		for (const TPair<FString, EAcFlightStage>& X : AcPilotFlight::Watched())
			if (X.Key == Two.OtherCallsign) Stage = X.Value;
		if (Stage != EAcFlightStage::None)
		{
			Two.bStagesInOrder = Two.bStagesInOrder && uint8(Stage) >= Two.LastStage;
			Two.LastStage = FMath::Max(Two.LastStage, uint8(Stage));
			if (Stage >= EAcFlightStage::Board && Tagged(Two.OtherCallsign)) Two.bTagWhileAboard = true;
		}
		if (U) Stop(Face(Two.Left));
		auto Want = [&](const EAcFlightStage St, const TCHAR* Name, const TCHAR* What) {
			if (Stage != St || Two.bShotStage[uint8(St)]) return false;
			Two.bShotStage[uint8(St)] = true;
			T.Phase -= 1;  // back here once the picture is written
			Shoot(Name, What);
			return true;
		};
		// The ship low over the pad: 2.2 s of its 3 s descent.
		if (Stage == EAcFlightStage::Descend && Two.DescendSeen < 0) Two.DescendSeen = Now;
		if (Stage == EAcFlightStage::Descend && Now - Two.DescendSeen > 2.2
			&& Want(EAcFlightStage::Descend, TEXT("w5-10-b-ship-down"), TEXT("A's Dropship coming down onto its pad, seen by B")))
			return;
		if (Want(EAcFlightStage::Board, TEXT("w5-11-b-boarding"), TEXT("A's Prospector walking up the ramp, its tag gone"))) return;
		if (Want(EAcFlightStage::Lift, TEXT("w5-12-b-lift"), TEXT("A's Dropship lifting off the pad"))) return;
		if (Two.LastStage >= uint8(EAcFlightStage::Lift) && Stage == EAcFlightStage::None)
		{
			Say(Two.bStagesInOrder && Two.LastStage >= uint8(EAcFlightStage::Turns),
				FString::Printf(TEXT("B saw A's flight come down, board, lift and go (to stage %d)"), int32(Two.LastStage)));
			if (bPictures) Say(!Two.bTagWhileAboard, TEXT("A's tag gone while aboard"));
			Advance();
			return;
		}
		TimedOut(TEXT("A's flight watched"), 90);
		return;
	}
	case 27:
		if (!bA && !SyncSeen(TEXT("landed"))) return void(TimedOut(TEXT("A landed"), 60));
		Advance();
		return;

	// MARK: 5 The Fork
	case 28:
		Say(Mmo->DevClock(bA ? TEXT("brood zone here") : TEXT("as 2 brood zone here")), TEXT("the server takes the pilot to The Fork"));
		Advance();
		return;
	case 29:
		if (!(Seated() && Here == TEXT("cold-fork") && GroundReady() && Since() > 3.0)) return void(TimedOut(TEXT("at The Fork"), 120));
		Two.Ore = Ore;
		Two.Hydrogen = Hydrogen;
		if (U) Two.ForkAt = U->position;
		SyncMark(TEXT("fork"));
		Advance();
		return;
	case 30:
		if (!SyncSeen(TEXT("fork"))) return void(TimedOut(TEXT("the other at The Fork"), 120));
		if (bA && !Two.bSent)
		{
			Two.bSent = true;
			Say(Mmo->DevClock(TEXT("brood clear\ncanyon reset\ncanyon send 1")), TEXT("wave 1 sent"));
		}
		if (const ac::Unit* O = OtherUnit()) Stop(Face(O->position));
		if (Since() < 3.0) return;
		if (bPictures) Say(Tagged(Two.OtherCallsign), FString::Printf(TEXT("the other's tag at The Fork (%s)"), *FString::Join(AAcHUD::Get(this)->TagsShown(), TEXT(", "))));
		Two.Struck = 0;
		Advance();
		return;
	case 31:
		// Both strike wave 1's bugs; each, its strikes in, steps back past
		// the open ground it came on (E1: the prize's ring sits round the
		// well, and a bug turns on its hitter: out of its reach) while the
		// other's come.
		if (!Two.bSent) Fight();
		else if (U && Pilot)
		{
			const ac::Vec2 Back = Two.ForkAt + ac::Vec2(0, 10);
			Pilot->Press(false);
			if (ac::distance(U->position, Back) > 1.5) Pilot->SetTestInput(FVector2D(0, -1), Face(Back));
			else Stop(Pilot->Yaw());
		}
		// Each one's strikes in (the bugs fell a Prospector that fights on
		// long, at the pictures' pace).
		if (Two.Struck > 1.5 && !Two.bSent)
		{
			Two.bSent = true;
			SyncMark(TEXT("struck"), FString::Printf(TEXT("%.1f"), Two.Struck));
		}
		if (bA && Two.bSent && SyncSeen(TEXT("struck")) && Since() > 2.0)
		{
			if (Pilot) Pilot->Press(false);
			Say(Mmo->DevClock(TEXT("canyon beat\nbrood clear")), TEXT("wave 1 beaten (both struck it), its bugs gone"));
			Advance();
			return;
		}
		if (!bA && Two.bSent && SyncSeen(TEXT("beaten-sent")))
		{
			if (Pilot) Pilot->Press(false);
			Advance();
			return;
		}
		TimedOut(TEXT("both striking wave 1"), 120);
		return;
	case 32:
		if (bA) SyncMark(TEXT("beaten-sent"));
		if (Pilot) Pilot->Press(false);
		Advance();
		return;
	case 33:
	{
		// B steps back from A (the fight left them side by side), then stands.
		if (!bA && Pilot && Since() < 1.2) return Pilot->SetTestInput(FVector2D(0, 1), Pilot->Yaw());
		if (Pilot) Stop(Pilot->Yaw());
		if (!(Ore > Two.Ore && Since() > 1.0)) return void(TimedOut(TEXT("the wave's share in the wallet"), 30));
		if (bPictures && !Toasted(TEXT("YOUR SHARE")) && Since() < 6.0) return;
		if (bPictures) Say(Toasted(TEXT("YOUR SHARE")), FString::Printf(TEXT("the share's toast: %s"), *Toasts()));
		Say(true, FString::Printf(TEXT("paid its share of wave 1: +%lld ore, +%lld MH"), (long long)(Ore - Two.Ore), (long long)(Hydrogen - Two.Hydrogen)));
		SyncMark(TEXT("paid"), FString::Printf(TEXT("%lld"), (long long)(Ore - Two.Ore)));
		Shoot(FString::Printf(TEXT("w5-13-%s-share"), R), TEXT("wave 1 beaten: this pilot's share, the toast"));
		return;
	}
	case 34:
		if (!SyncSeen(TEXT("paid"))) return void(TimedOut(TEXT("the other paid"), 60));
		Advance();
		return;

	// MARK: 6 B leaves mid-wave and comes back
	case 35:
		if (bA)
		{
			Say(Mmo->DevClock(TEXT("canyon send 2")), TEXT("wave 2 sent"));
			SyncMark(TEXT("wave2"));
			Two.TickWas = -1;
			Advance();
			return;
		}
		if (!SyncSeen(TEXT("wave2"))) return void(TimedOut(TEXT("wave 2"), 30));
		if (Since() < 4.0) return;
		// B leaves mid-wave: where it is, its wallet, then its game ends.
		SyncMark(TEXT("quit"), FString::Printf(TEXT("%s %.3f %.3f %lld %lld"), *Here, U ? U->position.x : 0.0, U ? U->position.y : 0.0, (long long)Ore, (long long)Hydrogen));
		Say(true, FString::Printf(TEXT("B leaves mid-wave %s"), *Where()));
		Finish();
		return;
	case 36:
	{
		// A, undisturbed: seated in Cold Fork, the snapshots coming, B's tag
		// gone, then back.
		if (!(Seated() && Here == TEXT("cold-fork")))
		{
			Say(false, FString::Printf(TEXT("A undisturbed while B is away (%s)"), *Where()));
			SyncMark(TEXT("failed"));
			return Finish();
		}
		if (M->serverTick() != Two.TickWas)
		{
			Two.TickWas = M->serverTick();
			Two.TickSince = Now;
		}
		if (Now - Two.TickSince > 2.0)
		{
			Say(false, TEXT("A's snapshots stopped while B left"));
			SyncMark(TEXT("failed"));
			return Finish();
		}
		// The wave beaten once B quits (A alone would fall to it while the
		// server still waits on B: a lost goodbye leaves it to the timeout).
		if (SyncSeen(TEXT("quit")) && !Two.bSent)
		{
			Two.bSent = true;
			Two.QuitSeen = Now;
			// One control file read: the beat and the next wave held off (two
			// writes in a tick keep only the second).
			Say(Mmo->DevClock(TEXT("canyon beat\nbrood clear\ncanyon break 600")), TEXT("wave 2 beaten with B away, its bugs gone, the next held off"));
		}
		if (Two.bSent && !OtherHuman() && !Two.bOtherGone)
		{
			Two.bOtherGone = true;
			Two.GoneAt = Now;
			Say(true, FString::Printf(TEXT("B gone from the sky %.1f s after it quit; the server carries on (%s)"), Now - Two.QuitSeen, *Where()));
		}
		// A drives on: back toward the open ground it came to The Fork on, a
		// few cells off where B left (B comes back there).
		if (Two.bOtherGone && Pilot && U && Now - Two.GoneAt < 2.5 && ac::distance(U->position, Two.ForkAt) > 1.5)
		{
			Pilot->Press(false);
			Pilot->SetTestInput(FVector2D(0, -1), Face(Two.ForkAt));
		}
		else Fight();
		if (Two.bOtherGone && OtherUnit() && SyncSeen(TEXT("back")))
		{
			Say(true, FString::Printf(TEXT("A drove on while B was away; B back, %s"), *Where()));
			Advance();
			return;
		}
		TimedOut(TEXT("B away and back"), 300);
		return;
	}
	case 37:
	{
		const ac::Unit* O = OtherUnit();
		if (O) Stop(Face(O->position) + 0.35);
		if (Since() < 3.0) return;
		if (bPictures) Say(Tagged(Two.OtherCallsign), TEXT("B's tag over its Prospector again"));
		Shoot(TEXT("w5-15-a-other-back"), TEXT("A at The Fork: B back in its Prospector, its tag again"));
		return;
	}
	case 38:
		if (!SyncSeen(TEXT("done"))) return void(TimedOut(TEXT("B done"), 60));
		Say(Mmo->DevClock(TEXT("canyon reset")), TEXT("The Fork reset"));
		Finish();
		return;

	// B again (-AcMmoTwoBack): where it left, with its wallet.
	case 600:
	{
		FString Body;
		Two.OtherCallsign.Empty();
		if (FString A; FFileHelper::LoadFileToString(A, *MarkPath(TEXT("A"), TEXT("seated")))) Two.OtherCallsign = A.TrimStartAndEnd();
		if (!FFileHelper::LoadFileToString(Body, *MarkPath(TEXT("B"), TEXT("quit"))))
		{
			Say(false, TEXT("B's quit mark"));
			return Finish();
		}
		TArray<FString> F;
		Body.ParseIntoArray(F, TEXT(" "));
		if (F.Num() < 5)
		{
			Say(false, FString::Printf(TEXT("B's quit mark: %s"), *Body));
			return Finish();
		}
		const ac::Vec2 Was(FCString::Atod(*F[1]), FCString::Atod(*F[2]));
		const int64 OreWas = FCString::Atoi64(*F[3]), MhWas = FCString::Atoi64(*F[4]);
		Say(Here == F[0], FString::Printf(TEXT("B back in %s (left in %s)"), *Here, *F[0]));
		Say(U && ac::distance(U->position, Was) < 3.0, FString::Printf(TEXT("B back where it left: %.1f cells off (%s)"), U ? ac::distance(U->position, Was) : -1.0, *Where()));
		Say(Ore == OreWas && Hydrogen == MhWas, FString::Printf(TEXT("B's wallet as it left it: %lld ore %lld MH (left with %lld, %lld)"), (long long)Ore,
			(long long)Hydrogen, (long long)OreWas, (long long)MhWas));
		SyncMark(TEXT("back"));
		Advance();
		return;
	}
	case 601:
	{
		const ac::Unit* O = OtherUnit();
		if (O) Stop(Face(O->position) + 0.35);
		if (Since() < 3.0) return;
		if (bPictures) Say(Tagged(Two.OtherCallsign), TEXT("A's tag from B's view again"));
		Shoot(TEXT("w5-14-b-back"), TEXT("B's game again: back at The Fork where it left, its wallet, A beside it"));
		return;
	}
	case 602:
		SyncMark(TEXT("done"));
		Finish();
		return;
	default:
		Finish();
		return;
	}
}
