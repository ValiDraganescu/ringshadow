#include "AcShelter.h"

#include "AcPilotPawn.h"
#include "AcShuttleShip.h"
#include "AcShuttleView.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	TAutoConsoleVariable<int32> CVarShelter(
		TEXT("ac.Weather.Shelter"), 1,
		TEXT("No rain in a Citadel's hall (1), or the rain everywhere as before (0; also -AcNoShelter)."));
}

bool AcShelter::On()
{
	static const bool bFlagOff = FParse::Param(FCommandLine::Get(), TEXT("AcNoShelter"));
	return !bFlagOff && CVarShelter.GetValueOnGameThread() != 0;
}

TOptional<AcShelter::FBox> AcShelter::BoxOf(const ac::Structure& S, const double GroundCells)
{
	if (S.kind != ac::StructureKind::citadel || !S.complete() || !(S.hp > 0)) return {};
	const ac::Footprint& F = ac::Rules::footprint(S.kind);
	if (!F.interior) return {};
	const ac::Slab& In = *F.interior;
	const FVector Lo = AcSpace::ToWorld(S.position + In.min, GroundCells + In.z0);
	const FVector Hi = AcSpace::ToWorld(S.position + In.max, GroundCells + In.z1);
	FBox B;
	B.Centre = FVector2D((Lo.X + Hi.X) * 0.5, (Lo.Y + Hi.Y) * 0.5);
	B.Half = FVector2D((Hi.X - Lo.X) * 0.5 + FaceCm, (Hi.Y - Lo.Y) * 0.5 + FaceCm);
	B.Low = Lo.Z - SinkCm;
	B.High = Hi.Z + FaceCm;
	return B;
}

TArray<AcShelter::FBox> AcShelter::Boxes(const std::vector<ac::Structure>& Structures, TFunctionRef<double(ac::Vec2)> GroundCells,
	const FVector& Camera)
{
	TArray<FBox> Out;
	if (!ac::Rules::citadelHall()) return Out;
	for (const ac::Structure& S : Structures)
	{
		if (S.kind != ac::StructureKind::citadel) continue;
		if (TOptional<FBox> B = BoxOf(S, GroundCells(S.position))) Out.Add(*B);
	}
	Out.Sort([&Camera](const FBox& A, const FBox& B)
	{
		return FVector2D::DistSquared(A.Centre, FVector2D(Camera)) < FVector2D::DistSquared(B.Centre, FVector2D(Camera));
	});
	if (Out.Num() > Slots) Out.SetNum(Slots);
	return Out;
}

float AcShelter::EaseLens(const float Rate, const float Target, const float Full, const float Dt)
{
	if (!(Full > 0.f)) return Target;
	const float Step = Full * Dt / (Target < Rate ? LensDrySeconds : LensWetSeconds);
	return Rate + FMath::Clamp(Target - Rate, -Step, Step);
}

bool AcShelter::DrivenSheltered(const UObject* WorldContext)
{
	const AAcPilotPawn* Pilot = AAcPilotPawn::Find(WorldContext);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(WorldContext);
	if (!On() || !Pilot || !Pilot->Driving() || !Sim || !Sim->IsRunning()) return false;
	const ac::Simulation& S = Sim->Simulation();
	const std::optional<int64_t> I = S.unitIndex(Pilot->Driven());
	if (!I) return false;
	const ac::Unit& U = S.state.units[size_t(*I)];
	if (S.sheltered(U)) return true;
	// In a walk-in Dropship's bay (the ramp is open sky).
	if (const ac::Shuttle* Ship = S.aboard(U))
	{
		return Ship->flying() || ac::ShuttleBay::inBay(ac::ShuttleBay::local(*Ship, U.position));
	}
	return false;
}

AcShelter::FShipBox AcShelter::BayOf(const FTransform& Placement)
{
	// The bay's corners in the ship's frame (cm): from the opening to under
	// the swept windshield (the cockpit's air over the nose block, up to
	// 30 cm short of the glass's foot: a thin wedge outside the glass's top
	// is dry too), between the walls' inner faces, from the deck slab to the
	// ceiling panels (ShuttleBay, Tools/Editor/models/ferry.py).
	using B = ac::ShuttleBay;
	const FVector Lo(AcSpace::ToCm(B::rear), -AcSpace::ToCm(B::halfWide) - 6.0, AcSpace::ToCm(B::deck - 0.25));
	const FVector Hi(AcShuttleShip::GlassBottomX - 30.0, AcSpace::ToCm(B::halfWide) + 6.0, AcSpace::ToCm(B::ceiling) + 8.0);
	FShipBox Box;
	Box.Centre = Placement.TransformPosition((Lo + Hi) * 0.5);
	const FQuat Q = Placement.GetRotation();
	Box.X = Q.GetAxisX();
	Box.Y = Q.GetAxisY();
	Box.Z = Q.GetAxisZ();
	Box.Half = (Hi - Lo) * 0.5 + FVector(FaceCm);
	return Box;
}

double AcShelter::FogClearCm(const FShipBox& Bay, const FVector& Camera)
{
	if (Bay.Half.IsNearlyZero() || !Bay.Contains(Camera)) return 0.0;
	double Far = 0.0;
	for (int32 K = 0; K < 8; ++K)
	{
		const FVector Corner = Bay.Centre + Bay.X * ((K & 1) ? Bay.Half.X : -Bay.Half.X) + Bay.Y * ((K & 2) ? Bay.Half.Y : -Bay.Half.Y)
			+ Bay.Z * ((K & 4) ? Bay.Half.Z : -Bay.Half.Z);
		Far = FMath::Max(Far, FVector::Dist(Corner, Camera));
	}
	return Far;
}

double AcShelter::FogClearCm(const FVector& Camera)
{
	const TOptional<FShipBox> Bay = NearestBay(Camera);
	return Bay ? FogClearCm(*Bay, Camera) : 0.0;
}

TOptional<AcShelter::FShipBox> AcShelter::NearestBay(const FVector& Camera)
{
	if (!On()) return {};
	const AcShuttleView::FShip* Best = nullptr;
	double BestD = ShipRange;
	for (const AcShuttleView::FShip& F : AcShuttleView::Ships())
	{
		const double D = FVector::Dist(F.Placement.GetLocation(), Camera);
		if (D < BestD)
		{
			BestD = D;
			Best = &F;
		}
	}
	if (!Best) return {};
	return BayOf(Best->Placement);
}
