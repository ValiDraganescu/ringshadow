#include "AcActPick.h"

#include "AcCitadelAct.h"
#include "AcDemolish.h"
#include "AcSpace.h"

#include "Act.h"
#include "Mule.h"
#include "Rules.h"
#include "Shuttle.h"

#include <atomic>

namespace
{
	std::atomic<int64> GLit{0};

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// How near (cells, from the driven unit) a thing's boxes are gathered.
	constexpr double Near = 16.0;

	/// The gap across (`Across`, cells) and up between the body (feet at
	/// `Feet`, `Top` tall) and the heights `Z0` to `Z1` (cells).
	double Gap3(const double Across, const double Feet, const double Top, const double Z0, const double Z1)
	{
		const double Up = FMath::Max3(Z0 - (Feet + Top), Feet - Z1, 0.0);
		return std::sqrt(Across * Across + Up * Up);
	}

	/// A box `B` of the world (cells), its heights from the ground `Ground`.
	AcActPick::FTarget Box(const AcActPick::EKind Kind, const int64 Id, const int32 Index, const ac::Block& B, const double Ground,
		const ac::Unit& U, const double Feet)
	{
		AcActPick::FTarget T;
		T.Kind = Kind;
		T.Id = Id;
		T.Index = Index;
		T.Frame = FTransform(AcSpace::QuatFromHeading(B.angle), AcSpace::ToWorld(B.centre, Ground + (B.z0 + B.z1) / 2));
		T.Half = FVector(AcSpace::ToCm(B.half.x), AcSpace::ToCm(B.half.y), AcSpace::ToCm((B.z1 - B.z0) / 2));
		T.Gap = Gap3(ac::Act::gap(U, B), Feet, ac::Act::bodyTop(U), Ground + B.z0, Ground + B.z1);
		return T;
	}

	/// A box `Local` of ship `S` (its frame), drawn by `Placement` (its
	/// cells to the world's cm); `World` the same box in the world (cells).
	AcActPick::FTarget ShipBox(const AcActPick::EKind Kind, const ac::Shuttle& S, const ac::Block& Local, const ac::Block& World,
		const FTransform& Placement, const ac::Unit& U, const double Feet)
	{
		AcActPick::FTarget T;
		T.Kind = Kind;
		T.Id = S.id;
		const FTransform Own(AcSpace::QuatFromHeading(Local.angle),
			FVector(AcSpace::ToCm(Local.centre.x), AcSpace::ToCm(Local.centre.y), AcSpace::ToCm((Local.z0 + Local.z1) / 2)));
		T.Frame = Own * Placement;
		T.Frame.SetScale3D(FVector::OneVector);
		T.Half = FVector(AcSpace::ToCm(Local.half.x), AcSpace::ToCm(Local.half.y), AcSpace::ToCm((Local.z1 - Local.z0) / 2));
		const double Mid = AcSpace::HeightToSim(T.Frame.GetLocation());
		const double HalfUp = (Local.z1 - Local.z0) / 2;
		T.Gap = Gap3(ac::Act::gap(U, World), Feet, ac::Act::bodyTop(U), Mid - HalfUp, Mid + HalfUp);
		return T;
	}

	bool OnFoot(const ac::Unit& U)
	{
		using ETask = ac::Unit::Task;
		return U.kind == ac::UnitKind::prospector && U.task != ETask::aboard && U.task != ETask::atGun && U.task != ETask::inDerrick;
	}
}

TArray<AcActPick::FTarget> AcActPick::Targets(const ac::Simulation& Sim, const ac::Unit& U, const FWorld& W)
{
	TArray<FTarget> Out;
	const bool bFoot = OnFoot(U);
	const bool bMule = U.kind == ac::UnitKind::mule;
	if (!bFoot && !bMule) return Out;
	auto Ground = [&](const ac::Vec2 P) { return W.Ground ? W.Ground(P) : 0.0; };
	for (const ac::Structure& S : Sim.state.structures)
	{
		if (!S.complete() || ac::distance(S.position, U.position) > ac::Rules::radius(S.kind) + Near) continue;
		const double G = Ground(S.position);
		if (S.kind == ac::StructureKind::citadel && S.door)
		{
			// The door while it stands (an open door lies on the ground: a
			// look through the doorway is not on it), its panels always.
			for (int32 Side = 0; Side < 2; ++Side)
			{
				if (const std::optional<ac::Block> P = ac::Act::doorPanel(S, Side == 1)) Out.Add(Box(EKind::Door, S.id, Side, *P, G, U, W.Feet));
			}
			if (const std::optional<ac::Block> L = ac::Act::doorLeaf(S); L && S.door->open < 0.95)
			{
				Out.Add(Box(EKind::Door, S.id, 2, *L, G, U, W.Feet));
			}
			if (bFoot && S.guns)
			{
				for (int32 K = 0; K < 4; ++K) Out.Add(Box(EKind::Gun, S.id, K, ac::Act::console(S, K), G, U, W.Feet));
			}
		}
		else if (bFoot && S.kind == ac::StructureKind::foundry)
		{
			Out.Add(Box(EKind::Buy, S.id, INDEX_NONE, ac::Act::foundryPost(S), G, U, W.Feet));
		}
		else if (bFoot && S.kind == ac::StructureKind::landingPad)
		{
			Out.Add(Box(EKind::Call, S.id, INDEX_NONE, ac::Act::padPost(S), G, U, W.Feet));
		}
	}
	if (bFoot)
	{
		for (const ac::Unit& M : Sim.state.units)
		{
			if (M.kind != ac::UnitKind::mule || !(M.hp > 0) || ac::distance(M.position, U.position) > Near) continue;
			const double G = Ground(M.position);
			Out.Add(Box(EKind::Board, M.id, INDEX_NONE, ac::Act::perch(M), G, U, W.Feet));
			Out.Add(Box(EKind::Fill, M.id, INDEX_NONE, ac::Act::hopper(M), G, U, W.Feet));
		}
	}
	// In a ship's bay (or riding one in flight): its two buttons.
	if (const ac::Shuttle* S = Sim.aboard(U); S && (S->flying() || ac::ShuttleBay::inBay(ac::ShuttleBay::local(*S, U.position))))
	{
		TOptional<FTransform> Placement = W.Ship ? W.Ship(S->id) : TOptional<FTransform>();
		if (!Placement)
		{
			Placement = FTransform(AcSpace::QuatFromHeading(S->heading), AcSpace::ToWorld(S->position, S->ground));
		}
		Out.Add(ShipBox(EKind::Ramp, *S, ac::Act::rampPostLocal(), ac::Act::rampPost(*S), *Placement, U, W.Feet));
		Out.Add(ShipBox(EKind::TakeOff, *S, ac::Act::takeOffLocal(), ac::Act::takeOff(*S), *Placement, U, W.Feet));
	}
	return Out;
}

TOptional<double> AcActPick::Enters(const FTarget& T, const FVector& Start, const FVector& End)
{
	// The segment in the box's own frame, clipped by its three slabs.
	const FVector A = T.Frame.InverseTransformPositionNoScale(Start), B = T.Frame.InverseTransformPositionNoScale(End);
	const FVector D = B - A;
	double T0 = 0.0, T1 = 1.0;
	for (int32 K = 0; K < 3; ++K)
	{
		const double O = A[K], V = D[K], H = T.Half[K];
		if (FMath::Abs(V) < 1e-9)
		{
			if (O < -H || O > H) return {};
			continue;
		}
		double Lo = (-H - O) / V, Hi = (H - O) / V;
		if (Lo > Hi) Swap(Lo, Hi);
		T0 = FMath::Max(T0, Lo);
		T1 = FMath::Min(T1, Hi);
		if (T0 > T1) return {};
	}
	return T0;
}

TOptional<AcActPick::FTarget> AcActPick::Pick(const TConstArrayView<FTarget> Targets, const FRay& Ray, bool* bOnBox)
{
	if (bOnBox) *bOnBox = false;
	const double Length = FVector::Dist(Ray.Start, Ray.End);
	if (Length < 1.0) return {};
	// How far along (0 to 1) the world stops the ray, a little past it.
	const double Stop = FMath::Min(1.0, (FVector::Dist(Ray.Start, Ray.Blocked) + AcSpace::ToCm(Tolerance)) / Length);
	const FTarget* Best = nullptr;
	double BestAt = 0.0;
	for (const FTarget& T : Targets)
	{
		const TOptional<double> At = Enters(T, Ray.Start, Ray.End);
		if (!At || *At > Stop || (Best && *At >= BestAt)) continue;
		Best = &T;
		BestAt = *At;
	}
	if (!Best) return {};
	if (bOnBox) *bOnBox = true;
	if (Best->Gap > ac::Act::reach) return {};
	return *Best;
}

void AcActPick::SetLit(const int64 Building) { GLit = Building; }
int64 AcActPick::Lit() { return GLit.load(); }

double AcActPick::GapTo(const ac::Unit& U, const double Feet, const FVector& At)
{
	const ac::Block Point{AcSpace::ToSim(At), ac::Vec2(0, 0), 0, 0, 0};
	const double Z = AcSpace::HeightToSim(At);
	return Gap3(ac::Act::gap(U, Point), Feet, ac::Act::bodyTop(U), Z, Z);
}

FString AcActPick::FAct::Prompt() const
{
	if (Kind == EKind::Demolish) return Why ? *Why : AcDemolish::Prompt(Building);
	if (Why) return *Why;
	switch (Kind)
	{
	case EKind::Door: return bOpen ? TEXT("E · OPEN") : TEXT("E · CLOSE");
	case EKind::Gun: return FString::Printf(TEXT("E · TAKE GUN %s"), *AcCitadelAct::GunName(Index));
	case EKind::Board: return TEXT("E · CLIMB ON");
	case EKind::Fill: return TEXT("E · FILL THE MULE");
	case EKind::Buy:
		return FString::Printf(TEXT("E · BUY MULE · %lld ORE · %lld MH"), (long long)ac::Mule::ore(), (long long)ac::Mule::hydrogen());
	case EKind::Call: return TEXT("E · CALL THE DROPSHIP");
	case EKind::Ramp: return bOpen ? TEXT("E · OPEN THE RAMP") : TEXT("E · CLOSE THE RAMP");
	case EKind::TakeOff: return TEXT("E · TAKE OFF");
	case EKind::Leave: return TEXT("E · CLIMB OFF");
	case EKind::Demolish: break;
	}
	return FString();
}

TOptional<AcActPick::FAct> AcActPick::Act(const ac::Simulation& Sim, const ac::Unit& U, const AcMuleAct::FOwn& Own,
	const TOptional<FTarget>& Picked, const bool bOnBox, const int64 Crosshair, const double CrosshairGap)
{
	FAct A;
	if (Picked)
	{
		A.Kind = Picked->Kind;
		A.Id = Picked->Id;
		A.Index = Picked->Index;
		switch (Picked->Kind)
		{
		case EKind::Door:
		case EKind::Gun:
		case EKind::Buy:
		case EKind::Call:
		{
			const std::optional<ac::Structure> S = Sim.state.structure(Picked->Id);
			if (!S) return {};
			if (Picked->Kind == EKind::Door)
			{
				A.bInside = ac::Rules::footprint(S->kind).inside(U.position - S->position);
				A.bOpen = !(S->door && S->door->opening);
				if (const std::optional<std::string> Why = Sim.doorRefusal(*S, A.bOpen, U)) A.Why = Text(*Why);
			}
			else if (Picked->Kind == EKind::Gun)
			{
				if (S->guns && (*S->guns)[size_t(FMath::Clamp(A.Index, 0, 3))].gunner) A.Why = FString(TEXT("Someone is at that gun"));
				else if (!Sim.sheltered(U)) A.Why = FString(TEXT("Get inside the Citadel first"));
			}
			else if (Picked->Kind == EKind::Buy)
			{
				A.Why = AcMuleAct::BuyRefusal(Own);
			}
			else
			{
				// A Dropship on the pad or coming down onto it: no call.
				for (const ac::Shuttle& Ship : Sim.state.shuttles)
				{
					if (Ship.stage == ac::Shuttle::Stage::depart || ac::distance(Ship.position, S->position) >= ac::Rules::radius(S->kind)) continue;
					A.Why = FString(Ship.stage == ac::Shuttle::Stage::descend ? TEXT("The Dropship is on its way") : TEXT("The Dropship is here"));
				}
			}
			return A;
		}
		case EKind::Board:
		case EKind::Fill:
		{
			const std::optional<int64_t> I = Sim.unitIndex(Picked->Id);
			if (!I) return {};
			const ac::Unit& M = Sim.state.units[size_t(*I)];
			const std::optional<std::string> Why = Picked->Kind == EKind::Board ? Sim.muleBoardRefusal(U, M) : Sim.muleFillRefusal(U, M);
			if (Why) A.Why = Text(*Why);
			// Only its owner drives it or fills it (the server says it again).
			else if (Own.Mule != M.id) A.Why = FString(TEXT("Not your Mule"));
			return A;
		}
		case EKind::Ramp:
		case EKind::TakeOff:
		{
			const ac::Shuttle* S = nullptr;
			for (const ac::Shuttle& Ship : Sim.state.shuttles)
			{
				if (Ship.id == Picked->Id) S = &Ship;
			}
			if (!S) return {};
			if (Picked->Kind == EKind::Ramp)
			{
				// Down or on its way down: raise it; else lower it.
				A.bOpen = !(S->ramp.opening || S->ramp.open >= ac::ShuttleBay::rampDown);
				if (const std::optional<std::string> Why = Sim.rampRefusal(*S, U, A.bOpen)) A.Why = Text(*Why);
			}
			else if (const std::optional<std::string> Why = Sim.takeOffRefusal(*S, U))
			{
				A.Why = Text(*Why);
			}
			return A;
		}
		case EKind::Demolish:
		case EKind::Leave: break;
		}
		return {};
	}
	// No box under the crosshair: the realm's building it is on, in reach.
	if (!bOnBox && Crosshair != INDEX_NONE && CrosshairGap <= ac::Act::reach)
	{
		if (const std::optional<ac::Structure> S = Sim.state.structure(Crosshair); S && S->owner == ac::Pilot::player)
		{
			A.Kind = EKind::Demolish;
			A.Id = S->id;
			A.Building = S->kind;
			if (const std::optional<std::string> Why = Sim.demolishRefusal(*S)) A.Why = Text(*Why);
			return A;
		}
	}
	// Driving a Mule with nothing in reach under the crosshair: climb off.
	if (U.kind == ac::UnitKind::mule)
	{
		A.Kind = EKind::Leave;
		A.Id = U.id;
		if (const std::optional<int64_t> Rider = Sim.muleRider(U.id)) A.Rider = *Rider;
		else A.Why = FString(TEXT("No one on the perch"));
		return A;
	}
	return {};
}
