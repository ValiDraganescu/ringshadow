// Automation tests for the third-person camera's feel (`FAcPilotChase`,
// docs/shooter-feel.md "Third person (2026-10-18)"). Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Pilot
#include "AcPilotAim.h"
#include "AcPilotCamera.h"
#include "AcPilotChase.h"
#include "AcSpace.h"

#include "Simulation.h"
#include "WindowMaps.h"

#include "Misc/AutomationTest.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPilotChaseZoomTest, "Autocraft.Pilot.ChaseZoomClamps", Flags)
bool FAcPilotChaseZoomTest::RunTest(const FString&)
{
	using FZoom = FAcPilotChase::FZoom;
	FZoom Z;
	TestEqual(TEXT("the old boom to start with"), Z.Target, 1.f);
	// A notch out: 12% longer; a notch in puts it back.
	Z.Notch(-1);
	TestTrue(TEXT("a notch out is 12%"), FMath::IsNearlyEqual(Z.Target, 1.12f, 1e-5f));
	Z.Notch(1);
	TestTrue(TEXT("a notch in undoes it"), FMath::IsNearlyEqual(Z.Target, 1.f, 1e-5f));
	// Held at both ends.
	for (int32 I = 0; I < 40; ++I) Z.Notch(1);
	TestEqual(TEXT("in no closer than 0.45"), Z.Target, FZoom::Min);
	for (int32 I = 0; I < 40; ++I) Z.Notch(-1);
	TestEqual(TEXT("out no farther than 2.6"), Z.Target, FZoom::Max);
	// A trackpad's burst counts three notches at most.
	Z.Set(1.f, true);
	Z.Notch(50);
	TestTrue(TEXT("a burst is three notches"), FMath::IsNearlyEqual(Z.Target, 1.f / (1.12f * 1.12f * 1.12f), 1e-5f));
	Z.Set(9.f, true);
	TestEqual(TEXT("the setting is clamped too"), Z.Target, FZoom::Max);

	// Eased: snapped at the first frame, then most of the way in 0.15 s.
	Z.Set(1.f, true);
	TestEqual(TEXT("snaps on the first frame"), Z.Tick(10.0), 1.0);
	Z.Notch(-1);
	const double Early = Z.Tick(10.0 + 1.0 / 60.0);
	TestTrue(TEXT("one frame in: on its way, not there"), Early > 1.0 && Early < 1.1);
	double Shown = Early;
	for (int32 F = 2; F <= 9; ++F) Shown = Z.Tick(10.0 + F / 60.0);
	TestTrue(TEXT("0.15 s in: 95% of the way"), Shown > 1.0 + 0.95 * 0.12 - 1e-6 && Shown <= 1.12 + 1e-9);
	for (int32 F = 10; F <= 60; ++F) Shown = Z.Tick(10.0 + F / 60.0);
	TestTrue(TEXT("then there"), FMath::IsNearlyEqual(Shown, 1.12, 1e-4));

	// The tilt over a building site: none up to the old boom, 25° at the far end.
	TestEqual(TEXT("no tilt zoomed in"), FZoom::Tilt(0.45), 0.0);
	TestEqual(TEXT("no tilt at 1"), FZoom::Tilt(1.0), 0.0);
	TestTrue(TEXT("some tilt half way out"), FZoom::Tilt(1.8) > 0.0 && FZoom::Tilt(1.8) < FZoom::MaxTilt);
	TestTrue(TEXT("25 degrees at 2.6"), FMath::IsNearlyEqual(FZoom::Tilt(2.6), FMath::DegreesToRadians(25.0), 1e-9));
	TestTrue(TEXT("never past 25 degrees"), FMath::IsNearlyEqual(FZoom::Tilt(9.0), FZoom::MaxTilt, 1e-9));
	// Holding a building out: the boom out to the placing view, the player's zoom kept.
	Z.Set(0.45f, true);
	Z.Tick(20.0);
	Z.Floor = FZoom::Placing;
	for (int32 F = 1; F <= 60; ++F) Shown = Z.Tick(20.0 + F / 60.0);
	TestTrue(TEXT("placing: out to 1.8"), FMath::IsNearlyEqual(Shown, double(FZoom::Placing), 1e-4));
	TestEqual(TEXT("placing: the player's zoom kept"), Z.Target, FZoom::Min);
	TestTrue(TEXT("placing: tilted over the site"), FZoom::Tilt(Shown) > FMath::DegreesToRadians(10.0));
	Z.Floor = 0.f;
	for (int32 F = 61; F <= 120; ++F) Shown = Z.Tick(20.0 + F / 60.0);
	TestTrue(TEXT("placed: back in"), FMath::IsNearlyEqual(Shown, double(FZoom::Min), 1e-4));
	// A new ride (Reset) shows the zoom wanted at once.
	FAcPilotChase Chase;
	Chase.Zoom.Set(2.f, false);
	Chase.Zoom.Tick(1.0);
	Chase.Zoom.Notch(-1);
	Chase.Reset();
	TestTrue(TEXT("a new ride snaps"), FMath::IsNearlyEqual(Chase.Zoom.Tick(1.01), double(Chase.Zoom.Target), 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPilotWalkCameraTest, "Autocraft.Pilot.WalkFollowsTheCamera", Flags)
bool FAcPilotWalkCameraTest::RunTest(const FString&)
{
	// A small open map, the player's Prospector taken over and stood on open
	// ground (nothing within 3 cells it could bump into).
	const ac::MapDefinition Map = ac::WindowMaps::build(ac::MapChoice{ac::MapStyle::openField, ac::MapSize::small});
	ac::Simulation Sim(ac::GameState::new_(Map), Map);
	int64 Id = INDEX_NONE;
	for (const ac::Unit& U : Sim.state.units)
	{
		if (U.owner == 0 && U.kind == ac::UnitKind::prospector) { Id = U.id; break; }
	}
	if (!TestTrue(TEXT("a Prospector to drive"), Id != INDEX_NONE && Sim.take(Id))) return false;
	ac::Unit U = *Sim.pilotUnit();
	auto Open = [&](const ac::Vec2 P) {
		for (int32 K = 0; K < 16; ++K)
		{
			const double A = K * UE_DOUBLE_PI / 8;
			for (const double R : {0.0, 1.5, 3.0})
				if (!Sim.pilotCanStand(U, P + ac::Vec2(std::cos(A), std::sin(A)) * R)) return false;
		}
		return true;
	};
	bool bFound = false;
	for (double R = 4; R < 40 && !bFound; R += 1)
	{
		for (int32 K = 0; K < 24 && !bFound; ++K)
		{
			const ac::Vec2 P = U.position + ac::Vec2(std::cos(K * UE_DOUBLE_PI / 12), std::sin(K * UE_DOUBLE_PI / 12)) * R;
			if (Open(P)) { U.position = P; bFound = true; }
		}
	}
	if (!TestTrue(TEXT("open ground near the Prospector"), bFound)) return false;

	// The camera yawed 60° off the unit's heading (0), the act key held at a
	// point straight ahead of the unit: it keeps facing that point...
	const double Yaw = FMath::DegreesToRadians(60.0);
	const TOptional<FVector> Aim = AcSpace::ToWorld(U.position + ac::Vec2(5, 0));
	const double Aimed = AcPilotAim::ChaseHeading(U, Yaw, Aim, true, true);
	TestTrue(TEXT("aiming, it faces what the sight is on"), FMath::IsNearlyEqual(Aimed, 0.0, 1e-9));
	// ...and W walks it along the camera's forward all the same.
	U.heading = Aimed;
	const ac::Vec2 From = U.position;
	Sim.pilotWalk(U, AcPilotAim::ChaseWalk(ac::Vec2(1, 0), Yaw, U.heading), 0.25);
	const ac::Vec2 Moved = U.position - From;
	TestTrue(TEXT("it moved"), ac::length(Moved) > 0.3);
	const double Off = std::remainder(std::atan2(Moved.y, Moved.x) - Yaw, 2.0 * UE_DOUBLE_PI);
	TestTrue(FString::Printf(TEXT("along the camera's forward (%.4f rad off)"), Off), FMath::Abs(Off) < 1e-6);
	// D: the camera's right, wherever the unit faces.
	const ac::Vec2 Was = U.position;
	Sim.pilotWalk(U, AcPilotAim::ChaseWalk(ac::Vec2(0, 1), Yaw, U.heading), 0.25);
	const ac::Vec2 Side = U.position - Was;
	const double SideOff = std::remainder(std::atan2(Side.y, Side.x) - (Yaw + UE_DOUBLE_PI / 2), 2.0 * UE_DOUBLE_PI);
	TestTrue(FString::Printf(TEXT("D along the camera's right (%.4f rad off)"), SideOff), ac::length(Side) > 0.2 && FMath::Abs(SideOff) < 1e-6);

	// Walking with nothing aimed at: it turns to the camera's yaw and goes at full pace.
	const double Free = AcPilotAim::ChaseHeading(U, Yaw, Aim, true, false);
	TestTrue(TEXT("walking free, it faces the camera's yaw"), FMath::IsNearlyEqual(Free, Yaw, 1e-9));
	U.heading = Free;
	const ac::Vec2 Start = U.position;
	Sim.pilotWalk(U, AcPilotAim::ChaseWalk(ac::Vec2(1, 0), Yaw, U.heading), 0.25);
	TestTrue(TEXT("faster facing the way it walks"), ac::length(U.position - Start) > ac::length(Moved) + 1e-6);
	// Standing: toward what the sight is on, as before.
	TestTrue(TEXT("standing, it faces the sight's point"), FMath::IsNearlyEqual(AcPilotAim::ChaseHeading(U, Yaw, AcSpace::ToWorld(U.position + ac::Vec2(0, 5)), false, false), UE_DOUBLE_PI / 2, 1e-9));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPilotLookAroundTest, "Autocraft.Pilot.LookAroundKeepsTheAim", Flags)
bool FAcPilotLookAroundTest::RunTest(const FString&)
{
	using FOrbit = FAcPilotChase::FOrbit;
	// The mouse turns it only while the right button is held.
	FOrbit O;
	O.Turn(0.5, 0.2);
	TestFalse(TEXT("not held: the mouse is the look's"), O.Off());
	O.Hold(true, 10.0);
	O.Turn(0.8, 0.3);
	TestTrue(TEXT("held: swung round by the mouse"), FMath::IsNearlyEqual(O.Yaw, 0.8, 1e-12));
	TestTrue(TEXT("a little pitch: half the look's"), FMath::IsNearlyEqual(O.Pitch, 0.15, 1e-12));
	O.Turn(0, 5.0);
	TestEqual(TEXT("the pitch held to 20 degrees"), O.Pitch, FOrbit::MaxPitch);
	O.Turn(-0.4, -5.0 - 2 * FOrbit::MaxPitch / FOrbit::PitchShare);
	TestEqual(TEXT("down no farther either"), O.Pitch, -FOrbit::MaxPitch);
	O.Tick(11.0);
	TestTrue(TEXT("held, it stays where the mouse left it"), FMath::IsNearlyEqual(O.Yaw, 0.4, 1e-12));
	// Let go: back behind the unit over 0.25 s, eased.
	O.Hold(false, 12.0);
	O.Tick(12.125);
	TestTrue(TEXT("half way back at half the time"), FMath::IsNearlyEqual(O.Yaw, 0.2, 1e-9) && FMath::IsNearlyEqual(O.Pitch, -FOrbit::MaxPitch / 2, 1e-9));
	O.Tick(12.25);
	TestFalse(TEXT("behind the unit after 0.25 s"), O.Off());
	TestEqual(TEXT("exactly behind"), O.Yaw, 0.0);
	// The short way round: swung past the back, it comes back the near way.
	O.Hold(true, 20.0);
	O.Turn(3.0, 0);
	O.Turn(0.5, 0);
	TestTrue(TEXT("past the back reads as the other side"), O.Yaw < 0);
	O.Hold(false, 21.0);
	O.Tick(21.1);
	TestTrue(TEXT("and eases back that way"), O.Yaw < 0 && O.Yaw > -2.8);

	// The views: a Prospector on open ground, no world (no walls, no rays).
	ac::Unit U;
	U.kind = ac::UnitKind::prospector;
	U.position = ac::Vec2(10, 10);
	U.heading = 0.3;
	const FTransform Root(AcSpace::QuatFromHeading(U.heading), AcSpace::ToWorld(U.position, 0.0));
	FAcPilotChase Chase;
	const double Yaw = 0.3, Pitch = -0.15;
	FVector AimAt, ShownAt;
	FQuat AimRot, ShownRot;
	// The aim's view (what the crosshair is cast from), then the one shown
	// while looking round: 0.9 rad across.
	Chase.Follow(nullptr, Root, U, nullptr, Pitch, Yaw, 1.0, AimAt, AimRot);
	Chase.Follow(nullptr, Root, U, nullptr, Pitch, Yaw + 0.9, 1.0, ShownAt, ShownRot);
	const FVector Aim = AimAt + AimRot.GetForwardVector() * AcSpace::ToCm(20.0);
	const TOptional<FVector2D> OnAim = AcPilotAim::ScreenOffset(AimAt, AimRot, FAcPilotCamera::FovDegrees, Aim);
	TestTrue(TEXT("the aim's view: the crosshair in the middle"), OnAim && OnAim->IsNearlyZero(1e-6));
	// The camera went round the unit: as far from its eye, elsewhere.
	TestTrue(TEXT("round the unit, as far"), FMath::IsNearlyEqual((ShownAt - *Chase.Eye()).Size(), (AimAt - *Chase.Eye()).Size(), 1.0));
	TestTrue(TEXT("and somewhere else"), FVector::Dist(ShownAt, AimAt) > AcSpace::ToCm(1.0));
	// Seen from there the crosshair is where the aim is: off the middle,
	// to the left (the camera swung right), still on the view.
	const TOptional<FVector2D> Off = AcPilotAim::ScreenOffset(ShownAt, ShownRot, FAcPilotCamera::FovDegrees, Aim);
	TestTrue(TEXT("looking round: the crosshair off the middle, left"), Off && Off->X < -0.2);
	// The heading and the walk are the aim's: neither reads the swing.
	const double Heading = AcPilotAim::Facing(U, Yaw, Aim);
	TestTrue(TEXT("the unit faces the aim's point"), FMath::IsNearlyEqual(Heading, Yaw, 0.02));
	const ac::Vec2 Walk = AcPilotAim::ChaseWalk(ac::Vec2(1, 0), Yaw, Heading);
	// (The step `pilotWalk` takes: backing and sideways at their shares.)
	const ac::Vec2 Step(Walk.x * (Walk.x < 0 ? ac::Pilot::backPace : 1.0), Walk.y * ac::Pilot::sidePace);
	const double Along = std::atan2(Step.y, Step.x) + Heading;
	TestTrue(TEXT("W walks along the aim's forward"), FMath::Abs(std::remainder(Along - Yaw, 2.0 * UE_DOUBLE_PI)) < 1e-6);
	// Swung right round, the aim is behind the camera: no crosshair.
	Chase.Follow(nullptr, Root, U, nullptr, Pitch, Yaw + UE_DOUBLE_PI, 1.0, ShownAt, ShownRot);
	TestFalse(TEXT("turned round: the aim off the view"), AcPilotAim::ScreenOffset(ShownAt, ShownRot, FAcPilotCamera::FovDegrees, Aim).IsSet());
	return true;
}

#endif
