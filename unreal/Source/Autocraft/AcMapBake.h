// `-AcMapBake=DIR`: the world map's satellite pictures (M2, docs/regions.md
// "The satellite map"). For every region of a world the game builds its
// ground as a client does (`ac::net::Mirror` from the region file: the
// terrain, its materials, cliffs, rocks and plants, the ore and the wells; no
// units, no buildings, no fog), lights it with the world's day (a held hour,
// clear weather), and photographs it straight down through an orthographic
// scene capture: `DIR/<region id>.png`, `-AcMapBakePx` pixels a cell (8),
// north up. Ground out of play (`MapDefinition::inPlay`: whatever outline
// the region has) is transparent. Then it quits.
//
//   uv run unreal/Tools/bake_map.py      (this run, then the import into
//                                         /Game/UI/Map, both hidden)
//
// -AcMapBakeWorld=FILE  the world (unreal/server/worlds/first.json)
// -AcMapBakeOnly=ID,ID  only these regions
// -AcMapBakeHour=H      the hour of the light (10.5)
//
// Run it as a picture run (-game -RenderOffscreen) on any map
// (-AcMap=highlands-small): the bake shows each region in its place.
#pragma once

#include "CoreMinimal.h"
#include "RegionFile.h"
#include "Subsystems/WorldSubsystem.h"

#include "NetMirror.h"

#include <memory>

#include "AcMapBake.generated.h"

class ASceneCapture2D;
class UTextureRenderTarget2D;

UCLASS()
class AUTOCRAFT_API UAcMapBakeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/// This process was started to bake.
	static bool IsBakeRun();

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FRegion
	{
		FString Id;
		FString Text;
		ac::MapChoice Choice;
		/// Its palette and its place on the grid (W3, W4): the ground is
		/// painted as in the game, blended toward the neighbours' at the edges.
		ac::region::Ground Ground = ac::region::Ground::plain;
		int32 Column = 0, Row = 0;
	};
	/// Shows region `Index`, or captures it once it has settled.
	void Show(int32 Index);
	bool Capture(int32 Index);

	FString Dir;
	TArray<FRegion> Regions;
	/// Every region of the world (the neighbours' palettes).
	TArray<FRegion> All;
	int32 PixelsPerCell = 8;
	double Hour = 10.5;
	int32 Current = INDEX_NONE;
	int32 Frames = 0;
	int32 Failures = 0;
	bool bDone = false;
	std::unique_ptr<ac::net::Mirror> Mirror;
	UPROPERTY()
	TObjectPtr<ASceneCapture2D> Camera;
	UPROPERTY()
	TObjectPtr<UTextureRenderTarget2D> Target;
};
