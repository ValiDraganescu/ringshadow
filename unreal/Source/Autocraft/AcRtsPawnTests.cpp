// Automation tests for the top-down camera (`AAcRtsPawn`). Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Rts
#include "AcPilotPawn.h"
#include "AcRtsPawn.h"

#include "FreeView.h"

#include "Misc/AutomationTest.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcRtsRightDragOrbitsTest, "Autocraft.Rts.RightDragOrbits", Flags)
bool FAcRtsRightDragOrbitsTest::RunTest(const FString&)
{
	// A view in the middle of a map, the HUD's cover along the bottom.
	ac::FreeView View(ac::GroundRect{-60, -40, 60, 40}, ac::Vec2(3, -2), 40, ac::Vec2(1280, 800));
	View.resize(ac::Vec2(1280, 800), AAcRtsPawn::DefaultCover);
	View.center(ac::Vec2(3, -2));
	const ac::Vec2 Mid(640, 400);
	const ac::Vec2 Focus = View.ground(Mid);
	const ac::Vec3 Eye = View.cameraPosition();
	const double Pitch = View.projection().pitchDegrees;

	// A drag: 30 frames of the mouse moving right and a little down.
	const float Dt = 1.f / 60.f;
	const FVector2D Step(12, 4);
	for (int32 F = 0; F < 30; ++F) View.orbit(AAcRtsPawn::OrbitTurn(Step, Dt));
	const double Want = 30 * Step.X * AAcPilotPawn::LookGain(Step, Dt) * AAcPilotPawn::YawPerPoint;
	TestTrue(FString::Printf(TEXT("turned by the pilot's look gain (%.4f rad, want %.4f)"), View.yaw, Want),
		Want > 0.1 && FMath::IsNearlyEqual(View.yaw, std::remainder(Want, 2.0 * UE_DOUBLE_PI), 1e-9));
	// The focus stays: the ground under the screen's centre, the target.
	TestTrue(TEXT("the focus point unchanged"), ac::distance(View.ground(Mid), Focus) < 1e-9 && ac::distance(View.target, Focus) < 1e-12);
	// No pitch change: the same angle, the eye as high and as far from the focus.
	const ac::Vec3 At = View.cameraPosition();
	TestEqual(TEXT("the pitch unchanged"), View.projection().pitchDegrees, Pitch);
	TestTrue(TEXT("the eye as high"), FMath::IsNearlyEqual(At.y, Eye.y, 1e-9));
	TestTrue(TEXT("and as far from the focus"),
		FMath::IsNearlyEqual(ac::distance(ac::Vec2(At.x, At.z), Focus), ac::distance(ac::Vec2(Eye.x, Eye.z), Focus), 1e-9));
	// The view looks the way it turned: up the screen is the yaw's forward.
	const ac::Vec2 Up = View.ground(ac::Vec2(640, 100)) - Focus;
	const double Ahead = std::atan2(Up.y, Up.x), WantAhead = std::atan2(-std::cos(View.yaw), std::sin(View.yaw));
	TestTrue(TEXT("up the screen is the turned forward"), FMath::Abs(std::remainder(Ahead - WantAhead, 2.0 * UE_DOUBLE_PI)) < 1e-6);
	// A pick still lands under the pointer.
	const ac::Vec2 G = View.ground(ac::Vec2(200, 650));
	TestTrue(TEXT("view points and the ground agree"), ac::distance(View.canvas(ac::Vec3(G.x, 0, G.y)), ac::Vec2(200, 650)) < 1e-6);

	// The mouse moving only up or down turns nothing; left turns back.
	TestEqual(TEXT("up and down: no turn"), AAcRtsPawn::OrbitTurn(FVector2D(0, 25), Dt), 0.0);
	TestTrue(TEXT("left turns left"), AAcRtsPawn::OrbitTurn(FVector2D(-12, 0), Dt) < 0);
	return true;
}

#endif
