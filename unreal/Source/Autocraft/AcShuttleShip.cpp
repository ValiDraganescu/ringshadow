#include "AcShuttleShip.h"

#include "AcDaylight.h"
#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcPose.h"
#include "AcPoseFerry.h"
#include "AcShuttleView.h"
#include "AcSimSubsystem.h"
#include "AcVisor.h"
#include "AcWeather.h"

#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "Shuttle.h"
#include "Types.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGlass(TEXT("ac.Ferry.Glass"), 1,
		TEXT("The walk-in Dropship's windshield rain (1) or none (0)."));
	TAutoConsoleVariable<float> CVarCabinLight(TEXT("ac.Ferry.CabinLight"), 30.f,
		TEXT("The walk-in Dropship's cabin lamps' light (unitless intensity)."));

	/// The presses heard from the pawn, by ship (real times).
	TMap<int64, TPair<double, double>> GPresses;

	FVector GlassPoint(const double U, const double V)
	{
		using namespace AcShuttleShip;
		const FVector B(GlassBottomX, 0.0, GlassBottomZ), T(GlassTopX, 0.0, GlassTopZ);
		const FVector Up = (T - B).GetSafeNormal();
		return B + FVector(0.0, U, 0.0) + Up * V;
	}
}

double AcShuttleShip::Thrust(const uint8 Stage, const double Clock, const double Seconds)
{
	using S = ac::Shuttle::Stage;
	const double F = Seconds > 0 ? FMath::Clamp(Clock / Seconds, 0.0, 1.0) : 0.0;
	switch (S(Stage))
	{
	case S::descend: return 0.35 + 0.65 * FMath::SmoothStep(0.4, 1.0, F);
	case S::ground: return 0.12;
	case S::lift: return 1.0;
	case S::turns: return 0.65;
	case S::dash: return 1.0;
	case S::approach: return 0.4 + 0.6 * FMath::SmoothStep(0.35, 0.9, F);
	case S::depart: return 1.0;
	}
	return 0.1;
}

void AcShuttleShip::Pressed(const int64 Ship, const bool bTakeOff, const double At)
{
	TPair<double, double>& P = GPresses.FindOrAdd(Ship, TPair<double, double>(-1e9, -1e9));
	(bTakeOff ? P.Value : P.Key) = At;
}

AAcShuttleShip::AAcShuttleShip()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	SetCanBeDamaged(false);
}

bool AAcShuttleShip::Build(const int32 Team)
{
	if (Model) return true;
	Model = FAcModelCatalog::Get().Find(FName(AcPoseFerry::Model));
	if (!Model)
	{
		UE_LOG(LogAutocraft, Warning, TEXT("shuttle: no model %s"), AcPoseFerry::Model);
		return false;
	}
	for (int32 P = 0; P < Model->Parts.Num(); ++P)
	{
		const FAcModelPart& Part = Model->Parts[P];
		for (const FAcModelMesh& X : Part.Meshes)
		{
			UStaticMesh* Mesh = X.LoadMesh();
			if (!Mesh) continue;
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
			C->SetMobility(EComponentMobility::Movable);
			C->SetupAttachment(Root);
			C->SetStaticMesh(Mesh);
			if (UMaterialInterface* Mat = X.LoadMaterial()) C->SetMaterial(0, Mat);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCustomPrimitiveDataFloat(AcModelData::TeamIndex, float(Team));
			C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, 1.f);
			C->RegisterComponent();
			Meshes.Add(C);
			MeshPart.Add(P);
		}
		// The cabin lamps light the bay (warm, short, no shadows).
		if (Part.Name.ToString().StartsWith(TEXT("cabinLamps_")))
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
			L->SetMobility(EComponentMobility::Movable);
			L->SetupAttachment(Root);
			L->SetCastShadows(false);
			L->bUseInverseSquaredFalloff = false;
			L->SetIntensityUnits(ELightUnits::Unitless);
			L->SetLightColor(FLinearColor(1.0f, 0.62f, 0.3f));
			L->SetAttenuationRadius(320.f);
			L->SetSourceRadius(8.f);
			L->RegisterComponent();
			Lights.Add(L);
			LightPart.Add(P);
		}
	}
	// The windshield's rain: a post-process for the ship nearest the camera.
	static UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weather/M_AcWindshield.M_AcWindshield"));
	if (Glass)
	{
		Rain = NewObject<UPostProcessComponent>(this, TEXT("WindshieldRain"), RF_Transient);
		Rain->bUnbound = true;
		Rain->bEnabled = false;
		Rain->SetupAttachment(Root);
		Rain->RegisterComponent();
		RainMaterial = UMaterialInstanceDynamic::Create(Glass, this);
		Rain->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, RainMaterial));
	}
	UE_LOG(LogAutocraft, Log, TEXT("shuttle %lld: %d meshes, %d lights"), (long long)ShipId, Meshes.Num(), Lights.Num());
	return true;
}

void AAcShuttleShip::Apply(const FTransform& Placement, const FAcFerryLook& Look)
{
	if (!Model) return;
	FAcPose P;
	P.Reset(*Model);
	AcPoseFerry::Pose(*Model, P, Look);
	Root->SetWorldTransform(Placement);
	const int32 N = Model->Parts.Num();
	TArray<FTransform> ToRoot;
	ToRoot.SetNumUninitialized(N);
	TArray<bool> Shown;
	Shown.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		const int32 Parent = Model->Parts[I].Parent;
		ToRoot[I] = Parent == INDEX_NONE ? P.Local[I] : P.Local[I] * ToRoot[Parent];
		Shown[I] = P.Visible[I] && (Parent == INDEX_NONE || Shown[Parent]);
	}
	for (int32 K = 0; K < Meshes.Num(); ++K)
	{
		UStaticMeshComponent* C = Meshes[K];
		const int32 I = MeshPart[K];
		C->SetRelativeTransform(ToRoot[I]);
		C->SetVisibility(Shown[I]);
		C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, P.Emission[I]);
	}
	const float Light = CVarCabinLight.GetValueOnGameThread();
	for (int32 K = 0; K < Lights.Num(); ++K)
	{
		Lights[K]->SetRelativeLocation(ToRoot[LightPart[K]].GetLocation() - FVector(0, 0, 12));
		Lights[K]->SetIntensity(Light * P.Emission[LightPart[K]]);
	}
}

void AAcShuttleShip::Glass(FVector& Origin, FVector& Across, FVector& Up, FVector& Right, FVector& Left) const
{
	using namespace AcShuttleShip;
	const FTransform T = Root->GetComponentTransform();
	Origin = T.TransformPosition(GlassPoint(0.0, 0.0));
	Across = T.TransformVector(FVector(0, 1, 0)).GetSafeNormal();
	Up = T.TransformVector(GlassPoint(0.0, 1.0) - GlassPoint(0.0, 0.0)).GetSafeNormal();
	Right = T.TransformVector(FVector(-SideBack, SideOut - SideFrom, 0)).GetSafeNormal();
	Left = T.TransformVector(FVector(-SideBack, -(SideOut - SideFrom), 0)).GetSafeNormal();
}

void AcShuttleShip::Tick(UWorld* World)
{
	if (!World) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World);
	AcShuttleView::Update(World);
	TMap<int64, AAcShuttleShip*> Have;
	for (TActorIterator<AAcShuttleShip> It(World); It; ++It) Have.Add(It->ShipId, *It);
	const bool bLive = Sim && Sim->IsRunning() && Sim->IsMmo();
	const double Now = FPlatformTime::Seconds();
	const double T = World->GetTimeSeconds();

	// The weather the glass gets, and the camera (the nearest ship's glass is drawn).
	AAcWeather* Weather = AAcWeather::Find(World);
	const AAcDaylight* Daylight = AAcDaylight::Find(World);
	const double Rate = Weather ? FMath::Clamp(Weather->Last().rain, 0.0, 1.0) : 0.0;
	const double Flash = Weather ? Weather->Last().flash : 0.0;
	const double DayLight = (Daylight ? 1.0 - Daylight->GetDark() : 1.0) * (1.0 - 0.3 * (Weather ? Weather->Last().gloom : 0.0));
	const APlayerController* PC = World->GetFirstPlayerController();
	const FVector Cam = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	AAcShuttleShip* Nearest = nullptr;
	double NearestD = AcShuttleShip::GlassRange;

	if (bLive)
	{
		for (const AcShuttleView::FShip& F : AcShuttleView::Ships())
		{
			const ac::Shuttle& S = F.Ship;
			AAcShuttleShip* A = nullptr;
			if (AAcShuttleShip** Found = Have.Find(S.id))
			{
				A = *Found;
				Have.Remove(S.id);
			}
			else
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				A = World->SpawnActor<AAcShuttleShip>(Params);
				if (!A) continue;
				A->ShipId = S.id;
				if (!A->Build(0))
				{
					A->Destroy();
					continue;
				}
				UE_LOG(LogAutocraft, Log, TEXT("shuttle %lld: drawn"), (long long)S.id);
			}
			// The ship's own wiper, on the rain where it is.
			A->Wiper.update(T, Rate);
			const TPair<double, double>* Press = GPresses.Find(S.id);
			FAcFerryLook L;
			L.Time = T;
			L.RampOpen = S.ramp.open;
			L.RampWay = (S.ramp.opening && S.ramp.open < 1.0) ? 1 : (!S.ramp.opening && S.ramp.open > 0.0 ? -1 : 0);
			L.RampPressAge = Press ? Now - Press->Key : 1e9;
			L.TakeoffPressAge = Press ? Now - Press->Value : 1e9;
			L.bCanTakeOff = S.grounded() && S.ramp.open <= 0.0 && !S.ramp.opening;
			L.bAboard = !S.riders.empty() || S.boarded;
			L.WiperAngle = A->Wiper.angle();
			L.Thrust = AcShuttleShip::Thrust(uint8(S.stage), F.Clock, ac::shuttleStageSeconds(S, S.stage));
			A->Apply(F.Placement, L);
			const double D = FVector::Dist(Cam, F.Placement.GetLocation());
			if (D < NearestD)
			{
				NearestD = D;
				Nearest = A;
			}
		}
	}
	for (const TPair<int64, AAcShuttleShip*>& Gone : Have)
	{
		UE_LOG(LogAutocraft, Log, TEXT("shuttle %lld: gone"), (long long)Gone.Key);
		GPresses.Remove(Gone.Key);
		Gone.Value->Destroy();
	}

	// The windshield's rain on the nearest ship's glass.
	for (TActorIterator<AAcShuttleShip> It(World); It; ++It)
	{
		AAcShuttleShip* A = *It;
		if (!A->Rain || !A->RainMaterial) continue;
		FAcVisorParams P = AcVisor::Params(A->Wiper, 0.0, Flash, DayLight);
		const bool bOn = A == Nearest && P.bWet && CVarGlass.GetValueOnGameThread() != 0;
		if (A->Rain->bEnabled != bOn) A->Rain->bEnabled = bOn;
		if (!bOn) continue;
		// The arc on this glass: the pivots' height down from its top and the
		// reach, in pane heights; the blades' park and far stops.
		using namespace AcShuttleShip;
		const double H = FVector::Dist(FVector(GlassBottomX, 0, GlassBottomZ), FVector(GlassTopX, 0, GlassTopZ));
		const double Park = FMath::DegreesToRadians(AcPoseFerry::WiperParkDeg), Far = FMath::DegreesToRadians(AcPoseFerry::WiperFarDeg);
		P.Arc = FLinearColor(float((H - WiperPivotV) / H), float(WiperReach / H), float(Park), float(Far));
		P.Blade.R = float(AcPoseFerry::WiperOnGlass(A->Wiper.angle()));
		P.Blade.A = float(0.6 / H);
		FVector O, U, V, R, Lt;
		A->Glass(O, U, V, R, Lt);
		const double SideLen = FVector(SideBack, SideOut - SideFrom, 0).Size();
		UMaterialInstanceDynamic* M = A->RainMaterial;
		M->SetScalarParameterValue(TEXT("LensRain"), P.Rain);
		M->SetVectorParameterValue(TEXT("GlassA"), P.A);
		M->SetVectorParameterValue(TEXT("GlassB"), P.B);
		M->SetVectorParameterValue(TEXT("GlassSeed"), P.Seed);
		M->SetVectorParameterValue(TEXT("GlassArc"), P.Arc);
		M->SetVectorParameterValue(TEXT("GlassBlade"), P.Blade);
		M->SetVectorParameterValue(TEXT("GlassHist"), P.Hist);
		M->SetVectorParameterValue(TEXT("GlassAir"), P.Air);
		M->SetVectorParameterValue(TEXT("GlassPivots"), FLinearColor(float(WiperLeftU / H), float(WiperRightU / H), float(PaneHalf / H), 0.f));
		M->SetVectorParameterValue(TEXT("GlassOrigin"), FLinearColor(float(O.X), float(O.Y), float(O.Z), float(PaneHalf)));
		M->SetVectorParameterValue(TEXT("GlassAcross"), FLinearColor(float(U.X), float(U.Y), float(U.Z), float(H)));
		M->SetVectorParameterValue(TEXT("GlassUp"), FLinearColor(float(V.X), float(V.Y), float(V.Z), float(SideFrom)));
		M->SetVectorParameterValue(TEXT("GlassRight"), FLinearColor(float(R.X), float(R.Y), float(R.Z), float(SideLen)));
		M->SetVectorParameterValue(TEXT("GlassLeft"), FLinearColor(float(Lt.X), float(Lt.Y), float(Lt.Z), float(SideLen)));
	}
}
