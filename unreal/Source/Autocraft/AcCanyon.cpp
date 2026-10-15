#include "AcCanyon.h"

#include "AcMmo.h"
#include "AcProgressHud.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RegionFile.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	using FZone = ac::net::Sky::Zone;

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// "North tunnel" → "North": the tunnels named together read "the North
	/// and East tunnels".
	FString Short(const FString& Name)
	{
		FString Out = Name;
		Out.RemoveFromEnd(TEXT(" tunnel"));
		return Out;
	}
}

const TArray<FAcZonePlace>& AcCanyon::Places(const FAcMmoClient& Mmo)
{
	static FString CachedFor;
	static TArray<FAcZonePlace> Cache;
	if (CachedFor == Mmo.WorldPath()) return Cache;
	CachedFor = Mmo.WorldPath();
	Cache.Reset();
	FString Json;
	TSharedPtr<FJsonObject> World;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *CachedFor) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), World) || !World
		|| !World->TryGetArrayField(TEXT("grid"), Rows))
	{
		return Cache;
	}
	for (const TSharedPtr<FJsonValue>& Row : *Rows)
	{
		for (const TSharedPtr<FJsonValue>& Entry : Row->AsArray())
		{
			FString File, Body;
			if (!Entry->TryGetString(File) || !FFileHelper::LoadFileToString(Body, *FPaths::Combine(FPaths::GetPath(CachedFor), File))) continue;
			const ac::region::Parsed Parsed = ac::region::parse(TCHAR_TO_UTF8(*Body));
			if (!Parsed.file) continue;
			const ac::region::RegionFile& F = *Parsed.file;
			for (int32 K = 0; K < int32(F.zones.size()); ++K)
			{
				const ac::region::Zone& Z = F.zones[size_t(K)];
				FAcZonePlace P;
				P.Region = Text(F.id);
				P.RegionName = Text(F.name);
				P.Index = K;
				P.Name = Text(Z.name);
				P.Id = Text(Z.id);
				// As the server: the prize's deposits, else its wells.
				std::vector<ac::Vec2> Prize;
				for (const int64_t I : Z.prizeDeposits)
					if (I >= 0 && size_t(I) < F.deposits.size()) Prize.push_back(F.deposits[size_t(I)].at);
				if (Prize.empty())
					for (const int64_t I : Z.prizeWells)
						if (I >= 0 && size_t(I) < F.wells.size()) Prize.push_back(F.wells[size_t(I)].at);
				for (const ac::Vec2 Q : Prize) P.Middle = P.Middle + Q / double(Prize.size());
				// As the server (Z4: pilots build anywhere in the zone): its
				// outline's farthest corner.
				for (const ac::Vec2 O : Z.outline) P.Reach = FMath::Max(P.Reach, ac::distance(O, P.Middle));
				for (const ac::region::Zone::Entrance& E : Z.entrances)
				{
					P.Entrances.Add(Text(E.name));
					P.Roost.Add(E.roost);
					if (E.roost) P.Roosts.Add(E.at);
				}
				for (const ac::Vec2 Q : Z.flood) P.Flood.Add(Q);
				Cache.Add(P);
			}
		}
	}
	return Cache;
}

FString AcCanyon::IdOf(const FAcMmoClient& Mmo, const FString& Name)
{
	for (const FAcZonePlace& P : Places(Mmo))
		if (P.Name.Equals(Name, ESearchCase::IgnoreCase)) return P.Id;
	return FString();
}

const FAcZonePlace* AcCanyon::Place(const FAcMmoClient& Mmo, const FZone& Z)
{
	const ac::net::WorldMessage* W = Mmo.World();
	if (!W || Z.region < 0 || size_t(Z.region) >= W->regions.size()) return nullptr;
	const FString Region = Text(W->regions[size_t(Z.region)].id);
	for (const FAcZonePlace& P : Places(Mmo))
	{
		if (P.Region == Region && P.Index == int32(Z.zone)) return &P;
	}
	return nullptr;
}

const FZone* AcCanyon::Near(const FAcMmoClient& Mmo)
{
	const ac::net::WorldMessage* W = Mmo.World();
	if (!W || !Mmo.Sky() || W->here < 0 || size_t(W->here) >= W->regions.size()) return nullptr;
	const ac::net::WorldMessage::Region& Here = W->regions[size_t(W->here)];
	const FZone* Next = nullptr;
	for (const FZone& Z : Mmo.Sky()->zones)
	{
		if (Z.region == W->here) return &Z;
		if (Z.region < 0 || size_t(Z.region) >= W->regions.size()) continue;
		const ac::net::WorldMessage::Region& R = W->regions[size_t(Z.region)];
		if (FMath::Abs(R.column - Here.column) + FMath::Abs(R.row - Here.row) == 1 && !Next) Next = &Z;
	}
	return Next;
}

FString AcCanyon::Title(const FZone& Z)
{
	switch (Z.state)
	{
	case FZone::State::waiting: return TEXT("WAVE 1  ·  THE OPENING WAVE");
	case FZone::State::running:
		return FString::Printf(TEXT("WAVE %lld / %lld%s"), (long long)Z.wave, (long long)Z.waves, Z.boss ? TEXT("  ·  MINI-BOSS") : TEXT(""));
	case FZone::State::failed: return TEXT("FALLEN");
	case FZone::State::held: return TEXT("HELD");
	}
	return FString();
}

FString AcCanyon::Line(const FZone& Z, const double Now)
{
	const auto In = [&](const std::optional<double>& At) { return AcProgressHud::Countdown(FMath::Max(0.0, At.value_or(Now) - Now)); };
	switch (Z.state)
	{
	case FZone::State::failed: return TEXT("The Matriarch comes for the turrets");
	case FZone::State::held: return FString::Printf(TEXT("No waves  ·  the prize is yours  ·  waves again in %s"), *In(Z.next));
	case FZone::State::waiting:
	case FZone::State::running: break;
	}
	if (Z.ends)
	{
		return FString::Printf(TEXT("%lld bugs left  ·  %lld / %lld ore taken"), (long long)Z.left, (long long)Z.taken, (long long)Z.mark);
	}
	if (Z.paused || !Z.next) return TEXT("Waiting for a pilot");
	if (Z.state == FZone::State::waiting) return FString::Printf(TEXT("Wave 1 comes in %s  ·  beat it to start"), *In(Z.next));
	return FString::Printf(TEXT("Next wave in %s"), *In(Z.next));
}

FString AcCanyon::Tunnels(const FZone& Z, const FAcZonePlace* Place)
{
	TArray<FString> Names, Roosts;
	for (int32 K = 0; K < 32; ++K)
	{
		if (!(Z.tunnels & (1u << uint32(K)))) continue;
		// Z3: a roost is named as it is ("The Roost"), not as a tunnel.
		if (Place && K < Place->Roost.Num() && Place->Roost[K]) Roosts.Add(Place->Entrances[K]);
		else Names.Add(Place && K < Place->Entrances.Num() ? Short(Place->Entrances[K]) : FString::Printf(TEXT("tunnel %d"), K + 1));
	}
	FString Out;
	if (Names.Num() == 1) Out = TEXT("the ") + Names[0] + TEXT(" tunnel");
	else if (Names.Num() > 1)
	{
		const FString Last = Names.Pop();
		Out = TEXT("the ") + FString::Join(Names, TEXT(", ")) + TEXT(" and ") + Last + TEXT(" tunnels");
	}
	for (const FString& R : Roosts) Out = Out.IsEmpty() ? R : Out + TEXT(" and ") + R;
	return Out.IsEmpty() ? FString(TEXT("the tunnels")) : Out;
}

std::vector<ac::net::atlas::ZoneMark> AcCanyon::Marks(const FAcMmoClient& Mmo)
{
	std::vector<ac::net::atlas::ZoneMark> Out;
	if (!Mmo.Sky()) return Out;
	const double Now = Mmo.Sky()->time;
	for (const FZone& Z : Mmo.Sky()->zones)
	{
		const FAcZonePlace* P = Place(Mmo, Z);
		if (!P) continue;
		ac::net::atlas::ZoneMark M;
		M.region = TCHAR_TO_UTF8(*P->Region);
		M.at = P->Middle;
		M.radius = P->Reach;
		M.title = TCHAR_TO_UTF8(*(P->Name.ToUpper() + TEXT("  ·  ") + Title(Z)));
		M.lines = {TCHAR_TO_UTF8(*(TEXT("Canyon zone  ·  ") + P->RegionName)), TCHAR_TO_UTF8(*Line(Z, Now))};
		M.alert = Z.ends.has_value() || Z.state == FZone::State::failed;
		Out.push_back(M);
	}
	return Out;
}
