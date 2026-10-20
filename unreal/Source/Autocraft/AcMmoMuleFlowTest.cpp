// -AcFlowTest's Mule step (C4, docs/haul-and-ferry.md section 2; the other
// MMORPG steps are in AcMmoFlowTest.cpp and beside it), over the local server
// on the first world with the dev rig (a full wallet; build the server first:
// make -C unreal/server). It starts a new world and waits to be seated in the
// Prospector on the start pad.
//
//   mmomule   The server's dev line puts a finished Foundry ahead (`mule
//             foundry`, the pilot by its call post). The crosshair on the post's
//             button (C5: E works only what it is on, in reach), "E · BUY MULE": E opens
//             the Foundry's menu (or buys) and E again buys: 600 ore and 150 MH leave
//             the wallet, the Progress message tells the Mule rolling out;
//             about 20 s later it is out of the Foundry's door. Then the
//             staging: a Citadel ahead (`citadel here`), the Mule set outside
//             its open door facing in, the pilot by its hopper (`mule door`),
//             25 ore in the gripper (`carry ore 25`). At the hopper, the
//             crosshair on it, "E · FILL THE MULE": E puts the load in (the
//             Mule holds 25, the wallet unchanged). At the perch, the
//             crosshair on it, "E · CLIMB ON": E, and the
//             pilot drives the Mule. Driven straight ahead it goes in through
//             the door; in the hall it books: the wallet rises by the load
//             less the realm's cut, the Mule empty. Nothing under the
//             crosshair, "E · CLIMB OFF": E, and the
//             pilot is on foot in its Prospector again. Every move through the
//             pawn's own hooks (the act key E, the move keys, the look).
//
// Run hidden in a game process (ringshadow-testing, "Flow tests"):
//   uv run unreal/Tools/hidden_run.py --out DIR --headless -- -AcFlowTest=mmomule
#include "AcGameFlow.h"

#include "AcActPick.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotOverlay.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"

#include "Act.h"
#include "Mule.h"
#include "Progression.h"
#include "Rules.h"
#include "Simulation.h"

#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include <cmath>

namespace
{
	constexpr double SeatSeconds = 20;
	constexpr double AskSeconds = 8;
	constexpr double WalkSeconds = 20;
	/// The roll-out (20 s) and some for the snapshots.
	constexpr double RollOutSeconds = 30;
	/// The drive from the door into the hall at 1.18 cells a second.
	constexpr double DriveSeconds = 15;
	constexpr int64 Load = 25;

	/// The step's own memory.
	struct FMuleFlow
	{
		double Settled = -1;
		bool bPressed = false;
		bool bAsked = false;
		int64 Foundry = -1;
		int64 Citadel = -1;
		int64 Mule = -1;
		int64 Prospector = -1;
		/// The wallet before the purchase, and before the fill.
		int64 Ore = 0;
		int64 Hydrogen = 0;
		double BoughtAt = -1;
		/// The command line before the dev rig was added (put back after).
		FString CommandLineWas;
	};
	FMuleFlow MuleFlow;
}

void UAcGameFlowSubsystem::TickMmoMuleFlow(const FString& Step)
{
	FFlowTest& T = *Test;
	const double Now = FPlatformTime::Seconds();
	auto Since = [&] { return Now - T.PhaseAt; };
	auto Advance = [&] {
		T.Phase += 1;
		T.PhaseAt = Now;
		MuleFlow.Settled = -1;
		MuleFlow.bPressed = false;
		MuleFlow.bAsked = false;
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
			Pilot->Press(false, true);
		}
		StopMmo();
		// The steps after this one go without the dev rig.
		if (!MuleFlow.CommandLineWas.IsEmpty()) FCommandLine::Set(*MuleFlow.CommandLineWas);
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
	if (Step != TEXT("mmomule"))
	{
		Check(false, FString::Printf(TEXT("no Mule step %s (mmomule)"), *Step));
		NextStep();
		return;
	}
	if (T.Phase == 0)
	{
		if (Sim && Sim->IsMmo()) StopMmo();
		MuleFlow = FMuleFlow();
		// The dev rig: a full wallet (the Mule's 600 ore and 150 MH).
		if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig")))
		{
			MuleFlow.CommandLineWas = FCommandLine::Get();
			FCommandLine::Append(TEXT(" -AcMmoDevRig"));
		}
		StartMmo(true);
		Advance();
		return;
	}
	if (T.Phase == 1)
	{
		if (Seated() && Mmo->Here() && Mmo->Progress() && Since() > 1.0)
		{
			Check(Mmo->DevClock(TEXT("mule foundry")), TEXT("mule: a Foundry asked for ahead of the pilot (dev)"));
			Advance();
		}
		else Waited(TEXT("mule: seated"), SeatSeconds);
		return;
	}
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::Simulation* S = M ? &M->simulation() : nullptr;
	const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!Seated() || !S || !U)
	{
		Waited(TEXT("mule: still driving"), 4);
		return;
	}
	const TOptional<FAcPilotInfo>& Info = Overlay ? Overlay->LastInfo() : TOptional<FAcPilotInfo>();
	const FString Prompt = Info && Info->Prompt ? *Info->Prompt : FString();
	const FString Note = Info && Info->Note ? *Info->Note : FString();
	const ac::Player& Side = S->state.players[size_t(ac::Pilot::player)];
	const ac::Unit* Mule = MuleFlow.Mule >= 0 ? M->unit(MuleFlow.Mule) : nullptr;
	auto Structure = [&](const int64 Id) -> const ac::Structure* {
		for (const ac::Structure& B : S->state.structures)
			if (B.id == Id) return &B;
		return nullptr;
	};
	// Stand still a moment (the snapshots and the prompt catch up).
	auto Settle = [&](const double Seconds) {
		if (MuleFlow.Settled < 0) MuleFlow.Settled = Now;
		return Now - MuleFlow.Settled >= Seconds;
	};
	// E, once.
	auto PressE = [&] {
		Pilot->Press(true, true);
		Pilot->Press(false, true);
		MuleFlow.bPressed = true;
	};
	// Walk to a world point: the move keys forward, the mouse's yaw onto it;
	// stopped within `Near`, facing `Face` when given. True once there.
	auto WalkTo = [&](const ac::Vec2 To, const double Near, const TOptional<ac::Vec2> Face = {}) {
		const ac::Vec2 D = To - U->position;
		if (ac::length(D) <= Near)
		{
			const ac::Vec2 F = Face ? *Face - U->position : D;
			Pilot->SetTestInput(FVector2D::ZeroVector, std::atan2(F.y, F.x));
			return true;
		}
		Pilot->SetTestInput(FVector2D(0, -1), std::atan2(D.y, D.x));
		return false;
	};
	switch (T.Phase)
	{
	case 2:
	{
		// The Foundry by the pilot; at it, "E · BUY MULE".
		const ac::Structure* F = nullptr;
		for (const ac::Structure& B : S->state.structures)
			if (B.kind == ac::StructureKind::foundry && B.complete() && ac::distance(B.position, U->position) < 12) F = &B;
		if (!F)
		{
			Waited(TEXT("mule: a finished Foundry by the pilot"), AskSeconds);
			return;
		}
		MuleFlow.Foundry = F->id;
		MuleFlow.Prospector = U->id;
		// By its call post, the crosshair on its button.
		if (WalkTo(F->position + ac::Act::foundryPostAt(ac::Rules::radius(F->kind)) + ac::Vec2(0.6, 1.0), 0.5)) Pilot->TestLookAtAct(AcActPick::EKind::Buy, F->id);
		if (!Prompt.Contains(TEXT("BUY MULE")))
		{
			Waited(FString::Printf(TEXT("mule: at the Foundry \"E · BUY MULE\" (\"%s\")"), *Prompt), WalkSeconds);
			return;
		}
		if (!Settle(0.5)) return;
		Check(true, FString::Printf(TEXT("mule: at the Foundry: \"%s\""), *Prompt));
		MuleFlow.Ore = Side.ore;
		MuleFlow.Hydrogen = Side.hydrogen;
		Check(MuleFlow.Ore >= ac::Mule::ore() && MuleFlow.Hydrogen >= ac::Mule::hydrogen(),
			FString::Printf(TEXT("mule: the wallet holds its price (%lld ore, %lld MH)"), (long long)MuleFlow.Ore, (long long)MuleFlow.Hydrogen));
		PressE();
		MuleFlow.BoughtAt = Now;
		Advance();
		return;
	}
	case 3:
	{
		// Bought (a menu first: E again). The wallet pays, the Progress
		// message tells it.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		if (!MuleFlow.bAsked && Since() > 1.0 && !(P && P->mule))
		{
			MuleFlow.bAsked = true;
			Pilot->TestLookAtAct(AcActPick::EKind::Buy, MuleFlow.Foundry);
			PressE();
		}
		if (!(P && P->mule && Side.ore <= MuleFlow.Ore - ac::Mule::ore()))
		{
			Waited(FString::Printf(TEXT("mule: bought, the wallet paid (\"%s\")"), *Note), AskSeconds);
			return;
		}
		Check(Side.ore == MuleFlow.Ore - ac::Mule::ore() && Side.hydrogen == MuleFlow.Hydrogen - ac::Mule::hydrogen(),
			FString::Printf(TEXT("mule: 600 ore and 150 MH paid (%lld, %lld)"), (long long)(MuleFlow.Ore - Side.ore),
				(long long)(MuleFlow.Hydrogen - Side.hydrogen)));
		Check(P->mule->foundry == MuleFlow.Foundry && (P->mule->unit == 0) == (P->mule->readyAt > 0),
			TEXT("mule: the Progress message tells it rolling out of the Foundry"));
		Advance();
		return;
	}
	case 4:
	{
		// Out of the door about 20 s after.
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const ac::Unit* Out = P && P->mule && P->mule->unit ? M->unit(P->mule->unit) : nullptr;
		if (!Out)
		{
			Waited(TEXT("mule: rolled out of the Foundry"), RollOutSeconds);
			return;
		}
		MuleFlow.Mule = Out->id;
		const ac::Structure* F = Structure(MuleFlow.Foundry);
		const double Took = Now - MuleFlow.BoughtAt;
		Check(Took > ac::Mule::rollOut() * 0.75, FString::Printf(TEXT("mule: out after %.1f s (rolls out in %.0f)"), Took, ac::Mule::rollOut()));
		Check(Out->kind == ac::UnitKind::mule && F && ac::distance(Out->position, ac::Mule::door(F->position)) < 1.5,
			TEXT("mule: at the Foundry's door"));
		Advance();
		return;
	}
	case 5:
	{
		// Turned away from the Foundry (the Citadel goes up ahead of the
		// pilot, its door toward it: the Foundry must not stand in the way
		// in), then the staging in one go (the server reads the dev file's
		// lines together; a later write would replace an unread one): the
		// Citadel, the Mule outside its open door facing in, the pilot by
		// the hopper, 25 ore in the gripper.
		if (!MuleFlow.bAsked)
		{
			Pilot->SetTestInput(FVector2D::ZeroVector, UE_DOUBLE_PI / 2);
			if (Since() < 1.5) return;
			MuleFlow.bAsked = true;
			Check(Mmo->DevClock(TEXT("citadel here\nmule door\ncarry ore 25")),
				TEXT("mule: a Citadel ahead, the Mule staged at its open door, a load in the gripper (dev)"));
			return;
		}
		// The Citadel the Mule stands at.
		const ac::Structure* C = nullptr;
		for (const ac::Structure& B : S->state.structures)
			if (B.kind == ac::StructureKind::citadel && B.complete() && B.door && Mule && ac::distance(B.position + ac::Vec2(0, 9.5), Mule->position) < 0.5)
				C = &B;
		if (!C)
		{
			Waited(TEXT("mule: the Mule staged by a Citadel's door"), AskSeconds);
			return;
		}
		MuleFlow.Citadel = C->id;
		const bool bStaged = Mule && ac::distance(Mule->position, C->position + ac::Vec2(0, 9.5)) < 0.5
			&& U->carrying == Load && ac::Simulation::doorOpen(*C);
		if (!bStaged)
		{
			Waited(TEXT("mule: staged by the Citadel's door"), AskSeconds);
			return;
		}
		Advance();
		return;
	}
	case 6:
	{
		// At the hopper: "E · FILL THE MULE"; E puts the load in.
		if (!Mule)
		{
			Waited(TEXT("mule: the Mule in sight"), 4);
			return;
		}
		if (!MuleFlow.bPressed)
		{
			if (WalkTo(ac::Mule::hopper(*Mule) + ac::Vec2(1.2, 0), 0.6, ac::Mule::hopper(*Mule))) Pilot->TestLookAtAct(AcActPick::EKind::Fill, Mule->id);
			if (!Prompt.Contains(TEXT("FILL")))
			{
				Waited(FString::Printf(TEXT("mule: at the hopper \"E · FILL THE MULE\" (\"%s\")"), *Prompt), WalkSeconds);
				return;
			}
			if (!Settle(0.5)) return;
			Check(true, FString::Printf(TEXT("mule: at the hopper: \"%s\""), *Prompt));
			MuleFlow.Ore = Side.ore;
			PressE();
			return;
		}
		if (!(Mule->haul && Mule->haul->ore == Load && U->carrying == 0))
		{
			Waited(FString::Printf(TEXT("mule: the load in the hopper (\"%s\")"), *Note), AskSeconds);
			return;
		}
		Check(true, TEXT("mule: the Mule holds the 25 ore, the gripper empty"));
		Check(Side.ore == MuleFlow.Ore, FString::Printf(TEXT("mule: the wallet unchanged by the fill (%lld)"), (long long)(Side.ore - MuleFlow.Ore)));
		Advance();
		return;
	}
	case 7:
	{
		// At the perch: "E · CLIMB ON"; E, and the pilot drives the Mule.
		if (!Mule)
		{
			Waited(TEXT("mule: the Mule in sight"), 4);
			return;
		}
		if (!MuleFlow.bPressed)
		{
			// Beside the perch on the side the pilot stands (from the hopper
			// it walks along the Mule's flank, never across its body).
			const ac::Vec2 Perch = ac::Mule::perch(*Mule);
			const ac::Vec2 Across = ac::Mule::point(*Mule, ac::Vec2(0, 1)) - Mule->position;
			const double Lateral = ac::dot(U->position - Mule->position, Across) >= 0 ? 1.4 : -1.4;
			if (WalkTo(ac::Mule::point(*Mule, ac::Vec2(ac::Mule::perchLocal.x, Lateral)), 0.6, Perch)) Pilot->TestLookAtAct(AcActPick::EKind::Board, Mule->id);
			if (!Prompt.Contains(TEXT("CLIMB ON")))
			{
				Waited(FString::Printf(TEXT("mule: at the perch \"E · CLIMB ON\" (\"%s\", %.2f cells from the perch, task %d)"), *Prompt,
						   ac::distance(U->position, Perch), int(U->task)),
					WalkSeconds);
				return;
			}
			if (!Settle(0.5)) return;
			Check(true, FString::Printf(TEXT("mule: at the perch: \"%s\""), *Prompt));
			PressE();
			return;
		}
		if (U->id != MuleFlow.Mule)
		{
			Waited(FString::Printf(TEXT("mule: driving the Mule (\"%s\")"), *Note), AskSeconds);
			return;
		}
		const ac::Unit* Rider = M->unit(MuleFlow.Prospector);
		Check(Rider && Rider->task == ac::Unit::Task::aboard && Rider->structure == MuleFlow.Mule, TEXT("mule: the Prospector on its perch"));
		Advance();
		return;
	}
	case 8:
	{
		// Straight ahead, in through the door; in the hall the load books.
		const ac::Structure* C = Structure(MuleFlow.Citadel);
		if (!C)
		{
			Waited(TEXT("mule: the Citadel"), 4);
			return;
		}
		const bool bIn = ac::Rules::footprint(C->kind).inside(U->position - C->position);
		const bool bBooked = U->haul && U->haul->total() == 0;
		if (!(bIn && bBooked))
		{
			// The look along the hull: the move key then drives it ahead (a
			// look off to the side would turn it).
			Pilot->SetTestInput(FVector2D(0, -1), U->heading);
			Waited(FString::Printf(TEXT("mule: driven into the hall, booked (%.1f cells from its middle)"), ac::distance(U->position, C->position)),
				DriveSeconds);
			return;
		}
		Pilot->SetTestInput(FVector2D::ZeroVector, Pilot->Yaw());
		const int64 Cut = ac::Progression::realmTax(Load);
		if (!(Side.ore >= MuleFlow.Ore + Load - Cut))
		{
			Waited(TEXT("mule: the wallet rises"), 4);
			return;
		}
		Check(Side.ore == MuleFlow.Ore + Load - Cut,
			FString::Printf(TEXT("mule: in the hall the wallet rose by %lld (25 less the realm's %lld)"), (long long)(Side.ore - MuleFlow.Ore),
				(long long)Cut));
		Advance();
		return;
	}
	case 9:
	{
		// Nothing under the crosshair in reach (level, along the hull):
		// "E · CLIMB OFF": E, and the pilot is on foot again.
		if (!MuleFlow.bPressed)
		{
			if (U->kind == ac::UnitKind::mule)
			{
				Pilot->SetTestInput(FVector2D::ZeroVector, U->heading);
				Pilot->Look(0.0, Pilot->ViewPitch() / AAcPilotPawn::PitchPerPoint);
			}
			if (!Prompt.Contains(TEXT("CLIMB OFF")))
			{
				Waited(FString::Printf(TEXT("mule: aboard \"E · CLIMB OFF\" (\"%s\")"), *Prompt), AskSeconds);
				return;
			}
			if (!Settle(0.5)) return;
			Check(true, FString::Printf(TEXT("mule: aboard: \"%s\""), *Prompt));
			PressE();
			return;
		}
		if (U->id != MuleFlow.Prospector)
		{
			Waited(TEXT("mule: on foot in the Prospector again"), AskSeconds);
			return;
		}
		Check(U->task != ac::Unit::Task::aboard, TEXT("mule: on foot in the Prospector again"));
		Finish();
		return;
	}
	default:
		Finish();
		return;
	}
}
