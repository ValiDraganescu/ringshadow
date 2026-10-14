#include "AcMissions.h"

#include "AcHudStyle.h"
#include "AcMmo.h"
#include "AcPilotText.h"

#include "Progression.h"
#include "Simulation.h"

namespace
{
	using FMission = ac::net::CitadelMission;

	FString Grouped(int64 N) { return FText::AsNumber(N).ToString(); }
}

FString AcMissions::KindName(const FMission::Kind Kind)
{
	switch (Kind)
	{
	case FMission::Kind::haul: return TEXT("HAUL");
	case FMission::Kind::survey: return TEXT("SURVEY");
	case FMission::Kind::salvage: return TEXT("SALVAGE");
	case FMission::Kind::hold: return TEXT("HOLD");
	case FMission::Kind::hunt: return TEXT("HUNT");
	}
	return TEXT("MISSION");
}

FString AcMissions::TierName(const FMission::Tier Tier)
{
	switch (Tier)
	{
	case FMission::Tier::easy: return TEXT("EASY");
	case FMission::Tier::hard: return TEXT("HARD");
	case FMission::Tier::perilous: return TEXT("PERILOUS");
	}
	return TEXT("EASY");
}

FString AcMissions::Reward(const FMission& M)
{
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("%s XP"), *Grouped(int64(FMath::RoundToDouble(M.xp)))));
	if (M.ore > 0) Parts.Add(FString::Printf(TEXT("%s ore"), *Grouped(M.ore)));
	if (M.hydrogen > 0) Parts.Add(FString::Printf(TEXT("%s MH"), *Grouped(M.hydrogen)));
	if (M.blueprint && M.count > 0)
		Parts.Add(FString::Printf(TEXT("%s blueprint ×%lld"), UTF8_TO_TCHAR(ac::Simulation::title(*M.blueprint).c_str()), (long long)M.count));
	if (M.item && M.count > 0) Parts.Add(FString::Printf(TEXT("%s ×%lld"), UTF8_TO_TCHAR(ac::Shop::item(*M.item).title), (long long)M.count));
	return FString::Join(Parts, TEXT(" · "));
}

FString AcMissions::Progress(const FMission& M)
{
	if (M.state == FMission::State::done) return TEXT("DONE");
	const long long Have = (long long)M.have, Need = (long long)M.need;
	switch (M.kind)
	{
	case FMission::Kind::haul:
		return FString::Printf(TEXT("%s / %s %s home"), *Grouped(Have), *Grouped(Need), M.target == "hydrogen" ? TEXT("MH") : TEXT("ore"));
	case FMission::Kind::survey: return FString::Printf(TEXT("%lld / %lld s scanned"), Have, Need);
	case FMission::Kind::salvage: return TEXT("the site unopened");
	case FMission::Kind::hold: return FString::Printf(TEXT("%lld / %lld s held in the shadow"), Have, Need);
	case FMission::Kind::hunt: return FString::Printf(TEXT("%lld / %lld of the Brood"), Have, Need);
	}
	return FString();
}

float AcMissions::Share(const FMission& M)
{
	if (M.state == FMission::State::done) return 1.f;
	return M.need > 0 ? FMath::Clamp(float(M.have) / float(M.need), 0.f, 1.f) : 0.f;
}

FString AcMissions::RegionName(const FAcMmoClient& Mmo, const std::string& Id)
{
	if (Id.empty()) return FString();
	if (const ac::net::WorldMessage* W = Mmo.World())
	{
		for (const ac::net::WorldMessage::Region& R : W->regions)
			if (R.id == Id) return UTF8_TO_TCHAR(R.name.c_str());
	}
	return UTF8_TO_TCHAR(Id.c_str());
}

TArray<const FMission*> AcMissions::Marked(const FAcMmoClient& Mmo)
{
	TArray<const FMission*> Out;
	if (const ac::net::ProgressMessage* P = Mmo.Progress())
	{
		for (const FMission& M : P->journal)
			if (M.state == FMission::State::taken && !M.region.empty()) Out.Add(&M);
	}
	return Out;
}

FLinearColor AcMissions::TierColor(const FMission::Tier Tier)
{
	switch (Tier)
	{
	case FMission::Tier::easy: return FAcHudStyle::Srgb(0.45, 0.95, 0.62);
	case FMission::Tier::hard: return FAcHudStyle::Srgb(1.0, 0.72, 0.3);
	case FMission::Tier::perilous: return FAcHudStyle::Srgb(1.0, 0.36, 0.3);
	}
	return FLinearColor::White;
}

TArray<FAcPilotGoal> AcMissions::Goals(const FAcMmoClient& Mmo, const ac::Vec2 From)
{
	TArray<FAcPilotGoal> Out;
	const ac::net::WorldMessage::Region* Here = Mmo.Here();
	if (!Here || Mmo.Travelling()) return Out;
	for (const FMission* M : Marked(Mmo))
	{
		if (M->region != Here->id || M->kind == FMission::Kind::hunt) continue;
		FAcPilotGoal G{UTF8_TO_TCHAR(M->title.c_str()), M->at, ac::distance(M->at, From)};
		G.Color = TierColor(M->tier);
		G.bMission = true;
		Out.Add(G);
	}
	return Out;
}

FString AcMissions::Heading(const ac::Vec2 D)
{
	static const TCHAR* const Names[] = {TEXT("north"), TEXT("north-east"), TEXT("east"), TEXT("south-east"), TEXT("south"), TEXT("south-west"),
		TEXT("west"), TEXT("north-west")};
	const double Degrees = FMath::RadiansToDegrees(std::atan2(D.x, -D.y));
	const int32 I = (int32(FMath::RoundToDouble(Degrees / 45.0)) % 8 + 8) % 8;
	return Names[I];
}
