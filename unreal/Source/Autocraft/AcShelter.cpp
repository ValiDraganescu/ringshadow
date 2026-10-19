#include "AcShelter.h"

#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Rules.h"
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
	return I && S.sheltered(S.state.units[size_t(*I)]);
}
