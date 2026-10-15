// `AAcBrine`: a canyon zone's flooded floor (Z3, docs/canyon.md "The Drowned
// Cut"): a sheet of brine over the zone's `flood` outline in the pilot's
// region, at the canyon floor's height, in the Metallic Hydrogen pool's
// material (M_Mercury) tinted a pale salt green, its ripples on. The ground
// above the water (the rims) hides it, so the sheet shows only on the floor.
// The server slows the Brood's walkers in it (WorldCanyon.cpp).
//
// Usage: `AAcBrine::SpawnFor(World)` once per game world (AAcGameMode).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcBrine.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AUTOCRAFT_API AAcBrine : public AActor
{
	GENERATED_BODY()

public:
	AAcBrine();
	static AAcBrine* SpawnFor(UWorld* World);
	static AAcBrine* Find(const UWorld* World);
	virtual void Tick(float DeltaSeconds) override;
	/// The sheets drawn now (the shots check it).
	int32 Sheets() const { return Drawn; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;
	FString BuiltFor;
	int32 Drawn = 0;
	double RebuildAt = 0;
};
