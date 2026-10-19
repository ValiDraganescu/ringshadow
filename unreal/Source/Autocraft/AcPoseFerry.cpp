#include "AcPoseFerry.h"

#include "AcModelCatalog.h"
#include "AcPose.h"

#include <cmath>

namespace
{
	double Fract(const double X) { return X - std::floor(X); }

	void SetEmission(FAcPose& P, const int32 I, const double Value)
	{
		if (I != INDEX_NONE) P.Emission[I] = float(Value);
	}

	/// The part's rest turned by `Turn` in its own frame.
	void Turn(FAcPose& P, const FAcModelInfo& M, const int32 I, const FQuat& By)
	{
		if (I == INDEX_NONE) return;
		P.Local[I].SetRotation(M.Parts[I].Rest.GetRotation() * By);
	}

	/// The part's rest moved by `By` along its own axes (cm).
	void Push(FAcPose& P, const FAcModelInfo& M, const int32 I, const FVector& By)
	{
		if (I == INDEX_NONE) return;
		const FTransform& Rest = M.Parts[I].Rest;
		P.Local[I].SetTranslation(Rest.GetTranslation() + Rest.GetRotation().RotateVector(By));
	}
}

double AcPoseFerry::Pressed(const double Age)
{
	if (Age < 0.0 || Age >= PressOut) return 0.0;
	if (Age < PressIn) return Age / PressIn;
	const double T = (Age - PressIn) / (PressOut - PressIn);
	return 1.0 - T * T * (3.0 - 2.0 * T);
}

double AcPoseFerry::WiperOnGlass(const double Angle)
{
	const double F = FMath::Clamp((Angle + UE_DOUBLE_HALF_PI) / UE_DOUBLE_PI, 0.0, 1.0);
	return FMath::DegreesToRadians(WiperParkDeg + (WiperFarDeg - WiperParkDeg) * F);
}

void AcPoseFerry::Pose(const FAcModelInfo& M, FAcPose& P, const FAcFerryLook& In)
{
	static const FAcPartIndex Ramp(TEXT("ramp")), RampButton(TEXT("rampButton")), LampOpen(TEXT("rampLamps_0")),
		LampShut(TEXT("rampLamps_1")), Takeoff(TEXT("takeoffButton")), Nav(TEXT("navGlow"));
	static const FAcPartIndex Screens[2] = {FAcPartIndex(TEXT("consoleScreens_0")), FAcPartIndex(TEXT("consoleScreens_1"))};
	static const FAcPartIndex Wipers[2] = {FAcPartIndex(TEXT("wipers_0")), FAcPartIndex(TEXT("wipers_1"))};
	static const FAcPartIndex Fans[2] = {FAcPartIndex(TEXT("fans_0")), FAcPartIndex(TEXT("fans_1"))};
	static const FAcPartIndex Jets[2] = {FAcPartIndex(TEXT("engineGlow_0")), FAcPartIndex(TEXT("engineGlow_1"))};
	static const FAcPartIndex Beacons[2] = {FAcPartIndex(TEXT("beacon_0")), FAcPartIndex(TEXT("beacon_1"))};
	P.bAnimated = true;
	const double T = In.Time;

	// The ramp: about the ship's Y axis through the hinge, the top outward
	// (-X) and down; eased so it settles on the ground and in the frame.
	const double Open = FMath::Clamp(In.RampOpen, 0.0, 1.0);
	const double Eased = Open * Open * (3.0 - 2.0 * Open);
	Turn(P, M, Ramp.Get(M), FQuat(FVector::YAxisVector, -FMath::DegreesToRadians(RampOpenDeg * Eased)));

	// The ramp post: its button pushed in (toward the post, -Y: the post
	// stands on the left wall), the orange lamp lit while open and flashing
	// while the ramp moves, the green one while it is shut.
	Push(P, M, RampButton.Get(M), FVector(0.0, -RampButtonTravel * Pressed(In.RampPressAge), 0.0));
	const bool bMoving = In.RampWay != 0 && Open > 0.0 && Open < 1.0;
	const bool bFlash = In.RampWay != 0 && Fract(T * 2.0) < 0.5;
	const bool bShut = Open <= 0.0 && In.RampWay <= 0;
	SetEmission(P, LampOpen.Get(M), bMoving ? (bFlash ? 1.0 : 0.08) : (bShut ? 0.05 : 1.0));
	SetEmission(P, LampShut.Get(M), bShut ? 1.0 : 0.05);

	// The take-off button: lit (breathing a little) while the ship can go,
	// dim otherwise; pushed in along its axis on a press.
	Push(P, M, Takeoff.Get(M), FVector(0.0, 0.0, -TakeoffButtonTravel * Pressed(In.TakeoffPressAge)));
	SetEmission(P, Takeoff.Get(M), In.bCanTakeOff ? 0.85 + 0.15 * std::sin(T * 3.0) : 0.08);
	for (const FAcPartIndex& S : Screens) SetEmission(P, S.Get(M), In.bAboard ? 1.0 + 0.3 * In.Thrust : 0.25);

	// The wipers: about the glass's normal (their local X), positive to the
	// ship's right (+Y), which is a negative turn about the outward normal.
	const double W = WiperOnGlass(In.WiperAngle);
	for (const FAcPartIndex& Wi : Wipers) Turn(P, M, Wi.Get(M), FQuat(FVector::XAxisVector, -W));

	// The engines: the fans spin up and the jets brighten with the thrust.
	const double Thrust = FMath::Clamp(In.Thrust, 0.0, 1.0);
	for (int32 K = 0; K < 2; ++K)
	{
		const double Side = K == 0 ? 1.0 : -1.0;
		Turn(P, M, Fans[K].Get(M), FQuat(FVector::XAxisVector, Side * T * (4.0 + 30.0 * Thrust)));
		const double Flicker = 0.08 * std::sin(T * 37.0 + Side * 3.0) * std::sin(T * 13.0);
		SetEmission(P, Jets[K].Get(M), 0.25 + 1.6 * Thrust + Flicker * (0.3 + Thrust));
		// The fins' red beacons blink twice every two seconds.
		const double Ph = Fract(T * 0.5 + 0.25 * K);
		SetEmission(P, Beacons[K].Get(M), (Ph < 0.05 || (Ph > 0.12 && Ph < 0.17)) ? 1.6 : 0.2);
	}
	SetEmission(P, Nav.Get(M), 0.8 + 0.4 * Thrust);
}
