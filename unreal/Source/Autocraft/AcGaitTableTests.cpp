// Automation tests for the gait tables (AcGaitTable.h): sampling, the
// prospector's measured walk, and the pose that plays it. Run headless:
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Gait
#include "AcGaitTable.h"
#include "AcSpace.h"
#include "AcModelCatalog.h"
#include "AcPose.h"

#include "Misc/AutomationTest.h"
#include "TerrainField.h"
#include "Types.h"
#include "WindowMaps.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcGaitSampleTest, "Autocraft.Gait.Sample", Flags)
bool FAcGaitSampleTest::RunTest(const FString&)
{
	const TArray<double> C = {0.0, 1.0, 0.0, -1.0};
	TestEqual(TEXT("a bin's own value"), FAcGaitTable::Sample(C, 0.25), 1.0, 1e-9);
	TestEqual(TEXT("halfway between bins"), FAcGaitTable::Sample(C, 0.125), 0.5, 1e-9);
	TestEqual(TEXT("the last bin wraps to the first"), FAcGaitTable::Sample(C, 0.875), -0.5, 1e-9);
	TestEqual(TEXT("a phase past 1 wraps"), FAcGaitTable::Sample(C, 1.25), 1.0, 1e-9);
	TestEqual(TEXT("a negative phase wraps"), FAcGaitTable::Sample(C, -0.75), 1.0, 1e-9);
	TestEqual(TEXT("an empty channel is 0"), FAcGaitTable::Sample({}, 0.3), 0.0, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcGaitProspectorTableTest, "Autocraft.Gait.ProspectorTable", Flags)
bool FAcGaitProspectorTableTest::RunTest(const FString&)
{
	const FAcGaitTable* T = FAcGaitTable::Find(TEXT("prospector_walk"));
	if (!T)
	{
		AddError(TEXT("no Content/Models/Gaits/prospector_walk.json"));
		return false;
	}
	TestTrue(TEXT("three channels of the same length"), T->Hip.Num() >= 16 && T->Hip.Num() == T->Knee.Num() && T->Knee.Num() == T->Foot.Num());
	double HipMin = 0, HipMax = 0, KneeMax = 0;
	for (int32 I = 0; I < T->Hip.Num(); ++I)
	{
		HipMin = FMath::Min(HipMin, T->Hip[I]);
		HipMax = FMath::Max(HipMax, T->Hip[I]);
		KneeMax = FMath::Max(KneeMax, T->Knee[I]);
		TestTrue(TEXT("angles are radians within a leg's reach"), std::abs(T->Hip[I]) < 1.6 && std::abs(T->Knee[I]) < 2.6 && std::abs(T->Foot[I]) < 1.6);
	}
	TestTrue(TEXT("the hip swings forward and back"), HipMax > 0.3 && HipMin < -0.3);
	TestTrue(TEXT("the knee lifts"), KneeMax > 0.5);
	TestTrue(TEXT("phase 0 is the hip's upward zero crossing"), T->HipAt(0.0) > -0.1 && T->HipAt(0.05) > T->HipAt(0.0));
	TestTrue(TEXT("no gait table is null"), FAcGaitTable::Find(TEXT("no_such_gait")) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcGaitProspectorPoseTest, "Autocraft.Gait.ProspectorPose", Flags)
bool FAcGaitProspectorPoseTest::RunTest(const FString&)
{
	const FAcModelInfo* Model = FAcModelCatalog::Get().Find(FName(TEXT("prospector_blue")));
	const FAcPoseFn Fn = AcPose::Find(ac::UnitKind::prospector);
	if (!Model || !Fn)
	{
		AddError(TEXT("no prospector model or pose"));
		return false;
	}
	const int32* L0 = Model->PartIndex.Find(FName(TEXT("legs_0")));
	const int32* L1 = Model->PartIndex.Find(FName(TEXT("legs_1")));
	if (!L0 || !L1)
	{
		AddError(TEXT("the prospector has no legs_0 / legs_1"));
		return false;
	}
	ac::TerrainField Field{ac::WindowMaps::playground()};
	ac::GameState State;
	State.time = 10.0;
	FAcUnitMemory Memory;
	auto Posed = [&](const double Stride, const bool bWalking)
	{
		ac::Unit U(7, ac::UnitKind::prospector, 0, ac::Vec2(0, 0), 0.0, ac::Unit::Task::idle);
		U.stride = Stride;
		U.moving = bWalking;
		FAcPose P;
		FAcPoseContext C{State, &U, nullptr, *Model, Field, Memory, 10.0, 0.0, false, 0};
		AcPose::RestUnit(C, P);
		Fn(C, P);
		return P;
	};
	// Standing: both legs at rest. Walking: the legs differ, and the hull
	// moves up and down with the lower sole (the sink), within reason.
	const FAcPose Still = Posed(0.0, false);
	TestTrue(TEXT("standing, the legs rest"), Still.Local[*L0].Equals(Model->Parts[*L0].Rest, 1e-6) && Still.Local[*L1].Equals(Model->Parts[*L1].Rest, 1e-6));
	int32 Apart = 0;
	double ZMin = 1e9, ZMax = -1e9;
	const double ZRest = Still.Placement.GetTranslation().Z;
	for (double S = 0.0; S < 2.0; S += 0.1)
	{
		const FAcPose P = Posed(S, true);
		// Where each leg hangs: its down vector's forward (UE X) component.
		const double A0 = P.Local[*L0].GetRotation().RotateVector(FVector(0, 0, -1)).X;
		const double A1 = P.Local[*L1].GetRotation().RotateVector(FVector(0, 0, -1)).X;
		if (std::abs(A0 - A1) > 0.05) ++Apart;
		const double Z = P.Placement.GetTranslation().Z;
		ZMin = FMath::Min(ZMin, Z);
		ZMax = FMath::Max(ZMax, Z);
	}
	TestTrue(TEXT("walking, the legs are apart most of the stride"), Apart >= 15);
	TestTrue(TEXT("the hull bobs"), ZMax - ZMin > 1.0);
	TestTrue(TEXT("the hull stays within 30 cm of its rest"), ZMin > ZRest - 30.0 && ZMax < ZRest + 30.0);
	return true;
}

/// The drill stroke: the table has its clocked channels, and a mining
/// Prospector's drill arm and hull move through the stroke.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcGaitProspectorDrillTest, "Autocraft.Gait.ProspectorDrill", Flags)
bool FAcGaitProspectorDrillTest::RunTest(const FString&)
{
	const FAcGaitTable* T = FAcGaitTable::Find(TEXT("prospector_drill"));
	if (!T)
	{
		AddError(TEXT("no prospector_drill table (Content/Models/Gaits)"));
		return false;
	}
	TestTrue(TEXT("lean, push and pitch channels"), T->Has(TEXT("lean")) && T->Has(TEXT("push")) && T->Has(TEXT("pitch")));
	TestTrue(TEXT("a clocked stroke"), T->PeriodS > 0.5 && T->PeriodS < 10.0);
	double LeanMin = 1e9, LeanMax = -1e9, PushMin = 1e9, PushMax = -1e9, PitchMax = -1e9;
	for (double Ph = 0.0; Ph < 1.0; Ph += 1.0 / 64)
	{
		LeanMin = FMath::Min(LeanMin, T->At(TEXT("lean"), Ph));
		LeanMax = FMath::Max(LeanMax, T->At(TEXT("lean"), Ph));
		PushMin = FMath::Min(PushMin, T->At(TEXT("push"), Ph));
		PushMax = FMath::Max(PushMax, T->At(TEXT("push"), Ph));
		PitchMax = FMath::Max(PitchMax, T->At(TEXT("pitch"), Ph));
	}
	TestTrue(TEXT("the hull rocks a few degrees"), LeanMax - LeanMin > 0.05 && LeanMax - LeanMin < 0.6);
	TestTrue(TEXT("the arm slides a few cm, within reach"), PushMax - PushMin > 0.02 && PushMin > -0.3 && PushMax < 0.3);
	TestTrue(TEXT("the drill dips, not past 45 degrees"), PitchMax > 0.1 && PitchMax < 0.8);
	TestEqual(TEXT("an unknown channel samples 0"), T->At(TEXT("nothing"), 0.3), 0.0);

	const FAcModelInfo* Model = FAcModelCatalog::Get().Find(FName(TEXT("prospector_blue")));
	const FAcPoseFn Fn = AcPose::Find(ac::UnitKind::prospector);
	const int32* Arm = Model ? Model->PartIndex.Find(FName(TEXT("drillArm"))) : nullptr;
	const int32* Body = Model ? Model->PartIndex.Find(FName(TEXT("body"))) : nullptr;
	if (!Model || !Fn || !Arm || !Body)
	{
		AddError(TEXT("no prospector model, pose, drillArm or body"));
		return false;
	}
	ac::TerrainField Field{ac::WindowMaps::playground()};
	ac::GameState State;
	FAcUnitMemory Memory;
	ac::Unit U(7, ac::UnitKind::prospector, 0, ac::Vec2(0, 0), 0.0, ac::Unit::Task::mining);
	U.timer = 5.0;
	U.moving = false;
	auto Posed = [&](const double Time)
	{
		State.time = Time;
		FAcPose P;
		FAcPoseContext C{State, &U, nullptr, *Model, Field, Memory, Time, 0.0, false, 0};
		AcPose::RestUnit(C, P);
		Fn(C, P);
		return P;
	};
	// Over a stroke the arm's slide and pitch and the hull's lean each cover their table's range.
	double ArmXMin = 1e9, ArmXMax = -1e9, ArmPitchMax = -1e9, BodyMin = 1e9, BodyMax = -1e9;
	for (double Time = 10.0; Time < 10.0 + T->PeriodS; Time += T->PeriodS / 32)
	{
		const FAcPose P = Posed(Time);
		const double X = P.Local[*Arm].GetTranslation().X;
		ArmXMin = FMath::Min(ArmXMin, X);
		ArmXMax = FMath::Max(ArmXMax, X);
		// The drill arm's forward (UE X), pitched down: its Z goes negative.
		ArmPitchMax = FMath::Max(ArmPitchMax, -P.Local[*Arm].GetRotation().RotateVector(FVector(1, 0, 0)).Z);
		// The hull's up vector tips forward and back.
		const double Up = P.Local[*Body].GetRotation().RotateVector(FVector(0, 0, 1)).X;
		BodyMin = FMath::Min(BodyMin, Up);
		BodyMax = FMath::Max(BodyMax, Up);
	}
	TestTrue(TEXT("the arm slides through the stroke (cm)"), ArmXMax - ArmXMin > AcSpace::ToCm(PushMax - PushMin) * 0.9);
	TestTrue(TEXT("the drill dips"), ArmPitchMax > std::sin(PitchMax) * 0.9);
	TestTrue(TEXT("the hull rocks"), BodyMax - BodyMin > std::sin(LeanMax - LeanMin) * 0.9);
	return true;
}

#endif
