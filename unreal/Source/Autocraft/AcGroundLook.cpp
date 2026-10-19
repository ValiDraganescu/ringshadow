#include "AcGroundLook.h"

#include "AcMmo.h"
#include "AcSimSubsystem.h"

#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "NetProtocol.h"

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
			P.Layer = 4;
			break;
		case G::ash:
			P.Tint = FLinearColor(0.16f, 0.16f, 0.165f, 0.9f);
			P.Layer = 5;
			break;
		case G::glass:
			P.Tint = FLinearColor(0.045f, 0.05f, 0.06f, 0.92f);
			P.Fx = FLinearColor(0, 1, 0, 0);
			P.Layer = 6;
			break;
		case G::slag:
			P.Tint = FLinearColor(0.034f, 0.03f, 0.028f, 0.95f);
			P.Fx = FLinearColor(0, 0, 1, 0);
			P.Layer = 7;
			break;
		case G::frost:
			P.Tint = FLinearColor(0.15f, 0.17f, 0.2f, 0.8f);
			P.Layer = 8;
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

	ac::groundblend::Frame FAround::Frame(const ac::GroundRect& B, const ac::Vec2 Centre) const
	{
		std::array<std::string_view, 9> V;
		for (int32 K = 0; K < 9; K++) V[size_t(K)] = Ids[K];
		const ac::Vec2 At = Centre + Shift;
		return ac::groundblend::frame(float(B.minX + At.x), float(B.minZ + At.y), float(B.maxX + At.x), float(B.maxZ + At.y), V);
	}

	/// The grid steps to the eight round a region: N, E, S, W, NE, SE, SW, NW
	/// (the grid's rows run north to south).
	static const int64 Steps[8][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}, {1, -1}, {1, 1}, {-1, 1}, {-1, -1}};

	TOptional<FAround> AroundOf(const UAcSimSubsystem* Sim, const int64 Region)
	{
		const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
		const ac::net::WorldMessage* World = Mmo ? Mmo->World() : nullptr;
		if (!World || Region < 0 || Region >= int64(World->regions.size())) return {};
		const ac::net::WorldMessage::Region& Here = World->regions[size_t(Region)];
		FAround Out;
		Out.Own = Here.ground;
		Out.Ids[0] = Here.id;
		for (const ac::net::WorldMessage::Region& R : World->regions)
			for (int32 E = 0; E < 8; E++)
			{
				if (R.column != Here.column + Steps[E][0] || R.row != Here.row + Steps[E][1]) continue;
				Out.Around[E] = R.ground;
				Out.Ids[E + 1] = R.id;
			}
		return Out;
	}

	namespace
	{
		TAutoConsoleVariable<int32> CVarBleed(TEXT("ac.GroundBleed"), 1,
			TEXT("Palettes bleed into each other where regions meet (1), or each region's stops at its edges (0: for measuring)."));

		struct FState
		{
			TWeakObjectPtr<UMaterialInstanceDynamic> Material;
			TMap<FName, FLinearColor> Vectors;
			float Frost = -1.0f;
			float Embers = -1.0f;
		};
		/// Each material's last parameters (only what changed is set).
		TMap<TObjectKey<UMaterialInstanceDynamic>, FState> GStates;
	}

	static void Set(UMaterialInstanceDynamic* Material, const FAround& Around, double Hour, double Dark);

	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, const double Hour, const double Dark)
	{
		const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
		const ac::net::WorldMessage* World = Mmo ? Mmo->World() : nullptr;
		Apply(Material, Sim, World ? World->here : INDEX_NONE, Hour, Dark);
	}

	void Apply(UMaterialInstanceDynamic* Material, const UAcSimSubsystem* Sim, const int64 Region, const double Hour, const double Dark)
	{
		if (!Material) return;
		const TOptional<FAround> Around = AroundOf(Sim, Region);
		Set(Material, Around ? *Around : FAround(), Hour, Dark);
	}

	void ApplyAround(UMaterialInstanceDynamic* Material, const FAround& Around, const double Hour, const double Dark)
	{
		if (Material) Set(Material, Around, Hour, Dark);
	}

	static void Set(UMaterialInstanceDynamic* Material, const FAround& Around, const double Hour, const double Dark)
	{
		// Frost's hour (the frost ground's own: the shader puts it wherever
		// that ground bleeds); the slag's embers by the dark.
		const float Frost = FrostAt(Hour);
		const float Embers = float(FMath::Lerp(0.6, 1.5, FMath::Clamp(Dark, 0.0, 1.0)));

		if (GStates.Num() > 64)
		{
			for (auto It = GStates.CreateIterator(); It; ++It)
				if (!It.Value().Material.IsValid()) It.RemoveCurrent();
		}
		FState& S = GStates.FindOrAdd(TObjectKey<UMaterialInstanceDynamic>(Material));
		const bool bNew = S.Material.Get() != Material;
		if (bNew) S.Vectors.Reset();
		const auto Vector = [&](const FName Name, const FLinearColor V) {
			if (const FLinearColor* Was = S.Vectors.Find(Name); Was && Was->Equals(V, 1e-4f)) return;
			Material->SetVectorParameterValue(Name, V);
			S.Vectors.Add(Name, V);
		};

		static const FName TintName(TEXT("GroundTint")), FxName(TEXT("GroundFx")), LayerName(TEXT("GroundLayer"));
		static const FName TintNames[8] = {TEXT("NbTintN"), TEXT("NbTintE"), TEXT("NbTintS"), TEXT("NbTintW"), TEXT("NbTintNE"), TEXT("NbTintSE"),
			TEXT("NbTintSW"), TEXT("NbTintNW")};
		static const FName FxNames[8] = {TEXT("NbFxN"), TEXT("NbFxE"), TEXT("NbFxS"), TEXT("NbFxW"), TEXT("NbFxNE"), TEXT("NbFxSE"), TEXT("NbFxSW"),
			TEXT("NbFxNW")};
		static const FName NbLayerName(TEXT("NbLayer")), NbLayerDName(TEXT("NbLayerD")), NeighboursName(TEXT("Neighbours"));
		static const FName EdgeName(TEXT("BleedEdge")), VName(TEXT("BleedV")), HName(TEXT("BleedH")), ShiftName(TEXT("BleedShift"));

		const FPalette Own = Of(Around.Own);
		Vector(TintName, Own.Tint);
		Vector(FxName, Own.Fx);
		if (const FLinearColor* Was = S.Vectors.Find(LayerName); !Was || Was->R != Own.Layer)
		{
			Material->SetScalarParameterValue(LayerName, Own.Layer);
			S.Vectors.Add(LayerName, FLinearColor(Own.Layer, 0, 0, 0));
		}
		float Layers[8];
		for (int32 E = 0; E < 8; E++)
		{
			const FPalette P = Around.Around[E] ? Of(*Around.Around[E]) : FPalette();
			Vector(TintNames[E], P.Tint);
			Vector(FxNames[E], P.Fx);
			Layers[E] = P.Layer;
		}
		Vector(NbLayerName, FLinearColor(Layers[0], Layers[1], Layers[2], Layers[3]));
		Vector(NbLayerDName, FLinearColor(Layers[4], Layers[5], Layers[6], Layers[7]));
		// Where they meet: the seeds (24 bits, exact in a float), and which
		// edges have a region beyond (the dark seam only at the world's rim).
		const ac::groundblend::Frame F = Around.Frame(ac::GroundRect{}, ac::Vec2(0, 0));
		const auto Seeds = [](const std::array<uint32_t, 4>& A) { return FLinearColor(float(A[0]), float(A[1]), float(A[2]), float(A[3])); };
		Vector(EdgeName, Seeds(F.edge));
		Vector(VName, Seeds(F.beyondV));
		Vector(HName, Seeds(F.beyondH));
		const float On = CVarBleed.GetValueOnGameThread() != 0 ? 1.f : 0.f;
		Vector(NeighboursName, FLinearColor(F.has[0] ? On : 0.f, F.has[1] ? On : 0.f, F.has[2] ? On : 0.f, F.has[3] ? On : 0.f));
		Vector(ShiftName, FLinearColor(float(Around.Shift.x), float(Around.Shift.y), 0, 0));
		if (bNew || FMath::Abs(S.Frost - Frost) > 1.0f / 256) Material->SetScalarParameterValue(TEXT("Frost"), Frost);
		if (bNew || FMath::Abs(S.Embers - Embers) > 1.0f / 256) Material->SetScalarParameterValue(TEXT("EmberGain"), Embers);
		S.Material = Material;
		S.Frost = Frost;
		S.Embers = Embers;
	}
}
