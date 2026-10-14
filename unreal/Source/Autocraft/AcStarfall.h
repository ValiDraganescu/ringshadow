// `AAcStarfall`: starfalls on screen in the MMORPG mode (docs/weather.md,
// "Starfall"; docs/mmorpg-m7.md E2). The server picks them and tells every
// pilot (`net::Sky::starfalls`); this draws them from the world's sky as the
// client follows it (`FAcMmoClient::Sky`), nothing of its own:
//
// - the radio's toast a minute before ("STARFALL IN 60", the region and the
//   bearing from the camera), and at the impact;
// - the streak, `ac::Starfall::head` over its last `streak` seconds: a white
//   head with a halo and a hot tail, a light that sweeps the land, a smoke
//   trail that lingers and spreads; seen from the neighbouring regions too
//   (placed by the world grid);
// - the impact: a flash and its light, a dust ring thrown out and a column
//   of smoke;
// - in the pilot's region, the crater (a decal, M_StarfallCrater) hot at
//   first, fading over a world day, and its deposit drawn with M_OpalHot
//   (hot veins, its own glow at night, embers rising off it and a warm
//   light), hidden from AAcResources' plain ore (`AAcResources::Hot`).
//
// The materials: Tools/Editor/make_starfall_materials.py.
//
// Usage: `AAcStarfall::SpawnFor(World)` once per game world (AAcGameMode).
// Out of the mode it does nothing. Dev: -AcMmoStarfall=S drops one near the
// pilot S seconds after it comes (the server's `--starfall`).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcScenery.h"

#include "AcStarfall.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;

UCLASS()
class AUTOCRAFT_API AAcStarfall : public AActor
{
	GENERATED_BODY()

public:
	AAcStarfall();

	static AAcStarfall* SpawnFor(UWorld* World);
	static AAcStarfall* Find(const UWorld* World);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FDrawn
	{
		/// The fall's impact point in this region's ground (cells), its
		/// height (cells) and the bearing it comes from.
		FVector Impact = FVector::ZeroVector;
		double ImpactTime = 0;
		double From = 0;
		bool bHere = false;
		uint32 Id = 0;
		/// Its deposit's index in the mirror's patches (here, landed).
		int64 Patch = -1;
	};

	void Load();
	void Off();
	void Announce(const FDrawn& F, const FString& Region, double Time);
	void DrawStreak(const FDrawn& F, double Time);
	void DrawImpact(const FDrawn& F, double Time);
	void DrawCraters(const TArray<FDrawn>& Here, double Time, double Dark);
	void SyncHotOre(const TArray<FDrawn>& Here);
	void Glow(const FVector& At, double Cells, const FLinearColor& Colour, double Intensity, const FQuat& Turn = FQuat::Identity,
		const FVector& Stretch = FVector::OneVector);
	void Smoke(const FVector& At, double Cells, double Opacity, double Warm);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Glows;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Smokes;
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> HeadLight;
	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> FlashLight;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> CraterLights;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Decals;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DecalMaterials;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CraterMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HotMaterial;

	bool bLoaded = false;
	/// The hot deposits drawn: their patch ids, the region and the mirror
	/// they were drawn for, and the models with their stage parts.
	FAcSceneryBatches HotOre;
	struct FHot
	{
		int64 Id = 0;
		FAcSceneryModel Model;
		int32 RootPart = INDEX_NONE;
		int32 StagePart[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
		int64 Stage = -1;
		FVector At = FVector::ZeroVector;
	};
	TArray<FHot> Hot;
	int64 HotRegion = -1;
	int64 HotGeneration = -1;
	/// The falls announced and told down (their ids).
	TSet<uint32> Announced, Landed;
	/// Instance data built each frame.
	TArray<FTransform> GlowAt, SmokeAt;
	TArray<float> GlowData, SmokeData;
};
