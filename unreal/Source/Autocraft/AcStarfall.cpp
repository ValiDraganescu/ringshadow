// Starfalls on screen: see AcStarfall.h.
#include "AcStarfall.h"

#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcModelCatalog.h"
#include "AcProgressHud.h"
#include "AcResources.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#include "NetMirror.h"
#include "NetProtocol.h"
#include "Rules.h"
#include "Starfall.h"
#include "WindowMaps.h"

namespace
{
	const TCHAR* const SphereMesh = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* const GlowPath = TEXT("/Game/Materials/M_StarfallGlow.M_StarfallGlow");
	const TCHAR* const SmokePath = TEXT("/Game/Materials/M_StarfallSmoke.M_StarfallSmoke");
	const TCHAR* const CraterPath = TEXT("/Game/Materials/M_StarfallCrater.M_StarfallCrater");
	const TCHAR* const HotPath = TEXT("/Game/Materials/M_OpalHot.M_OpalHot");
	const TCHAR* const ShimmerPath = TEXT("/Game/Materials/M_StarfallShimmer.M_StarfallShimmer");

	TAutoConsoleVariable<float> CVarLightGain(TEXT("ac.StarfallLightGain"), 1.f,
		TEXT("Scale on the starfall's lights (the streak's head, the flash, the crater's glow)."));

	/// The smoke trail: a puff every this many seconds of the streak, each
	/// lasting `PuffLife` s, from `PuffFrom` to `PuffTo` cells across,
	/// drifting on a wind of `Wind` cells a second.
	constexpr double PuffEvery = 0.05;
	constexpr double PuffLife = 30.0;
	constexpr double PuffFrom = 5.0, PuffTo = 44.0;
	constexpr double Wind = 1.6;
	/// The glowing trail behind the head: a piece of the path every this
	/// many seconds, glowing `TrainLife` s after the head passed, cooling
	/// from orange to a faint train.
	constexpr double TrainEvery = 0.06;
	constexpr double TrainLife = 4.5;
	/// The impact: the flash's seconds, the dust ring's seconds and puffs.
	constexpr double FlashLife = 1.4;
	constexpr double DustLife = 9.0;
	constexpr int32 DustPuffs = 56;
	/// The crater: hot at first, cooling over this many seconds.
	constexpr double CoolTime = 600.0;
	/// The deposit drawn this much bigger than a plain one: rich.
	constexpr double HotSize = 1.3;
	/// Embers over a hot deposit at night (`DayEmbers` by day, small and
	/// dim, the heat shimmer over it instead).
	constexpr int32 Embers = 28;
	constexpr int32 DayEmbers = 7;
	constexpr int32 ShimmerColumns = 9;

	FName OreModel(const int64 Id) { return FName(*FString::Printf(TEXT("ore_%d"), int32(((Id % 4) + 4) % 4))); }

	double Rand(const uint32 Seed, const int32 K, const int32 Salt)
	{
		const uint64 X = ac::Starfall::mix((uint64(Seed) << 32) ^ (uint64(uint32(K)) << 8) ^ uint64(Salt));
		return double(X >> 11) * (1.0 / 9007199254740992.0);
	}

	FVector Cells(const FVector& C) { return FVector(C.X * AcSpace::CmPerCell, C.Y * AcSpace::CmPerCell, C.Z * AcSpace::CmPerCell); }

	/// A ground point and a height (cells) in Unreal space.
	FVector WorldAt(const ac::Vec2 P, const double Height) { return AcSpace::ToWorld(P, Height); }
}

AAcStarfall::AAcStarfall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Starfall"));
	RootComponent = Root;
}

AAcStarfall* AAcStarfall::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcStarfall> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcStarfall* AAcStarfall::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcStarfall* S = Find(World)) return S;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcStarfall");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcStarfall>(AAcStarfall::StaticClass(), FTransform::Identity, Params);
}

void AAcStarfall::EndPlay(const EEndPlayReason::Type Reason)
{
	AAcResources::Hot.Reset();
	Super::EndPlay(Reason);
}

void AAcStarfall::Load()
{
	bLoaded = true;
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, SphereMesh);
	auto Batch = [&](const TCHAR* Name, const TCHAR* Path, const int32 Floats) {
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this, Name);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Sphere);
		if (UMaterialInterface* M = LoadObject<UMaterialInterface>(nullptr, Path)) C->SetMaterial(0, M);
		else UE_LOG(LogAutocraft, Warning, TEXT("starfall: no %s (run Tools/Editor/make_starfall_materials.py)"), Path);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->SetCastShadow(false);
		C->SetNumCustomDataFloats(Floats);
		C->SetupAttachment(Root);
		C->RegisterComponent();
		AddInstanceComponent(C);
		return C;
	};
	Glows = Batch(TEXT("StarfallGlows"), GlowPath, 4);
	Smokes = Batch(TEXT("StarfallSmoke"), SmokePath, 3);
	Shimmers = Batch(TEXT("StarfallShimmer"), ShimmerPath, 1);
	auto Light = [&](const TCHAR* Name) {
		UPointLightComponent* L = NewObject<UPointLightComponent>(this, Name);
		L->SetMobility(EComponentMobility::Movable);
		L->SetCastShadows(false);
		L->SetIntensityUnits(ELightUnits::Unitless);
		L->bUseInverseSquaredFalloff = false;
		L->LightFalloffExponent = 2.f;
		L->SetSourceRadius(50.f);
		L->SetVisibility(false);
		L->SetupAttachment(Root);
		L->RegisterComponent();
		AddInstanceComponent(L);
		return L;
	};
	HeadLight = Light(TEXT("StarfallHead"));
	FlashLight = Light(TEXT("StarfallFlash"));
	CraterMaterial = LoadObject<UMaterialInterface>(nullptr, CraterPath);
	HotMaterial = LoadObject<UMaterialInterface>(nullptr, HotPath);
	if (!CraterMaterial || !HotMaterial)
		UE_LOG(LogAutocraft, Warning, TEXT("starfall: no crater or hot ore material (run Tools/Editor/make_starfall_materials.py)"));
	FAcSceneryOptions O;
	O.bHierarchical = false;
	O.Mobility = EComponentMobility::Movable;
	O.Material = HotMaterial;
	HotOre.Init(this, Root, O, TEXT("HotOre"));
}

void AAcStarfall::Off()
{
	if (Glows) Glows->ClearInstances();
	if (Smokes) Smokes->ClearInstances();
	if (Shimmers) Shimmers->ClearInstances();
	for (UInstancedStaticMeshComponent* C : {Glows.Get(), Smokes.Get(), Shimmers.Get()})
		if (C) C->SetVisibility(false);
	if (HeadLight) HeadLight->SetVisibility(false);
	if (FlashLight) FlashLight->SetVisibility(false);
	for (UPointLightComponent* L : CraterLights) L->SetVisibility(false);
	for (UDecalComponent* D : Decals) D->SetVisibility(false);
	if (!Hot.IsEmpty())
	{
		HotOre.Clear();
		Hot.Reset();
	}
	HotRegion = -1;
	AAcResources::Hot.Reset();
}

void AAcStarfall::Glow(const FVector& At, const double Size, const FLinearColor& Colour, const double Intensity, const FQuat& Turn,
	const FVector& Stretch)
{
	if (Intensity <= 0.0 || Size <= 0.0) return;
	// The sphere is a metre (a cell) across: `Size` cm across.
	GlowAt.Add(FTransform(Turn, At, Stretch * (Size / 100.0)));
	GlowData.Append({float(Intensity), Colour.R, Colour.G, Colour.B});
}

void AAcStarfall::Smoke(const FVector& At, const double Size, const double Opacity, const double Warm)
{
	if (Opacity <= 0.003 || Size <= 0.0) return;
	SmokeAt.Add(FTransform(FQuat::Identity, At, FVector(Size / 100.0)));
	SmokeData.Append({float(FMath::Clamp(Opacity, 0.0, 1.0)), 1.f, float(FMath::Clamp(Warm, 0.0, 1.0))});
}

void AAcStarfall::Shimmer(const FVector& At, const double Size, const double Strength, const double Tall)
{
	if (Strength <= 0.003 || Size <= 0.0) return;
	ShimmerAt.Add(FTransform(FQuat::Identity, At, FVector(Size / 100.0, Size / 100.0, Size * Tall / 100.0)));
	ShimmerData.Add(float(FMath::Clamp(Strength, 0.0, 1.0)));
}

void AAcStarfall::Announce(const FDrawn& F, const FString& Region, const double Time)
{
	// From the pilot's Prospector (the camera before it is seated).
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	ac::Vec2 From = AcSpace::ToSim(Cam);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	if (const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr) From = U->position;
	const ac::Vec2 To(F.Impact.X, F.Impact.Y);
	const double Bearing = ac::Starfall::bearing(From, To);
	const double Metres = ac::length(To - From);
	const bool bComing = F.ImpactTime > Time;
	const FString Title = bComing ? FString::Printf(TEXT("KSTR: STARFALL IN %.0f"), FMath::Max(1.0, F.ImpactTime - Time))
								  : FString(TEXT("KSTR: STARFALL DOWN"));
	const FString Where = Metres < 1000 ? FString::Printf(TEXT("%.0f m"), Metres) : FString::Printf(TEXT("%.1f km"), Metres / 1000);
	const FString Sub = FString::Printf(TEXT("%s  ·  bearing %03.0f°, %s  ·  a rich deposit in the crater, first come first served"), *Region,
		Bearing, *Where);
	UE_LOG(LogAutocraft, Display, TEXT("starfall: %s, %s (fall %u at clock %.1f s)"), *Title, *Sub, F.Id, F.ImpactTime);
	if (const UAcProgressHudSubsystem* HudSys = UAcProgressHudSubsystem::Get(this))
	{
		if (const TSharedPtr<SAcProgressHud> W = HudSys->Widget()) W->Toast(Title, Sub, FAcHudStyle::Amber(), FPlatformTime::Seconds());
	}
}

void AAcStarfall::DrawStreak(const FDrawn& F, const double Time)
{
	const double Before = F.ImpactTime - Time;
	const FVector Ground = Cells(F.Impact);
	auto HeadAt = [&](const double B) {
		const ac::Vec3 H = ac::Starfall::head(B, F.From);
		return Ground + Cells(FVector(H.x, H.z, H.y));
	};
	const double First = F.ImpactTime - ac::Starfall::streak;
	const double Last = FMath::Min(Time, F.ImpactTime);
	// The smoke trail: puffs left along the path, spreading, thinning and
	// drifting on the wind (one way a fall, a little up); they linger half
	// a minute after the impact.
	if (Time - First > 0 && Time < F.ImpactTime + PuffLife)
	{
		const uint32 Seed = F.Id * 7919u + 3u;
		const double WindAt = Rand(Seed, 0, 9) * UE_DOUBLE_TWO_PI;
		const FVector Blow(FMath::Cos(WindAt), FMath::Sin(WindAt), 0.12);
		int32 K = 0;
		for (double E = First; E <= Last; E += PuffEvery, ++K)
		{
			const double Age = Time - E;
			if (Age > PuffLife) continue;
			const double A = Age / PuffLife;
			const FVector Drift = (Blow * Wind * Age + FVector(Rand(Seed, K, 1) - 0.5, Rand(Seed, K, 2) - 0.5, Rand(Seed, K, 3) * 0.6) *
								  (1.0 + 6.0 * FMath::Sqrt(A))) * AcSpace::CmPerCell;
			// Young puffs near the head glow with its fire.
			const double Warm = FMath::Exp(-Age * 1.5);
			Smoke(HeadAt(F.ImpactTime - E) + Drift, AcSpace::ToCm(FMath::Lerp(PuffFrom, PuffTo, FMath::Sqrt(A))) * (0.7 + 0.6 * Rand(Seed, K, 4)),
				0.7 * FMath::Pow(1.0 - A, 1.6) * FMath::Min(1.0, Age * 4.0 + 0.3), Warm);
		}
	}
	// The glowing trail: the air it tore through, a piece of the path at a
	// time, orange and wide where the head just passed, cooling to a thin
	// pale train that fades a few seconds on (after the impact too).
	if (Time - First > 0 && Time < F.ImpactTime + TrainLife)
	{
		for (double E = FMath::Max(First, Time - TrainLife); E + TrainEvery <= Last + 1e-6; E += TrainEvery)
		{
			const double Age = Time - (E + TrainEvery);
			if (Age < 0) continue;
			const FVector A = HeadAt(F.ImpactTime - E), B = HeadAt(F.ImpactTime - E - TrainEvery);
			const FVector Along = B - A;
			const double Len = Along.Size();
			if (Len <= 1.0) continue;
			const double Fresh = FMath::Exp(-Age * 2.4);
			const double Cool = FMath::Exp(-Age * 0.55) * (1.0 - Age / TrainLife);
			const double Thick = 1.4 + 2.2 * FMath::Sqrt(Age / TrainLife);
			const FQuat Turn = FRotationMatrix::MakeFromX(Along / Len).ToQuat();
			const FVector Mid = (A + B) * 0.5;
			const double Stretch = Len / AcSpace::ToCm(Thick) * 1.35;
			const FLinearColor Colour = FLinearColor::LerpUsingHSV(FLinearColor(0.75f, 0.85f, 1.0f), FLinearColor(1.0f, 0.45f, 0.12f), float(Fresh));
			Glow(Mid, AcSpace::ToCm(Thick), Colour, 4.5 * Fresh + 0.9 * Cool, Turn, FVector(Stretch, 1.0, 1.0));
		}
	}
	if (Before <= 0 || Before > ac::Starfall::streak)
	{
		return;
	}
	// The head: white hot, flickering as it burns away, a halo, sparks shed
	// behind it.
	const FVector Head = HeadAt(Before);
	const FVector Back = (HeadAt(Before + 0.5) - Head).GetSafeNormal();
	const double In = FMath::Clamp((ac::Starfall::streak - Before) / 1.5, 0.0, 1.0);
	const double Seed = double(F.Id % 977);
	const double Flare = FMath::Pow(FMath::Max(0.0, FMath::Sin(Time * 5.3 + Seed) * FMath::Sin(Time * 2.9 + 2.0 * Seed)), 6.0);
	const double Flicker = 0.72 + 0.12 * FMath::Sin(Time * 37.0 + Seed) + 0.1 * FMath::Sin(Time * 61.0 + 3.0 * Seed) + 0.6 * Flare;
	Glow(Head, AcSpace::ToCm(6.0), FLinearColor(1.0f, 0.97f, 0.9f), 60.0 * In * Flicker);
	Glow(Head, AcSpace::ToCm(18.0), FLinearColor(1.0f, 0.6f, 0.25f), 2.6 * In * Flicker);
	const FQuat Along = FRotationMatrix::MakeFromX(Back).ToQuat();
	Glow(Head + Back * AcSpace::ToCm(24.0), AcSpace::ToCm(2.0), FLinearColor(1.0f, 0.62f, 0.25f), 16.0 * In, Along, FVector(26.0, 1.0, 1.0));
	const uint32 Sparks = F.Id * 31337u + 5u;
	for (int32 K = 0; K < 10; ++K)
	{
		// A spark shed every so often, falling behind and dying in a second.
		const double Period = 0.7 + 0.5 * Rand(Sparks, K, 1);
		const double Phase = FMath::Fmod(Time / Period + Rand(Sparks, K, 2), 1.0);
		const FVector Jitter = FVector(Rand(Sparks, K, 3) - 0.5, Rand(Sparks, K, 4) - 0.5, Rand(Sparks, K, 5) - 0.5) * AcSpace::ToCm(6.0 * Phase);
		Glow(Head + Back * AcSpace::ToCm(70.0 * Phase * (0.6 + 0.8 * Rand(Sparks, K, 6))) + Jitter, AcSpace::ToCm(1.6),
			FLinearColor(1.0f, 0.55f, 0.18f), 14.0 * In * (1.0 - Phase) * (1.0 - Phase));
	}
	HeadLight->SetWorldLocation(Head);
	HeadLight->SetLightColor(FLinearColor(1.0f, 0.7f, 0.4f));
	HeadLight->SetIntensity(float(1200.0 * In * Flicker * CVarLightGain.GetValueOnGameThread()));
	HeadLight->SetAttenuationRadius(float(AcSpace::ToCm(100.0)));
	HeadLight->SetVisibility(true);
}

void AAcStarfall::DrawImpact(const FDrawn& F, const double Time)
{
	const double Age = Time - F.ImpactTime;
	if (Age < 0 || Age > DustLife) return;
	const FVector Ground = Cells(F.Impact);
	const uint32 Seed = F.Id * 104729u + 11u;
	// The flash: a ball of light that swells and fades, and its light.
	if (Age < FlashLife)
	{
		const double A = Age / FlashLife;
		const double Fade = FMath::Exp(-Age * 4.0);
		Glow(Ground + FVector(0, 0, AcSpace::ToCm(2.0)), AcSpace::ToCm(4.0 + 16.0 * FMath::Sqrt(A)), FLinearColor(1.0f, 0.72f, 0.42f), 1.5 * Fade);
		Glow(Ground + FVector(0, 0, AcSpace::ToCm(2.0)), AcSpace::ToCm(3.0 + 4.5 * A), FLinearColor(1.0f, 0.97f, 0.9f), 6.0 * Fade);
		FlashLight->SetWorldLocation(Ground + FVector(0, 0, AcSpace::ToCm(6.0)));
		FlashLight->SetLightColor(FLinearColor(1.0f, 0.8f, 0.55f));
		FlashLight->SetIntensity(float(600.0 * Fade * CVarLightGain.GetValueOnGameThread()));
		FlashLight->SetAttenuationRadius(float(AcSpace::ToCm(50.0)));
		FlashLight->SetVisibility(true);
	}
	// The dust ring: thrown out fast, slowing, rising and thinning.
	const double A = Age / DustLife;
	const double Out = ac::Starfall::craterRadius + 26.0 * (1.0 - FMath::Exp(-Age * 1.6));
	for (int32 K = 0; K < DustPuffs; ++K)
	{
		const double Ang = (double(K) + Rand(Seed, K, 1)) / DustPuffs * UE_DOUBLE_TWO_PI;
		const double R = Out * (0.8 + 0.35 * Rand(Seed, K, 2));
		const FVector P = Ground + FVector(FMath::Cos(Ang) * AcSpace::ToCm(R), FMath::Sin(Ang) * AcSpace::ToCm(R),
										   AcSpace::ToCm(0.8 + 2.5 * A + 1.5 * Rand(Seed, K, 3)));
		Smoke(P, AcSpace::ToCm(5.0 + 10.0 * FMath::Sqrt(A)) * (0.7 + 0.6 * Rand(Seed, K, 4)), 0.85 * (1.0 - A) * (1.0 - A),
			0.25 * FMath::Exp(-Age * 2.0));
	}
	// A column of smoke over the crater, lit from below at first.
	for (int32 K = 0; K < 18; ++K)
	{
		const double Up = (K + Rand(Seed, K, 5)) / 18.0;
		const double H = Up * (4.0 + 22.0 * FMath::Sqrt(A));
		const FVector P = Ground + FVector((Rand(Seed, K, 6) - 0.5) * AcSpace::ToCm(3.0 + 4.0 * Up), (Rand(Seed, K, 7) - 0.5) * AcSpace::ToCm(3.0 + 4.0 * Up),
										   AcSpace::ToCm(H));
		Smoke(P, AcSpace::ToCm(3.0 + 5.0 * Up + 3.0 * A), 0.6 * (1.0 - A) * (1.0 - 0.4 * Up), FMath::Exp(-Age * 0.8) * (1.0 - Up));
	}
}

void AAcStarfall::DrawCraters(const TArray<FDrawn>& Here, const double Time, const double Dark)
{
	int32 N = 0;
	for (const FDrawn& F : Here)
	{
		const double Age = Time - F.ImpactTime;
		if (Age < 0) continue;
		// The decal: scorched ground, a rim, rays of thrown dust; hot at
		// first, fading over the crater's life.
		if (Decals.Num() <= N)
		{
			UDecalComponent* D = NewObject<UDecalComponent>(this);
			D->SetupAttachment(Root);
			D->SetRelativeRotation(FRotator(-90.0, 0.0, 0.0));
			D->RegisterComponent();
			AddInstanceComponent(D);
			UMaterialInstanceDynamic* M = CraterMaterial ? UMaterialInstanceDynamic::Create(CraterMaterial, this) : nullptr;
			if (M) D->SetDecalMaterial(M);
			Decals.Add(D);
			DecalMaterials.Add(M);
			UPointLightComponent* L = NewObject<UPointLightComponent>(this);
			L->SetMobility(EComponentMobility::Movable);
			L->SetCastShadows(false);
			L->SetIntensityUnits(ELightUnits::Unitless);
			L->bUseInverseSquaredFalloff = false;
			L->LightFalloffExponent = 2.f;
			L->SetupAttachment(Root);
			L->RegisterComponent();
			AddInstanceComponent(L);
			CraterLights.Add(L);
		}
		const double Life = Age / ac::Starfall::craterLife;
		const double Fade = Life < 0.8 ? 1.0 : FMath::Clamp((1.0 - Life) / 0.2, 0.0, 1.0);
		const double Heat = FMath::Exp(-Age / (CoolTime / 3.0));
		const double Span = ac::Starfall::craterRadius * 2.6;
		UDecalComponent* D = Decals[N];
		D->SetWorldLocation(Cells(F.Impact));
		D->DecalSize = FVector(AcSpace::ToCm(6.0), AcSpace::ToCm(Span), AcSpace::ToCm(Span));
		D->SetVisibility(Fade > 0);
		if (UMaterialInstanceDynamic* M = DecalMaterials[N])
		{
			M->SetScalarParameterValue(TEXT("Fade"), float(Fade));
			M->SetScalarParameterValue(TEXT("Heat"), float(Heat));
			M->SetScalarParameterValue(TEXT("Seed"), float(F.Id % 97));
		}
		// Its glow: the deposit's warm light, strong while hot, and its own
		// at night (the opal's veins run hot).
		const bool bOre = F.Patch >= 0;
		UPointLightComponent* L = CraterLights[N];
		L->SetWorldLocation(Cells(F.Impact) + FVector(0, 0, AcSpace::ToCm(6.0)));
		L->SetLightColor(FLinearColor(1.0f, 0.5f, 0.18f));
		// By day a touch against the sun (stronger, it lit the dark crust
		// gold); at night the pool of warm light.
		const double Glowing = (bOre ? 0.3 + 0.9 * Dark : 0.0) + 1.5 * Heat;
		L->SetIntensity(float((4.0 + 21.0 * Dark) * Glowing * Fade * CVarLightGain.GetValueOnGameThread()));
		L->SetAttenuationRadius(float(AcSpace::ToCm(12.0)));
		L->SetVisibility(Glowing * Fade > 0.01);
		++N;
	}
	for (int32 I = N; I < Decals.Num(); ++I)
	{
		Decals[I]->SetVisibility(false);
		CraterLights[I]->SetVisibility(false);
	}
	// Embers off the hot deposits: sparks that rise, flicker and die, at
	// night; by day a few small dim ones, and the air shimmering over it
	// while it is hot.
	for (const FHot& H : Hot)
	{
		if (H.Stage <= 0) continue;
		const uint32 Seed = uint32(H.Id) * 2246822519u + 7u;
		const int32 Count = FMath::RoundToInt32(FMath::Lerp(double(DayEmbers), double(Embers), Dark));
		const double Small = FMath::Lerp(0.6, 1.0, Dark);
		for (int32 K = 0; K < Count; ++K)
		{
			const double Period = 1.8 + 1.6 * Rand(Seed, K, 1);
			const double Phase = FMath::Fmod(Time / Period + Rand(Seed, K, 2), 1.0);
			const double Ang = Rand(Seed, K, 3) * UE_DOUBLE_TWO_PI;
			const double R = 0.4 + 1.3 * Rand(Seed, K, 4) + 0.4 * Phase;
			const FVector P = H.At + FVector(FMath::Cos(Ang + Phase) * AcSpace::ToCm(R), FMath::Sin(Ang + Phase) * AcSpace::ToCm(R),
											 AcSpace::ToCm(0.3 + 3.2 * Phase));
			const double Life = FMath::Sin(Phase * UE_DOUBLE_PI) * (0.6 + 0.4 * FMath::Sin(Time * 23.0 + K * 3.1));
			Glow(P, AcSpace::ToCm((0.09 + 0.06 * Rand(Seed, K, 5)) * Small), FLinearColor(1.0f, 0.42f + 0.3f * float(Rand(Seed, K, 6)), 0.1f),
				(3.0 + 33.0 * Dark) * Life);
		}
		// The shimmer: tall soft columns of bent air rising off it, slow.
		const double Age = Time - H.Born;
		const double Heat = FMath::Exp(-FMath::Max(0.0, Age) / (CoolTime / 3.0));
		const double ByDay = (1.0 - Dark) * (0.35 + 0.65 * Heat);
		for (int32 K = 0; K < ShimmerColumns; ++K)
		{
			const double Period = 3.0 + 2.0 * Rand(Seed, K, 11);
			const double Phase = FMath::Fmod(Time / Period + Rand(Seed, K, 12), 1.0);
			const double Ang = Rand(Seed, K, 13) * UE_DOUBLE_TWO_PI;
			const double R = 0.8 * Rand(Seed, K, 14);
			const FVector P = H.At + FVector(FMath::Cos(Ang) * AcSpace::ToCm(R), FMath::Sin(Ang) * AcSpace::ToCm(R), AcSpace::ToCm(1.6 + 3.5 * Phase));
			Shimmer(P, AcSpace::ToCm(2.2 + 1.2 * Phase), ByDay * FMath::Sin(Phase * UE_DOUBLE_PI), 1.8);
		}
	}
}

void AAcStarfall::SyncHotOre(const TArray<FDrawn>& Here)
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::GameState& St = Sim->Simulation().state;
	// The deposits wanted: landed here, in the mirror.
	TArray<int64> Want;
	TMap<int64, double> BornOf;
	for (const FDrawn& F : Here)
		if (F.Patch >= 0 && F.Patch < int64(St.patches.size()))
		{
			Want.Add(St.patches[size_t(F.Patch)].id);
			BornOf.Add(Want.Last(), F.ImpactTime);
		}
	TSet<int64> Ids(Want);
	const int64 Gen = Mmo ? Mmo->Generation() : 0;
	bool bSame = Gen == HotGeneration && Want.Num() == Hot.Num();
	for (int32 I = 0; bSame && I < Hot.Num(); ++I) bSame = Hot[I].Id == Want[I];
	if (!bSame)
	{
		HotOre.Clear();
		Hot.Reset();
		HotGeneration = Gen;
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		TArray<FAcSceneryModel*> Models;
		Hot.Reserve(Want.Num());
		for (const int64 Id : Want)
		{
			const ac::OreDeposit* P = nullptr;
			for (const ac::OreDeposit& X : St.patches)
				if (X.id == Id) P = &X;
			const FAcModelInfo* Model = P ? FAcModelCatalog::Get().Find(OreModel(Id)) : nullptr;
			if (!Model) continue;
			const FTransform Placement(FRotator(0.0, -FMath::RadiansToDegrees(P->angle), 0.0),
				AcSpace::ToWorld(P->position, Terrain ? Terrain->FieldHeight(P->position) : 0.0), FVector(HotSize));
			FHot& H = Hot.AddDefaulted_GetRef();
			H.Id = Id;
			H.At = Placement.GetTranslation();
			H.Model = HotOre.AddModel(*Model, Placement);
			for (int32 I = 0; I < Model->Parts.Num(); ++I)
			{
				const FString Name = Model->Parts[I].Name.ToString();
				if (Model->Parts[I].Parent == INDEX_NONE) H.RootPart = I;
				else if (Name.StartsWith(TEXT("stage")))
				{
					const int32 K3 = FCString::Atoi(*Name + 5);
					if (K3 >= 1 && K3 <= 3) H.StagePart[K3 - 1] = I;
				}
			}
		}
		for (FHot& H : Hot) Models.Add(&H.Model);
		HotOre.Flush(Models);
		if (!Hot.IsEmpty()) UE_LOG(LogAutocraft, Log, TEXT("starfall: %d hot deposits drawn here"), Hot.Num());
		// The crater's decal would paint its floor and its hot core on the
		// deposit (it read gold, its own crust unseen): the deposit takes none.
		TArray<UInstancedStaticMeshComponent*> Batches;
		GetComponents(Batches);
		for (UInstancedStaticMeshComponent* C : Batches)
			if (C != Glows && C != Smokes && C != Shimmers) C->SetReceivesDecals(false);
	}
	AAcResources::Hot = Ids;
	for (FHot& H : Hot) H.Born = BornOf.FindRef(H.Id);
	// Their stages, as the plain ore's (AAcResources::ApplyStage).
	for (FHot& H : Hot)
	{
		int64 Stage = 0;
		for (const ac::OreDeposit& X : St.patches)
			if (X.id == H.Id) Stage = X.stage();
		if (Stage == H.Stage) continue;
		H.Stage = Stage;
		if (H.RootPart != INDEX_NONE) HotOre.SetVisible(H.Model, H.RootPart, Stage > 0);
		for (int32 K = 1; K <= 3; ++K)
			if (H.StagePart[K - 1] != INDEX_NONE) HotOre.SetVisible(H.Model, H.StagePart[K - 1], Stage >= K && Stage > 0);
	}
	HotOre.Commit();
}

void AAcStarfall::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	const std::optional<ac::net::Sky>* SkyP = Mmo ? &Mmo->Sky() : nullptr;
	const ac::net::WorldMessage* WorldMsg = Mmo ? Mmo->World() : nullptr;
	if (!Mmo || !SkyP || !*SkyP || !WorldMsg || !Sim->Simulation().map || WorldMsg->here < 0 ||
		WorldMsg->here >= int64(WorldMsg->regions.size()))
	{
		Off();
		return;
	}
	if (!bLoaded) Load();
	const ac::net::Sky& Sky = **SkyP;
	const double Time = Sky.time;
	const double Dark = ac::Rules::dark(Sky.hour());
	const ac::net::WorldMessage::Region& HereR = WorldMsg->regions[size_t(WorldMsg->here)];
	const ac::MapCells HereCells = ac::cells(HereR.size);
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());

	GlowAt.Reset();
	GlowData.Reset();
	SmokeAt.Reset();
	SmokeData.Reset();
	ShimmerAt.Reset();
	ShimmerData.Reset();
	HeadLight->SetVisibility(false);
	FlashLight->SetVisibility(false);

	TArray<FDrawn> Here;
	for (const ac::net::Sky::Starfall& S : Sky.starfalls)
	{
		if (S.region < 0 || S.region >= int64(WorldMsg->regions.size())) continue;
		const ac::net::WorldMessage::Region& R = WorldMsg->regions[size_t(S.region)];
		FDrawn F;
		F.Id = S.id;
		F.ImpactTime = S.impact;
		F.From = S.from;
		F.bHere = S.region == WorldMsg->here;
		// Another region's ground: placed by the world grid, side by side.
		ac::Vec2 At = S.at;
		double Height = 0;
		if (F.bHere)
		{
			Height = Terrain ? Terrain->FieldHeight(At) : 0.0;
			F.Patch = S.patch ? *S.patch : -1;
		}
		else
		{
			const ac::MapCells C = ac::cells(R.size);
			At.x += double(R.column - HereR.column) * (HereCells.width + C.width) / 2.0;
			At.y += double(R.row - HereR.row) * (HereCells.depth + C.depth) / 2.0;
		}
		F.Impact = FVector(At.x, At.y, Height);
		// The radio: once when told, once when down (here and next door).
		const bool bNear = F.bHere || (FMath::Abs(R.column - HereR.column) <= 1 && FMath::Abs(R.row - HereR.row) <= 1);
		if (!Announced.Contains(S.id) && Time < S.impact)
		{
			Announced.Add(S.id);
			Announce(F, FString(UTF8_TO_TCHAR(R.name.c_str())), Time);
		}
		if (!Landed.Contains(S.id) && Time >= S.impact && Time < S.impact + 30.0)
		{
			Landed.Add(S.id);
			if (bNear) Announce(F, FString(UTF8_TO_TCHAR(R.name.c_str())), Time);
		}
		DrawStreak(F, Time);
		DrawImpact(F, Time);
		if (F.bHere) Here.Add(F);
	}
	SyncHotOre(Here);
	DrawCraters(Here, Time, Dark);

	auto Push = [](UInstancedStaticMeshComponent* C, const TArray<FTransform>& At, const TArray<float>& Data, const int32 Floats) {
		C->ClearInstances();
		// Hidden when empty: a cleared batch could still draw its last
		// instances (the streak's head showed again after a view change).
		C->SetVisibility(!At.IsEmpty());
		if (At.IsEmpty()) return;
		C->AddInstances(At, false, true);
		for (int32 I = 0; I < At.Num(); ++I) C->SetCustomData(I, TArrayView<const float>(Data.GetData() + I * Floats, Floats), false);
		C->MarkRenderStateDirty();
	};
	Push(Glows, GlowAt, GlowData, 4);
	Push(Smokes, SmokeAt, SmokeData, 3);
	Push(Shimmers, ShimmerAt, ShimmerData, 1);
}
