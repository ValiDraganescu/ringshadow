#include "AcBrine.h"

#include "AcCanyon.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const TCHAR* const MercuryPath = TEXT("/Game/Materials/M_Mercury.M_Mercury");
	/// The sheet this far over the canyon's floor (cells).
	constexpr double Over = 0.3;
	/// Its grid: a vertex every this many cells (its edge follows the outline
	/// to this).
	constexpr double Step = 0.5;

	bool Inside(const TArray<ac::Vec2>& Poly, const ac::Vec2 P)
	{
		bool In = false;
		for (int32 I = 0, J = Poly.Num() - 1; I < Poly.Num(); J = I++)
		{
			if ((Poly[I].y > P.y) != (Poly[J].y > P.y)
				&& P.x < (Poly[J].x - Poly[I].x) * (P.y - Poly[I].y) / (Poly[J].y - Poly[I].y) + Poly[I].x)
				In = !In;
		}
		return In;
	}
}

AAcBrine::AAcBrine()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Brine"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;
}

AAcBrine* AAcBrine::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcBrine> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcBrine* AAcBrine::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcBrine* B = Find(World)) return B;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcBrine");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcBrine>(AAcBrine::StaticClass(), FTransform::Identity, Params);
}

void AAcBrine::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	const TOptional<FAcRegionAround> Here = Mmo ? Mmo->HereGround() : TOptional<FAcRegionAround>();
	// The ground's frame: the mirror's map (the region the pilot is in).
	const ac::net::Mirror* Mirror = Mmo ? Mmo->Mirror() : nullptr;
	const ac::MapDefinition* Map = Mirror && Mirror->simulation().map ? &*Mirror->simulation().map : nullptr;
	if (!Mmo || !Terrain || !Here || !Map || Mmo->MirrorIndex() != Here->Index)
	{
		if (Drawn > 0) Mesh->ClearAllMeshSections();
		Drawn = 0;
		BuiltFor.Reset();
		return;
	}
	// Again on a new region, and a few times while its ground fills in
	// (the floor's height is read off it).
	const double Now = FPlatformTime::Seconds();
	const FString Key = Here->Id;
	if (Key == BuiltFor && Now < RebuildAt) return;
	if (Key != BuiltFor) RebuildAt = Now + 2.0;
	else RebuildAt = Now + 1e9;
	BuiltFor = Key;
	Mesh->ClearAllMeshSections();
	Drawn = 0;
	if (!Material)
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MercuryPath))
		{
			Material = UMaterialInstanceDynamic::Create(Base, this);
			// Salt water: grey-green, less of a mirror than the MH; the pool's
			// ripples are a small well's (its local space), so off here.
			Material->SetVectorParameterValue(TEXT("BaseColorTint"), FLinearColor(0.10f, 0.17f, 0.15f));
			Material->SetScalarParameterValue(TEXT("Metallic"), 0.75f);
			Material->SetScalarParameterValue(TEXT("Roughness"), 0.16f);
			Material->SetScalarParameterValue(TEXT("EnvGain"), 0.22f);
			Material->SetScalarParameterValue(TEXT("NightGlow"), 0.f);
			Material->SetScalarParameterValue(TEXT("Pool"), 0.f);
		}
	const ac::Vec2 Corner(Map->bounds.minX, Map->bounds.minZ);
	for (const FAcZonePlace& P : AcCanyon::Places(*Mmo))
	{
		if (P.Region != Here->Id || P.Flood.Num() < 3) continue;
		// The floor's height: the prize's middle, on the canyon's floor.
		const double Level = Terrain->FieldHeight(P.Middle + Corner) + Over;
		double X0 = 1e9, Y0 = 1e9, X1 = -1e9, Y1 = -1e9;
		for (const ac::Vec2 Q : P.Flood)
		{
			X0 = FMath::Min(X0, Q.x), Y0 = FMath::Min(Y0, Q.y), X1 = FMath::Max(X1, Q.x), Y1 = FMath::Max(Y1, Q.y);
		}
		// A grid over its outline's box, a cell past it; the cells whose
		// middle is inside (the rims over the water hide the rest).
		const int32 Nx = FMath::CeilToInt((X1 - X0 + 2) / Step) + 1, Ny = FMath::CeilToInt((Y1 - Y0 + 2) / Step) + 1;
		TArray<FVector> Pos;
		TArray<int32> Index;
		TArray<FVector> Normals;
		TArray<FVector2D> Uv;
		for (int32 J = 0; J < Ny; ++J)
			for (int32 I = 0; I < Nx; ++I)
			{
				const ac::Vec2 Q(X0 - 1 + I * Step, Y0 - 1 + J * Step);
				Pos.Add(AcSpace::ToWorld(Q + Corner, Level) - GetActorLocation());
				Normals.Add(FVector::UpVector);
				Uv.Add(FVector2D(Q.x / 8.0, Q.y / 8.0));
			}
		for (int32 J = 0; J + 1 < Ny; ++J)
			for (int32 I = 0; I + 1 < Nx; ++I)
			{
				const ac::Vec2 Mid(X0 - 1 + (I + 0.5) * Step, Y0 - 1 + (J + 0.5) * Step);
				if (!Inside(P.Flood, Mid)) continue;
				const int32 A = J * Nx + I, B = A + 1, C = A + Nx, D = C + 1;
				Index.Append({A, C, B, B, C, D});
			}
		if (Index.IsEmpty()) continue;
		Mesh->CreateMeshSection(Drawn, Pos, Index, Normals, Uv, {}, {}, false);
		if (Material) Mesh->SetMaterial(Drawn, Material);
		Drawn += 1;
	}
	UE_LOG(LogAutocraft, Log, TEXT("brine: %s: %d sheets (%d zones known)"), *Key, Drawn, AcCanyon::Places(*Mmo).Num());
}
