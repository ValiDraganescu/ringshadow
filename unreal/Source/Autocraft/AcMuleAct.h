// `AcMuleAct`: the Mule's and the Foundry's side of the act key (C4,
// docs/haul-and-ferry.md section 2) in the MMORPG mode: the pilot's own Mule
// as the server says it, why a Mule can't be bought, the refusal's toast,
// the HUD's load line. What E does at a Mule (climb on at its perch, fill it
// at its hopper, climb off) and at a Foundry's call post (buy one) is
// `AcActPick`'s (C5, docs/acting.md).
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Simulation.h"

class FAcMmoClient;

namespace AcMuleAct
{
	/// What the pilot's own Mule is now, as the server's Progress says it.
	struct FOwn
	{
		/// Its unit here (unset: none here, or rolling out).
		TOptional<int64> Mule;
		/// It has one (here, elsewhere, or rolling out of a Foundry's door).
		bool bHas = false;
		bool bRolling = false;
	};

	/// The pilot's own Mule from the MMORPG client's Progress (none: no client).
	AUTOCRAFT_API FOwn OwnOf(const FAcMmoClient* Mmo);

	/// Why a Mule can't be bought now (nil: it can, as far as the client
	/// sees; the wallet is the server's to check): "You have a Mule already",
	/// "Your Mule is rolling out".
	AUTOCRAFT_API TOptional<FString> BuyRefusal(const FOwn& Own);

	/// A refusal of the Mule's (the client's, the server's) as a toast.
	AUTOCRAFT_API void Toast(const UObject* WorldContext, const FString& Why);

	/// "In the Mule: 380 ore, 120 MH" for Mule `M` (the HUD's line).
	AUTOCRAFT_API FString LoadLine(const ac::Unit& M);
	/// The Mule the HUD's line is about for the driven unit `U`: the Mule it
	/// drives, else its own within `NearCells` (unset: none).
	inline constexpr double NearCells = 4.0;
	AUTOCRAFT_API const ac::Unit* Shown(const ac::Simulation& Sim, const ac::Unit& U, const FOwn& Own);
}
