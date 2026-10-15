// A static mesh the game builds at run time (`BuildFromMeshDescriptions`:
// the ground, wrecks, beacons, pointer cues). A cooked game can't work out
// its materials' UV densities (`UpdateUVChannelData` is editor-only), and
// texture streaming's first look at such a mesh fires an ensure, a stack
// walk on the game thread (K3, packaged only). They are marked made: the
// densities stay 0, as streaming read them before.
#pragma once

#include "Engine/StaticMesh.h"

inline void AcBuiltMeshReady(UStaticMesh* Mesh)
{
	if (!Mesh) return;
	for (FStaticMaterial& M : Mesh->GetStaticMaterials()) M.UVChannelData.bInitialized = true;
}
