// Automation tests for the quality tiers (AcQualityTier.h): each Apple GPU
// lands in the tier docs/perf.md's table gives it. Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Quality; Quit" -TestExit="Automation Test Queue Empty"
#include "AcQualityTier.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcQualityTierTableTest, "Autocraft.Quality.Tiers", Flags)
bool FAcQualityTierTableTest::RunTest(const FString&)
{
	using namespace AcQualityTier;
	struct FGpu { const TCHAR* Name; int32 Cores; ETier Tier; };
	// The table in docs/perf.md "Round 8" (the machines Apple sells and sold).
	const FGpu Gpus[] = {
		{TEXT("Apple M1"), 7, ETier::Floor},     {TEXT("Apple M1"), 8, ETier::Floor},
		{TEXT("Apple M1 Pro"), 14, ETier::Mid},  {TEXT("Apple M1 Pro"), 16, ETier::Mid},
		{TEXT("Apple M1 Max"), 24, ETier::High}, {TEXT("Apple M1 Max"), 32, ETier::High},
		{TEXT("Apple M1 Ultra"), 48, ETier::Top},
		{TEXT("Apple M2"), 8, ETier::Floor},     {TEXT("Apple M2"), 10, ETier::Mid},
		{TEXT("Apple M2 Pro"), 16, ETier::Mid},  {TEXT("Apple M2 Pro"), 19, ETier::High},
		{TEXT("Apple M2 Max"), 30, ETier::High}, {TEXT("Apple M2 Max"), 38, ETier::Top},
		{TEXT("Apple M3"), 8, ETier::Mid},       {TEXT("Apple M3"), 10, ETier::Mid},
		{TEXT("Apple M3 Pro"), 14, ETier::Mid},  {TEXT("Apple M3 Pro"), 18, ETier::High},
		{TEXT("Apple M3 Max"), 30, ETier::Top},  {TEXT("Apple M3 Max"), 40, ETier::Top},
		{TEXT("Apple M4"), 8, ETier::Mid},       {TEXT("Apple M4"), 10, ETier::Mid},
		{TEXT("Apple M4 Pro"), 16, ETier::High}, {TEXT("Apple M4 Pro"), 20, ETier::High},
		{TEXT("Apple M4 Max"), 32, ETier::Top},  {TEXT("Apple M4 Max"), 40, ETier::Top},
	};
	for (const FGpu& G : Gpus)
	{
		TestEqual(FString::Printf(TEXT("%s, %d cores"), G.Name, G.Cores), Name(FromScore(Score(G.Name, G.Cores))), Name(G.Tier));
	}
	TestEqual(TEXT("the M4 Max's score"), Score(TEXT("Apple M4 Max"), 40), 54.0);
	TestEqual(TEXT("a later generation"), GenerationFactor(TEXT("Apple M5 Pro")), 1.45);
	TestEqual(TEXT("two digits"), GenerationFactor(TEXT("Apple M12")), 1.45);
	TestEqual(TEXT("not an M-series GPU"), GenerationFactor(TEXT("AMD Radeon Pro 5500M")), 0.0);
	TestEqual(TEXT("no cores: Max by its name"), Name(FromName(TEXT("Apple M2 Max"))), Name(ETier::Top));
	TestEqual(TEXT("no cores: Pro by its name"), Name(FromName(TEXT("Apple M1 Pro"))), Name(ETier::High));
	TestEqual(TEXT("no cores: else mid"), Name(FromName(TEXT("Intel Iris Plus"))), Name(ETier::Mid));
	TestTrue(TEXT("named"), Named(TEXT("FLOOR")).IsSet() && *Named(TEXT("FLOOR")) == ETier::Floor);
	TestFalse(TEXT("named nothing"), Named(TEXT("ultra")).IsSet());
	return true;
}

#endif
