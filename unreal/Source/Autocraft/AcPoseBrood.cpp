// The Brood's poses (docs/creatures.md, "Models and how to drive them";
// art/models/<kind>/parts.txt): plain functions of a bug's wire state
// (`ac::Bug`: its state, height, strikes and sac) and its walk.
//
//   Shardling: a tripod gait (legs 0, 2, 4 against 1, 3, 5), the body's bob,
//     the mandibles twitching; each strike a lunge, the head dipping, the
//     mandibles open and snap; a flinch when hit; dead, it rolls on its side,
//     legs curled, the glow fading over 2 s.
//   Gloamwing: the thorax lifted to its height (authored perched, the thorax
//     80 cm up), bobbing; the wings flapping (two pairs out of phase), the
//     lantern swaying behind; nose down on the move, down 35° in the dive,
//     the lantern flaring on the spit; dead, the wings stop and it falls,
//     tumbling.
//   Lodeback: a slow four-leg walk (0, 3 against 1, 2); eating, the head and
//     claws dig and the jaw grinds; the sac 0.55 to 1.0 of its size and as
//     bright with its fill; a bite the head's thrust; dead, the sac bursts in
//     a flash and the body sinks.
//   The Matriarch (N3): a slow breath, the crown and egg pods glowing with
//     it; the slam (forelimbs up over 0.8 s, then the crash), the spit (the
//     head draws back and lunges, the jaw wide), the roar (she rears a cell,
//     the crown fans open, the pods flare, 2 s); dead, she sinks, the crown
//     folds, the glow dies over 4 s. Her strike's kind rides in `Bug::sac`.
//
// No team colour: the glow of each kind is its own (the models' materials).
// Under the ringshadow band the glow brightens × 1.6 (`FAcPoseContext::Sweep`).
#include "AcPose.h"

#include "AcModelCatalog.h"
#include "AcSpace.h"

#include "Brood.h"
#include "Ringshadow.h"

#include <cmath>

namespace
{
	constexpr double Pi = UE_DOUBLE_PI;

	FQuat RotX(const double A) { return FQuat(FVector(1, 0, 0), A); }
	FQuat RotY(const double A) { return FQuat(FVector(0, 1, 0), A); }
	FQuat RotZ(const double A) { return FQuat(FVector(0, 0, 1), A); }

	void Turn(FAcPose& P, const FAcModelInfo& M, const int32 I, const FQuat& Q)
	{
		if (I == INDEX_NONE) return;
		P.Local[I].SetRotation((Q * M.Parts[I].Rest.GetRotation()).GetNormalized());
	}

	void Move(FAcPose& P, const FAcModelInfo& M, const int32 I, const FVector& By)
	{
		if (I == INDEX_NONE) return;
		P.Local[I].SetTranslation(M.Parts[I].Rest.GetTranslation() + By);
	}

	void Glow(FAcPose& P, const int32 I, const double Value)
	{
		if (I != INDEX_NONE) P.Emission[I] = float(Value);
	}

	double Ease(const double X) { const double T = FMath::Clamp(X, 0.0, 1.0); return T * T * (3.0 - 2.0 * T); }

	/// A bug's own memory: its walk easing, its last strike, its last hp,
	/// when it was first seen dead, its height and sac eased.
	struct FBugState : FAcPoseState
	{
		double Walk = 0.0;
		int64 Strikes = -1;
		double StrikeAt = -100.0;
		double Hp = -1.0;
		double HurtAt = -100.0;
		TOptional<double> DeadAt;
		TOptional<double> Height;
		TOptional<double> Sac;
		double Aim = 0.0;
	};

	/// The common part of every bug's pose: the memory brought up to date.
	/// Returns the strike's progress (0..1 over `Lunge` s after a new strike;
	/// past 1: none under way).
	double Common(const FAcPoseContext& C, FBugState& S, const double Lunge)
	{
		const ac::Unit& U = *C.Unit;
		const double Time = C.Time;
		const ac::Bug B = U.bug.value_or(ac::Bug{});
		const bool bMoving = B.state == ac::Bug::State::move || B.state == ac::Bug::State::flee || U.moving.value_or(false);
		const double Rate = C.Dt > 0.0 ? FMath::Min(1.0, C.Dt * 6.0) : 1.0;
		S.Walk += ((bMoving ? 1.0 : 0.0) - S.Walk) * Rate;
		if (S.Strikes < 0) S.Strikes = B.strikes;
		if (B.strikes != S.Strikes)
		{
			S.Strikes = B.strikes;
			S.StrikeAt = Time;
		}
		if (S.Hp >= 0.0 && U.hp < S.Hp - 0.01) S.HurtAt = Time;
		S.Hp = U.hp;
		if (B.state == ac::Bug::State::dead && !S.DeadAt) S.DeadAt = Time;
		return (Time - S.StrikeAt) / Lunge;
	}

	/// The glow now: × 1.6 under the ringshadow band, fading over `Fade` s
	/// once dead.
	double GlowOf(const FAcPoseContext& C, const FBugState& S, const double Fade)
	{
		double G = 1.0;
		if (C.Sweep) G *= 1.0 + 0.6 * ac::Ringshadow::cover(*C.Sweep, C.Unit->position);
		if (S.DeadAt) G *= FMath::Max(0.0, 1.0 - (C.Time - *S.DeadAt) / Fade);
		return G;
	}

	/// The head's turn toward its target, radians off the heading, eased.
	double AimAt(const FAcPoseContext& C, FBugState& S, const double Limit)
	{
		const ac::Unit& U = *C.Unit;
		double Want = 0.0;
		if (U.target)
			if (const std::optional<ac::Unit> V = C.State.unit(*U.target))
			{
				const ac::Vec2 D = V->position - U.position;
				Want = FMath::Clamp(std::remainder(std::atan2(D.y, D.x) - U.heading, 2.0 * Pi), -Limit, Limit);
			}
		S.Aim += (Want - S.Aim) * (C.Dt > 0.0 ? FMath::Min(1.0, C.Dt * 8.0) : 1.0);
		return S.Aim;
	}

	// --- Shardling ---------------------------------------------------------------

	void PoseShardling(const FAcPoseContext& C, FAcPose& P)
	{
		static const FAcPartIndex Body(TEXT("body")), Head(TEXT("head"));
		static const FAcPartIndex Mandibles[2] = {FAcPartIndex(TEXT("mandible_0")), FAcPartIndex(TEXT("mandible_1"))};
		static const FAcPartIndex Legs[6] = {FAcPartIndex(TEXT("leg_0")), FAcPartIndex(TEXT("leg_1")), FAcPartIndex(TEXT("leg_2")),
			FAcPartIndex(TEXT("leg_3")), FAcPartIndex(TEXT("leg_4")), FAcPartIndex(TEXT("leg_5"))};
		const FAcModelInfo& M = C.Model;
		const ac::Unit& U = *C.Unit;
		FBugState& S = C.Memory.State<FBugState>();
		const double K = Common(C, S, 0.25);
		const double T = C.Time + double(U.id) * 1.37;
		P.bAnimated = true;
		const double G = GlowOf(C, S, 2.0);
		for (int32 I = 0; I < P.Emission.Num(); ++I) P.Emission[I] = float(G);

		if (S.DeadAt)
		{
			// It rolls on its side, the legs curl in; a while later it sinks.
			const double D = C.Time - *S.DeadAt;
			const double Roll = Ease(D / 0.6);
			const double Sink = Ease((D - 4.0) / 2.0);
			Turn(P, M, Body.Get(M), RotX(1.45 * Roll));
			Move(P, M, Body.Get(M), FVector(0, 0, -14.0 * Roll - 30.0 * Sink));
			for (int32 L = 0; L < 6; ++L)
			{
				const double Side = L < 3 ? -1.0 : 1.0;
				Turn(P, M, Legs[L].Get(M), RotX(Side * 0.9 * Roll) * RotZ(-Side * 0.3 * Roll));
			}
			for (int32 Q = 0; Q < 2; ++Q) Turn(P, M, Mandibles[Q].Get(M), RotZ((Q == 0 ? 1.0 : -1.0) * 0.3 * Roll));
			return;
		}

		// The gait: a step every 0.55 cells walked.
		const double Cycle = U.stride / 0.55;
		const double Lunge = K < 1.0 ? std::sin(Pi * FMath::Clamp(K, 0.0, 1.0)) : 0.0;
		const double Hurt = FMath::Max(0.0, 1.0 - (C.Time - S.HurtAt) / 0.2);
		const double Bob = 2.0 * std::abs(std::sin(Pi * 2.0 * Cycle)) * S.Walk + 0.8 * std::sin(T * 2.2) * (1.0 - S.Walk);
		Move(P, M, Body.Get(M), FVector(30.0 * Lunge - 8.0 * Hurt, 0, Bob));
		Turn(P, M, Body.Get(M), RotY(0.08 * Lunge - 0.06 * Hurt) * RotX(0.03 * std::sin(Pi * 2.0 * Cycle) * S.Walk));
		Turn(P, M, Head.Get(M), RotZ(AimAt(C, S, 0.6)) * RotY(0.3 * Lunge));
		// Mandibles: twitching apart at rest; on a strike wide open, then shut.
		double Open = 0.1 + 0.06 * std::sin(T * 7.0) * std::sin(T * 2.3);
		if (K < 1.0) Open = K < 0.45 ? FMath::Lerp(Open, 0.7, Ease(K / 0.3)) : FMath::Lerp(0.7, -0.05, Ease((K - 0.45) / 0.3));
		for (int32 Q = 0; Q < 2; ++Q) Turn(P, M, Mandibles[Q].Get(M), RotZ((Q == 0 ? -1.0 : 1.0) * Open));
		for (int32 L = 0; L < 6; ++L)
		{
			const double Side = L < 3 ? -1.0 : 1.0;
			const double Phase = Pi * 2.0 * (Cycle + ((L % 2) == 0 ? 0.0 : 0.5));
			const double Swing = 0.44 * std::sin(Phase) * S.Walk;
			const double Lift = 0.35 * FMath::Max(0.0, std::cos(Phase)) * S.Walk;
			Turn(P, M, Legs[L].Get(M), RotZ(Side * Swing) * RotX(Side * Lift));
		}
	}

	// --- Gloamwing ---------------------------------------------------------------

	void PoseGloamwing(const FAcPoseContext& C, FAcPose& P)
	{
		static const FAcPartIndex Thorax(TEXT("thorax")), Head(TEXT("head")), Abdomen(TEXT("abdomen"));
		static const FAcPartIndex Wings[4] = {FAcPartIndex(TEXT("wing_0")), FAcPartIndex(TEXT("wing_1")), FAcPartIndex(TEXT("wing_2")),
			FAcPartIndex(TEXT("wing_3"))};
		static const FAcPartIndex Legs[4] = {FAcPartIndex(TEXT("leg_0")), FAcPartIndex(TEXT("leg_1")), FAcPartIndex(TEXT("leg_2")),
			FAcPartIndex(TEXT("leg_3"))};
		const FAcModelInfo& M = C.Model;
		const ac::Unit& U = *C.Unit;
		const ac::Bug B = U.bug.value_or(ac::Bug{});
		FBugState& S = C.Memory.State<FBugState>();
		const double K = Common(C, S, 0.4);
		const double T = C.Time + double(U.id) * 0.91;
		P.bAnimated = true;
		const double G = GlowOf(C, S, 2.5);
		for (int32 I = 0; I < P.Emission.Num(); ++I) P.Emission[I] = float(G);

		// Its height, eased (the wire's is in 0.1 cell steps).
		const double Want = S.DeadAt ? 0.0 : B.height;
		double H = S.Height.Get(Want);
		H += (Want - H) * (C.Dt > 0.0 ? FMath::Min(1.0, C.Dt * (S.DeadAt ? 2.5 : 6.0)) : 1.0);
		S.Height = H;
		const double Lift = H * AcSpace::CmPerCell - 80.0;

		if (S.DeadAt)
		{
			// The wings stop, it falls tumbling, then lies in the dust.
			const double D = C.Time - *S.DeadAt;
			const double Tumble = Ease(D / 1.2);
			Move(P, M, Thorax.Get(M), FVector(0, 0, FMath::Max(-62.0, Lift)));
			Turn(P, M, Thorax.Get(M), RotX(2.2 * Tumble) * RotY(0.6 * Tumble));
			for (int32 W = 0; W < 4; ++W)
			{
				const double Side = W < 2 ? -1.0 : 1.0;
				Turn(P, M, Wings[W].Get(M), RotX(Side * 0.5 * Tumble));
			}
			for (int32 L = 0; L < 4; ++L) Turn(P, M, Legs[L].Get(M), RotY(0.8 * Tumble));
			return;
		}

		const bool bDive = B.state == ac::Bug::State::dive;
		const double Pitch = bDive ? 0.61 : 0.26 * S.Walk;
		const double Bob = 4.0 * std::sin(T * 3.1);
		Move(P, M, Thorax.Get(M), FVector(0, 0, Lift + Bob));
		Turn(P, M, Thorax.Get(M), RotY(Pitch) * RotX(0.06 * std::sin(T * 1.3)));
		Turn(P, M, Head.Get(M), RotZ(AimAt(C, S, 0.7)) * RotY(K < 1.0 ? -0.25 * std::sin(Pi * K) : 0.0));
		// The wings: about 18 beats a second, the pairs out of phase; a
		// little folded in the dive.
		const double Beat = 2.0 * Pi * 18.0 * C.Time + double(U.id);
		for (int32 W = 0; W < 4; ++W)
		{
			const double Side = W < 2 ? -1.0 : 1.0;
			const double Pair = (W % 2) == 0 ? 0.0 : Pi;
			const double Flap = (bDive ? 0.35 : 0.6) * std::sin(Beat + Pair) - (bDive ? 0.25 : 0.0);
			Turn(P, M, Wings[W].Get(M), RotX(Side * Flap));
		}
		// The lantern sways behind and flares on the spit.
		const double Flare = K < 2.5 ? 2.0 * std::exp(-FMath::Max(0.0, K) * 2.0) : 0.0;
		Turn(P, M, Abdomen.Get(M), RotY(-0.15 * S.Walk + 0.1 * std::sin(T * 1.7)) * RotZ(0.08 * std::sin(T * 1.1)));
		Glow(P, Abdomen.Get(M), G * (1.0 + Flare));
		for (int32 L = 0; L < 4; ++L)
		{
			const double Trail = bDive ? 0.45 : -0.35 * S.Walk + 0.08 * std::sin(T * 2.0 + double(L));
			Turn(P, M, Legs[L].Get(M), RotY(Trail));
		}
	}

	// --- Lodeback ----------------------------------------------------------------

	void PoseLodeback(const FAcPoseContext& C, FAcPose& P)
	{
		static const FAcPartIndex Body(TEXT("body")), Head(TEXT("head")), Jaw(TEXT("jaw")), Sac(TEXT("sac"));
		static const FAcPartIndex Claws[2] = {FAcPartIndex(TEXT("claw_0")), FAcPartIndex(TEXT("claw_1"))};
		static const FAcPartIndex Legs[4] = {FAcPartIndex(TEXT("leg_0")), FAcPartIndex(TEXT("leg_1")), FAcPartIndex(TEXT("leg_2")),
			FAcPartIndex(TEXT("leg_3"))};
		const FAcModelInfo& M = C.Model;
		const ac::Unit& U = *C.Unit;
		const ac::Bug B = U.bug.value_or(ac::Bug{});
		FBugState& S = C.Memory.State<FBugState>();
		const double K = Common(C, S, 0.35);
		const double T = C.Time + double(U.id) * 0.53;
		P.bAnimated = true;
		const double G = GlowOf(C, S, 3.0);
		for (int32 I = 0; I < P.Emission.Num(); ++I) P.Emission[I] = float(G);

		// The sac: its fill eased; 0.55 of its size empty, whole when full.
		const double Fill = FMath::Clamp(B.sac / ac::Brood::sacFull, 0.0, 1.0);
		double F = S.Sac.Get(Fill);
		F += (Fill - F) * (C.Dt > 0.0 ? FMath::Min(1.0, C.Dt * 3.0) : 1.0);
		S.Sac = F;

		if (S.DeadAt)
		{
			// The sac bursts in a flash of gold; the body sinks.
			const double D = C.Time - *S.DeadAt;
			const double Burst = Ease(D / 0.3);
			if (const int32 I = Sac.Get(M); I != INDEX_NONE)
			{
				const double Size = (0.55 + 0.45 * F) * (1.0 + 0.3 * Burst) * (1.0 - Burst);
				P.Local[I].SetScale3D(M.Parts[I].Rest.GetScale3D() * FMath::Max(0.001, Size));
				P.Visible[I] = Burst < 0.99;
				Glow(P, I, 1.0 + 6.0 * std::exp(-D * 8.0));
			}
			const double Sink = Ease(D / 5.0);
			Move(P, M, Body.Get(M), FVector(0, 0, -24.0 * Ease(D / 0.8) - 30.0 * Sink));
			Turn(P, M, Body.Get(M), RotX(0.12 * Ease(D / 0.8)));
			for (int32 L = 0; L < 4; ++L)
			{
				const double Side = L < 2 ? -1.0 : 1.0;
				Turn(P, M, Legs[L].Get(M), RotX(Side * 0.5 * Ease(D / 0.8)));
			}
			return;
		}

		const bool bEating = B.state == ac::Bug::State::eat;
		const double Cycle = U.stride / 0.9;
		const double Lunge = K < 1.0 ? std::sin(Pi * FMath::Clamp(K, 0.0, 1.0)) : 0.0;
		Move(P, M, Body.Get(M), FVector(0, 0, 3.0 * std::abs(std::sin(Pi * 2.0 * Cycle)) * S.Walk + 1.0 * std::sin(T * 0.9)));
		Turn(P, M, Body.Get(M), RotX(0.05 * std::sin(Pi * 2.0 * Cycle) * S.Walk) * RotY(bEating ? 0.08 : 0.0));
		for (int32 L = 0; L < 4; ++L)
		{
			const double Side = L < 2 ? -1.0 : 1.0;
			// 0 and 3 against 1 and 2.
			const double Phase = Pi * 2.0 * (Cycle + ((L == 0 || L == 3) ? 0.0 : 0.5));
			Turn(P, M, Legs[L].Get(M), RotZ(Side * 0.3 * std::sin(Phase) * S.Walk) * RotX(Side * 0.25 * FMath::Max(0.0, std::cos(Phase)) * S.Walk));
		}
		// Eating: the head and claws dig in a loop, the jaw grinds; a bite: the
		// head thrusts, the jaw snaps.
		const double Dig = bEating ? 1.0 : 0.0;
		Move(P, M, Head.Get(M), FVector(15.0 * Lunge, 0, 0));
		Turn(P, M, Head.Get(M), RotZ(AimAt(C, S, 0.4)) * RotY(Dig * (0.3 + 0.15 * std::sin(T * 5.0)) + 0.2 * Lunge));
		Turn(P, M, Jaw.Get(M), RotY(Dig * 0.12 * std::sin(T * 9.0) + (K < 1.0 ? (K < 0.5 ? 0.35 : -0.05) : 0.0)));
		for (int32 Q = 0; Q < 2; ++Q)
		{
			const double Side = Q == 0 ? -1.0 : 1.0;
			Turn(P, M, Claws[Q].Get(M), RotY(Dig * 0.45 * std::sin(T * 5.0 + double(Q) * Pi)) * RotZ(-Side * 0.25 * Lunge));
		}
		if (const int32 I = Sac.Get(M); I != INDEX_NONE)
		{
			const double Pulse = 1.0 + 0.02 * std::sin(T * 2.0) * (bEating ? 1.0 : 0.3);
			P.Local[I].SetScale3D(M.Parts[I].Rest.GetScale3D() * ((0.55 + 0.45 * F) * Pulse));
			// Not past 1: a full sac's gold burns, it doesn't wash white.
			Glow(P, I, G * (0.25 + 0.45 * F));
		}
	}

	// --- The Matriarch (N3: her slam, spit, roar and death) -----------------------

	/// Her memory past a bug's: when her roar began, her last strike's kind.
	struct FQueenState : FBugState
	{
		TOptional<double> RoarAt;
		double Struck = 0.0;
	};

	void PoseMatriarch(const FAcPoseContext& C, FAcPose& P)
	{
		static const FAcPartIndex Body(TEXT("body")), Abdomen(TEXT("abdomen")), Crown(TEXT("crown")), Pods(TEXT("pods")),
			Head(TEXT("head")), Jaw(TEXT("jaw"));
		static const FAcPartIndex Fore[2] = {FAcPartIndex(TEXT("forelimb_0")), FAcPartIndex(TEXT("forelimb_1"))};
		static const FAcPartIndex Legs[6] = {FAcPartIndex(TEXT("leg_0")), FAcPartIndex(TEXT("leg_1")), FAcPartIndex(TEXT("leg_2")),
			FAcPartIndex(TEXT("leg_3")), FAcPartIndex(TEXT("leg_4")), FAcPartIndex(TEXT("leg_5"))};
		const FAcModelInfo& M = C.Model;
		const ac::Unit& U = *C.Unit;
		const ac::Bug B = U.bug.value_or(ac::Bug{});
		FQueenState& S = C.Memory.State<FQueenState>();
		const int64 Before = S.Strikes;
		Common(C, S, 1.0);
		// The strike's kind rides in the sac (`Matriarch::struckSlam`, `struckSpit`).
		if (S.Strikes != Before) S.Struck = B.sac;
		const double Since = C.Time - S.StrikeAt;
		P.bAnimated = true;
		const double G = GlowOf(C, S, 4.0);
		const double T = C.Time;
		const double Breath = std::sin(2.0 * Pi * 0.2 * T);
		for (int32 I = 0; I < P.Emission.Num(); ++I) P.Emission[I] = float(G);

		if (S.DeadAt)
		{
			// She sinks, the crown folds, the glow dies over 4 s.
			const double D = C.Time - *S.DeadAt;
			const double Fall = Ease(D / 1.2), Sink = Ease(D / 4.0);
			Move(P, M, Body.Get(M), FVector(0, 0, -60.0 * Fall - 90.0 * Sink));
			Turn(P, M, Body.Get(M), RotX(0.18 * Fall) * RotY(0.12 * Fall));
			Turn(P, M, Head.Get(M), RotY(0.5 * Fall));
			Turn(P, M, Jaw.Get(M), RotY(0.25 * Fall));
			if (const int32 I = Crown.Get(M); I != INDEX_NONE)
				P.Local[I].SetScale3D(M.Parts[I].Rest.GetScale3D() * FMath::Max(0.05, 1.0 - 0.45 * Fall));
			for (int32 L = 0; L < 6; ++L)
			{
				const double Side = L < 3 ? -1.0 : 1.0;
				Turn(P, M, Legs[L].Get(M), RotX(Side * 0.45 * Fall));
			}
			for (int32 F = 0; F < 2; ++F) Turn(P, M, Fore[F].Get(M), RotY(0.6 * Fall));
			return;
		}

		// The roar: she rears up a cell, the crown fans open, the jaw gapes,
		// the forelimbs brace wide, the egg pods flare; 2 s, eased in and out.
		const bool bRoaring = B.state == ac::Bug::State::roar;
		if (bRoaring && !S.RoarAt) S.RoarAt = C.Time;
		if (!bRoaring && S.RoarAt && C.Time - *S.RoarAt > ac::Matriarch::roarFor + 0.4) S.RoarAt.Reset();
		const double RoarT = S.RoarAt ? C.Time - *S.RoarAt : 99.0;
		const double Roar = RoarT < 99.0 ? Ease(RoarT / 0.35) * (1.0 - Ease((RoarT - ac::Matriarch::roarFor) / 0.4)) : 0.0;
		const double Shudder = Roar * 0.03 * std::sin(T * 31.0);

		// The slam: both forelimbs rise over 0.8 s, crash down, and come back.
		double Lift = 0.0, Crash = 0.0;
		if (ac::Matriarch::slammed(S.Struck) && Since >= 0.0 && Since < 2.0)
		{
			const double W = ac::Matriarch::slamWindup;
			if (Since < W) Lift = Ease(Since / W);
			else
			{
				const double After = Since - W;
				Lift = 1.0 - Ease(After / 0.12);
				Crash = After < 0.12 ? Ease(After / 0.12) : 1.0 - Ease((After - 0.12) / 0.9);
			}
		}
		// The spit: the head draws back, then lunges, the jaw wide.
		double Draw = 0.0, Lunge = 0.0;
		if (ac::Matriarch::spat(S.Struck) && Since >= 0.0 && Since < 1.0)
		{
			Draw = Since < 0.18 ? Ease(Since / 0.18) : 1.0 - Ease((Since - 0.18) / 0.12);
			Lunge = Since < 0.18 ? 0.0 : Since < 0.32 ? Ease((Since - 0.18) / 0.14) : 1.0 - Ease((Since - 0.32) / 0.5);
		}

		// The body: her breath, the rear of the roar, the slam's lean and
		// crash forward, a slow sway when she walks.
		const double Cycle = U.stride / 2.2;
		for (const FAcPartIndex* X : {&Body, &Abdomen})
			if (const int32 I = X->Get(M); I != INDEX_NONE) P.Local[I].SetScale3D(M.Parts[I].Rest.GetScale3D() * (1.0 + 0.03 * Breath));
		Move(P, M, Body.Get(M), FVector(-20.0 * Lift + 25.0 * Crash, 0, 100.0 * Roar + 25.0 * Lift - 30.0 * Crash + 6.0 * Breath));
		Turn(P, M, Body.Get(M), RotY(-0.22 * Roar - 0.1 * Lift + 0.14 * Crash) * RotX(0.04 * std::sin(Pi * 2.0 * Cycle) * S.Walk + Shudder));
		// The egg sac swells and pulses brighter on the roar.
		Glow(P, Abdomen.Get(M), G * (1.0 + 0.15 * Breath + 1.2 * Roar));
		// The head: to her target, lifted on the roar, back and forth on the spit.
		Move(P, M, Head.Get(M), FVector(-40.0 * Draw + 70.0 * Lunge, 0, 20.0 * Roar));
		Turn(P, M, Head.Get(M), RotZ(AimAt(C, S, 0.5)) * RotY(-0.35 * Roar - 0.25 * Draw + 0.2 * Lunge + 0.25 * Crash));
		Turn(P, M, Jaw.Get(M), RotY(0.55 * Roar + 0.45 * FMath::Max(Draw, Lunge) + 0.04 * (1.0 + Breath)));
		// The crown: breathes, fans open on the roar.
		if (const int32 I = Crown.Get(M); I != INDEX_NONE)
		{
			P.Local[I].SetScale3D(M.Parts[I].Rest.GetScale3D() * (1.0 + 0.35 * Roar + 0.02 * Breath));
			Turn(P, M, I, RotY(-0.3 * Roar));
		}
		Glow(P, Crown.Get(M), G * (1.0 + 0.25 * Breath + 1.5 * Roar));
		Glow(P, Pods.Get(M), G * (1.0 + 0.2 * Breath + 2.5 * Roar));
		// The forelimbs: up for the slam, down in the crash, braced wide on the roar.
		for (int32 F = 0; F < 2; ++F)
		{
			const double Side = F == 0 ? -1.0 : 1.0;
			Turn(P, M, Fore[F].Get(M), RotY(-1.15 * Lift + 0.35 * Crash) * RotX(Side * 0.35 * Roar));
		}
		// The legs: a slow shifting step as she walks (0, 2, 4 against 1, 3, 5).
		for (int32 L = 0; L < 6; ++L)
		{
			const double Side = L < 3 ? -1.0 : 1.0;
			const double Phase = Pi * 2.0 * (Cycle + ((L % 2) == 0 ? 0.0 : 0.5));
			Turn(P, M, Legs[L].Get(M),
				RotZ(Side * 0.18 * std::sin(Phase) * S.Walk) * RotX(Side * (0.15 * FMath::Max(0.0, std::cos(Phase)) * S.Walk - 0.12 * Roar)));
		}
	}

	FAcPoseRegistration RegShardling(ac::UnitKind::shardling, &PoseShardling);
	FAcPoseRegistration RegGloamwing(ac::UnitKind::gloamwing, &PoseGloamwing);
	FAcPoseRegistration RegLodeback(ac::UnitKind::lodeback, &PoseLodeback);
	FAcPoseRegistration RegMatriarch(ac::UnitKind::matriarch, &PoseMatriarch);
}
