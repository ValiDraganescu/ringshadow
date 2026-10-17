#include "AcPilotCamera.h"

#include "AcKindTable.h"
#include "AcSpace.h"

#include "Rules.h"

#include "HAL/IConsoleManager.h"

#include <cmath>

namespace
{
	constexpr double Pi = UE_DOUBLE_PI;

	/// SceneKit axes ↔ Unreal axes for directions (a swap of Y and Z: its
	/// own inverse).
	FVector Swap(const FVector& V) { return FVector(V.X, V.Z, V.Y); }

	/// The picture harness's boom (AcMmoShots, -AcMmoShotsRingshadow's
	/// rs-02b): the third-person boom's length times this, its lift raised
	/// by as much. 1 in play.
	TAutoConsoleVariable<float> CVarShotBoom(TEXT("ac.Pilot.ShotBoom"), 1.f,
		TEXT("Third-person boom length multiplier for picture runs (1: normal)"));
}

double FAcPilotCamera::HorizontalFov(const double Aspect)
{
	const double Half = FMath::DegreesToRadians(FovDegrees * 0.5);
	return FMath::RadiansToDegrees(2.0 * std::atan(std::tan(Half) * FMath::Max(Aspect, 0.1)));
}

// The eye, its pivot and the first pitch are the cockpit's: the kind row of
// the kind whose cockpit it drives with (AcKindTable.h).
FVector FAcPilotCamera::Eye(const ac::UnitKind Kind) { return AcKinds::Look(AcKinds::CockpitKind(Kind)).Eye; }

FVector FAcPilotCamera::Pivot(const ac::UnitKind Kind) { return AcKinds::Look(AcKinds::CockpitKind(Kind)).Pivot; }

double FAcPilotCamera::Bob(const ac::Unit& U)
{
	// `Models.Comet.bob(stride:)`: a Comet's run lifts its whole body.
	if (U.kind == ac::UnitKind::comet && U.walking() && !U.jump()) return std::abs(std::sin(U.stride * 4.6)) * 0.035;
	return 0.0;
}

double FAcPilotCamera::StartPitch(const ac::UnitKind Kind) { return AcKinds::Look(AcKinds::CockpitKind(Kind)).StartPitch; }

void FAcPilotCamera::PitchLimits(const ac::UnitKind Kind, double& OutLow, double& OutHigh)
{
	const bool bAir = ac::Rules::stats(Kind).air;
	OutLow = bAir ? -1.4 : -1.2;
	OutHigh = bAir ? 0.4 : 0.9;
}

void FAcPilotCamera::Boom(const ac::UnitKind Kind, float& OutLift, float& OutLength)
{
	OutLift = AcKinds::Look(Kind).BoomLift;
	OutLength = AcKinds::Look(Kind).BoomLength;
	const float Shot = FMath::Max(1.f, CVarShotBoom.GetValueOnGameThread());
	OutLength *= Shot;
	OutLift += 2.f * (Shot - 1.f);
}

void FAcPilotCamera::Reset()
{
	CurrentLean = FAcViewLean();
	Eased.Reset();
	EyeZ.Reset();
}

void FAcPilotCamera::Follow(const FTransform& Root, const ac::Unit& U, const double RealNow, FVector& OutLocation,
	FQuat& OutRotation)
{
	// The turret's turn on the hull: heading grows toward the unit's right
	// (+Z), a turn of −θ about the model's +Y.
	const FQuat Turret(FVector(0, 1, 0), -std::remainder(U.look() - U.heading, 2.0 * Pi));
	const FVector P = Pivot(U.kind);
	FVector E = P + Turret.RotateVector(Eye(U.kind) - P);
	E.Y += CurrentLean.Lift;
	FVector At = Root.TransformPosition(AcSpace::FromSceneKit(E.X, E.Y, E.Z));
	// A soldier's run bobs its whole body: the eye takes it off again.
	At.Z -= AcSpace::ToCm(Bob(U));
	if (!U.stats().bio)
	{
		// A machine rides on its suspension: the eye follows the ground up
		// and down over about a tenth of a second, not every bump.
		if (EyeZ && RealNow - EyeZ->Value < 0.5)
		{
			At.Z = EyeZ->Key + (At.Z - EyeZ->Key) * FMath::Min(1.0, (RealNow - EyeZ->Value) * 9.0);
		}
		EyeZ = TPair<double, double>(At.Z, RealNow);
	}
	OutLocation = At;

	// The model faces +X; a camera looks down −Z. Match the model's
	// (turret's) forward, roll with the machine, then pitch about the
	// camera's own X (all in SceneKit axes, as Swift).
	const FVector F = Swap(Root.TransformVectorNoScale(Swap(Turret.RotateVector(FVector(1, 0, 0)))));
	const double Yaw = std::atan2(-F.X, -F.Z);
	const FQuat Q = FQuat(FVector(0, 1, 0), Yaw) * FQuat(FVector(0, 0, 1), CurrentLean.Roll)
		* FQuat(FVector(1, 0, 0), Pitch + CurrentLean.Shake.X) * FQuat(FVector(0, 1, 0), CurrentLean.Shake.Y);
	const FVector Forward = Swap(Q.RotateVector(FVector(0, 0, -1)));
	const FVector Up = Swap(Q.RotateVector(FVector(0, 1, 0)));
	OutRotation = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
}

void FAcPilotCamera::SetLean(const FAcViewLean& InLean, const bool bBio, const double Time)
{
	CurrentLean = InLean;
	if (bBio) return;
	// A machine's roll into turns and its bounce, eased over about a fifth
	// of a second; jolts (`Shake`) stay sharp.
	if (!Eased || Time - Eased->Value >= 0.5)
	{
		Eased = TPair<FAcViewLean, double>(CurrentLean, Time);
		return;
	}
	const float K = float(FMath::Min(1.0, (Time - Eased->Value) * 5.0));
	FAcViewLean L = CurrentLean;
	L.Roll = Eased->Key.Roll + (CurrentLean.Roll - Eased->Key.Roll) * K;
	L.Lift = Eased->Key.Lift + (CurrentLean.Lift - Eased->Key.Lift) * K;
	CurrentLean = L;
	Eased = TPair<FAcViewLean, double>(L, Time);
}

void FAcPilotCamera::Track(const ac::Unit& U, const double Time)
{
	const FLast Now{U.id, U.heading, U.stride, Time};
	if (!Last || Last->Id != U.id)
	{
		TurnRate = 0;
		SpeedRate = 0;
		Last = Now;
		return;
	}
	const double Dt = Time - Last->Time;
	if (Dt > 1e-4)
	{
		const double K = FMath::Min(1.0, Dt * 6.0);
		TurnRate += (std::remainder(U.heading - Last->Heading, 2.0 * Pi) / Dt - TurnRate) * K;
		SpeedRate += (FMath::Max(0.0, U.stride - Last->Stride) / Dt - SpeedRate) * K;
	}
	Last = Now;
}
