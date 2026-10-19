// Automation tests for the minigun class's rounds in the game (C2,
// docs/guns-and-practice.md §1, §6): the client flies a round on the core's
// own path (`FAcRoundFlight`, AcRounds.h, against `Simulation::stepRounds`
// at uneven frame steps), and the gun feed's ballistic impact marker
// (`AcPilotText::BallisticImpact`; `SAcPilotOverlay` draws its ring). Run
// headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Effects.RoundFliesTheSamePathAsTheCore+Autocraft.Hud.FeedImpactMarker; Quit"
//     -TestExit="Automation Test Queue Empty"
#include "AcPilotText.h"
#include "AcRounds.h"
#include "SAcPilotOverlay.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"

#include "Rules.h"
#include "Simulation.h"
#include "Types.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	double Dist(const ac::Vec3 A, const ac::Vec3 B)
	{
		const ac::Vec3 D = A - B;
		return std::sqrt(D.x * D.x + D.y * D.y + D.z * D.z);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcEffectsRoundPathTest, "Autocraft.Effects.RoundFliesTheSamePathAsTheCore", Flags)
bool FAcEffectsRoundPathTest::RunTest(const FString&)
{
	// A round fired up and out over flat ground (no map: the ground at 0),
	// stepped by the core at uneven frame steps; the client's flight from
	// the same muzzle and velocity is where the core's round is at every step.
	ac::Simulation S(ac::GameState{});
	const ac::Vec3 Muzzle(10, 10, 6), Velocity = ac::Rules::roundVelocity(0.4, 0.12);
	S.state.time = 2.0;
	S.state.rounds.push_back(ac::Round{1, 99, 0, Muzzle, Velocity, 1.0, 2.0});
	const FAcRoundFlight F{1, 99, Muzzle, Velocity, 2.0};
	const double Steps[] = {1.0 / 60, 1.0 / 30, 1.0 / 45, 0.02, 1.0 / 120, 0.05};
	double Worst = 0;
	int32 Checked = 0;
	std::optional<ac::GameEvent::Impact> Hit;
	double Before = S.state.time;
	for (int32 K = 0; K < 400 && !Hit; ++K)
	{
		const double Dt = Steps[K % UE_ARRAY_COUNT(Steps)];
		Before = S.state.time;
		S.state.time += Dt;
		std::vector<ac::GameEvent> Events;
		S.stepRounds(Dt, Events);
		for (const ac::GameEvent& E : Events)
			if (const auto* I = E.as<ac::GameEvent::Impact>()) Hit = *I;
		if (Hit || S.state.rounds.empty()) break;
		Worst = FMath::Max(Worst, Dist(S.state.rounds[0].position, F.At(S.state.time)));
		++Checked;
	}
	TestTrue(FString::Printf(TEXT("the core flew it %d steps"), Checked), Checked > 20);
	TestTrue(FString::Printf(TEXT("the client's round is where the core's is (worst %.3g cells)"), Worst), Worst <= 1e-6);
	// It comes down on the ground, on the client's path: where the path
	// crosses z = 0 in that last step.
	TestTrue(TEXT("it struck the ground"), Hit.has_value() && Hit->what == ac::GameEvent::Impact::What::ground);
	if (Hit)
	{
		double Lo = Before - F.Born, Hi = S.state.time - F.Born;
		for (int32 B = 0; B < 60; ++B)
		{
			const double M = (Lo + Hi) / 2;
			(F.At(F.Born + M).z < 0 ? Hi : Lo) = M;
		}
		const double Off = Dist(Hit->at, F.At(F.Born + Hi));
		TestTrue(FString::Printf(TEXT("the impact is on the client's path (%.3g cells off)"), Off), Off < 0.02);
	}
	// The streak's tail is 1.5 cells back along the path, never behind the muzzle.
	const double T = F.Born + 0.5;
	TestTrue(TEXT("the tail 1.5 cells back"), FMath::IsNearlyEqual(Dist(F.At(T), F.Tail(T, 1.5)), 1.5, 0.05));
	TestTrue(TEXT("the tail at the muzzle as it leaves"), Dist(F.Tail(F.Born + 0.001, 1.5), Muzzle) < 1e-9);
	TestTrue(TEXT("gone after its life"), F.Expired(F.Born + ac::Rules::roundLife) && !F.Expired(F.Born + ac::Rules::roundLife - 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcHudFeedImpactMarkerTest, "Autocraft.Hud.FeedImpactMarker", Flags)
bool FAcHudFeedImpactMarkerTest::RunTest(const FString&)
{
	// Flat ground (no map): a level round from 4.9 cells up falls for a
	// second (g 9.8), so it lands 90 cells out.
	ac::GameState St;
	ac::Simulation Flat(St);
	const ac::Vec3 Muzzle(0, 0, 0.5 * ac::Rules::roundGravity);
	const ac::Vec3 Level(ac::Rules::roundSpeed, 0, 0);
	const TOptional<ac::Vec3> Ground = AcPilotText::BallisticImpact(Flat, Muzzle, Level, -1);
	TestTrue(TEXT("a level round lands"), Ground.IsSet());
	if (Ground)
	{
		TestTrue(FString::Printf(TEXT("90 cells out on the ground (%.3f, %.3f)"), Ground->x, Ground->z),
			FMath::IsNearlyEqual(Ground->x, ac::Rules::roundSpeed, 0.05) && FMath::Abs(Ground->z) < 0.05);
	}
	// A building in the way: the round strikes its near face, 30 cells less its radius.
	St.structures.push_back(ac::Structure(5, ac::StructureKind::garrison, 1, ac::Vec2(30, 0)));
	ac::Simulation Walled(St);
	const TOptional<ac::Vec3> Wall = AcPilotText::BallisticImpact(Walled, Muzzle - ac::Vec3(0, 0, 3.5), Level, -1);
	const double Face = 30 - ac::Rules::radius(ac::StructureKind::garrison);
	TestTrue(FString::Printf(TEXT("it strikes the building's face at %.2f"), Face), Wall && FMath::IsNearlyEqual(Wall->x, Face, 0.05));
	// Its own building: passed for the first moments (the Citadel's corner guns).
	ac::GameState Own;
	Own.structures.push_back(ac::Structure(7, ac::StructureKind::garrison, 0, ac::Vec2(0, 0)));
	ac::Simulation Home(Own);
	const TOptional<ac::Vec3> Out = AcPilotText::BallisticImpact(Home, ac::Vec3(0, 0, 1.0), Level, 7);
	TestTrue(TEXT("its own building passed"), Out && Out->x > ac::Rules::radius(ac::StructureKind::garrison) + 1);
	// Straight up: past its life in the air, no marker.
	TestFalse(TEXT("straight up: none"), AcPilotText::BallisticImpact(Flat, Muzzle, ac::Vec3(0, 0, ac::Rules::roundSpeed), -1).IsSet());

	// The feed draws the marker: a ring and "34 c".
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		UE_LOG(LogTemp, Display, TEXT("Autocraft.Hud.FeedImpactMarker: no Slate renderer, the paint left unchecked"));
		return true;
	}
	TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1280, 720));
	TSharedRef<SAcPilotOverlay> Overlay = SNew(SAcPilotOverlay).Panel(true);
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(1280, 720), FSlateLayoutTransform());
	auto Paint = [&](const FAcPilotInfo& Info)
	{
		Overlay->Reset();
		Overlay->Show(Info, 100.0);
		FSlateWindowElementList List(Window);
		FHittestGrid Grid;
		const FPaintArgs Args(&Overlay.Get(), Grid, FVector2D::ZeroVector, 100.0, 0.016f);
		return Overlay->OnPaint(Args, Geometry, FSlateRect(0, 0, 1280, 720), List, 0, FWidgetStyle(), true);
	};
	FAcPilotInfo AtGun;
	AtGun.Name = TEXT("PROSPECTOR");
	AtGun.Hp = AtGun.MaxHp = 60;
	AtGun.Feed = FAcTurretFeed();
	FAcPilotInfo Marked = AtGun;
	Marked.Feed->Impact = FVector2D(0.2, -0.1);
	Marked.Feed->ImpactCells = 34;
	const int32 Without = Paint(AtGun), With = Paint(Marked);
	TestTrue(FString::Printf(TEXT("the marker paints (%d layers without, %d with)"), Without, With), With > Without + 3);
	// Off the glass: not drawn.
	FAcPilotInfo Off = AtGun;
	Off.Feed->Impact = FVector2D(5.0, 0.0);
	TestEqual(TEXT("off the view: no marker"), Paint(Off), Without);
	return true;
}

#endif
