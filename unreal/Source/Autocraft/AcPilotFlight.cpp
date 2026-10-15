// The MMORPG world's Dropship flight as the player sees it (F2): the ship and
// the Prospector's copy (`AAcFlightShip`), and the pilot pawn's flight view
// (AcPilotFlight.h).
#include "AcPilotFlight.h"

#include "AcAudioDirector.h"
#include "AcMusicPlayer.h"
#include "AcEffects.h"
#include "AcEffectsBursts.h"
#include "AcEffectsExtension.h"
#include "AcGasGiant.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcModelCatalog.h"
#include "AcParticles.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"
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
		// A2's cue timeline: the Dropship's moments.
		if (UAcMusicPlayer* M = UAcMusicPlayer::Get(Ctx))
		{
			switch (Moment)
			{
			case FAcAudioRules::EFlight::Spool: M->Cue(TEXT("flight"), TEXT("flspool"), AcSound::FlightSpoolGain); break;
			case FAcAudioRules::EFlight::LiftOff: M->Cue(TEXT("flight"), TEXT("flroar"), AcSound::FlightRoarGain); break;
			case FAcAudioRules::EFlight::Retro: M->Cue(TEXT("flight"), TEXT("flretro"), AcSound::FlightRetroGain); break;
			case FAcAudioRules::EFlight::Touchdown: M->Cue(TEXT("flight"), TEXT("fltouch"), AcSound::FlightTouchGain); break;
			}
		}
	}
	void Cruise(const UObject* Ctx, const bool bOn)
	{
		if (UAcAudioDirector* A = UAcAudioDirector::Get(Ctx)) A->Rules().Cruise(bOn);
		static bool bWas = false;
		if (bOn != bWas)
			if (UAcMusicPlayer* M = UAcMusicPlayer::Get(Ctx)) M->Cue(TEXT("flight"), bOn ? TEXT("flcruise on") : TEXT("flcruise off"), bOn ? AcSound::FlightCruiseGain : 0.0);
		bWas = bOn;
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

AAcFlightShip* AAcFlightShip::SpawnFor(UWorld* World, const FName Name)
{
	if (!World) return nullptr;
	for (TActorIterator<AAcFlightShip> It(World); It; ++It)
	{
		if (It->GetFName() == Name) return *It;
	}
	FActorSpawnParameters P;
	P.Name = Name;
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
		// (A flier's ground shadow stays out: the flight's ship is in the air.)
		if (bHidden || Part.Opacity < 0.999f || Part.Name.ToString().Contains(TEXT("shadow"))) continue;
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

void AAcFlightShip::Prepare(const FName Model, const int32 Team)
{
	if (Key != Model || KeyTeam != Team) Build(Model, Team);
	Hide();
}

void AAcFlightShip::SetScale(const double Scale)
{
	Root->SetWorldScale3D(FVector(Scale));
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

namespace
{
	/// The Dropship's model, the realm's colours.
	const FName DropshipModel(TEXT("dropship_blue"));
	/// The Dropship model's body hovers this high over its root (cells, the
	/// strategy game's hover less the body's depth under its centre): the
	/// flight lowers it so its belly sits on `Belly` over the pad.
	constexpr double ModelHover = 1.9;
	/// The ship's belly over its ground point (cells).
	constexpr double Belly = 0.1;
	double ShipLift() { return AcSpace::ToCm(Belly - ModelHover * AcPilotFlight::ShipScale); }
	/// The ramp at the ship's back (cells behind its centre).
	constexpr double Ramp = 1.6;

	/// The ground's height (cm) under an Unreal point, `Fallback` off the
	/// drawn ground.
	double GroundCm(UAcEffects* Fx, const FVector& P, const double Fallback)
	{
		if (!Fx) return Fallback;
		const double Z = Fx->GroundZ(AcSpace::ToSim(P));
		return FMath::IsFinite(Z) ? Z : Fallback;
	}

	/// A camera easing after `Want` (a spring: `Rate` per second).
	FVector Toward(const FVector& From, const FVector& Want, const double Rate, const double Dt)
	{
		return FMath::Lerp(From, Want, 1.0 - FMath::Exp(-Rate * Dt));
	}
}

EAcFlightStage AAcPilotPawn::FlightStage() const
{
	if (!Flight) return EAcFlightStage::None;
	return Flight->Path.Stage(Wall() - Flight->Start);
}

double AAcPilotPawn::FlightSeconds() const
{
	return Flight ? Wall() - Flight->Start : -1.0;
}

bool AAcPilotPawn::StartFlight()
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	if (!Mmo || !Mmo->Ferry()) return false;
	const FAcMmoClient::FFerry& F = *Mmo->Ferry();
	FFlightShow Show;
	Show.Start = F.StartWall;
	Show.Path.Plan = F.Plan;
	Show.Path.A = F.From;
	Show.Path.B = F.To.Get(F.From);
	Show.Path.Giant = GiantWay();
	// Where the Prospector stood (it walks aboard from there), and the view.
	Show.From = LastPlacement.Get(FTransform(F.From));
	Show.EyeFrom = GetActorLocation();
	Show.RotFrom = GetActorQuat();
	Show.Rider = LastModel;
	Show.Team = LastTeam;
	UAcEffects* Fx = UAcEffects::Get(this);
	Show.Path.A.Z = GroundCm(Fx, Show.Path.A, Show.From.GetLocation().Z);
	Show.Path.B.Z = Show.Path.A.Z;
	Flight = Show;
	Dive.Reset();
	PullOut.Reset();
	Departure.Reset();
	ApplyView();
	UE_LOG(LogAutocraft, Log, TEXT("flight: the Dropship called at %s for %s %s, %.1f s in (total %.1f)"), *Show.Path.A.ToCompactString(),
		*F.ToRegion, *F.Pad, Wall() - Show.Start, Show.Path.Plan.total());
	return true;
}

bool AAcPilotPawn::FlightView(FVector& At, FQuat& Rot, const FTransform* Landing)
{
	using namespace AcPilotFlight;
	if (!Flight) return false;
	FFlightShow& F = *Flight;
	FPath& Trip = F.Path;
	const ac::Ferry::Plan& Plan = Trip.Plan;
	AAcFlightShip* Ship = AAcFlightShip::SpawnFor(GetWorld());
	AAcFlightShip* Rider = AAcFlightShip::SpawnFor(GetWorld(), TEXT("AcFlightRider"));
	UAcEffects* Fx = UAcEffects::Get(this);
	double Dt = 0;
	if (Fx)
	{
		Dt = F.LastFx >= 0 ? FMath::Clamp(Fx->Now() - F.LastFx, 0.0, 0.1) : 0.0;
		F.LastFx = Fx->Now();
	}
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const FAcMmoClient::FFerry* Live = Mmo && Mmo->Ferry() ? &*Mmo->Ferry() : nullptr;
	// The server's clock again (a later World message), the target pad once
	// its region's map is known, its height once its ground is drawn.
	if (Live)
	{
		F.Start = Live->StartWall;
		if (Live->To)
		{
			Trip.B.X = Live->To->X;
			Trip.B.Y = Live->To->Y;
		}
	}
	const bool bSwitched = Mmo && Live && Mmo->MirrorIndex() == Live->ToIndex;
	if ((bSwitched || Landing) && !F.bToHeight && Fx)
	{
		Trip.B.Z = GroundCm(Fx, Trip.B, Trip.B.Z);
		F.bToHeight = true;
	}
	double T = Wall() - F.Start;
	// The approach waits for the region change (another pilot drives there:
	// the server holds the ship), the touch down for the Prospector.
	if (Live && !bSwitched && T > Plan.approachAt() - 0.05) T = Plan.approachAt() - 0.05;
	if (!Landing && T > Plan.total()) T = Plan.total();
	if (Landing && F.StepOutStart < 0)
	{
		F.StepOutStart = Wall();
		// The step out from the touch down on, as the server timed it.
		T = FMath::Max(T, Plan.total());
		UE_LOG(LogAutocraft, Log, TEXT("flight: on the target pad, %.1f s after the call; stepping out"), Wall() - F.Start);
	}
	const double Out = F.StepOutStart >= 0 ? Wall() - F.StepOutStart : -1.0;
	const EAcFlightStage Moment = Out >= 0 ? EAcFlightStage::StepOut : Trip.Stage(T);
	const FVector Way = Trip.Way();
	const FVector Side = Trip.Side(Way);
	const FVector Giant = Trip.Giant;

	// The ship.
	const FVector Body = Out >= 0 ? Trip.B : Trip.Pos(T);
	const FQuat ShipRot = Out >= 0 ? Trip.Rot(Plan.total() - 0.31) : Trip.Rot(T);
	const double Thrust = Out >= 0 ? 0.1 : Trip.Thrust(T);
	if (Ship)
	{
		Ship->SetScale(ShipScale);
		Ship->Show(DropshipModel, F.Team, FTransform(ShipRot, Body), ShipLift());
		Ship->SetGlow(1.0 + 2.5 * Thrust);
	}
	if (Fx && Moment != EAcFlightStage::Board && Moment != EAcFlightStage::StepOut)
		Jets(*Fx, Body + Up() * Cells(Belly), ShipRot, Thrust, Dt);
	// The pads' lamps while it is over them.
	GBusyPads.Reset();
	if (Moment <= EAcFlightStage::Lift) GBusyPads.Add(Trip.A);
	if (Moment >= EAcFlightStage::Approach) GBusyPads.Add(Trip.B);

	// The moments, heard and shown once.
	if (!F.bLanded && Moment >= EAcFlightStage::Board)
	{
		F.bLanded = true;
		Hear(this, FAcAudioRules::EFlight::Touchdown);
		if (Fx)
		{
			Shock(*Fx, Trip.A, 3.5, 0.6);
			DustRing(*Fx, Trip.A, 1.6, 0);
		}
	}
	if (!F.bRoared && Moment >= EAcFlightStage::Lift)
	{
		F.bRoared = true;
		Hear(this, FAcAudioRules::EFlight::LiftOff);
		if (Fx)
		{
			Shock(*Fx, Trip.A, 3.0, 0.5);
			FAcScorchRequest Burn;
			Burn.At = Trip.A;
			Burn.Radius = Cells(1.4);
			Burn.Life = 30;
			Burn.Time = Fx->Now();
			Burn.Darkness = 0.6;
			Fx->Scorch(Burn);
		}
	}
	if (!F.bRetro && Moment >= EAcFlightStage::Approach)
	{
		F.bRetro = true;
		Hear(this, FAcAudioRules::EFlight::Retro);
	}
	if (!F.bTouched && (Moment == EAcFlightStage::StepOut || T >= Plan.total() - 0.02))
	{
		F.bTouched = true;
		Hear(this, FAcAudioRules::EFlight::Touchdown);
		if (Fx)
		{
			Shock(*Fx, Trip.B, 3.5, 0.6);
			DustRing(*Fx, Trip.B, 1.6, 0);
			Fx->DustPuff(Trip.B, 1.2, Fx->Now());
		}
	}
	Cruise(this, Moment >= EAcFlightStage::Lift && Moment <= EAcFlightStage::Dash);
	// The dust under the jets near the ground.
	const double Low = (Body.Z - (Moment >= EAcFlightStage::Approach ? Trip.B.Z : Trip.A.Z)) / Cells(1);
	if (Fx && Thrust > 0.3 && Low < 6.0 && Wall() - F.LastRing > 0.17)
	{
		F.LastRing = Wall();
		DustRing(*Fx, Moment >= EAcFlightStage::Approach ? Trip.B : Trip.A, 1.0 + 0.9 * (6.0 - Low), Low);
	}
	// The dash: cloud streaks, puffs of pale vapour flung past the ship.
	if (Fx && Moment == EAcFlightStage::Dash && Wall() - F.LastCloud > 0.05)
	{
		F.LastCloud = Wall();
		if (FAcParticles* P = AcEffectsBursts::ParticlesOf(Fx))
		{
			const FVector Fwd = ShipRot.GetForwardVector();
			for (int32 K = 0; K < 3; ++K)
			{
				const FVector Off = Fwd * Cells(FMath::FRandRange(14.0, 40.0)) + Side * Cells(FMath::FRandRange(-12.0, 12.0)) +
					Up() * Cells(FMath::FRandRange(-6.0, 7.0));
				FAcEmit Puff;
				Puff.Size = FMath::FRandRange(2.5, 5.0);
				Puff.Speed = 0.3;
				Puff.Rate = 40;
				Puff.Duration = 0.05;
				P->Burst(EAcParticle::MuzzleSmoke, FTransform(Body + Off), Fx->Now(), Puff);
			}
		}
	}

	// The Prospector's copy: walking up the ramp, and off it at the target.
	if (Rider)
	{
		const FVector Back = -ShipRot.GetForwardVector();
		if (Moment == EAcFlightStage::Descend || Moment == EAcFlightStage::Board)
		{
			const FVector Stood = F.From.GetLocation();
			const FVector RampAt = Trip.A + Back * Cells(Ramp * ShipScale) + FVector(0, 0, Stood.Z - Trip.A.Z);
			const double U = Moment == EAcFlightStage::Board ? (T - Plan.boardAt()) / Plan.board : 0.0;
			// Up to the ramp, then in.
			const FVector P0 = FMath::Lerp(Stood, RampAt, Ease(FMath::Min(U / 0.7, 1.0)));
			const FVector Pos = U > 0.7 ? FMath::Lerp(RampAt, Trip.A + Up() * Cells(0.3), (U - 0.7) / 0.3) : P0;
			FVector Dir = (U < 0.7 ? RampAt - Stood : Trip.A - RampAt);
			Dir.Z = 0;
			const FQuat Face = Dir.SizeSquared() > 1 ? Dir.Rotation().Quaternion() : F.From.GetRotation();
			if (U < 0.98 && !F.Rider.IsNone()) Rider->Show(F.Rider, F.Team, FTransform(U > 0 ? Face : F.From.GetRotation(), Pos), 0);
			else Rider->Hide();
		}
		else if (Moment == EAcFlightStage::StepOut && Landing && !F.Rider.IsNone())
		{
			const double U = FMath::Clamp(Out / ac::Ferry::stepOut, 0.0, 1.0);
			const FVector Hatch = Trip.B + Back * Cells(Ramp * ShipScale * 0.5) + Up() * Cells(0.2);
			const FVector Stand = Landing->GetLocation();
			FVector Dir = Stand - Hatch;
			Dir.Z = 0;
			Rider->Show(F.Rider, F.Team, FTransform(Dir.SizeSquared() > 1 ? Dir.Rotation().Quaternion() : Landing->GetRotation(),
				FMath::Lerp(Hatch, Stand, Ease(U))), 0);
		}
		else Rider->Hide();
	}

	// The camera.
	FVector Pos;
	FQuat Aim;
	auto Look = [](const FVector& From, const FVector& To) { return LookAt(From, To); };
	const FVector WideA = Trip.A - Giant * Cells(17) + Side * Cells(6) + Up() * Cells(2.2);
	const FVector CloseA = Trip.A - Giant * Cells(9) + Side * Cells(4) + Up() * Cells(1.4);
	const FVector ChaseAt = Body - ShipRot.GetForwardVector() * Cells(Moment == EAcFlightStage::Dash ? 11 : 15) + Up() * Cells(3.5);
	// Beside the approach's glide path, back along it: the ship comes past
	// and down onto the pad in front of the camera.
	const FVector WideB = Trip.B - Way * Cells(11) + Side * Cells(10) + Up() * Cells(3.5);
	switch (Moment)
	{
	case EAcFlightStage::Descend:
	{
		// Out of the pilot's eye to the low spot by the pad, looking along the
		// giant's way: the ship comes in over the camera, the giant beyond.
		const double In = Ease(T / 1.2);
		Pos = FMath::Lerp(F.EyeFrom, WideA, In);
		Aim = FQuat::Slerp(F.RotFrom, Look(Pos, Body + Up() * Cells(0.6)), Ease(T / 0.8));
		break;
	}
	case EAcFlightStage::Board:
	{
		const double U = (T - Plan.boardAt()) / Plan.board;
		Pos = FMath::Lerp(WideA, CloseA, Ease(U));
		Aim = Look(Pos, Trip.A + Up() * Cells(0.8) - Giant * Cells(1.2));
		break;
	}
	case EAcFlightStage::Lift:
	{
		const double U = (T - Plan.liftAt()) / Plan.lift;
		Pos = FMath::Lerp(CloseA, WideA + Up() * Cells(1.0), Ease(U));
		Aim = Look(Pos, Body + Up() * Cells(0.6));
		break;
	}
	case EAcFlightStage::Turns:
	case EAcFlightStage::Dash:
	{
		// The chase: easing after the ship, the view along its nose, banked
		// with it a little.
		const double U = (T - Plan.turnsAt()) / Plan.turns;
		const FVector Want = Moment == EAcFlightStage::Turns && U < 0.5 ? FMath::Lerp(WideA + Up() * Cells(1.0), ChaseAt, Ease(U / 0.5)) : ChaseAt;
		Pos = F.bCam ? Toward(F.Cam, Want, Moment == EAcFlightStage::Dash ? 14.0 : 5.0, Dt > 0 ? Dt : 1.0 / 60) : Want;
		const FRotator R = ShipRot.Rotator();
		const FQuat Along = Look(Pos, Body + ShipRot.GetForwardVector() * Cells(18) + Up() * Cells(1.0));
		const FRotator A0 = Along.Rotator();
		Aim = FRotator(A0.Pitch, A0.Yaw, R.Roll * 0.35).Quaternion();
		break;
	}
	case EAcFlightStage::Approach:
	{
		// Wide from beside the approach's glide path, the ship coming down onto the pad.
		const double U = (T - Plan.approachAt()) / Plan.approach;
		Pos = FMath::Lerp(F.bCam ? F.Cam : WideB, WideB, Ease(U / 0.35));
		Aim = FQuat::Slerp(F.bCam ? F.CamRot : Look(Pos, Body), Look(Pos, Body + Up() * Cells(0.5)), Ease(U / 0.3));
		break;
	}
	default:
	{
		// The step out: round behind the ship, where the ramp is, the
		// Prospector walking off it; then into the pilot's own view.
		const double U = Landing ? FMath::Clamp(Out / ac::Ferry::stepOut, 0.0, 1.0) : 0.0;
		const FVector Back = -ShipRot.GetForwardVector().GetSafeNormal2D();
		const FVector RampCam = Trip.B + Back * Cells(8) + Side * Cells(5) + Up() * Cells(2.2);
		const FVector From = FMath::Lerp(WideB, RampCam, Ease(U / 0.4));
		Pos = FMath::Lerp(From, At, Ease((U - 0.45) / 0.55));
		const FQuat OnRamp = Look(Pos, Trip.B + Back * Cells(Ramp) + Up() * Cells(0.5));
		Aim = FQuat::Slerp(OnRamp, Rot, Ease((U - 0.45) / 0.55));
		break;
	}
	}
	F.Cam = Pos;
	F.CamRot = Aim;
	F.bCam = true;
	At = Pos;
	Rot = Aim;
	// Stepped out: the ship lifts away by itself, control is back.
	if (Out >= ac::Ferry::stepOut)
	{
		FDeparture D;
		D.Start = Wall();
		D.At = Trip.B;
		D.Way = ShipRot.GetForwardVector().GetSafeNormal2D();
		D.Team = F.Team;
		UE_LOG(LogAutocraft, Log, TEXT("flight: stepped out, %.1f s after the call"), Wall() - F.Start);
		EndFlight();
		Departure = D;
		return false;
	}
	return true;
}

void AAcPilotPawn::Depart()
{
	using namespace AcPilotFlight;
	if (!Departure) return;
	AAcFlightShip* Ship = AAcFlightShip::SpawnFor(GetWorld());
	UAcEffects* Fx = UAcEffects::Get(this);
	const double T = Wall() - Departure->Start;
	if (T >= DepartSeconds || !Ship)
	{
		if (Ship) Ship->Hide();
		Departure.Reset();
		GBusyPads.Reset();
		Cruise(this, false);
		return;
	}
	double Dt = 0;
	if (Fx)
	{
		Dt = Departure->LastFx >= 0 ? FMath::Clamp(Fx->Now() - Departure->LastFx, 0.0, 0.1) : 0.0;
		Departure->LastFx = Fx->Now();
	}
	// A breath on the pad, then up and away along its nose, speeding up.
	const double U = FMath::Max(0.0, T - 0.6);
	const FVector Body = Departure->At + Up() * Cells(1.2 * U * U) + Departure->Way * Cells(2.5 * U * U * U);
	const FQuat Rot = FRotator(FMath::Min(12.0, 4.0 * U), Departure->Way.Rotation().Yaw, 0).Quaternion();
	Ship->Show(DropshipModel, Departure->Team, FTransform(Rot, Body), ShipLift());
	const double Thrust = FMath::SmoothStep(0.0, 0.6, T);
	Ship->SetGlow(1.0 + 2.5 * Thrust);
	if (Fx) Jets(*Fx, Body + Up() * Cells(Belly), Rot, Thrust, Dt);
	GBusyPads.Reset();
	if (T < 2.0) GBusyPads.Add(Departure->At);
	if (T < 0.05) Hear(this, FAcAudioRules::EFlight::LiftOff);
}

void AAcPilotPawn::EndFlight()
{
	if (!Flight) return;
	Flight.Reset();
	Cruise(this, false);
	if (UWorld* World = GetWorld())
	{
		if (AAcFlightShip* Rider = AAcFlightShip::SpawnFor(World, TEXT("AcFlightRider"))) Rider->Hide();
	}
	AcPilotFlight::GBusyPads.Reset();
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

// MARK: - The other pilots' flights (W5)

namespace
{
	struct FWatched
	{
		double Start = 0;
		double ElapsedSeen = -1;
		AcPilotFlight::FPath Path;
		int64 From = 0;
		int64 To = 0;
		bool bToHeight = false;
		double LastFx = -1;
		int32 Slot = 0;
		/// Where its Prospector stood when it called the ship, and its model.
		FTransform Stood;
		FName Rider;
		EAcFlightStage Stage = EAcFlightStage::None;
	};
	TMap<FString, FWatched> GWatched;
	/// The last place of each other pilot's Prospector on the ground here,
	/// and its model: the ship takes that one aboard.
	TMap<FString, TPair<FTransform, FName>> GLastSeen;

	FName WatchedName(const TCHAR* What, const int32 Slot) { return FName(*FString::Printf(TEXT("AcWatched%s%d"), What, Slot)); }
	double GroundAt(const UWorld* World, const ac::Vec2 P)
	{
		const AAcTerrain* T = AAcTerrain::Find(World);
		return T ? T->FieldHeight(P) : 0.0;
	}
}

TArray<TPair<FString, EAcFlightStage>> AcPilotFlight::Watched()
{
	TArray<TPair<FString, EAcFlightStage>> Out;
	for (const auto& [Name, X] : GWatched)
		if (X.Stage != EAcFlightStage::None) Out.Add({Name, X.Stage});
	return Out;
}

void AcPilotFlight::TickWatched(UWorld* World)
{
	const UAcSimSubsystem* Sim = World ? UAcSimSubsystem::Get(World) : nullptr;
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	const ac::net::WorldMessage* W = Mmo ? Mmo->World() : nullptr;
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const double Now = FPlatformTime::Seconds();
	auto HideSlot = [&](const int32 Slot) {
		if (!World) return;
		if (AAcFlightShip* Ship = AAcFlightShip::SpawnFor(World, WatchedName(TEXT("Ship"), Slot))) Ship->Hide();
		if (AAcFlightShip* Rider = AAcFlightShip::SpawnFor(World, WatchedName(TEXT("Rider"), Slot))) Rider->Hide();
	};
	TSet<FString> Live;
	if (Mmo && W && Mmo->Sky())
	{
		const UAcWorldRenderer* Renderer = UAcWorldRenderer::Get(World);
		for (const ac::net::Sky::Human& H : Mmo->Sky()->pilots)
		{
			if (P && H.callsign == P->callsign) continue;
			const FString Name = UTF8_TO_TCHAR(H.callsign.c_str());
			// On the ground here: where it stands, for the ship to take it from.
			if (!H.ferry)
			{
				if (const ac::Unit* U = M && H.unit && H.region == W->here ? M->unit(H.unit) : nullptr)
				{
					const FAcModelInfo* Model = Renderer ? Renderer->ModelOf(U->id) : nullptr;
					GLastSeen.Add(Name, {FTransform(AcSpace::QuatFromHeading(U->heading), AcSpace::ToWorld(U->position, GroundAt(World, U->position))),
						Model ? Model->Name : FName()});
				}
				continue;
			}
			const ac::net::Sky::Human::Ferry& F = *H.ferry;
			FWatched* X = GWatched.Find(Name);
			if (!X || X->From != H.region || X->To != F.to)
			{
				const TOptional<FVector> A = Mmo->RegionPoint(H.region, F.boarded);
				if (!A) continue;
				FWatched New;
				New.Start = Now - F.elapsed;
				New.From = H.region;
				New.To = F.to;
				New.Path.Plan = F.plan;
				New.Path.A = *A;
				// The target pad once its region's map is known (else the
				// giant's way: the ship goes off toward it).
				New.Path.B = Mmo->RegionPoint(F.to, F.at).Get(*A);
				New.Path.Giant = GiantWay();
				TSet<int32> Taken;
				for (const auto& [K, V] : GWatched) Taken.Add(V.Slot);
				while (Taken.Contains(New.Slot)) New.Slot += 1;
				if (const TPair<FTransform, FName>* Seen = GLastSeen.Find(Name))
				{
					New.Stood = Seen->Key;
					New.Rider = Seen->Value;
				}
				else
				{
					New.Stood = FTransform(*A);
				}
				if (UAcEffects* Fx = UAcEffects::Get(World)) New.Path.A.Z = GroundCm(Fx, New.Path.A, New.Stood.GetLocation().Z);
				New.Path.B.Z = New.Path.A.Z;
				UE_LOG(LogAutocraft, Log, TEXT("flight: %s's Dropship called at %s, %.1f s in (total %.1f)"), *Name,
					*New.Path.A.ToCompactString(), F.elapsed, F.plan.total());
				X = &GWatched.Add(Name, New);
			}
			else if (F.elapsed != X->ElapsedSeen && FMath::Abs(X->Start - (Now - F.elapsed)) > 0.25)
			{
				X->Start = Now - F.elapsed;
			}
			X->ElapsedSeen = F.elapsed;
			if (const TOptional<FVector> B = Mmo->RegionPoint(F.to, F.at); B && X->Path.B.Equals(X->Path.A, 1.0))
			{
				X->Path.B.X = B->X;
				X->Path.B.Y = B->Y;
			}
			Live.Add(Name);
		}
	}
	// Gone from the sky's flights: landed (its Prospector on the ground
	// again) or off line.
	for (auto It = GWatched.CreateIterator(); It; ++It)
	{
		if (Live.Contains(It.Key())) continue;
		HideSlot(It.Value().Slot);
		It.RemoveCurrent();
	}
	if (!World || !W) return;
	UAcEffects* Fx = UAcEffects::Get(World);
	for (auto& [Name, X] : GWatched)
	{
		const ac::Ferry::Plan& Plan = X.Path.Plan;
		const double T = Now - X.Start;
		// Here while it leaves (up to the switch), or where it comes in.
		const bool bLeaving = W->here == X.From && T < Plan.approachAt();
		const bool bComing = W->here == X.To && T >= Plan.approachAt() && T < Plan.total() + 1.0;
		if (!bLeaving && !bComing)
		{
			X.Stage = EAcFlightStage::None;
			HideSlot(X.Slot);
			continue;
		}
		if (bComing && !X.bToHeight && Fx)
		{
			X.Path.B.Z = GroundCm(Fx, X.Path.B, X.Path.B.Z);
			X.bToHeight = true;
		}
		const double Tc = FMath::Min(T, Plan.total());
		const EAcFlightStage Was = X.Stage;
		X.Stage = X.Path.Stage(Tc);
		if (X.Stage != Was)
		{
			const FVector At = X.Path.Pos(Tc);
			UE_LOG(LogAutocraft, Log, TEXT("flight: %s's Dropship, stage %d at %s (%s), rider %s from %s"), *Name, int32(X.Stage), *At.ToCompactString(),
				bLeaving ? TEXT("leaving here") : TEXT("coming here"), *X.Rider.ToString(), *X.Stood.GetLocation().ToCompactString());
		}
		double Dt = 0;
		if (Fx)
		{
			Dt = X.LastFx >= 0 ? FMath::Clamp(Fx->Now() - X.LastFx, 0.0, 0.1) : 0.0;
			X.LastFx = Fx->Now();
		}
		const FVector Body = X.Path.Pos(Tc);
		const FQuat Rot = X.Path.Rot(FMath::Min(Tc, Plan.total() - 0.31));
		const double Thrust = X.Path.Thrust(Tc);
		if (AAcFlightShip* Ship = AAcFlightShip::SpawnFor(World, WatchedName(TEXT("Ship"), X.Slot)))
		{
			Ship->SetScale(ShipScale);
			Ship->Show(DropshipModel, 0, FTransform(Rot, Body), ShipLift());
			Ship->SetGlow(1.0 + 2.5 * Thrust);
		}
		if (Fx && X.Stage != EAcFlightStage::Board) Jets(*Fx, Body + Up() * Cells(Belly), Rot, Thrust, Dt);
		// Its Prospector: standing where it called the ship, then up the ramp.
		AAcFlightShip* Rider = AAcFlightShip::SpawnFor(World, WatchedName(TEXT("Rider"), X.Slot));
		if (!Rider) continue;
		if (bLeaving && (X.Stage == EAcFlightStage::Descend || X.Stage == EAcFlightStage::Board) && !X.Rider.IsNone())
		{
			const FVector Back = -Rot.GetForwardVector();
			const FVector Stood = X.Stood.GetLocation();
			const FVector RampAt = X.Path.A + Back * Cells(Ramp * ShipScale) + FVector(0, 0, Stood.Z - X.Path.A.Z);
			const double U = X.Stage == EAcFlightStage::Board ? (Tc - Plan.boardAt()) / Plan.board : 0.0;
			const FVector P0 = FMath::Lerp(Stood, RampAt, Ease(FMath::Min(U / 0.7, 1.0)));
			const FVector Pos = U > 0.7 ? FMath::Lerp(RampAt, X.Path.A + Up() * Cells(0.3), (U - 0.7) / 0.3) : P0;
			FVector Dir = U < 0.7 ? RampAt - Stood : X.Path.A - RampAt;
			Dir.Z = 0;
			const FQuat Face = Dir.SizeSquared() > 1 ? Dir.Rotation().Quaternion() : X.Stood.GetRotation();
			if (U < 0.98) Rider->Show(X.Rider, 0, FTransform(U > 0 ? Face : X.Stood.GetRotation(), Pos), 0);
			else Rider->Hide();
		}
		else Rider->Hide();
	}
}
