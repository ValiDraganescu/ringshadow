// The region's ground palette on screen (docs/regions.md, "ground"; W3): the
// MMORPG mode paints each region's ground its own way (rust, salt, ash,
// glass, slag, frost, ochre) through M_Terrain's ground parameters
// (Tools/Editor/make_terrain_material.py), and blends toward each
// neighbour's palette over the last 16 cells to the edge, so both sides of
// an edge meet in one colour and the scenery past it is the neighbour's.
// The palette comes from the World message (`WorldMessage::Region::ground`),
// the region the pilot is in and the grid round it.
//
// Out of the mode, or on `plain` ground, the parameters stay at the
// material's defaults: the game's own brown dirt.
#pragma once

#include "CoreMinimal.h"

#include "RegionFile.h"

class UMaterialInstanceDynamic;
class UAcSimSubsystem;

namespace AcGroundLook
{
	/// One ground's look, as M_Terrain reads it.
	struct FPalette
	{
		/// rgb: the ground's colour (linear albedo); a: how much of it over
		/// the textures' own colour (0: plain).
		FLinearColor Tint = FLinearColor(0.2f, 0.15f, 0.1f, 0.0f);
		/// Salt crust, glass facets, slag embers, ochre bands (0-1 each).
		FLinearColor Fx = FLinearColor(0, 0, 0, 0);
		/// Frost on it from night to mid-morning (0-1), the region's own only.
		float Frost = 0.0f;
	};

	FPalette Of(ac::region::Ground Ground);

	/// How much of a frosty ground's frost shows at an hour of the world's
	/// day: forming through the night, thickest at dawn, gone by mid-morning.
	float FrostAt(double Hour);

	/// Sets the terrain material's ground parameters for the region the
	/// pilot is in (from `Sim`'s world client) at `Hour`, with the slag's
	/// embers stronger as it gets `Dark` (0-1). Only what changed is set.
	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, double Hour, double Dark);
	/// The same for region `Region` of the World message (W4: each ground
	/// set's material is its own region's; INDEX_NONE: plain).
	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, int64 Region, double Hour, double Dark);
	/// A ground's palette and its neighbours' (N, E, S, W; unset: none),
	/// out of a world (the map bake's region).
	void ApplyGround(UMaterialInstanceDynamic* Material, ac::region::Ground Ground, const TOptional<ac::region::Ground> (&Around)[4],
		double Hour, double Dark);
}
