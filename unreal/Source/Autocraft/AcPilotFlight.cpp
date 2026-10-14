// The MMORPG world's pad flight as the player sees it: the ship
// (`AAcFlightShip`) and the pilot pawn's flight view (AcPilotFlight.h).
#include "AcPilotFlight.h"

#include "AcAudioDirector.h"
#include "AcEffects.h"
#include "AcEffectsBursts.h"
#include "AcEffectsExtension.h"
#include "AcGasGiant.h"
#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcParticles.h"
#include "AcPilotPawn.h"
#include "AcSpace.h"
#include "AcWorldRenderer.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"

namespace
{
	FVector Up() { return FVector::UpVector; }
	/// The flight's clock: real seconds, as the server's flight runs (the
	/// pawn's `Clock` adds up frame times, short of a slow frame's).
	double Wall() { return FPlatformTime::Seconds(); }
	double Cells(const double C) { return AcSpace::ToCm(C); }

	/// The view's way on the ground (a view straight down: +X).
	FVector Flat(const FQuat& Q)
	{
		FVector F = Q.GetForwardVector();
		F.Z = 0;
		return F.SizeSquared() > 1e-6 ? F.GetSafeNormal() : FVector::ForwardVector;
	}

	FQuat LookAt(const FVector& From, const FVector& To)
	{
		const FVector D = To - From;
		return D.SizeSquared() > 1.0 ? D.Rotation().Quaternion() : FQuat::Identity;
	}

	/// Toward the giant on the ground (the flight's way), and across it.
	FVector GiantWay()
	{
		FVector G = FAcGasGiant::Direction();
		G.Z = 0;
		return G.SizeSquared() > 1e-6 ? G.GetSafeNormal() : FVector::ForwardVector;
	}
	FVector Across(const FVector& Way) { return FVector::CrossProduct(FVector::UpVector, Way); }

	/// The jets under the ship (`Body`: its ground point lifted), `Thrust`
	/// 0..1: two flames pushed down out of its belly, a hot core in each, a
	/// glow, pale heat rising round them; `Dt` the effects' seconds since the
	/// last frame. Particles stream on the effects' clock (game time).
	void Jets(UAcEffects& Fx, const FVector& Body, const FQuat& Turn, const double Thrust, const double Dt)
	{
		if (Thrust <= 0.01) return;
		const FVector Side = Turn.GetRightVector();
		const FVector Nozzles[2] = {Body + Side * Cells(0.42) + FVector::UpVector * Cells(0.22),
			Body - Side * Cells(0.42) + FVector::UpVector * Cells(0.22)};
		FAcParticles* P = Dt > 0 ? AcEffectsBursts::ParticlesOf(&Fx) : nullptr;
		for (int32 I = 0; I < 2; ++I)
		{
			Fx.Flash(Nozzles[I] - FVector::UpVector * Cells(0.1), 0.5 + 0.9 * Thrust, Fx.Now(), 0.05);
			if (!P) continue;
			// The plume flies along its frame's -X: pitched up a quarter
			// turn, straight down. The jet's core flies down as it is.
			FAcEmit Plume;
			Plume.Speed = 3.0 + 5.0 * Thrust;
			Plume.Size = 0.22 + 0.16 * Thrust;
			P->Stream(EAcParticle::Plume, FAcParticles::KeyOf(-31, 1 + I), FTransform(FRotator(90, 0, 0), Nozzles[I]), 60 + 140 * Thrust, Fx.Now(), Dt,
				Plume);
			FAcEmit Core;
			Core.Speed = 4.0 + 4.0 * Thrust;
			Core.Size = 0.12 + 0.08 * Thrust;
			P->Stream(EAcParticle::CometJet, FAcParticles::KeyOf(-31, 3 + I), FTransform(FQuat::Identity, Nozzles[I]), 50 + 110 * Thrust, Fx.Now(),
				Dt, Core);
			// The heat over the flame: a pale haze drifting up and away.
			FAcEmit Heat;
			Heat.Size = 0.16;
			P->Stream(EAcParticle::FireflyExhaust, FAcParticles::KeyOf(-31, 5 + I), FTransform(FQuat::Identity, Nozzles[I]), 22 * Thrust, Fx.Now(),
				Dt, Heat);
		}
	}

	/// A flight's moment heard (F1b), and the cruise's loop on or off.
	void Hear(const UObject* Ctx, const FAcAudioRules::EFlight Moment)
	{
		if (UAcAudioDirector* A = UAcAudioDirector::Get(Ctx)) A->Rules().Flight(Moment);
	}
	void Cruise(const UObject* Ctx, const bool bOn)
	{
		if (UAcAudioDirector* A = UAcAudioDirector::Get(Ctx)) A->Rules().Cruise(bOn);
	}

	/// The dust the jets blow off the ground `Height` cells under them: a
	/// ring of it racing out from `Ground` (`Radius` cells), thinner higher.
	void DustRing(UAcEffects& Fx, const FVector& Ground, const double Radius, const double Height)
	{
		const int32 N = 12;
		const double Size = FMath::Lerp(0.9, 0.35, FMath::Clamp(Height / 4.0, 0.0, 1.0));
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

	/// The ring of a jet's blast over the ground, and the pad's scorch.
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
}

// MARK: - The ship

AAcFlightShip::AAcFlightShip()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
	SetCanBeDamaged(false);
}

AAcFlightShip* AAcFlightShip::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (TActorIterator<AAcFlightShip> It(World); It) return *It;
	FActorSpawnParameters P;
	P.Name = TEXT("AcFlightShip");
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAcFlightShip* Ship = World->SpawnActor<AAcFlightShip>(P);
	if (Ship) Ship->Hide();
	return Ship;
}

void AAcFlightShip::Build(const FName Model, const int32 Team)
{
	for (UStaticMeshComponent* C : Meshes)
	{
		if (C) C->DestroyComponent();
	}
	Meshes.Reset();
	Key = Model;
	KeyTeam = Team;
	const FAcModelInfo* Info = FAcModelCatalog::Get().Find(Model);
	if (!Info)
	{
		UE_LOG(LogAutocraft, Warning, TEXT("flight: no model %s"), *Model.ToString());
		return;
	}
	// The model at rest, as the model row draws it: its parts that show (a
	// hidden part, or one under it, stays out; so do the see-through
	// smears), its materials, its team's colour.
	for (int32 P = 0; P < Info->Parts.Num(); ++P)
	{
		bool bHidden = false;
		for (int32 Q = P; Q != INDEX_NONE; Q = Info->Parts[Q].Parent) bHidden |= Info->Parts[Q].bHidden;
		const FAcModelPart& Part = Info->Parts[P];
		if (bHidden || Part.Opacity < 0.999f) continue;
		const FTransform Rest = Info->RestToModel(P);
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
			C->SetRelativeTransform(Rest);
			C->RegisterComponent();
			Meshes.Add(C);
		}
	}
	UE_LOG(LogAutocraft, Log, TEXT("flight: ship %s, team %d, %d meshes"), *Model.ToString(), Team, Meshes.Num());
}

bool AAcFlightShip::Show(const FName Model, const int32 Team, const FTransform& Placement, const double Lift)
{
	if (Key != Model || KeyTeam != Team) Build(Model, Team);
	if (Meshes.Num() == 0)
	{
		Hide();
		return false;
	}
	Root->SetWorldLocationAndRotation(Placement.GetLocation() + FVector(0, 0, Lift), Placement.GetRotation());
	if (!bShown)
	{
		bShown = true;
		Root->SetVisibility(true, true);
	}
	return true;
}

void AAcFlightShip::SetGlow(const double Glow)
{
	for (UStaticMeshComponent* C : Meshes)
	{
		if (C) C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, float(Glow));
	}
}

void AAcFlightShip::Hide()
{
	bShown = false;
	Root->SetVisibility(false, true);
}

// MARK: - The pilot pawn's flight view

EAcFlightStage AAcPilotPawn::FlightStage() const
{
	if (!Flight) return EAcFlightStage::None;
	if (Flight->LandStart >= 0) return EAcFlightStage::Landing;
	return Wall() - Flight->Start < AcPilotFlight::LiftSeconds ? EAcFlightStage::LiftOff : EAcFlightStage::Transit;
}

double AAcPilotPawn::FlightSeconds() const
{
	return Flight ? Wall() - Flight->Start : -1.0;
}

double AAcPilotPawn::LandingSeconds() const
{
	return Flight && Flight->LandStart >= 0 ? Wall() - Flight->LandStart : -1.0;
}

bool AAcPilotPawn::StartFlight(const int64 Visit)
{
	if (!LastPlacement || LastModel.IsNone())
	{
		UE_LOG(LogAutocraft, Log, TEXT("flight: lift-off with no unit seen: the view holds"));
		return false;
	}
	FFlightShow F;
	F.Start = Wall();
	F.From = *LastPlacement;
	// The view as it was drawn last (the pilot's eye, a chase camera, a dive).
	F.EyeFrom = GetActorLocation();
	F.RotFrom = GetActorQuat();
	F.Model = LastModel;
	F.Team = LastTeam;
	F.Visit = Visit;
	Flight = F;
	Dive.Reset();
	PullOut.Reset();
	Hear(this, FAcAudioRules::EFlight::Spool);
	// The cockpit off, the ship drawn.
	ApplyView();
	UE_LOG(LogAutocraft, Log, TEXT("flight: lift-off at %s (%s)"), *F.From.GetLocation().ToCompactString(), *F.Model.ToString());
	return true;
}

bool AAcPilotPawn::FlightView(FVector& At, FQuat& Rot, const FTransform* Landing, const int64 Visit)
{
	using namespace AcPilotFlight;
	if (!Flight) return false;
	AAcFlightShip* Ship = AAcFlightShip::SpawnFor(GetWorld());
	UAcEffects* Fx = UAcEffects::Get(this);
	// The effects' seconds since the last frame (the jets' streams).
	double Dt = 0;
	if (Fx)
	{
		Dt = Flight->LastFx >= 0 ? FMath::Clamp(Fx->Now() - Flight->LastFx, 0.0, 0.1) : 0.0;
		Flight->LastFx = Fx->Now();
	}
	const FVector Way = GiantWay();
	const FVector Side = Across(Way);
	// The ground under a point, cm (the camera kept over it).
	auto Above = [&](FVector P, const double Cell)
	{
		if (Fx) P.Z = FMath::Max(P.Z, Fx->GroundZ(AcSpace::ToSim(P)) + Cells(Cell));
		return P;
	};

	if (Landing)
	{
		if (Flight->LandStart < 0)
		{
			Flight->LandStart = Wall();
			Flight->LastRing = -1;
			if (const UAcWorldRenderer* R = UAcWorldRenderer::Get(this))
			{
				if (const FAcModelInfo* Model = R->ModelOf(DrivenId)) Flight->Model = Model->Name;
			}
			ApplyView();
			Hear(this, FAcAudioRules::EFlight::Retro);
			UE_LOG(LogAutocraft, Log, TEXT("flight: landing at %s, %.1f s after the lift-off"), *Landing->GetLocation().ToCompactString(),
				Wall() - Flight->Start);
		}
		const double L = Wall() - Flight->LandStart;
		const FVector Pad = Landing->GetLocation();
		// The cruise's wind gives way to the retro thrust.
		Cruise(this, L < 0.4 * LandSeconds);
		if (L >= LandSeconds)
		{
			if (Fx)
			{
				// Touch down: the ring of the blast, the dust thrown out.
				Shock(*Fx, Pad, 3.5, 0.6);
				DustRing(*Fx, Pad, 1.6, 0);
				Fx->DustPuff(Pad, 1.2, Fx->Now());
			}
			Hear(this, FAcAudioRules::EFlight::Touchdown);
			UE_LOG(LogAutocraft, Log, TEXT("flight: touched down, %.1f s after the lift-off"), Wall() - Flight->Start);
			EndFlight();
			return false;
		}
		// Down on its jets along the way it came, slowing over the pad.
		const double H = LandHeight(L);
		const FVector Body = Pad + Up() * Cells(H) - Way * Cells(LandDrift(L));
		FTransform Place = *Landing;
		Place.SetLocation(Pad - Way * Cells(LandDrift(L)));
		if (Ship)
		{
			Ship->Show(Flight->Model, Flight->Team, Place, Cells(H));
			Ship->SetGlow(1.0 + 2.5 * (1.0 - FMath::SmoothStep(0.6, 1.0, L / LandSeconds)));
		}
		if (Fx)
		{
			// Retro thrust: full as it comes in, easing at the pad.
			Jets(*Fx, Body, Place.GetRotation(), 1.0 - 0.6 * FMath::SmoothStep(0.5, 1.0, L / LandSeconds), Dt);
			if (H < 4.0 && Wall() - Flight->LastRing > 0.18)
			{
				Flight->LastRing = Wall();
				DustRing(*Fx, Pad, 1.0 + 0.8 * (4.0 - H), H);
			}
		}
		// From behind and beside it (the way it came), down into the
		// pilot's own view.
		const double S = L / LandSeconds;
		const FVector Orbit = Above(Body - Way * Cells(11) + Side * Cells(5) + Up() * Cells(4), 1.5);
		const FVector Pos = FMath::Lerp(Orbit, At, Ease(S));
		// The ship left of the middle as it comes down: the arrival's toasts
		// (a region discovered, a first landing) stand in the middle.
		const FQuat On = LookAt(Pos, Body + Up() * Cells(0.6));
		const FQuat Left = LookAt(Pos, Body + Up() * Cells(0.6) + On.GetRightVector() * Cells(0.22) * FVector::Dist(Pos, Body) / Cells(1));
		Rot = FQuat::Slerp(Left, Rot, FMath::SmoothStep(0.5, 1.0, S));
		At = Pos;
		SetFade(LandFade(L));
		return true;
	}

	// In the air: spooled up on the pad, then off it, up and away toward
	// the giant.
	const double T = Wall() - Flight->Start;
	const double H = LiftHeight(T);
	const FVector Ground = Flight->From.GetLocation();
	const FVector Body = Ground + Up() * Cells(H) + Way * Cells(Drift(T));
	// A RegionChange read, its first snapshot not yet: the old region is
	// gone from under the ship; dark until the landing.
	const bool bGone = Visit != Flight->Visit;
	// The roar as the jets take hold; the cruise's wind once it climbs away.
	if (!Flight->bRoared && T > SpoolSeconds)
	{
		Flight->bRoared = true;
		Hear(this, FAcAudioRules::EFlight::LiftOff);
	}
	Cruise(this, T > 0.6 * LiftSeconds);
	if (Ship)
	{
		if (bGone) Ship->Hide();
		else
		{
			FTransform Place = Flight->From;
			Place.SetLocation(Ground + Way * Cells(Drift(T)));
			Ship->Show(Flight->Model, Flight->Team, Place, Cells(H));
			// The engines lit: the glow climbs through the spool-up.
			Ship->SetGlow(1.0 + 3.0 * LiftThrust(T));
			// A shudder on the pad while the jets spool up.
			if (T < SpoolSeconds + 0.3)
			{
				const double Shake = Cells(0.025) * FMath::SmoothStep(0.0, SpoolSeconds, T) * (1.0 - FMath::SmoothStep(SpoolSeconds, SpoolSeconds + 0.3, T));
				Ship->Show(Flight->Model, Flight->Team, Place, Cells(H) + Shake * FMath::Sin(T * 90.0));
			}
		}
	}
	if (Fx && !bGone)
	{
		Jets(*Fx, Body, Flight->From.GetRotation(), LiftThrust(T), Dt);
		// The pad scorched and a shock ring off it as the jets take hold.
		if (!Flight->bLiftRing && T > 0.25)
		{
			Flight->bLiftRing = true;
			Shock(*Fx, Ground, 3.0, 0.5);
		}
		if (!Flight->bScorched && T > SpoolSeconds)
		{
			Flight->bScorched = true;
			FAcScorchRequest Burn;
			Burn.At = Ground;
			Burn.Radius = Cells(1.4);
			Burn.Life = 30;
			Burn.Time = Fx->Now();
			Burn.Darkness = 0.6;
			Fx->Scorch(Burn);
		}
		// The dust ring racing out while the jets are near the ground.
		if (H < 5.0 && T > 0.15 && Wall() - Flight->LastRing > 0.16)
		{
			Flight->LastRing = Wall();
			DustRing(*Fx, Ground, 1.2 + 5.0 * FMath::Min(T / LiftSeconds, 1.0), H);
		}
	}

	// The camera: low on the ground behind the pad, the giant beyond, up at
	// the ship; then swinging up behind it as it heads off, levelling out.
	const FVector Low = Above(Ground - Way * Cells(11) + Side * Cells(4) + Up() * Cells(1.6 + 0.7 * FMath::Min(H, LiftTop)), 1.2);
	FVector Pos;
	FQuat Aim;
	// The ship a little left of the middle: the HUD's contracts sit right.
	auto OnShip = [&](const FVector& From)
	{
		const FQuat At0 = LookAt(From, Body + Up() * Cells(0.5));
		return LookAt(From, Body + Up() * Cells(0.5) + At0.GetRightVector() * Cells(0.12) * FVector::Dist(From, Body) / Cells(1));
	};
	if (T < LiftSeconds)
	{
		// Off the pilot's view at once, easing into the low spot.
		const double In = 1.0 - FMath::Pow(1.0 - FMath::Clamp(T / 0.5, 0.0, 1.0), 3.0);
		Pos = FMath::Lerp(Flight->EyeFrom, Low, In);
		Aim = FQuat::Slerp(Flight->RotFrom, OnShip(Pos), FMath::SmoothStep(0.0, 0.35, T));
		Flight->LowEye = Pos;
	}
	else
	{
		const double Tc = T - LiftSeconds;
		const double Swing = FMath::SmoothStep(0.0, 1.4, Tc);
		const FVector Behind = Body - Way * Cells(16) + Up() * Cells(4);
		Pos = FMath::Lerp(Flight->LowEye, Behind, Ease(Swing));
		// Up at the ship, then level along the way: the giant over it, the
		// region falling away below.
		const FQuat Level = FRotator(2.0, Way.Rotation().Yaw, 0).Quaternion();
		Aim = FQuat::Slerp(OnShip(Pos), Level, FMath::SmoothStep(0.2, 1.6, Tc));
	}
	At = Pos;
	Rot = Aim;
	SetFade(bGone ? 1.0 : TransitFade(T));
	return true;
}

void AAcPilotPawn::EndFlight()
{
	if (!Flight) return;
	Flight.Reset();
	Cruise(this, false);
	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<AAcFlightShip> It(World); It)
		{
			It->SetGlow(1.0);
			It->Hide();
		}
	}
	SetFade(0);
	ApplyView();
}

void AAcPilotPawn::SetFade(const double Amount)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* Manager = PC ? PC->PlayerCameraManager.Get() : nullptr;
	if (!Manager) return;
	if (Amount <= 0.001) Manager->StopCameraFade();
	else Manager->SetManualCameraFade(float(FMath::Min(Amount, 1.0)), FLinearColor::Black, false);
}
