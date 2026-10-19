// `AcMuleAct`: what the act key does at a Mule or a Foundry (C4,
// docs/haul-and-ferry.md section 2) in the MMORPG mode, before the
// Citadel's acts: driving the Mule, E climbs off ("E · CLIMB OFF"); on foot
// at its perch (`ac::Mule::reach` of `Mule::perch`), E climbs on ("E ·
// CLIMB ON"); carrying a load at its hopper, E tips it in ("E · FILL THE
// MULE"; the hopper wins over the perch while it carries); at a finished
// Foundry with no Mule of its own, E buys one ("E · BUY MULE · 600 ORE ·
// 150 MH"). A refusal the client can see (someone else's Mule, a full one,
// a Mule already) is the prompt and the key's note, and nothing is sent;
// the server's own refusals come back as notes. The pawn sends the
// MMORPG client's take (board, leave), MuleFill and BuyMule on it.
//
// Plain C++ over `ac::Simulation` (no world).
#pragma once

#include "CoreMinimal.h"

#include "Simulation.h"

class FAcMmoClient;

namespace AcMuleAct
{
	/// How far from a Foundry's centre, past its radius, its E reaches (cells).
	inline constexpr double FoundryReach = 2.0;

	struct FAct
	{
		enum class EKind : uint8
		{
			Board,
			Leave,
			Fill,
			Buy
		};
		EKind Kind = EKind::Board;
		/// The Mule (Board, Leave, Fill) or the Foundry (Buy).
		int64 Target = 0;
		/// Leave: the Prospector riding it (driven again on foot).
		int64 Rider = 0;
		/// Why the key would be refused now: the prompt says it, the key
		/// shows it as a note and sends nothing.
		TOptional<FString> Why;

		/// "E · CLIMB ON", "E · CLIMB OFF", "E · FILL THE MULE",
		/// "E · BUY MULE · 600 ORE · 150 MH", or `Why`.
		AUTOCRAFT_API FString Prompt() const;
	};

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

	/// The act for the driven unit `U`, or unset (nothing at hand).
	AUTOCRAFT_API TOptional<FAct> Find(const ac::Simulation& Sim, const ac::Unit& U, const FOwn& Own);

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
