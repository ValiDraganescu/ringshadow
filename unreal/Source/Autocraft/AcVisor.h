// The visor's glass in the rain (docs/weather.md, "Rain on the visor and the
// wipers"): what the `M_AcLensRain` post-process is told each frame, made
// only from the wiper's rule (`ac::Wipers`) and the weather around. The
// glass is the driven unit's in first person: the cockpit, and a Citadel
// gun's live feed (its camera's housing has the same wiper); never the
// chase camera's or the top-down view's.
#pragma once

#include "CoreMinimal.h"

#include "Wipers.h"

/// The material's parameters (Tools/Editor/make_weather_materials.py).
struct FAcVisorParams
{
	/// `LensRain`: the rain rate on the glass now (0 sheltered).
	float Rain = 0.f;
	/// `GlassA`, `GlassB`: the latest sweep and the one before: seconds
	/// since it began, seconds each way (0: none), the water landed since
	/// it began and since it ended.
	FLinearColor A = FLinearColor::Transparent;
	FLinearColor B = FLinearColor::Transparent;
	/// `GlassSeed`: the two sweeps' numbers (wrapped), the water on a spot
	/// the blade never reaches.
	FLinearColor Seed = FLinearColor::Transparent;
	/// `GlassArc`: the pivot's height (view heights, below the bottom
	/// centre), the blade's reach, the park and far angles.
	FLinearColor Arc = FLinearColor::Transparent;
	/// `GlassBlade`: its angle, its way (1 out, -1 back, 0 parked), how long
	/// its smear takes to dry (s), its half width (view heights).
	FLinearColor Blade = FLinearColor::Transparent;
	/// `GlassHist`: the water landed in the last 0.5, 1, 2 and 4 s.
	FLinearColor Hist = FLinearColor::Transparent;
	/// `GlassAir`: the wind across the view (-1 to the left, 1 right), the
	/// lightning's flash, the daylight (0 night, 1 day), the glass's clock (s).
	FLinearColor Air = FLinearColor::Transparent;
	/// Anything to draw: water on the glass or the blade moving.
	bool bWet = false;
};

namespace AcVisor
{
	/// The blade's pivot below the view's bottom centre and its reach (view
	/// heights): the arc covers all but the top corners.
	constexpr double PivotY = 1.06;
	constexpr double Reach = 1.12;
	constexpr double BladeHalfWidth = 0.0085;
	/// The glass's clock wraps (s): a whole number of the rivulets' slots.
	constexpr double ClockWrap = 4096.0;
	/// Water (rain-seconds) under which a never-wiped corner shows nothing.
	constexpr double DryWater = 0.05;
	/// The wind across the view at this speed (m/s) and more is full.
	constexpr double FullWind = 20.0;

	/// The parameters for the wiper as it is now, the wind across the view
	/// (m/s, positive to the right), the flash (0-1) and the daylight (0-1).
	FAcVisorParams Params(const ac::Wipers& Wiper, double WindAcross, double Flash, double Light);
}
