// `AcDemolish`: taking down a realm building by hand (C3,
// docs/guns-and-practice.md "Limits per region and demolish"). Driving, E
// held for `ac::Rules::demolishHold` on a building of the realm under the
// crosshair within the act's reach (`AcActPick`: the body's edge within
// `ac::Act::reach` of the point the crosshair is on, no act's box under it)
// asks the server to demolish it; let go sooner, or look off it, and
// nothing happens. Only E: the click and Space keep every act they have
// (fire, weld, mend, enter). The pawn holds the key (`FHold`), the cockpit shows the prompt and
// the ring (`FAcPilotInfo::Demolish`); the top-down console's DEMOLISH button
// holds the same time on the selected building.
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Simulation.h"

namespace AcDemolish
{
	/// "HOLD E · DEMOLISH GATLING GUN · +62 ORE +12 MH" (no MH part when it
	/// gives none back); with `Key` empty (the console's button) "DEMOLISH
	/// GATLING GUN · +62 ORE +12 MH".
	AUTOCRAFT_API FString Prompt(ac::StructureKind Kind, const TCHAR* Key = TEXT("HOLD E · "));
	/// "Gatling Gun demolished · +62 ore" (the toast).
	AUTOCRAFT_API FString Toast(ac::StructureKind Kind, int64 Ore, int64 Hydrogen);

	/// The hold: started on a building, it fills over `Rules::demolishHold`
	/// while the same building stays the target; another target (or none),
	/// or the key let go (`Cancel`), ends it with nothing done.
	struct AUTOCRAFT_API FHold
	{
		int64 Building = INDEX_NONE;
		double Since = 0;

		bool Active() const { return Building != INDEX_NONE; }
		void Start(int64 InBuilding, double Now);
		void Cancel() { Building = INDEX_NONE; }
		/// One frame with the key still down and `Target` under the sight.
		/// The building once the hold is full (then it ends); unset before,
		/// and when it was cancelled by a change of target.
		TOptional<int64> Tick(int64 Target, double Now);
		/// 0 to 1 through the hold (0 when none).
		double Fill(double Now) const;
	};
}
