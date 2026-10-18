// The driven Prospector's repair line shows what a repair costs (docs/balance.md
// "Repairs"), and why it can't go on when the bank is empty. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Hud
#include "AcPilotText.h"

#include "Rules.h"
#include "Simulation.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcRepairLinePriceTest, "Autocraft.Hud.RepairLineShowsThePrice", Flags)
bool FAcRepairLinePriceTest::RunTest(const FString&)
{
	// Blue alone: a Citadel at half its bar, a driven Prospector at its west
	// face looking at it, R held.
	ac::GameState S;
	S.players.assign(1, ac::Player{});
	S.score.assign(1, 0);
	S.players[0].ore = 1000;
	S.nextID = 1;
	const ac::Vec2 At(40, 40);
	ac::Structure C(S.nextID++, ac::StructureKind::citadel, 0, At);
	C.hp = ac::Rules::hp(C.kind) / 2;
	S.structures.push_back(C);
	S.units.push_back(ac::Unit(S.nextID++, ac::UnitKind::prospector, 0,
		At - ac::Vec2(ac::Rules::radius(C.kind) + 0.6, 0), 0.0, ac::Unit::Task::idle));
	ac::Simulation Sim(S, std::nullopt);
	if (!TestTrue(TEXT("a Prospector to drive"), Sim.take(S.units[0].id))) return false;
	Sim.pilot->heading = 0;
	Sim.pilot->mend = true;
	auto Prompt = [&]() -> FString {
		const TOptional<FAcPilotFrame> F = FAcPilotFrame::Of(Sim);
		if (!F) return FString();
		const FAcPilotInfo Info = AcPilotText::Info(Sim, *F, FAcPilotInputs());
		return Info.Prompt.Get(FString());
	};

	// 100 HP of a 1500-HP Citadel: 100 / 1500 × half its 400 ore, 13.3.
	const FString Priced = Prompt();
	AddInfo(Priced);
	TestTrue(TEXT("the repair line"), Priced.Contains(TEXT("repair")));
	TestTrue(TEXT("with its price per 100 HP"), Priced.EndsWith(TEXT("13 ore / 100 HP")));
	TestEqual(TEXT("the price alone"), AcPilotText::RepairPrice(Sim, *Sim.pilotUnit(), C.id).Get(FString()), FString(TEXT("13 ore / 100 HP")));
	// A building that costs MH shows both.
	ac::Structure Lab(S.nextID + 10, ac::StructureKind::lab, 0, At + ac::Vec2(0, 20));
	Sim.state.structures.push_back(Lab);
	const double LabOre = 100.0 / ac::Rules::hp(Lab.kind) * ac::Rules::repairShare * ac::Rules::cost(Lab.kind);
	const double LabMh = 100.0 / ac::Rules::hp(Lab.kind) * ac::Rules::repairShare * ac::Rules::hydrogenCost(Lab.kind);
	const FString LabPrice = AcPilotText::RepairPrice(Sim, *Sim.pilotUnit(), Lab.id).Get(FString());
	AddInfo(LabPrice);
	TestTrue(TEXT("ore and MH"), LabPrice.Contains(TEXT(" ore ")) && LabPrice.EndsWith(TEXT(" MH / 100 HP")) && LabOre > 0 && LabMh > 0);

	// Broke: the refusal instead.
	Sim.state.players[0].ore = 0;
	const FString Broke = Prompt();
	AddInfo(Broke);
	TestEqual(TEXT("no ore: the refusal"), Broke, FString(TEXT("No ore to repair")));
	return true;
}

#endif
