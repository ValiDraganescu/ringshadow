// The visor's material parameters follow the wiper's rule (docs/weather.md,
// "Rain on the visor and the wipers"). Run:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Weather.Visor
#include "AcVisor.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcVisorParamsTest, "Autocraft.Weather.Visor", Flags)
bool FAcVisorParamsTest::RunTest(const FString&)
{
	ac::Wipers W;
	double T = 50;
	auto Run = [&](const double Rate, const double Seconds, const TFunction<void(const FAcVisorParams&)>& Each = nullptr)
	{
		for (const double End = T + Seconds; T < End; T += 1.0 / 60)
		{
			W.update(T, Rate);
			if (Each) Each(AcVisor::Params(W, 6.0, 0.0, 1.0));
		}
	};

	// Dry and parked: nothing on the glass.
	Run(0.0, 2);
	FAcVisorParams P = AcVisor::Params(W, 0.0, 0.0, 1.0);
	TestFalse(TEXT("dry: nothing to draw"), P.bWet);
	TestEqual(TEXT("dry: the blade parked"), P.Blade.R, float(ac::Wipers::ParkAngle));
	TestEqual(TEXT("dry: not moving"), P.Blade.G, 0.f);
	TestEqual(TEXT("no sweep yet"), P.A.G, 0.f);

	// A storm: the rain fed, the blade sweeping out and back across the arc.
	float Lo = 10, Hi = -10;
	bool bOut = false, bBack = false;
	Run(1.0, 4, [&](const FAcVisorParams& Q)
	{
		Lo = FMath::Min(Lo, Q.Blade.R);
		Hi = FMath::Max(Hi, Q.Blade.R);
		bOut |= Q.Blade.G > 0.5f;
		bBack |= Q.Blade.G < -0.5f;
	});
	P = AcVisor::Params(W, 6.0, 0.5, 0.8);
	TestEqual(TEXT("storm: the rate fed"), P.Rain, 1.f);
	TestTrue(TEXT("storm: wet"), P.bWet);
	TestTrue(TEXT("storm: out and back"), bOut && bBack);
	TestTrue(TEXT("storm: across the whole arc"), Lo < -1.4f && Hi > 1.4f);
	TestEqual(TEXT("storm: fast sweeps"), P.A.G, float(ac::Wipers::FastEach));
	TestTrue(TEXT("storm: the sweep before kept"), P.B.G > 0.f && P.B.R > P.A.R);
	TestTrue(TEXT("storm: water lands since the sweep began"), P.A.B > 0.f && P.A.B >= P.A.A);
	TestTrue(TEXT("storm: the recent history grows with age"), P.Hist.R > 0.f && P.Hist.A > P.Hist.B && P.Hist.B > P.Hist.G && P.Hist.G > P.Hist.R);
	TestTrue(TEXT("the wind across the view, signed and bounded"), P.Air.R > 0.f && P.Air.R <= 1.f);
	TestEqual(TEXT("the flash"), P.Air.G, 0.5f);
	TestEqual(TEXT("the arc's numbers are the rule's"), P.Arc.B, float(ac::Wipers::ParkAngle));
	TestEqual(TEXT("the arc's far stop"), P.Arc.A, float(ac::Wipers::FarAngle));
	TestEqual(TEXT("the smear dries as the rule says"), P.Blade.B, float(ac::Wipers::SmearSeconds));

	// Sheltered (the rate fed eases to 0): the last sweeps, then parked; the
	// glass under the arc clear, a corner the blade never reaches still wet.
	Run(0.0, 5);
	P = AcVisor::Params(W, 6.0, 0.0, 1.0);
	TestEqual(TEXT("sheltered: no rain fed"), P.Rain, 0.f);
	TestEqual(TEXT("sheltered: parked"), P.Blade.R, float(ac::Wipers::ParkAngle));
	TestEqual(TEXT("sheltered: not moving"), P.Blade.G, 0.f);
	TestTrue(TEXT("sheltered: nothing landed since the last sweep"), P.A.B < 1e-4f && P.A.A < 1e-4f);
	TestTrue(TEXT("sheltered: the corners stay wet"), P.Seed.B > 1.f && P.bWet);
	TestEqual(TEXT("sheltered: nothing lately"), P.Hist.A, 0.f);
	return true;
}

#endif
