#include "AcDemolish.h"

#include "AcPilotText.h"

#include "Rules.h"

FString AcDemolish::Prompt(const ac::StructureKind Kind, const TCHAR* Key)
{
	const ac::Price Back = ac::Simulation::salvage(Kind, true);
	FString Out = FString::Printf(TEXT("%sDEMOLISH %s · +%lld ORE"), Key, *AcPilotText::Title(Kind).ToUpper(), (long long)Back.ore);
	if (Back.hydrogen > 0) Out += FString::Printf(TEXT(" +%lld MH"), (long long)Back.hydrogen);
	return Out;
}

FString AcDemolish::Toast(const ac::StructureKind Kind, const int64 Ore, const int64 Hydrogen)
{
	FString Out = FString::Printf(TEXT("%s demolished · +%lld ore"), *AcPilotText::Title(Kind), (long long)Ore);
	if (Hydrogen > 0) Out += FString::Printf(TEXT(" +%lld MH"), (long long)Hydrogen);
	return Out;
}

void AcDemolish::FHold::Start(const int64 InBuilding, const double Now)
{
	Building = InBuilding;
	Since = Now;
}

TOptional<int64> AcDemolish::FHold::Tick(const int64 Target, const double Now)
{
	if (!Active()) return {};
	if (Target != Building)
	{
		Cancel();
		return {};
	}
	if (Now - Since < ac::Rules::demolishHold) return {};
	const int64 Done = Building;
	Cancel();
	return Done;
}

double AcDemolish::FHold::Fill(const double Now) const
{
	return Active() ? FMath::Clamp((Now - Since) / ac::Rules::demolishHold, 0.0, 1.0) : 0.0;
}
