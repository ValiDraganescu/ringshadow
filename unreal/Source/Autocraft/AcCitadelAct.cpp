#include "AcCitadelAct.h"

#include "AcSpace.h"

#include "Defence.h"
#include "Rules.h"

#include <atomic>

namespace
{
	std::atomic<int64> GPanelCitadel{0};
	std::atomic<int32> GPanel{INDEX_NONE};
}

const TCHAR* AcCitadelAct::Compass(const int32 Gun)
{
	static const TCHAR* Names[4] = {TEXT("SE"), TEXT("SW"), TEXT("NW"), TEXT("NE")};
	return Names[FMath::Clamp(Gun, 0, 3)];
}

FString AcCitadelAct::GunName(const int32 Gun) { return FString::Printf(TEXT("%d %s"), FMath::Clamp(Gun, 0, 3) + 1, Compass(Gun)); }

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
