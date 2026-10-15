// The MMORPG world's Dropship flight as the player sees it (F2,
// docs/mmorpg-m8.md; the server's rule: `WorldMap::fly`, `ac::Ferry`). The
// Prospector does not fly by itself: it stands on a Landing Pad and calls the
// Dropship, which comes down onto the pad, takes it aboard, lifts off, banks
// through two turns with the ringed giant swinging through the view, dashes
// across the sky through streaking cloud, then slows and comes down onto the
// target pad, where the Prospector steps out. Drawn by the pilot pawn
// (`AAcPilotPawn`, AcPilotFlight.cpp) from the World message's ferry
// (`FAcMmoClient::Ferry`): the stages are the server's (`ac::Ferry::Plan`),
// timed on the real-seconds clock from the call.
//
//   descend   the Dropship comes in over the camera from behind, slowing,
//             down onto the boarding pad on its retro jets; the camera
//             leaves the pilot's view for a low spot by the pad, the giant
//             beyond.
//   board     a copy of the Prospector (the server took it aboard at the
//             call: it is in no region, nothing can touch it) walks up the
//             ramp at the ship's back; the camera closer.
//   lift      off the pad, accelerating up, dust racing out under it.
//   turns     climbing through two banking turns from the giant's way onto
//             the target's; the camera chases.
//   dash      the speed burst across the sky, high, clouds streaking past;
//             the chase tight behind it.
//   approach  the pilot is in the target region now (the server's switch,
//             a seamless region change: its ground and mirror were made
//             ahead, the world frame moves under the ship, nothing redraws);
//             the ship slows and comes down onto the target pad, seen wide.
//   step out  the touch down: the ramp, the Prospector walking off onto the
//             pad where the server put it; the camera into the pilot's view,
//             control back at the end. The ship lifts away by itself after.
//
// Everything is drawn in Unreal space, the session's frame (AcSpace.h): the
// two pads keep their places through the region change.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Flight.h"

#include "AcPilotFlight.generated.h"

class UStaticMeshComponent;

enum class EAcFlightStage : uint8
{
	None,
	Descend,
	Board,
	Lift,
	Turns,
	Dash,
	Approach,
	StepOut,
};

namespace AcPilotFlight
{
	/// Landing Pads the Dropship is over this frame (Unreal, session frame):
	/// their lamps flash (`PoseLandingPad`).
	inline TArray<FVector> GBusyPads;
	inline bool PadBusy(const FVector& At)
	{
		for (const FVector& P : GBusyPads)
		{
			if (FVector::DistSquaredXY(P, At) < 200.0 * 200.0) return true;
		}
		return false;
	}

	/// W5: the other human pilots' flights (the sky's, `Sky::Human::ferry`),
	/// each frame: while this pilot is in the region one leaves, the
	/// Dropship coming down onto its pad, its Prospector walking up the ramp,
	/// the lift and the way out; while in the region it comes to, the
	/// approach and the touch down. The game flow's tick calls it.
	void TickWatched(UWorld* World);
	/// The flights drawn this frame: each pilot's callsign and its stage.
	TArray<TPair<FString, EAcFlightStage>> Watched();

	/// The ship drawn bigger than the strategy game's Dropship: it carries a
	/// Prospector, and fills a 3-cell pad.
	inline constexpr double ShipScale = 1.6;
	/// How long the ship takes to lift away after the step out (s).
	inline constexpr double DepartSeconds = 6.0;
	/// The cruise height over the target pad where the approach starts, and
	/// how far back along the way (cells).
	inline constexpr double ApproachHigh = 40.0;
	inline constexpr double ApproachBack = 60.0;

	/// Smootherstep: no jolt at either end.
	inline double Ease(const double T)
	{
		const double S = FMath::Clamp(T, 0.0, 1.0);
		return S * S * S * (S * (S * 6.0 - 15.0) + 10.0);
	}

	/// A cubic Hermite curve: from `P0` (tangent `M0`) to `P1` (tangent
	/// `M1`), `U` 0..1; tangents per unit of `U`.
	inline FVector Hermite(const FVector& P0, const FVector& M0, const FVector& P1, const FVector& M1, const double U)
	{
		const double U2 = U * U, U3 = U2 * U;
		return P0 * (2 * U3 - 3 * U2 + 1) + M0 * (U3 - 2 * U2 + U) + P1 * (-2 * U3 + 3 * U2) + M1 * (U3 - U2);
	}

	/// The Dropship's way (F2): from the boarding pad `A` to the target pad
	/// `B` (Unreal cm, on the ground), the giant's way on the ground `Giant`,
	/// the server's stages. Times are seconds since the call.
	struct FPath
	{
		FVector A = FVector::ZeroVector;
		FVector B = FVector::ZeroVector;
		FVector Giant = FVector::ForwardVector;
		ac::Ferry::Plan Plan;

		static double Cm(const double Cells) { return Cells * 100.0; }
		/// Flat, from A toward B; the giant's way for a hop on one ground.
		FVector Way() const
		{
			FVector D = B - A;
			D.Z = 0;
			return D.Size() > Cm(30) ? D.GetSafeNormal() : Giant;
		}
		FVector Side(const FVector& W) const { return FVector::CrossProduct(FVector::UpVector, W); }

		EAcFlightStage Stage(const double T) const
		{
			if (T < Plan.boardAt()) return EAcFlightStage::Descend;
			if (T < Plan.liftAt()) return EAcFlightStage::Board;
			if (T < Plan.turnsAt()) return EAcFlightStage::Lift;
			if (T < Plan.dashAt()) return EAcFlightStage::Turns;
			if (T < Plan.approachAt()) return EAcFlightStage::Dash;
			if (T < Plan.total()) return EAcFlightStage::Approach;
			return EAcFlightStage::StepOut;
		}

		/// Where the descent starts: behind the low camera and high.
		FVector DescendFrom() const { return A - Giant * Cm(45) + FVector::UpVector * Cm(32); }
		/// The turns' Bezier points: off the lift's top, climbing, from the
		/// giant's way onto the target's.
		void TurnPoints(FVector P[4]) const
		{
			const FVector W = Way();
			P[0] = A + FVector::UpVector * Cm(10);
			P[1] = P[0] + FVector::UpVector * Cm(5) + Giant * Cm(18);
			P[3] = P[0] + FVector::UpVector * Cm(22) + Giant * Cm(20) + W * Cm(26);
			P[2] = P[3] - W * Cm(16) - FVector::UpVector * Cm(4);
		}
		/// The approach's start: high over the way back from B.
		FVector ApproachFrom() const { return B - Way() * Cm(ApproachBack) + FVector::UpVector * Cm(ApproachHigh); }
		/// The dash's tangents (per unit of its `U`): out of the turns at
		/// their speed, into the approach still fast.
		FVector DashOut() const
		{
			FVector P[4];
			TurnPoints(P);
			return (P[3] - P[2]) * 3.0 / Plan.turns * Plan.dash;
		}
		FVector DashIn() const
		{
			FVector P[4];
			TurnPoints(P);
			const double Far = FVector::Dist(P[3], ApproachFrom());
			return (Way() * Far * 0.9 - FVector::UpVector * Cm(4)) ;
		}

		/// The ship's place at `T` (its belly's centre, cm).
		FVector Pos(const double T) const
		{
			const double D = Plan.descend, Bd = Plan.board, L = Plan.lift, Tn = Plan.turns, Ds = Plan.dash, Ap = Plan.approach;
			if (T < Plan.boardAt())
			{
				const double U = FMath::Clamp(T / D, 0.0, 1.0);
				const FVector S = DescendFrom();
				return Hermite(S, (A - S) * 1.4 + FVector::UpVector * Cm(4), A, -FVector::UpVector * Cm(1.5), U);
			}
			if (T < Plan.liftAt()) return A;
			if (T < Plan.turnsAt())
			{
				const double U = (T - Plan.liftAt()) / L;
				return A + FVector::UpVector * Cm(10) * U * U;
			}
			if (T < Plan.dashAt())
			{
				FVector P[4];
				TurnPoints(P);
				const double U = (T - Plan.turnsAt()) / Tn;
				const double V = 1 - U;
				const FVector Bez = P[0] * V * V * V + P[1] * 3 * V * V * U + P[2] * 3 * V * U * U + P[3] * U * U * U;
				// Two banks: a weave across the way, none at either end.
				return Bez + Side(Way()) * Cm(7) * FMath::Sin(UE_TWO_PI * U) * 4 * U * V;
			}
			if (T < Plan.approachAt())
			{
				FVector P[4];
				TurnPoints(P);
				const double U = (T - Plan.dashAt()) / Ds;
				return Hermite(P[3], DashOut(), ApproachFrom(), DashIn(), U);
			}
			const double U = FMath::Clamp((T - Plan.approachAt()) / Ap, 0.0, 1.0);
			// In at the dash's last speed, slowing to settle on the pad.
			return Hermite(ApproachFrom(), DashIn() / Ds * Ap, B, -FVector::UpVector * Cm(2), U);
		}

		/// The ship's turn at `T`: along its way (the giant's on the pads),
		/// the nose a little up or down with the climb, banked into the turns.
		FQuat Rot(const double T) const
		{
			const EAcFlightStage S = Stage(T);
			if (S == EAcFlightStage::Descend || S == EAcFlightStage::Board || S == EAcFlightStage::Lift)
				return FRotator(0, Giant.Rotation().Yaw, 0).Quaternion();
			const double H = 0.05;
			const FVector V = Pos(FMath::Min(T + H, Plan.total() - 0.3)) - Pos(FMath::Min(T - H, Plan.total() - 0.4));
			FVector Flat(V.X, V.Y, 0);
			const double Yaw = Flat.Size() > 2.0 ? Flat.Rotation().Yaw : Way().Rotation().Yaw;
			const double Pitch = Flat.Size() > 2.0 ? FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(V.Z, Flat.Size())) * 0.5, -15.0, 18.0) : 0.0;
			// The bank: from how fast the way turns.
			auto YawAt = [&](const double Q) {
				const FVector Vq = Pos(FMath::Min(Q + H, Plan.total() - 0.3)) - Pos(FMath::Min(Q - H, Plan.total() - 0.4));
				return FVector(Vq.X, Vq.Y, 0).Rotation().Yaw;
			};
			const double Rate = FMath::FindDeltaAngleDegrees(YawAt(T - 0.15), YawAt(T + 0.15)) / 0.3;
			const double Roll = S == EAcFlightStage::StepOut ? 0.0 : FMath::Clamp(Rate * 0.6, -40.0, 40.0);
			return FRotator(Pitch, Yaw, Roll).Quaternion();
		}

		/// The jets' strength (0..1).
		double Thrust(const double T) const
		{
			switch (Stage(T))
			{
			case EAcFlightStage::Descend: return 0.35 + 0.65 * FMath::SmoothStep(0.4, 1.0, T / Plan.descend);
			case EAcFlightStage::Board: return 0.15;
			case EAcFlightStage::Lift: return 1.0;
			case EAcFlightStage::Turns: return 0.6;
			case EAcFlightStage::Dash: return 1.0;
			case EAcFlightStage::Approach: return 0.4 + 0.6 * FMath::SmoothStep(0.35, 0.9, (T - Plan.approachAt()) / Plan.approach);
			default: return 0.1;
			}
		}
	};
}

/// A model at rest for the flight (AcModelRow's way: static meshes with the
/// model's materials, its team colour): the Dropship, and the Prospector's
/// copy walking aboard and off. Only the picture: the real unit is out of
/// every region aboard, and hidden until it has stepped out.
UCLASS()
class AUTOCRAFT_API AAcFlightShip : public AActor
{
	GENERATED_BODY()

public:
	AAcFlightShip();

	/// The one named `Name` (the Dropship "AcFlightShip", the Prospector's
	/// copy aboard "AcFlightRider"), spawned on first ask.
	static AAcFlightShip* SpawnFor(UWorld* World, FName Name = TEXT("AcFlightShip"));

	/// The model `Model` of team `Team` placed at `Placement` (the renderer's
	/// placement: the ground point under the unit, turned to its heading),
	/// lifted `Lift` cm. False when the model is unknown.
	bool Show(FName Model, int32 Team, const FTransform& Placement, double Lift);
	/// Its size (1: the model's).
	void SetScale(double Scale);
	/// Builds the model's meshes now, hidden (the first flight's call does
	/// not build them in its frame).
	void Prepare(FName Model, int32 Team);
	/// Its lights' glow (the models' emission scale; 1 at rest): the engines
	/// lit for the flight.
	void SetGlow(double Glow);
	void Hide();
	bool IsShown() const { return bShown; }

private:
	void Build(FName Model, int32 Team);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	FName Key;
	int32 KeyTeam = -1;
	bool bShown = false;
};
