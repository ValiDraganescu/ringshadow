// The walk-in Dropship's pose (AcPoseFerry.h) on its model (`ferry_blue`).
//
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Pose.Ferry
#include "AcModelCatalog.h"
#include "AcPose.h"
#include "AcPoseFerry.h"

#include "Misc/AutomationTest.h"

#include "Wipers.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// Part `I`'s transform in the model's frame (cm) from the pose's locals.
	FTransform ToModel(const FAcPose& P, const FAcModelInfo& M, int32 I)
	{
		FTransform T = FTransform::Identity;
		while (I > 0)
		{
			T = T * P.Local[I];
			I = M.Parts[I].Parent;
		}
		return T;
	}

	struct FFerryScene
	{
		const FAcModelInfo* M = FAcModelCatalog::Get().Find(FName(AcPoseFerry::Model));
		int32 Part(const TCHAR* Name) const { return M ? FAcPartIndex(FName(Name)).Get(*M) : INDEX_NONE; }
		FAcPose Pose(const FAcFerryLook& In) const
		{
			FAcPose P;
			P.Reset(*M);
			AcPoseFerry::Pose(*M, P, In);
			return P;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPoseFerryRampTest, "Autocraft.Pose.Ferry.Ramp", Flags)
bool FAcPoseFerryRampTest::RunTest(const FString&)
{
	const FFerryScene S;
	if (!TestNotNull(TEXT("the ferry_blue model"), S.M)) return false;
	const int32 Ramp = S.Part(TEXT("ramp"));
	if (!TestTrue(TEXT("a ramp part"), Ramp != INDEX_NONE)) return false;
	// The ramp's top edge at rest: over the hinge (x -250, z 50), 2 cells up.
	const FVector Top(-250.0, 0.0, 250.0);
	const FTransform Rest = ToModel(S.Pose({}), *S.M, Ramp);
	const FVector TopLocal = Rest.InverseTransformPosition(Top);
	FAcFerryLook Open;
	Open.RampOpen = 1.0;
	const FVector Down = ToModel(S.Pose(Open), *S.M, Ramp).TransformPosition(TopLocal);
	// Open: the top lies on the ground 2.0 behind the hinge's foot (x -443.6).
	TestTrue(FString::Printf(TEXT("open: the top on the ground behind the ship (%s)"), *Down.ToString()),
		FMath::IsNearlyEqual(Down.Z, 0.0, 1.0) && FMath::IsNearlyEqual(Down.X, -443.6, 1.5));
	Open.RampOpen = 0.5;
	const FVector Half = ToModel(S.Pose(Open), *S.M, Ramp).TransformPosition(TopLocal);
	TestTrue(FString::Printf(TEXT("half open: out the back and over the deck (%s)"), *Half.ToString()), Half.X < -300.0 && Half.Z > 150.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPoseFerryWipersTest, "Autocraft.Pose.Ferry.Wipers", Flags)
bool FAcPoseFerryWipersTest::RunTest(const FString&)
{
	const FFerryScene S;
	if (!TestNotNull(TEXT("the ferry_blue model"), S.M)) return false;
	for (const TCHAR* Name : {TEXT("wipers_0"), TEXT("wipers_1")})
	{
		const int32 W = S.Part(Name);
		if (!TestTrue(FString::Printf(TEXT("a part %s"), Name), W != INDEX_NONE)) return false;
		// The blade runs along the part's local Z (up the glass); its pivot is the part's origin.
		auto Tip = [&](const double Angle)
		{
			FAcFerryLook L;
			L.WiperAngle = Angle;
			const FTransform T = ToModel(S.Pose(L), *S.M, W);
			return T.TransformPosition(FVector(0, 0, 50)) - T.GetLocation();
		};
		const FVector Park = Tip(ac::Wipers::ParkAngle), Up = Tip(0.0), Far = Tip(ac::Wipers::FarAngle);
		TestTrue(FString::Printf(TEXT("%s parked: lying to the left along the sill (%s)"), Name, *Park.ToString()), Park.Y < -40.0 && Park.Z < 20.0);
		TestTrue(FString::Printf(TEXT("%s halfway: up the glass and leaning back (%s)"), Name, *Up.ToString()),
			Up.Z > 30.0 && Up.X < 0.0 && FMath::Abs(Up.Y) < 25.0);
		TestTrue(FString::Printf(TEXT("%s far: out to the right (%s)"), Name, *Far.ToString()), Far.Y > 35.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPoseFerryButtonsTest, "Autocraft.Pose.Ferry.Buttons", Flags)
bool FAcPoseFerryButtonsTest::RunTest(const FString&)
{
	const FFerryScene S;
	if (!TestNotNull(TEXT("the ferry_blue model"), S.M)) return false;
	const int32 Ramp = S.Part(TEXT("rampButton")), Take = S.Part(TEXT("takeoffButton"));
	const int32 Open = S.Part(TEXT("rampLamps_0")), Shut = S.Part(TEXT("rampLamps_1"));
	if (!TestTrue(TEXT("the buttons and lamps"), Ramp != INDEX_NONE && Take != INDEX_NONE && Open != INDEX_NONE && Shut != INDEX_NONE)) return false;
	TestEqual(TEXT("a press: out at once"), AcPoseFerry::Pressed(0.0), 0.0);
	TestEqual(TEXT("a press: all the way in"), AcPoseFerry::Pressed(AcPoseFerry::PressIn), 1.0);
	TestEqual(TEXT("a press: back out"), AcPoseFerry::Pressed(AcPoseFerry::PressOut), 0.0);

	FAcFerryLook L;
	const FAcPose Rest = S.Pose(L);
	L.RampPressAge = AcPoseFerry::PressIn;
	L.TakeoffPressAge = AcPoseFerry::PressIn;
	const FAcPose In = S.Pose(L);
	const FVector RampMove = ToModel(In, *S.M, Ramp).GetLocation() - ToModel(Rest, *S.M, Ramp).GetLocation();
	TestTrue(FString::Printf(TEXT("the ramp button goes into its post (-Y, %s)"), *RampMove.ToString()),
		RampMove.Equals(FVector(0, -AcPoseFerry::RampButtonTravel, 0), 0.01));
	const FVector TakeMove = ToModel(In, *S.M, Take).GetLocation() - ToModel(Rest, *S.M, Take).GetLocation();
	TestTrue(FString::Printf(TEXT("the take-off button goes down into the desk (%s)"), *TakeMove.ToString()),
		FMath::IsNearlyEqual(TakeMove.Size(), AcPoseFerry::TakeoffButtonTravel, 0.01) && TakeMove.Z < -1.5);

	// The lamps: shut, the green one; open, the orange one; moving, the orange flashes.
	TestTrue(TEXT("shut: green lit, orange dark"), Rest.Emission[Shut] > 0.9f && Rest.Emission[Open] < 0.1f);
	FAcFerryLook O;
	O.RampOpen = 1.0;
	const FAcPose Down = S.Pose(O);
	TestTrue(TEXT("open: orange lit, green dark"), Down.Emission[Open] > 0.9f && Down.Emission[Shut] < 0.1f);
	O.RampOpen = 0.5;
	O.RampWay = -1;
	O.Time = 0.1;
	const float A = S.Pose(O).Emission[Open];
	O.Time = 0.35;
	const float B = S.Pose(O).Emission[Open];
	TestTrue(FString::Printf(TEXT("moving: the orange flashes (%.2f, %.2f)"), A, B), FMath::Abs(A - B) > 0.5f);
	// The take-off button: lit only when the ship can go.
	FAcFerryLook T;
	TestTrue(TEXT("take-off dark"), S.Pose(T).Emission[Take] < 0.2f);
	T.bCanTakeOff = true;
	TestTrue(TEXT("take-off lit"), S.Pose(T).Emission[Take] > 0.6f);
	return true;
}

#endif
