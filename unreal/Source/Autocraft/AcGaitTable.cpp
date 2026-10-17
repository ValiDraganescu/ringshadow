#include "AcGaitTable.h"

#include "AcLog.h"

#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

FString FAcGaitTable::PathFor(const FName Name)
{
	return FPaths::ProjectContentDir() / TEXT("Models/Gaits") / (Name.ToString() + TEXT(".json"));
}

namespace
{
	bool ReadChannel(const TSharedPtr<FJsonObject>& Table, const TCHAR* Key, TArray<double>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Table->TryGetArrayField(Key, Values) || !Values || Values->IsEmpty()) return false;
		Out.Reset(Values->Num());
		for (const TSharedPtr<FJsonValue>& V : *Values) Out.Add(V->AsNumber());
		return true;
	}
}

bool FAcGaitTable::Load(const FString& Path)
{
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path)) return false;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root) return false;
	const TSharedPtr<FJsonObject>* Table = nullptr;
	if (!Root->TryGetObjectField(TEXT("table"), Table) || !Table) return false;
	Channels.Reset();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Table)->Values)
	{
		TArray<double> Values;
		if (ReadChannel(*Table, *Field.Key, Values)) Channels.Add(FName(Field.Key), MoveTemp(Values));
	}
	if (Channels.IsEmpty()) return false;
	Hip = Channels.FindRef(TEXT("hip"));
	Knee = Channels.FindRef(TEXT("knee"));
	Foot = Channels.FindRef(TEXT("foot"));
	PeriodS = 0.0;
	Root->TryGetNumberField(TEXT("period_s"), PeriodS);
	return true;
}

double FAcGaitTable::At(const FName Channel, const double Phase01) const
{
	const TArray<double>* Found = Channels.Find(Channel);
	return Found ? Sample(*Found, Phase01) : 0.0;
}

const FAcGaitTable* FAcGaitTable::Find(const FName Name)
{
	// Pose functions run on worker threads: one load per name, under a lock.
	static FCriticalSection Lock;
	static TMap<FName, TUniquePtr<FAcGaitTable>> Cache;
	FScopeLock Guard(&Lock);
	if (const TUniquePtr<FAcGaitTable>* Found = Cache.Find(Name)) return Found->Get();
	TUniquePtr<FAcGaitTable> T = MakeUnique<FAcGaitTable>();
	const FString Path = PathFor(Name);
	// -AcNoGait: the procedural gaits, for a before-and-after.
	if (FParse::Param(FCommandLine::Get(), TEXT("AcNoGait")) || !T->Load(Path))
	{
		if (FPaths::FileExists(Path) && !FParse::Param(FCommandLine::Get(), TEXT("AcNoGait")))
		{
			UE_LOG(LogAutocraft, Warning, TEXT("gait table %s does not parse"), *Path);
		}
		T.Reset();
	}
	else
	{
		UE_LOG(LogAutocraft, Log, TEXT("gait table %s: %d channels, period %.2f s"), *Name.ToString(), T->Channels.Num(), T->PeriodS);
	}
	return Cache.Add(Name, MoveTemp(T)).Get();
}

double FAcGaitTable::Sample(const TArray<double>& Channel, const double Phase01)
{
	const int32 N = Channel.Num();
	if (N == 0) return 0.0;
	const double X = (Phase01 - std::floor(Phase01)) * N;
	const int32 I = int32(X) % N;
	const double F = X - std::floor(X);
	return Channel[I] * (1.0 - F) + Channel[(I + 1) % N] * F;
}
