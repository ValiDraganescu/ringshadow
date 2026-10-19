// A model at rest (`AAcFlightShip`, AcPilotFlight.h): what is left of F2's
// Dropship flight show.
#include "AcPilotFlight.h"

#include "AcLog.h"
#include "AcModelCatalog.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"

// MARK: - The ship

AAcFlightShip::AAcFlightShip()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	SetCanBeDamaged(false);
}

AAcFlightShip* AAcFlightShip::SpawnFor(UWorld* World, const FName Name)
{
	if (!World) return nullptr;
	for (TActorIterator<AAcFlightShip> It(World); It; ++It)
	{
		if (It->GetFName() == Name) return *It;
	}
	FActorSpawnParameters P;
	P.Name = Name;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAcFlightShip* Ship = World->SpawnActor<AAcFlightShip>(P);
	if (Ship) Ship->Hide();
	return Ship;
}

void AAcFlightShip::Build(const FName Model, const int32 Team)
{
	for (UStaticMeshComponent* C : Meshes)
	{
		if (C) C->DestroyComponent();
	}
	Meshes.Reset();
	Key = Model;
	KeyTeam = Team;
	const FAcModelInfo* Info = FAcModelCatalog::Get().Find(Model);
	if (!Info)
	{
		UE_LOG(LogAutocraft, Warning, TEXT("flight: no model %s"), *Model.ToString());
		return;
	}
	// The model at rest, as the model row draws it: its parts that show (a
	// hidden part, or one under it, stays out; so do the see-through
	// smears), its materials, its team's colour.
	for (int32 P = 0; P < Info->Parts.Num(); ++P)
	{
		bool bHidden = false;
		for (int32 Q = P; Q != INDEX_NONE; Q = Info->Parts[Q].Parent) bHidden |= Info->Parts[Q].bHidden;
		const FAcModelPart& Part = Info->Parts[P];
		// (A flier's ground shadow stays out: the flight's ship is in the air.)
		if (bHidden || Part.Opacity < 0.999f || Part.Name.ToString().Contains(TEXT("shadow"))) continue;
		const FTransform Rest = Info->RestToModel(P);
		for (const FAcModelMesh& X : Part.Meshes)
		{
			UStaticMesh* Mesh = X.LoadMesh();
			if (!Mesh) continue;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Root);
			C->SetStaticMesh(Mesh);
			if (UMaterialInterface* Mat = X.LoadMaterial()) C->SetMaterial(0, Mat);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCustomPrimitiveDataFloat(AcModelData::TeamIndex, float(Team));
			C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, 1.f);
			C->SetRelativeTransform(Rest);
			C->RegisterComponent();
			Meshes.Add(C);
		}
	}
	UE_LOG(LogAutocraft, Log, TEXT("flight: ship %s, team %d, %d meshes"), *Model.ToString(), Team, Meshes.Num());
}

bool AAcFlightShip::Show(const FName Model, const int32 Team, const FTransform& Placement, const double Lift)
{
	if (Key != Model || KeyTeam != Team) Build(Model, Team);
	if (Meshes.Num() == 0)
	{
		Hide();
		return false;
	}
	Root->SetWorldLocationAndRotation(Placement.GetLocation() + FVector(0, 0, Lift), Placement.GetRotation());
	if (!bShown)
	{
		bShown = true;
		Root->SetVisibility(true, true);
	}
	return true;
}

void AAcFlightShip::Prepare(const FName Model, const int32 Team)
{
	if (Key != Model || KeyTeam != Team) Build(Model, Team);
	Hide();
}

void AAcFlightShip::SetScale(const double Scale)
{
	Root->SetWorldScale3D(FVector(Scale));
}

void AAcFlightShip::SetGlow(const double Glow)
{
	for (UStaticMeshComponent* C : Meshes)
	{
		if (C) C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, float(Glow));
	}
}

void AAcFlightShip::Hide()
{
	bShown = false;
	Root->SetVisibility(false, true);
}
