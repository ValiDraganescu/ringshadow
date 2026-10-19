#include "AcShuttleShip.h"

#include "AcAudioDirector.h"
#include "AcDaylight.h"
#include "AcEffects.h"
#include "AcEffectsBursts.h"
#include "AcEffectsExtension.h"
#include "AcParticles.h"
#include "AcSpace.h"
#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcMusicPlayer.h"
#include "AcPilotPawn.h"
#include "AcPose.h"
#include "AcPoseFerry.h"
#include "AcShuttleView.h"
#include "AcSimSubsystem.h"
#include "AcVisor.h"
#include "AcWeather.h"

#include "Algo/Find.h"
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
#include "Simulation.h"
#include "Types.h"

namespace
{
	TAutoConsoleVariable<int32> CVarGlass(TEXT("ac.Ferry.Glass"), 1,
		TEXT("The walk-in Dropship's windshield rain (1) or none (0)."));
	TAutoConsoleVariable<float> CVarCabinLight(TEXT("ac.Ferry.CabinLight"), 30.f,
		TEXT("The walk-in Dropship's cabin lamps' light (unitless intensity)."));

	/// The presses heard from the pawn, by ship (real times).
	TMap<int64, TPair<double, double>> GPresses;
	/// The ship the driven unit is in this frame (0: none).
	int64 GEarsIn = 0;

	double Cells(const double C) { return AcSpace::ToCm(C); }

	/// The lift jets under the nacelles (the ship's root `Root` where it is
	/// drawn), `Thrust` 0..1: two flames pushed down, a hot core in each, pale
	/// heat rising round them; `Dt` the effects' seconds since the last frame
	/// (moved from F2's flight show).
	void Jets(UAcEffects& Fx, const FTransform& Root, const double Thrust, const double Dt, const int64 Key)
	{
		if (Thrust <= 0.01) return;
		FAcParticles* P = Dt > 0 ? AcEffectsBursts::ParticlesOf(&Fx) : nullptr;
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector Nozzle = Root.TransformPosition(FVector(-122.0, I == 0 ? 198.0 : -198.0, 170.0));
			Fx.Flash(Nozzle - FVector::UpVector * Cells(0.1), 0.5 + 0.9 * Thrust, Fx.Now(), 0.05);
			if (!P) continue;
			FAcEmit Plume;
			Plume.Speed = 3.0 + 5.0 * Thrust;
			Plume.Size = 0.3 + 0.2 * Thrust;
			P->Stream(EAcParticle::Plume, FAcParticles::KeyOf(-32, int32(Key * 8 + 1 + I)), FTransform(FRotator(90, 0, 0), Nozzle), 60 + 140 * Thrust,
				Fx.Now(), Dt, Plume);
			FAcEmit Core;
			Core.Speed = 4.0 + 4.0 * Thrust;
			Core.Size = 0.16 + 0.1 * Thrust;
			P->Stream(EAcParticle::CometJet, FAcParticles::KeyOf(-32, int32(Key * 8 + 3 + I)), FTransform(FQuat::Identity, Nozzle), 50 + 110 * Thrust,
				Fx.Now(), Dt, Core);
			FAcEmit Heat;
			Heat.Size = 0.2;
			P->Stream(EAcParticle::FireflyExhaust, FAcParticles::KeyOf(-32, int32(Key * 8 + 5 + I)), FTransform(FQuat::Identity, Nozzle), 22 * Thrust,
				Fx.Now(), Dt, Heat);
		}
	}

	/// The dust the jets blow off the ground `Height` cells under them: a
	/// ring of it racing out from `Ground` (`Radius` cells), thinner higher.
	void DustRing(UAcEffects& Fx, const FVector& Ground, const double Radius, const double Height)
	{
		const int32 N = 14;
		const double Size = FMath::Lerp(1.0, 0.4, FMath::Clamp(Height / 4.0, 0.0, 1.0));
		const double Turn = FMath::FRand() * UE_TWO_PI;
		for (int32 I = 0; I < N; ++I)
		{
			const double A = Turn + UE_TWO_PI * I / N;
			FAcBurstRequest R;
			R.Kind = EAcBurst::Dust;
			R.At = Ground + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Cells(Radius) + FVector::UpVector * Cells(0.1);
			R.Size = Size;
			R.Time = Fx.Now();
			Fx.Burst(R);
		}
	}

	/// The ring of a jet's blast over the ground.
	void Shock(UAcEffects& Fx, const FVector& Ground, const double Radius, const double Bright)
	{
		FAcShockRingRequest Ring;
		Ring.At = Ground + FVector::UpVector * Cells(0.05);
		Ring.Radius = Cells(Radius);
		Ring.Life = 0.45;
		Ring.Time = Fx.Now();
		Ring.Bright = Bright;
		Fx.ShockRing(Ring);
	}

	/// The Landing Pads a Dropship is over this frame.
	TArray<FVector> GBusyPads;

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

const ac::Wipers* AcShuttleShip::AboardWipers(UWorld* World)
{
	if (!World || GEarsIn == 0) return nullptr;
	for (TActorIterator<AAcShuttleShip> It(World); It; ++It)
	{
		if (It->ShipId == GEarsIn) return &It->Wiper;
	}
	return nullptr;
}

bool AcShuttleShip::PadBusy(const FVector& At)
{
	for (const FVector& P : GBusyPads)
	{
		if (FVector::DistSquaredXY(P, At) < 200.0 * 200.0) return true;
	}
	return false;
}

void AAcShuttleShip::Effects(const ac::Shuttle& S, const FTransform& Placement, const double Thrust, const double Now, const double Dt)
{
	UAcEffects* Fx = UAcEffects::Get(this);
	using EStage = ac::Shuttle::Stage;
	const FVector Pad = AcSpace::ToWorld(S.position, S.ground);
	const bool bFlying = S.stage != EStage::ground;
	if (Fx && bFlying) Jets(*Fx, Placement, Thrust, Dt, ShipId);
	// The moments: the lift-off's blast and scorch, the touch down's dust.
	if (Fx && LastStage >= 0 && LastStage != int32(S.stage))
	{
		if (S.stage == EStage::lift)
		{
			Shock(*Fx, Pad, 4.5, 0.5);
			FAcScorchRequest Burn;
			Burn.At = Pad;
			Burn.Radius = Cells(2.2);
			Burn.Life = 30;
			Burn.Time = Fx->Now();
			Burn.Darkness = 0.6;
			Fx->Scorch(Burn);
		}
		if (S.stage == EStage::ground)
		{
			Shock(*Fx, Pad, 4.5, 0.6);
			DustRing(*Fx, Pad, 2.4, 0);
			Fx->DustPuff(Pad, 1.6, Fx->Now());
		}
	}
	LastStage = int32(S.stage);
	// The dust under the jets near the ground.
	const double Low = AcSpace::ToCells(Placement.GetLocation().Z) - S.ground;
	if (Fx && bFlying && Thrust > 0.3 && Low < 7.0 && Now - LastRing > 0.17)
	{
		LastRing = Now;
		DustRing(*Fx, FVector(Placement.GetLocation().X, Placement.GetLocation().Y, Pad.Z), 1.6 + 0.9 * (7.0 - Low), Low);
	}
	// The dash: puffs of pale vapour flung past the ship, the speed seen.
	if (Fx && S.stage == EStage::dash && Now - LastCloud > 0.05)
	{
		LastCloud = Now;
		if (FAcParticles* P = AcEffectsBursts::ParticlesOf(Fx))
		{
			const FQuat Q = Placement.GetRotation();
			for (int32 K = 0; K < 3; ++K)
			{
				const FVector Off = Q.GetForwardVector() * Cells(FMath::FRandRange(14.0, 40.0)) + Q.GetRightVector() * Cells(FMath::FRandRange(-12.0, 12.0))
					+ FVector::UpVector * Cells(FMath::FRandRange(-6.0, 7.0));
				FAcEmit Puff;
				Puff.Size = FMath::FRandRange(2.5, 5.0);
				Puff.Speed = 0.3;
				Puff.Rate = 40;
				Puff.Duration = 0.05;
				P->Burst(EAcParticle::MuzzleSmoke, FTransform(Placement.GetLocation() + Off), Fx->Now(), Puff);
			}
		}
	}
	// The pad's lamps alternate while it comes down, stands and lifts.
	if (S.stage == EStage::descend || S.stage == EStage::ground || S.stage == EStage::lift || S.stage == EStage::approach || S.stage == EStage::depart)
	{
		GBusyPads.Add(S.stage == EStage::approach ? AcSpace::ToWorld(ac::Vec2(S.to.x, S.to.y), S.to.z) : Pad);
	}
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
	GBusyPads.Reset();
	const double Dt = World->GetDeltaSeconds();
	// The ship the ears ride in (the driven unit in its bay, or riding it in flight).
	int64 EarsIn = 0;
	if (const AAcPilotPawn* Pilot = AAcPilotPawn::Find(World); Pilot && Pilot->Driving() && bLive)
	{
		const ac::Simulation& Sm = Sim->Simulation();
		if (const std::optional<int64_t> I = Sm.unitIndex(Pilot->Driven()))
		{
			const ac::Unit& U = Sm.state.units[size_t(*I)];
			if (const AcShuttleView::FShip* F = AcShuttleView::Aboard(U))
			{
				if (F->Ship.flying() || ac::ShuttleBay::inBay(ac::ShuttleBay::local(F->Ship, U.position))) EarsIn = F->Ship.id;
			}
		}
	}
	GEarsIn = EarsIn;
	std::vector<FAcAudioRules::FShipHear> Heard;

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
			else if (const TPair<int64, AAcShuttleShip*>* Near = Algo::FindByPredicate(Have, [&](const TPair<int64, AAcShuttleShip*>& H)
				{ return !AcShuttleView::Find(H.Key) && FVector::Dist(H.Value->GetActorLocation(), F.Placement.GetLocation()) < 5000.0; }))
			{
				// The same ship under a new number (the target region's copy at
				// the switch): kept, its wipers and its glass's water with it.
				A = Near->Value;
				Have.Remove(Near->Key);
				UE_LOG(LogAutocraft, Log, TEXT("shuttle %lld: now %lld"), (long long)A->ShipId, (long long)S.id);
				A->ShipId = S.id;
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
			// The flight's moments on the music's cue timeline (the radio
			// ducks under them; the siege's shots read them), for the ship
			// the pilot rides.
			if (S.id == EarsIn && A->CuedStage >= 0 && A->CuedStage != int32(S.stage))
			{
				if (UAcMusicPlayer* M = UAcMusicPlayer::Get(World))
				{
					using EStage = ac::Shuttle::Stage;
					if (S.stage == EStage::lift)
					{
						M->Cue(TEXT("flight"), TEXT("flroar"), AcSound::FlightRoarGain);
						M->Cue(TEXT("flight"), TEXT("flcruise on"), AcSound::FlightCruiseGain);
					}
					if (S.stage == EStage::approach) M->Cue(TEXT("flight"), TEXT("flretro"), AcSound::FlightRetroGain);
					if (S.stage == EStage::ground)
					{
						M->Cue(TEXT("flight"), TEXT("flcruise off"), 0.0);
						M->Cue(TEXT("flight"), TEXT("fltouch"), AcSound::FlightTouchGain);
					}
				}
			}
			A->CuedStage = S.id == EarsIn ? int32(S.stage) : -1;
			A->Effects(S, F.Placement, L.Thrust, Now, Dt);
			FAcAudioRules::FShipHear H;
			H.Id = S.id;
			H.At = ac::Vec2(F.Pose.at.x, F.Pose.at.y);
			H.Lift = float(FMath::Max(0.0, F.Pose.at.z - S.ground) + 1.0);
			H.Thrust = float(L.Thrust);
			H.RampWay = L.RampWay;
			H.Stage = uint8(S.stage);
			H.bAboard = S.id == EarsIn;
			H.WiperPasses = A->Wiper.passes();
			H.bWiperDry = A->Wiper.dry();
			Heard.push_back(H);
			const double D = FVector::Dist(Cam, F.Placement.GetLocation());
			if (D < NearestD)
			{
				NearestD = D;
				Nearest = A;
			}
		}
	}
	if (UAcAudioDirector* Audio = UAcAudioDirector::Get(World)) Audio->Rules().Shuttles(Heard);
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
