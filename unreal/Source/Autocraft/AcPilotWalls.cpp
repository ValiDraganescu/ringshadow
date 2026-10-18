#include "AcPilotWalls.h"

#include "AcPose.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Rules.h"
#include "Simulation.h"

namespace
{
	/// Where the segment `O` + t·`D` (t in 0 to 1) enters box `B`, or false.
	bool Enters(const FVector& O, const FVector& D, const FBox& B, double& OutT)
	{
		double T0 = 0, T1 = 1;
		for (int32 A = 0; A < 3; ++A)
		{
			if (FMath::Abs(D[A]) < 1e-9)
			{
				if (O[A] < B.Min[A] || O[A] > B.Max[A]) return false;
				continue;
			}
			double Ta = (B.Min[A] - O[A]) / D[A], Tb = (B.Max[A] - O[A]) / D[A];
			if (Ta > Tb) Swap(Ta, Tb);
			T0 = FMath::Max(T0, Ta);
			T1 = FMath::Min(T1, Tb);
			if (T0 > T1) return false;
		}
		OutT = T0;
		return true;
	}

	void Add(const ac::Slab& L, const ac::Vec2 At, const double Base, const double Sink, TArray<AcPilotWalls::FSlab>& Out)
	{
		const double Z1 = L.z1 + Sink;
		if (Z1 <= 0) return;
		const double Z0 = FMath::Max(0.0, L.z0 + Sink);
		const FVector Lo = AcSpace::ToWorld(At + L.min, Base + Z0);
		const FVector Hi = AcSpace::ToWorld(At + L.max, Base + Z1);
		Out.Add({FBox(Lo, Hi), Z1 <= AcPilotWalls::LowWall});
	}
}

void AcPilotWalls::SlabsOf(const ac::Structure& S, const double GroundCells, TArray<FSlab>& Out)
{
	const ac::Footprint& F = ac::Rules::footprint(S.kind);
	const double Sink = AcPose::ConstructionSink(S);
	for (const ac::Slab& W : F.walls) Add(W, S.position, GroundCells, Sink, Out);
	if (F.door && !(S.door && S.door->open >= 0.95)) Add(*F.door, S.position, GroundCells, Sink, Out);
	if (F.roof) Add(*F.roof, S.position, GroundCells, Sink, Out);
}

void AcPilotWalls::Gather(UWorld* World, const ac::Vec2 Near, TArray<FSlab>& Out)
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World);
	if (!Sim || !Sim->IsMmo() || !Sim->IsRunning()) return;
	const AAcTerrain* Terrain = AAcTerrain::Find(World);
	for (const ac::Structure& S : Sim->Simulation().state.structures)
	{
		// A pad is ground to walk on, not a wall.
		if (ac::Rules::walkable(S.kind)) continue;
		if (ac::distance(S.position, Near) - ac::Rules::radius(S.kind) > Range) continue;
		SlabsOf(S, Terrain ? Terrain->FieldHeight(S.position) : 0.0, Out);
	}
}

double AcPilotWalls::Sweep(const FVector& From, const FVector& To, const double RadiusCm, const TConstArrayView<FSlab> Slabs,
	bool* bOutLow)
{
	const FVector D = To - From;
	double Best = 1;
	bool bLow = false;
	for (const FSlab& S : Slabs)
	{
		if (S.Box.IsInsideOrOn(From)) continue;
		// The sphere against the box grown by its radius (the corners
		// square, a little wide of round: never through).
		FBox B = S.Box.ExpandBy(RadiusCm);
		if (B.IsInsideOrOn(From)) B = S.Box;
		double T = 1;
		if (Enters(From, D, B, T) && T < Best)
		{
			Best = T;
			bLow = S.bLow;
		}
	}
	if (bOutLow) *bOutLow = Best < 1 && bLow;
	return Best;
}

FVector AcPilotWalls::ClampEye(UWorld* World, const FVector& Centre, const FVector& Eye)
{
	TArray<FSlab> Slabs;
	Gather(World, AcSpace::ToSim(Eye), Slabs);
	if (Slabs.IsEmpty()) return Eye;
	const FVector From(Centre.X, Centre.Y, Eye.Z);
	const double T = Sweep(From, Eye, AcSpace::ToCm(EyeRadius), Slabs);
	return From + (Eye - From) * T;
}
