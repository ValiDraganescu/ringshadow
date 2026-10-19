#include "AcShuttleAct.h"

#include "Rules.h"
#include "Shuttle.h"
#include "Simulation.h"

namespace
{
	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// How far (seen from above, cells) `Local` is from the button at `B`.
	double Off(const ac::Vec2 Local, const ac::Vec3& B) { return ac::distance(Local, ac::Vec2(B.x, B.y)); }
}

FString AcShuttleAct::FAct::Prompt() const
{
	if (Why) return *Why;
	switch (Kind)
	{
	case EKind::Call: return TEXT("E · CALL THE DROPSHIP");
	case EKind::Ramp: return bOpen ? TEXT("E · OPEN THE RAMP") : TEXT("E · CLOSE THE RAMP");
	case EKind::TakeOff: return TEXT("E · TAKE OFF");
	}
	return FString();
}

TOptional<AcShuttleAct::FAct> AcShuttleAct::Find(const ac::Simulation& Sim, const ac::Unit& U)
{
	if (U.kind != ac::UnitKind::prospector || U.airborne()) return {};
	// In a ship (its bay; riding one in flight): its two buttons.
	if (const ac::Shuttle* S = Sim.aboard(U))
	{
		const ac::Vec2 L = ac::ShuttleBay::local(*S, U.position);
		if (!ac::ShuttleBay::inBay(L) && !S->flying()) return {};
		FAct A;
		A.Ship = S->id;
		if (Off(L, ac::ShuttleBay::takeOffButton) <= ac::ShuttleBay::reach)
		{
			A.Kind = FAct::EKind::TakeOff;
			if (const std::optional<std::string> Why = Sim.takeOffRefusal(*S, U)) A.Why = Text(*Why);
			return A;
		}
		if (Off(L, ac::ShuttleBay::rampButton) <= ac::ShuttleBay::reach)
		{
			A.Kind = FAct::EKind::Ramp;
			// Down or on its way down: raise it; else lower it.
			A.bOpen = !(S->ramp.opening || S->ramp.open >= ac::ShuttleBay::rampDown);
			if (const std::optional<std::string> Why = Sim.rampRefusal(*S, U, A.bOpen)) A.Why = Text(*Why);
			return A;
		}
		return {};
	}
	// On a finished realm Landing Pad with no Dropship on it or coming: call one.
	for (const ac::Structure& P : Sim.state.structures)
	{
		if (P.kind != ac::StructureKind::landingPad || !P.complete()) continue;
		const double R = ac::Rules::radius(P.kind);
		const ac::Vec2 D = U.position - P.position;
		if (FMath::Abs(D.x) > R || FMath::Abs(D.y) > R) continue;
		for (const ac::Shuttle& S : Sim.state.shuttles)
		{
			if (S.stage != ac::Shuttle::Stage::depart && ac::distance(S.position, P.position) < R) return {};
		}
		FAct A;
		A.Kind = FAct::EKind::Call;
		A.Pad = P.id;
		return A;
	}
	return {};
}
