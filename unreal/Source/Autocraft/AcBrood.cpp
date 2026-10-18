// The Brood's raids told to the pilot: see AcBrood.h.
#include "AcBrood.h"

#include "AcFind.h"

#include "AcAudioDirector.h"
#include "AcEffects.h"
#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcLeveling.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "Brood.h"
#include "NetMirror.h"
#include "NetProtocol.h"
#include "Simulation.h"
#include "Starfall.h"

namespace
{
	/// The swarm's gain in the pilot's ear (the KSTR radio's toast with it).
	constexpr float SwarmGain = 0.7f;
	/// The Matriarch's roar: full within `RoarNear` cells, down to `RoarFar`
	/// at `RoarAcross` (her region's width); next door, `RoarNextDoor`. The
	/// view shakes within `RoarShakeCells`, for the roar's 2 s.
	constexpr float RoarFull = 0.9f;
	constexpr double RoarNear = 20.0;
	constexpr double RoarAcross = 150.0;
	constexpr float RoarFar = 0.3f;
	constexpr float RoarNextDoor = 0.15f;
	constexpr double RoarShakeCells = 35.0;

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
	// Every frame: the Matriarch's strikes are drawn as they come.
	PrimaryActorTick.TickInterval = 0.f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

AAcBrood* AAcBrood::Find(const UWorld* World)
{
	return AcFindActor<AAcBrood>(World);
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

float AAcBrood::RoarGain(const double Cells, const bool bNextDoor)
{
	if (bNextDoor) return RoarNextDoor;
	const double T = FMath::Clamp((Cells - RoarNear) / (RoarAcross - RoarNear), 0.0, 1.0);
	return float(FMath::Lerp(double(RoarFull), double(RoarFar), T));
}

double AAcBrood::RoarShake(const double Cells) { return Cells < RoarShakeCells ? 0.9 * (1.0 - Cells / RoarShakeCells) + 0.1 : 0.0; }

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
	// The Matriarch's strikes: the slam's crash after her forelimbs rise,
	// the spit's glob and puddle at once.
	if (M)
	{
		UAcEffects* Fx = UAcEffects::Get(this);
		const ac::GameState& St = M->simulation().state;
		TSet<int64> Seen;
		for (const ac::Unit& U : St.units)
		{
			if (U.kind != ac::UnitKind::matriarch || !U.bug) continue;
			Seen.Add(U.id);
			int64& Was = QueenStrikes.FindOrAdd(U.id, -1);
			const bool bNew = Was >= 0 && U.bug->strikes != Was && U.hp > 0;
			Was = U.bug->strikes;
			if (!bNew || !Fx) continue;
			const double Now = Fx->Now();
			if (ac::Matriarch::spat(U.bug->sac))
			{
				std::optional<ac::Vec2> To;
				double Height = 0;
				if (U.target)
				{
					if (const auto V = St.unit(*U.target))
					{
						To = V->position;
						if (V->bug) Height = V->bug->height;
					}
					else if (const auto B = St.structure(*U.target)) To = B->position;
				}
				if (To) Fx->MatriarchSpit(U.position, U.heading, *To, Height, Now);
			}
			else
			{
				const ac::Vec2 Front = U.position + ac::Vec2(std::cos(U.heading), std::sin(U.heading)) * (ac::Rules::radius(U.kind) + 1.6);
				TWeakObjectPtr<UAcEffects> Weak(Fx);
				Fx->After(ac::Matriarch::slamWindup, Now, [Weak, Front, Now]()
				{
					if (UAcEffects* E = Weak.Get()) E->MatriarchSlam(Front, Now + ac::Matriarch::slamWindup);
				});
			}
		}
		for (auto It = QueenStrikes.CreateIterator(); It; ++It)
			if (!Seen.Contains(It.Key())) It.RemoveCurrent();
	}
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
		// The banner slot (U1): one at a time at the top, never over the action.
		if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this)) L->ShowBanner(Title(), Sub, FString(), false, FAcHudStyle::Srgb(1, 0.35, 0.25));
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->PlayMenu("broodswarm", SwarmGain);
	}
	// The Matriarch's roars: heard in her region and next door.
	for (const ac::net::Sky::Roar& R : Sky->roars)
	{
		if (Roared.Contains(R.id) || R.region < 0 || R.region >= int64(World->regions.size())) continue;
		const ac::net::WorldMessage::Region& There = World->regions[size_t(R.region)];
		const int64 Off = FMath::Abs(There.column - Here.column) + FMath::Abs(There.row - Here.row);
		if (Off > 1) continue;
		Roared.Add(R.id);
		const double Cells = Off == 0 && Me ? ac::length(R.position - Me->position) : 1e9;
		const float Gain = RoarGain(Cells, Off != 0);
		UE_LOG(LogAutocraft, Display, TEXT("brood: the Matriarch roars (roar %u, %.0f cells off%s, gain %.2f)"), R.id, Off == 0 ? Cells : -1.0,
			Off == 0 ? TEXT("") : TEXT(", next door"), Gain);
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->PlayMenu("matriarchroar", Gain);
		if (const double Shake = Off == 0 ? RoarShake(Cells) : 0.0; Shake > 0.0)
			if (UAcEffects* Fx = UAcEffects::Get(this)) Fx->Rumble(R.position, Fx->Now(), Shake, RoarShakeCells * 2, ac::Matriarch::roarFor);
	}
}
