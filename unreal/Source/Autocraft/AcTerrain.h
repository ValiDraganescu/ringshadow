// `AAcTerrain`: the ground (GAME-LAYER.md §2.6 "Terrain", chunk A2), the
// port of `TerrainBuilder` (Sources/Autocraft/Terrain.swift).
//
// - The map's ground: a height field from `ac::TerrainField` at 4 vertices
//   per cell, central-difference normals, each square split along the
//   diagonal from (i+1, j) to (i, j+1), as in Swift. `Heights()` is that
//   same grid (`FAcHeightGrid`, engine-free), so picking and the pilot's
//   rays meet exactly the triangles that are drawn.
// - The border: the scenery past the map's edge, out of play, 60+ cells
//   wide, finer near the edge, a touch (0.08) lower under the map's own
//   mesh, casting no shadow (`TerrainBuilder.border`).
// - The splat map: RGBA at 4 texels per cell, grass, highland, plating,
//   scorch (`TerrainField.materials`); dirt fills the rest. `M_Terrain`
//   (made by Tools/Editor/make_terrain_material.py) blends the ground
//   textures by it, puts rock on cliffs by slope and fades the border into
//   the haze.
// - Border rocks: where `GameScene.borderRocks` strews them
//   (`BorderRocks()`); the doodad chunk (B8) draws them with its doodad
//   meshes.
//
// Mesh path: the ground is a set of `UStaticMesh`es built at runtime from
// `FMeshDescription`s by the fast build (`BuildFromMeshDescriptions`,
// bFastBuild), 32 × 32-cell tiles. The map never changes during a game, so
// a static mesh is the right kind: the static draw path (no per-frame
// mesh work), per-tile culling, cached virtual shadow map pages, and a ray
// tracing representation for hardware Lumen. The fast build is the one path
// that works in a packaged game too; ProceduralMeshComponent and
// DynamicMeshComponent redraw dynamically and are meant for meshes that
// change. Nanite and mesh distance fields need the editor's builders, so the
// ground has neither (it is low-poly enough not to need Nanite; software
// Lumen sees it through screen traces).
//
// Ground sets (W4, seamless regions): each map's ground (its tiles, splat,
// material, doodads and border) is an `AAcRegionGround` of its own, kept
// while it is the pilot's region or round it. In a world the regions round
// the pilot's (`FAcMmoClient::Around`) are built ahead, a step a frame, at
// their places on the world's grid: their ground is the horizon beyond the
// walls, and walking over an edge makes the next one current with nothing
// built then (the world's origin moves under them, AcSimSubsystem.h
// `IsSeamlessStart`). Only a set at the world's rim draws the border
// scenery, on its rim sides. The material samples by world-grid cells
// (`Origin`: the current region's centre on the grid), so its textures do
// not move when the origin does.
//
// Usage: `AAcTerrain::SpawnFor(World)` once per game world (AAcGameMode):
// it builds from `UAcSimSubsystem::Map()` and rebuilds when a game on
// another map starts. Or `Build(Map)` directly.
//
// `-AcTerrainView[=POINTS_PER_CELL]` (with `-AcShot`): frames the Swift
// game's default top-down view (over blue's main base, 40 points per cell,
// pitch 56°, vertical FOV 30°, `ac::FreeView`) and, when the level has no
// sun yet, puts up a stand-in noon sun and sky. For terrain renders until
// the RTS camera (A3) and daylight (B9) exist. `-AcTerrainCover=PTS` is the
// console's cover (about 233 at 1600 × 1000), `-AcTerrainAt=X,Y` centres
// the view on a ground point (as `windowshot --at`).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcHeightGrid.h"
#include "AcScenery.h"
#include "RegionFile.h"
#include "TerrainField.h"

#include "Async/Future.h"
#include "Types.h"

#include <memory>
#include <optional>
#include <vector>

#include "AcTerrain.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UAcSimSubsystem;

class AAcRegionGround;
struct FAcRegionAround;

/// A rock strewn over the border scenery (`GameScene.borderRocks`).
struct FAcBorderRock
{
	ac::Doodad::Kind Kind = ac::Doodad::Kind::rock;
	/// The doodad's shape seed (`Doodad.variant`); the export has `variant % 4`.
	uint32 Variant = 0;
	/// Unreal world transform: on `borderHeight`, sunk 0.15 · scale, turned
	/// and scaled as in Swift.
	FTransform Transform;
};

/// One map's ground (a ground set, W4): built at once or a step at a time,
/// at its place relative to the region the pilot is in.
UCLASS()
class AUTOCRAFT_API AAcRegionGround : public AActor
{
	GENERATED_BODY()

public:
	AAcRegionGround();

	/// What to build: the map, its key (`AAcTerrain::KeyOf`), its centre on
	/// the world's grid (cells; 0 out of a world), the sides with no region
	/// beyond (bits 1 north, 2 east, 4 south, 8 west: the border scenery's),
	/// its region's index in the World message (INDEX_NONE: none).
	void Begin(std::shared_ptr<const ac::MapDefinition> Map, const FString& Key, ac::Vec2 Centre, uint8 Rim, int64 Region,
		UMaterialInterface* Base);
	/// All of it now (the region the pilot is in, when nothing was built
	/// ahead), or a step (a tile, the doodads in parts) within about
	/// `BudgetMs`. True once complete.
	void BuildNow();
	bool Step(double BudgetMs);
	bool IsComplete() const { return bComplete; }

	const FString& Key() const { return MapKey; }
	ac::Vec2 Centre() const { return GridCentre; }
	uint8 Rim() const { return RimSides; }
	int64 Region() const { return RegionIndex; }
	const ac::MapDefinition& Map() const { return *MapDef; }
	const ac::TerrainField& Field() const { return *FieldPtr; }
	const FAcHeightGrid& Grid() const { return HeightGrid; }
	UMaterialInstanceDynamic* Material() const { return Mat; }
	UTexture2D* Splat() const { return SplatTex; }
	const TArray<FAcBorderRock>& Rocks() const { return BorderRocks; }
	/// The world-grid cells of Unreal's origin (the session's first region's
	/// centre: the material's `Origin`).
	void SetOrigin(ac::Vec2 Origin);
	/// Seconds spent building on the game's thread, all steps.
	double BuildSeconds() const { return Spent; }

private:
	struct FPrepared;
	void PrepareCpu();
	void MakeMaterial();
	void AddPiece(UStaticMesh* Mesh, const TCHAR* Name, bool bCastShadow);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Mat;
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SplatTex;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

	std::shared_ptr<const ac::MapDefinition> MapDef;
	std::unique_ptr<ac::TerrainField> FieldPtr;
	FAcHeightGrid HeightGrid;
	TArray<FAcBorderRock> BorderRocks;
	FString MapKey;
	ac::Vec2 GridCentre{0, 0};
	/// The pilot's region's centre on the grid (the material's `Origin`).
	ac::Vec2 GridOrigin{0, 0};
	uint8 RimSides = 0xF;
	int64 RegionIndex = INDEX_NONE;
	/// The CPU part (field, grid, splat, mesh descriptions, placements),
	/// made on a worker thread for a set built ahead.
	TSharedPtr<FPrepared> Prepared;
	TFuture<void> Preparing;
	int32 NextPiece = 0;
	int32 NextDoodad = 0;
	bool bMaterial = false;
	bool bComplete = false;
	/// Built at once (the region the pilot is in), not ahead.
	bool bNow = false;
	double Spent = 0;
	/// The map's doodads (rocks, plants, landmarks, the walls' rocks) and
	/// the border rocks on the rim sides.
	FAcSceneryBatches DoodadBatches;
	FAcSceneryBatches RockBatches;
	TArray<FAcSceneryModel> DoodadModels;
};

UCLASS()
class AUTOCRAFT_API AAcTerrain : public AActor
{
	GENERATED_BODY()

public:
	/// Mesh vertices per cell (`TerrainBuilder.density`).
	static constexpr int32 Density = FAcHeightGrid::density;
	/// Cells per side of a mesh tile.
	static constexpr int32 TileCells = 32;
	/// Splat texels per cell.
	static constexpr int32 SplatPerCell = 4;
	/// The haze the border fades into (`TerrainBuilder.haze`), sRGB-ish as
	/// Swift writes it.
	static FLinearColor Haze() { return FLinearColor(0.27f, 0.22f, 0.19f, 1.0f); }
	/// How far the border reaches past the map, cells (`borderWidth`).
	static double BorderWidth(const ac::GroundRect& B);
	/// A map's ground set's key: its name, version and seed.
	static FString KeyOf(const ac::MapDefinition& Map);

	AAcTerrain();

	/// The terrain of the world (spawned on first ask, built from the sim's
	/// map and rebuilt when a game on another map starts).
	static AAcTerrain* SpawnFor(UWorld* World);
	static AAcTerrain* Find(const UWorld* World);

	/// Build everything for `Map` (a no-op when it is already built for it;
	/// a set built ahead is taken as it is).
	void Build(const ac::MapDefinition& Map);
	bool IsBuilt() const { return Current != nullptr; }

	/// The field (heights, materials, border). Valid once built.
	const ac::TerrainField& GetField() const { return Current->Field(); }
	/// The drawn mesh's height grid, shared with the rays (E1) and picking.
	const FAcHeightGrid& Heights() const { static const FAcHeightGrid None; return Current ? Current->Grid() : None; }
	/// The drawn ground's height (cells) under a sim ground point: the mesh
	/// triangle on the map, `borderHeight` past it.
	double HeightAt(ac::Vec2 P) const { return Current ? Current->Grid().height(P) : 0.0; }
	/// The same for an Unreal location: the ground's Z (cm) under it.
	double GroundZ(const FVector& World) const;
	/// The field's own smooth height (cells), as the sim stands units on it
	/// (`GameScene.groundY`): use this to place things, `HeightAt` to hit
	/// what is drawn.
	double FieldHeight(ac::Vec2 P) const { return Current ? Current->Field().height(P) : 0.0; }

	/// Where the border rocks go (`BorderRocks`); the set draws them.
	const TArray<FAcBorderRock>& BorderRocks() const { static const TArray<FAcBorderRock> None; return Current ? Current->Rocks() : None; }

	/// The ground material (a `M_Terrain` instance): B9 sets `Haze`, the
	/// fog chunk may read `Bounds`.
	UMaterialInstanceDynamic* GetMaterial() const { return Current ? Current->Material() : nullptr; }
	UTexture2D* GetSplat() const { return Current ? Current->Splat() : nullptr; }
	/// Every ground set's material (the current one's first) and its
	/// region's index in the World message, for what the sky sets on all
	/// of them (`AAcDaylight`); `MaterialsVersion` changes when the list does.
	void ForEachMaterial(TFunctionRef<void(UMaterialInstanceDynamic*, int64 Region)> Each) const;
	/// The map bake (AcMapBake.h, out of a world): the current ground's
	/// palette from its region file (nil: plain, or the World message's).
	struct FGroundOverride
	{
		ac::region::Ground Own = ac::region::Ground::plain;
		TOptional<ac::region::Ground> Around[4];
	};
	void SetGroundOverride(TOptional<FGroundOverride> Ground) { GroundOverride = MoveTemp(Ground); }
	const TOptional<FGroundOverride>& GetGroundOverride() const { return GroundOverride; }
	int32 MaterialsVersion() const { return Version; }
	/// The current set's centre on the world's grid (cells; 0 out of a world).
	ac::Vec2 CurrentCentre() const { return Current ? Current->Centre() : ac::Vec2(0, 0); }
	/// The ground sets (tests, pictures): how many, how many built.
	int32 NumSets() const;
	int32 NumSetsComplete() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void OnGameStarted(UAcSimSubsystem& Sim);
	/// W4: the sets round the pilot's region, made and built ahead; the far
	/// ones let go.
	void SyncAround();
	AAcRegionGround* Spawn(std::shared_ptr<const ac::MapDefinition> Map, const FString& Key, ac::Vec2 Centre, uint8 Rim, int64 Region);
	void MakeCurrent(AAcRegionGround* Set);
	/// The world frame's anchor on the grid (FAcMmoClient::SessionCentre; 0
	/// out of a world).
	ac::Vec2 SessionCentre() const;
	/// -AcTerrainView: frame the Swift default view (see the file comment).
	void StageViewIfAsked();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<AAcRegionGround>> Sets;
	UPROPERTY(Transient)
	TObjectPtr<AAcRegionGround> Current;

	/// The material the ground draws with (made by make_terrain_material.py).
	UPROPERTY(EditAnywhere, Category = "Terrain")
	TSoftObjectPtr<UMaterialInterface> TerrainMaterial;

	FDelegateHandle GameStartedHandle;
	bool bViewStaged = false;
	int32 Version = 0;
	TOptional<FGroundOverride> GroundOverride;
};
