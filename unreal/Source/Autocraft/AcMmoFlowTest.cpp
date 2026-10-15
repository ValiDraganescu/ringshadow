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
//             (here and in mmofly: the wallet shown never dips over the
//             travel and no gain flashes)
//   mmofly    F2's Dropship: Landing Pads under the pilot and in Cold Fork
//             (the `ferry` dev lines), M's flight menu (the region file's pad
//             not pickable), Cold Fork's Landing Pad picked and FLY pressed;
//             the flight shown (AcPilotFlight.h): descend, board, lift,
//             turns, dash, approach and step out in turn, never a fade, the
//             approach's region change seamless, control back on Cold Fork's
//             Landing Pad (-AcMmoFlightShots=DIR: a PNG of four stages)
//   mmohandback  Esc (a hand back) then a take again: the pawn never dives
//             back in on an old snapshot, nor drops a unit just taken
//   mmoresume  a world that lasts (docs/mmorpg-m3.md S4): the home screen's
//             MMORPG NEW WORLD (TEST), asked and confirmed; a Citadel built;
//             the mode left and MMORPG (TEST) again: the Citadel, the wallet,
//             the same pilot key, the same Prospector on the same spot; then
//             a new world: no Citadel, 600 ore, at the start pad, same key
//
// Each step but mmoresume's resume starts a new world (the start pad). The
// pilot key and the save are the test's own (`FAcMmoClient::Directory`:
// -AcMmoDir, or -AcSaveDir's `mmo` folder), never the player's.
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

#include "Rules.h"
#include "Simulation.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
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

	const ac::Structure* CitadelIn(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}

	/// mmofly: the flight as the pilot pawn showed it (AcPilotFlight.h).
	struct FFlightSeen
	{
		/// When each stage was first seen (seconds; < 0: never), by
		/// `EAcFlightStage`.
		double Seen[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
		/// The most the picture was faded aboard (F2: never).
		double MostFade = 0;
		/// Seamless region changes before FLY.
		int32 SeamlessBefore = 0;
		/// The Landing Pads asked of the server.
		bool bPadsAsked = false;
		/// -AcMmoFlightShots=DIR: the pictures taken.
		bool bShot[8] = {};
	};
	FFlightSeen FlightSeen;

	/// mmocross, mmofly: the wallet the HUD showed over the travel
	/// (docs/mmorpg-m5.md, E3): never below the stock it set out with, and
	/// no gain flashed (no income on the way).
	struct FWalletSeen
	{
		int64 From = -1;
		int64 Lowest = -1;
		int32 GainsFrom = 0;
	};
	FWalletSeen WalletSeen;

	const SAcResourceBar* WalletBar(const UObject* Ctx)
	{
		const AAcHUD* Hud = AAcHUD::Get(Ctx);
		return Hud && Hud->Root() ? Hud->Root()->ResourceBar().Get() : nullptr;
	}

	void FlightShot(bool& bTaken, const TCHAR* Name)
	{
		FString Dir;
		if (bTaken || !FParse::Value(FCommandLine::Get(), TEXT("AcMmoFlightShots="), Dir) || Dir.IsEmpty()) return;
		bTaken = true;
		Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, FString(Name) + TEXT(".png"));
		FScreenshotRequest::RequestScreenshot(Path, /*bShowUI*/ true, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogAutocraft, Display, TEXT("flowtest: flight shot %s"), *Path);
	}

	FString KeyOf(const FAcMmoClient* Mmo)
	{
		const ac::net::Client* Net = Mmo ? Mmo->Connection() : nullptr;
		return Net && Net->pilotKey() ? Utf(ac::net::pilotKeyHex(*Net->pilotKey())) : FString();
	}
}

bool UAcGameFlowSubsystem::MmoBuildCitadel()
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!U) return false;
	// Where one fits round the Prospector, as the ghost finds it: off the
	// pad's plate too, so up to the server's slack past the reach (the
	// pilot starts on the pad).
	const ac::Simulation& S = M->simulation();
	const double Reach = ac::Rules::radius(ac::StructureKind::citadel) + 2.2;
	for (double D = Reach; D <= Reach + 3.0; D += 0.5)
	{
		for (int32 K = 0; K < 16; ++K)
		{
			const double H = K * 2.0 * PI / 16;
			const std::optional<ac::Vec2> P = S.snap(ac::StructureKind::citadel, U->position + ac::Vec2(std::cos(H), std::sin(H)) * D);
			if (P && ac::distance(*P, U->position) <= Reach + 3.0 && !S.placementRefusal(ac::StructureKind::citadel, *P)
				&& !Mmo->PadPlateRefusal(ac::StructureKind::citadel, *P))
			{
				return Mmo->Build(ac::StructureKind::citadel, *P);
			}
		}
	}
	return false;
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
	const bool bResume = Step == TEXT("mmoresume");
	if (Step != TEXT("mmocross") && Step != TEXT("mmofly") && !bHand && !bResume)
	{
		Check(false, FString::Printf(TEXT("no MMORPG step %s (mmocross, mmofly, mmohandback, mmoresume)"), *Step));
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

	if (bResume)
	{
		const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
		const ac::GameState* S = M ? &M->simulation().state : nullptr;
		const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
		// Seated, the region's first snapshots in (the wallet and the buildings;
		// the pilots' side, and the Brood's where the region has a brood level).
		auto Ready = [&] { return Seated() && Mmo->Here() && U && !S->players.empty() && Since() > 1.0; };
		auto Waited = [&](const TCHAR* What) {
			if (Since() <= SeatSeconds) return false;
			Check(false, FString::Printf(TEXT("%s within %.0f s (%s)"), What, SeatSeconds, Mmo ? *Mmo->Message() : TEXT("no client")));
			Finish();
			return true;
		};
		auto PickOnHome = [&](const EAcMenuAction Action) {
			return OpenMenu(EAcMenuMode::Home) && Menu && Menu->Highlight(Action);
		};
		switch (T.Phase)
		{
		case 0:
		{
			if (Sim && Sim->IsMmo()) StopMmo();
			// The home screen's MMORPG NEW WORLD (TEST): asked first, BACK
			// leaves it be; asked again and confirmed, the mode starts.
			Check(PickOnHome(EAcMenuAction::MmoNewWorld), TEXT("the home screen has MMORPG NEW WORLD (TEST)"));
			if (!Menu)
			{
				Finish();
				return;
			}
			Menu->PickHighlighted();
			Check(Menu->ConfirmingNewWorld() && !Sim->IsMmo(), TEXT("NEW WORLD asks first"));
			Menu->PickHighlighted();
			Check(Menu && !Menu->ConfirmingNewWorld() && !Sim->IsMmo(), TEXT("BACK (the default) starts nothing"));
			Menu->PickHighlighted();
			Check(Menu->Highlight(EAcMenuAction::MmoNewWorld), TEXT("DELETE AND START OVER on the page"));
			// (The pick closes the menu: held while it runs.)
			const TSharedPtr<SAcMenu> Held = Menu;
			Held->PickHighlighted();
			Check(Sim->IsMmo() && !Menu, TEXT("confirmed: the mode starts, the menu closed"));
			Advance();
			return;
		}
		case 1:
			if (Ready())
			{
				Check(Utf(Mmo->Here()->id) == TEXT("start") && !CitadelIn(*S) && S->players[0].ore == 600,
					FString::Printf(TEXT("a new world: in %s, %lld ore, no Citadel"), *Utf(Mmo->Here()->id), (long long)S->players[0].ore));
				MmoLeft = FMmoLeft();
				MmoLeft.Key = KeyOf(Mmo);
				Check(MmoLeft.Key.Len() == 32, FString::Printf(TEXT("joined as pilot %s"), *MmoLeft.Key.Left(8)));
				Check(MmoBuildCitadel(), TEXT("a Citadel asked for by the Prospector"));
				Advance();
			}
			else Waited(TEXT("seated"));
			return;
		case 2:
			// Its scaffold and the paid wallet; then a moment, and leave.
			if (const ac::Structure* C = S ? CitadelIn(*S) : nullptr; C && S->players[0].ore == 200 && U)
			{
				if (MmoLeft.Citadel < 0) T.PhaseAt = Now;
				MmoLeft.Citadel = C->id;
				MmoLeft.CitadelAt = FVector2D(C->position.x, C->position.y);
				MmoLeft.Ore = S->players[0].ore;
				MmoLeft.Unit = U->id;
				MmoLeft.UnitAt = FVector2D(U->position.x, U->position.y);
				MmoLeft.Region = Utf(Mmo->Here()->id);
				if (Since() < 1.0) return;
				Check(true, FString::Printf(TEXT("Citadel #%lld at %.1f,%.1f, %lld ore, Prospector #%lld at %.2f,%.2f in %s"), (long long)MmoLeft.Citadel,
					MmoLeft.CitadelAt.X, MmoLeft.CitadelAt.Y, (long long)MmoLeft.Ore, (long long)MmoLeft.Unit, MmoLeft.UnitAt.X, MmoLeft.UnitAt.Y,
					*MmoLeft.Region));
				for (const FString& Refused : Mmo->TakeBuildRefusals()) Check(false, FString::Printf(TEXT("built, not refused: %s"), *Refused));
				// QUIT TO HOME: the mode left (the server saves on its stop),
				// then MMORPG (TEST) again.
				StopMmo();
				Check(!Sim->IsMmo(), TEXT("left the mode"));
				Check(PickOnHome(EAcMenuAction::Mmo), TEXT("the home screen has MMORPG (TEST)"));
				if (const TSharedPtr<SAcMenu> Held = Menu) Held->PickHighlighted();
				Check(Sim->IsMmo(), TEXT("MMORPG (TEST) resumes"));
				Advance();
			}
			else Waited(TEXT("the Citadel shown"));
			return;
		case 3:
			if (Ready() && CitadelIn(*S))
			{
				const ac::Structure* C = CitadelIn(*S);
				Check(KeyOf(Mmo) == MmoLeft.Key, FString::Printf(TEXT("the same pilot %s"), *KeyOf(Mmo).Left(8)));
				Check(Utf(Mmo->Here()->id) == MmoLeft.Region, FString::Printf(TEXT("back in %s"), *Utf(Mmo->Here()->id)));
				Check(C->id == MmoLeft.Citadel && FVector2D::Distance(FVector2D(C->position.x, C->position.y), MmoLeft.CitadelAt) < 0.01,
					FString::Printf(TEXT("the Citadel #%lld back at %.1f,%.1f"), (long long)C->id, C->position.x, C->position.y));
				Check(S->players[0].ore == MmoLeft.Ore, FString::Printf(TEXT("the wallet back: %lld ore"), (long long)S->players[0].ore));
				const double Off = FVector2D::Distance(FVector2D(U->position.x, U->position.y), MmoLeft.UnitAt);
				Check(U->id == MmoLeft.Unit && Off < 0.05, FString::Printf(TEXT("driving the same Prospector #%lld on the same spot (%.3f cells off)"),
					(long long)U->id, Off));
				Check(Mmo->TakesSent() == 0, TEXT("seated by the server"));
				// Started over: the same pilot, new.
				StopMmo();
				StartMmo(true);
				Advance();
			}
			else Waited(TEXT("seated with the Citadel back"));
			return;
		case 4:
			if (Ready())
			{
				// The region file's start pad (F2: `NearestPad` is the Landing Pads').
				std::optional<ac::region::PadNear> Near;
				if (const ac::net::Mirror* Mr = Mmo->Mirror(); Mr && Mr->driven() && Mmo->Here())
					if (const ac::Unit* Me = Mr->unit(*Mr->driven()))
						Near = ac::region::nearestPad(ac::net::padsOnGround(*Mmo->Here(), Mr->simulation().map->bounds), Me->position);
				Check(!CitadelIn(*S), TEXT("NEW WORLD: no Citadel"));
				Check(S->players[0].ore == 600, FString::Printf(TEXT("NEW WORLD: %lld ore"), (long long)S->players[0].ore));
				Check(Utf(Mmo->Here()->id) == TEXT("start") && Near && Near->distance <= FAcMmoClient::PadReach,
					FString::Printf(TEXT("NEW WORLD: at the start pad (%s, %.1f cells)"), *Utf(Mmo->Here()->id), Near ? Near->distance : -1.0));
				Check(KeyOf(Mmo) == MmoLeft.Key, TEXT("NEW WORLD: the same pilot key"));
				Finish();
			}
			else Waited(TEXT("seated in the new world"));
			return;
		default:
			Finish();
			return;
		}
	}

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

	// The wallet shown from setting out till seated in the new region.
	if (T.Phase >= 2 && T.Phase <= 5 && WalletSeen.From >= 0)
	{
		if (const SAcResourceBar* Bar = WalletBar(this))
		{
			WalletSeen.Lowest = WalletSeen.Lowest < 0 ? Bar->GetShownOre() : FMath::Min(WalletSeen.Lowest, Bar->GetShownOre());
		}
	}

	if (bFly && Pilot && (T.Phase == 3 || T.Phase == 4))
	{
		// The flight as shown: each stage first seen, the fade, a picture of
		// four stages with -AcMmoFlightShots=DIR.
		FFlightSeen& F = FlightSeen;
		const int32 St = int32(Pilot->FlightStage());
		if (St > 0 && St < 8 && F.Seen[St] < 0) F.Seen[St] = Now;
		if (const APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager.Get() : nullptr; Manager && Manager->bEnableFading)
			F.MostFade = FMath::Max(F.MostFade, double(Manager->FadeAmount));
		static const TCHAR* Names[8] = {nullptr, TEXT("1-descend"), nullptr, TEXT("2-lift"), nullptr, TEXT("3-dash"), TEXT("4-approach"), nullptr};
		if (St > 0 && St < 8 && Names[St] && Now - F.Seen[St] > 0.8) FlightShot(F.bShot[St], Names[St]);
	}

	switch (T.Phase)
	{
	case 0:
		if (Sim && Sim->IsMmo()) StopMmo();
		// From the start pad: a new world.
		StartMmo(true);
		Advance();
		return;
	case 1:
		if (Seated() && Mmo->Here())
		{
			Check(Utf(Mmo->Here()->id) == TEXT("start"), FString::Printf(TEXT("seated in Prospector #%lld in %s"), (long long)*Mmo->Driven(),
				*Utf(Mmo->Here()->id)));
			MmoVisitWas = Mmo->Visit();
			Check(Mmo->TakesSent() == 0, FString::Printf(TEXT("seated by the server, no take sent (%d)"), Mmo->TakesSent()));
			WalletSeen = FWalletSeen();
			if (const SAcResourceBar* Bar = WalletBar(this); Bar && Mmo->Mirror())
			{
				// The stock as the HUD shows it once it caught up.
				if (Bar->GetShownOre() != Mmo->Mirror()->simulation().state.players[0].ore && Since() < SeatSeconds) return;
				WalletSeen.From = Bar->GetShownOre();
				WalletSeen.Lowest = WalletSeen.From;
				WalletSeen.GainsFrom = Bar->GetGainsFlashed();
			}
			Check(WalletSeen.From > 0, FString::Printf(TEXT("the HUD shows the wallet, %lld ore"), (long long)WalletSeen.From));
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
			// The dive into the unit over first: the flight leaves the pilot's view.
			if (Pilot && Pilot->InDive() && Since() < ArriveSeconds) return;
			// A Landing Pad under the pilot and one in Cold Fork (F2: the
			// realm's buildings; the dev lines put them up finished).
			if (!FlightSeen.bPadsAsked)
			{
				FlightSeen = FFlightSeen();
				FlightSeen.bPadsAsked = true;
				Check(Mmo->DevClock(TEXT("ferry pad here\nferry pad cold-fork")), TEXT("Landing Pads here and in Cold Fork asked for"));
				return;
			}
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			bool bThere = false;
			if (const ac::net::WorldMessage* W = Mmo->World())
				for (const ac::net::WorldMessage::Region& G : W->regions)
					if (Utf(G.id) == Into)
						for (const ac::net::WorldMessage::Pad& Q : G.pads) bThere |= Q.building.has_value();
			if (!(Near && Near->Within() && bThere))
			{
				if (Since() > 10)
				{
					Check(false, TEXT("on a Landing Pad, one in Cold Fork, within 10 s"));
					Finish();
				}
				return;
			}
			// M, a Landing Pad picked, FLY: as the player does it.
			ToggleWorldMap();
			const TSharedPtr<SAcWorldMap> Map = WorldMapWidget();
			Check(Map.IsValid(), TEXT("M opened the world map"));
			if (!Map)
			{
				Finish();
				return;
			}
			Check(!Map->PickPad(Into, TEXT("landing")), TEXT("the region file's pad is not flown to"));
			Check(Map->PickPad(Into, TEXT("Landing Pad 1")), TEXT("picked Cold Fork's Landing Pad"));
			// Slow frames from here: the RegionChange and the new region's
			// first snapshot are read in the same frame (the hand check's case).
			Mmo->TestReadEvery = 0.2;
			FlightSeen.SeamlessBefore = Sim->SeamlessChanges();
			Map->Fly();
			Check(!WorldMapWidget().IsValid(), TEXT("FLY asked for the flight and closed the map"));
			Advance();
			return;
		}
		// East along the pad's row, round the well at (128, 50) on its north
		// side, to the east edge's passage (41–55, W4): region cells.
		if (const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr; M && M->driven() && Pilot)
		{
			if (const ac::Unit* U = M->unit(*M->driven()))
			{
				const ac::GroundRect& B = M->simulation().map->bounds;
				const double X = U->position.x - B.minX;
				const ac::Vec2 To = X < 117 ? ac::Vec2(118, 48) : X < 123 ? ac::Vec2(124, 44) : ac::Vec2(B.maxX - B.minX + 4, 44);
				Pilot->SetTestInput(FVector2D(0, -1), std::atan2(B.minZ + To.y - U->position.y, B.minX + To.x - U->position.x));
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
		// (A flight: once it stepped out, control back.)
		if (Seated() && Since() > 0.5 && ViewOff() < 12.0 && !(bFly && Pilot->InFlight()))
		{
			Advance();
			return;
		}
		if (Since() > (bFly ? TravelSeconds : ArriveSeconds))
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
			// The wallet over the travel: never shown lower, no gain flashed.
			const SAcResourceBar& Bar = *Hud->Root()->ResourceBar();
			Check(WalletSeen.Lowest >= WalletSeen.From && Bar.GetShownOre() == WalletSeen.From,
				FString::Printf(TEXT("the wallet shown held %lld ore over the travel (lowest %lld, now %lld)"), (long long)WalletSeen.From,
					(long long)WalletSeen.Lowest, (long long)Bar.GetShownOre()));
			Check(Bar.GetGainsFlashed() == WalletSeen.GainsFrom,
				FString::Printf(TEXT("no gain flashed over the travel (%d)"), Bar.GetGainsFlashed() - WalletSeen.GainsFrom));
		}
		else
		{
			Check(false, TEXT("a HUD"));
		}
		if (bFly)
		{
			const std::optional<FAcMmoClient::FPadNear> Near = Mmo->NearestPad();
			Check(Near && Near->Within(), TEXT("on Cold Fork's Landing Pad"));
			// The flight shown (AcPilotFlight.h): every stage, in order.
			const FFlightSeen& F = FlightSeen;
			FString Stages;
			bool bAll = true, bOrder = true;
			for (int32 St = 1; St <= int32(EAcFlightStage::StepOut); ++St)
			{
				bAll &= F.Seen[St] >= 0;
				if (St > 1) bOrder &= F.Seen[St] >= F.Seen[St - 1];
				Stages += FString::Printf(TEXT(" %d:%s"), St, F.Seen[St] >= 0 ? TEXT("seen") : TEXT("missed"));
			}
			Check(bAll, FString::Printf(TEXT("the flight shown, every stage:%s"), *Stages));
			Check(bOrder, TEXT("descend, board, lift, turns, dash, approach, step out: in that order"));
			Check(F.MostFade <= 0.001, FString::Printf(TEXT("never a fade aboard (the most %.2f)"), F.MostFade));
			Check(Sim->SeamlessChanges() > F.SeamlessBefore && Mmo->LastWasSeamless(), TEXT("the approach's region change was seamless"));
			Check(!Pilot->InFlight() && Pilot->FlightStage() == EAcFlightStage::None, TEXT("stepped out: control back"));
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
