// The walk-in Dropship's act key (AcShuttleAct.h): on foot by proximity;
// driving a Mule, a ship's button or a Citadel's door only with the
// crosshair on it (`DriverAim`), else nothing here (E climbs off).
//
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Pilot.ShuttleAct
#include "AcCitadelAct.h"
#include "AcShuttleAct.h"

#include "Misc/AutomationTest.h"

#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"
#include "Types.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// The aim put back after a test.
	struct FAimReset
	{
		~FAimReset() { AcShuttleAct::SetDriverAim({}); }
	};

	ac::Simulation WithShip(const double RampOpen)
	{
		ac::GameState G;
		G.players = {ac::Player{}, ac::Player{}};
		G.score = {0, 0};
		G.nextID = 10;
		ac::Shuttle S;
		S.id = 1;
		S.position = ac::Vec2(30, 30);
		S.stage = ac::Shuttle::Stage::ground;
		S.ramp = ac::Door{RampOpen, RampOpen > 0};
		G.shuttles.push_back(S);
		return ac::Simulation(G, std::nullopt);
	}

	FString PromptOf(const TOptional<AcShuttleAct::FAct>& A) { return A ? A->Prompt() : FString(TEXT("(none)")); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcShuttleActTest, "Autocraft.Pilot.ShuttleAct", Flags)
bool FAcShuttleActTest::RunTest(const FString&)
{
	const FAimReset Reset;
	ac::Simulation Sim = WithShip(1.0);
	const ac::Shuttle& Ship = Sim.state.shuttles.front();
	auto At = [&](const ac::Vec2 Local) { return ac::ShuttleBay::world(Ship, Local); };

	// On foot: by the ramp post, by the console.
	const ac::Unit Post(9, ac::UnitKind::prospector, 0, At(ac::Vec2(-1.6, -0.4)), 0, ac::Unit::Task::idle);
	TestEqual(TEXT("on foot at the post, the ramp down"), PromptOf(AcShuttleAct::Find(Sim, Post)), FString(TEXT("E · CLOSE THE RAMP")));
	const ac::Unit Console(9, ac::UnitKind::prospector, 0, At(ac::Vec2(1.9, 0.0)), 0, ac::Unit::Task::idle);
	TestEqual(TEXT("on foot at the console, the ramp down"), PromptOf(AcShuttleAct::Find(Sim, Console)), FString(TEXT("Close the ramp first")));

	// Driving a Mule at the back of the bay: nothing without the crosshair on a button.
	const ac::Unit Mule(11, ac::UnitKind::mule, 0, At(ac::Vec2(-0.95, 0.0)), 0, ac::Unit::Task::idle);
	AcShuttleAct::SetDriverAim({});
	TestFalse(TEXT("driving, the crosshair off the buttons: no ship act (E climbs off)"), AcShuttleAct::Find(Sim, Mule).IsSet());
	AcShuttleAct::FDriverAim OnRamp;
	OnRamp.bRampButton = true;
	AcShuttleAct::SetDriverAim(OnRamp);
	const TOptional<AcShuttleAct::FAct> A = AcShuttleAct::Find(Sim, Mule);
	TestTrue(TEXT("driving, the crosshair on the ramp post: the ramp"), A && A->Kind == AcShuttleAct::FAct::EKind::Ramp && !A->bOpen);
	AddInfo(FString::Printf(TEXT("driving at the post: \"%s\""), *PromptOf(A)));
	AcShuttleAct::FDriverAim OnTakeOff;
	OnTakeOff.bTakeOffButton = true;
	AcShuttleAct::SetDriverAim(OnTakeOff);
	const TOptional<AcShuttleAct::FAct> B = AcShuttleAct::Find(Sim, Mule);
	TestTrue(TEXT("driving, the crosshair on the take-off button: take off"), B && B->Kind == AcShuttleAct::FAct::EKind::TakeOff);

	// The Citadel's door: driving, only with the crosshair on it.
	const bool bHallWas = ac::Rules::citadelHall();
	ac::Rules::setCitadelHall(true);
	ac::GameState G;
	G.players = {ac::Player{}, ac::Player{}};
	G.score = {0, 0};
	G.nextID = 10;
	ac::Structure C(7, ac::StructureKind::citadel, 0, ac::Vec2(20, 20));
	C.door = ac::Door{};
	G.structures.push_back(C);
	ac::Simulation Hall(G, std::nullopt);
	const ac::Unit AtDoor(11, ac::UnitKind::mule, 0, ac::Vec2(20, 20) + ac::Vec2(0, 8.0), -0.5 * UE_DOUBLE_PI, ac::Unit::Task::idle);
	AcShuttleAct::SetDriverAim({});
	TestFalse(TEXT("driving at the gate, the crosshair off it: no door act"), AcCitadelAct::Find(Hall, AtDoor).IsSet());
	AcShuttleAct::FDriverAim OnDoor;
	OnDoor.bDoor = true;
	AcShuttleAct::SetDriverAim(OnDoor);
	const TOptional<AcCitadelAct::FAct> D = AcCitadelAct::Find(Hall, AtDoor);
	TestTrue(FString::Printf(TEXT("driving at the gate, the crosshair on it: the door (\"%s\")"), D ? *D->Prompt() : TEXT("none")),
		D && D->Kind == AcCitadelAct::FAct::EKind::Door);
	const ac::Unit OnFoot(9, ac::UnitKind::prospector, 0, ac::Vec2(20, 20) + ac::Vec2(0, 7.2), -0.5 * UE_DOUBLE_PI, ac::Unit::Task::idle);
	AcShuttleAct::SetDriverAim({});
	TestTrue(TEXT("on foot at the gate: the door, aim or not"), AcCitadelAct::Find(Hall, OnFoot).IsSet());
	ac::Rules::setCitadelHall(bHallWas);
	return true;
}

#endif
