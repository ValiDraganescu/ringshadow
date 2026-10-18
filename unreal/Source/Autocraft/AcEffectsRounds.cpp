// The minigun class's rounds (C2, docs/guns-and-practice.md §1, §6): an
// effects extension (AcEffectsExtension.h) that draws what the core's rounds
// do. Each `GameEvent::Fired` (a Citadel corner gun's, a gatling gun's)
// becomes a red-hot streak flown on the core's parabola (`FAcRoundFlight`,
// AcRounds.h) until its `Impact` or `Rules::roundLife`: a yellow-white core
// 1.5 cells long in an orange-red halo. Leaving the muzzle it starts at the
// model's muzzle (the turret as posed), the offset to the core's muzzle
// fading out over its first 0.15 s. Each round flashes at the muzzle; every
// tenth puffs smoke, every fifth throws a spent casing.
//
// Impacts by what was struck: the ground, a dust plume, sparks and a small
// scorch, scaled by the round's speed there; a building or rock, sparks and
// a flash; a bug, a burst of ichor and chitin and a hit flash; a Target
// Dummy, a flash (its rings and the HITS count are its pose's and its life
// bar's).
//
// Pools: 256 streak cores and halos (four guns at 30 rounds a second, a
// second in the air), 64 casings, 96 ichor bits.
#include "AcEffects.h"
#include "AcEffectsExtension.h"
#include "AcEffectPool.h"
#include "AcRounds.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "Rules.h"
#include "Types.h"

#include <cmath>

namespace
{
	constexpr double Cm = AcSpace::CmPerCell;
	/// The streak's length, cells, and its core's and halo's thickness.
	constexpr double StreakLength = 1.5;
	constexpr double CoreWidth = 0.04;
	constexpr double HaloWidth = 0.11;
	/// A muzzle this close to the camera (the gun the pilot mans) makes no
	/// flash, smoke or casing of its own: the turret's model flashes there.
	constexpr double EyeClear = 3.0;
	/// The model's muzzle to the core's: faded out over this, seconds.
	constexpr double OffsetFade = 0.15;
	/// The bug's ichor (a sickly green-yellow) and a spent casing's brass.
	const FLinearColor Ichor(0.72f, 0.95f, 0.22f);
	const FLinearColor Brass(0.62f, 0.45f, 0.16f);

	FVector World(const ac::Vec3 P) { return AcSpace::ToWorld(ac::Vec2(P.x, P.y), P.z); }

	/// A small thing thrown (a casing, a fleck of ichor): flies under
	/// gravity and is gone after `Life`.
	struct FBit
	{
		FVector At = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		double Born = 0;
		double Life = 0;
		double Size = 1;
		bool bLive = false;
	};

	class FAcRoundsExtension final : public FAcEffectsExtension
	{
	public:
		virtual void Begin(UAcEffects& InEffects) override
		{
			E = &InEffects;
			Core = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(FLinearColor(1.f, 0.93f, 0.72f), 14.0), 256, TEXT("AcRoundCore"));
			Halo = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(FLinearColor(1.f, 0.36f, 0.08f), 3.2, true), 256, TEXT("AcRoundHalo"));
			Casings = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(Brass, 0.5), 64, TEXT("AcRoundCasing"));
			Flecks = &E->MakePool(E->SphereMesh(), E->EmissiveMaterial(Ichor, 2.0, true), 96, TEXT("AcRoundIchor"));
			Flights.SetNum(Core->Num());
			Live.Init(false, Core->Num());
			Offsets.Init(FVector::ZeroVector, Core->Num());
			CasingBits.SetNum(Casings->Num());
			FleckBits.SetNum(Flecks->Num());
		}

		virtual void Consume(const FAcFrame& Frame) override
		{
			if (!E || !Frame.State) return;
			for (const ac::GameEvent& Ev : Frame.Events)
			{
				if (const auto* F = Ev.as<ac::GameEvent::Fired>()) Fired(*Frame.State, *F, Frame.Time);
				else if (const auto* I = Ev.as<ac::GameEvent::Impact>()) Impact(*Frame.State, *I, Frame.Time);
			}
		}

		virtual void Update(const double Time) override
		{
			if (!E) return;
			for (int32 I = 0; I < Flights.Num(); ++I)
			{
				if (!Live[I]) continue;
				const FAcRoundFlight& R = Flights[I];
				if (R.Expired(Time))
				{
					EndRound(I);
					continue;
				}
				// Head and tail on the core's path, the model's muzzle offset
				// fading out as each leaves it.
				const double TailTime = FMath::Max(R.Born, Time - StreakLength / FMath::Max(R.Speed(Time), 1e-3));
				const FVector Head = World(R.At(Time)) + Offsets[I] * Fade(Time - R.Born);
				const FVector Tail = World(R.At(TailTime)) + Offsets[I] * Fade(TailTime - R.Born);
				Place(I, Tail, Head);
			}
			Fly(*Casings, CasingBits, Time, true);
			Fly(*Flecks, FleckBits, Time, false);
		}

		virtual void Clear() override
		{
			for (int32 I = 0; I < Live.Num(); ++I)
				if (Live[I]) EndRound(I);
			Slots.Reset();
			for (FBit& B : CasingBits) B.bLive = false;
			for (FBit& B : FleckBits) B.bLive = false;
			if (Casings) for (int32 I = 0; I < Casings->Num(); ++I) Casings->Hide(I);
			if (Flecks) for (int32 I = 0; I < Flecks->Num(); ++I) Flecks->Hide(I);
			Fires.Reset();
		}

	private:
		static double Fade(const double Age) { return FMath::Clamp(1.0 - Age / OffsetFade, 0.0, 1.0); }

		void Place(const int32 I, const FVector& Tail, const FVector& Head)
		{
			const FVector D = Head - Tail;
			const double Len = D.Size();
			if (Len < 1.0)
			{
				Core->Hide(I);
				Halo->Hide(I);
				return;
			}
			const FVector Dir = D / Len, Mid = (Head + Tail) / 2;
			Core->Set(I, Core->Shape(Mid, FAcEffectPool::Along(Dir), FVector(CoreWidth * Cm, CoreWidth * Cm, Len)));
			Halo->Set(I, Halo->Shape(Mid, FAcEffectPool::Along(Dir), FVector(HaloWidth * Cm, HaloWidth * Cm, Len * 1.05)));
		}

		void EndRound(const int32 I)
		{
			Live[I] = false;
			Slots.Remove(Flights[I].Round);
			Core->Hide(I);
			Halo->Hide(I);
		}

		bool NearEye(const FVector& At) const
		{
			const UWorld* World = E->GetWorld();
			const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			return PC && PC->PlayerCameraManager && FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), At) < EyeClear * Cm;
		}

		/// Where the shooter's model fires from now (cm), if drawn.
		bool ModelMuzzle(const ac::GameState& State, const ac::GameEvent::Fired& F, FVector& Out) const
		{
			const std::optional<ac::Structure> St = State.structure(F.shooter);
			if (!St) return false;
			if (St->kind == ac::StructureKind::gatling) return E->PartLocation(F.shooter, TEXT("muzzles_0"), Out);
			if (St->kind == ac::StructureKind::citadel && St->guns)
			{
				const ac::Vec2 Ahead(F.muzzle.x + F.velocity.x, F.muzzle.y + F.velocity.y);
				return E->CitadelMuzzle(*St, Ahead, Out);
			}
			return false;
		}

		void Fired(const ac::GameState& State, const ac::GameEvent::Fired& F, const double Time)
		{
			const int32 I = Core->Next();
			if (Live[I]) EndRound(I);
			FAcRoundFlight& R = Flights[I];
			R.Round = F.round;
			R.Shooter = F.shooter;
			R.Muzzle = F.muzzle;
			R.Velocity = F.velocity;
			R.Born = Time;
			Live[I] = true;
			Slots.Add(F.round, I);
			const FVector CoreMuzzle = World(F.muzzle);
			FVector Muzzle;
			const bool bModel = ModelMuzzle(State, F, Muzzle);
			// A muzzle far from the core's (a model not drawn yet, a lag): none.
			Offsets[I] = bModel && FVector::Dist(Muzzle, CoreMuzzle) < 3 * Cm ? Muzzle - CoreMuzzle : FVector::ZeroVector;
			if (!bModel) Muzzle = CoreMuzzle;
			Place(I, Muzzle, Muzzle);

			// The muzzle flash every round; smoke every tenth; a casing every fifth.
			const FVector Along = (World(F.muzzle + F.velocity) - CoreMuzzle).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
			int32& N = Fires.FindOrAdd(F.shooter);
			++N;
			if (NearEye(Muzzle)) return;
			E->Flash(Muzzle + Along * 0.15 * Cm, 1.5, Time, 0.035);
			if (N % 10 == 0) E->DustPuff(Muzzle + Along * 0.3 * Cm, 0.35, Time);
			if (N % 5 == 0)
			{
				const FVector Side = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
				Throw(*Casings, CasingBits, Muzzle - Along * 0.7 * Cm, (Side * 2.2 + FVector::UpVector * 1.6 - Along * 0.5) * Cm, Time, 0.7,
					1.0);
			}
		}

		void Impact(const ac::GameState& State, const ac::GameEvent::Impact& I, const double Time)
		{
			double Speed = ac::Rules::roundSpeed;
			FVector Along = FVector::ZeroVector;
			if (const int32* Slot = Slots.Find(I.round))
			{
				Speed = Flights[*Slot].Speed(Time);
				const ac::Vec3 V = Flights[*Slot].VelocityAt(Time);
				Along = (World(ac::Vec3(V.x, V.y, V.z)) - World(ac::Vec3(0, 0, 0))).GetSafeNormal();
				EndRound(*Slot);
			}
			const FVector At = World(I.at);
			// Harder and bigger the faster it came (a long lob lands soft).
			const double K = FMath::Clamp(Speed / ac::Rules::roundSpeed, 0.4, 1.0);
			using EWhat = ac::GameEvent::Impact::What;
			FAcBurstRequest Sparks;
			Sparks.Kind = EAcBurst::Ricochet;
			Sparks.At = At;
			Sparks.Time = Time;
			Sparks.Duration = 0.08;
			if (!Along.IsNearlyZero()) Sparks.Along = Along.MirrorByVector(FVector::UpVector);
			switch (I.what)
			{
			case EWhat::ground:
			{
				E->DustPuff(At, 0.7 * K, Time);
				Sparks.Size = 1.2 * K;
				E->Burst(Sparks);
				FAcScorchRequest S;
				S.At = FVector(At.X, At.Y, E->GroundZ(ac::Vec2(I.at.x, I.at.y)) + 1);
				S.Radius = 0.22 * Cm * K;
				S.Life = 6;
				S.Time = Time;
				S.Darkness = 0.7;
				E->Scorch(S);
				break;
			}
			case EWhat::structure:
			{
				const std::optional<ac::Structure> St = State.structure(I.victim);
				const bool bDummy = St && St->kind == ac::StructureKind::dummy;
				E->Flash(At, bDummy ? 1.3 : 1.7 * K, Time, 0.07);
				Sparks.Size = (bDummy ? 1.4 : 2.0) * K;
				E->Burst(Sparks);
				break;
			}
			case EWhat::unit:
			{
				const std::optional<ac::Unit> U = State.unit(I.victim);
				if (U && U->bug)
				{
					// Chitin and ichor: a hit flash and a spray of green flecks.
					E->Flash(At, 1.2, Time, 0.06);
					for (int32 B = 0; B < 6; ++B)
					{
						const double A = (B + FMath::FRand() * 0.6) * 2 * UE_DOUBLE_PI / 6;
						const FVector Out = FVector(std::cos(A), std::sin(A), 0.9 + FMath::FRand()) * (1.4 + FMath::FRand()) * Cm
							+ (Along.IsNearlyZero() ? FVector::ZeroVector : Along * 1.2 * Cm);
						Throw(*Flecks, FleckBits, At, Out, Time, 0.32, 0.8 + 0.5 * FMath::FRand());
					}
				}
				else
				{
					E->Flash(At, 1.4 * K, Time, 0.06);
					Sparks.Size = 1.5 * K;
					E->Burst(Sparks);
				}
				break;
			}
			}
		}

		void Throw(FAcEffectPool& Pool, TArray<FBit>& Bits, const FVector& At, const FVector& Velocity, const double Time, const double Life,
			const double Size)
		{
			const int32 I = Pool.Next();
			Bits[I] = {At, Velocity, Time, Life, Size, true};
		}

		/// Bits fly under gravity (cells/s² as the rounds'), tumbling casings
		/// along their way, flecks shrinking as they go.
		void Fly(FAcEffectPool& Pool, TArray<FBit>& Bits, const double Time, const bool bCasing)
		{
			for (int32 I = 0; I < Bits.Num(); ++I)
			{
				FBit& B = Bits[I];
				if (!B.bLive) continue;
				const double T = Time - B.Born;
				if (T >= B.Life || T < 0)
				{
					B.bLive = false;
					Pool.Hide(I);
					continue;
				}
				const FVector P = B.At + B.Velocity * T + FVector(0, 0, -0.5 * ac::Rules::roundGravity * Cm * T * T);
				if (bCasing)
				{
					const FQuat Spin(FVector(B.Velocity.Y, -B.Velocity.X, 0).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector), T * 25.0);
					Pool.Set(I, Pool.Shape(P, Spin, FVector(0.035 * Cm, 0.035 * Cm, 0.12 * Cm) * B.Size));
				}
				else
				{
					const double D = 0.07 * Cm * B.Size * (1.0 - T / B.Life);
					Pool.Set(I, Pool.Shape(P, FQuat::Identity, FVector(D)));
				}
			}
		}

		UAcEffects* E = nullptr;
		FAcEffectPool* Core = nullptr;
		FAcEffectPool* Halo = nullptr;
		FAcEffectPool* Casings = nullptr;
		FAcEffectPool* Flecks = nullptr;
		TArray<FAcRoundFlight> Flights;
		TArray<bool> Live;
		TArray<FVector> Offsets;
		/// The round's id → its slot, while it flies.
		TMap<int64, int32> Slots;
		/// Rounds fired by each shooter (the smoke's and the casings' count).
		TMap<int64, int32> Fires;
		TArray<FBit> CasingBits, FleckBits;
	};

	FAcEffectsExtensionRegistration RegRounds(TEXT("rounds"), 40, [] { return TUniquePtr<FAcEffectsExtension>(new FAcRoundsExtension); });
}
