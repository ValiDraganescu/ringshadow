// Quality tiers (docs/perf.md, client round 8): one picture per class of
// Mac GPU, so a smaller Mac keeps its frame rate. The tier comes from the
// GPU: its name ("Apple M2 Pro") gives the generation, IOKit gives its core
// count, and their product scores it against the M4 Max (40 cores x 1.35 =
// 54). The engine's device-profile matching sees only the name, which
// cannot tell an 8-core M4 from a 10-core one, so the choice is made here;
// each lower tier's settings are a `[Ac_<Tier> DeviceProfileFragment]`
// section of DefaultDeviceProfiles.ini, set at device-profile priority.
// The top tier sets nothing: today's picture, unchanged. As of 2026-10-18
// no lower tier has its section yet (docs/perf.md round 8: measured, not
// settled), so every Mac draws as the top tier; the tier is chosen and logged.
//
// Force a tier: `-AcTier=top|high|mid|floor`, or `Tier=` in the `[AcQuality]`
// section of Engine.ini (Saved/Config/Mac/Engine.ini on a player's Mac).
// The choice is logged once at start (`quality: tier ...`) and readable as
// `ac.QualityTier`. A tier never changes the HUD's scale.
#pragma once

#include "CoreMinimal.h"

namespace AcQualityTier
{
	enum class ETier : uint8 { Top, High, Mid, Floor };

	/// "top", "high", "mid", "floor".
	const TCHAR* Name(ETier Tier);
	/// From a name (case-insensitive); unset when it names none.
	TOptional<ETier> Named(const FString& Name);

	/// The generation's per-core factor from the GPU's name: M1 1.0, M2 1.12,
	/// M3 1.25, M4 1.35, later 1.45; 0 when it is not an Apple M-series GPU.
	double GenerationFactor(const FString& GpuName);
	/// Cores x the generation's factor (the M4 Max's 40 cores: 54).
	double Score(const FString& GpuName, int32 Cores);
	/// The tier a score falls in: top >= 35, high >= 18, mid >= 9.5, else floor.
	ETier FromScore(double Score);
	/// Without a core count, the name alone: Max and Ultra top, Pro high, else mid.
	ETier FromName(const FString& GpuName);

	/// Chooses the tier (an override first, then the GPU), applies its
	/// settings and logs it. Once, at the game module's start.
	void Apply();
	/// The tier chosen (top before Apply).
	ETier Current();
}
