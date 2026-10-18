#include "AcPilotChase.h"

#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcPilotCamera.h"
#include "AcPilotWalls.h"
#include "AcRays.h"
#include "AcRaysWorld.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcWorldRenderer.h"
#include "AcPose.h"
#include "AcSimSubsystem.h"
#include "Rules.h"
#include "Simulation.h"

#include "Engine/World.h"

#include <cmath>
#include <vector>

float FAcPilotChase::Reach(const FAcModelInfo& Model, const TConstArrayView<bool> Visible, const TConstArrayView<FTransform> Local)
{
	// Game thread only (the pilot's camera).
	static TMap<const FAcModelInfo*, float> Measured;
	if (const float* Known = Measured.Find(&Model)) return *Known;

	const int32 N = Model.Parts.Num();
	int32 Root = 0;
	for (int32 P = 0; P < N; ++P)
	{
		if (Model.Parts[P].Parent == INDEX_NONE) { Root = P; break; }
	}
	const double RootScale = N > 0 ? Model.Parts[Root].Rest.GetScale3D().X : 1.0;
	float Far = 0.f;
	int32 FarPart = INDEX_NONE;
	if (const FAcRayShape* Shape = FAcRayShapes::Get().Find(&Model); Shape && !Shape->Empty())
	{
		// Each part in the root's space (the root's own transform left out,
		// as Swift walks from the root node), hidden parts and theirs skipped.
		std::vector<FAcMat4> ToRoot(static_cast<size_t>(N));
		std::vector<uint8_t> Shown(static_cast<size_t>(N), 1);
		for (int32 P = 0; P < N; ++P)
		{
			const int32 Up = Model.Parts[P].Parent;
			const FAcMat4 Rest = AcRaySpace::Matrix(Local.IsValidIndex(P) ? Local[P] : Model.Parts[P].Rest);
			if (P == Root || Up == INDEX_NONE) ToRoot[size_t(P)] = FAcMat4();
			else if (Up == Root) ToRoot[size_t(P)] = Rest;
			else ToRoot[size_t(P)] = ToRoot[size_t(Up)] * Rest;
			const bool bOwn = Visible.IsValidIndex(P) ? Visible[P] : !Model.Parts[P].bHidden;
			Shown[size_t(P)] = (P == Root || bOwn) && (Up == INDEX_NONE || Shown[size_t(Up)]);
		}
		for (const FAcRayShape::FPiece& Piece : Shape->Pieces)
		{
			if (!Shown[size_t(Piece.Part)]) continue;
			const FAcMat4 M = ToRoot[size_t(Piece.Part)] * Piece.At;
			for (int C = 0; C < 8; ++C)
			{
				const FAcF3 Corner{(C & 1) == 0 ? Piece.Box.Lo.x : Piece.Box.Hi.x, (C & 2) == 0 ? Piece.Box.Lo.y : Piece.Box.Hi.y,
					(C & 4) == 0 ? Piece.Box.Lo.z : Piece.Box.Hi.z};
				const FAcF3 W = M.Point(Corner);
				const float Dxz = std::sqrt(W.x * W.x + W.z * W.z);
				if (Dxz > Far) FarPart = Piece.Part;
				Far = FMath::Max(Far, Dxz);
			}
		}
	}
	else if (Model.Bounds.IsValid)
	{
		// No ray pieces: the rest bounds' corners across the ground.
		for (int C = 0; C < 4; ++C)
		{
			const double X = (C & 1) ? Model.Bounds.Max.X : Model.Bounds.Min.X;
			const double Y = (C & 2) ? Model.Bounds.Max.Y : Model.Bounds.Min.Y;
			Far = FMath::Max(Far, float(AcSpace::ToCells(FVector2D(X, Y).Size())));
		}
	}
	const float Reach = Far > 0.f ? Far * float(RootScale) : 1.f;
	Measured.Add(&Model, Reach);
	UE_LOG(LogAutocraft, Log, TEXT("pilot: %s reaches %.2f cells (third-person boom; farthest: %s)"), *Model.Name.ToString(), Reach,
		Model.Parts.IsValidIndex(FarPart) ? *Model.Parts[FarPart].Name.ToString() : TEXT("-"));
	return Reach;
}

float FAcPilotChase::AtEase(UWorld* World, const ac::Unit& U, const FAcModelInfo& Model)
{
	// Swift measures the live node the first time it chases a kind, which is
	// the unit as it stood before the take-over: at ease, not aiming (a
	// Ranger's rifle held up across the ground would reach 1 cell, its body
	// 0.6), the parts its owner's upgrades hide hidden. Pose it so once.
	static TSet<const FAcModelInfo*> Posed;
	if (Posed.Contains(&Model)) return Reach(Model);
	Posed.Add(&Model);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World);
	const AAcTerrain* Terrain = AAcTerrain::Find(World);
	if (!Sim || !Sim->IsRunning() || !Terrain) return Reach(Model);
	ac::Unit V = U;
	V.task = ac::Unit::Task::idle;
	V.target.reset();
	V.aim.reset();
	V.moving = false;
	FAcUnitMemory Memory;
	Memory.FirstSeen = Sim->State().time;
	FAcPoseContext C{Sim->State(), &V, nullptr, Model, Terrain->GetField(), Memory, Sim->State().time, 0.0, false, Sim->LocalPlayer()};
	FAcPose Pose;
	AcPose::RestUnit(C, Pose);
	if (const FAcPoseFn Fn = AcPose::Find(V.kind)) Fn(C, Pose);
	return Reach(Model, Pose.Visible, Pose.Local);
}

void FAcPilotChase::FZoom::Notch(const float Notches)
{
	// A trackpad's flick sends a few at once: at most three notches an event.
	Target = FMath::Clamp(Target * std::pow(Step, -FMath::Clamp(Notches, -3.f, 3.f)), Min, Max);
}

void FAcPilotChase::FZoom::Set(const float To, const bool bSnap)
{
	Target = FMath::Clamp(To, Min, Max);
	if (bSnap) ShownAt = -1;
}

double FAcPilotChase::FZoom::Tick(const double RealNow)
{
	if (ShownAt < 0 || RealNow < ShownAt)
	{
		Shown = Target;
	}
	else
	{
		// 95% of the way in `EaseSeconds`.
		const double K = 1.0 - std::exp(-3.0 * (RealNow - ShownAt) / EaseSeconds);
		Shown += (Target - Shown) * K;
		if (std::abs(Shown - Target) < 1e-4) Shown = Target;
	}
	ShownAt = RealNow;
	return Shown;
}

double FAcPilotChase::FZoom::Tilt(const double Z)
{
	return MaxTilt * FMath::SmoothStep(1.0, double(Max), Z);
}

void FAcPilotChase::Reset()
{
	PivotZ.Reset();
	ChaseEye.Reset();
	Zoom.ShownAt = -1;
}

void FAcPilotChase::Follow(UWorld* World, const FTransform& Root, const ac::Unit& U, const FAcModelInfo* Model, const double Pitch,
	const double Yaw, const double RealNow, FVector& OutLocation, FQuat& OutRotation)
{
	// The unit's eye on its model (as the first-person eye, the turret's
	// turn counted, no lean and no bob): where its shots start.
	const FQuat Turret(FVector(0, 1, 0), -std::remainder(U.look() - U.heading, 2.0 * UE_DOUBLE_PI));
	const FVector P = FAcPilotCamera::Pivot(U.kind);
	const FVector E = P + Turret.RotateVector(FAcPilotCamera::Eye(U.kind) - P);
	const FVector Eye = Root.TransformPosition(AcSpace::FromSceneKit(E.X, E.Y, E.Z));

	const double R = Model ? AtEase(World, U, *Model) : 1.0;
	const FVector Ahead(std::cos(Yaw), std::sin(Yaw), 0.0);
	// The unit's right across the ground (heading grows toward it).
	const FVector Right(-std::sin(Yaw), std::cos(Yaw), 0.0);
	// The wheel's zoom: the boom's length, and far out a tilt over the site.
	const double Z = Zoom.Tick(RealNow);
	const double Tilted = FMath::Min(Pitch, FMath::Max(Pitch - FZoom::Tilt(Z), -1.4));
	const FVector Look = Ahead * std::cos(Tilted) + FVector(0, 0, std::sin(Tilted));
	float Lift = 0.f, Length = 1.f;
	FAcPilotCamera::Boom(U.kind, Lift, Length);
	FVector Pivot = Eye + FVector(0, 0, AcSpace::ToCm(0.1 + 0.15 * R + Lift)) + Right * AcSpace::ToCm(0.4 + 0.45 * R);
	// The pivot rides the ground's bumps eased, as a machine's eye does.
	if (PivotZ && RealNow - PivotZ->Value < 0.5)
	{
		Pivot.Z = PivotZ->Key + (Pivot.Z - PivotZ->Key) * FMath::Min(1.0, (RealNow - PivotZ->Value) * 9.0);
	}
	PivotZ = TPair<double, double>(Pivot.Z, RealNow);
	FVector At = Pivot - Look * AcSpace::ToCm((2.6 + 3.3 * R) * Length * Z);
	if (UAcRaysSubsystem* Rays = UAcRaysSubsystem::Get(World))
	{
		// The boom comes in where a building or a rock is behind the unit...
		if (const TOptional<float> T = Rays->Blocked(Pivot, At)) At = Pivot + (At - Pivot) * FMath::Max(0.1, double(*T) - 0.08);
		// ...nor between the camera and the unit's body.
		const FVector Body = (Eye + Root.GetTranslation()) / 2;
		if (const TOptional<float> T = Rays->Blocked(At, Body); T && *T < 0.9f)
		{
			At = At + (Pivot - At) * FMath::Min(0.9, double(*T) + 0.08);
		}
	}
	// The buildings are not in the rays (docs/citadel-interior.md §6,
	// docs/mmorpg-shots.md #12): the boom sweeps a sphere against their
	// slabs (AcPilotWalls.h) and stops at the first, the pivot first kept
	// on the unit's side of a wall it hugs. A low wall close ahead of the
	// unit (a scaffold being welded, a small building) lifts the camera to
	// look over it, and tilts it down by as much, so the unit keeps its
	// place in the glass instead of sinking behind the dashboard.
	FVector Aim = Look;
	TArray<AcPilotWalls::FSlab> Slabs;
	AcPilotWalls::Gather(World, U.position, Slabs);
	if (!Slabs.IsEmpty())
	{
		const double Radius = AcSpace::ToCm(AcPilotWalls::ChaseRadius);
		Pivot = Eye + (Pivot - Eye) * AcPilotWalls::Sweep(Eye, Pivot, Radius, Slabs);
		bool bLow = false;
		const double T = AcPilotWalls::Sweep(Pivot, At, Radius, Slabs, &bLow);
		At = Pivot + (At - Pivot) * T;
		double Raise = bLow ? 0.8 * (1 - T) : 0.0;
		const FVector Unit = AcSpace::ToWorld(U.position);
		for (const AcPilotWalls::FSlab& W : Slabs)
		{
			if (!W.bLow) continue;
			const FVector2D Near(FMath::Clamp(Unit.X, W.Box.Min.X, W.Box.Max.X), FMath::Clamp(Unit.Y, W.Box.Min.Y, W.Box.Max.Y));
			const FVector2D FromUnit = FVector2D(W.Box.GetCenter()) - FVector2D(Unit);
			const double Gap = AcSpace::ToCells((Near - FVector2D(Unit)).Size());
			if (Gap < 1.6 && (Gap <= 0 || FVector2D::DotProduct(FromUnit, FVector2D(Ahead)) > 0.3 * FromUnit.Size()))
			{
				Raise = FMath::Max(Raise, 1.3 * FMath::Clamp((1.6 - Gap) / 1.6, 0.0, 1.0));
			}
		}
		if (Raise > 0)
		{
			const FVector Boom = Pivot - At;
			// Lifted, but never into a slab over it (a roof).
			const FVector Up = At + FVector(0, 0, AcSpace::ToCm(Raise));
			At = At + (Up - At) * AcPilotWalls::Sweep(At, Up, Radius, Slabs);
			const double Lifted = At.Z - (Pivot - Boom).Z;
			if (Boom.Size() > AcSpace::ToCm(0.5)) Aim = (Boom - FVector(0, 0, Lifted)).GetSafeNormal();
		}
	}
	if (const AAcTerrain* Terrain = AAcTerrain::Find(World))
	{
		At.Z = FMath::Max(At.Z, AcSpace::ToCm(Terrain->FieldHeight(AcSpace::ToSim(At)) + 0.3));
	}
	ChaseEye = Eye;
	OutLocation = At;
	OutRotation = FRotationMatrix::MakeFromXZ(Aim, FVector::UpVector).ToQuat();
}
