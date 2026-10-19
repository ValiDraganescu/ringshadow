// Automation tests for the Citadel's interior in the game (C1,
// docs/citadel-interior.md §6, §7): the look at a corner gun (`AcCitadelAct`),
// the gun's live feed (`AcPilotText::Feed`, `SAcPilotOverlay`) and the
// Citadel's pose (the ramp, the stations, the hall lamps, the door panels,
// the turrets). Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Pilot.GunLookClamps+Autocraft.Hud.TurretFeedPaints+Autocraft.Pose.CitadelRamp; Quit"
//     -TestExit="Automation Test Queue Empty"
#include "AcCitadelAct.h"
#include "AcModelCatalog.h"
#include "AcPilotText.h"
#include "AcPose.h"
#include "AcPoseBuildings.h"
#include "AcSpace.h"
#include "SAcPilotOverlay.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"

#include "Defence.h"
#include "Rules.h"
#include "TerrainField.h"
#include "Types.h"
#include "WindowMaps.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;
	constexpr double Pi = UE_DOUBLE_PI;

	/// The MMO's hollow 12x12 Citadel for a test, the switch put back after.
	struct FHallOn
	{
		bool bWas = ac::Rules::citadelHall();
		FHallOn() { ac::Rules::setCitadelHall(true); }
		~FHallOn() { ac::Rules::setCitadelHall(bWas); }
	};

	ac::Structure Citadel()
	{
		ac::Structure S(7, ac::StructureKind::citadel, 0, ac::Vec2(20, 20));
		S.door = ac::Door{};
		S.guns = std::array<ac::CornerGun, 4>{};
		return S;
	}

	/// A Citadel model with the parts `PoseCitadel` drives, each at rest
	/// under the root (the real one may not have landed yet).
	FAcModelInfo HallModel()
	{
		FAcModelInfo M;
		M.Name = TEXT("citadel_test");
		M.Category = TEXT("building");
		auto Add = [&M](const FString& Name, const int32 Parent)
		{
			FAcModelPart P;
			P.Name = FName(*Name);
			P.Parent = Parent;
			M.PartIndex.Add(P.Name, M.Parts.Num());
			M.Parts.Add(P);
			return M.Parts.Num() - 1;
		};
		const int32 Root = Add(TEXT("citadel"), INDEX_NONE);
		Add(TEXT("ramp"), Root);
		for (int32 K = 0; K < 4; ++K)
		{
			Add(FString::Printf(TEXT("gunScreens_%d"), K), Root);
			const int32 T = Add(FString::Printf(TEXT("turrets_%d"), K), Root);
			const int32 C = Add(FString::Printf(TEXT("turretCradles_%d"), K), T);
			Add(FString::Printf(TEXT("turretBarrels_%d"), K), C);
		}
		for (int32 K = 0; K < 8; ++K) Add(FString::Printf(TEXT("hallLamps_%d"), K), Root);
		for (int32 K = 0; K < 2; ++K) Add(FString::Printf(TEXT("doorPanels_%d"), K), Root);
		return M;
	}

	struct FHallScene
	{
		ac::TerrainField Field{ac::WindowMaps::playground()};
		ac::GameState State;
		FAcUnitMemory Memory;
		FAcModelInfo Model = HallModel();

		FAcPose Pose(const ac::Structure& S)
		{
			State.time = 10.0;
			State.structures = {S};
			FAcPose P;
			FAcPoseContext C{State, nullptr, &State.structures[0], Model, Field, Memory, 10.0, 1.0 / 60, false, 0};
			AcPose::RestStructure(C, P);
			if (const FAcPoseFn Fn = AcPose::Find(S.kind)) Fn(C, P);
			return P;
		}
		int32 Part(const TCHAR* Name) const { return Model.PartIndex.FindChecked(FName(Name)); }
	};

	/// Where a part's rotation takes UE up (the ramp's top).
	FVector UpOf(const FAcPose& P, const int32 Part) { return P.Local[Part].GetRotation().RotateVector(FVector::UpVector); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPilotGunLookTest, "Autocraft.Pilot.GunLookClamps", Flags)
bool FAcPilotGunLookTest::RunTest(const FString&)
{
	const FHallOn Hall;
	const double Half = ac::Defence::gunArcHalf;
	for (int32 Gun = 0; Gun < 4; ++Gun)
	{
		const double Mid = ac::Defence::cornerAngle(Gun);
		// A turn past the stop holds at the stop, however far.
		TestTrue(TEXT("held at the right stop"), FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, Mid, 10.0), Mid + Half, 1e-9));
		TestTrue(TEXT("held at the left stop"), FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, Mid, -10.0), Mid - Half, 1e-9));
		// No wrap: from a stop, on across the hull's side (outside the arc), it stays.
		TestTrue(TEXT("no wrap past the stop"), FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, Mid + Half, 2.6), Mid + Half, 1e-9));
		TestTrue(TEXT("no wrap the other way"), FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, Mid - Half, -2.6), Mid - Half, 1e-9));
		// Back off the stop at once (nothing stored up past it).
		TestTrue(TEXT("back from the stop at once"),
			FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, AcCitadelAct::LookYaw(Gun, Mid, 10.0), -0.1), Mid + Half - 0.1, 1e-9));
		// Inside the arc it turns freely, and a look the far side of the circle comes into it.
		TestTrue(TEXT("free inside the arc"), FMath::IsNearlyEqual(AcCitadelAct::LookYaw(Gun, Mid + 0.3, 0.2), Mid + 0.5, 1e-9));
		TestTrue(TEXT("brought into the arc"),
			FMath::Abs(std::remainder(AcCitadelAct::LookYaw(Gun, Mid + 2 * Pi + 0.4, 0.0) - (Mid + 0.4), 2 * Pi)) < 1e-9);
	}
	TestEqual(TEXT("pitch's floor"), AcCitadelAct::LookPitch(ac::Defence::gunPitchMin - 0.2), ac::Defence::gunPitchMin);
	TestEqual(TEXT("pitch's ceiling"), AcCitadelAct::LookPitch(ac::Defence::gunPitchMax + 0.2), ac::Defence::gunPitchMax);
	TestEqual(TEXT("pitch inside"), AcCitadelAct::LookPitch(0.2), 0.2);
	// The guns' names by the HUD's compass (north is -y): +x+y, -x+y, -x-y, +x-y.
	TestEqual(TEXT("gun 0"), AcCitadelAct::GunName(0), FString(TEXT("1 SE")));
	TestEqual(TEXT("gun 1"), AcCitadelAct::GunName(1), FString(TEXT("2 SW")));
	TestEqual(TEXT("gun 2"), AcCitadelAct::GunName(2), FString(TEXT("3 NW")));
	TestEqual(TEXT("gun 3"), AcCitadelAct::GunName(3), FString(TEXT("4 NE")));
	// The eye: the gun's (Defence::gunEye) over the Citadel's ground.
	const ac::Vec2 Was = AcSpace::Origin();
	AcSpace::SetOrigin(ac::Vec2(0, 0));
	const ac::Structure S = Citadel();
	const ac::Vec3 E = ac::Defence::gunEye(S.position, 0, 0.5, 0.3);
	const FVector Eye = AcCitadelAct::Eye(S, 2.0, 0, 0.5, 0.3);
	TestTrue(TEXT("the eye over the deck"), Eye.Equals(FVector(E.x * 100, E.y * 100, (2.0 + E.z) * 100), 0.01));
	TestTrue(TEXT("the eye up on the roof"), E.z > ac::Defence::deckHeight);
	AcSpace::SetOrigin(Was);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcHudTurretFeedTest, "Autocraft.Hud.TurretFeedPaints", Flags)
bool FAcHudTurretFeedTest::RunTest(const FString&)
{
	const FHallOn Hall;
	ac::Structure S = Citadel();
	S.marks = ac::TurretMarks{2, 1, 0, 0};
	const double Mid = ac::Defence::cornerAngle(0);
	const std::optional<ac::UnitStats> W = ac::Defence::weapon(S);
	const double Cool = W ? W->cooldown : ac::UnitStats::citadelGuns.cooldown;

	// Cold and ready: no shots.
	FAcTurretFeed F = AcPilotText::Feed(S, 0, Mid + 0.4, 0.1, {}, 100.0);
	TestEqual(TEXT("its name"), F.Title, FString(TEXT("TURRET 1 · SE · CITADEL")));
	TestTrue(TEXT("the aim off the middle"), FMath::IsNearlyEqual(F.Off, 0.4, 1e-9));
	TestEqual(TEXT("cold"), F.Heat, 0);
	TestEqual(TEXT("ready"), F.Ready, 1.0);
	const std::optional<ac::UnitStats> Wm = ac::Defence::weapon(S);
	TestEqual(TEXT("its numbers: rounds a second, damage, muzzle speed"), F.Marks,
		FString::Printf(TEXT("%lld/s · %.1f · V0 90"), (long long)FMath::RoundToDouble(1.0 / Wm->cooldown), Wm->damage));
	// At the stop the needle sits on it.
	F = AcPilotText::Feed(S, 0, Mid + 3.0, 0.0, {}, 100.0);
	TestTrue(TEXT("held at the stop"), FMath::IsNearlyEqual(F.Off, F.ArcHalf, 1e-9));
	TestTrue(TEXT("the dial's middle straight up"), FMath::IsNearlyEqual(SAcPilotOverlay::GaugeAngle(0), 0.5 * Pi, 1e-9));
	TestTrue(TEXT("right is clockwise on the dial"), SAcPilotOverlay::GaugeAngle(0.5) < 0.5 * Pi);
	// C2: the gun's own heat: half hot, half the pips; at 1 OVERHEAT and
	// every pip; just fired: not ready.
	(*S.guns)[0].heat = 0.5;
	F = AcPilotText::Feed(S, 0, Mid, 0.0, {}, 100.0);
	TestEqual(TEXT("half hot: half the pips"), F.Heat, FAcTurretFeed::HeatPips / 2);
	TestFalse(TEXT("half hot: not overheated"), F.bOverheat);
	(*S.guns)[0].heat = 1.0;
	(*S.guns)[0].overheated = true;
	TArray<double> Shots = {100.0 - Cool * 0.5};
	F = AcPilotText::Feed(S, 0, Mid, 0.0, Shots, 100.0);
	TestEqual(TEXT("overheated: every pip"), F.Heat, FAcTurretFeed::HeatPips);
	TestTrue(TEXT("overheated: OVERHEAT"), F.bOverheat);
	TestTrue(TEXT("just fired: cooling"), F.Ready < 1.0);
	F = AcPilotText::Feed(S, 0, Mid, 0.0, {100.0 - 3 * Cool}, 100.0);
	TestEqual(TEXT("a round a while ago: ready"), F.Ready, 1.0);

	// The overlay paints the feed over the view: more than without it.
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		UE_LOG(LogTemp, Display, TEXT("Autocraft.Hud.TurretFeedPaints: no Slate renderer, the paint left unchecked"));
		return true;
	}
	TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1280, 720));
	TSharedRef<SAcPilotOverlay> Overlay = SNew(SAcPilotOverlay).Panel(true);
	const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(1280, 720), FSlateLayoutTransform());
	auto Paint = [&](const FAcPilotInfo& Info)
	{
		Overlay->Reset();
		Overlay->Show(Info, 100.0);
		FSlateWindowElementList Out(Window);
		FHittestGrid Grid;
		const FPaintArgs Args(&Overlay.Get(), Grid, FVector2D::ZeroVector, 100.0, 0.016f);
		return Overlay->OnPaint(Args, Geometry, FSlateRect(0, 0, 1280, 720), Out, 0, FWidgetStyle(), true);
	};
	FAcPilotInfo Plain;
	Plain.Name = TEXT("PROSPECTOR");
	Plain.Hp = Plain.MaxHp = 60;
	FAcPilotInfo AtGun = Plain;
	AtGun.Feed = AcPilotText::Feed(S, 0, Mid + 0.4, 0.1, Shots, 100.0);
	const int32 Without = Paint(Plain), With = Paint(AtGun);
	UE_LOG(LogTemp, Display, TEXT("Autocraft.Hud.TurretFeedPaints: %d layers without the feed, %d with"), Without, With);
	TestTrue(FString::Printf(TEXT("the feed paints (%d layers without, %d with)"), Without, With), With > Without + 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPoseCitadelRampTest, "Autocraft.Pose.CitadelRamp", Flags)
bool FAcPoseCitadelRampTest::RunTest(const FString&)
{
	const FHallOn Hall;
	FHallScene Scene;
	ac::Structure S = Citadel();
	const int32 Ramp = Scene.Part(TEXT("ramp"));

	// Shut: the ramp stands in the doorway.
	FAcPose P = Scene.Pose(S);
	TestTrue(TEXT("shut: standing"), UpOf(P, Ramp).Equals(FVector::UpVector, 1e-4));
	// Open: 90 degrees outward about its hinge, its top to the front (+Y).
	S.door->open = 1.0;
	P = Scene.Pose(S);
	TestTrue(FString::Printf(TEXT("open: flat, its top out the front (%s)"), *UpOf(P, Ramp).ToString()),
		UpOf(P, Ramp).Equals(FVector(0, 1, 0), 1e-4));
	S.door->open = 0.5;
	P = Scene.Pose(S);
	TestTrue(TEXT("half open: 45 degrees"), UpOf(P, Ramp).Equals(FVector(0, 1, 1).GetSafeNormal(), 1e-4));

	// A manned gun's station screen lit blue; the rest dark glass.
	(*S.guns)[1].gunner = 42;
	P = Scene.Pose(S);
	TestTrue(TEXT("gun 2's screen lit"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("gunScreens_1"))], 1.9f / 0.04f, 0.01f));
	TestTrue(TEXT("gun 1's screen dark"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("gunScreens_0"))], 1.f, 0.001f));
	// The hall lamps steady once complete.
	TestTrue(TEXT("hall lamps lit"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("hallLamps_3"))], 1.f, 0.001f));
	// The door panel on the pilot's side while in reach.
	AcCitadelAct::SetPanel(S.id, 1);
	P = Scene.Pose(S);
	TestTrue(TEXT("the inner panel lit"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("doorPanels_1"))], 1.f, 0.001f));
	TestTrue(TEXT("the outer one dark"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("doorPanels_0"))], 0.05f, 0.001f));
	AcCitadelAct::SetPanel(0, INDEX_NONE);
	P = Scene.Pose(S);
	TestTrue(TEXT("no panel out of reach"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("doorPanels_1"))], 0.05f, 0.001f));

	// The turrets: shown once complete (no marks needed), turned by the aim,
	// the cradle pitched by the pitch.
	(*S.guns)[0].aim = ac::Defence::cornerAngle(0) + 0.5;
	(*S.guns)[0].pitch = 0.4;
	P = Scene.Pose(S);
	TestFalse(TEXT("no marks"), S.marks.has_value());
	TestTrue(TEXT("turret shown"), P.Visible[Scene.Part(TEXT("turrets_0"))]);
	TestTrue(TEXT("the cradle pitched"), !P.Local[Scene.Part(TEXT("turretCradles_0"))].GetRotation().Equals(FQuat::Identity, 1e-3));
	TestTrue(TEXT("gun 2's cradle level"), P.Local[Scene.Part(TEXT("turretCradles_1"))].GetRotation().Equals(FQuat::Identity, 1e-6));

	// The real model (Tools/Editor/models/citadel.py): every part the pose
	// drives, and its ramp opening outward about its hinge.
	if (const FAcModelInfo* Real = FAcModelCatalog::Get().Find(FName(TEXT("citadel_blue"))))
	{
		for (const TCHAR* Name : {TEXT("ramp"), TEXT("doorPanels_0"), TEXT("doorPanels_1"), TEXT("gunScreens_0"), TEXT("gunScreens_3"),
				 TEXT("hallLamps_0"), TEXT("hallLamps_7"), TEXT("turrets_0"), TEXT("turretCradles_0"), TEXT("turretBarrels_0"),
				 TEXT("turretMuzzles_3")})
		{
			TestTrue(FString::Printf(TEXT("citadel_blue has %s"), Name), Real->PartIndex.Contains(FName(Name)));
		}
		if (const int32* R = Real->PartIndex.Find(FName(TEXT("ramp"))))
		{
			// The ramp's top (up in its parent at rest) once the door is open.
			FHallScene Live;
			Live.Model = *Real;
			ac::Structure Open = Citadel();
			Open.door->open = 1.0;
			const FQuat Rest = Real->Parts[*R].Rest.GetRotation();
			const FVector Top = (Live.Pose(Open).Local[*R].GetRotation() * Rest.Inverse()).RotateVector(FVector::UpVector);
			TestTrue(FString::Printf(TEXT("the real ramp lies out the front (its top %s)"), *Top.ToString()), Top.Equals(FVector(0, 1, 0), 1e-3));
		}
	}
	else
	{
		AddError(TEXT("no citadel_blue in the catalog"));
	}

	// Under construction: dark, the ramp shut, the turrets hidden.
	ac::Structure Site(8, ac::StructureKind::citadel, 0, ac::Vec2(20, 20), 30.0);
	P = Scene.Pose(Site);
	TestTrue(TEXT("building: hall dark"), FMath::IsNearlyEqual(P.Emission[Scene.Part(TEXT("hallLamps_0"))], 0.05f, 0.001f));
	TestFalse(TEXT("building: no turrets"), P.Visible[Scene.Part(TEXT("turrets_0"))]);
	TestTrue(TEXT("the Citadel stands as tall as the core says"),
		FMath::IsNearlyEqual(AcPose::BuildingHeight(ac::StructureKind::citadel), ac::Rules::height(ac::StructureKind::citadel), 1e-9)
			&& AcPose::BuildingHeight(ac::StructureKind::citadel) > ac::Defence::deckHeight);
	return true;
}

#endif
