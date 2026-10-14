// The Brood's raids told to the pilot: see AcBrood.h.
#include "AcBrood.h"

#include "AcAudioDirector.h"
#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "NetMirror.h"
#include "NetProtocol.h"
#include "Simulation.h"
#include "Starfall.h"

namespace
{
	/// The swarm's gain in the pilot's ear (the KSTR radio's toast with it).
	constexpr float SwarmGain = 0.7f;

	const TCHAR* Compass(const double Bearing)
	{
		static const TCHAR* const Points[8] = {TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"), TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW")};
		const int32 I = int32(FMath::RoundToInt(FMath::Fmod(Bearing + 360.0, 360.0) / 45.0)) % 8;
		return Points[I];
	}
}

AAcBrood::AAcBrood()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AAcBrood* AAcBrood::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcBrood> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcBrood* AAcBrood::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcBrood* B = Find(World)) return B;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcBrood");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcBrood>(AAcBrood::StaticClass(), FTransform::Identity, Params);
}

FString AAcBrood::Title() { return TEXT("KSTR: BROOD SWARM"); }

FString AAcBrood::Line(const FString& Building, const double Metres, const double Bearing, const FString& Region)
{
	if (!Region.IsEmpty()) return FString::Printf(TEXT("heading for the %s  ·  in %s"), *Building, *Region);
	const FString Far = Metres < 1000 ? FString::Printf(TEXT("%.0f m"), Metres) : FString::Printf(TEXT("%.1f km"), Metres / 1000);
	return FString::Printf(TEXT("heading for the %s  ·  %s %s"), *Building, *Far, Compass(Bearing));
}

void AAcBrood::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	if (!Mmo) return;
	const std::optional<ac::net::Sky>& Sky = Mmo->Sky();
	const ac::net::WorldMessage* World = Mmo->World();
	if (!Sky || !World || World->here < 0 || World->here >= int64(World->regions.size())) return;
	const ac::net::WorldMessage::Region& Here = World->regions[size_t(World->here)];
	const ac::net::Mirror* M = Mmo->Mirror();
	const ac::Unit* Me = M && M->driven() ? M->unit(*M->driven()) : nullptr;
	for (const ac::net::Sky::Raid& R : Sky->raids)
	{
		if (Told.Contains(R.id) || R.region < 0 || R.region >= int64(World->regions.size())) continue;
		const ac::net::WorldMessage::Region& There = World->regions[size_t(R.region)];
		// Its region, or one next to it (not across a corner).
		const int64 Off = FMath::Abs(There.column - Here.column) + FMath::Abs(There.row - Here.row);
		if (Off > 1) continue;
		Told.Add(R.id);
		const FString Building = UTF8_TO_TCHAR(ac::Simulation::title(R.kind).c_str());
		double Metres = 0, Bearing = 0;
		if (Off == 0 && Me)
		{
			Metres = ac::length(R.at - Me->position);
			Bearing = ac::Starfall::bearing(Me->position, R.at);
		}
		const FString Region = Off == 0 ? FString() : FString(UTF8_TO_TCHAR(There.name.c_str()));
		const FString Sub = Line(Building, Metres, Bearing, Region);
		UE_LOG(LogAutocraft, Display, TEXT("brood: %s, %s (raid %u on building %lld, arrives at clock %.1f s)"), *Title(), *Sub, R.id,
			(long long)R.building, R.arrives);
		if (const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			if (const TSharedPtr<SAcProgressHud> W = Hud->Widget()) W->Toast(Title(), Sub, FAcHudStyle::Srgb(1, 0.35, 0.25), FPlatformTime::Seconds());
		}
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->PlayMenu("broodswarm", SwarmGain);
	}
}
