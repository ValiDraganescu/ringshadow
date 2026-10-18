#include "AcTerrainLabScenery.h"

#include "AcLog.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcWork.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "NavGrid.h"
#include "RegionFile.h"
#include "TerrainField.h"

#include <cmath>

namespace
{
	const TCHAR* RockDir = TEXT("/Game/TerrainLab/Rocks/");
	const TCHAR* RockMaterialPath = TEXT("/Game/TerrainLab/M_TerrainLabRock.M_TerrainLabRock");

	/// A small seeded random, the same each run for a set.
	struct FRng
	{
		uint64 S;
		explicit FRng(uint64 Seed) : S(Seed ^ 0x9e3779b97f4a7c15ull) {}
		uint64 Next()
		{
			S ^= S >> 12; S ^= S << 25; S ^= S >> 27;
			return S * 0x2545f4914f6cdd1dull;
		}
		double Unit() { return double(Next() >> 11) / double(1ull << 53); }
		double Range(double A, double B) { return A + (B - A) * Unit(); }
		int32 Pick(int32 N) { return FMath::Min(int32(Unit() * N), N - 1); }
	};

	/// A spatial hash of points kept apart by at least `Gap` cells.
	struct FApart
	{
		double Gap;
		TMap<FIntPoint, TArray<ac::Vec2>> Cells;
		explicit FApart(double InGap) : Gap(InGap) {}
		FIntPoint Key(ac::Vec2 P) const { return FIntPoint(FMath::FloorToInt(P.x / Gap), FMath::FloorToInt(P.y / Gap)); }
		bool TryAdd(ac::Vec2 P)
		{
			const FIntPoint K = Key(P);
			for (int32 Dy = -1; Dy <= 1; Dy++)
				for (int32 Dx = -1; Dx <= 1; Dx++)
					if (const TArray<ac::Vec2>* L = Cells.Find(K + FIntPoint(Dx, Dy)))
						for (const ac::Vec2& Q : *L)
							if (ac::distance(P, Q) < Gap) return false;
			Cells.FindOrAdd(K).Add(P);
			return true;
		}
	};

	FLinearColor Param(UMaterialInstanceDynamic* M, const TCHAR* Name, FLinearColor Default)
	{
		FLinearColor V = Default;
		if (M) M->GetVectorParameterValue(FHashedMaterialParameterInfo(FName(Name)), V);
		return V;
	}
	float Scalar(UMaterialInstanceDynamic* M, const TCHAR* Name, float Default)
	{
		float V = Default;
		if (M) M->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(Name)), V);
		return V;
	}
}

namespace
{
	/// The kit's meshes, in index order: a kind and how many of it.
	struct FKitKind { const TCHAR* Name; int32 Count; bool bRock; };
	const FKitKind Kit[] = {{TEXT("slab"), 8, true}, {TEXT("talus"), 6, true}, {TEXT("spire"), 4, true}, {TEXT("pebble"), 6, false},
		{TEXT("crust"), 4, false}, {TEXT("shard"), 4, false}, {TEXT("clinker"), 4, false}, {TEXT("flake"), 4, false},
		{TEXT("rime"), 4, false}, {TEXT("tuft"), 4, false}};
	/// The first index of kind `Name` (its `I`th mesh: + I).
	int32 KitAt(const TCHAR* Name)
	{
		int32 At = 0;
		for (const FKitKind& K : Kit)
		{
			if (FCString::Strcmp(K.Name, Name) == 0) return At;
			At += K.Count;
		}
		return 0;
	}
	/// Clutter is drawn this near the camera only, cm.
	constexpr float ClutterCull = 3500.0f;
	/// Instances added a frame.
	constexpr int32 PerFrame = 3000;
}

AAcTerrainLabScenery::AAcTerrainLabScenery()
{
	PrimaryActorTick.bCanEverTick = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

UHierarchicalInstancedStaticMeshComponent* AAcTerrainLabScenery::Batch(const int32 Mesh)
{
	if (Batches.Num() <= Mesh) Batches.SetNum(Names.Num());
	if (Batches[Mesh]) return Batches[Mesh];
	UStaticMesh* SM = Meshes.IsValidIndex(Mesh) ? Meshes[Mesh].Get() : nullptr;
	if (!SM) return nullptr;
	const bool bRock = Mesh < KitAt(TEXT("pebble"));
	UHierarchicalInstancedStaticMeshComponent* C = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, FName(*(TEXT("Lab_") + Names[Mesh])));
	C->SetStaticMesh(SM);
	C->SetMobility(EComponentMobility::Movable);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	// Rule 4: no distance fields (their shadows cost 6 ms at night with the
	// kit in them); clutter casts no shadow and stays out of the GI.
	C->bAffectDistanceFieldLighting = false;
	C->SetCastShadow(bRock);
	if (!bRock)
	{
		C->SetAffectDynamicIndirectLighting(false);
		C->bVisibleInRayTracing = false;
		C->SetCullDistances(int32(ClutterCull * 0.8f), int32(ClutterCull));
	}
	C->SetNumCustomDataFloats(4);
	if (RockMaterial) C->SetMaterial(0, RockMaterial);
	C->SetupAttachment(Root);
	C->RegisterComponent();
	Batches[Mesh] = C;
	return C;
}

void AAcTerrainLabScenery::Build(AAcRegionGround* Set)
{
	RockMaterial = LoadObject<UMaterialInterface>(nullptr, RockMaterialPath);
	if (!RockMaterial) UE_LOG(LogAutocraft, Error, TEXT("terrain lab scenery: %s is missing (run Tools/Editor/make_terrain_lab_rock_material.py)"), RockMaterialPath);
	AttachToActor(Set, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetKey = Set->Key();

	// The kit, and the sizes the plan scales by.
	TArray<FVector> Sizes;
	for (const FKitKind& K : Kit)
		for (int32 I = 0; I < K.Count; I++)
		{
			const FString N = FString::Printf(TEXT("%s_%d"), K.Name, I);
			UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *(FString(RockDir) + TEXT("SM_") + N + TEXT(".SM_") + N));
			if (!SM) UE_LOG(LogAutocraft, Error, TEXT("terrain lab scenery: %sSM_%s is missing (run Tools/Editor/models/terrain_lab_rocks.py)"), RockDir, *N);
			Names.Add(N);
			Meshes.Add(SM);
			Sizes.Add(SM ? SM->GetBoundingBox().GetSize() : FVector(100));
		}

	// The colours, as the set's material has them (AcGroundLook set the
	// palette on it): the rock the ground's cliffs take, tinted as they are.
	UMaterialInstanceDynamic* M = Set->Material();
	const FLinearColor Tint = Param(M, TEXT("GroundTint"), FLinearColor(0.2f, 0.15f, 0.1f, 0.0f));
	const FLinearColor RockC = Param(M, TEXT("RockColour"), FLinearColor(0.1f, 0.078f, 0.052f));
	const FLinearColor DirtC = Param(M, TEXT("DirtColour"), FLinearColor(0.275f, 0.099f, 0.026f));
	const FLinearColor GrassC = Param(M, TEXT("GrassColour"), FLinearColor(0.102f, 0.116f, 0.016f));
	const int32 Layer = FMath::RoundToInt(Scalar(M, TEXT("GroundLayer"), 0.0f));
	const uint64 Seed = GetTypeHash(SetKey);
	std::shared_ptr<const ac::MapDefinition> MapPtr = Set->MapPtr();

	Planning = AcWork::Run([MapPtr, Sizes, Tint, RockC, DirtC, GrassC, Layer, Seed]() -> TSharedPtr<FPlan> {
		const double Start = FPlatformTime::Seconds();
		TSharedPtr<FPlan> Result = MakeShared<FPlan>();
		FPlan& Pl = *Result;
		const ac::MapDefinition& Map = *MapPtr;
		const ac::TerrainField Field(Map);
		const ac::GroundRect& B = Map.bounds;
		const double LH = Map.levelHeight;
		FRng Rng(Seed);
		const auto Tinted = [&Tint](const FLinearColor C, const float K) {
			return FLinearColor::LerpUsingHSV(C, FLinearColor(Tint.R, Tint.G, Tint.B), FMath::Clamp(Tint.A * K, 0.0f, 1.0f));
		};
		// The cliffs' faces take the palette as the ground's do (walls a
		// little less: their tops are the world's one rock, W4b).
		const FLinearColor WallRock = Tinted(RockC * 1.2f, 0.85f);
		const FLinearColor OwnRock = Tinted(RockC * 1.2f, 1.0f);
		const auto Add = [&Pl](const int32 Mesh, const FTransform& T, const FLinearColor C, const float Slice) {
			Pl.Items.Add(FItem{Mesh, T, C, Slice});
		};
		const auto Cm = [](const ac::Vec2 P, const double H) { return FVector(AcSpace::ToCm(P.x), AcSpace::ToCm(P.y), AcSpace::ToCm(H)); };
		const auto H = [&Field](const ac::Vec2 P) { return Field.height(P); };
		const auto Slope = [&Field](const ac::Vec2 P) {
			const double Dx = Field.height(P + ac::Vec2(0.5, 0)) - Field.height(P - ac::Vec2(0.5, 0));
			const double Dz = Field.height(P + ac::Vec2(0, 0.5)) - Field.height(P - ac::Vec2(0, 0.5));
			return ac::Vec2(Dx, Dz);
		};
		const auto OnWall = [&Field, &Map](const ac::Vec2 P) {
			if (!Map.edges || Map.edges->outline.empty()) return false;
			const ac::TerrainField::Wall W = Field.wall(P);
			return W.coverage > 0.05 && W.island < 0;
		};
		// The ramps, the base's and the region file's (height edits).
		struct FSeg { ac::Vec2 A, B; double Half; };
		TArray<FSeg> Ramps;
		for (const ac::Ramp& R : Map.ramps) Ramps.Add({R.low, R.high, R.width / 2});
		for (const ac::HeightEdit& E : Map.heightEdits)
			if (E.kind == ac::HeightEdit::Kind::ramp && E.points.size() == 2) Ramps.Add({E.points[0], E.points[1], E.width / 2});
		const auto NearRamp = [&Ramps](const ac::Vec2 P) {
			for (const FSeg& R : Ramps)
			{
				const ac::Vec2 Ab = R.B - R.A;
				const double L2 = FMath::Max(ac::dot(Ab, Ab), 1e-9);
				const double T = FMath::Clamp(ac::dot(P - R.A, Ab) / L2, -0.15, 1.15);
				if (ac::distance(P, R.A + Ab * T) < R.Half + 2.5) return true;
			}
			return false;
		};

		// Round 3: the faces. Every cell of the map (the walls' faces in it
		// too), a slab stack where the ground is steeper than a machine climbs.
		const int32 Slab0 = KitAt(TEXT("slab")), Talus0 = KitAt(TEXT("talus")), Spire0 = KitAt(TEXT("spire"));
		const double Steep = ac::NavGrid::maxSlope * 1.3;
		FApart FaceApart(1.7), TalusApart(1.3);
		for (double Z = B.minZ + 0.5; Z < B.maxZ; Z += 0.5)
			for (double X = B.minX + 0.5; X < B.maxX; X += 0.5)
			{
				const ac::Vec2 P(X + Rng.Range(-0.2, 0.2), Z + Rng.Range(-0.2, 0.2));
				const ac::Vec2 G = Slope(P);
				const double S = ac::length(G);
				if (!(S > Steep)) continue;
				// A ramp's sides: no faces within 2.5 cells of its sides (units
				// pass there; docs/terrain-lab.md, round 3).
				if (Field.ramp(P).weight > 0.02 || NearRamp(P)) continue;
				if (!FaceApart.TryAdd(P)) continue;
				// Out of the face: downhill. Its foot and top where it flattens.
				const ac::Vec2 Out = G * (-1.0 / S);
				ac::Vec2 Foot = P, Top = P;
				for (int32 K = 0; K < 16 && ac::length(Slope(Foot)) > 0.45; K++) Foot = Foot + Out * 0.25;
				for (int32 K = 0; K < 16 && ac::length(Slope(Top)) > 0.45; K++) Top = Top - Out * 0.25;
				const double Hf = H(Foot), Ht = H(Top);
				const double Rise = Ht - Hf;
				if (Rise < 0.6 * LH) continue;
				const double Run = FMath::Max(ac::distance(Foot, Top), 0.3);
				const double Angle = std::atan2(Rise, Run);
				const int32 N = FMath::Clamp(FMath::RoundToInt(Rise / 2.6), 1, 5);
				const double Yaw = FMath::RadiansToDegrees(std::atan2(Out.y, Out.x));
				const double Lean = 90.0 - FMath::RadiansToDegrees(Angle);
				const FLinearColor C = (OnWall(P) ? WallRock : OwnRock) * float(Rng.Range(0.85, 1.12));
				for (int32 I = 0; I < N; I++)
				{
					// The slab's foot on the face, a share of the way up it, set
					// in so its front stands at most 0.3 cells out of the face.
					const double T0 = double(I) / N;
					const ac::Vec2 At = Foot - Out * (Run * T0) - Out * 0.45;
					const double H0 = Hf + Rise * T0 - 0.15;
					const int32 Which = Slab0 + Rng.Pick(8);
					const double Tall = FMath::Max(Sizes[Which].Z, 50.0);
					const double Slant = AcSpace::ToCm(Rise / N) / FMath::Max(std::sin(Angle), 0.3);
					const FVector Scale(Rng.Range(0.55, 0.8), Rng.Range(0.8, 1.15), FMath::Clamp(Slant / Tall * 1.08, 0.4, 3.0));
					const FRotator R(Lean * Rng.Range(0.85, 1.0), Yaw + Rng.Range(-8.0, 8.0), Rng.Range(-3.0, 3.0));
					Add(Which, FTransform(R, Cm(At, H0), Scale), C, 3.0f);
					Pl.Faces++;
				}
				// Talus at the foot, out to a cell and a bit.
				for (int32 K = 0; K < 2; K++)
				{
					const ac::Vec2 Q = Foot + Out * Rng.Range(0.1, 1.4) + ac::Vec2(Out.y, -Out.x) * Rng.Range(-0.9, 0.9);
					if (!TalusApart.TryAdd(Q) || ac::length(Slope(Q)) > 0.6) continue;
					const double Sc = Rng.Range(0.7, 1.7);
					Add(Talus0 + Rng.Pick(6), FTransform(FRotator(Rng.Range(-20, 20), Rng.Range(0, 360), Rng.Range(-20, 20)), Cm(Q, H(Q) - 0.08 * Sc), FVector(Sc)),
						C * float(Rng.Range(0.9, 1.1)), 3.0f);
					Pl.Talus++;
				}
			}

		// A broken crown on each spire (S2), as wide as its top.
		if (Map.edges)
			for (size_t K = 0; K < Map.edges->islands.size(); K++)
			{
				if (Map.edges->islandKind(K) != int64_t(ac::IslandKind::spire)) continue;
				const std::vector<ac::Vec2>& Is = Map.edges->islands[K];
				ac::Vec2 Mid(0, 0);
				for (const ac::Vec2& Q : Is) Mid = Mid + Q;
				Mid = Mid * (1.0 / double(Is.size()));
				double Rad = 0;
				for (const ac::Vec2& Q : Is) Rad += ac::distance(Q, Mid);
				Rad /= double(Is.size());
				const int32 Which = Spire0 + Rng.Pick(4);
				const double Sc = AcSpace::ToCm(Rad * 1.5) / FMath::Max(Sizes[Which].X, 100.0);
				Add(Which, FTransform(FRotator(0, Rng.Range(0, 360), 0), Cm(Mid, H(Mid) - 0.6), FVector(Sc)), OwnRock, 3.0f);
				Pl.Crowns++;
			}

		// Round 4: clutter by palette, on open, gentle ground, off the pads,
		// deposits and wells.
		struct FKind { const TCHAR* Name; int32 Count; float Lo, Hi; float Slice; FLinearColor C; };
		TArray<FKind> Kinds;
		const FLinearColor Ground = Tinted(DirtC, 0.6f);
		switch (Layer)
		{
		case 4: Kinds = {{TEXT("crust"), 4, 0.7f, 1.4f, 4, FLinearColor(0.42f, 0.39f, 0.34f)}, {TEXT("pebble"), 6, 0.8f, 2.0f, 4, Ground * 0.8f}}; break;
		case 5: Kinds = {{TEXT("flake"), 4, 0.8f, 2.0f, 5, FLinearColor(0.05f, 0.05f, 0.05f)}, {TEXT("pebble"), 6, 0.8f, 2.0f, 5, FLinearColor(0.09f, 0.085f, 0.08f)}}; break;
		case 6: Kinds = {{TEXT("shard"), 4, 0.6f, 1.6f, 6, FLinearColor(0.02f, 0.035f, 0.045f)}, {TEXT("pebble"), 6, 0.8f, 2.0f, 6, FLinearColor(0.05f, 0.05f, 0.055f)}}; break;
		case 7: Kinds = {{TEXT("clinker"), 4, 0.7f, 1.6f, 7, FLinearColor(0.07f, 0.065f, 0.06f)}, {TEXT("pebble"), 6, 0.8f, 2.0f, 7, FLinearColor(0.08f, 0.07f, 0.06f)}}; break;
		case 8: Kinds = {{TEXT("rime"), 4, 0.6f, 1.4f, 8, FLinearColor(0.55f, 0.56f, 0.58f)}, {TEXT("pebble"), 6, 0.8f, 2.0f, 3, OwnRock}}; break;
		default: Kinds = {{TEXT("pebble"), 6, 0.8f, 2.5f, 0, Ground * 1.25f}, {TEXT("talus"), 6, 0.25f, 0.5f, 3, OwnRock}}; break;
		}
		const FLinearColor Tuft = Tinted(GrassC * 1.6f + FLinearColor(0.08f, 0.06f, 0.02f), 0.3f);
		const int32 Tuft0 = KitAt(TEXT("tuft"));
		TArray<ac::Vec2> Keep;
		for (const ac::Vec2& Q : Map.plates) Keep.Add(Q);
		for (const ac::OreSpec& O : Map.deposits) Keep.Add(O.position);
		for (const ac::WellSpec& W : Map.wells) Keep.Add(W.position);
		const auto Kept = [&Keep](const ac::Vec2 P) {
			for (const ac::Vec2& Q : Keep)
				if (ac::distance(P, Q) < ac::region::padReach + 1.5) return true;
			return false;
		};
		for (double Z = B.minZ + 0.5; Z < B.maxZ; Z += 1.0)
			for (double X = B.minX + 0.5; X < B.maxX; X += 1.0)
			{
				const int32 Here = Rng.Pick(3);
				for (int32 K = 0; K < Here; K++)
				{
					const ac::Vec2 P(X + Rng.Range(-0.5, 0.5), Z + Rng.Range(-0.5, 0.5));
					if (ac::length(Slope(P)) > 0.35 || OnWall(P) || Kept(P)) continue;
					const ac::TerrainField::Materials Mt = Field.materials(P);
					if (Mt.z > 0.2) continue;  // plating
					// Dry tufts where the ground is grassy, stones elsewhere.
					if (Layer == 0 && Mt.x > 0.35 && Rng.Unit() < 0.7)
					{
						const double Sc = Rng.Range(0.7, 1.4);
						Add(Tuft0 + Rng.Pick(4), FTransform(FRotator(Rng.Range(-8, 8), Rng.Range(0, 360), Rng.Range(-8, 8)), Cm(P, H(P) - 0.02), FVector(Sc)),
							Tuft * float(Rng.Range(0.8, 1.2)), 1.0f);
						Pl.Clutter++;
						continue;
					}
					const FKind& Kd = Kinds[Rng.Unit() < 0.6 ? 0 : 1];
					const double Sc = Rng.Range(Kd.Lo, Kd.Hi);
					Add(KitAt(Kd.Name) + Rng.Pick(Kd.Count),
						FTransform(FRotator(Rng.Range(-10, 10), Rng.Range(0, 360), Rng.Range(-10, 10)), Cm(P, H(P) - 0.015 * Sc), FVector(Sc)),
						Kd.C * float(Rng.Range(0.8, 1.2)), Kd.Slice);
					Pl.Clutter++;
				}
			}
		Pl.Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
		return Result;
	});
}

void AAcTerrainLabScenery::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Plan)
	{
		if (!Planning.IsValid() || !Planning.IsReady()) return;
		Plan = Planning.Get();
		Next = 0;
		Faces = Plan->Faces;
		Talus = Plan->Talus;
		Crowns = Plan->Crowns;
		Clutter = Plan->Clutter;
	}
	if (Next >= Plan->Items.Num()) return;
	const double Start = FPlatformTime::Seconds();
	const int32 End = FMath::Min(Next + PerFrame, Plan->Items.Num());
	TSet<UHierarchicalInstancedStaticMeshComponent*> Touched;
	for (; Next < End; ++Next)
	{
		const FItem& It = Plan->Items[Next];
		UHierarchicalInstancedStaticMeshComponent* C = Batch(It.Mesh);
		if (!C) continue;
		const int32 I = C->AddInstance(It.T, false);
		const float Data[4] = {It.C.R, It.C.G, It.C.B, It.Slice};
		C->SetCustomData(I, MakeArrayView(Data, 4), false);
		Touched.Add(C);
	}
	for (UHierarchicalInstancedStaticMeshComponent* C : Touched) C->MarkRenderStateDirty();
	if (Next >= Plan->Items.Num())
		UE_LOG(LogAutocraft, Log, TEXT("terrain lab scenery: %s: %d faces, %d talus, %d crowns, %d clutter; planned in %.1f ms on a worker, last step %.1f ms"),
			*SetKey, Faces, Talus, Crowns, Clutter, Plan->Ms, (FPlatformTime::Seconds() - Start) * 1000.0);
}

bool UAcTerrainLabSceneryWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UAcTerrainLabSceneryWorld::Tick(float)
{
	AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	const bool bOn = Terrain && AAcTerrain::LabOn();
	TSet<AAcRegionGround*> Live;
	if (bOn)
		Terrain->ForEachSet([&](AAcRegionGround* Set) {
			if (!Set->IsComplete()) return;
			Live.Add(Set);
			if (Laid.Contains(Set)) return;
			// A few frames under the lab's material first: the sky sets the
			// palette on a new instance on its next tick.
			if (++Waited.FindOrAdd(Set) < 8) return;
			AAcTerrainLabScenery* S = GetWorld()->SpawnActor<AAcTerrainLabScenery>();
			S->Build(Set);
			Laid.Add(Set, S);
		});
	for (auto It = Laid.CreateIterator(); It; ++It)
		if (!Live.Contains(It.Key().Get()))
		{
			if (It.Value()) It.Value()->Destroy();
			Waited.Remove(It.Key());
			It.RemoveCurrent();
		}
	if (!bOn) Waited.Reset();
}
