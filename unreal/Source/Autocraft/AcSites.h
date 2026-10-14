// `AAcSites`: the blueprint sites on the ground in the MMORPG mode
// (docs/prospector-progression.md, "Blueprint sites"; docs/mmorpg-m7.md W2).
//
// Every site of the pilot's region, from the world's region files
// (`AcProgressHud::SitesIn`), stands as its kind's model
// (`site_crate`, `site_wreck`, `site_cache`, `site_vault`, made by
// Tools/Editor/models/sites.py), the same for every pilot. Its front faces
// the lowest ground round it (a vault's door away from its rock). What the
// pilot did with it shows on it:
//
// - not opened: the `_shut` parts (the lid on, the tarp over, the door
//   closed), the gold `lamp` blinking and the vault's `seam` lit, a small
//   gold light on the ground at night;
// - known (on the pilot's map) and not opened: also a gold column of light
//   over it, widened with distance so it reads from across the region;
// - opened: the `_open` parts (the lid beside it, the tarp thrown back, the
//   door slid apart), every light out.
//
// `Where` gives a site's middle for the opening ring (AcProgressHud).
// Usage: `AAcSites::SpawnFor(World)` once per game world (AAcGameMode). Out
// of the mode it draws nothing.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcScenery.h"
#include "Progression.h"
#include "Types.h"

#include "AcSites.generated.h"

class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
struct FAcModelInfo;

UCLASS()
class AUTOCRAFT_API AAcSites : public AActor
{
	GENERATED_BODY()

public:
	AAcSites();

	static AAcSites* SpawnFor(UWorld* World);
	static AAcSites* Find(const UWorld* World);

	/// Site `Id` ("<region id>/<site name>")'s middle in Unreal space, a
	/// little over the ground; unset: not in this region.
	TOptional<FVector> Where(const FString& Id) const;
	/// Sites placed, and of them the columns shown (tests, shots).
	int32 Num() const { return Sites.Num(); }
	int32 Columns() const;

	/// Pictures only (-AcMmoShotsW2): sites stood beside the region's own
	/// at ground points (cells), each shut or opened, known or not, facing
	/// `FaceTo`. An empty list takes them away.
	struct FStaged
	{
		ac::SiteKind Kind = ac::SiteKind::crate;
		ac::Vec2 Ground;
		ac::Vec2 FaceTo;
		bool bOpened = false;
		bool bKnown = false;
	};
	void Stage(const TArray<FStaged>& In);

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FSite
	{
		FString Id;
		FVector At = FVector::ZeroVector;
		/// Model height (cm) for the ring and the column's foot.
		double Tall = 100;
		uint32 Seed = 0;
		FAcSceneryModel Model;
		TArray<int32> Shut, Open, Lamps, Seams;
		/// What is shown now (-1: not yet applied).
		int8 bOpened = -1;
		int8 bLampOn = -1;
		/// Staged: its state stays as staged.
		TOptional<FStaged> Staged;
		TObjectPtr<UStaticMeshComponent> Beam;
		TObjectPtr<UStaticMeshComponent> Halo;
		TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;
		TObjectPtr<UMaterialInstanceDynamic> HaloMaterial;
		TObjectPtr<UPointLightComponent> Light;
	};

	void Clear();
	void Place(const class FAcMmoClient& Mmo, const FString& Region);
	void Show(FSite& S, bool bOpened, bool bKnown, double Time, const FVector& Camera, double Dark);
	UStaticMeshComponent* Column(const TCHAR* Name, UMaterialInstanceDynamic*& OutMaterial);

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> Root;
	/// Keeps the columns' components and materials alive.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> Owned;

	FAcSceneryBatches Batches;
	bool bBatches = false;
	TArray<FSite> Sites;
	/// What the sites were placed for: the region, the terrain's field.
	FString PlacedRegion;
	const void* PlacedField = nullptr;
	TArray<FStaged> StagedList;
};
