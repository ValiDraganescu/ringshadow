#include "AcQualityTier.h"

#include "AcLog.h"
#include "AcPerfSystem.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"

namespace
{
	AcQualityTier::ETier GCurrent = AcQualityTier::ETier::Top;
	FString GCurrentName = TEXT("top");
	FAutoConsoleVariableRef CVarQualityTier(TEXT("ac.QualityTier"), GCurrentName,
		TEXT("The quality tier this Mac runs (read only; force one with -AcTier= or [AcQuality] Tier= in Engine.ini). docs/perf.md, client round 8."),
		ECVF_ReadOnly);
}

const TCHAR* AcQualityTier::Name(const ETier Tier)
{
	switch (Tier)
	{
	case ETier::Top: return TEXT("top");
	case ETier::High: return TEXT("high");
	case ETier::Mid: return TEXT("mid");
	case ETier::Floor: return TEXT("floor");
	}
	return TEXT("top");
}

TOptional<AcQualityTier::ETier> AcQualityTier::Named(const FString& InName)
{
	for (const ETier T : {ETier::Top, ETier::High, ETier::Mid, ETier::Floor})
		if (InName.Equals(Name(T), ESearchCase::IgnoreCase)) return T;
	return {};
}

double AcQualityTier::GenerationFactor(const FString& GpuName)
{
	// "Apple M3 Pro": the digits after "M".
	const int32 At = GpuName.Find(TEXT("Apple M"), ESearchCase::IgnoreCase);
	if (At == INDEX_NONE) return 0;
	int32 Generation = 0;
	for (int32 I = At + 7; I < GpuName.Len() && FChar::IsDigit(GpuName[I]); ++I) Generation = Generation * 10 + (GpuName[I] - TEXT('0'));
	switch (Generation)
	{
	case 0: return 0;
	case 1: return 1.0;
	case 2: return 1.12;
	case 3: return 1.25;
	case 4: return 1.35;
	default: return 1.45;
	}
}

double AcQualityTier::Score(const FString& GpuName, const int32 Cores)
{
	return Cores > 0 ? Cores * GenerationFactor(GpuName) : 0.0;
}

AcQualityTier::ETier AcQualityTier::FromScore(const double Score)
{
	return Score >= 35 ? ETier::Top : Score >= 18 ? ETier::High : Score >= 9.5 ? ETier::Mid : ETier::Floor;
}

AcQualityTier::ETier AcQualityTier::FromName(const FString& GpuName)
{
	if (GpuName.Contains(TEXT("Max")) || GpuName.Contains(TEXT("Ultra"))) return ETier::Top;
	if (GpuName.Contains(TEXT("Pro"))) return ETier::High;
	return ETier::Mid;
}

AcQualityTier::ETier AcQualityTier::Current() { return GCurrent; }

void AcQualityTier::Apply()
{
	// The editor's own viewport keeps the editor's settings.
	if (GIsEditor) return;
	const FString Gpu = FPlatformMisc::GetPrimaryGPUBrand();
	const int32 Cores = AcPerfSystem::GpuCores();
	const double S = Score(Gpu, Cores);
	ETier Tier = S > 0 ? FromScore(S) : FromName(Gpu);
	FString Reason = S > 0 ? FString::Printf(TEXT("%s, %d GPU cores, score %.1f of the M4 Max's 54"), *Gpu, Cores, S)
						   : FString::Printf(TEXT("%s, cores unknown: by its name"), *Gpu);
	FString Forced;
	if (FParse::Value(FCommandLine::Get(), TEXT("AcTier="), Forced) || (GConfig && GConfig->GetString(TEXT("AcQuality"), TEXT("Tier"), Forced, GEngineIni)))
	{
		if (const TOptional<ETier> F = Named(Forced))
		{
			Reason = FString::Printf(TEXT("forced (%s would be %s: %s)"), *Forced, Name(Tier), *Reason);
			Tier = *F;
		}
		else
		{
			UE_LOG(LogAutocraft, Warning, TEXT("quality: no tier named '%s' (top, high, mid, floor); choosing by the GPU"), *Forced);
		}
	}
	GCurrent = Tier;
	GCurrentName = Name(Tier);
	// The top tier sets nothing; a lower one its fragment's CVars, at the
	// device profile's priority (a console or command-line value still wins).
	int32 Set = 0;
	if (Tier != ETier::Top && GConfig)
	{
		const FString Section = FString::Printf(TEXT("Ac_%s DeviceProfileFragment"), *FString(Name(Tier)).Left(1).ToUpper().Append(FString(Name(Tier)).Mid(1)));
		TArray<FString> CVars;
		GConfig->GetArray(*Section, TEXT("CVars"), CVars, GDeviceProfilesIni);
		for (const FString& Line : CVars)
		{
			FString Key, Value;
			if (!Line.Split(TEXT("="), &Key, &Value)) continue;
			Key.TrimStartAndEndInline();
			Value.TrimStartAndEndInline();
			if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Key))
			{
				V->Set(*Value, ECVF_SetByDeviceProfile);
				++Set;
			}
			else
			{
				UE_LOG(LogAutocraft, Warning, TEXT("quality: %s sets %s, which this build does not have"), *Section, *Key);
			}
		}
	}
	// No [Ac_<Tier> DeviceProfileFragment] yet: the tier is named and logged,
	// and it draws as the top tier does (docs/perf.md, round 8: the sets are
	// still to be measured).
	UE_LOG(LogAutocraft, Log, TEXT("quality: tier %s (%s), %d settings%s"), Name(Tier), *Reason, Set,
		Tier != ETier::Top && Set == 0 ? TEXT(" (none set for this tier yet: it draws as the top tier)") : TEXT(""));
}
