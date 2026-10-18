#include "AcInstancedMesh.h"

#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"

namespace
{
	TAutoConsoleVariable<int32> CVarCheapBounds(TEXT("ac.RenderCheapBounds"), 1,
		TEXT("Instanced meshes that move every frame take cheap bounds (UAcInstancedMesh): 1 on, 0 the engine's walk (to compare)."));
	TAutoConsoleVariable<int32> CVarMaterialDescFrames(TEXT("ac.RenderMaterialDescFrames"), 64,
		TEXT("Instanced meshes keep their materials' property descriptor for this many instance updates (UAcInstancedMesh); 0: the engine's, every update (to compare)."));
}

void UAcInstancedMesh::SetBox(const FBox& WorldBox)
{
	bTold = WorldBox.IsValid != 0;
	const FTransform& T = GetComponentTransform();
	Box = !bTold ? FBox(ForceInit) : T.Equals(FTransform::Identity) ? WorldBox : WorldBox.TransformBy(T.Inverse());
}

FBoxSphereBounds UAcInstancedMesh::CalcBounds(const FTransform& BoundTransform) const
{
	if (CVarCheapBounds.GetValueOnAnyThread() == 0) return Super::CalcBounds(BoundTransform);
	FBox Local = Box;
	if (!bTold)
	{
		const UStaticMesh* Mesh = GetStaticMesh();
		if (!Mesh || PerInstanceSMData.Num() == 0) return Super::CalcBounds(BoundTransform);
		{
			const FBoxSphereBounds MB = Mesh->GetBounds();
			const double Reach = MB.Origin.Size() + MB.SphereRadius;
			FVector Lo(UE_BIG_NUMBER), Hi(-UE_BIG_NUMBER);
			double Scale2 = 0;
			for (const FInstancedStaticMeshInstanceData& I : PerInstanceSMData)
			{
				const FMatrix& M = I.Transform;
				const double S2 = FMath::Max3(FVector(M.M[0][0], M.M[0][1], M.M[0][2]).SizeSquared(),
					FVector(M.M[1][0], M.M[1][1], M.M[1][2]).SizeSquared(), FVector(M.M[2][0], M.M[2][1], M.M[2][2]).SizeSquared());
				if (S2 <= 1e-8) continue;
				const FVector P(M.M[3][0], M.M[3][1], M.M[3][2]);
				Lo = Lo.ComponentMin(P);
				Hi = Hi.ComponentMax(P);
				Scale2 = FMath::Max(Scale2, S2);
			}
			Local = Scale2 > 0 ? FBox(Lo, Hi).ExpandBy(Reach * FMath::Sqrt(Scale2) + 1.0) : FBox(ForceInit);
		}
	}
	if (!Local.IsValid) return FBoxSphereBounds(BoundTransform.GetLocation(), FVector::ZeroVector, 0.f);
	FBoxSphereBounds Out(BoundTransform.Equals(FTransform::Identity) ? Local : Local.TransformBy(BoundTransform));
	Out.BoxExtent *= BoundsScale;
	Out.SphereRadius *= BoundsScale;
	return Out;
}

void UAcInstancedMesh::BuildComponentInstanceData(const EShaderPlatform InShaderPlatform, FInstanceUpdateComponentDesc& OutData)
{
	const int32 Frames = CVarMaterialDescFrames.GetValueOnAnyThread();
	if (Frames <= 0)
	{
		Super::BuildComponentInstanceData(InShaderPlatform, OutData);
		return;
	}
	// The engine's (UInstancedStaticMeshComponent::BuildComponentInstanceData,
	// UE 5.8: compare it again after an engine update; the cvar at 0 is the
	// engine's own) line for line, but the descriptor: only when the mesh, the materials or
	// the platform changed, or `Frames` updates on. Conservative bounds are
	// never turned on here, so the engine's other branch never runs.
	const UStaticMesh* Mesh = GetStaticMesh();
	if (DescLeft <= 0 || Mesh != DescMesh || InShaderPlatform != DescPlatform || OverrideMaterials != DescMaterials)
	{
		MaterialDesc = GetUsedMaterialPropertyDesc(InShaderPlatform);
		DescMesh = Mesh;
		DescPlatform = InShaderPlatform;
		DescMaterials = OverrideMaterials;
		DescLeft = Frames;
	}
	--DescLeft;
	OutData.PrimitiveMaterialDesc = MaterialDesc;
	OutData.Flags = MakeInstanceDataFlags(MaterialDesc.bAnyMaterialHasPerInstanceRandom, MaterialDesc.bAnyMaterialHasPerInstanceCustomData);
	OutData.ComponentMobility = Mobility;
	OutData.PrimitiveLocalToWorld = GetRenderMatrix();
	OutData.StaticMeshBounds = Mesh->GetBounds();
	OutData.NumProxyInstances = PerInstanceSMData.Num();
	OutData.NumSourceInstances = PerInstanceSMData.Num();
	OutData.NumCustomDataFloats = NumCustomDataFloats;
	OutData.BuildChangeSet = [this](FISMInstanceUpdateChangeSet& ChangeSet)
	{
		BuildInstanceDataDeltaChangeSetCommon(ChangeSet);
		ChangeSet.SetInstanceTransforms(MakeStridedView(PerInstanceSMData, &FInstancedStaticMeshInstanceData::Transform));
		ChangeSet.SetInstancePrevTransforms(MakeArrayView(PerInstancePrevTransform));
	};
}
