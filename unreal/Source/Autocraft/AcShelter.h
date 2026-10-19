// `AcShelter`: no rain indoors (docs/weather.md, "Shelter: no rain
// indoors"). One geometric notion, the shelter box: for every complete
// Citadel with the hall, the world-space box of its interior
// (`Rules::footprint(citadel).interior`; Citadels never turn, so the box is
// axis-aligned) from under the floor to the roof slab's underside. AAcWeather
// writes up to `Slots` of them, nearest the camera first, into MPC_AcWorld
// (`Shelter0..3`, `ShelterLow`, `ShelterHigh`), where the rain streaks
// (M_AcRain), the wet hulls (M_Hull) and the wet ground (M_Terrain) read them.
// The lens drops and the rain bed follow the driven unit instead
// (`Simulation::sheltered`), not the camera.
#pragma once

#include "CoreMinimal.h"

#include "Types.h"

namespace ac
{
	class Simulation;
}

namespace AcShelter
{
	/// How many boxes the materials read.
	inline constexpr int32 Slots = 4;
	/// Each box grown by this on every side (cm), so the hall's inner faces
	/// (the walls at ±500, the ceiling at 600) fall inside it.
	inline constexpr double FaceCm = 5.0;
	/// How far under the hall's ground the box reaches (cm): the model's
	/// walls and floor go this deep to hide a slope (citadel.py `SINK`).
	inline constexpr double SinkCm = 100.0;
	/// The lens dries in this long (the beads slide off), and wets again in this.
	inline constexpr float LensDrySeconds = 1.5f;
	inline constexpr float LensWetSeconds = 0.5f;

	/// One shelter box, world cm.
	struct FBox
	{
		FVector2D Centre = FVector2D::ZeroVector, Half = FVector2D::ZeroVector;
		double Low = 0, High = 0;
		bool Contains(const FVector& P) const
		{
			return FMath::Abs(P.X - Centre.X) < Half.X && FMath::Abs(P.Y - Centre.Y) < Half.Y && P.Z > Low && P.Z < High;
		}
		bool operator==(const FBox&) const = default;
	};

	/// The box of Citadel `S` standing on ground `GroundCells` (its centre's
	/// height); none for any other kind, one not complete or dead, or without
	/// the hall (`Rules::citadelHall`).
	AUTOCRAFT_API TOptional<FBox> BoxOf(const ac::Structure& S, double GroundCells);
	/// The boxes of `Structures`, nearest `Camera` first, at most `Slots`.
	AUTOCRAFT_API TArray<FBox> Boxes(const std::vector<ac::Structure>& Structures, TFunctionRef<double(ac::Vec2)> GroundCells,
		const FVector& Camera);

	/// The lens drops' rate (M_AcLensRain's `LensRain`) a frame of `Dt`
	/// seconds on from `Rate`, toward `Target` (0 sheltered, else the rain
	/// rate `Full`): it dries in `LensDrySeconds` and wets again in
	/// `LensWetSeconds`, both at `Full`'s pace.
	AUTOCRAFT_API float EaseLens(float Rate, float Target, float Full, float Dt);

	/// The driven unit (the pilot's) stands sheltered in the hall; false
	/// when no unit is driven.
	AUTOCRAFT_API bool DrivenSheltered(const UObject* WorldContext);

	/// The shelter is on (`ac.Weather.Shelter` 1, the default). Off
	/// (0, or -AcNoShelter): no boxes, nothing sheltered, the rain
	/// everywhere as before it (for comparison shots).
	AUTOCRAFT_API bool On();
}
