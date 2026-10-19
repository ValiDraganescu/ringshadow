// The act key's one rule (C5, AcActPick.h, docs/acting.md): E acts only with
// the crosshair's ray on the thing's box and the body within reach of it.
// Aimed at a button in reach: its act; aimed past it: none; aimed at it from
// too far: none; a door, a gun's console and a Mule side by side: each only
// when aimed at; driving a Mule, nothing aimed: climb off. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Pilot.ActPick
#include "AcActPick.h"

#include "AcSpace.h"

#include "Misc/AutomationTest.h"

#include "Act.h"
#include "Defence.h"
#include "Mule.h"
#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"
#include "Types.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// The MMO's hollow Citadel and the world's origin for a test, put back after.
	struct FScope
	{
		bool bWas = ac::Rules::citadelHall();
		ac::Vec2 Origin = AcSpace::Origin();
		FScope()
		{
			ac::Rules::setCitadelHall(true);
			AcSpace::SetOrigin(ac::Vec2(0, 0));
		}
		~FScope()
		{
			ac::Rules::setCitadelHall(bWas);
			AcSpace::SetOrigin(Origin);
		}
	};

	constexpr int64 CitadelId = 7, FoundryId = 8, PadId = 9, MuleId = 20, PilotId = 21;
	const ac::Vec2 CitadelAt(40, 40), FoundryAt(70, 40), PadAt(40, 75);

	ac::Unit Fresh(const int64 Id, const ac::UnitKind Kind, const ac::Vec2 At, const double Heading)
	{
		ac::Unit U(Id, Kind, 0, At, Heading, ac::Unit::Task::idle);
		U.hp = ac::Rules::stats(Kind).hp;
		return U;
	}

	/// A Citadel (its door shut, its guns armed), a Foundry and a Landing
	/// Pad of the realm's; the pilot's Mule by the Citadel's door, its nose
	/// toward +x, its perch beside the outer door panel.
	ac::Simulation Scene()
	{
		ac::GameState G;
		G.players = {ac::Player{}, ac::Player{}};
		G.score = {0, 0};
		G.nextID = 100;
		ac::Structure C(CitadelId, ac::StructureKind::citadel, ac::Pilot::player, CitadelAt);
		C.door = ac::Door{};
		C.guns = std::array<ac::CornerGun, 4>{};
		G.structures.push_back(C);
		G.structures.push_back(ac::Structure(FoundryId, ac::StructureKind::foundry, ac::Pilot::player, FoundryAt));
		G.structures.push_back(ac::Structure(PadId, ac::StructureKind::landingPad, ac::Pilot::player, PadAt));
		G.units.push_back(Fresh(MuleId, ac::UnitKind::mule, CitadelAt + ac::Vec2(1.6, 8.8), 0));
		return ac::Simulation(G, std::nullopt);
	}

	/// What E does for `U` looking from its eye (1.4 cells up) at `At`
	/// (cells, `Height` up), the ray going on 30 cells; the world stops it
	/// at `Wall` cells along when set.
	TOptional<AcActPick::FAct> Aimed(const ac::Simulation& Sim, const ac::Unit& U, const ac::Vec2 At, const double Height,
		const TOptional<double> Wall = {})
	{
		AcActPick::FWorld W;
		W.Ground = [](ac::Vec2) { return 0.0; };
		W.Feet = 0;
		const FVector Eye = AcSpace::ToWorld(U.position, 1.4);
		const FVector Dir = (AcSpace::ToWorld(At, Height) - Eye).GetSafeNormal();
		AcActPick::FRay Ray;
		Ray.Start = Eye;
		Ray.End = Eye + Dir * AcSpace::ToCm(30);
		Ray.Blocked = Wall ? Eye + Dir * AcSpace::ToCm(*Wall) : Ray.End;
		bool bOnBox = false;
		const TOptional<AcActPick::FTarget> T = AcActPick::Pick(AcActPick::Targets(Sim, U, W), Ray, &bOnBox);
		AcMuleAct::FOwn Own;
		Own.Mule = MuleId;
		return AcActPick::Act(Sim, U, Own, T, bOnBox, INDEX_NONE, 1e9);
	}

	FString PromptOf(const TOptional<AcActPick::FAct>& A) { return A ? A->Prompt() : FString(TEXT("(none)")); }

	ac::Vec2 Mid(const ac::Block& B) { return B.centre; }
	double Up(const ac::Block& B) { return (B.z0 + B.z1) / 2; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcActPickTest, "Autocraft.Pilot.ActPick", Flags)
bool FAcActPickTest::RunTest(const FString&)
{
	const FScope Scope;
	ac::Simulation Sim = Scene();
	const ac::Structure C = *Sim.state.structure(CitadelId);
	const ac::Unit& M = Sim.state.units.front();
	const ac::Block Outer = *ac::Act::doorPanel(C, false), Inner = *ac::Act::doorPanel(C, true), Leaf = *ac::Act::doorLeaf(C);

	// Outside by the door, the Mule's perch beside it: each only when aimed at.
	const ac::Unit Out = Fresh(PilotId, ac::UnitKind::prospector, Outer.centre + ac::Vec2(0, 1.2), -0.5 * UE_DOUBLE_PI);
	TestEqual(TEXT("aimed at the outer panel in reach"), PromptOf(Aimed(Sim, Out, Mid(Outer), Up(Outer))), FString(TEXT("E · OPEN")));
	TestEqual(TEXT("aimed at the door itself"), PromptOf(Aimed(Sim, Out, Mid(Leaf), 1.5)), FString(TEXT("E · OPEN")));
	const ac::Block Perch = ac::Act::perch(M), Hopper = ac::Act::hopper(M);
	TestTrue(TEXT("the perch is in reach there too"), ac::Act::gap(Out, Perch) <= ac::Act::reach);
	TestEqual(TEXT("aimed at the Mule's perch beside the door"), PromptOf(Aimed(Sim, Out, Mid(Perch), Up(Perch))), FString(TEXT("E · CLIMB ON")));
	TestEqual(TEXT("aimed at its hopper, nothing carried"), PromptOf(Aimed(Sim, Out, Mid(Hopper), Up(Hopper))), FString(TEXT("Nothing to put in")));
	ac::Unit Carrying = Out;
	Carrying.carrying = 10;
	TestEqual(TEXT("aimed at its hopper, carrying"), PromptOf(Aimed(Sim, Carrying, Mid(Hopper), Up(Hopper))), FString(TEXT("E · FILL THE MULE")));
	TestEqual(TEXT("aimed past them all, at the sky"), PromptOf(Aimed(Sim, Out, Out.position + ac::Vec2(0, 8), 6.0)), FString(TEXT("(none)")));
	TestEqual(TEXT("aimed at the ground at its feet"), PromptOf(Aimed(Sim, Out, Out.position + ac::Vec2(0, 0.6), 0.0)), FString(TEXT("(none)")));
	TestEqual(TEXT("aimed at the panel with a wall before it"), PromptOf(Aimed(Sim, Out, Mid(Outer), Up(Outer), 0.3)), FString(TEXT("(none)")));
	// Aimed at the panel from too far: nothing (not a nearer act either).
	const ac::Unit Far = Fresh(PilotId, ac::UnitKind::prospector, Outer.centre + ac::Vec2(3.0, 0.5), UE_DOUBLE_PI);
	TestEqual(TEXT("aimed at the panel from too far"), PromptOf(Aimed(Sim, Far, Mid(Outer), Up(Outer))), FString(TEXT("(none)")));

	// In the hall at gun 1's station, the inner panel in reach too.
	const ac::Unit Hall = Fresh(PilotId, ac::UnitKind::prospector, ac::Defence::gunStation(C.position, 0), 0);
	const ac::Block Console = ac::Act::console(C, 0);
	TestTrue(TEXT("the inner panel in reach of the station"), ac::Act::gap(Hall, Inner) <= ac::Act::reach);
	TestEqual(TEXT("aimed at the console"), PromptOf(Aimed(Sim, Hall, Mid(Console), 1.0)), FString(TEXT("E · TAKE GUN 1 SE")));
	TestEqual(TEXT("aimed at the inner panel from the station"), PromptOf(Aimed(Sim, Hall, Mid(Inner), Up(Inner))), FString(TEXT("E · OPEN")));
	TestEqual(TEXT("aimed into the hall"), PromptOf(Aimed(Sim, Hall, C.position, 1.4)), FString(TEXT("(none)")));
	TestEqual(TEXT("aimed at gun 2's console across the hall"), PromptOf(Aimed(Sim, Hall, Mid(ac::Act::console(C, 1)), 1.0)), FString(TEXT("(none)")));
	{
		ac::Simulation Taken = Sim;
		(*Taken.state.structures.front().guns)[0].gunner = 99;
		TestEqual(TEXT("the gun taken"), PromptOf(Aimed(Taken, Hall, Mid(Console), 1.0)), FString(TEXT("Someone is at that gun")));
	}
	// The door open: E closes it from its panel.
	{
		ac::Simulation Open = Sim;
		Open.state.structures.front().door = ac::Door{1.0, true};
		TestEqual(TEXT("open: the inner panel closes it"), PromptOf(Aimed(Open, Hall, Mid(Inner), Up(Inner))), FString(TEXT("E · CLOSE")));
	}

	// The call posts.
	const ac::Block Buy = ac::Act::foundryPost(*Sim.state.structure(FoundryId));
	const ac::Unit AtFoundry = Fresh(PilotId, ac::UnitKind::prospector, Buy.centre + ac::Vec2(0.4, 1.0), 0);
	TestTrue(TEXT("aimed at the Foundry's post"), PromptOf(Aimed(Sim, AtFoundry, Mid(Buy), Up(Buy))).StartsWith(TEXT("E · BUY MULE")));
	const ac::Unit FarFoundry = Fresh(PilotId, ac::UnitKind::prospector, Buy.centre + ac::Vec2(0, 3.5), 0);
	TestEqual(TEXT("aimed at the Foundry's post from too far"), PromptOf(Aimed(Sim, FarFoundry, Mid(Buy), Up(Buy))), FString(TEXT("(none)")));
	const ac::Block Call = ac::Act::padPost(*Sim.state.structure(PadId));
	const ac::Unit AtPad = Fresh(PilotId, ac::UnitKind::prospector, Call.centre + ac::Vec2(-0.6, 0.9), 0);
	TestEqual(TEXT("aimed at the pad's post"), PromptOf(Aimed(Sim, AtPad, Mid(Call), Up(Call))), FString(TEXT("E · CALL THE DROPSHIP")));
	TestEqual(TEXT("aimed at the pad's deck"), PromptOf(Aimed(Sim, AtPad, PadAt, 0.1)), FString(TEXT("(none)")));

	// Driving the Mule: nothing aimed, climb off; the door aimed in reach, the door.
	{
		ac::Simulation Riding = Sim;
		ac::Unit& Mule = Riding.state.units.front();
		Mule.cargo = std::vector<int64_t>{PilotId};
		Mule.position = CitadelAt + ac::Vec2(1.6, 7.6);
		const ac::Unit Driven = Mule;
		TestEqual(TEXT("driving, aimed at the sky"), PromptOf(Aimed(Riding, Driven, Driven.position + ac::Vec2(8, 0), 4.0)), FString(TEXT("E · CLIMB OFF")));
		TestEqual(TEXT("driving, aimed at the outer panel"), PromptOf(Aimed(Riding, Driven, Mid(Outer), Up(Outer))), FString(TEXT("E · OPEN")));
		TestTrue(TEXT("driving, its own perch is no target"), !AcActPick::Targets(Riding, Driven, AcActPick::FWorld()).ContainsByPredicate(
			[](const AcActPick::FTarget& T) { return T.Kind == AcActPick::EKind::Board || T.Kind == AcActPick::EKind::Fill; }));
	}

	// In a landed Dropship's bay: its ramp post and take-off button.
	{
		ac::Simulation Ship = Sim;
		const int64 Id = Ship.callShuttle(ac::Vec2(100, 100), 0);
		ac::Shuttle& S = *Ship.shuttle(Id);
		S.stage = ac::Shuttle::Stage::ground;
		S.ramp = ac::Door{1.0, true};
		const ac::Vec3 B = ac::ShuttleBay::rampButton, T = ac::ShuttleBay::takeOffButton;
		const ac::Unit AtPost = Fresh(PilotId, ac::UnitKind::prospector, ac::ShuttleBay::world(S, ac::Vec2(B.x + 0.6, B.y + 0.8)), 0);
		Ship.state.units.push_back(AtPost);
		Ship.reindex();
		TestEqual(TEXT("in the bay, aimed at the ramp post"),
			PromptOf(Aimed(Ship, AtPost, ac::ShuttleBay::world(S, ac::Vec2(B.x, B.y)), S.ground + B.z)), FString(TEXT("E · CLOSE THE RAMP")));
		TestEqual(TEXT("in the bay, aimed at the take-off button from the ramp"),
			PromptOf(Aimed(Ship, AtPost, ac::ShuttleBay::world(S, ac::Vec2(T.x, T.y)), S.ground + T.z)), FString(TEXT("(none)")));
		const ac::Unit AtHelm = Fresh(PilotId + 1, ac::UnitKind::prospector, ac::ShuttleBay::world(S, ac::Vec2(T.x - 0.8, T.y)), 0);
		Ship.state.units.push_back(AtHelm);
		Ship.reindex();
		TestEqual(TEXT("at the console, aimed at the take-off button, the ramp down"),
			PromptOf(Aimed(Ship, AtHelm, ac::ShuttleBay::world(S, ac::Vec2(T.x, T.y)), S.ground + T.z)), FString(TEXT("Close the ramp first")));
	}

	// The box: entered from outside, from inside, missed.
	{
		AcActPick::FTarget T;
		T.Frame = FTransform(FQuat(FVector::UpVector, 0.5), FVector(100, 0, 100));
		T.Half = FVector(20, 20, 20);
		TestTrue(TEXT("through its middle"), AcActPick::Enters(T, FVector(0, 0, 100), FVector(200, 0, 100)).IsSet());
		TestEqual(TEXT("from inside: at once"), AcActPick::Enters(T, FVector(100, 0, 100), FVector(300, 0, 100)).Get(-1.0), 0.0);
		TestFalse(TEXT("over it"), AcActPick::Enters(T, FVector(0, 0, 200), FVector(200, 0, 200)).IsSet());
		TestFalse(TEXT("short of it"), AcActPick::Enters(T, FVector(0, 0, 100), FVector(50, 0, 100)).IsSet());
	}
	return true;
}

#endif
