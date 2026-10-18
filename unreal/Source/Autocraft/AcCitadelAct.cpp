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
	static const TCHAR* Names[4] = {TEXT("NE"), TEXT("NW"), TEXT("SW"), TEXT("SE")};
	return Names[FMath::Clamp(Gun, 0, 3)];
}

FString AcCitadelAct::GunName(const int32 Gun) { return FString::Printf(TEXT("%d %s"), FMath::Clamp(Gun, 0, 3) + 1, Compass(Gun)); }

FString AcCitadelAct::FAct::Prompt() const
{
	if (Why) return *Why;
	if (Kind == EKind::Man) return FString::Printf(TEXT("E · TAKE GUN %s"), *GunName(Gun));
	return bOpen ? FString(TEXT("E · OPEN")) : FString(TEXT("E · CLOSE"));
}

TOptional<AcCitadelAct::FAct> AcCitadelAct::Find(const ac::Simulation& Sim, const ac::Unit& U)
{
	if (U.task == ac::Unit::Task::atGun) return {};
	const bool bSheltered = Sim.sheltered(U);
	TOptional<FAct> Door;
	for (const ac::Structure& S : Sim.state.structures)
	{
		if (S.kind != ac::StructureKind::citadel || !S.complete()) continue;
		if (ac::distance(S.position, U.position) > ac::Rules::radius(S.kind) + ac::Rules::doorReach + 1) continue;
		// A free gun station within reach (from the hall).
		if (bSheltered && S.guns)
		{
			for (int32 K = 0; K < 4; ++K)
			{
				if ((*S.guns)[K].gunner) continue;
				if (ac::distance(U.position, ac::Defence::gunStation(S.position, K)) > ac::Defence::stationReach) continue;
				FAct A;
				A.Kind = FAct::EKind::Man;
				A.Citadel = S.id;
				A.Gun = K;
				A.bInside = true;
				return A;
			}
		}
		// The door within reach of its slab, on either side.
		const ac::Footprint& F = ac::Rules::footprint(S.kind);
		if (Door || !S.door || !F.door) continue;
		const ac::Vec2 Local = U.position - S.position;
		if (Off(*F.door, Local) > ac::Rules::doorReach) continue;
		const bool bInside = F.inside(Local);
		// Outside it only opens; inside it closes while it opens.
		if (!bInside && S.door->opening) continue;
		FAct A;
		A.Kind = FAct::EKind::Door;
		A.Citadel = S.id;
		A.bInside = bInside;
		A.bOpen = !(bInside && S.door->opening);
		if (const std::optional<std::string> Why = Sim.doorRefusal(S, A.bOpen, U)) A.Why = FString(UTF8_TO_TCHAR(Why->c_str()));
		Door = A;
	}
	return Door;
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

FVector AcCitadelAct::Eye(const ac::Structure& Citadel, const double GroundCells, const int32 Gun, const double Yaw)
{
	const ac::Vec3 E = ac::Defence::gunEye(Citadel.position, FMath::Clamp(Gun, 0, 3), Yaw);
	return AcSpace::ToWorld(ac::Vec2(E.x, E.y), GroundCells + E.z);
}

void AcCitadelAct::SetPanel(const int64 Citadel, const int32 Panel)
{
	GPanelCitadel = Citadel;
	GPanel = Panel;
}

int32 AcCitadelAct::Panel(const int64 Citadel) { return Citadel == GPanelCitadel.load() ? GPanel.load() : INDEX_NONE; }
