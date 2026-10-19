// Automation tests for the shelter (`AcShelter`, docs/weather.md "Shelter:
// no rain indoors"): the Citadels' boxes and the lens drying. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Weather.Shelter
#include "AcShelter.h"
#include "AcShuttleShip.h"
#include "AcSpace.h"

#include "Rules.h"
#include "Shuttle.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcShelterBoxesTest, "Autocraft.Weather.Shelter.Boxes", Flags)
bool FAcShelterBoxesTest::RunTest(const FString&)
{
	// The MMO's hollow Citadel (docs/citadel-interior.md §1), put back after.
	const bool bHall = ac::Rules::citadelHall();
	ac::Rules::setCitadelHall(true);
	const ac::Vec2 Was = AcSpace::Origin();
	AcSpace::SetOrigin(ac::Vec2(10, 0));
	std::vector<ac::Structure> Built;
	Built.emplace_back(1, ac::StructureKind::citadel, 0, ac::Vec2(40, 20));
	Built.emplace_back(2, ac::StructureKind::citadel, 0, ac::Vec2(0, 0));
	Built.emplace_back(3, ac::StructureKind::garrison, 0, ac::Vec2(2, 30));
	ac::Structure Building(4, ac::StructureKind::citadel, 0, ac::Vec2(-30, 0));
	Building.buildLeft = 10.0;
	Built.push_back(Building);
	for (ac::Structure& S : Built) S.hp = S.kind == ac::StructureKind::citadel && S.complete() ? 1500 : 400;
	auto Ground = [](const ac::Vec2 P) { return P.x > 20 ? 2.0 : 0.5; };
	const TArray<AcShelter::FBox> Boxes = AcShelter::Boxes(Built, Ground, AcSpace::ToWorld(ac::Vec2(1, 1), 3));
	TestEqual(TEXT("one box per complete Citadel"), Boxes.Num(), 2);
	if (Boxes.Num() == 2)
	{
		const AcShelter::FBox& Near = Boxes[0];
		TestTrue(TEXT("nearest the camera first"), Near.Centre.Equals(FVector2D(AcSpace::ToWorld(ac::Vec2(0, 0))), 1e-6));
		// The hall: 10x10 cells, 6 high, over its ground (0.5 cells here).
		TestTrue(TEXT("the hall's half side and the faces' margin"), Near.Half.Equals(FVector2D(500 + AcShelter::FaceCm), 1e-6));
		TestTrue(TEXT("from under the floor"), FMath::IsNearlyEqual(Near.Low, 50 - AcShelter::SinkCm));
		TestTrue(TEXT("to the roof slab's underside"), FMath::IsNearlyEqual(Near.High, 50 + 600 + AcShelter::FaceCm));
		const auto At = [](double X, double Y, double Z) { return AcSpace::ToWorld(ac::Vec2(X, Y), Z); };
		TestTrue(TEXT("the floor inside"), Near.Contains(At(0, 0, 0.5 + 0.05)));
		TestTrue(TEXT("an inner wall face inside"), Near.Contains(At(4.999, 2, 2.5)));
		TestTrue(TEXT("the ceiling inside"), Near.Contains(At(1, 1, 0.5 + 6.0)));
		TestFalse(TEXT("the doorway outside"), Near.Contains(At(0, 5.5, 1.5)));
		TestFalse(TEXT("the outer wall face outside"), Near.Contains(At(6.0, 0, 2.5)));
		TestFalse(TEXT("the roof's top outside"), Near.Contains(At(0, 0, 0.5 + 7.0)));
		TestTrue(TEXT("the far one on its own ground"), FMath::IsNearlyEqual(Boxes[1].High, 200 + 600 + AcShelter::FaceCm));
	}
	// Dead: no shelter. Without the hall (the strategy game): none at all.
	Built[1].hp = 0;
	TestEqual(TEXT("a dead Citadel shelters nothing"), AcShelter::Boxes(Built, Ground, FVector::ZeroVector).Num(), 1);
	ac::Rules::setCitadelHall(false);
	TestEqual(TEXT("no hall, no box"), AcShelter::Boxes(Built, Ground, FVector::ZeroVector).Num(), 0);
	ac::Rules::setCitadelHall(bHall);
	AcSpace::SetOrigin(Was);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcShelterLensTest, "Autocraft.Weather.Shelter.Lens", Flags)
bool FAcShelterLensTest::RunTest(const FString&)
{
	// Out in the rain the lens follows the rain; in the hall it dries in
	// 1.5 s; back out it wets in 0.5 s.
	constexpr float Rain = 0.8f, Dt = 1.f / 60.f;
	auto Run = [&](float L, const float Target, const float Seconds)
	{
		for (int32 K = 0; K < FMath::RoundToInt(Seconds / Dt); ++K) L = AcShelter::EaseLens(L, Target, Rain, Dt);
		return L;
	};
	const float Out = Run(0.f, Rain, 1.f);
	TestTrue(TEXT("out: the rain's rate"), FMath::IsNearlyEqual(Out, Rain));
	const float Mid = Run(Out, 0.f, AcShelter::LensDrySeconds * 0.5f);
	TestTrue(TEXT("half way in: half dry"), FMath::IsNearlyEqual(Mid, Rain * 0.5f, 0.01f));
	const float Dry = Run(Mid, 0.f, AcShelter::LensDrySeconds * 0.5f + 0.05f);
	TestEqual(TEXT("in the hall: dry"), Dry, 0.f);
	const float Wetting = Run(Dry, Rain, AcShelter::LensWetSeconds * 0.5f);
	TestTrue(TEXT("back out: half wet in a quarter second"), FMath::IsNearlyEqual(Wetting, Rain * 0.5f, 0.01f));
	const float Wet = Run(Wetting, Rain, AcShelter::LensWetSeconds * 0.5f + 0.05f);
	TestTrue(TEXT("back out: the rain's rate"), FMath::IsNearlyEqual(Wet, Rain));
	// No rain: nothing to ease.
	TestEqual(TEXT("dry weather: dry glass"), AcShelter::EaseLens(Wet, 0.f, 0.f, Dt), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcShelterShipBayTest, "Autocraft.Weather.Shelter.ShipBay", Flags)
bool FAcShelterShipBayTest::RunTest(const FString&)
{
	// C5: the walk-in Dropship's dry box, the ship turned and banked: the
	// whole bay and the cockpit's air under the swept windshield, nothing
	// outside the hull's sides, behind the opening or over the roof.
	const FTransform Placement(FRotator(8.0, 30.0, -20.0), FVector(1000.0, -500.0, 3000.0));
	const AcShelter::FShipBox Box = AcShelter::BayOf(Placement);
	using B = ac::ShuttleBay;
	auto At = [&](const double X, const double Y, const double Z) { return Placement.TransformPosition(FVector(AcSpace::ToCm(X), AcSpace::ToCm(Y), AcSpace::ToCm(Z))); };
	TestTrue(TEXT("the bay's middle"), Box.Contains(At(0, 0, B::deck + 1.0)));
	TestTrue(TEXT("the back corner by the opening"), Box.Contains(At(B::rear + 0.1, B::halfWide - 0.1, B::ceiling - 0.1)));
	TestTrue(TEXT("over the deck at the ramp post"), Box.Contains(At(B::rampButton.x, B::rampButton.y, B::rampButton.z)));
	TestTrue(TEXT("the cockpit's air under the windshield"), Box.Contains(At(7.6, 0, 1.5)));
	TestFalse(TEXT("outside the flank"), Box.Contains(At(0, B::hullHalfWide + 0.3, 1.5)));
	TestFalse(TEXT("behind the opening"), Box.Contains(At(B::rear - 0.5, 0, 1.5)));
	TestFalse(TEXT("over the roof"), Box.Contains(At(0, 0, B::roofTop + 0.3)));
	TestFalse(TEXT("ahead of the glass"), Box.Contains(At(AcSpace::ToCells(AcShuttleShip::GlassBottomX) + 0.3, 0, 1.5)));
	// From the bay's middle the volumetric fog starts past its farthest
	// corner (over 6 cells off); from outside, at the camera.
	const double Clear = AcShelter::FogClearCm(Box, At(0, 0, B::deck + 1.5));
	TestTrue(FString::Printf(TEXT("the fog starts past the bay (%.0f cm)"), Clear), Clear > AcSpace::ToCm(6.5) && Clear < AcSpace::ToCm(12.0));
	TestEqual(TEXT("outside, no change"), AcShelter::FogClearCm(Box, At(0, 4.0, 1.5)), 0.0);
	return true;
}

#endif
