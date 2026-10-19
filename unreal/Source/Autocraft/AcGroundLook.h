// The region's ground palette on screen (docs/regions.md, "ground"; W3): the
// MMORPG mode paints each region's ground its own way (rust, salt, ash,
// glass, slag, frost, ochre) through M_Terrain's ground parameters
// (Tools/Editor/make_terrain_material.py), and the palettes bleed into each
// other where regions meet: a wandering, patchy band 34 cells wide across
// each edge, the corners' diagonal neighbours too (`ac::groundblend`,
// GroundBlend.h; the world-building skill's GROUND-LOOK.md, "Where palettes
// meet"). Both sides of an edge draw the same ground there, and the
// scenery's clutter and rock faces follow the same field.
// The palettes come from the World message (`WorldMessage::Region::ground`),
// the region the pilot is in and the grid round it.
//
// Out of the mode, or on `plain` ground, the parameters stay at the
// material's defaults: the game's own brown dirt.
#pragma once

#include "CoreMinimal.h"

#include "GroundBlend.h"
#include "RegionFile.h"
#include "Types.h"

#include <string>

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
		/// The ground's own texture (M_Terrain's arrays' slice; 0 the dirt,
		/// which rust and ochre tint; 8 the frost, which rimes from night to
		/// mid-morning wherever it lies).
		float Layer = 0.0f;
	};

	FPalette Of(ac::region::Ground Ground);

	/// How much of a frosty ground's frost shows at an hour of the world's
	/// day: forming through the night, thickest at dawn, gone by mid-morning.
	float FrostAt(double Hour);

	/// A region's ground and what it meets: its palette, the eight round it
	/// (N, E, S, W, NE, SE, SW, NW; unset: none) and their ids (own first,
	/// then the same order; "" none), which seed where they meet.
	struct FAround
	{
		ac::region::Ground Own = ac::region::Ground::plain;
		TOptional<ac::region::Ground> Around[8];
		std::string Ids[9];
		/// Added to a world-grid point for the field: the map bake shows a
		/// region alone at the grid's 0, this is its place on the grid.
		ac::Vec2 Shift{0, 0};
		/// The field's frame for a ground whose map has bounds `B` and lies
		/// at `Centre` on the world's grid (`AAcRegionGround::Centre`).
		ac::groundblend::Frame Frame(const ac::GroundRect& B, ac::Vec2 Centre) const;
		/// Slot `S` (`ac::groundblend::Slot`)'s palette: an empty slot is
		/// the region's own.
		ac::region::Ground At(int32 S) const { return S <= 0 || S > 8 || !Around[S - 1] ? Own : *Around[S - 1]; }
	};

	/// Region `Region` of `Sim`'s World message (unset: not in a world).
	TOptional<FAround> AroundOf(const UAcSimSubsystem* Sim, int64 Region);

	/// Sets the terrain material's ground parameters for the region the
	/// pilot is in (from `Sim`'s world client) at `Hour`, with the slag's
	/// embers stronger as it gets `Dark` (0-1). Only what changed is set.
	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, double Hour, double Dark);
	/// The same for region `Region` of the World message (W4: each ground
	/// set's material is its own region's; INDEX_NONE: plain).
	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, int64 Region, double Hour, double Dark);
	/// A ground and what is round it, out of a world (the map bake's region).
	void ApplyAround(UMaterialInstanceDynamic* Material, const FAround& Around, double Hour, double Dark);
}
