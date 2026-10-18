// The minigun class's rounds on the client (C2, docs/guns-and-practice.md
// §1, §6): a `GameEvent::Fired` gives a round's muzzle and velocity, and the
// client flies it on the core's own parabola (`ac::Rules::roundAt`, one
// integrator for both; no constant copied here) until its `Impact` or
// `Rules::roundLife`. Plain C++ (no engine), so
// Autocraft.Effects.RoundFliesTheSamePathAsTheCore checks it against
// `Simulation::stepRounds`. The streaks, flashes and impacts are
// AcEffectsRounds.cpp's.
#pragma once

#include "CoreMinimal.h"

#include "Rules.h"
#include "SimdMath.h"

#include <cmath>

struct FAcRoundFlight
{
	int64 Round = 0;
	int64 Shooter = 0;
	/// Cells (z above the ground datum) and cells a second, as fired.
	ac::Vec3 Muzzle;
	ac::Vec3 Velocity;
	/// Game time it left the muzzle.
	double Born = 0;

	/// Where it is at game time `Time`, cells.
	ac::Vec3 At(const double Time) const { return ac::Rules::roundAt(Muzzle, Velocity, FMath::Max(0.0, Time - Born)); }
	/// Its velocity then: the fall's (the derivative of `roundAt`).
	ac::Vec3 VelocityAt(const double Time) const
	{
		const double T = FMath::Max(0.0, Time - Born);
		return Velocity + ac::Vec3(0, 0, -ac::Rules::roundGravity * T);
	}
	double Speed(const double Time) const
	{
		const ac::Vec3 V = VelocityAt(Time);
		return std::sqrt(V.x * V.x + V.y * V.y + V.z * V.z);
	}
	/// Past `Rules::roundLife`: gone, whether or not an Impact came.
	bool Expired(const double Time) const { return Time - Born >= ac::Rules::roundLife; }
	/// The streak's tail at `Time`: `Length` cells back along the path, not
	/// before the muzzle.
	ac::Vec3 Tail(const double Time, const double Length) const
	{
		const double S = FMath::Max(Speed(Time), 1e-3);
		return At(FMath::Max(Born, Time - Length / S));
	}
};
