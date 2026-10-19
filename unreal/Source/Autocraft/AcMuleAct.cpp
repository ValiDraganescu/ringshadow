#include "AcMuleAct.h"

#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcProgressHud.h"

#include "Mule.h"
#include "Rules.h"

namespace
{
	FString Text(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }

	bool IsMule(const ac::Unit& U) { return U.kind == ac::UnitKind::mule && U.hp > 0; }
}

FString AcMuleAct::FAct::Prompt() const
{
	if (Why) return *Why;
	switch (Kind)
	{
	case EKind::Board: return TEXT("E · CLIMB ON");
	case EKind::Leave: return TEXT("E · CLIMB OFF");
	case EKind::Fill: return TEXT("E · FILL THE MULE");
	case EKind::Buy:
		return FString::Printf(TEXT("E · BUY MULE · %lld ORE · %lld MH"), (long long)ac::Mule::ore(), (long long)ac::Mule::hydrogen());
	}
	return FString();
}

AcMuleAct::FOwn AcMuleAct::OwnOf(const FAcMmoClient* Mmo)
{
	FOwn Own;
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	if (!P || !P->mule) return Own;
	Own.bHas = true;
	Own.bRolling = P->mule->unit == 0;
	Own.Mule = Mmo->OwnMule();
	return Own;
}

TOptional<FString> AcMuleAct::BuyRefusal(const FOwn& Own)
{
	if (Own.bRolling) return FString(TEXT("Your Mule is rolling out"));
	if (Own.bHas) return FString(TEXT("You have a Mule already"));
	return {};
}

TOptional<AcMuleAct::FAct> AcMuleAct::Find(const ac::Simulation& Sim, const ac::Unit& U, const FOwn& Own)
{
	// Driving the Mule: climb off.
	if (U.kind == ac::UnitKind::mule)
	{
		FAct A;
		A.Kind = FAct::EKind::Leave;
		A.Target = U.id;
		if (const std::optional<int64_t> Rider = Sim.muleRider(U.id)) A.Rider = *Rider;
		else A.Why = FString(TEXT("No one on the perch"));
		return A;
	}
	if (U.kind != ac::UnitKind::prospector || U.task == ac::Unit::Task::aboard || U.task == ac::Unit::Task::atGun) return {};

	// The nearest Mule whose perch or hopper is in reach.
	const ac::Unit* Best = nullptr;
	double BestD = 0.0;
	for (const ac::Unit& M : Sim.state.units)
	{
		if (!IsMule(M)) continue;
		const double D = FMath::Min(ac::distance(U.position, ac::Mule::perch(M)), ac::distance(U.position, ac::Mule::hopper(M)));
		if (D > ac::Mule::reach || (Best && D >= BestD)) continue;
		Best = &M;
		BestD = D;
	}
	if (Best)
	{
		const ac::Unit& M = *Best;
		FAct A;
		A.Target = M.id;
		const bool bAtHopper = ac::distance(U.position, ac::Mule::hopper(M)) <= ac::Mule::reach;
		const bool bAtPerch = ac::distance(U.position, ac::Mule::perch(M)) <= ac::Mule::reach;
		// Carrying at the hopper fills it; else at the perch climbs on.
		if (U.carrying > 0 && bAtHopper)
		{
			A.Kind = FAct::EKind::Fill;
			if (const std::optional<std::string> Why = Sim.muleFillRefusal(U, M)) A.Why = Text(*Why);
		}
		else if (bAtPerch)
		{
			A.Kind = FAct::EKind::Board;
			if (const std::optional<std::string> Why = Sim.muleBoardRefusal(U, M)) A.Why = Text(*Why);
		}
		else
		{
			// At the hopper with nothing in the gripper: nothing to do there.
			return {};
		}
		// Only its owner drives it or fills it (the server says it again).
		if (!A.Why && Own.Mule != M.id) A.Why = FString(TEXT("Not your Mule"));
		return A;
	}

	// A finished Foundry in reach: buy a Mule.
	for (const ac::Structure& S : Sim.state.structures)
	{
		if (S.kind != ac::StructureKind::foundry || !S.complete()) continue;
		if (ac::distance(S.position, U.position) > ac::Rules::radius(S.kind) + FoundryReach) continue;
		FAct A;
		A.Kind = FAct::EKind::Buy;
		A.Target = S.id;
		A.Why = BuyRefusal(Own);
		return A;
	}
	return {};
}

void AcMuleAct::Toast(const UObject* WorldContext, const FString& Why)
{
	if (UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(WorldContext); Hud && Hud->Widget())
	{
		Hud->Widget()->Toast(Why.ToUpper(), TEXT("The Mule"), FAcHudStyle::Amber(), FPlatformTime::Seconds());
	}
	UE_LOG(LogAutocraft, Log, TEXT("mule: refused: %s"), *Why);
}

FString AcMuleAct::LoadLine(const ac::Unit& M)
{
	const ac::Haul H = M.haul.value_or(ac::Haul{});
	return FString::Printf(TEXT("In the Mule: %lld ore, %lld MH"), (long long)H.ore, (long long)H.hydrogen);
}

const ac::Unit* AcMuleAct::Shown(const ac::Simulation& Sim, const ac::Unit& U, const FOwn& Own)
{
	if (U.kind == ac::UnitKind::mule) return nullptr;
	if (!Own.Mule) return nullptr;
	const std::optional<int64_t> I = Sim.unitIndex(*Own.Mule);
	if (!I) return nullptr;
	const ac::Unit& M = Sim.state.units[size_t(*I)];
	return IsMule(M) && ac::distance(M.position, U.position) <= NearCells ? &M : nullptr;
}
