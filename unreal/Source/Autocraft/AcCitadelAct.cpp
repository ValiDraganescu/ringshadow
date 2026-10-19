#include "AcCitadelAct.h"

#include "AcSpace.h"

#include "Defence.h"
#include "Rules.h"

#include <atomic>

namespace
{
	std::atomic<int64> GPanelCitadel{0};
	std::atomic<int32> GPanel{INDEX_NONE};

	/// How far `Local` is from slab `L` across the ground, cells (0 on it).
	double Off(const ac::Slab& L, const ac::Vec2 Local)
	{
		const double Dx = FMath::Max3(L.min.x - Local.x, 0.0, Local.x - L.max.x);
		const double Dy = FMath::Max3(L.min.y - Local.y, 0.0, Local.y - L.max.y);
		return std::sqrt(Dx * Dx + Dy * Dy);
	}
}

const TCHAR* AcCitadelAct::Compass(const int32 Gun)
{
	static const TCHAR* Names[4] = {TEXT("SE"), TEXT("SW"), TEXT("NW"), TEXT("NE")};
	return Names[FMath::Clamp(Gun, 0, 3)];
}

FString AcCitadelAct::GunName(const int32 Gun) { return FString::Printf(TEXT("%d %s"), FMath::Clamp(Gun, 0, 3) + 1, Compass(Gun)); }

FString AcCitadelAct::FAct::Prompt() const
{
	if (Why) return *Why;
	if (Kind == EKind::Man) return FString::Printf(TEXT("E · TAKE GUN %s"), *GunName(Gun));
	return bOpen ? FString(TEXT("E · OPEN")) : FString(TEXT("E · CLOSE"));
}

bool AcCitadelAct::AimedAtDoorway(const ac::Footprint& F, const ac::Vec2 Local, const double Look)
{
	if (!F.door) return false;
	// The doorway with its panels beside the jambs (0.75 out from each side).
	const ac::Slab& D = *F.door;
	const ac::Vec2 Lo = D.min - ac::Vec2(1.0, 0.5), Hi = D.max + ac::Vec2(1.0, 0.5);
	const ac::Vec2 Dir(std::cos(Look), std::sin(Look));
	// The look's ray against that box seen from above (slab clipping).
	double T0 = 0.0, T1 = ac::Rules::doorReach + 2.0;
	for (int32 K = 0; K < 2; ++K)
	{
		const double O = K == 0 ? Local.x : Local.y, V = K == 0 ? Dir.x : Dir.y;
		const double Min = K == 0 ? Lo.x : Lo.y, Max = K == 0 ? Hi.x : Hi.y;
		if (std::abs(V) < 1e-9)
		{
			if (O < Min || O > Max) return false;
			continue;
		}
		double A = (Min - O) / V, B = (Max - O) / V;
		if (A > B) std::swap(A, B);
		T0 = FMath::Max(T0, A);
		T1 = FMath::Min(T1, B);
		if (T0 > T1) return false;
	}
	return true;
}

TOptional<AcCitadelAct::FAct> AcCitadelAct::Find(const ac::Simulation& Sim, const ac::Unit& U)
{
	if (U.task == ac::Unit::Task::atGun) return {};
	const bool bSheltered = Sim.sheltered(U);
	TOptional<FAct> Best;
	for (const ac::Structure& S : Sim.state.structures)
	{
		if (S.kind != ac::StructureKind::citadel || !S.complete()) continue;
		if (ac::distance(S.position, U.position) > ac::Rules::radius(S.kind) + ac::Rules::doorReach + 1) continue;
		const ac::Footprint& F = ac::Rules::footprint(S.kind);
		const ac::Vec2 Local = U.position - S.position;
		// The nearest free gun station within reach (from the hall).
		TOptional<FAct> Gun;
		double GunOff = 0.0;
		if (bSheltered && S.guns)
		{
			for (int32 K = 0; K < 4; ++K)
			{
				if ((*S.guns)[K].gunner) continue;
				const double D = ac::distance(U.position, ac::Defence::gunStation(S.position, K));
				if (D > ac::Defence::stationReach || (Gun && D >= GunOff)) continue;
				FAct A;
				A.Kind = FAct::EKind::Man;
				A.Citadel = S.id;
				A.Gun = K;
				A.bInside = true;
				Gun = A;
				GunOff = D;
			}
		}
		// The door within reach of its slab, on either side: it closes
		// while it opens and opens while it shuts (C4: outside too).
		TOptional<FAct> Door;
		const double DoorOff = S.door && F.door ? Off(*F.door, Local) : 1e9;
		if (DoorOff <= ac::Rules::doorReach)
		{
			FAct A;
			A.Kind = FAct::EKind::Door;
			A.Citadel = S.id;
			A.bInside = F.inside(Local);
			A.bOpen = !S.door->opening;
			if (const std::optional<std::string> Why = Sim.doorRefusal(S, A.bOpen, U)) A.Why = FString(UTF8_TO_TCHAR(Why->c_str()));
			Door = A;
		}
		// Both in reach: the door when the look is on the doorway or its
		// panels, or the pilot is nearer the door than the station.
		TOptional<FAct> Here = Gun;
		if (Door && (!Gun || AimedAtDoorway(F, Local, U.look()) || DoorOff < GunOff)) Here = Door;
		if (Here && (!Best || Here->Kind == FAct::EKind::Man)) Best = Here;
	}
	return Best;
}

TOptional<TPair<int64, int32>> AcCitadelAct::Manned(const ac::Simulation& Sim, const ac::Unit& U)
{
	if (U.task != ac::Unit::Task::atGun) return {};
	const std::optional<std::pair<size_t, int>> G = Sim.mannedGun(U);
	if (!G || G->first >= Sim.state.structures.size()) return {};
	return TPair<int64, int32>(Sim.state.structures[G->first].id, G->second);
}

double AcCitadelAct::LookYaw(const int32 Gun, const double Was, const double Delta)
{
	const double Mid = ac::Defence::cornerAngle(FMath::Clamp(Gun, 0, 3));
	const double Off = FMath::Clamp(std::remainder(Was - Mid, 2.0 * UE_DOUBLE_PI), -ac::Defence::gunArcHalf, ac::Defence::gunArcHalf);
	return Mid + FMath::Clamp(Off + Delta, -ac::Defence::gunArcHalf, ac::Defence::gunArcHalf);
}

double AcCitadelAct::LookPitch(const double Pitch)
{
	return FMath::Clamp(Pitch, ac::Defence::gunPitchMin, ac::Defence::gunPitchMax);
}

FVector AcCitadelAct::Eye(const ac::Structure& Citadel, const double GroundCells, const int32 Gun, const double Yaw, const double Pitch)
{
	const ac::Vec3 E = ac::Defence::gunEye(Citadel.position, FMath::Clamp(Gun, 0, 3), Yaw, LookPitch(Pitch));
	return AcSpace::ToWorld(ac::Vec2(E.x, E.y), GroundCells + E.z);
}

void AcCitadelAct::SetPanel(const int64 Citadel, const int32 Panel)
{
	GPanelCitadel = Citadel;
	GPanel = Panel;
}

int32 AcCitadelAct::Panel(const int64 Citadel) { return Citadel == GPanelCitadel.load() ? GPanel.load() : INDEX_NONE; }
