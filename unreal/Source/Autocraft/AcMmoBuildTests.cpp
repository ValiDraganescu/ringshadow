// Building in the MMORPG mode (AcMmo.h `Build`, AcPilotBuild.cpp) against the
// real local server on the first world: the pilot starts at the start pad
// in a Prospector with 600 ore; a spot where a Citadel fits ahead of it, as
// the ghost finds it on the mirror (`snap`, `placementRefusal`); the Build
// request; the scaffold and the Citadel's 400 of the wallet's 600 ore spent show in the mirror;
// the welding Prospector's work ring and dashboard show the Citadel's progress, rising.
// A spot on the nearest ore, out of the Prospector's reach from the pad, is
// refused by the server with its reason. One isolated
// moment of a few seconds, in real time. Run hidden as AcMmoTests.cpp says
// (`Automation RunTests Autocraft.Mmo`).
#include "AcMmo.h"
#include "AcPilotText.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "RegionFile.h"
#include "Rules.h"
#include "Simulation.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	template <class F> bool RunUntil(FAcMmoClient& Client, F&& Done, const double Seconds)
	{
		const double End = FPlatformTime::Seconds() + Seconds;
		while (FPlatformTime::Seconds() < End)
		{
			Client.Tick(FPlatformTime::Seconds());
			if (Done()) return true;
			FPlatformProcess::Sleep(1.0f / 120.0f);
		}
		return false;
	}

	const ac::Structure* CitadelIn(const ac::GameState& S)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (B.kind == ac::StructureKind::citadel && B.owner == ac::Pilot::player) return &B;
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoBuildTest, "Autocraft.Mmo.BuildCitadel", Flags)
bool FAcMmoBuildTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	// It builds at once: every blueprint (the server's --dev-blueprints).
	if (!FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevBlueprints"))) FCommandLine::Append(TEXT(" -AcMmoDevBlueprints"));
	if (!Client.Start(FPlatformTime::Seconds()))
	{
		AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
		return false;
	}
	const bool bDriving = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed
			|| (Client.Driven() && Client.Mirror() && Client.Mirror()->unit(*Client.Driven()));
	}, 15.0);
	if (!bDriving || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("no Prospector driven: phase %d, %s"), (int32)Client.Phase(), *Client.Message()));
		return false;
	}
	ac::net::Mirror& M = *Client.Mirror();
	const ac::Simulation& S = M.simulation();
	TestEqual(TEXT("the wallet starts at 600 ore"), (int64)S.state.players[0].ore, (int64)600);
	TestTrue(TEXT("no buildings yet"), CitadelIn(S.state) == nullptr);
	const ac::Unit U = *M.unit(*Client.Driven());

	// Refused by the server: on the nearest ore deposit, out of reach (the
	// ghost would refuse it too; the request goes anyway, as a stale
	// mirror's would).
	if (!S.state.patches.empty())
	{
		const ac::OreDeposit* Near = &S.state.patches[0];
		for (const ac::OreDeposit& P : S.state.patches)
		{
			if (ac::distance(P.position, U.position) < ac::distance(Near->position, U.position)) Near = &P;
		}
		TestTrue(TEXT("asked on the ore"), Client.Build(ac::StructureKind::citadel, Near->position));
		TArray<FString> Refused;
		RunUntil(Client, [&] {
			Refused.Append(Client.TakeBuildRefusals());
			return Refused.Num() > 0;
		}, 3.0);
		TestEqual(TEXT("one refusal"), Refused.Num(), 1);
		if (Refused.Num() > 0) AddInfo(FString::Printf(TEXT("refused: %s"), *Refused[0]));
		TestTrue(TEXT("still no Citadel"), CitadelIn(S.state) == nullptr);
	}

	// Where a Citadel fits round the Prospector, as the ghost finds it: the
	// ground clear and off the pad's plate (the client's rule, the server's
	// too), within the server's slack past the reach. The first spot that
	// fits but for the plate is refused by both with the same reason.
	const double Reach = ac::Rules::radius(ac::StructureKind::citadel) + 2.2;
	std::optional<ac::Vec2> At, OnPlate;
	for (double D = Reach; D <= Reach + 3.0 && !At; D += 0.5)
	{
		for (int32 K = 0; K < 16 && !At; ++K)
		{
			const double H = K * 2.0 * PI / 16;
			const std::optional<ac::Vec2> P = S.snap(ac::StructureKind::citadel, U.position + ac::Vec2(std::cos(H), std::sin(H)) * D);
			if (!P || ac::distance(*P, U.position) > Reach + 3.0 || S.placementRefusal(ac::StructureKind::citadel, *P)) continue;
			if (!Client.PadPlateRefusal(ac::StructureKind::citadel, *P)) At = P;
			else if (!OnPlate) OnPlate = P;
		}
	}
	if (!At || !OnPlate)
	{
		AddError(TEXT("no spot for a Citadel round the start pad, on and off its plate"));
		return false;
	}
	{
		const std::optional<std::string> Why = Client.PadPlateRefusal(ac::StructureKind::citadel, *OnPlate);
		TestTrue(TEXT("the client says why on the plate"), Why && *Why == ac::region::padPlateRefusal);
		TestTrue(TEXT("asked on the plate"), Client.Build(ac::StructureKind::citadel, *OnPlate));
		TArray<FString> Refused;
		RunUntil(Client, [&] {
			Refused.Append(Client.TakeBuildRefusals());
			return !Refused.IsEmpty();
		}, 3.0);
		TestTrue(FString::Printf(TEXT("the server refuses the plate the same (%s)"), Refused.IsEmpty() ? TEXT("-") : *Refused[0]),
			Refused.Num() == 1 && Refused[0] == FString(UTF8_TO_TCHAR(ac::region::padPlateRefusal)));
		TestTrue(TEXT("no Citadel on the plate"), CitadelIn(S.state) == nullptr);
	}
	TestTrue(TEXT("asked"), Client.Build(ac::StructureKind::citadel, *At));
	const bool bShown = RunUntil(Client, [&] { return CitadelIn(S.state) && S.state.players[0].ore == 200; }, 3.0);
	TestTrue(TEXT("the Citadel's scaffold and the paid wallet show in the mirror"), bShown);
	if (const ac::Structure* C = CitadelIn(S.state))
	{
		TestTrue(TEXT("where it was asked"), ac::distance(C->position, *At) < 0.01);
		TestTrue(TEXT("going up"), !C->complete());
		AddInfo(FString::Printf(TEXT("Citadel #%lld at %.1f,%.1f, wallet %lld ore"), (long long)C->id, C->position.x, C->position.y,
			(long long)S.state.players[0].ore));
	}
	// Welding up close: the pilot's work ring and dashboard show the
	// building's progress (docs/mmorpg-m5.md, E3), rising.
	auto Weld = [&]() -> TOptional<double> {
		const TOptional<FAcPilotFrame> F = FAcPilotFrame::Of(S);
		if (!F) return {};
		const FAcPilotInfo Info = AcPilotText::Info(S, *F, FAcPilotInputs());
		if (!Info.Progress || Info.ProgressLabel != TEXT("CITADEL")) return {};
		TestTrue(TEXT("the dashboard shows it too"), Info.Console && Info.Console->Progress.IsSet()
			&& Info.Console->ProgressLabel.Get(TEXT("")) == TEXT("CITADEL"));
		return Info.Progress;
	};
	TOptional<double> WeldFrom;
	RunUntil(Client, [&] { WeldFrom = Weld(); return WeldFrom.IsSet(); }, 3.0);
	TestTrue(TEXT("the overlay shows CITADEL n% while welding"), WeldFrom.IsSet());
	if (WeldFrom)
	{
		TOptional<double> WeldTo;
		RunUntil(Client, [&] { WeldTo = Weld(); return WeldTo && *WeldTo > *WeldFrom + 0.01; }, 4.0);
		TestTrue(FString::Printf(TEXT("its progress rises (%.0f%% -> %.0f%%)"), *WeldFrom * 100, WeldTo.Get(-1) * 100),
			WeldTo && *WeldTo > *WeldFrom + 0.01);
	}
	TestTrue(TEXT("no refusal"), Client.TakeBuildRefusals().Num() == 0);
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	Client.Stop();
	return true;
}

#endif
