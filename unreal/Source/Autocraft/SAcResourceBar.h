// `SAcResourceBar`: ore, Metallic Hydrogen and supply on a 340×42 plate, top
// right (`HUD.buildBar`, Sources/Autocraft/HUD.swift:91). The ore count
// climbs toward the stockpile by max(1, Δ/3) per update, 60 times a second,
// so a deposit reads as a running counter (HUD.swift:398-403).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AUTOCRAFT_API SAcResourceBar : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcResourceBar) {}
	SLATE_END_ARGS()

	/// The plate's size in points.
	static constexpr float Width = 340.0f;
	static constexpr float Height = 42.0f;

	void Construct(const FArguments& InArgs);

	/// The local player's stock. `RealDelta` (seconds) paces the ore count-up.
	void Update(int64 Ore, int64 Hydrogen, int64 SupplyUsed, int64 SupplyCap, double RealDelta);
	/// Show the next stock at once, with no gain flashed (a new game, a new
	/// region's mirror). Until that update the shown stock stays.
	void Reset() { bRebase = true; }

	int64 GetShownOre() const { return ShownOre; }
	/// Gains flashed so far (the MMORPG test's checks).
	int32 GetGainsFlashed() const { return GainsFlashed; }
	/// The supply (housing) count: off in the MMORPG test, where it means
	/// nothing.
	void SetShowSupply(bool bShow);
	bool ShowsSupply() const { return bShowSupply; }
	/// The MMORPG test: a gain flashes the count and says "+N" beside it
	/// (docs/mmorpg-shots.md #11).
	void SetFlashGains(bool bFlash) { bFlashGains = bFlash; }
	/// The MMORPG mode (C1): the realm wallet, small in the supply's cell
	/// ("REALM" over "1,240 · 56 MH"). Unset: not shown.
	void SetRealm(TOptional<TPair<int64, int64>> OreHydrogen);
	TOptional<TPair<int64, int64>> GetRealm() const { return Realm; }
	/// R2: the realm's projects running, each its name ("BEACON") and the
	/// share of its world day left: a gold bar under the realm wallet, a
	/// segment each, and their names beside "REALM".
	void SetProjects(const TArray<TPair<FString, float>>& InProjects);
	const TArray<TPair<FString, float>>& GetProjects() const { return Projects; }

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Width, Height); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
	int64 ShownOre = -1;
	int64 TargetOre = 0;
	int64 Hydrogen = 0;
	int64 SupplyUsed = 0;
	int64 SupplyCap = 0;
	bool bShowSupply = true;
	bool bFlashGains = false;
	bool bRebase = false;
	TOptional<TPair<int64, int64>> Realm;
	TArray<TPair<FString, float>> Projects;
	int32 GainsFlashed = 0;
	/// The last gain of ore and MH, and when (real seconds).
	int64 GainOre = 0, GainHydrogen = 0;
	double GainOreAt = -10, GainHydrogenAt = -10;
	/// Real time not yet spent on count-up steps.
	double Pending = 0.0;
};
