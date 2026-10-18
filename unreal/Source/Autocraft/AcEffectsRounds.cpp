// The minigun class's rounds (C2, docs/guns-and-practice.md §1, §6): an
// effects extension (AcEffectsExtension.h) that draws what the core's rounds
// do. Each `GameEvent::Fired` (a Citadel corner gun's, a gatling gun's)
// becomes a red-hot tracer flown on the core's parabola (`FAcRoundFlight`,
// AcRounds.h) until its `Impact` or `Rules::roundLife`: an orange-red body
// 1.5 cells long, a yellow-white core in its front part, an additive
// orange-red halo round it and a dimmer tail 1.5 cells on behind it, all
// emissive well over daylight. Leaving the muzzle it starts at the
// model's muzzle (the turret as posed), the offset to the core's muzzle
// fading out over its first 0.15 s. Each round flashes at the muzzle; every
// tenth puffs smoke, every fifth throws a spent casing.
//
// Impacts by what was struck, scaled by the round's speed there: the ground,
// an orange flash, hot sparks that arc out and fall, a scorch and now and
// then a dust plume; a building or rock, a flash and sparks; a
// bug, a hit flash and a burst of ichor flecks; a Target Dummy, a flash and
// sparks (its rings and the HITS count are its pose's and the HUD's).
//
// Pools: 256 tracers of four parts (four guns at 30 rounds a second, a
// second in the air), 64 impact glows, 256 sparks, 64 casings, 192 ichor
// flecks.
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
	/// The tracer: its body's length (the tail as long again behind it), the
	/// share of it the white-hot core runs along from the front, and each
	/// part's thickness, cells.
	constexpr double StreakLength = 1.5;
	constexpr double CoreShare = 0.6;
	constexpr double CoreWidth = 0.02;
	constexpr double BodyWidth = 0.045;
	constexpr double HaloWidth = 0.12;
	constexpr double TailWidth = 0.06;
	/// A muzzle this close to the camera (the gun the pilot mans) makes no
	/// flash, smoke or casing of its own: the turret's model flashes there.
	constexpr double EyeClear = 3.0;
	/// The model's muzzle to the core's: faded out over this, seconds.
	constexpr double OffsetFade = 0.15;
	/// The tracer's colours: white-hot, red-hot, its glow; an impact's
	/// orange flash and its sparks; the bug's ichor (a sickly green-yellow);
	/// a spent casing's brass.
	const FLinearColor WhiteHot(1.f, 0.9f, 0.62f);
	const FLinearColor RedHot(1.f, 0.3f, 0.05f);
	const FLinearColor HotGlow(1.f, 0.2f, 0.03f);
	const FLinearColor Fire(1.f, 0.32f, 0.04f);
	const FLinearColor Spark(1.f, 0.72f, 0.28f);
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
			Core = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(WhiteHot, 40.0), 256, TEXT("AcRoundCore"));
			Body = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(RedHot, 18.0), 256, TEXT("AcRoundBody"));
			Halo = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(HotGlow, 6.0, true), 256, TEXT("AcRoundHalo"));
			Tail = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(HotGlow, 2.2, true), 256, TEXT("AcRoundTail"));
			Glows = &E->MakePool(E->SphereMesh(), E->EmissiveMaterial(Fire, 0.9, true), 64, TEXT("AcRoundGlow"));
			Sparks = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(Spark, 3.5, true), 256, TEXT("AcRoundSpark"));
			Casings = &E->MakePool(E->CylinderMesh(), E->EmissiveMaterial(Brass, 0.5), 64, TEXT("AcRoundCasing"));
			Flecks = &E->MakePool(E->SphereMesh(), E->EmissiveMaterial(Ichor, 3.0, true), 192, TEXT("AcRoundIchor"));
			Flights.SetNum(Core->Num());
			Live.Init(false, Core->Num());
			Offsets.Init(FVector::ZeroVector, Core->Num());
			CasingBits.SetNum(Casings->Num());
			FleckBits.SetNum(Flecks->Num());
			SparkBits.SetNum(Sparks->Num());
			GlowBits.SetNum(Glows->Num());
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
				// Points on the core's path so many cells back from the head,
				// the model's muzzle offset fading out as each leaves it.
				const double Speed = FMath::Max(R.Speed(Time), 1e-3);
				auto Back = [&](const double Cells)
				{
					const double T = FMath::Max(R.Born, Time - Cells / Speed);
					return World(R.At(T)) + Offsets[I] * Fade(T - R.Born);
				};
				Place(I, Back(0), Back(StreakLength * CoreShare), Back(StreakLength), Back(2 * StreakLength));
			}
			Fly(*Casings, CasingBits, Time, EBit::Casing);
			Fly(*Flecks, FleckBits, Time, EBit::Fleck);
			Fly(*Sparks, SparkBits, Time, EBit::Spark);
			Fly(*Glows, GlowBits, Time, EBit::Glow);
		}

		virtual void Clear() override
		{
			for (int32 I = 0; I < Live.Num(); ++I)
				if (Live[I]) EndRound(I);
			Slots.Reset();
			for (TArray<FBit>* Bits : {&CasingBits, &FleckBits, &SparkBits, &GlowBits})
				for (FBit& B : *Bits) B.bLive = false;
			for (FAcEffectPool* Pool : {Casings, Flecks, Sparks, Glows})
				if (Pool) for (int32 I = 0; I < Pool->Num(); ++I) Pool->Hide(I);
			Fires.Reset();
		}

	private:
		static double Fade(const double Age) { return FMath::Clamp(1.0 - Age / OffsetFade, 0.0, 1.0); }

		/// One part of tracer `I` from `From` back to `To` (cm), `Width` cells thick.
		static void Segment(FAcEffectPool& Pool, const int32 I, const FVector& From, const FVector& To, const double Width)
		{
			const FVector D = From - To;
			const double Len = D.Size();
			if (Len < 1.0)
			{
				Pool.Hide(I);
				return;
			}
			Pool.Set(I, Pool.Shape((From + To) / 2, FAcEffectPool::Along(D / Len), FVector(Width * Cm, Width * Cm, Len)));
		}

		/// Tracer `I`: the core from its head back to `CoreEnd`, the body
		/// and its halo to `BodyEnd`, the tail on to `TailEnd`.
		void Place(const int32 I, const FVector& Head, const FVector& CoreEnd, const FVector& BodyEnd, const FVector& TailEnd)
		{
			Segment(*Core, I, Head, CoreEnd, CoreWidth);
			Segment(*Body, I, Head, BodyEnd, BodyWidth);
			Segment(*Halo, I, Head, BodyEnd, HaloWidth);
			Segment(*Tail, I, BodyEnd, TailEnd, TailWidth);
		}

		void EndRound(const int32 I)
		{
			Live[I] = false;
			Slots.Remove(Flights[I].Round);
			for (FAcEffectPool* Pool : {Core, Body, Halo, Tail}) Pool->Hide(I);
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
			Place(I, Muzzle, Muzzle, Muzzle, Muzzle);

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
			// Thrown back off the surface: the round's way mirrored by the ground.
			const FVector Off = Along.IsNearlyZero() ? FVector::UpVector : Along.MirrorByVector(FVector::UpVector);
			using EWhat = ac::GameEvent::Impact::What;
			switch (I.what)
			{
			case EWhat::ground:
			{
				// An orange flash at its root, hot sparks that arc out and
				// fall, a scorch, and a dust plume for one strike in five (a
				// stream's sunlit puffs pile up into one white cloud: a
				// plume each, or bigger ones, hid the impacts).
				if (++GroundStrikes % 5 == 0) E->DustPuff(At, 0.8 * K, Time);
				// (Four or five overlap at 30 rounds a second: each faint, or
				// they add up to white.)
				Glow(At, 0.3 * K, 0.1, Time);
				// (Few and faint each: a stream's sparks overlap, and a hundred
				// bright ones bloom into one white ball.)
				SprayOfSparks(At, Off, int32(3 * K) + 1, 6.5 * K, Time);
				FAcScorchRequest S;
				S.At = FVector(At.X, At.Y, E->GroundZ(ac::Vec2(I.at.x, I.at.y)) + 1);
				S.Radius = 0.3 * Cm * K;
				S.Life = 6;
				S.Time = Time;
				S.Darkness = 0.75;
				E->Scorch(S);
				break;
			}
			case EWhat::structure:
			{
				const std::optional<ac::Structure> St = State.structure(I.victim);
				const bool bDummy = St && St->kind == ac::StructureKind::dummy;
				E->Flash(At, bDummy ? 1.5 : 1.9 * K, Time, 0.07);
				Glow(At, 0.22 * K, 0.1, Time);
				SprayOfSparks(At, Off, bDummy ? 3 : 4, 6.0 * K, Time);
				break;
			}
			case EWhat::unit:
			{
				const std::optional<ac::Unit> U = State.unit(I.victim);
				if (U && U->bug)
				{
					// Chitin and ichor: a hit flash and a burst of green flecks.
					E->Flash(At, 1.6, Time, 0.07);
					for (int32 B = 0; B < 12; ++B)
					{
						const double A = (B + FMath::FRand() * 0.6) * 2 * UE_DOUBLE_PI / 12;
						const FVector Out = FVector(std::cos(A), std::sin(A), 0.9 + FMath::FRand()) * (2.0 + 1.6 * FMath::FRand()) * Cm
							+ (Along.IsNearlyZero() ? FVector::ZeroVector : Along * 1.6 * Cm);
						Throw(*Flecks, FleckBits, At, Out, Time, 0.4, 1.1 + 0.7 * FMath::FRand());
					}
				}
				else
				{
					E->Flash(At, 1.6 * K, Time, 0.06);
					Glow(At, 0.2 * K, 0.1, Time);
					SprayOfSparks(At, Off, 3, 6.0 * K, Time);
				}
				break;
			}
			}
		}

		/// An orange flash at `At`, swelling to `Radius` cells and gone in `Life`.
		void Glow(const FVector& At, const double Radius, const double Life, const double Time)
		{
			Throw(*Glows, GlowBits, At, FVector::ZeroVector, Time, Life, Radius);
		}

		/// `N` hot sparks thrown round `Off` (a unit vector) at about `Speed`
		/// cells a second; they arc out under gravity and fall.
		void SprayOfSparks(const FVector& At, const FVector& Off, const int32 N, const double Speed, const double Time)
		{
			for (int32 B = 0; B < N; ++B)
			{
				const FVector Spread = FVector(FMath::FRandRange(-1.0, 1.0), FMath::FRandRange(-1.0, 1.0), FMath::FRandRange(0.2, 1.2));
				const FVector V = (Off * 0.8 + Spread * 0.7).GetSafeNormal() * Speed * FMath::FRandRange(0.6, 1.3) * Cm;
				Throw(*Sparks, SparkBits, At, V, Time, FMath::FRandRange(0.3, 0.6), FMath::FRandRange(0.7, 1.3));
			}
		}

		void Throw(FAcEffectPool& Pool, TArray<FBit>& Bits, const FVector& At, const FVector& Velocity, const double Time, const double Life,
			const double Size)
		{
			const int32 I = Pool.Next();
			Bits[I] = {At, Velocity, Time, Life, Size, true};
		}

		enum class EBit : uint8 { Casing, Fleck, Spark, Glow };

		/// Bits fly under gravity (cells/s² as the rounds'): casings tumble
		/// along their way, flecks shrink as they go, sparks streak along
		/// their motion and dim, an impact's glow swells and fades.
		void Fly(FAcEffectPool& Pool, TArray<FBit>& Bits, const double Time, const EBit Kind)
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
				const double Left = 1.0 - T / B.Life;
				switch (Kind)
				{
				case EBit::Casing:
				{
					const FQuat Spin(FVector(B.Velocity.Y, -B.Velocity.X, 0).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector), T * 25.0);
					Pool.Set(I, Pool.Shape(P, Spin, FVector(0.035 * Cm, 0.035 * Cm, 0.12 * Cm) * B.Size));
					break;
				}
				case EBit::Fleck:
					Pool.Set(I, Pool.Shape(P, FQuat::Identity, FVector(0.08 * Cm * B.Size * Left)));
					break;
				case EBit::Spark:
				{
					const FVector V = B.Velocity + FVector(0, 0, -ac::Rules::roundGravity * Cm * T);
					const double W = 0.022 * Cm * B.Size * (0.4 + 0.6 * Left);
					Pool.Set(I, Pool.Shape(P, FAcEffectPool::Along(V.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector)),
						FVector(W, W, 0.16 * Cm * B.Size * Left + 1.0)));
					break;
				}
				case EBit::Glow:
				{
					// Up to its size in the first third, then gone.
					const double Grow = FMath::Min(1.0, T / (B.Life / 3)) * (T < B.Life / 3 ? 1.0 : Left * 1.5);
					Pool.Set(I, Pool.Shape(B.At, FQuat::Identity, FVector(2 * B.Size * Cm * FMath::Max(Grow, 0.05))));
					break;
				}
				}
			}
		}

		UAcEffects* E = nullptr;
		FAcEffectPool* Core = nullptr;
		FAcEffectPool* Body = nullptr;
		FAcEffectPool* Halo = nullptr;
		FAcEffectPool* Tail = nullptr;
		FAcEffectPool* Glows = nullptr;
		FAcEffectPool* Sparks = nullptr;
		FAcEffectPool* Casings = nullptr;
		FAcEffectPool* Flecks = nullptr;
		TArray<FAcRoundFlight> Flights;
		TArray<bool> Live;
		TArray<FVector> Offsets;
		/// The round's id → its slot, while it flies.
		TMap<int64, int32> Slots;
		/// Rounds fired by each shooter (the smoke's and the casings' count),
		/// and rounds that struck the ground (the plumes' count).
		TMap<int64, int32> Fires;
		int32 GroundStrikes = 0;
		TArray<FBit> CasingBits, FleckBits, SparkBits, GlowBits;
	};

	FAcEffectsExtensionRegistration RegRounds(TEXT("rounds"), 40, [] { return TUniquePtr<FAcEffectsExtension>(new FAcRoundsExtension); });
}
