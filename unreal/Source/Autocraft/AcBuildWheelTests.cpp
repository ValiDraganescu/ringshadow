// Automation tests for the radial build wheel (C2, docs/guns-and-practice.md
// §5): the stick picks the slice under its direction, the first at the top
// and the rest clockwise, nothing inside the dead zone; the stick is held in
// its reach; the overlay paints the wheel. Run headless:
//   UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Hud.BuildWheelPicksBySector; Quit"
//     -TestExit="Automation Test Queue Empty"
#include "AcBuildWheel.h"
#include "AcPilotText.h"
#include "SAcPilotOverlay.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/HittestGrid.h"
#include "Layout/WidgetPath.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// A stick `L` points out at `A` radians clockwise from the top.
	FVector2D At(double A, double L) { return FVector2D(std::sin(A), std::cos(A)) * L; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcHudBuildWheelTest, "Autocraft.Hud.BuildWheelPicksBySector", Flags)
bool FAcHudBuildWheelTest::RunTest(const FString&)
{
	constexpr double Pi = UE_DOUBLE_PI;
	// The four compass points on a wheel of four, eleven and twelve.
	TestEqual(TEXT("up: the first slice"), FAcBuildWheel::Sector(FVector2D(0, 80), 4), 0);
	TestEqual(TEXT("right: the second (clockwise)"), FAcBuildWheel::Sector(FVector2D(80, 0), 4), 1);
	TestEqual(TEXT("down: the third"), FAcBuildWheel::Sector(FVector2D(0, -80), 4), 2);
	TestEqual(TEXT("left: the last"), FAcBuildWheel::Sector(FVector2D(-80, 0), 4), 3);
	TestEqual(TEXT("a hair left of up: still the first"), FAcBuildWheel::Sector(FVector2D(-5, 80), 4), 0);
	for (const int32 N : {4, 11, 12})
	{
		for (int32 I = 0; I < N; ++I)
		{
			const double Mid = FAcBuildWheel::SliceAngle(I, N), Half = Pi / N;
			TestEqual(FString::Printf(TEXT("%d of %d: its middle"), I, N), FAcBuildWheel::Sector(At(Mid, 60), N), I);
			TestEqual(FString::Printf(TEXT("%d of %d: inside its left edge"), I, N), FAcBuildWheel::Sector(At(Mid - Half + 0.01, 60), N), I);
			TestEqual(FString::Printf(TEXT("%d of %d: inside its right edge"), I, N), FAcBuildWheel::Sector(At(Mid + Half - 0.01, 60), N), I);
			TestEqual(FString::Printf(TEXT("%d of %d: past its right edge"), I, N), FAcBuildWheel::Sector(At(Mid + Half + 0.01, 60), N),
				(I + 1) % N);
		}
	}
	// The dead zone, and no slices.
	TestEqual(TEXT("inside the dead zone: none"), FAcBuildWheel::Sector(FVector2D(0, FAcBuildWheel::DeadZone - 1), 11), INDEX_NONE);
	TestEqual(TEXT("at the dead zone's edge: picked"), FAcBuildWheel::Sector(FVector2D(0, FAcBuildWheel::DeadZone), 11), 0);
	TestEqual(TEXT("no slices: none"), FAcBuildWheel::Sector(FVector2D(0, 80), 0), INDEX_NONE);

	// The stick: the mouse's motion since B, held in its reach, so a turn of
	// the hand swings it round at once.
	FAcBuildWheel W;
	TestEqual(TEXT("open: nothing picked"), W.Pick(11), INDEX_NONE);
	W.Move(FVector2D(10, 20));
	TestEqual(TEXT("a nudge: still nothing"), W.Pick(11), INDEX_NONE);
	W.Move(FVector2D(900, -20));
	TestTrue(TEXT("held in its reach"), FMath::IsNearlyEqual(W.Stick.Size(), FAcBuildWheel::Reach, 1e-9));
	TestEqual(TEXT("far right: the slice at three o'clock"), W.Pick(4), 1);
	W.Move(FVector2D(-2 * FAcBuildWheel::Reach, 0));
	TestEqual(TEXT("swung left: the slice at nine o'clock"), W.Pick(4), 3);
	W.Reset();
	TestEqual(TEXT("reset: nothing picked"), W.Pick(4), INDEX_NONE);

	// The overlay paints the wheel over the view: more than without it.
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		UE_LOG(LogTemp, Display, TEXT("Autocraft.Hud.BuildWheelPicksBySector: no Slate renderer, the paint left unchecked"));
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
	FAcPilotInfo Open = Plain;
	FAcBuildWheelInfo Wheel;
	for (int32 I = 0; I < 11; ++I)
	{
		FAcWheelSlice S;
		S.Kind = ac::StructureKind::citadel;
		S.Number = I + 1;
		S.Title = TEXT("CITADEL");
		S.Icon = TEXT("citadel");
		S.Ore = 400;
		S.State = I % 3 == 0 ? FAcWheelSlice::EState::Locked : FAcWheelSlice::EState::Build;
		S.Tag = S.State == FAcWheelSlice::EState::Locked ? TEXT("LV 8") : TEXT("×1 COPY");
		Wheel.Slices.Add(S);
	}
	Wheel.Stick = FVector2D(80, 0);
	Wheel.Pick = FAcBuildWheel::Sector(Wheel.Stick, Wheel.Slices.Num());
	Open.Wheel = Wheel;
	const int32 Without = Paint(Plain), With = Paint(Open);
	UE_LOG(LogTemp, Display, TEXT("Autocraft.Hud.BuildWheelPicksBySector: %d layers without the wheel, %d with"), Without, With);
	TestTrue(FString::Printf(TEXT("the wheel paints (%d layers without, %d with)"), Without, With), With > Without + 100);
	return true;
}

#endif
