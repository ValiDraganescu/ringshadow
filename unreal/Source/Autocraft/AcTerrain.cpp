// Port of `TerrainBuilder` (Sources/Autocraft/Terrain.swift) and
// `GameScene.borderRocks` (GameScene.swift). See AcTerrain.h.
#include "AcTerrain.h"

#include "AcFind.h"

#include "AcBuiltMesh.h"
#include "AcLog.h"
#include "AcShot.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcDoodads.h"
#include "AcMmo.h"
#include "AcModelCatalog.h"
#include "AcWork.h"

#include "FreeView.h"
#include "Noise.h"
#include "WindowMaps.h"

#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Containers/Ticker.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/CommandLine.h"
#include "UObject/UObjectIterator.h"
#include "Misc/Parse.h"
#include "ShaderCompiler.h"
#include "StaticMeshAttributes.h"

#include <cmath>

namespace
{
	/// A height field over grid lines `Xs` × `Zs` (cells), as the Swift code
	/// holds it: heights row by row (j outer), with normals from central
	/// differences (one-sided at the ends).
	struct FGridLines
	{
		std::vector<double> Xs, Zs;
		std::vector<float> H;
		int64 Nx = 0, Nz = 0;

		float At(int64 I, int64 J) const
		{
			I = FMath::Clamp<int64>(I, 0, Nx - 1);
			J = FMath::Clamp<int64>(J, 0, Nz - 1);
			return H[size_t(J * Nx + I)];
		}

		/// Swift's normal `normalize(-dx, 1, -dz)` in SceneKit axes is
		/// `(-dh/dx, -dh/dy, 1)` in Unreal's (the height scale is the same
		/// in both: cells and cm alike).
		FVector3f Normal(int64 I, int64 J, bool bCentral) const
		{
			const int64 I0 = bCentral ? I - 1 : FMath::Max<int64>(I - 1, 0), I1 = bCentral ? I + 1 : FMath::Min<int64>(I + 1, Nx - 1);
			const int64 J0 = bCentral ? J - 1 : FMath::Max<int64>(J - 1, 0), J1 = bCentral ? J + 1 : FMath::Min<int64>(J + 1, Nz - 1);
			// The map's grid: central differences over 2·step with the
			// heights clamped at the edges (`H(i ± 1, j)` in Swift).
			const double Dx = bCentral ? Xs[1] - Xs[0] : 0, Dz = bCentral ? Zs[1] - Zs[0] : 0;
			const float SlopeX = bCentral ? float((At(I1, J) - At(I0, J)) / (2 * Dx))
			                              : float((At(I1, J) - At(I0, J)) / float(Xs[size_t(I1)] - Xs[size_t(I0)]));
			const float SlopeY = bCentral ? float((At(I, J1) - At(I, J0)) / (2 * Dz))
			                              : float((At(I, J1) - At(I, J0)) / float(Zs[size_t(J1)] - Zs[size_t(J0)]));
			return FVector3f(-SlopeX, -SlopeY, 1.0f).GetSafeNormal();
		}
	};

	/// A mesh over vertices [I0, I1] × [J0, J1] of `F`; `Skip(i, j)` leaves
	/// out the square whose low corner is (i, j). Null when nothing is left.
	TUniquePtr<FMeshDescription> MakeDescription(const FGridLines& F, int64 I0, int64 I1, int64 J0, int64 J1, bool bCentral,
	                                             bool bUVs, TFunctionRef<bool(int64, int64)> Skip)
	{
		const int64 W = I1 - I0 + 1, D = J1 - J0 + 1;
		int64 Quads = 0;
		for (int64 J = J0; J < J1; J++)
			for (int64 I = I0; I < I1; I++)
				if (!Skip(I, J)) Quads++;
		if (Quads == 0) return nullptr;

		TUniquePtr<FMeshDescription> MD = MakeUnique<FMeshDescription>();
		FStaticMeshAttributes Attr(*MD);
		Attr.Register();
		TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
		TVertexInstanceAttributesRef<FVector3f> Normals = Attr.GetVertexInstanceNormals();
		TVertexInstanceAttributesRef<FVector3f> Tangents = Attr.GetVertexInstanceTangents();
		TVertexInstanceAttributesRef<float> Signs = Attr.GetVertexInstanceBinormalSigns();
		TVertexInstanceAttributesRef<FVector2f> UVs = Attr.GetVertexInstanceUVs();
		UVs.SetNumChannels(1);
		MD->ReserveNewVertices(int32(W * D));
		MD->ReserveNewVertexInstances(int32(W * D));
		MD->ReserveNewTriangles(int32(Quads * 2));
		MD->ReserveNewPolygons(int32(Quads * 2));
		MD->ReserveNewEdges(int32(Quads * 3 + W + D));
		const FPolygonGroupID Group = MD->CreatePolygonGroup();
		Attr.GetPolygonGroupMaterialSlotNames()[Group] = FName("Ground");

		// One vertex instance per vertex: the ground is smooth everywhere.
		TArray<FVertexInstanceID> Ids;
		Ids.SetNumUninitialized(int32(W * D));
		for (int64 J = J0; J <= J1; J++)
		{
			for (int64 I = I0; I <= I1; I++)
			{
				const FVertexID V = MD->CreateVertex();
				Pos[V] = FVector3f(float(F.Xs[size_t(I)] * AcSpace::CmPerCell), float(F.Zs[size_t(J)] * AcSpace::CmPerCell),
				                   F.At(I, J) * float(AcSpace::CmPerCell));
				const FVertexInstanceID VI = MD->CreateVertexInstance(V);
				const FVector3f N = F.Normal(I, J, bCentral);
				// Tangent along +X (the textures' U), bitangent +Y (their V):
				// Cross(N, T) is +Y for flat ground, so the sign is +1.
				const FVector3f T = (FVector3f(1, 0, 0) - N * N.X).GetSafeNormal();
				Normals[VI] = N;
				Tangents[VI] = T;
				Signs[VI] = 1.0f;
				UVs.Set(VI, 0, bUVs ? FVector2f(float(double(I) / double(F.Nx - 1)), float(double(J) / double(F.Nz - 1)))
				                    : FVector2f::ZeroVector);
				Ids[int32((J - J0) * W + (I - I0))] = VI;
			}
		}
		// Swift's `[a, c, b1, b1, c, d]`: the diagonal runs from (i+1, j) to
		// (i, j+1) (`FAcHeightGrid::height` relies on it). SceneKit → Unreal
		// swaps two axes, and Unreal's other handedness swaps them back, so
		// the same order faces up.
		for (int64 J = J0; J < J1; J++)
		{
			for (int64 I = I0; I < I1; I++)
			{
				if (Skip(I, J)) continue;
				const FVertexInstanceID A = Ids[int32((J - J0) * W + (I - I0))], B1 = Ids[int32((J - J0) * W + (I + 1 - I0))];
				const FVertexInstanceID C = Ids[int32((J + 1 - J0) * W + (I - I0))], Dd = Ids[int32((J + 1 - J0) * W + (I + 1 - I0))];
				MD->CreateTriangle(Group, {A, C, B1});
				MD->CreateTriangle(Group, {B1, C, Dd});
			}
		}
		return MD;
	}

	UStaticMesh* MakeMesh(UObject* Outer, const FName Name, const FMeshDescription& MD, UMaterialInterface* Material)
	{
		UStaticMesh* Mesh = NewObject<UStaticMesh>(Outer, Name, RF_Transient);
		Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, FName("Ground")));
		Mesh->bSupportRayTracing = true;
		UStaticMesh::FBuildMeshDescriptionsParams Params;
		Params.bFastBuild = true;
		Params.bCommitMeshDescription = false;
		Params.bMarkPackageDirty = false;
		Params.bBuildSimpleCollision = false;
		Params.bAllowCpuAccess = false;
		Mesh->BuildFromMeshDescriptions({&MD}, Params);
		AcBuiltMeshReady(Mesh);
		return Mesh;
	}


	TAutoConsoleVariable<int32> CVarTerrainLab(
		TEXT("ac.TerrainLab"), 0,
		TEXT("1: the ground draws with the terrain lab's material (docs/terrain-lab.md); 0: the game's M_Terrain. ")
		TEXT("Also -AcTerrainLab on the command line."));

	TAutoConsoleVariable<int32> CVarBorderShadow(
		TEXT("ac.TerrainBorderShadow"), 0,
		TEXT("1: the border scenery casts shadows (Swift: never). Takes effect on the next build."));
}

double AAcTerrain::BorderWidth(const ac::GroundRect& B)
{
	return FMath::Max(60.0, 0.45 * FMath::Max(B.width(), B.depth()));
}

bool AAcTerrain::LabOn()
{
	// -AcTerrainLab: on from the start (the console may turn it off again).
	static const bool bFlag = [] {
		if (FParse::Param(FCommandLine::Get(), TEXT("AcTerrainLab"))) CVarTerrainLab->Set(1, ECVF_SetByCommandline);
		return true;
	}();
	(void)bFlag;
	return CVarTerrainLab.GetValueOnGameThread() != 0;
}

FString AAcTerrain::KeyOf(const ac::MapDefinition& Map)
{
	return FString::Printf(TEXT("%s|%lld|%llu"), UTF8_TO_TCHAR(Map.name.c_str()), (long long)Map.version, (unsigned long long)Map.seed);
}

// MARK: - A ground set

/// What a set's build makes off the game's thread: the field and the grid,
/// the splat's texels, every mesh's description, the border rocks and where
/// each doodad stands.
struct AAcRegionGround::FPrepared
{
	std::unique_ptr<ac::TerrainField> Field;
	FAcHeightGrid Grid;
	TArray<uint8> SplatBytes;
	int32 SplatW = 0, SplatH = 0;
	/// The terrain lab's relief bake (round 2), the splat's size: R the
	/// ground's occlusion by the land round it, G its curvature (0.5 flat,
	/// more a hollow, less a crest).
	TArray<uint8> ReliefBytes;
	struct FPiece
	{
		TUniquePtr<FMeshDescription> Mesh;
		FString Name;
		bool bShadow = true;
	};
	TArray<FPiece> Pieces;
	TArray<FAcBorderRock> Rocks;
	/// Each doodad and the field's height under it.
	TArray<TPair<ac::Doodad, double>> Doodads;
};

namespace
{
	/// The CPU part of a set's build (`AAcRegionGround::FPrepared`): the
	/// ground in 32-cell tiles, and the border scenery on the rim sides only
	/// (bits 1 north, 2 east, 4 south, 8 west), so it never covers a
	/// neighbour's ground.
	template <class FPrep>
	void PrepareGround(FPrep& P, const ac::MapDefinition& Map, const uint8 Rim, const bool bBorderShadow, const bool bAhead)
	{
		// Built ahead (W4): on one thread, never crowding the frame's own
		// work on the task graph.
		const EParallelForFlags Flags = bAhead ? EParallelForFlags::ForceSingleThread : EParallelForFlags::None;
		const auto Each = [Flags](const int32 N, TFunctionRef<void(int32)> Body) { ParallelFor(N, Body, Flags); };
		P.Field = std::make_unique<ac::TerrainField>(Map);
		const ac::TerrainField& Field = *P.Field;
		P.Grid = FAcHeightGrid(Field, AAcTerrain::Density, [&Each](int64_t N, const std::function<void(int64_t)>& Body) {
			Each(int32(N), [&Body](int32 J) { Body(J); });
		});
		const ac::GroundRect B = Map.bounds;

		// The splat map: RGBA, 4 texels per cell: grass, highland, plating,
		// scorch. Row 0 is minY. BGRA in memory.
		const double PerCell = AAcTerrain::SplatPerCell;
		P.SplatW = int32(B.width() * PerCell);
		P.SplatH = int32(B.depth() * PerCell);
		P.SplatBytes.SetNumZeroed(P.SplatW * P.SplatH * 4);
		const auto Byte = [](const double V) { return uint8(FMath::Clamp(V, 0.0, 1.0) * 255); };
		Each(P.SplatH, [&](const int32 Y) {
			const double Z = B.minZ + (double(Y) + 0.5) / PerCell;
			for (int32 X = 0; X < P.SplatW; X++)
			{
				const ac::TerrainField::Materials M = Field.materials(ac::Vec2(B.minX + (double(X) + 0.5) / PerCell, Z));
				uint8* Px = &P.SplatBytes[(Y * P.SplatW + X) * 4];
				Px[0] = Byte(M.z);
				Px[1] = Byte(M.y);
				Px[2] = Byte(M.x);
				Px[3] = Byte(M.w);
			}
		});

		// The terrain lab's relief (docs/terrain-lab.md round 2), at the
		// splat's texels from the drawn grid: the occlusion as a horizon
		// search in 8 directions out to 8 cells (cliff feet, gullies and
		// craters darken), the curvature as the ground a cell round against
		// the texel's own. M_Terrain does not read it.
		{
			const double BakeStart = FPlatformTime::Seconds();
			const FAcHeightGrid& G = P.Grid;
			P.ReliefBytes.SetNumZeroed(P.SplatW * P.SplatH * 4);
			const int32 Radii[] = {1, 2, 4, 8, 16, 32};
			const double Dirs[8][2] = {{1, 0}, {0.7071, 0.7071}, {0, 1}, {-0.7071, 0.7071}, {-1, 0}, {-0.7071, -0.7071}, {0, -1}, {0.7071, -0.7071}};
			Each(P.SplatH, [&](const int32 Y) {
				const double Z = B.minZ + (double(Y) + 0.5) / PerCell;
				const int64 J = FMath::RoundToInt64((Z - G.minZ) / G.step);
				for (int32 X = 0; X < P.SplatW; X++)
				{
					const double Xc = B.minX + (double(X) + 0.5) / PerCell;
					const int64 I = FMath::RoundToInt64((Xc - G.minX) / G.step);
					const double H0 = G.at(I, J);
					double Open = 0, Ring = 0;
					for (const auto& D : Dirs)
					{
						double Steep = 0;
						for (const int32 R : Radii)
						{
							const double Rise = G.at(I + FMath::RoundToInt64(D[0] * R), J + FMath::RoundToInt64(D[1] * R)) - H0;
							Steep = FMath::Max(Steep, Rise / (R * G.step));
							if (R == 4) Ring += Rise;
						}
						Open += 1.0 - Steep / FMath::Sqrt(1.0 + Steep * Steep);
					}
					uint8* Px = &P.ReliefBytes[(Y * P.SplatW + X) * 4];
					// BGRA in memory: R at 2, G at 1.
					Px[2] = Byte(Open / 8);
					Px[1] = Byte(0.5 + Ring / 8 * 0.5);
					Px[3] = 255;
				}
			});
			UE_LOG(LogAutocraft, Log, TEXT("ground: %s relief baked, %d x %d, %.0f ms"), UTF8_TO_TCHAR(Map.name.c_str()), P.SplatW, P.SplatH,
				(FPlatformTime::Seconds() - BakeStart) * 1000.0);
		}

		// The ground: tiles of TileCells × TileCells cells, sharing their
		// edge vertices (same heights and normals on both sides, no seam).
		{
			FGridLines F;
			F.Nx = P.Grid.nx;
			F.Nz = P.Grid.nz;
			F.Xs.resize(size_t(F.Nx));
			F.Zs.resize(size_t(F.Nz));
			for (int64 I = 0; I < F.Nx; I++) F.Xs[size_t(I)] = P.Grid.minX + double(I) * P.Grid.step;
			for (int64 J = 0; J < F.Nz; J++) F.Zs[size_t(J)] = P.Grid.minZ + double(J) * P.Grid.step;
			F.H = P.Grid.heights;
			const int64 Per = int64(AAcTerrain::TileCells) * AAcTerrain::Density;
			const int64 TilesX = FMath::DivideAndRoundUp<int64>(F.Nx - 1, Per), TilesZ = FMath::DivideAndRoundUp<int64>(F.Nz - 1, Per);
			TArray<TUniquePtr<FMeshDescription>> Descriptions;
			Descriptions.SetNum(int32(TilesX * TilesZ));
			Each(Descriptions.Num(), [&](const int32 K) {
				const int64 TX = K % TilesX, TZ = K / TilesX;
				const int64 I0 = TX * Per, J0 = TZ * Per;
				const int64 I1 = FMath::Min(I0 + Per, F.Nx - 1), J1 = FMath::Min(J0 + Per, F.Nz - 1);
				Descriptions[K] = MakeDescription(F, I0, I1, J0, J1, /*bCentral*/ true, /*bUVs*/ true, [](int64, int64) { return false; });
			});
			for (int32 K = 0; K < Descriptions.Num(); K++)
			{
				if (!Descriptions[K]) continue;
				P.Pieces.Add({MoveTemp(Descriptions[K]), FString::Printf(TEXT("Ground_%d_%d"), int32(K % TilesX), int32(K / TilesX)), true});
			}
		}

		// The border scenery past the map's edge, on the rim sides.
		if (Rim != 0)
		{
			const double Width = AAcTerrain::BorderWidth(B);
			// Grid lines along one axis: 0.5 cells near the edges, wider out.
			const auto Lines = [Width](const double Lo, const double Hi) {
				const auto Step = [](const double D) { return FMath::Min(0.5 + D * 0.12, 4.0); };
				std::vector<double> Out;
				double D = Width;
				while (D > 0.01)
				{
					Out.push_back(Lo - D);
					D -= Step(D);
				}
				for (double X = Lo; X < Lo + 2; X += 0.5) Out.push_back(X);
				for (double X = Lo + 2; X < Hi - 2; X += 1) Out.push_back(X);
				for (double X = Hi - 2; X <= Hi; X += 0.5) Out.push_back(X);
				D = 0.5;
				while (D <= Width + 1e-9)
				{
					Out.push_back(Hi + D);
					D += Step(D);
				}
				return Out;
			};
			FGridLines F;
			F.Xs = Lines(B.minX, B.maxX);
			F.Zs = Lines(B.minZ, B.maxZ);
			F.Nx = int64(F.Xs.size());
			F.Nz = int64(F.Zs.size());
			F.H.assign(size_t(F.Nx * F.Nz), 0.0f);
			Each(int32(F.Nz), [&](const int32 J) {
				for (int64 I = 0; I < F.Nx; I++)
				{
					const ac::Vec2 Q(F.Xs[size_t(I)], F.Zs[size_t(J)]);
					// Under the map's own mesh: a touch low, out of sight.
					const double Sink = Field.outside(Q) > 0 ? 0.0 : 0.08;
					F.H[size_t(J * F.Nx + I)] = float(Field.borderHeight(Q) - Sink);
				}
			});
			// Cells wholly a cell inside the map are the map mesh's: skipped.
			const ac::GroundRect Inner = B.inset(1);
			const auto Skip = [&](const int64 I, const int64 J) {
				return F.Xs[size_t(I)] >= Inner.minX && F.Xs[size_t(I + 1)] <= Inner.maxX && F.Zs[size_t(J)] >= Inner.minZ &&
				       F.Zs[size_t(J + 1)] <= Inner.maxZ;
			};
			const auto Index = [](const std::vector<double>& V, const double X) {
				return int64(std::lower_bound(V.begin(), V.end(), X) - V.begin());
			};
			const int64 IA = FMath::Clamp<int64>(Index(F.Xs, Inner.minX), 1, F.Nx - 1);
			const int64 IB = FMath::Clamp<int64>(Index(F.Xs, Inner.maxX), IA, F.Nx - 1);
			const int64 JA = FMath::Clamp<int64>(Index(F.Zs, Inner.minZ), 1, F.Nz - 1);
			const int64 JB = FMath::Clamp<int64>(Index(F.Zs, Inner.maxZ), JA, F.Nz - 1);
			// A north or south band reaches round a corner only where the
			// side beyond it is the rim too.
			const int64 I0 = (Rim & 8) ? 0 : IA, I1 = (Rim & 2) ? F.Nx - 1 : IB;
			struct FBand { uint8 Side; int64 I0, I1, J0, J1; const TCHAR* Name; };
			const FBand Bands[] = {
				{1, I0, I1, 0, JA, TEXT("Border_N")},
				{4, I0, I1, JB, F.Nz - 1, TEXT("Border_S")},
				{8, 0, IA, JA, JB, TEXT("Border_W")},
				{2, IB, F.Nx - 1, JA, JB, TEXT("Border_E")},
			};
			for (const FBand& Band : Bands)
			{
				if (!(Rim & Band.Side) || Band.I1 <= Band.I0 || Band.J1 <= Band.J0) continue;
				TUniquePtr<FMeshDescription> MD = MakeDescription(F, Band.I0, Band.I1, Band.J0, Band.J1, /*bCentral*/ false, /*bUVs*/ false, Skip);
				if (MD) P.Pieces.Add({MoveTemp(MD), Band.Name, bBorderShadow});
			}

			// `GameScene.borderRocks`: boulders and spires strewn over the
			// border scenery, thickest near the map's edge (rim sides only).
			const double Reach = 28.0;
			ac::SeededRandom Rng(Map.seed + 0xB0D3);
			using Kind = ac::Doodad::Kind;
			const Kind Kinds[] = {Kind::rock, Kind::rock, Kind::boulder, Kind::rockSpire};
			const int64 Count = int64((B.width() + B.depth()) * 2 * Reach / 110);
			int64 Placed = 0;
			for (int64 K = 0; K < Count * 20 && Placed < Count; K++)
			{
				// Swift evaluates the two coordinates left to right.
				const double X = Rng.range(B.minX - Reach, B.maxX + Reach);
				const double Y = Rng.range(B.minZ - Reach, B.maxZ + Reach);
				const ac::Vec2 Q(X, Y);
				const double D = Field.outside(Q);
				if (!(D > 1.5) || !(D < Reach) || !(Rng.unit() < 1 - 0.6 * D / Reach)) continue;
				const Kind Which = Kinds[FMath::Min(int32(Rng.unit() * 4), 3)];
				const double Scale = Rng.range(0.7, 1.5) * (1 + 0.5 * D / Reach);
				const double Turn = Rng.range(0, 2 * ac::pi);
				// Past a side with a region beyond: that region's ground.
				const bool bOn = (Q.y >= B.minZ || (Rim & 1)) && (Q.x <= B.maxX || (Rim & 2)) && (Q.y <= B.maxZ || (Rim & 4)) &&
				                 (Q.x >= B.minX || (Rim & 8));
				if (!bOn) continue;
				FAcBorderRock Rock;
				Rock.Kind = Which;
				Rock.Variant = uint32(K);
				// SceneKit `eulerAngles.y = a` is Unreal yaw -a (AcSpace.h).
				// In the set's own frame (the actor stands at its place in the
				// world): not `ToWorld`, whose origin moves with the pilot.
				Rock.Transform = FTransform(FRotator(0.0, -FMath::RadiansToDegrees(Turn), 0.0),
				                            FVector(AcSpace::ToCm(Q.x), AcSpace::ToCm(Q.y), AcSpace::ToCm(Field.borderHeight(Q) - 0.15 * Scale)),
				                            FVector(Scale));
				P.Rocks.Add(Rock);
				Placed++;
			}
		}

		// Where each doodad stands.
		P.Doodads.Reserve(int32(Map.doodads.size()));
		for (const ac::Doodad& D : Map.doodads) P.Doodads.Add({D, Field.height(D.position)});
	}

	/// Doodads placed a step at a time on a set built ahead.
	constexpr int32 DoodadsAStep = 120;
}

AAcRegionGround::AAcRegionGround()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

void AAcRegionGround::Begin(std::shared_ptr<const ac::MapDefinition> InMap, const FString& Key, const ac::Vec2 Centre, const uint8 Rim,
	const int64 Region, UMaterialInterface* Base)
{
	MapDef = MoveTemp(InMap);
	MapKey = Key;
	GridCentre = Centre;
	RimSides = Rim;
	RegionIndex = Region;
	BaseMaterial = Base;
}

void AAcRegionGround::PrepareCpu()
{
	if (Prepared) return;
	Prepared = MakeShared<FPrepared>();
	const bool bShadow = CVarBorderShadow.GetValueOnGameThread() != 0;
	const bool bAhead = !bNow;
	Preparing = AcWork::Run([P = Prepared, Map = MapDef, Rim = RimSides, bShadow, bAhead]() { PrepareGround(*P, *Map, Rim, bShadow, bAhead); });
}

void AAcRegionGround::BuildNow()
{
	bNow = true;
	PrepareCpu();
	Step(TNumericLimits<double>::Max());
}

void AAcRegionGround::MakeMaterial()
{
	FieldPtr = MoveTemp(Prepared->Field);
	HeightGrid = MoveTemp(Prepared->Grid);
	BorderRocks = Prepared->Rocks;
	Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("TerrainMaterial"));
	// A name of its own: a set's splat never waits on another's being let go.
	SplatTex = UTexture2D::CreateTransient(Prepared->SplatW, Prepared->SplatH, PF_B8G8R8A8,
		MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("TerrainSplat")));
	SplatTex->SRGB = false;
	SplatTex->Filter = TF_Bilinear;
	SplatTex->AddressX = TA_Clamp;
	SplatTex->AddressY = TA_Clamp;
	SplatTex->CompressionSettings = TC_VectorDisplacementmap;
	SplatTex->NeverStream = true;
	FTexture2DMipMap& Mip = SplatTex->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Prepared->SplatBytes.GetData(), Prepared->SplatBytes.Num());
	Mip.BulkData.Unlock();
	SplatTex->UpdateResource();
	Prepared->SplatBytes.Empty();
	if (Prepared->ReliefBytes.Num() == Prepared->SplatW * Prepared->SplatH * 4)
	{
		ReliefTex = UTexture2D::CreateTransient(Prepared->SplatW, Prepared->SplatH, PF_B8G8R8A8,
			MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), TEXT("TerrainRelief")));
		ReliefTex->SRGB = false;
		ReliefTex->Filter = TF_Bilinear;
		ReliefTex->AddressX = TA_Clamp;
		ReliefTex->AddressY = TA_Clamp;
		ReliefTex->CompressionSettings = TC_VectorDisplacementmap;
		ReliefTex->NeverStream = true;
		FTexture2DMipMap& ReliefMip = ReliefTex->GetPlatformData()->Mips[0];
		FMemory::Memcpy(ReliefMip.BulkData.Lock(LOCK_READ_WRITE), Prepared->ReliefBytes.GetData(), Prepared->ReliefBytes.Num());
		ReliefMip.BulkData.Unlock();
		ReliefTex->UpdateResource();
	}
	Prepared->ReliefBytes.Empty();
	SetParameters();
	bMaterial = true;
}

void AAcRegionGround::SetParameters()
{
	if (!Mat) return;
	const ac::GroundRect B = MapDef->bounds;
	Mat->SetTextureParameterValue(TEXT("Splat"), SplatTex);
	// The lab's relief bake (M_Terrain has no such parameter).
	if (ReliefTex) Mat->SetTextureParameterValue(TEXT("Relief"), ReliefTex);
	Mat->SetVectorParameterValue(TEXT("Origin"), FLinearColor(float(GridOrigin.x), float(GridOrigin.y), 0.f, 0.f));
	// In world-grid cells (the material samples by them: `Origin`).
	Mat->SetVectorParameterValue(TEXT("Bounds"), FLinearColor(float(B.minX + GridCentre.x), float(B.minZ + GridCentre.y), float(B.width()), float(B.depth())));
	const FLinearColor H = AAcTerrain::Haze();
	Mat->SetVectorParameterValue(TEXT("Haze"), FLinearColor(H.R, H.G, H.B, float(AAcTerrain::BorderWidth(B))));
	// Which sides have a region beyond (N, E, S, W), until the palette's
	// own sets it (AcGroundLook): the dark seam only at the world's rim.
	Mat->SetVectorParameterValue(TEXT("Neighbours"), FLinearColor((RimSides & 1) ? 0.f : 1.f, (RimSides & 2) ? 0.f : 1.f,
		(RimSides & 4) ? 0.f : 1.f, (RimSides & 8) ? 0.f : 1.f));
	// The walls' tops (W4b): one rock for the whole world above this
	// height, not the region's palette, so the high ground between the
	// regions reads as one land and no cell shows by its colour.
	if (MapDef->edges && !MapDef->edges->outline.empty())
		Mat->SetScalarParameterValue(TEXT("WallZ"), float(double(MapDef->edges->level) * MapDef->levelHeight - 0.5));
}

void AAcRegionGround::Rematerial(UMaterialInterface* Base)
{
	if (!bMaterial || !Base || BaseMaterial == Base) return;
	BaseMaterial = Base;
	Mat = UMaterialInstanceDynamic::Create(BaseMaterial, this, MakeUniqueObjectName(this, UMaterialInstanceDynamic::StaticClass(), TEXT("TerrainMaterial")));
	SetParameters();
	for (UStaticMeshComponent* C : Pieces)
		if (C) C->SetMaterial(0, Mat);
}

void AAcRegionGround::SetOrigin(const ac::Vec2 Origin)
{
	GridOrigin = Origin;
	if (Mat) Mat->SetVectorParameterValue(TEXT("Origin"), FLinearColor(float(Origin.x), float(Origin.y), 0.f, 0.f));
}

void AAcRegionGround::AddPiece(UStaticMesh* Mesh, const TCHAR* Name, const bool bCastShadow)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, FName(Name));
	C->SetMobility(EComponentMobility::Static);
	C->SetStaticMesh(Mesh);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetCastShadow(bCastShadow);
	C->bUseAsOccluder = true;
	C->SetupAttachment(Root);
	C->RegisterComponent();
	AddInstanceComponent(C);
	Pieces.Add(C);
}

bool AAcRegionGround::Step(const double BudgetMs)
{
	if (bComplete) return true;
	PrepareCpu();
	const double Start = FPlatformTime::Seconds();
	const auto Left = [&] { return (FPlatformTime::Seconds() - Start) * 1000.0 < BudgetMs; };
	// Built at once: wait for the worker; ahead: come back next frame.
	if (!Preparing.IsReady())
	{
		if (BudgetMs < TNumericLimits<double>::Max()) return false;
		Preparing.Wait();
	}
	do
	{
		if (!bMaterial)
		{
			MakeMaterial();
			continue;
		}
		if (NextPiece < Prepared->Pieces.Num())
		{
			FPrepared::FPiece& P = Prepared->Pieces[NextPiece++];
			UStaticMesh* Mesh = MakeMesh(this, FName(*(TEXT("SM_") + P.Name)), *P.Mesh, Mat);
			AddPiece(Mesh, *P.Name, P.bShadow);
			P.Mesh.Reset();
			continue;
		}
		const FVector At = GetActorLocation();
		if (NextDoodad == 0)
		{
			FAcSceneryOptions MapOptions;
			DoodadBatches.Init(this, Root, MapOptions, TEXT("Doodad"));
			FAcSceneryOptions RockOptions;
			RockOptions.bCastShadow = false;  // GameScene.borderRocks: castsShadow = false
			RockBatches.Init(this, Root, RockOptions, TEXT("BorderRock"));
		}
		if (NextDoodad < Prepared->Doodads.Num())
		{
			const int32 End = FMath::Min(NextDoodad + (BudgetMs < TNumericLimits<double>::Max() ? DoodadsAStep : Prepared->Doodads.Num()), Prepared->Doodads.Num());
			TArray<FAcSceneryModel> Chunk;
			for (; NextDoodad < End; ++NextDoodad)
			{
				const TPair<ac::Doodad, double>& D = Prepared->Doodads[NextDoodad];
				const FAcModelInfo* M = AAcDoodads::ModelFor(D.Key.kind, D.Key.variant);
				if (!M) continue;
				// `Placement` is in the world frame of now (`ToWorld`); the set's
				// own frame takes the origin back out, as a set built after the
				// pilot left the session's first region stands off it.
				FTransform T = AAcDoodads::Placement(D.Key, D.Value);
				T.AddToTranslation(At - AcSpace::OriginCm());
				if (!FirstOfKind.Contains(uint8(D.Key.kind))) FirstOfKind.Add(uint8(D.Key.kind), T.GetTranslation());
				Chunk.Add(DoodadBatches.AddModel(*M, T));
				DoodadAt.Add(D.Key.position);
			}
			TArray<FAcSceneryModel*> Ptrs;
			for (FAcSceneryModel& M : Chunk) Ptrs.Add(&M);
			DoodadBatches.Flush(Ptrs);
			DoodadModels.Append(MoveTemp(Chunk));
			if (NextDoodad < Prepared->Doodads.Num()) continue;
		}
		if (NextDoodad >= Prepared->Doodads.Num())
		{
			TArray<FAcSceneryModel> Rocks;
			for (const FAcBorderRock& R : BorderRocks)
			{
				const FAcModelInfo* M = AAcDoodads::ModelFor(R.Kind, R.Variant);
				if (!M) continue;
				FTransform T = R.Transform;
				T.AddToTranslation(At);
				Rocks.Add(RockBatches.AddModel(*M, T));
			}
			TArray<FAcSceneryModel*> Ptrs;
			for (FAcSceneryModel& M : Rocks) Ptrs.Add(&M);
			RockBatches.Flush(Ptrs);
			Prepared.Reset();
			bComplete = true;
		}
	} while (!bComplete && Left());
	Spent += FPlatformTime::Seconds() - Start;
	if (bComplete)
	{
		UE_LOG(LogAutocraft, Log, TEXT("ground: %s ready, %d pieces, %d doodads, %d border rocks, %.0f ms on the game's thread"),
			UTF8_TO_TCHAR(MapDef->name.c_str()), Pieces.Num(), DoodadBatches.NumInstances(), BorderRocks.Num(), Spent * 1000.0);
	}
	return bComplete;
}

// MARK: - The terrain: the sets

AAcTerrain::AAcTerrain()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
	TerrainMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Terrain/M_Terrain.M_Terrain")));
	LabMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/TerrainLab/M_TerrainLab.M_TerrainLab")));
}

UMaterialInterface* AAcTerrain::BaseMaterial()
{
	if (LabOn())
	{
		if (UMaterialInterface* Lab = LabMaterial.LoadSynchronous()) return Lab;
		static bool bSaid = false;
		if (!bSaid) UE_LOG(LogAutocraft, Error, TEXT("terrain lab: %s is missing (run Tools/Editor/make_terrain_lab_material.py); the game's ground instead"), *LabMaterial.ToString());
		bSaid = true;
	}
	UMaterialInterface* Base = TerrainMaterial.LoadSynchronous();
	if (!Base)
	{
		UE_LOG(LogAutocraft, Error, TEXT("terrain: %s is missing (run Tools/Editor/make_terrain_material.py)"), *TerrainMaterial.ToString());
	}
	return Base;
}

void AAcTerrain::SyncLab()
{
	const int32 On = LabOn() ? 1 : 0;
	if (On == LabDrawn) return;
	const bool bFirst = LabDrawn < 0;
	LabDrawn = On;
	if (bFirst && !On) return;
	UMaterialInterface* Base = BaseMaterial();
	for (TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets)
		if (S.Value) S.Value->Rematerial(Base);
	// The sky and the palette set theirs on the new instances (AAcDaylight).
	Version += 1;
	UE_LOG(LogAutocraft, Log, TEXT("terrain lab: %s, %d sets"), On ? TEXT("on") : TEXT("off"), Sets.Num());
}

AAcTerrain* AAcTerrain::Find(const UWorld* World)
{
	return AcFindActor<AAcTerrain>(World);
}

AAcTerrain* AAcTerrain::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	AAcTerrain* Terrain = Find(World);
	if (!Terrain)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("AcTerrain");
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Terrain = World->SpawnActor<AAcTerrain>(AAcTerrain::StaticClass(), FTransform::Identity, Params);
	}
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World))
	{
		if (!Terrain->GameStartedHandle.IsValid())
		{
			Terrain->GameStartedHandle = Sim->OnGameStarted.AddUObject(Terrain, &AAcTerrain::OnGameStarted);
		}
		if (Sim->IsRunning()) Terrain->OnGameStarted(*Sim);
	}
	Terrain->StageViewIfAsked();
	return Terrain;
}

void AAcTerrain::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && GameStartedHandle.IsValid())
	{
		Sim->OnGameStarted.Remove(GameStartedHandle);
	}
	GameStartedHandle.Reset();
	Super::EndPlay(Reason);
}

double AAcTerrain::GroundZ(const FVector& World) const
{
	return !Current || Current->Grid().empty() ? 0.0 : AcSpace::ToCm(Current->Grid().height(AcSpace::ToSim(World)));
}

void AAcTerrain::OnGameStarted(UAcSimSubsystem& Sim)
{
	Build(Sim.Map());
}

AAcRegionGround* AAcTerrain::Spawn(std::shared_ptr<const ac::MapDefinition> Map, const FString& Key, const ac::Vec2 Centre, const uint8 Rim,
	const int64 Region)
{
	UMaterialInterface* Base = BaseMaterial();
	// At its place in the world frame (AcSpace.h `Origin`): its centre on
	// the grid less the session's first region's. It never moves.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const ac::Vec2 At = Centre - SessionCentre();
	AAcRegionGround* Set = GetWorld()->SpawnActor<AAcRegionGround>(AAcRegionGround::StaticClass(),
		FTransform(FVector(AcSpace::ToCm(At.x), AcSpace::ToCm(At.y), 0.0)), Params);
	Set->Begin(MoveTemp(Map), Key, Centre, Rim, Region, Base);
	Set->SetOrigin(SessionCentre());
	Sets.Add(Key, Set);
	return Set;
}

void AAcTerrain::Build(const ac::MapDefinition& InMap)
{
	const FString Key = KeyOf(InMap);
	if (Current && Current->Key() == Key && Current->Map() == InMap) return;
	const double Start = FPlatformTime::Seconds();
	// Where it lies in a world, and its rim (out of one: the origin, all rim).
	ac::Vec2 Centre(0, 0);
	uint8 Rim = 0xF;
	int64 Region = INDEX_NONE;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (const TOptional<FAcRegionAround> Here = Mmo ? Mmo->HereGround() : TOptional<FAcRegionAround>(); Here && Mmo->Mirror())
	{
		Centre = Here->Centre;
		Rim = Here->Rim;
		Region = Here->Index;
	}
	TObjectPtr<AAcRegionGround>* Found = Sets.Find(Key);
	AAcRegionGround* Set = Found ? Found->Get() : nullptr;
	const bool bAhead = Set && Set->IsComplete();
	// (A set made ahead knows its own place: the World message that says the
	// pilot's region changed may come a frame after the region.)
	if (Set && !(Set->Map() == InMap))
	{
		Sets.Remove(Key);
		Set->Destroy();
		Set = nullptr;
	}
	if (!Set)
	{
		// Out of a world, or another world: only this map's ground.
		if (!Mmo)
		{
			for (TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets)
				if (S.Value) S.Value->Destroy();
			Sets.Reset();
			Current = nullptr;
		}
		Set = Spawn(std::make_shared<const ac::MapDefinition>(InMap), Key, Centre, Rim, Region);
	}
	if (!Set->IsComplete()) Set->BuildNow();
	MakeCurrent(Set);
	UE_LOG(LogAutocraft, Log, TEXT("terrain: %s, %lld x %lld vertices, %d border rocks, %s in %.0f ms"), UTF8_TO_TCHAR(InMap.name.c_str()),
		(long long)Set->Grid().nx, (long long)Set->Grid().nz, Set->Rocks().Num(), bAhead ? TEXT("built ahead, taken") : TEXT("built"),
		(FPlatformTime::Seconds() - Start) * 1000.0);
}

void AAcTerrain::MakeCurrent(AAcRegionGround* Set)
{
	Current = Set;
	Version += 1;
}

ac::Vec2 AAcTerrain::SessionCentre() const
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	return Mmo ? Mmo->SessionCentre() : ac::Vec2(0, 0);
}

void AAcTerrain::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SyncLab();
	SyncAround();
}

void AAcTerrain::SyncAround()
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo || !Current) return;
	// C1: the doodads a complete Citadel's grown square takes, hidden (the
	// server keeps the same rule; docs/citadel-interior.md §9).
	if (Current->IsComplete() && Sim->IsRunning())
	{
		const ac::Simulation& S = Sim->Simulation();
		uint32 Key = 1;
		for (const ac::Structure& B : S.state.structures)
		{
			if (B.kind != ac::StructureKind::citadel || !B.complete()) continue;
			Key = HashCombine(Key, HashCombine(GetTypeHash(B.id), HashCombine(GetTypeHash(B.position.x), GetTypeHash(B.position.y))));
		}
		Current->Swallow(Key, [&S](const ac::Vec2 P) { return S.swallowedByCitadel(P); });
	}
	TSet<FString> Keep;
	Keep.Add(Current->Key());
	for (const FAcRegionAround& A : Mmo->Around())
	{
		if (!A.Map) continue;
		const FString Key = KeyOf(*A.Map);
		Keep.Add(Key);
		if (Sets.Contains(Key)) continue;
		Spawn(A.Map, Key, A.Centre, A.Rim, A.Index);
		Version += 1;
	}
	// The far ones let go (a flight away, or two regions on).
	for (auto It = Sets.CreateIterator(); It; ++It)
	{
		if (Keep.Contains(It.Key())) continue;
		if (It.Value()) It.Value()->Destroy();
		It.RemoveCurrent();
		Version += 1;
	}
	// One set a step a frame, a few milliseconds of it, a while after the
	// region changed (the frames round a crossing stay clear); at the
	// session's start from the first frame (Q2). One at a time all the same:
	// eight sets drawn at once held frames for seconds.
	if (Mmo->SinceRegionChange() < 2.0 && !Mmo->SessionStart()) return;
	// Aboard the Dropship only the target's set builds (Q2): a set filled in
	// under the ship's high view cost a 0.5 to 0.8 s frame each (its
	// instances added with the whole region in sight), where the same set
	// filled in from the ground cost nothing; the rest wait for the landing.
	const ac::net::WorldMessage* W = Mmo->World();
	const TOptional<int64> Only = Mmo->Travelling() ? TOptional<int64>(W && W->flyingTo ? *W->flyingTo : INDEX_NONE) : TOptional<int64>();
	for (TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets)
	{
		if (!S.Value || S.Value->IsComplete() || (Only && S.Value->Region() != *Only)) continue;
		S.Value->Step(4.0);
		break;
	}
}

int32 AAcRegionGround::Swallow(const uint32 Key, TFunctionRef<bool(ac::Vec2)> Swallowed)
{
	if (Key == SwallowKey && SwallowSeen == DoodadModels.Num())
	{
		int32 N = 0;
		for (const bool B : DoodadSwallowed) N += B ? 1 : 0;
		return N;
	}
	SwallowKey = Key;
	SwallowSeen = DoodadModels.Num();
	DoodadSwallowed.SetNum(DoodadModels.Num());
	int32 Hidden = 0;
	for (int32 I = 0; I < DoodadModels.Num() && I < DoodadAt.Num(); ++I)
	{
		const bool bGone = Swallowed(DoodadAt[I]);
		Hidden += bGone ? 1 : 0;
		if (bGone == DoodadSwallowed[I]) continue;
		DoodadSwallowed[I] = bGone;
		for (int32 Part = 0; Part < DoodadModels[I].Parts.Num(); ++Part) DoodadBatches.SetVisible(DoodadModels[I], Part, !bGone);
	}
	DoodadBatches.Commit();
	if (Hidden > 0) UE_LOG(LogAutocraft, Log, TEXT("terrain: %d doodads under a Citadel hidden in %s"), Hidden, *MapKey);
	return Hidden;
}

void AAcTerrain::ForEachMaterial(TFunctionRef<void(UMaterialInstanceDynamic*, int64 Region)> Each) const
{
	if (Current && Current->Material()) Each(Current->Material(), Current->Region());
	for (const TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets)
		if (S.Value && S.Value != Current && S.Value->Material()) Each(S.Value->Material(), S.Value->Region());
}

int32 AAcTerrain::NumSets() const
{
	return Sets.Num();
}

TOptional<FVector> AAcTerrain::DrawnIn(const int64 Region, const ac::Doodad::Kind Kind) const
{
	for (const TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets)
		if (S.Value && S.Value->Region() == Region) return S.Value->DrawnAt(Kind);
	return TOptional<FVector>();
}

int32 AAcTerrain::NumSetsComplete() const
{
	int32 N = 0;
	for (const TPair<FString, TObjectPtr<AAcRegionGround>>& S : Sets) N += S.Value && S.Value->IsComplete() ? 1 : 0;
	return N;
}

// -AcTerrainView: the Swift game's default top-down framing and a stand-in
// sun, for terrain renders before the camera (A3) and daylight (B9) exist.
namespace
{
	/// A stand-in noon sun and sky, when nothing else (B9's AAcDaylight)
	/// lit the level by the first frame.
	void StandInSun(UWorld* World)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		bool bSun = false;
		for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered()) bSun = true;
		}
		if (!bSun)
		{
			// Noon (`Daylight.sun(hour: 12)`), toward the sun in SceneKit axes.
			const FVector Toward = AcSpace::AxesFromSceneKit(-60, 110, -55).GetSafeNormal();
			ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector::ZeroVector, (-Toward).Rotation(), Params);
			UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(Sun->GetLightComponent());
			L->SetMobility(EComponentMobility::Movable);
			L->SetIntensity(5.0f);
			L->SetLightColor(FLinearColor(1.0f, 0.93f, 0.82f));
			L->SetAtmosphereSunLight(true);
			ASkyAtmosphere* Atmosphere = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			(void)Atmosphere;
			ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
			USkyLightComponent* S = Sky->GetLightComponent();
			S->SetMobility(EComponentMobility::Movable);
			S->bRealTimeCapture = true;
			S->SetIntensity(1.0f);
			S->RecaptureSky();
		}

	}

	void StageTerrainView(UWorld* World, AAcTerrain* Terrain, const ac::MapDefinition& Map)
	{
		const TCHAR* Cmd = FCommandLine::Get();
		int32 W = 1600, H = 1000;
		FParse::Value(Cmd, TEXT("ResX="), W);
		FParse::Value(Cmd, TEXT("ResY="), H);
		double Zoom = 0, Cover = 0;
		FParse::Value(Cmd, TEXT("AcTerrainView="), Zoom);
		FParse::Value(Cmd, TEXT("AcTerrainCover="), Cover);

		// Over blue's main base (`GameController.swift:167`).
		const ac::Vec2 Main = Map.starts.empty() ? Map.front() : Map.bases[size_t(Map.starts[0])].center;
		ac::FreeView View(Map.bounds, Main);
		View.resize(ac::Vec2(W, H), Cover);
		if (Zoom > 0)
		{
			View.pointsPerCell = FMath::Clamp(Zoom, View.minPointsPerCell(), ac::FreeView::maxPointsPerCell);
			View.clamp();
		}
		// -AcTerrainAt=X,Y: centre there (as `windowshot --at`).
		FString At;
		if (FParse::Value(Cmd, TEXT("AcTerrainAt="), At, /*bShouldStopOnSeparator*/ false))
		{
			FString Xs, Ys;
			if (At.Split(TEXT(","), &Xs, &Ys)) View.center(ac::Vec2(FCString::Atod(*Xs), FCString::Atod(*Ys)));
		}
		const ac::Vec3 Eye = View.cameraPosition();
		const double Pitch = View.projection().pitchDegrees, Fov = View.projection().fovDegrees;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Camera = World->SpawnActor<ACameraActor>(
			AcSpace::FromSceneKit(Eye.x, Eye.y, Eye.z), FRotator(-Pitch, -90.0, 0.0), Params);
		UCameraComponent* Cam = Camera->GetCameraComponent();
		Cam->SetConstraintAspectRatio(false);
		// The aspect too: with MaintainYFOV Unreal derives the vertical FOV
		// from the horizontal one through the camera's own aspect ratio.
		Cam->SetAspectRatio(float(W) / float(H));
		// Unreal's FOV is horizontal: the same vertical 30°.
		Cam->SetFieldOfView(float(FMath::RadiansToDegrees(2 * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Fov / 2)) * double(W) / double(H)))));
		Cam->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Cam->PostProcessSettings.AutoExposureMethod = AEM_Manual;
		Cam->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
		Cam->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
		Cam->PostProcessSettings.bOverride_AutoExposureBias = true;
		Cam->PostProcessSettings.AutoExposureBias = 0.0f;
		UE_LOG(LogAutocraft, Log, TEXT("terrain view: target %.1f,%.1f at %.1f points per cell, eye %s"),
		       View.target.x, View.target.y, View.pointsPerCell, *Camera->GetActorLocation().ToString());

		// Into the view at once and again once play is going (the camera
		// manager may still pick the controller's own view first).
		const TWeakObjectPtr<ACameraActor> WeakCamera(Camera);
		const TWeakObjectPtr<UWorld> WeakWorld(World);
		UAcShotSubsystem* Shot = UAcShotSubsystem::Get(World);
		if (Shot) Shot->Hold();
		const TWeakObjectPtr<UAcShotSubsystem> WeakShot(Shot);
		int32 Frames = 0;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCamera, WeakWorld, WeakShot, Frames](float) mutable {
			if (!WeakWorld.IsValid() || !WeakCamera.IsValid()) return false;
			if (APlayerController* PC = WeakWorld->GetFirstPlayerController())
			{
				if (PC->GetViewTarget() != WeakCamera.Get()) PC->SetViewTarget(WeakCamera.Get());
			}
			if (++Frames == 2) StandInSun(WeakWorld.Get());
			// Hold the shot until the shaders are compiled and the ground
			// textures are in.
			const bool bCompiling = GShaderCompilingManager && GShaderCompilingManager->GetNumRemainingJobs() > 0;
			if (bCompiling || Frames < 20)
			{
				return true;
			}
			IStreamingManager::Get().StreamAllResources(1.0f);
			if (WeakShot.IsValid()) WeakShot->Release();
			return false;
		}));
	}
}

void AAcTerrain::StageViewIfAsked()
{
	double Zoom = 0;
	const TCHAR* Cmd = FCommandLine::Get();
	if (bViewStaged || !IsBuilt()) return;
	if (!FParse::Param(Cmd, TEXT("AcTerrainView")) && !FParse::Value(Cmd, TEXT("AcTerrainView="), Zoom)) return;
	bViewStaged = true;
	StageTerrainView(GetWorld(), this, Current->Map());
}
