// The blueprint sites on the ground: see AcSites.h.
#include "AcSites.h"

#include "AcDaylight.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcModelCatalog.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "NetMirror.h"
#include "NetProtocol.h"
#include "Progression.h"

#include <cmath>

namespace
{
	const TCHAR* const CylinderMesh = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* const AdditivePath = TEXT("/Game/Materials/M_AcPointerAdd.M_AcPointerAdd");
	/// The blueprint's gold (the HUD's level gold, a touch warmer for light).
	const FLinearColor Gold(1.0f, 0.62f, 0.16f);
	/// The column: this tall (cm), at least this wide, and this wide per cm
	/// of distance from the camera (so it stays a few pixels across).
	constexpr double BeamTall = 5200;
	constexpr double BeamWidth = 22;
	constexpr double BeamPerCm = 0.0022;
	/// The lamp blinks on for `BlinkOn` s of every `BlinkEvery` s.
	constexpr double BlinkEvery = 1.4, BlinkOn = 0.45;

	using FAcStagedOrNot = TOptional<AAcSites::FStaged>;

	const TCHAR* ModelName(const ac::SiteKind K)
	{
		switch (K)
		{
		case ac::SiteKind::crate: return TEXT("site_crate_blue");
		case ac::SiteKind::wreck: return TEXT("site_wreck_blue");
		case ac::SiteKind::cache: return TEXT("site_cache_blue");
		case ac::SiteKind::vault: return TEXT("site_vault_blue");
		}
		return TEXT("site_crate_blue");
	}
}

AAcSites::AAcSites()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Sites"));
	RootComponent = Root;
}

AAcSites* AAcSites::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcSites> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcSites* AAcSites::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcSites* S = Find(World)) return S;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcSites");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcSites>(AAcSites::StaticClass(), FTransform::Identity, Params);
}

void AAcSites::EndPlay(const EEndPlayReason::Type Reason)
{
	Clear();
	Super::EndPlay(Reason);
}

TOptional<FVector> AAcSites::Where(const FString& Id) const
{
	for (const FSite& S : Sites)
		if (S.Id == Id) return S.At + FVector(0, 0, S.Tall * 0.5);
	return {};
}

void AAcSites::Stage(const TArray<FStaged>& In)
{
	StagedList = In;
	// Placed again with the region's own on the next tick.
	PlacedRegion.Reset();
}

int32 AAcSites::Columns() const
{
	int32 N = 0;
	for (const FSite& S : Sites) N += S.Beam && S.Beam->IsVisible() ? 1 : 0;
	return N;
}

void AAcSites::Clear()
{
	for (FSite& S : Sites)
	{
		for (UActorComponent* C : {(UActorComponent*)S.Beam.Get(), (UActorComponent*)S.Halo.Get(), (UActorComponent*)S.Light.Get()})
			if (C) C->DestroyComponent();
	}
	Sites.Reset();
	Owned.Reset();
	if (bBatches) Batches.Clear();
	PlacedRegion.Reset();
	PlacedField = nullptr;
}

UStaticMeshComponent* AAcSites::Column(const TCHAR* Name, UMaterialInstanceDynamic*& OutMaterial)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, MakeUniqueObjectName(this, UStaticMeshComponent::StaticClass(), Name));
	C->SetMobility(EComponentMobility::Movable);
	C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CylinderMesh));
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(false);
	OutMaterial = nullptr;
	if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, AdditivePath))
	{
		OutMaterial = UMaterialInstanceDynamic::Create(Base, this);
		OutMaterial->SetVectorParameterValue(TEXT("Color"), Gold);
		OutMaterial->SetScalarParameterValue(TEXT("Intensity"), 1.0f);
		OutMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.5f);
		OutMaterial->SetScalarParameterValue(TEXT("Shape"), 0.0f);
		C->SetMaterial(0, OutMaterial);
		Owned.Add(OutMaterial);
	}
	C->SetupAttachment(Root);
	C->RegisterComponent();
	C->SetVisibility(false);
	Owned.Add(C);
	return C;
}

void AAcSites::Place(const FAcMmoClient& Mmo, const FString& Region)
{
	Clear();
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	const ac::net::Mirror* M = Mmo.Mirror();
	if (!Terrain || !Terrain->IsBuilt() || !M || !M->simulation().map) return;
	PlacedRegion = Region;
	PlacedField = &Terrain->GetField();
	if (!bBatches)
	{
		FAcSceneryOptions O;
		O.bHierarchical = false;
		O.Mobility = EComponentMobility::Movable;
		Batches.Init(this, Root, O, TEXT("Site"));
		bBatches = true;
	}
	const ac::GroundRect& Bounds = M->simulation().map->bounds;
	TArray<FAcSceneryModel*> Models;
	TArray<AcProgressHud::FSitePlace> Places = AcProgressHud::SitesIn(Mmo, Region);
	for (int32 I = 0; I < StagedList.Num(); ++I)
	{
		AcProgressHud::FSitePlace& P = Places.AddDefaulted_GetRef();
		P.Id = FString::Printf(TEXT("staged/%d"), I);
		P.Kind = StagedList[I].Kind;
		P.Cell = ac::Vec2(StagedList[I].Ground.x - Bounds.minX, StagedList[I].Ground.y - Bounds.minZ);
	}
	const int32 Real = Places.Num() - StagedList.Num();
	Sites.Reserve(Places.Num());
	for (int32 PlaceIndex = 0; PlaceIndex < Places.Num(); ++PlaceIndex)
	{
		const AcProgressHud::FSitePlace& P = Places[PlaceIndex];
		const FAcStagedOrNot Staged = PlaceIndex >= Real ? FAcStagedOrNot(StagedList[PlaceIndex - Real]) : FAcStagedOrNot();
		const FAcModelInfo* Info = FAcModelCatalog::Get().Find(ModelName(P.Kind));
		if (!Info)
		{
			UE_LOG(LogAutocraft, Warning, TEXT("sites: no model %s in the catalog (run Tools/Editor/models/sites.py)"), ModelName(P.Kind));
			continue;
		}
		const ac::Vec2 G(P.Cell.x + Bounds.minX, P.Cell.y + Bounds.minZ);
		// Seated on the lowest ground under it (nothing hangs over a slope:
		// a wreck on a crest sinks into it).
		double Ground = Terrain->FieldHeight(G);
		for (int32 K = 0; K < 8; ++K)
		{
			const double A = K * UE_DOUBLE_PI / 4;
			Ground = FMath::Min(Ground, Terrain->FieldHeight(ac::Vec2(G.x + 1.3 * std::cos(A), G.y + 1.3 * std::sin(A))));
		}
		// Face the lowest ground three cells round: a vault's door off its rock.
		double Lowest = TNumericLimits<double>::Max();
		double Toward = 0;
		for (int32 K = 0; K < 16; ++K)
		{
			const double A = K * UE_DOUBLE_PI / 8;
			const double H = Terrain->FieldHeight(ac::Vec2(G.x + 3 * std::cos(A), G.y + 3 * std::sin(A)));
			if (H < Lowest - 0.05)
			{
				Lowest = H;
				Toward = A;
			}
		}
		if (Staged) Toward = std::atan2(Staged->FaceTo.y - G.y, Staged->FaceTo.x - G.x);
		FSite& S = Sites.AddDefaulted_GetRef();
		S.Id = P.Id;
		S.Staged = Staged;
		S.Seed = GetTypeHash(P.Id);
		// A crashed lander lies half in the ground.
		S.At = AcSpace::ToWorld(G, Ground - (P.Kind == ac::SiteKind::wreck ? 0.22 : 0.0));
		S.Tall = FMath::Clamp(Info->Bounds.IsValid ? Info->Bounds.Max.Z : 100.0, 80.0, 300.0);
		// The model's front is +Y: turn it toward `Toward` (a world angle).
		const FTransform Placement(FRotator(0.0, FMath::RadiansToDegrees(Toward) - 90.0, 0.0), S.At);
		S.Model = Batches.AddModel(*Info, Placement);
		for (int32 I = 0; I < Info->Parts.Num(); ++I)
		{
			const FString Name = Info->Parts[I].Name.ToString();
			if (Name.EndsWith(TEXT("_shut"))) S.Shut.Add(I);
			else if (Name.EndsWith(TEXT("_open"))) S.Open.Add(I);
			else if (Name == TEXT("lamp")) S.Lamps.Add(I);
			else if (Name == TEXT("seam")) S.Seams.Add(I);
		}
		UMaterialInstanceDynamic* Mat = nullptr;
		S.Beam = Column(TEXT("SiteBeam"), Mat);
		S.BeamMaterial = Mat;
		S.Halo = Column(TEXT("SiteHalo"), Mat);
		S.HaloMaterial = Mat;
		UPointLightComponent* L = NewObject<UPointLightComponent>(this, MakeUniqueObjectName(this, UPointLightComponent::StaticClass(), TEXT("SiteLight")));
		L->SetMobility(EComponentMobility::Movable);
		L->SetLightColor(Gold);
		L->SetAttenuationRadius(700);
		L->SetCastShadows(false);
		L->SetupAttachment(Root);
		L->RegisterComponent();
		L->SetWorldLocation(S.At + FVector(0, 0, S.Tall + 60));
		L->SetVisibility(false);
		S.Light = L;
		Owned.Add(L);
	}
	for (FSite& S : Sites) Models.Add(&S.Model);
	Batches.Flush(Models);
	UE_LOG(LogAutocraft, Log, TEXT("sites: %d placed in %s"), Sites.Num(), *Region);
}

void AAcSites::Show(FSite& S, const bool bOpened, const bool bKnown, const double Time, const FVector& Camera, const double Dark)
{
	if (S.bOpened != int8(bOpened))
	{
		S.bOpened = int8(bOpened);
		for (const int32 I : S.Shut) Batches.SetVisible(S.Model, I, !bOpened);
		for (const int32 I : S.Open) Batches.SetVisible(S.Model, I, bOpened);
		for (const int32 I : S.Seams) Batches.SetVisible(S.Model, I, !bOpened);
	}
	// Blinking out of step with the other sites.
	const double Phase = FMath::Fmod(Time + (S.Seed % 1000) * 0.0137, BlinkEvery);
	const bool bLamp = !bOpened && Phase < BlinkOn;
	if (S.bLampOn != int8(bLamp))
	{
		S.bLampOn = int8(bLamp);
		for (const int32 I : S.Lamps) Batches.SetVisible(S.Model, I, bLamp);
	}
	if (S.Light)
	{
		S.Light->SetVisibility(!bOpened && Dark > 0.05);
		S.Light->SetIntensity(float((bLamp ? 900.0 : 380.0) * Dark));
	}
	const bool bColumn = bKnown && !bOpened;
	for (UStaticMeshComponent* C : {S.Beam.Get(), S.Halo.Get()})
		if (C) C->SetVisibility(bColumn);
	if (!bColumn || !S.Beam) return;
	const double Far = FVector::Dist(Camera, S.At);
	const double Wide = FMath::Max(BeamWidth, Far * BeamPerCm);
	// Engine cylinder: 100 cm across and tall, its pivot in the middle.
	const FVector Foot = S.At + FVector(0, 0, S.Tall * 0.6);
	S.Beam->SetWorldLocationAndRotation(Foot + FVector(0, 0, BeamTall / 2), FRotator::ZeroRotator);
	S.Beam->SetWorldScale3D(FVector(Wide / 100, Wide / 100, BeamTall / 100));
	S.Halo->SetWorldLocationAndRotation(Foot + FVector(0, 0, BeamTall * 0.35), FRotator::ZeroRotator);
	S.Halo->SetWorldScale3D(FVector(Wide * 3.2 / 100, Wide * 3.2 / 100, BeamTall * 0.7 / 100));
	// Breathing, brighter in the dark; near the site it thins out of the way.
	const double Breath = 0.75 + 0.25 * std::sin((Time + (S.Seed % 97) * 0.1) * UE_DOUBLE_PI / 1.6);
	const double Near = FMath::GetMappedRangeValueClamped(FVector2D(400.0, 1600.0), FVector2D(0.25, 1.0), Far);
	if (S.BeamMaterial) S.BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), float((0.5 + 0.35 * Dark) * Breath * Near));
	if (S.HaloMaterial) S.HaloMaterial->SetScalarParameterValue(TEXT("Opacity"), float((0.10 + 0.10 * Dark) * Breath * Near));
}

void AAcSites::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	const ac::net::WorldMessage::Region* Here = Mmo && !Mmo->Travelling() ? Mmo->Here() : nullptr;
	if (!Here)
	{
		if (!Sites.IsEmpty() || !PlacedRegion.IsEmpty()) Clear();
		return;
	}
	const FString Region = UTF8_TO_TCHAR(Here->id.c_str());
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	const void* Field = Terrain && Terrain->IsBuilt() ? &Terrain->GetField() : nullptr;
	if (Region != PlacedRegion || Field != PlacedField) Place(*Mmo, Region);
	if (Sites.IsEmpty()) return;

	const ac::net::ProgressMessage* P = Mmo->Progress();
	const double Time = GetWorld()->GetTimeSeconds();
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Camera = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const AAcDaylight* Day = AAcDaylight::Find(GetWorld());
	const double Dark = Day ? FMath::Clamp(double(Day->GetDark()), 0.0, 1.0) : 0.0;
	for (FSite& S : Sites)
	{
		const std::string Id = TCHAR_TO_UTF8(*S.Id);
		const bool bOpened = S.Staged ? S.Staged->bOpened : P && P->sitesFound.count(Id) > 0;
		const bool bKnown = S.Staged ? S.Staged->bKnown : P && P->sitesKnown.count(Id) > 0;
		Show(S, bOpened, bKnown, Time, Camera, Dark);
	}
	Batches.Commit();
}
