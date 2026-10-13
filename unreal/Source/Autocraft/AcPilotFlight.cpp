// The MMORPG world's pad flight as the player sees it: the ship
// (`AAcFlightShip`) and the pilot pawn's flight view (AcPilotFlight.h).
#include "AcPilotFlight.h"

#include "AcEffects.h"
#include "AcLog.h"
#include "AcModelCatalog.h"
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
	// The cockpit off, the ship drawn.
	ApplyView();
	if (UAcEffects* Fx = UAcEffects::Get(this)) Fx->DustPuff(F.From.GetLocation(), 2.0, Fx->Now());
	UE_LOG(LogAutocraft, Log, TEXT("flight: lift-off at %s (%s)"), *F.From.GetLocation().ToCompactString(), *F.Model.ToString());
	return true;
}

bool AAcPilotPawn::FlightView(FVector& At, FQuat& Rot, const FTransform* Landing, const int64 Visit)
{
	using namespace AcPilotFlight;
	if (!Flight) return false;
	AAcFlightShip* Ship = AAcFlightShip::SpawnFor(GetWorld());
	UAcEffects* Fx = UAcEffects::Get(this);
	// Thrust: a glow under the ship, dust where it is low over the ground.
	auto Thrust = [&](const FVector& Ground, const double Height)
	{
		if (!Fx) return;
		Fx->Flash(Ground + Up() * Cells(Height + 0.05), 1.0, Fx->Now(), 0.05);
		if (Height < 3.0 && Wall() - Flight->LastPuff > 0.18)
		{
			Flight->LastPuff = Wall();
			Fx->DustPuff(Ground, 1.5, Fx->Now());
		}
	};

	if (Landing)
	{
		if (Flight->LandStart < 0)
		{
			Flight->LandStart = Wall();
			if (const UAcWorldRenderer* R = UAcWorldRenderer::Get(this))
			{
				if (const FAcModelInfo* Model = R->ModelOf(DrivenId)) Flight->Model = Model->Name;
			}
			ApplyView();
			UE_LOG(LogAutocraft, Log, TEXT("flight: landing at %s, %.1f s after the lift-off"), *Landing->GetLocation().ToCompactString(),
				Wall() - Flight->Start);
		}
		const double L = Wall() - Flight->LandStart;
		if (L >= LandSeconds)
		{
			if (Fx) Fx->DustPuff(Landing->GetLocation(), 2.0, Fx->Now());
			UE_LOG(LogAutocraft, Log, TEXT("flight: touched down, %.1f s after the lift-off"), Wall() - Flight->Start);
			EndFlight();
			return false;
		}
		const double H = LandHeight(L);
		const FVector Body = Landing->GetLocation() + Up() * Cells(H);
		if (Ship) Ship->Show(Flight->Model, Flight->Team, *Landing, Cells(H));
		Thrust(Landing->GetLocation(), H);
		// From behind and over the ship (the pilot view's side) down into
		// the pilot's own view.
		const double S = L / LandSeconds;
		const FVector Orbit = Body - Flat(Rot) * Cells(10) + Up() * Cells(8);
		const FVector Pos = FMath::Lerp(Orbit, At, Ease(S));
		Rot = FQuat::Slerp(LookAt(Pos, Body + Up() * Cells(0.6)), Rot, FMath::SmoothStep(0.45, 1.0, S));
		At = Pos;
		SetFade(LandFade(L));
		return true;
	}

	// In the air: the ship climbs from where the unit stood; the camera
	// leaves the pilot's view up behind it, pulls back, then turns slowly
	// round it, tilting down to the region falling away.
	const double T = Wall() - Flight->Start;
	const double H = LiftHeight(T);
	const FVector Ground = Flight->From.GetLocation();
	const FVector Body = Ground + Up() * Cells(H);
	// A RegionChange read, its first snapshot not yet: the old region is
	// gone from under the ship; dark until the landing.
	const bool bGone = Visit != Flight->Visit;
	if (Ship)
	{
		if (bGone) Ship->Hide();
		else Ship->Show(Flight->Model, Flight->Team, Flight->From, Cells(H));
	}
	if (!bGone && T < LiftSeconds + 1.0) Thrust(Ground, H);
	const double Tr = FMath::Max(0.0, T - LiftSeconds);
	const double Out = FMath::SmoothStep(0.0, 2.5, Tr);
	const FVector Back = -Flat(Flight->RotFrom).RotateAngleAxis(5.0 * Tr, Up());
	const FVector Orbit = Body + Back * Cells(7 + 5 * Out) + Up() * Cells(3 + 6 * Out);
	const FVector Aim = Body + Up() * Cells(0.6 - 10 * Out);
	const double Lift = T / LiftSeconds;
	// Eased out: off the pad at once, slowing into place behind it.
	const FVector Pos = FMath::Lerp(Flight->EyeFrom, Orbit, 1.0 - FMath::Pow(1.0 - FMath::Clamp(Lift, 0.0, 1.0), 3.0));
	At = Pos;
	Rot = FQuat::Slerp(Flight->RotFrom, LookAt(Pos, Aim), FMath::SmoothStep(0.0, 0.6, Lift));
	SetFade(bGone ? 1.0 : TransitFade(T));
	return true;
}

void AAcPilotPawn::EndFlight()
{
	if (!Flight) return;
	Flight.Reset();
	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<AAcFlightShip> It(World); It) It->Hide();
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
