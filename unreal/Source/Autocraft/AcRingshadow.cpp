// The ringshadow sweep on screen: see AcRingshadow.h.
#include "AcRingshadow.h"

#include "AcDaylight.h"
#include "AcGasGiant.h"
#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"

#include "Ringshadow.h"

namespace
{
	const TCHAR* const MaterialPath = TEXT("/Game/Sky/M_Ringshadow.M_Ringshadow");
	const TCHAR* const WorldParametersPath = TEXT("/Game/Materials/MPC_AcWorld.MPC_AcWorld");
	/// The band's edges when none runs: far off every region.
	constexpr double FarOff = -1.0e6;
	/// The ring's strip in the sky, half its width (degrees).
	constexpr double RingHalf = 4.0;
	/// What the sun is dimmed to with the camera deep in the band.
	constexpr double SunInBand = 0.3;
}

AAcRingshadow::AAcRingshadow()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Post = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Ringshadow"));
	Post->bUnbound = true;
	Post->Priority = 1.0f;
	Post->bEnabled = false;
	RootComponent = Post;
}

AAcRingshadow* AAcRingshadow::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcRingshadow> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcRingshadow* AAcRingshadow::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcRingshadow* R = Find(World)) return R;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcRingshadow");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcRingshadow>(AAcRingshadow::StaticClass(), FTransform::Identity, Params);
}

void AAcRingshadow::Off()
{
	AAcDaylight::RingshadowSun = 1.0;
	CamCover = 0;
	if (!bOn) return;
	bOn = false;
	Post->bEnabled = false;
	if (WorldParameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepLead"), float(FarOff));
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepTrail"), float(FarOff));
	}
}

void AAcRingshadow::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const TOptional<FAcWorldSky>& Sky = AAcDaylight::WorldSky();
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sky || !Sim || !Sim->IsMmo() || !Sim->Simulation().map)
	{
		Off();
		return;
	}
	if (!bLoaded)
	{
		bLoaded = true;
		WorldParameters = LoadObject<UMaterialParameterCollection>(nullptr, WorldParametersPath);
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, MaterialPath))
		{
			Material = UMaterialInstanceDynamic::Create(Base, this, TEXT("RingshadowMaterial"));
			Post->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, Material));
		}
		else
		{
			UE_LOG(LogAutocraft, Warning, TEXT("ringshadow: no %s (run Tools/Editor/make_ringshadow_material.py)"), MaterialPath);
		}
	}

	const double Time = Sky->Time, Day = Sky->DayLength;
	const ac::Ringshadow::Sweep Next = ac::Ringshadow::next(Time, Day);
	// The radio, a minute before: once a sweep.
	if (Next.start > Time && Next.start - Time <= ac::Ringshadow::warning && FMath::Abs(Announced - Next.start) > 1.0)
	{
		Announced = Next.start;
		UE_LOG(LogAutocraft, Display, TEXT("ringshadow: in %.0f s (sweep at clock %.1f s)"), Next.start - Time, Next.start);
		if (const UAcProgressHudSubsystem* Hud = UAcProgressHudSubsystem::Get(this))
		{
			if (const TSharedPtr<SAcProgressHud> W = Hud->Widget())
			{
				W->Toast(TEXT("KSTR: RINGSHADOW IN 60"), TEXT("Hold your ground  ·  ore drilled under the band pays half again"), FAcHudStyle::Ice(),
					FPlatformTime::Seconds());
			}
		}
	}

	const ac::GroundRect& Bounds = Sim->Simulation().map->bounds;
	const std::optional<ac::Ringshadow::Band> Band = ac::Ringshadow::band(Time, Bounds, Day);
	if (!Band)
	{
		if (Begun >= 0 && FMath::Abs(Ended - Begun) > 1.0)
		{
			Ended = Begun;
			UE_LOG(LogAutocraft, Display, TEXT("ringshadow: sweep over"));
		}
		Off();
		return;
	}
	if (FMath::Abs(Begun - Next.start) > 1.0)
	{
		Begun = Next.start;
		UE_LOG(LogAutocraft, Display, TEXT("ringshadow: sweep begins, %.0f s, toward bearing %.0f°"), ac::Ringshadow::duration,
			ac::Ringshadow::bearing);
	}
	bOn = true;
	Post->bEnabled = Material != nullptr;

	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const ac::Vec2 At = AcSpace::ToSim(Eye);
	CamCover = ac::Ringshadow::cover(*Band, At);
	// Where the camera is across the band: 1 at its leading edge, 0 at its
	// trailing one; the ring's strip covers the sun in between.
	const double Across = (ac::dot(At, Band->dir) - Band->trail) / FMath::Max(1.0, Band->lead - Band->trail);
	const double RingOff = FMath::Clamp((0.5 - Across) * 2.0 * RingHalf, -60.0, 60.0);
	const double SunDim = FMath::Lerp(1.0, SunInBand, CamCover);
	AAcDaylight::RingshadowSun = SunDim;

	if (WorldParameters)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepDirX"), float(Band->dir.x));
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepDirY"), float(Band->dir.y));
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepLead"), float(Band->lead));
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepTrail"), float(Band->trail));
		UKismetMaterialLibrary::SetScalarParameterValue(this, WorldParameters, TEXT("SweepSoft"), float(Band->soft));
	}
	if (Material)
	{
		const AAcDaylight* Daylight = AAcDaylight::Find(GetWorld());
		const FVector Sun = AAcDaylight::SunToward(Daylight ? Daylight->GetHour() : AAcDaylight::HourAt(Time));
		Material->SetScalarParameterValue(TEXT("SunDim"), float(SunDim));
		Material->SetScalarParameterValue(TEXT("CamCover"), float(CamCover));
		Material->SetScalarParameterValue(TEXT("RingOn"), 1.0f);
		Material->SetScalarParameterValue(TEXT("RingOff"), float(RingOff));
		Material->SetScalarParameterValue(TEXT("RingHalf"), float(RingHalf));
		Material->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(FVector3f(Sun)));
		Material->SetVectorParameterValue(TEXT("Giant"), FLinearColor(FVector3f(FAcGasGiant::Direction())));
	}
}
