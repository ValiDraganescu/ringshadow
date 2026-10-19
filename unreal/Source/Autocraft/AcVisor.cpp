#include "AcVisor.h"

FAcVisorParams AcVisor::Params(const ac::Wipers& W, const double WindAcross, const double Flash, const double Light)
{
	FAcVisorParams P;
	const double Now = W.now();
	P.Rain = float(W.rate());
	auto Sweep = [&](const ac::WiperSweep& S)
	{
		if (S.number < 0) return FLinearColor(0, 0, 0, 0);
		return FLinearColor(float(Now - S.start), float(S.each), float(W.waterSinceStart(S)), float(W.waterSinceEnd(S)));
	};
	P.A = Sweep(W.latest());
	P.B = Sweep(W.previous());
	auto Number = [](const ac::WiperSweep& S) { return float(S.number < 0 ? 0 : S.number % 1024); };
	P.Seed = FLinearColor(Number(W.latest()), Number(W.previous()), float(W.all()), 0.f);
	P.Arc = FLinearColor(float(PivotY), float(Reach), float(ac::Wipers::ParkAngle), float(ac::Wipers::FarAngle));
	P.Blade = FLinearColor(float(W.angle()), float(W.moving()), float(ac::Wipers::SmearSeconds), float(BladeHalfWidth));
	const auto H = W.history();
	P.Hist = FLinearColor(float(H[0]), float(H[1]), float(H[2]), float(H[3]));
	P.Air = FLinearColor(float(FMath::Clamp(WindAcross / FullWind, -1.0, 1.0)), float(FMath::Clamp(Flash, 0.0, 1.0)),
		float(FMath::Clamp(Light, 0.0, 1.0)), float(FMath::Fmod(FMath::Max(Now, 0.0), ClockWrap)));
	P.bWet = W.rate() > 0.001 || !W.parked() || W.all() > DryWater;
	return P;
}
