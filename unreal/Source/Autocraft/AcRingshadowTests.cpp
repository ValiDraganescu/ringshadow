// Automation tests for the ringshadow's sky (docs/weather.md, "Ringshadow
// sweep"): the band on the ground is the shadow of the ring's arc, which the
// sky dome draws through the giant and the sun, so the sun the sky draws at
// the sweeps' hours and the giant's place must be the core's (the server
// casts the band by them). Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Ringshadow
#include "AcDaylight.h"
#include "AcGasGiant.h"

#include "Ringshadow.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// Compass bearing (0 north = -Y, 90 east = +X) and elevation, degrees.
	double BearingOf(const FVector& V)
	{
		const double B = FMath::RadiansToDegrees(FMath::Atan2(V.X, -V.Y));
		return B < 0 ? B + 360.0 : B;
	}
	double ElevationOf(const FVector& V) { return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(V.Z, -1.0, 1.0))); }
	double AngleDeg(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.0, 1.0)));
	}
	FVector Unreal(const ac::Vec3& V) { return FVector(V.x, V.z, V.y); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcRingshadowSkyTest, "Autocraft.Ringshadow.Sky", Flags)
bool FAcRingshadowSkyTest::RunTest(const FString&)
{
	// Where the sun stands as each sweep starts (Ringshadow.h, the core test
	// RingshadowSunAndGiantStandWhereTheSkyDrawsThem), and the giant.
	const double SunBearing[2] = {301.0, 20.2}, SunElevation[2] = {52.9, 27.2};
	const FVector Giant = FAcGasGiant::Direction();
	TestTrue(FString::Printf(TEXT("the giant hangs at bearing 280 (%.2f)"), BearingOf(Giant)), FMath::Abs(BearingOf(Giant) - 280.0) < 0.5);
	TestTrue(FString::Printf(TEXT("the giant hangs 12 up (%.2f)"), ElevationOf(Giant)), FMath::Abs(ElevationOf(Giant) - 12.0) < 0.5);
	TestTrue(TEXT("the sky's giant is the core's"), AngleDeg(Giant, Unreal(ac::Ringshadow::giant())) < 0.5);
	for (int32 I = 0; I < 2; ++I)
	{
		const double Hour = ac::Ringshadow::hours[I];
		const FVector Sun = AAcDaylight::SunToward(Hour);
		TestTrue(FString::Printf(TEXT("the sun at %.2f h stands at bearing %.1f (%.2f)"), Hour, SunBearing[I], BearingOf(Sun)),
			FMath::Abs(BearingOf(Sun) - SunBearing[I]) < 0.5);
		TestTrue(FString::Printf(TEXT("the sun at %.2f h stands %.1f up (%.2f)"), Hour, SunElevation[I], ElevationOf(Sun)),
			FMath::Abs(ElevationOf(Sun) - SunElevation[I]) < 0.5);
		TestTrue(FString::Printf(TEXT("the sky's sun at %.2f h is the core's"), Hour), AngleDeg(Sun, Unreal(ac::Ringshadow::sun(I))) < 0.5);
		// The arc's normal as M_AcSky works it out (RING_HLSL: cross(G, S)),
		// level: the way the band on the ground moves.
		const FVector Nrm = FVector::CrossProduct(Giant, Sun);
		const ac::Vec2 Dir = ac::Ringshadow::direction(I);
		const double Off = AngleDeg(FVector(Nrm.X, Nrm.Y, 0), FVector(Dir.x, Dir.y, 0));
		TestTrue(FString::Printf(TEXT("sweep %d's band moves the way the arc slides (%.2f° off)"), I, Off), Off < 0.5);
	}
	return true;
}

#endif
