// Automation tests for the pilot's veil over the far ground (docs/weather.md,
// "The night's reach and the far haze"): how far the eye sees by night and
// by day, how deep into the haze the far land goes, and the tone it goes to,
// so the numbers can't drift. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.NightHaze
#include "AcDaylight.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// M_AcSky's haze rising from the horizon (make_sky_material.py, HAZE_HLSL)
	/// at `E` degrees up, under a haze that rises `Rise` degrees.
	double DomeRise(const double E, const double Rise)
	{
		const double T = FMath::Clamp(E / FMath::Max(Rise, 1.0), 0.0, 1.0);
		return FMath::Pow(1.0 - T * T * (3.0 - 2.0 * T), 1.6);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcNightHazeTonesTest, "Autocraft.NightHaze.Tones", Flags)
bool FAcNightHazeTonesTest::RunTest(const FString&)
{
	// The night: 70 cells under the evening's ringlight (the giant 0.9 lit,
	// 21:00-23:00), less as it wanes toward dawn; the rain closes it in.
	const double Evening = AAcDaylight::ReachAt(0, 0.9, 0);
	TestTrue(FString::Printf(TEXT("the night's reach under the evening's ringlight is 70 cells (%.2f)"), Evening), FMath::IsNearlyEqual(Evening, 70.0, 0.01));
	TestTrue(FString::Printf(TEXT("a Citadel 35 cells off stands inside the night's reach (%.1f)"), Evening), Evening > 35.0);
	const double Midnight = AAcDaylight::ReachAt(0, 0.83, 0), Dawn = AAcDaylight::ReachAt(0, 0.29, 0);
	TestTrue(FString::Printf(TEXT("about 67 at midnight (%.1f), 44 before dawn (%.1f)"), Midnight, Dawn),
		FMath::Abs(Midnight - 67.0) < 1.0 && FMath::Abs(Dawn - 44.0) < 1.0);
	TestTrue(FString::Printf(TEXT("never under the old 26 cells on a clear night (%.1f)"), AAcDaylight::ReachAt(0, 0, 0)), AAcDaylight::ReachAt(0, 0, 0) > 26.0);
	TestTrue(TEXT("the rain closes it in"), AAcDaylight::ReachAt(0, 0.9, 1) < Evening);
	// The day: 110 cells whatever the giant.
	for (const double Lit : {0.0, 0.5, 1.0})
	{
		const double Day = AAcDaylight::ReachAt(1, Lit, 0);
		TestTrue(FString::Printf(TEXT("the day's reach is 110 cells, the giant %.1f lit (%.2f)"), Lit, Day), FMath::IsNearlyEqual(Day, 110.0, 0.01));
	}
	// The cap: never all the way into the haze, by night or by day.
	const double Cap = AAcDaylight::HazeCap();
	TestTrue(FString::Printf(TEXT("the far land goes 0.85-0.9 of the way into the haze (%.3f)"), Cap), Cap >= 0.85 && Cap <= 0.9);
	const double Lights = AAcDaylight::HazeLights();
	TestTrue(FString::Printf(TEXT("emissive surfaces take half the veil (%.2f)"), Lights), FMath::IsNearlyEqual(Lights, 0.5, 0.01));
	// The far tone: the sky dome's own at the horizon, where the haze is all
	// of the dome's colour, by night and by day: no band where land meets sky.
	const FLinearColor Haze(0.012f, 0.018f, 0.05f, 24.f);
	for (const double Dark : {0.0, 1.0})
	{
		const FLinearColor Far = AAcDaylight::VeilFar(Haze, Dark);
		TestTrue(FString::Printf(TEXT("the veil's far tone is the sky's horizon haze, dark %.0f (%s against %s)"), Dark, *Far.ToString(), *Haze.ToString()),
			Far.Equals(Haze, 1e-6f));
	}
	TestTrue(TEXT("at the horizon the dome is all haze"), FMath::IsNearlyEqual(DomeRise(0, 24), 1.0, 1e-9));
	return true;
}

#endif
