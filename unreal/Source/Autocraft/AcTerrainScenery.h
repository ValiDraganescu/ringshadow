// The ground's scenery (docs/terrain-lab.md, rounds 3 and 4, promoted
// 2026-10-19): the rock kit (Tools/Editor/models/terrain_rocks.py,
// /Game/Terrain/Rocks) laid over each ground set, client-only, never
// touching the sim:
//
//   faces    slabs of layered rock on every cliff face (stacked up a tall
//            one, leaning with it, no more than 0.3 cells out of it), talus
//            at the feet, a broken crown on each spire; none beside a ramp
//   clutter  by the palette within 35 m of the camera: pebbles and dry
//            tufts (plain, rust, ochre), crust plates (salt), flakes (ash),
//            shards (glass), clinker (slag), rime (frost); never on a pad, a
//            deposit or a well
//
// Where palettes meet (ac::groundblend, the ground's own field) each thing
// is the palette's whose ground it lies on and each face is tinted by the
// palettes there, so the clutter's boundary wanders with the ground's.
//
// Each set gets one AAcTerrainScenery, made from its field, the same every
// time (seeded by the set's key), and dropped when the set goes. No
// collision, no distance fields.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Async/Future.h"
#include "Subsystems/WorldSubsystem.h"

#include "AcTerrainScenery.generated.h"

class AAcRegionGround;
class UHierarchicalInstancedStaticMeshComponent;

UCLASS()
class AUTOCRAFT_API AAcTerrainScenery : public AActor
{
	GENERATED_BODY()

public:
	AAcTerrainScenery();

	/// Lay the kit over `Set` (complete) and stand with it: planned on a
	/// worker (the field's own heights), then added a few thousand
	/// instances a frame, so a region arriving never hitches.
	void Build(AAcRegionGround* Set);
	virtual void Tick(float DeltaSeconds) override;
	/// Instances by round, for the log and the shots.
	int32 Faces = 0, Talus = 0, Crowns = 0, Clutter = 0;

	/// One instance of the plan: a kit mesh (`Meshes`), where, its colour
	/// and its arrays' slice.
	struct FItem
	{
		int32 Mesh = 0;
		FTransform T;
		FLinearColor C;
		float Slice = 3;
		/// Clutter (pebbles, tufts...): kept off the buildings' footprints.
		bool bClutter = false;
	};
	struct FPlan
	{
		TArray<FItem> Items;
		int32 Faces = 0, Talus = 0, Crowns = 0, Clutter = 0;
		double Ms = 0;
	};

private:
	UHierarchicalInstancedStaticMeshComponent* Batch(int32 Mesh);
	/// C5: the clutter under a building's footprint hidden (a Landing Pad's
	/// low deck, a grown footprint), shown again once it is gone; looked at
	/// when the shown buildings change.
	void ClearUnderBuildings();

	/// A clutter instance laid: its batch, index, rest transform and where
	/// it stands (world cm, across the ground).
	struct FClutter
	{
		TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> Batch;
		int32 Index = 0;
		FTransform T;
		FVector2D At = FVector2D::ZeroVector;
		bool bHidden = false;
	};
	TArray<FClutter> Laid;
	uint32 BuildingsKey = 0;
	double NextClear = 0;

	/// The kit's meshes by index (`Names`), loaded on the game thread.
	TArray<FString> Names;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> Meshes;
	TFuture<TSharedPtr<FPlan>> Planning;
	TSharedPtr<FPlan> Plan;
	int32 Next = 0;
	FString SetKey;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RockMaterial;
};

/// Keeps one AAcTerrainScenery on each complete ground set (`ac.TerrainScenery 0`
/// takes them all away, for measuring).
UCLASS()
class AUTOCRAFT_API UAcTerrainSceneryWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAcTerrainSceneryWorld, STATGROUP_Tickables); }
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

private:
	UPROPERTY(Transient)
	TMap<TObjectPtr<AAcRegionGround>, TObjectPtr<AAcTerrainScenery>> Laid;
	/// Frames each set has stood complete (its palette is set on its
	/// material a frame or two after).
	TMap<TObjectPtr<AAcRegionGround>, int32> Waited;
};
