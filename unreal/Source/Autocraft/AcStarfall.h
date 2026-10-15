// `AAcStarfall`: starfalls on screen in the MMORPG mode (docs/weather.md,
// "Starfall"; docs/mmorpg-m7.md E2). The server picks them and tells every
// pilot (`net::Sky::starfalls`); this draws them from the world's sky as the
// client follows it (`FAcMmoClient::Sky`), nothing of its own:
//
// - the radio's toast a minute before ("STARFALL IN 60", the region and the
//   bearing from the camera), and at the impact;
// - the streak, `ac::Starfall::head` over its last `streak` seconds: a white
//   head that flickers and sheds sparks, a halo, a long glowing trail along
//   the path it came that cools from orange to a faint train, a light that
//   sweeps the land, and a smoke trail that lingers half a minute and
//   drifts on the wind; seen from the neighbouring regions too (placed by
//   the world grid);
// - the impact (E1, epic): a flash seen across the region (its light, the
//   screen flaring white, strongest near), a shockwave ring racing out past
//   the blast's edge, rocks thrown out glowing, a dust ring and a tall
//   column of smoke, and the ground heaving under the camera
//   (`UAcEffects::Quake`) within four blast radii; the server does the
//   blast's damage;
// - in the pilot's region, the crater (a decal, M_StarfallCrater) hot at
//   first, fading over a world day, the blast's ground scorched round it
//   (the same decal, faint, the blast's width), and its deposit drawn with M_OpalHot
//   (molten veins in a dark crust, its own glow at night, embers rising off
//   it at night and heat shimmer over it by day, M_StarfallShimmer, and a
//   warm light), hidden from AAcResources' plain ore (`AAcResources::Hot`).
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
#include "SimdMath.h"

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
		/// Its blast's radius, cells (`ac::Starfall::blastRadius` of its
		/// region).
		double Blast = 18;
		/// Its deposit's index in the mirror's patches (here, landed).
		int64 Patch = -1;
	};

	void Load();
	void Off();
	void Announce(const FDrawn& F, const FString& Region, double Time);
	void DrawStreak(const FDrawn& F, double Time);
	void DrawImpact(const FDrawn& F, double Time);
	/// At the impact, once a fall: the quake and the screen's flash, by how
	/// far the camera stands from it.
	void Felt(const FDrawn& F);
	/// Where the pilot stands (its driven unit; the camera before it is
	/// seated), this region's ground.
	ac::Vec2 ViewerAt() const;
	void DrawCraters(const TArray<FDrawn>& Here, double Time, double Dark);
	void SyncHotOre(const TArray<FDrawn>& Here);
	void Glow(const FVector& At, double Cells, const FLinearColor& Colour, double Intensity, const FQuat& Turn = FQuat::Identity,
		const FVector& Stretch = FVector::OneVector);
	void Smoke(const FVector& At, double Cells, double Opacity, double Warm);
	void Shimmer(const FVector& At, double Cells, double Strength, double Tall);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Glows;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Smokes;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Shimmers;
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
	/// E1: the blast's scorched ground round each crater.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> Scorches;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ScorchMaterials;
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
		/// Its fall's impact (the world's clock): how hot it still is.
		double Born = 0;
	};
	TArray<FHot> Hot;
	int64 HotRegion = -1;
	int64 HotGeneration = -1;
	/// The falls announced and told down (their ids).
	TSet<uint32> Announced, Landed, Quaked;
	/// Instance data built each frame.
	TArray<FTransform> GlowAt, SmokeAt, ShimmerAt;
	TArray<float> GlowData, SmokeData, ShimmerData;
};
