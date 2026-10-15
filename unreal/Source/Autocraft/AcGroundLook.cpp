#include "AcGroundLook.h"

#include "AcMmo.h"
#include "AcSimSubsystem.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtrTemplates.h"

namespace AcGroundLook
{
	FPalette Of(const ac::region::Ground Ground)
	{
		using G = ac::region::Ground;
		FPalette P;
		switch (Ground)
		{
		case G::plain: break;
		case G::rust: P.Tint = FLinearColor(0.30f, 0.092f, 0.042f, 0.85f); break;
		case G::salt:
			P.Tint = FLinearColor(0.60f, 0.58f, 0.53f, 0.9f);
			P.Fx = FLinearColor(1, 0, 0, 0);
			break;
		case G::ash: P.Tint = FLinearColor(0.16f, 0.16f, 0.165f, 0.9f); break;
		case G::glass:
			P.Tint = FLinearColor(0.045f, 0.05f, 0.06f, 0.92f);
			P.Fx = FLinearColor(0, 1, 0, 0);
			break;
		case G::slag:
			P.Tint = FLinearColor(0.034f, 0.03f, 0.028f, 0.95f);
			P.Fx = FLinearColor(0, 0, 1, 0);
			break;
		case G::frost:
			P.Tint = FLinearColor(0.15f, 0.17f, 0.2f, 0.8f);
			P.Frost = 1.0f;
			break;
		case G::ochre:
			P.Tint = FLinearColor(0.36f, 0.22f, 0.09f, 0.85f);
			P.Fx = FLinearColor(0, 0, 0, 1);
			break;
		}
		return P;
	}

	float FrostAt(const double Hour)
	{
		const double H = FMath::Fmod(FMath::Fmod(Hour, 24.0) + 24.0, 24.0);
		if (H < 12.0)
		{
			// Thicker toward dawn, melting from 06:30 to 09:30.
			return float(FMath::Lerp(0.6, 1.0, FMath::SmoothStep(0.0, 6.0, H)) * (1.0 - FMath::SmoothStep(6.5, 9.5, H)));
		}
		return float(0.6 * FMath::SmoothStep(19.0, 24.0, H));
	}

	namespace
	{
		struct FState
		{
			TWeakObjectPtr<UMaterialInstanceDynamic> Material;
			FPalette Own;
			FPalette Near[4];
			FLinearColor Neighbours = FLinearColor(0, 0, 0, 0);
			float Frost = -1.0f;
			float Embers = -1.0f;
		};
		/// Each material's last parameters (only what changed is set).
		TMap<TObjectKey<UMaterialInstanceDynamic>, FState> GStates;

		bool Same(const FPalette& A, const FPalette& B)
		{
			return A.Tint.Equals(B.Tint, 1e-4f) && A.Fx.Equals(B.Fx, 1e-4f) && FMath::IsNearlyEqual(A.Frost, B.Frost, 1e-4f);
		}
	}

	static void Set(UMaterialInstanceDynamic* Material, const FPalette& Own, const FPalette (&Near)[4], const FLinearColor& Neighbours,
		double Hour, double Dark);

	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, const double Hour, const double Dark)
	{
		const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
		const ac::net::WorldMessage* World = Mmo ? Mmo->World() : nullptr;
		Apply(Material, Sim, World ? World->here : INDEX_NONE, Hour, Dark);
	}

	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, const int64 Region, const double Hour, const double Dark)
	{
		if (!Material) return;
		FPalette Own;
		FPalette Near[4];
		FLinearColor Neighbours(0, 0, 0, 0);
		const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
		const ac::net::WorldMessage* World = Mmo ? Mmo->World() : nullptr;
		const ac::net::WorldMessage::Region* Here =
			World && Region >= 0 && Region < int64(World->regions.size()) ? &World->regions[size_t(Region)] : nullptr;
		if (Here)
		{
			Own = Of(Here->ground);
			// North, east, south, west: the grid's rows run north to south.
			const int64 Step[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
			for (const ac::net::WorldMessage::Region& R : World->regions)
			{
				for (int32 E = 0; E < 4; E++)
				{
					if (R.column != Here->column + Step[E][0] || R.row != Here->row + Step[E][1]) continue;
					Near[E] = Of(R.ground);
					Near[E].Frost = 0.0f;
					Neighbours.Component(E) = 1.0f;
				}
			}
		}
		Set(Material, Own, Near, Neighbours, Hour, Dark);
	}

	void ApplyGround(UMaterialInstanceDynamic* Material, const ac::region::Ground Ground, const TOptional<ac::region::Ground> (&Around)[4],
		const double Hour, const double Dark)
	{
		if (!Material) return;
		FPalette Near[4];
		FLinearColor Neighbours(0, 0, 0, 0);
		for (int32 E = 0; E < 4; E++)
		{
			if (!Around[E]) continue;
			Near[E] = Of(*Around[E]);
			Near[E].Frost = 0.0f;
			Neighbours.Component(E) = 1.0f;
		}
		Set(Material, Of(Ground), Near, Neighbours, Hour, Dark);
	}

	static void Set(UMaterialInstanceDynamic* Material, const FPalette& Own, const FPalette (&Near)[4], const FLinearColor& Neighbours, const double Hour,
		const double Dark)
	{
		const float Frost = Own.Frost * FrostAt(Hour);
		const float Embers = float(FMath::Lerp(0.6, 1.5, FMath::Clamp(Dark, 0.0, 1.0)));

		if (GStates.Num() > 64)
		{
			for (auto It = GStates.CreateIterator(); It; ++It)
				if (!It.Value().Material.IsValid()) It.RemoveCurrent();
		}
		FState& S = GStates.FindOrAdd(TObjectKey<UMaterialInstanceDynamic>(Material));
		const bool bNew = S.Material.Get() != Material;
		if (bNew || !Same(S.Own, Own))
		{
			Material->SetVectorParameterValue(TEXT("GroundTint"), Own.Tint);
			Material->SetVectorParameterValue(TEXT("GroundFx"), Own.Fx);
		}
		static const TCHAR* const TintNames[4] = {TEXT("NbTintN"), TEXT("NbTintE"), TEXT("NbTintS"), TEXT("NbTintW")};
		static const TCHAR* const FxNames[4] = {TEXT("NbFxN"), TEXT("NbFxE"), TEXT("NbFxS"), TEXT("NbFxW")};
		for (int32 E = 0; E < 4; E++)
		{
			if (!bNew && Same(S.Near[E], Near[E])) continue;
			Material->SetVectorParameterValue(TintNames[E], Near[E].Tint);
			Material->SetVectorParameterValue(FxNames[E], Near[E].Fx);
		}
		if (bNew || !S.Neighbours.Equals(Neighbours)) Material->SetVectorParameterValue(TEXT("Neighbours"), Neighbours);
		if (bNew || FMath::Abs(S.Frost - Frost) > 1.0f / 256) Material->SetScalarParameterValue(TEXT("Frost"), Frost);
		if (bNew || FMath::Abs(S.Embers - Embers) > 1.0f / 256) Material->SetScalarParameterValue(TEXT("EmberGain"), Embers);
		S.Material = Material;
		S.Own = Own;
		for (int32 E = 0; E < 4; E++) S.Near[E] = Near[E];
		S.Neighbours = Neighbours;
		S.Frost = Frost;
		S.Embers = Embers;
	}
}
