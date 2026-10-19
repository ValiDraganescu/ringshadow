// Automation tests for where the game's shots end and the Sentinel's pods
// point (docs/guns-and-practice.md, "Range and line of sight": every weapon
// aims at the target's body at its height now). The core's side is
// unreal/core-tests/tests/AimTests.cpp. Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Aim; Quit"
#include "AcKindTable.h"
#include "AcModelCatalog.h"
#include "AcPilotAim.h"
#include "AcPose.h"
#include "AcSpace.h"

#include "Misc/AutomationTest.h"

#include "Brood.h"
#include "Defence.h"
#include "TerrainField.h"
#include "Types.h"
#include "WindowMaps.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// One unit or building posed on the playground's ground, as the world
	/// renderer draws it: its parts' world transforms.
	struct FAimScene
	{
		ac::TerrainField Field{ac::WindowMaps::playground()};
		ac::GameState State;
		FAcUnitMemory Memory;
		const FAcModelInfo* Model = nullptr;
		FAcPose Pose;
		TArray<FTransform> World;

		explicit FAimScene(const FString& ModelName)
		{
			Model = FAcModelCatalog::Get().Find(FName(*ModelName));
			State.time = 10.0;
		}

		void Compose()
		{
			World.SetNum(Model->Parts.Num());
			for (int32 P = 0; P < Model->Parts.Num(); ++P) World[P] = AcPilotAim::PartWorld(*Model, Pose, P);
		}

		void PoseUnit(const ac::Unit& U)
		{
			Pose = FAcPose();
			const FAcPoseContext C{State, &U, nullptr, *Model, Field, Memory, State.time, 0.0, false, 0};
			AcPose::RestUnit(C, Pose);
			if (const FAcPoseFn Fn = AcPose::Find(U.kind)) Fn(C, Pose);
			Compose();
		}

		void PoseStructure(const ac::Structure& S, const double Dt)
		{
			Pose = FAcPose();
			const FAcPoseContext C{State, nullptr, &S, *Model, Field, Memory, State.time, Dt, false, 0};
			AcPose::RestStructure(C, Pose);
			if (const FAcPoseFn Fn = AcPose::Find(S.kind)) Fn(C, Pose);
			Compose();
		}

		int32 Part(const TCHAR* Name) const
		{
			const int32* I = Model->PartIndex.Find(FName(Name));
			return I ? *I : INDEX_NONE;
		}
	};

	const ac::Vec2 At{20, 20};

	ac::Unit Gloamwing(const double Height, const bool bDive)
	{
		ac::Unit U(7, ac::UnitKind::gloamwing, 1, At, 0, ac::Unit::Task::idle);
		U.bug = ac::Bug{};
		U.bug->height = Height;
		if (bDive)
		{
			U.bug->state = ac::Bug::State::dive;
			U.bug->diveFrom = 9.8;
		}
		return U;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcAimBodyTest, "Autocraft.Aim.Body", Flags)
bool FAcAimBodyTest::RunTest(const FString&)
{
	// The user's case (2026-10-19): a Sentinel's missiles flew to 2 cells
	// over the ground under a hovering Gloamwing, and the units' shots to its
	// feet: the effects looked for a `body` part the Gloamwing hasn't. Shots
	// now end on the drawn body: the Gloamwing's thorax at its height,
	// hovering or diving; a Kestrel's hull at its hover.
	struct FCase
	{
		const TCHAR* Name;
		ac::Unit U;
		double Height;
	};
	const FCase Cases[] = {
		{TEXT("a Gloamwing hovering"), Gloamwing(ac::Brood::hoverHeight, false), ac::Brood::hoverHeight},
		{TEXT("a Gloamwing diving"), Gloamwing(ac::Brood::diveHeight, true), ac::Brood::diveHeight},
		{TEXT("a Gloamwing on its way down"), Gloamwing(1.6, true), 1.6},
		{TEXT("a Kestrel"), ac::Unit(8, ac::UnitKind::kestrel, 0, At, 0, ac::Unit::Task::idle), ac::Rules::stats(ac::UnitKind::kestrel).hover},
	};
	for (const FCase& K : Cases)
	{
		FAimScene Scene(AcKinds::Look(K.U.kind).Model);
		if (!Scene.Model)
		{
			AddError(FString::Printf(TEXT("no model %s"), *AcKinds::Look(K.U.kind).Model));
			continue;
		}
		const double Ground = Scene.Field.height(At);
		// The core's body and the game's agree.
		TestEqual(*FString::Printf(TEXT("%s: the game's body height is the core's"), K.Name), AcPose::BodyHeight(K.U), K.Height, 1e-9);
		Scene.PoseUnit(K.U);
		const FVector Body = AcPose::BodyPoint(K.U, Scene.Model, Scene.World, Ground);
		const double Up = AcSpace::ToCells(Body.Z) - Ground;
		// Its body part, as drawn, give or take the bob.
		TestTrue(*FString::Printf(TEXT("%s: the shots end on its body (%.2f cells up, its height %.2f)"), K.Name, Up, K.Height),
			std::abs(Up - K.Height) < 0.2);
		const int32 Part = Scene.Part(*AcKinds::Look(K.U.kind).BodyPart.ToString());
		if (TestTrue(*FString::Printf(TEXT("%s: has its body part"), K.Name), Part != INDEX_NONE))
		{
			TestTrue(*FString::Printf(TEXT("%s: at the drawn part"), K.Name), Body.Equals(Scene.World[Part].GetLocation(), 0.01));
		}
		TestTrue(*FString::Printf(TEXT("%s: over its ground point"), K.Name),
			FVector2D(Body.X, Body.Y).Equals(FVector2D(AcSpace::ToWorld(At, 0).X, AcSpace::ToWorld(At, 0).Y), 40.0));
		// Not drawn: the core's height over its ground, not the ground.
		const FVector Far = AcPose::BodyPoint(K.U, nullptr, {}, Ground);
		TestEqual(*FString::Printf(TEXT("%s: not drawn, at the core's height"), K.Name), AcSpace::ToCells(Far.Z) - Ground, K.Height, 1e-6);
	}
	// A walker's chest, over its drawn root.
	const ac::Unit Ranger(9, ac::UnitKind::ranger, 0, At, 0, ac::Unit::Task::idle);
	TestEqual(TEXT("a Ranger: its chest"), AcPose::BodyHeight(Ranger), AcKinds::Look(ac::UnitKind::ranger).ChestHeight, 1e-9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcAimSentinelPodsTest, "Autocraft.Aim.SentinelPods", Flags)
bool FAcAimSentinelPodsTest::RunTest(const FString&)
{
	// The pods pitch by the core's solve (`Structure::pitch`): up at a
	// Gloamwing hovering 5 cells off, lower at one nearer the ground; at rest
	// while it is built.
	FAimScene Scene(TEXT("sentinel_blue"));
	if (!Scene.Model)
	{
		AddError(TEXT("no model sentinel_blue"));
		return false;
	}
	const int32 Pod = Scene.Part(TEXT("pods_0"));
	const int32 Muzzle = Scene.Part(TEXT("muzzles_0"));
	if (!TestTrue(TEXT("the Sentinel has pods_0 and muzzles_0"), Pod != INDEX_NONE && Muzzle != INDEX_NONE)) return false;
	ac::Structure S(5, ac::StructureKind::sentinel, 0, At);
	S.hp = ac::Rules::hp(ac::StructureKind::sentinel);
	S.target = 7;
	S.aim = 0;
	const auto PitchOf = [&Scene, Pod]()
	{
		const FVector F = Scene.World[Pod].GetRotation().RotateVector(FVector(1, 0, 0));
		return std::atan2(F.Z, FVector2D(F.X, F.Y).Size());
	};
	for (const double H : {ac::Brood::hoverHeight, 1.2, 0.6})
	{
		S.pitch = ac::Defence::sentinelPitch(5, H);
		Scene.PoseStructure(S, 1.0);
		TestEqual(*FString::Printf(TEXT("the pods pitch onto a body %.1f cells up"), H), PitchOf(), S.pitch, 0.01);
	}
	S.buildLeft = 5;
	S.pitch = 0.9;
	Scene.PoseStructure(S, 1.0);
	TestEqual(TEXT("built: the pods at rest"), PitchOf(), ac::Defence::sentinelRest, 0.01);
	return true;
}

#endif
