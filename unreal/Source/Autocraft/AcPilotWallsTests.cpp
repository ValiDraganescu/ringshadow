// Automation tests for the pilot's cameras against the walls (`AcPilotWalls`,
// docs/citadel-interior.md §6). Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Pilot.ChaseStopsAtWalls; Quit" -TestExit="Automation Test Queue Empty"
#include "AcPilotWalls.h"
#include "AcSpace.h"

#include "Rules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// A boom from `From` to `To` (cells, z the height) against `Slabs`: where the camera stops.
	FVector Stop(const TArray<AcPilotWalls::FSlab>& Slabs, const FVector& From, const FVector& To)
	{
		const FVector A = AcSpace::ToWorld(ac::Vec2(From.X, From.Y), From.Z);
		const FVector B = AcSpace::ToWorld(ac::Vec2(To.X, To.Y), To.Z);
		const FVector At = A + (B - A) * AcPilotWalls::Sweep(A, B, AcSpace::ToCm(AcPilotWalls::ChaseRadius), Slabs);
		return FVector(AcSpace::ToSim(At).x, AcSpace::ToSim(At).y, AcSpace::HeightToSim(At));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPilotChaseWallsTest, "Autocraft.Pilot.ChaseStopsAtWalls", Flags)
bool FAcPilotChaseWallsTest::RunTest(const FString&)
{
	// The MMO's hollow Citadel (docs/citadel-interior.md §1), put back after.
	const bool bHall = ac::Rules::citadelHall();
	ac::Rules::setCitadelHall(true);
	const ac::Vec2 Was = AcSpace::Origin();
	AcSpace::SetOrigin(ac::Vec2(0, 0));
	ac::Structure Citadel(1, ac::StructureKind::citadel, 0, ac::Vec2(0, 0));
	TArray<AcPilotWalls::FSlab> Closed;
	AcPilotWalls::SlabsOf(Citadel, 0.0, Closed);
	TestTrue(TEXT("the Citadel has walls, a door and a roof"), Closed.Num() >= 7);
	constexpr double R = AcPilotWalls::ChaseRadius;

	// In the hall, backed toward the -y wall: the boom stops a sphere short of its inner face.
	const FVector Back = Stop(Closed, FVector(0, -3, 2), FVector(0, -8, 3));
	TestTrue(TEXT("stops inside the -y wall"), FMath::IsNearlyEqual(Back.Y, -5 + R, 0.01));
	// The roof is the hall's ceiling.
	const FVector Up = Stop(Closed, FVector(1, 1, 3), FVector(1, 1, 9));
	TestTrue(TEXT("stops under the roof"), FMath::IsNearlyEqual(Up.Z, 6 - R, 0.01));
	// Hugging the +x wall (the pivot already within the sphere of it): still never through.
	const FVector Hug = Stop(Closed, FVector(4.9, 0, 2), FVector(8, 0, 2));
	TestTrue(TEXT("a pivot at the wall stops at the wall"), Hug.X <= 5.0 + 1e-6);

	// Outside, the boom from the doorstep into the closed door: stopped at its face.
	const FVector Door = Stop(Closed, FVector(0, 8, 1.5), FVector(0, 2, 1.5));
	TestTrue(TEXT("the closed door stops it"), FMath::IsNearlyEqual(Door.Y, 6 + R, 0.01));
	// The door open: through the doorway, into the hall.
	Citadel.door = ac::Door{1.0, true};
	TArray<AcPilotWalls::FSlab> Open;
	AcPilotWalls::SlabsOf(Citadel, 0.0, Open);
	TestEqual(TEXT("the open door is no slab"), Open.Num(), Closed.Num() - 1);
	const FVector Through = Stop(Open, FVector(0, 8, 1.5), FVector(0, 2, 1.5));
	TestTrue(TEXT("through the open doorway"), FMath::IsNearlyEqual(Through.Y, 2.0, 0.01));
	// Over the lintel, the wall over the door still stops it.
	const FVector Lintel = Stop(Open, FVector(0, 8, 5), FVector(0, 2, 5));
	TestTrue(TEXT("the lintel stops it"), FMath::IsNearlyEqual(Lintel.Y, 6 + R, 0.01));
	// Half open is still shut to the camera.
	Citadel.door = ac::Door{0.5, true};
	TArray<AcPilotWalls::FSlab> Half;
	AcPilotWalls::SlabsOf(Citadel, 0.0, Half);
	TestEqual(TEXT("a half-open door is solid"), Half.Num(), Closed.Num());

	// Clear of every wall: the whole boom.
	const FVector Free = Stop(Closed, FVector(0, 12, 1.5), FVector(4, 16, 3));
	TestTrue(TEXT("nothing in the way"), FMath::IsNearlyEqual(Free.Y, 16.0, 0.01));
	// The Citadel is no low wall; a scaffold sunk under 3 cells is.
	bool bLow = true;
	for (const AcPilotWalls::FSlab& S : Closed) bLow = bLow && S.bLow;
	TestFalse(TEXT("the Citadel's walls are tall"), bLow);
	ac::Structure Hab(2, ac::StructureKind::habDome, 0, ac::Vec2(0, 0));
	TArray<AcPilotWalls::FSlab> Small;
	AcPilotWalls::SlabsOf(Hab, 0.0, Small);
	TestTrue(TEXT("a Hab Dome is a low wall"), Small.Num() == 1 && Small[0].bLow);
	// The hall's gun console (+x +y corner): the boom from the hall's middle
	// toward it at desk height stops short of it, a low obstacle.
	bool bConsoleLow = false;
	const FVector A = AcSpace::ToWorld(ac::Vec2(2, 2), 1.0), B = AcSpace::ToWorld(ac::Vec2(4.8, 4.8), 1.0);
	const double T = AcPilotWalls::Sweep(A, B, AcSpace::ToCm(R), Closed, &bConsoleLow);
	const FVector At = A + (B - A) * T;
	TestTrue(TEXT("the console stops the boom"), T < 1 && AcSpace::ToSim(At).x < 4.0);
	TestTrue(TEXT("the console is low"), bConsoleLow);
	AcSpace::SetOrigin(Was);
	ac::Rules::setCitadelHall(bHall);
	return true;
}

#endif
