// Automation tests for demolish in the game (C3, docs/guns-and-practice.md
// "Limits per region and demolish"): the hold of E (`AcDemolish::FHold`), the
// building it aims at and the prompt's words. Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Pilot.DemolishHold; Quit"
//     -TestExit="Automation Test Queue Empty"
#include "AcDemolish.h"

#include "Misc/AutomationTest.h"

#include "Rules.h"
#include "Types.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcDemolishHoldTest, "Autocraft.Pilot.DemolishHold", Flags)

bool FAcDemolishHoldTest::RunTest(const FString&)
{
	using AcDemolish::FHold;
	const double T = ac::Rules::demolishHold;
	TestEqual(TEXT("the hold is 1.5 s"), T, 1.5);
	// Held the whole time on the same building: done once, at 1.5 s.
	{
		FHold H;
		TestFalse(TEXT("idle"), H.Active());
		TestFalse(TEXT("idle: nothing"), H.Tick(5, 10.0).IsSet());
		H.Start(5, 10.0);
		TestTrue(TEXT("started"), H.Active());
		TestFalse(TEXT("0.5 s: not yet"), H.Tick(5, 10.5).IsSet());
		TestTrue(TEXT("a third full"), FMath::IsNearlyEqual(H.Fill(10.5), 1.0 / 3, 1e-9));
		TestFalse(TEXT("1.49 s: not yet"), H.Tick(5, 11.49).IsSet());
		const TOptional<int64> Done = H.Tick(5, 11.5);
		TestTrue(TEXT("1.5 s: done"), Done.IsSet() && *Done == 5);
		TestFalse(TEXT("then it ends"), H.Active());
		TestFalse(TEXT("once"), H.Tick(5, 12.0).IsSet());
		TestEqual(TEXT("no fill after"), H.Fill(12.0), 0.0);
	}
	// Let go early: nothing.
	{
		FHold H;
		H.Start(5, 0.0);
		TestFalse(TEXT("1 s"), H.Tick(5, 1.0).IsSet());
		H.Cancel();
		TestFalse(TEXT("let go"), H.Active());
		TestFalse(TEXT("nothing after the time"), H.Tick(5, 2.0).IsSet());
	}
	// Looking off it (another building, or nothing): cancelled, and looking
	// back does not resume it.
	{
		FHold H;
		H.Start(5, 0.0);
		TestFalse(TEXT("another building"), H.Tick(6, 0.4).IsSet());
		TestFalse(TEXT("cancelled"), H.Active());
		TestFalse(TEXT("back on it: nothing"), H.Tick(5, 2.0).IsSet());
		H.Start(5, 3.0);
		TestFalse(TEXT("nothing under the sight"), H.Tick(INDEX_NONE, 3.2).IsSet());
		TestFalse(TEXT("cancelled again"), H.Active());
	}
	// The target: the realm's building in the repair reach, not past it, not another side's.
	{
		ac::GameState S;
		S.players = {ac::Player{}, ac::Player{}};
		S.score = {0, 0};
		S.nextID = 10;
		S.structures.push_back(ac::Structure(7, ac::StructureKind::gatling, ac::Pilot::player, ac::Vec2(20, 20)));
		S.structures.push_back(ac::Structure(8, ac::StructureKind::gatling, 1, ac::Vec2(20, 30)));
		ac::Simulation Sim(S, std::nullopt);
		ac::Unit U(9, ac::UnitKind::prospector, ac::Pilot::player, ac::Vec2(20 - ac::Rules::radius(ac::StructureKind::gatling) - 0.5, 20), 0,
			ac::Unit::Task::idle);
		TestTrue(TEXT("in reach"), AcDemolish::Target(Sim, U, 7) == TOptional<int64>(7));
		TestFalse(TEXT("nothing under the sight"), AcDemolish::Target(Sim, U, INDEX_NONE).IsSet());
		TestFalse(TEXT("another side's"), AcDemolish::Target(Sim, U, 8).IsSet());
		U.position = ac::Vec2(10, 20);
		TestFalse(TEXT("out of reach"), AcDemolish::Target(Sim, U, 7).IsSet());
	}
	TestEqual(TEXT("prompt"), AcDemolish::Prompt(ac::StructureKind::gatling), FString(TEXT("HOLD E · DEMOLISH GATLING GUN · +62 ORE +12 MH")));
	TestEqual(TEXT("prompt, no MH"), AcDemolish::Prompt(ac::StructureKind::dummy), FString(TEXT("HOLD E · DEMOLISH TARGET DUMMY · +5 ORE")));
	TestEqual(TEXT("button"), AcDemolish::Prompt(ac::StructureKind::dummy, TEXT("")), FString(TEXT("DEMOLISH TARGET DUMMY · +5 ORE")));
	TestEqual(TEXT("toast"), AcDemolish::Toast(ac::StructureKind::dummy, 5, 0), FString(TEXT("Target Dummy demolished · +5 ore")));
	return true;
}

#endif
