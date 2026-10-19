#include "AcMuleAct.h"

#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcProgressHud.h"

#include "Mule.h"
#include "Rules.h"

namespace
{
	bool IsMule(const ac::Unit& U) { return U.kind == ac::UnitKind::mule && U.hp > 0; }
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
