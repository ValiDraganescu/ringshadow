#include "AcShuttleView.h"

#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Rules.h"

namespace
{
	/// The ships drawn this frame, and each one's clock as last seen (the
	/// stage, the clock, and the real time it was seen at).
	TArray<AcShuttleView::FShip> GShips;
	struct FSeen
	{
		ac::Shuttle::Stage Stage = ac::Shuttle::Stage::descend;
		double Clock = 0, At = 0;
	};
	TMap<int64, FSeen> GSeen;

	/// How far ahead of the last snapshot the clock may run (s): a stalled
	/// connection freezes the ship rather than flying it on alone.
	constexpr double MaxAhead = 0.5;
}

void AcShuttleView::Update(const ac::GameState& State, const double Now)
{
	check(IsInGameThread());
	static uint64 Frame = 0;
	if (Frame == GFrameCounter && Frame != 0) return;
	Frame = GFrameCounter;
	GShips.Reset();
	TSet<int64> Live;
	for (const ac::Shuttle& S : State.shuttles)
	{
		Live.Add(S.id);
		FSeen& Seen = GSeen.FindOrAdd(S.id);
		if (Seen.At <= 0 || Seen.Stage != S.stage || Seen.Clock != S.clock)
		{
			Seen.Stage = S.stage;
			Seen.Clock = S.clock;
			Seen.At = Now;
		}
		FShip& F = GShips.AddDefaulted_GetRef();
		F.Ship = S;
		F.Clock = S.clock + FMath::Clamp(Now - Seen.At, 0.0, MaxAhead);
		const double End = ac::shuttleStageSeconds(S, S.stage);
		if (End > 0) F.Clock = FMath::Min(F.Clock, End);
		F.Pose = ac::shuttlePose(S, S.stage, F.Clock);
		F.Frame = FrameTransform(S);
		F.Placement = PoseTransform(F.Pose);
		F.Carrier = F.Frame.Inverse() * F.Placement;
	}
	for (auto It = GSeen.CreateIterator(); It; ++It)
	{
		if (!Live.Contains(It.Key())) It.RemoveCurrent();
	}
}

void AcShuttleView::Update(const UWorld* World)
{
	UAcSimSubsystem* Sim = World ? UAcSimSubsystem::Get(World) : nullptr;
	if (!Sim || !Sim->IsRunning()) return;
	Update(Sim->Shown(), FPlatformTime::Seconds());
}

TConstArrayView<AcShuttleView::FShip> AcShuttleView::Ships()
{
	return GShips;
}

const AcShuttleView::FShip* AcShuttleView::Find(const int64 Id)
{
	return GShips.FindByPredicate([Id](const FShip& F) { return F.Ship.id == Id; });
}

const AcShuttleView::FShip* AcShuttleView::Aboard(const ac::Unit& U)
{
	for (const FShip& F : GShips)
	{
		const ac::Shuttle& S = F.Ship;
		if (S.flying())
		{
			if (S.rides(U.id)) return &F;
			continue;
		}
		if (!S.grounded() || U.airborne()) continue;
		const ac::Vec2 L = ac::ShuttleBay::local(S, U.position);
		if (ac::ShuttleBay::inBay(L) || ac::ShuttleBay::onRamp(L)) return &F;
	}
	return nullptr;
}

FTransform AcShuttleView::FrameTransform(const ac::Shuttle& S)
{
	return FTransform(AcSpace::QuatFromHeading(S.heading), AcSpace::ToWorld(S.position, S.ground));
}

FTransform AcShuttleView::PoseTransform(const ac::ShuttlePose& P)
{
	// The sim's turns are Unreal's: yaw from +x toward +y, the nose up, the
	// bank putting the +y side down (Unreal's positive roll).
	const FRotator R(FMath::RadiansToDegrees(P.pitch), AcSpace::YawFromHeading(P.yaw), FMath::RadiansToDegrees(P.roll));
	return FTransform(R.Quaternion(), AcSpace::ToWorld(ac::Vec2(P.at.x, P.at.y), P.at.z));
}

double AcShuttleView::StandCm(const ac::Shuttle& S, const ac::Vec2 P)
{
	return AcSpace::ToCm(S.ground + ac::ShuttleBay::floor(ac::ShuttleBay::local(S, P), S.ramp.open));
}

bool AcShuttleView::Stand(const ac::Unit& U, FTransform& Placement)
{
	const FShip* F = Aboard(U);
	if (!F) return false;
	FVector T = Placement.GetTranslation();
	T.Z = StandCm(F->Ship, U.position);
	Placement.SetTranslation(T);
	return true;
}

bool AcShuttleView::Lift(const ac::Unit& U, FTransform& Placement)
{
	const FShip* F = Aboard(U);
	if (!F) return false;
	Placement = Placement * F->Carrier;
	return true;
}

void AcShuttleView::Slabs(const ac::Shuttle& S, TArray<AcPilotWalls::FSlab>& Out)
{
	const ac::Footprint& F = ac::ShuttleBay::footprint();
	// The boxes in the frame (cm, z over the world's zero), the frame turned to the heading on the pad.
	const FTransform Frame(AcSpace::QuatFromHeading(S.heading), AcSpace::ToWorld(S.position, 0.0));
	auto Add = [&](const ac::Slab& L)
	{
		const FVector Lo(AcSpace::ToCm(L.min.x), AcSpace::ToCm(L.min.y), AcSpace::ToCm(S.ground + L.z0));
		const FVector Hi(AcSpace::ToCm(L.max.x), AcSpace::ToCm(L.max.y), AcSpace::ToCm(S.ground + L.z1));
		AcPilotWalls::FSlab Slab;
		Slab.Box = FBox(Lo, Hi);
		Slab.bLow = L.z1 - L.z0 <= AcPilotWalls::LowWall && L.z1 <= ac::ShuttleBay::deck + 1.0;
		Slab.Frame = Frame;
		Out.Add(Slab);
	};
	for (const ac::Slab& W : F.walls) Add(W);
	if (F.door && S.ramp.open < ac::ShuttleBay::rampDown) Add(*F.door);
	if (F.roof) Add(*F.roof);
	for (const ac::Block& B : F.furniture) Add(B.bounds());
}
