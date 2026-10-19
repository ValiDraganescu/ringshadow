#include "AcPose.h"

#include "AcModelCatalog.h"
#include "AcKindTable.h"
#include "AcShuttleView.h"
#include "AcSpace.h"

#include "Rules.h"
#include "TerrainField.h"

#include <cmath>

namespace
{
	const int32 UnitKinds = (int32)ac::allCases<ac::UnitKind>().size();
	constexpr int32 StructureKinds = int32(ac::allCases<ac::StructureKind>().size());

	FAcPoseFn* UnitTable()
	{
		static FAcPoseFn Table[ac::allCases<ac::UnitKind>().size()] = {};
		return Table;
	}
	FAcPoseFn* StructureTable()
	{
		static FAcPoseFn Table[StructureKinds] = {};
		return Table;
	}

	double Smoothstep(const double A, const double B, const double X)
	{
		const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}
}

double FAcPoseContext::Ground(const ac::Vec2 P) const
{
	return Field.height(P);
}

bool FAcPoseContext::HasUpgrade(const int64 Player, const ac::Upgrade Upgrade) const
{
	if (Player < 0 || Player >= (int64)State.players.size()) return false;
	const auto& U = State.players[(size_t)Player].upgrades;
	return U && U->count(Upgrade) > 0;
}

void FAcPose::Reset(const FAcModelInfo& Model)
{
	const int32 N = Model.Parts.Num();
	Local.SetNumUninitialized(N);
	Visible.SetNumUninitialized(N);
	Emission.SetNumUninitialized(N);
	for (int32 I = 0; I < N; ++I)
	{
		const FAcModelPart& P = Model.Parts[I];
		Local[I] = P.Rest;
		Visible[I] = !P.bHidden;
		Emission[I] = 1.f;
	}
	Placement = FTransform::Identity;
	bAnimated = false;
	bHidden = false;
	bFrozen = false;
	Cues.Reset();
	MeshEmission.Reset();
}

int32 FAcPartIndex::Get(const FAcModelInfo& Model) const
{
	const int32* Found = Model.PartIndex.Find(Name);
	return Found ? *Found : INDEX_NONE;
}

namespace AcPose
{
	void Register(const ac::UnitKind Kind, const FAcPoseFn Fn) { UnitTable()[(int32)Kind] = Fn; }
	void Register(const ac::StructureKind Kind, const FAcPoseFn Fn) { StructureTable()[(int32)Kind] = Fn; }
	FAcPoseFn Find(const ac::UnitKind Kind) { return (int32)Kind < UnitKinds ? UnitTable()[(int32)Kind] : nullptr; }
	FAcPoseFn Find(const ac::StructureKind Kind)
	{
		return (int32)Kind < StructureKinds ? StructureTable()[(int32)Kind] : nullptr;
	}

	const TCHAR* ModelBase(const ac::UnitKind Kind) { return *AcKinds::Look(Kind).Base; }

	const TCHAR* ModelBase(const ac::StructureKind Kind)
	{
		switch (Kind)
		{
		case ac::StructureKind::citadel: return TEXT("citadel");
		case ac::StructureKind::habDome: return TEXT("habdome");
		case ac::StructureKind::garrison: return TEXT("garrison");
		case ac::StructureKind::bastion: return TEXT("bastion");
		case ac::StructureKind::derrick: return TEXT("derrick");
		case ac::StructureKind::foundry: return TEXT("foundry");
		case ac::StructureKind::spacedock: return TEXT("spacedock");
		case ac::StructureKind::lab: return TEXT("lab");
		case ac::StructureKind::sentinel: return TEXT("sentinel");
		case ac::StructureKind::gatling: return TEXT("gatling");
		case ac::StructureKind::landingPad: return TEXT("landingPad");
		case ac::StructureKind::dummy: return TEXT("dummy");
		}
		return TEXT("citadel");
	}

	double BuildingHeight(const ac::StructureKind Kind) { return ac::Rules::height(Kind); }

	FVector BuildingScale(const ac::StructureKind Kind)
	{
		// The footprint's half side and the height each model was made for
		// (GameScene.makeBuilding, GameScene.swift:363; Models.labHeight
		// 1.9): the strategy game's sizes. The Citadel's model is its hall
		// (citadel.py) and the dummy's its post (dummy.py): their own size.
		double R = 1.5, H = 2.0;
		switch (Kind)
		{
		case ac::StructureKind::citadel:
		case ac::StructureKind::dummy: return FVector::OneVector;
		case ac::StructureKind::habDome: R = 1.0; H = 1.2; break;
		case ac::StructureKind::garrison: H = 2.5; break;
		case ac::StructureKind::bastion: H = 1.4; break;
		case ac::StructureKind::derrick: H = 2.5; break;
		// foundry.py: made at the MMO's 7x7 (C5).
		case ac::StructureKind::foundry: R = 3.5; H = 5.76; break;
		case ac::StructureKind::spacedock: H = 3.2; break;
		case ac::StructureKind::lab: R = 1.0; H = 1.9; break;
		case ac::StructureKind::sentinel: R = 1.0; H = 1.7; break;
		case ac::StructureKind::gatling: R = 1.0; H = 1.3; break;
		// landing_pad.py: made at 11x11 (C5), its size the Dropship's.
		case ac::StructureKind::landingPad: R = 5.5; H = 0.18; break;
		}
		const double Across = ac::Rules::radius(Kind) / R;
		return FVector(Across, Across, ac::Rules::height(Kind) / H);
	}

	double ConstructionSink(const ac::Structure& S)
	{
		return -(1.0 - S.progress()) * BuildingHeight(S.kind) * 0.92;
	}

	double Hover(const ac::UnitKind Kind) { return AcKinds::Look(Kind).Hover; }

	double BodyHeight(const ac::Unit& U)
	{
		const ac::UnitStats& St = U.stats();
		return St.air || St.height > 0 ? U.bodyHeight() : AcKinds::Look(U.kind).ChestHeight;
	}

	FVector BodyPoint(const ac::Unit& U, const FAcModelInfo* Model, const TConstArrayView<FTransform> World, const double GroundCells)
	{
		const double Lift = BodyHeight(U);
		if (!Model || World.Num() == 0) return AcSpace::ToWorld(U.position, GroundCells + Lift);
		const FName Part = AcKinds::Look(U.kind).BodyPart;
		if (const int32* I = Part.IsNone() ? nullptr : Model->PartIndex.Find(Part); I && World.IsValidIndex(*I)) return World[*I].GetLocation();
		return World[0].GetLocation() + FVector(0, 0, AcSpace::ToCm(Lift));
	}

	double FlightBase(const FAcPoseContext& C)
	{
		const ac::Unit& U = *C.Unit;
		const ac::Vec2 Ahead = U.position + ac::Vec2(std::cos(U.heading), std::sin(U.heading)) * 1.5;
		const double Ground = FMath::Max(C.Ground(U.position), C.Ground(Ahead));
		double Y = C.Memory.AirY.Get(Ground);
		Y += (Ground - Y) * FMath::Min(1.0, C.Dt * (Ground > Y ? 3.0 : 1.2));
		C.Memory.AirY = Y;
		return Y;
	}

	double FlightY(const FAcPoseContext& C)
	{
		const ac::Unit& U = *C.Unit;
		const std::optional<double> J = U.jump();
		if (!J || !U.jumpFrom || !U.jumpTo) return C.Ground(U.position);
		const double A = C.Ground(*U.jumpFrom), B = C.Ground(*U.jumpTo);
		return A + (B - A) * Smoothstep(0.0, 1.0, *J);
	}

	double Turning(const FAcPoseContext& C)
	{
		const ac::Unit& U = *C.Unit;
		const double Last = C.Memory.LastHeading.Get(U.heading);
		C.Memory.LastHeading = U.heading;
		if (C.Dt <= 1e-4) return C.Memory.TurnRate;
		const double Raw = std::remainder(U.heading - Last, 2.0 * UE_DOUBLE_PI) / C.Dt;
		const double K = FMath::Min(1.0, C.Dt * 6.0);
		C.Memory.TurnRate = C.Memory.TurnRate * (1.0 - K) + Raw * K;
		return C.Memory.TurnRate;
	}

	TOptional<ac::GroundTilt> VehicleTilt(const FAcPoseContext& C)
	{
		const ac::Unit& U = *C.Unit;
		const std::optional<ac::Contacts> Contacts = ac::Contacts::of(U.kind);
		if (!Contacts) return {};
		// Under each contact: a Dropship's deck or ramp where one is (on the
		// pad, or the ship it rides: its frame's height), else the terrain.
		auto Under = [&C, &U](const ac::Vec2 P)
		{
			for (const AcShuttleView::FShip& F : AcShuttleView::Ships())
			{
				const ac::Shuttle& S = F.Ship;
				if (S.flying() ? !S.rides(U.id) : !S.grounded()) continue;
				const ac::Vec2 L = ac::ShuttleBay::local(S, P);
				if (ac::ShuttleBay::inBay(L) || ac::ShuttleBay::onRamp(L)) return S.ground + ac::ShuttleBay::floor(L, S.ramp.open);
			}
			return C.Ground(P);
		};
		const ac::GroundTilt Want = ac::groundTilt(Under, U.position, U.heading, *Contacts);
		ac::GroundTilt T = C.Memory.Tilt.Get(Want);
		const double K = FMath::Min(1.0, C.Dt * 10.0);
		T.pitch += (Want.pitch - T.pitch) * K;
		T.roll += (Want.roll - T.roll) * K;
		T.lift += (Want.lift - T.lift) * K;
		C.Memory.Tilt = T;
		return T;
	}

	FQuat TiltQuat(const ac::GroundTilt& T)
	{
		// UE's pitch is nose up; its roll turns the right side down.
		return FRotator(FMath::RadiansToDegrees(T.pitch), 0.0, -FMath::RadiansToDegrees(T.roll)).Quaternion();
	}

	void RestUnit(const FAcPoseContext& C, FAcPose& P)
	{
		P.Reset(C.Model);
		const ac::Unit& U = *C.Unit;
		double Y;
		if (U.stats().air) Y = FlightBase(C);
		else if (U.kind == ac::UnitKind::comet) Y = FlightY(C);
		else Y = C.Ground(U.position);
		FQuat Q = AcSpace::QuatFromHeading(U.heading);
		if (const TOptional<ac::GroundTilt> T = VehicleTilt(C))
		{
			Q = Q * TiltQuat(*T);
			Y += T->lift;
		}
		P.Placement = FTransform(Q, AcSpace::ToWorld(U.position, Y));
	}

	void RestStructure(const FAcPoseContext& C, FAcPose& P)
	{
		P.Reset(C.Model);
		const ac::Structure& S = *C.Structure;
		// Buildings face +Z in SceneKit (UE +Y) and are never turned; the
		// body rises out of the ground as it is built (`animate` :489).
		const double Y = C.Ground(S.position) + (S.complete() ? 0.0 : ConstructionSink(S));
		P.Placement = FTransform(FQuat::Identity, AcSpace::ToWorld(S.position, Y), BuildingScale(S.kind));
	}
}
