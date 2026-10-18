// Automation tests for the third-person camera's feel (`FAcPilotChase`,
// docs/shooter-feel.md "Third person (2026-10-18)"). Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Pilot
#include "AcPilotChase.h"

#include "Misc/AutomationTest.h"

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
	// A new ride (Reset) shows the zoom wanted at once.
	FAcPilotChase Chase;
	Chase.Zoom.Set(2.f, false);
	Chase.Zoom.Tick(1.0);
	Chase.Zoom.Notch(-1);
	Chase.Reset();
	TestTrue(TEXT("a new ride snaps"), FMath::IsNearlyEqual(Chase.Zoom.Tick(1.01), double(Chase.Zoom.Target), 1e-6));
	return true;
}

#endif
