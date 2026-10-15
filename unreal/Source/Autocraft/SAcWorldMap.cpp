#include "SAcWorldMap.h"

#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcLog.h"
#include "AcMissions.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/SlateBrush.h"

#include "Simulation.h"
#include "WindowMaps.h"

namespace
{
	namespace atlas = ac::net::atlas;

	// The bars, in the widget's points.
	constexpr float Margin = 24;
	constexpr float HeaderH = 76;
	constexpr float FooterH = 66;
	constexpr float ButtonH = 34;
	/// How far from an item's icon the pointer still finds it.
	constexpr double Reach = 8;

	enum ERow : int32
	{
		RowFly = 10,
		RowClose = 11,
	};

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
	FVector2f F(const ac::Vec2 V) { return FVector2f(float(V.x), float(V.y)); }
	ac::Vec2 V2(const FVector2f P) { return ac::Vec2(P.X, P.Y); }

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C)
	{
		const FVector2f S = B.GetSize();
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(S), FSlateLayoutTransform(FVector2D(B.Min))),
			FAcHudStyle::White(), ESlateDrawEffect::None, C);
	}

	/// A rectangle's outline `W` points wide, inside it.
	void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W = 1)
	{
		const FVector2f S = B.GetSize();
		Box(Out, Layer, G, FBox2f(B.Min, B.Min + FVector2f(S.X, W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Min.X, B.Max.Y - W), B.Max), C);
		Box(Out, Layer, G, FBox2f(B.Min + FVector2f(0, W), FVector2f(B.Min.X + W, B.Max.Y - W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Max.X - W, B.Min.Y + W), FVector2f(B.Max.X, B.Max.Y - W)), C);
	}

	FAcHudText Label(const FString& S, float Size, const FLinearColor& C, EAcHAlign H = EAcHAlign::Left,
		EAcVAlign V = EAcVAlign::Center, EAcFontWeight Weight = EAcFontWeight::Bold)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		return T;
	}

	/// A label with a dark shadow under it: it reads over the ground.
	void Shadowed(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FAcHudText& T, const FVector2f At)
	{
		FAcHudText S = T;
		S.Color = FLinearColor(0, 0, 0, 0.85f * T.Color.A);
		S.Paint(Out, Layer, G, At + FVector2f(1, 1));
		S.Paint(Out, Layer, G, At + FVector2f(-1, 1));
		T.Paint(Out, Layer + 1, G, At);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	/// A closed circle's points, for MakeLines.
	TArray<FVector2f> Circle(const FVector2f C, const float R, const int32 Sides = 24)
	{
		TArray<FVector2f> P;
		for (int32 I = 0; I <= Sides; ++I)
		{
			const float A = UE_TWO_PI * I / Sides;
			P.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		return P;
	}

	void Lines(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const TArray<FVector2f>& P, const FLinearColor& C, float W)
	{
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
	}

	/// A line with a dark edge under it.
	void Outlined(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const TArray<FVector2f>& P, const FLinearColor& C, float W)
	{
		Lines(Out, Layer, G, P, FLinearColor(0, 0, 0, 0.8f * C.A), W + 2.5f);
		Lines(Out, Layer + 1, G, P, C, W);
	}

	/// A convex polygon filled (a fan from its first point).
	void Fill(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const TArray<FVector2f>& P, const FLinearColor& C)
	{
		if (P.Num() < 3 || !FSlateApplication::IsInitialized()) return;
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FAcHudStyle::White());
		const FSlateShaderResourceProxy* Proxy = Handle.GetResourceProxy();
		const FVector2f Uv = Proxy ? Proxy->StartUV + Proxy->SizeUV * 0.5f : FVector2f(0.5f, 0.5f);
		const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
		const FColor Col = C.ToFColor(true);
		TArray<FSlateVertex> V;
		for (const FVector2f& Q : P) V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Q, Uv, Col));
		TArray<SlateIndex> I;
		for (int32 K = 1; K + 1 < P.Num(); ++K)
		{
			I.Add(0);
			I.Add(SlateIndex(K));
			I.Add(SlateIndex(K + 1));
		}
		FSlateDrawElement::MakeCustomVerts(Out, Layer, Handle, V, I, nullptr, 0, 0);
	}

	TArray<FVector2f> Diamond(const FVector2f At, const float D)
	{
		return {At + FVector2f(0, -D), At + FVector2f(D, 0), At + FVector2f(0, D), At + FVector2f(-D, 0), At + FVector2f(0, -D)};
	}

	/// The ground's colour of a style, dark (a region seen but not baked).
	FLinearColor StyleTint(const ac::MapStyle S)
	{
		switch (S)
		{
		case ac::MapStyle::highlands: return FAcHudStyle::Srgb(0.09, 0.14, 0.11);
		case ac::MapStyle::openField: return FAcHudStyle::Srgb(0.13, 0.14, 0.09);
		case ac::MapStyle::ridge: return FAcHudStyle::Srgb(0.09, 0.11, 0.16);
		case ac::MapStyle::badlands: return FAcHudStyle::Srgb(0.16, 0.1, 0.08);
		}
		return FAcHudStyle::Srgb(0.1, 0.1, 0.1);
	}

	// The overlays' colours.
	FLinearColor MercOrange() { return FAcHudStyle::Srgb(1.0, 0.55, 0.12); }
	FLinearColor BroodRed() { return FAcHudStyle::Srgb(1.0, 0.32, 0.26); }
	FLinearColor OreViolet() { return FAcHudStyle::Srgb(0.78, 0.5, 1.0); }
	FLinearColor Kstr() { return FAcHudStyle::Srgb(0.45, 1, 0.55); }
}

void SAcWorldMap::Construct(const FArguments& InArgs)
{
	OnFlyDelegate = InArgs._OnFly;
	OnTargetDelegate = InArgs._OnTarget;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
}

const FSlateBrush* SAcWorldMap::Picture(const std::string& RegionId) const
{
	// Brushes live as long as the process (the textures stay rooted).
	static TMap<FString, TUniquePtr<FSlateBrush>> Cache;
	const FString Id = Text(RegionId);
	if (const TUniquePtr<FSlateBrush>* Found = Cache.Find(Id)) return Found->Get();
	const FString Asset = TEXT("T_AcMap_") + Id.Replace(TEXT("-"), TEXT("_"));
	UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/UI/Map/%s.%s"), *Asset, *Asset), nullptr, LOAD_NoWarn);
	TUniquePtr<FSlateBrush> B;
	if (Texture)
	{
		Texture->AddToRoot();
		B = MakeUnique<FSlateBrush>();
		B->DrawAs = ESlateBrushDrawType::Image;
		B->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		B->SetResourceObject(Texture);
	}
	else
	{
		UE_LOG(LogAutocraft, Warning, TEXT("world map: no picture of %s (bake it: uv run unreal/Tools/bake_map.py)"), *Id);
	}
	const FSlateBrush* Out = B.Get();
	Cache.Add(Id, MoveTemp(B));
	return Out;
}

void SAcWorldMap::SetInfo(const FAcWorldMapInfo& In)
{
	const bool bNewWorld = !Info.World || !In.World || Info.World->regions != In.World->regions || Info.Seen != In.Seen
		|| Info.World->here != In.World->here;
	Info = In;
	if (bNewWorld)
	{
		Places = Info.World ? ac::net::atlas::layout(*Info.World, Info.Seen) : std::vector<ac::net::atlas::Place>{};
		WorldBounds = ac::net::atlas::bounds(Places);
	}
	if (Pick.Region != INDEX_NONE && (!Info.World || Pick.Region >= (int32)Info.World->regions.size()
		|| Pick.Pad >= (int32)Info.World->regions[Pick.Region].pads.size()))
	{
		Pick = FPick();
	}
	// The keys start on the pilot's own pad: ringed from the first look.
	if (Pick.Region == INDEX_NONE && Info.World && Info.FlyingTo.IsEmpty()) Pick = HerePad();
	// The units move: paint every frame it is told.
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcWorldMap::SetRefusal(const FString& Why)
{
	if (Why == Refusal) return;
	Refusal = Why;
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SAcWorldMap::PickPad(const FString& RegionId, const FString& PadName)
{
	if (!Info.World) return false;
	for (int32 R = 0; R < (int32)Info.World->regions.size(); ++R)
	{
		const ac::net::WorldMessage::Region& G = Info.World->regions[R];
		if (Text(G.id) != RegionId) continue;
		for (int32 P = 0; P < (int32)G.pads.size(); ++P)
		{
			if (Text(G.pads[P].name) != PadName) continue;
			// The rule and its words (atlas::padPick): a refusal says why.
			const ac::net::atlas::PadPick Rule = ac::net::atlas::padPick(*Info.World, Info.Seen, R, P);
			if (!Rule.ok)
			{
				SetRefusal(Text(Rule.why));
				UE_LOG(LogAutocraft, Log, TEXT("world map: no pick: %s"), *Text(Rule.why));
				return false;
			}
			Pick = FPick{R, P};
			Refusal.Reset();
			Invalidate(EInvalidateWidgetReason::Paint);
			UE_LOG(LogAutocraft, Log, TEXT("world map: pick %s %s"), *RegionId, *PadName);
			return true;
		}
	}
	UE_LOG(LogAutocraft, Log, TEXT("world map: no pick: no pad %s in %s"), *PadName, *RegionId);
	return false;
}

FString SAcWorldMap::PickedRegion() const
{
	return Info.World && Pick.Region != INDEX_NONE ? Text(Info.World->regions[Pick.Region].id) : FString();
}

FString SAcWorldMap::PickedPad() const
{
	return Info.World && Pick.Region != INDEX_NONE ? Text(Info.World->regions[Pick.Region].pads[Pick.Pad].name) : FString();
}

SAcWorldMap::FPick SAcWorldMap::HerePad() const
{
	if (!Info.World || Info.NearPad.IsEmpty()) return FPick();
	const int32 Here = (int32)Info.World->here;
	if (Here < 0 || Here >= (int32)Info.World->regions.size()) return FPick();
	const ac::net::WorldMessage::Region& G = Info.World->regions[Here];
	for (int32 P = 0; P < (int32)G.pads.size(); ++P)
	{
		if (G.pads[P].building && Text(G.pads[P].name) == Info.NearPad) return FPick{Here, P};
	}
	return FPick();
}

bool SAcWorldMap::Pickable(const int32 Region, const int32 Pad) const
{
	if (!Info.World || Region < 0 || Region >= (int32)Info.World->regions.size()) return false;
	const ac::net::WorldMessage::Region& G = Info.World->regions[Region];
	if (Pad < 0 || Pad >= (int32)G.pads.size() || !G.pads[Pad].building) return false;
	return ac::net::atlas::padPick(*Info.World, Info.Seen, Region, Pad).ok || (Region == (int32)Info.World->here && G.pads[Pad].building);
}

bool SAcWorldMap::PickIsHere() const
{
	if (!Info.World || Pick.Region == INDEX_NONE || !AtPad()) return false;
	const FPick Mine = HerePad();
	return Mine.Region == Pick.Region && Mine.Pad == Pick.Pad;
}

ac::Vec2 SAcWorldMap::PadPoint(const int32 Region, const int32 PadIndex) const
{
	const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, int64_t(Region));
	if (!P) return ac::Vec2(0, 0);
	return P->fromCells(Info.World->regions[Region].pads[PadIndex].at);
}

bool SAcWorldMap::MovePick(const int32 DX, const int32 DY)
{
	if (!Info.World || (DX == 0 && DY == 0)) return false;
	if (Pick.Region == INDEX_NONE)
	{
		Pick = HerePad();
		Refusal.Reset();
		Invalidate(EInvalidateWidgetReason::Paint);
		return Pick.Region != INDEX_NONE;
	}
	// The nearest Landing Pad that way: along the arrow counts once, across it
	// twice; a pad more across than along only when none is nearer the line.
	const ac::Vec2 From = PadPoint(Pick.Region, Pick.Pad);
	// (`Pick` may be on no Landing Pad: none here; then from the pilot.)
	const ac::Vec2 Dir = ac::normalize(ac::Vec2(DX, DY));
	FPick Best, Wide;
	double BestScore = TNumericLimits<double>::Max();
	double WideScore = TNumericLimits<double>::Max();
	for (int32 R = 0; R < (int32)Info.World->regions.size(); ++R)
	{
		for (int32 P = 0; P < (int32)Info.World->regions[R].pads.size(); ++P)
		{
			if ((R == Pick.Region && P == Pick.Pad) || !Pickable(R, P)) continue;
			const ac::Vec2 D = PadPoint(R, P) - From;
			const double Along = ac::dot(D, Dir);
			const double Across = FMath::Abs(D.x * Dir.y - D.y * Dir.x);
			if (Along <= 1) continue;
			const double Score = Along + 2 * Across;
			if (Across <= Along && Score < BestScore)
			{
				BestScore = Score;
				Best = FPick{R, P};
			}
			if (Score < WideScore)
			{
				WideScore = Score;
				Wide = FPick{R, P};
			}
		}
	}
	if (Best.Region == INDEX_NONE) Best = Wide;
	if (Best.Region == INDEX_NONE)
	{
		// The keys as a click: why nothing comes up.
		bool bElsewhere = false;
		for (int32 R = 0; R < (int32)Info.World->regions.size(); ++R)
			for (int32 P = 0; P < (int32)Info.World->regions[R].pads.size(); ++P)
				bElsewhere = bElsewhere || (R != (int32)Info.World->here && Pickable(R, P));
		const FString Why = bElsewhere ? FString(TEXT("No Landing Pad that way")) : Text(ac::net::atlas::onlyHere);
		SetRefusal(Why);
		UE_LOG(LogAutocraft, Log, TEXT("world map: no pick: %s"), *Why);
		return false;
	}
	Pick = Best;
	Refusal.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
	const ac::net::WorldMessage::Region& G = Info.World->regions[Pick.Region];
	UE_LOG(LogAutocraft, Log, TEXT("world map: pick %s %s"), *Text(G.id), *Text(G.pads[Pick.Pad].name));
	return true;
}

void SAcWorldMap::Fly()
{
	if (!Info.World || Pick.Region == INDEX_NONE)
	{
		SetRefusal(TEXT("Pick a Landing Pad to fly to first"));
		return;
	}
	if (PickIsHere())
	{
		SetRefusal(FString::Printf(TEXT("You are at %s: pick another Landing Pad to fly to"), *Info.NearPad));
		return;
	}
	const ac::net::WorldMessage::Region& G = Info.World->regions[Pick.Region];
	OnFlyDelegate.ExecuteIfBound(Text(G.id), Text(G.pads[Pick.Pad].name));
}

void SAcWorldMap::Close()
{
	OnCloseDelegate.ExecuteIfBound();
}

bool SAcWorldMap::AtPad() const
{
	return Info.NearDistance && *Info.NearDistance <= Info.PadReach;
}

FString SAcWorldMap::PriceText(const int32 Region) const
{
	if (!Info.World || Region < 0 || Region >= (int32)Info.World->regions.size()) return FString();
	const auto It = Info.FlightPrice.find(Text(Info.World->regions[Region].id));
	return It == Info.FlightPrice.end() ? FString() : FString::Printf(TEXT("%lld MH"), (long long)It->second);
}

bool SAcWorldMap::Short(const int32 Region) const
{
	if (!Info.World || !Info.Hydrogen || Region < 0 || Region >= (int32)Info.World->regions.size()) return false;
	const auto It = Info.FlightPrice.find(Text(Info.World->regions[Region].id));
	return It != Info.FlightPrice.end() && *Info.Hydrogen < It->second;
}

FString SAcWorldMap::StateLine(FLinearColor& Color) const
{
	Color = FAcHudStyle::Srgb(0.78, 0.84, 0.9);
	if (!Info.FlyingTo.IsEmpty())
	{
		Color = FAcHudStyle::Amber();
		return FString::Printf(TEXT("Aboard the Dropship to %s…"), *Info.FlyingTo);
	}
	if (!Refusal.IsEmpty())
	{
		Color = FAcHudStyle::Srgb(1.0, 0.45, 0.4);
		return Refusal;
	}
	if (!Info.World) return TEXT("Waiting for the world…");
	if (!Info.NearDistance)
	{
		return TEXT("No Landing Pad in this region: build one (B) to call the Dropship.");
	}
	if (!AtPad())
	{
		return FString::Printf(TEXT("Not at a Landing Pad: %s is %.0f cells away. Stand on it to call the Dropship."), *Info.NearPad,
			*Info.NearDistance);
	}
	const FString From = FString::Printf(TEXT("At %s%s"), *Info.NearPad, Info.bSpacedock ? TEXT(" (half the MH)") : TEXT(""));
	if (Pick.Region == INDEX_NONE || PickIsHere()) return FString::Printf(TEXT("%s: pick a Landing Pad to fly to."), *From);
	const ac::net::WorldMessage::Region& G = Info.World->regions[Pick.Region];
	const FString Price = PriceText(Pick.Region);
	if (Short(Pick.Region))
	{
		Color = FAcHudStyle::Srgb(1.0, 0.45, 0.4);
		return FString::Printf(TEXT("Not enough MH to fly to %s: %s, you have %lld."), *Text(G.name), *Price, (long long)*Info.Hydrogen);
	}
	if (Price.IsEmpty()) return FString::Printf(TEXT("%s: FLY the Dropship to %s in %s."), *From, *Text(G.pads[Pick.Pad].name), *Text(G.name));
	return FString::Printf(TEXT("%s: FLY the Dropship to %s in %s for %s."), *From, *Text(G.pads[Pick.Pad].name), *Text(G.name), *Price);
}

// MARK: - The view

double SAcWorldMap::Floor(const FVector2f ViewSize) const
{
	return ac::net::atlas::View::fit(WorldBounds, V2(ViewSize), 60).scale * 0.9;
}

void SAcWorldMap::EnsureView(const FVector2f ViewSize) const
{
	if (ViewSize.X <= 0 || ViewSize.Y <= 0 || Places.empty()) return;
	if (!bViewSet)
	{
		// Centred on the pilot, its region and a little round it in view.
		bViewSet = true;
		const ac::net::atlas::Place* Here = Info.World ? ac::net::atlas::placeOf(Places, Info.World->here) : nullptr;
		MapView.centre = Here ? Here->centre() : WorldBounds.centre();
		for (const ac::net::atlas::Item& It : Info.Items)
		{
			if (It.kind == ac::net::atlas::Kind::you) MapView.centre = It.at;
		}
		const double W = Here ? Here->width : 144, D = Here ? Here->depth : 96;
		MapView.scale = FMath::Clamp(FMath::Min(ViewSize.X / (W * 1.6), ViewSize.Y / (D * 1.6)), Floor(ViewSize), ac::net::atlas::View::maxScale);
	}
	if (bWorldToShow)
	{
		bWorldToShow = false;
		MapView = ac::net::atlas::View::fit(WorldBounds, V2(ViewSize - FVector2f(0, HeaderH + FooterH)), 30);
		MapView.centre.y -= (HeaderH - FooterH) / 2 / MapView.scale;
	}
	if (RegionToShow)
	{
		const ac::net::atlas::View Was = MapView;
		if (const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, std::string(TCHAR_TO_UTF8(**RegionToShow))))
		{
			const FVector2f Room = ViewSize - FVector2f(0, HeaderH + FooterH);
			MapView.centre = P->centre();
			MapView.scale = FMath::Clamp(FMath::Min(Room.X / P->width, Room.Y / P->depth) * RegionFill, Floor(ViewSize),
				ac::net::atlas::View::maxScale);
			MapView.centre.y -= (HeaderH - FooterH) / 2 / MapView.scale;
		}
		RegionToShow.Reset();
		if (GlideSeconds > 0)
		{
			GlideFrom = Was;
			GlideTo = MapView;
			GlideStart = FPlatformTime::Seconds();
		}
	}
	if (GlideSeconds > 0)
	{
		// Eased in and out; the scale on a log curve and the centre by the
		// inverse scale, so the point zoomed to drifts evenly to the middle.
		const double K = FMath::Clamp((FPlatformTime::Seconds() - GlideStart) / GlideSeconds, 0.0, 1.0);
		const double E = K * K * (3 - 2 * K);
		const double A = GlideFrom.scale, B = GlideTo.scale;
		MapView.scale = FMath::Exp(FMath::Lerp(FMath::Loge(A), FMath::Loge(B), E));
		const double Along = FMath::IsNearlyEqual(A, B) ? E : (1 / A - 1 / MapView.scale) / (1 / A - 1 / B);
		MapView.centre = GlideFrom.centre + (GlideTo.centre - GlideFrom.centre) * Along;
		if (K >= 1) GlideSeconds = 0;
	}
}

bool SAcWorldMap::GlideToRegion(const FString& RegionId, const double Fill, const double Seconds)
{
	if (!ShowRegion(RegionId, Fill)) return false;
	GlideSeconds = FMath::Max(0.0, Seconds);
	// Painted every frame while it glides.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) {
		Invalidate(EInvalidateWidgetReason::Paint);
		return GlideSeconds > 0 || RegionToShow ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
	}));
	return true;
}

void SAcWorldMap::SetView(const ac::Vec2 Centre, const double Scale)
{
	bViewSet = true;
	GlideSeconds = 0;
	MapView.centre = Centre;
	MapView.scale = FMath::Clamp(Scale, ac::net::atlas::View::minScale, ac::net::atlas::View::maxScale);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcWorldMap::ShowWorld()
{
	bViewSet = true;
	bWorldToShow = true;
	GlideSeconds = 0;
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SAcWorldMap::ShowRegion(const FString& RegionId, const double Fill)
{
	if (!ac::net::atlas::placeOf(Places, std::string(TCHAR_TO_UTF8(*RegionId)))) return false;
	bViewSet = true;
	GlideSeconds = 0;
	RegionToShow = RegionId;
	RegionFill = Fill;
	Invalidate(EInvalidateWidgetReason::Paint);
	return true;
}

bool SAcWorldMap::HoverWhere(TFunctionRef<bool(const ac::net::atlas::Item&)> Matches)
{
	for (const ac::net::atlas::Item& It : Info.Items)
	{
		if (!Matches(It)) continue;
		Scripted = It;
		Invalidate(EInvalidateWidgetReason::Paint);
		return true;
	}
	Scripted.Reset();
	return false;
}

int32 SAcWorldMap::ItemAt(const FVector2f Local) const
{
	if (Size.X <= 0 || Local.Y < HeaderH || Local.Y > Size.Y - FooterH) return INDEX_NONE;
	const std::optional<size_t> Hit = ac::net::atlas::pick(Info.Items, MapView, V2(Size), V2(Local), Reach);
	return Hit ? int32(*Hit) : INDEX_NONE;
}

int32 SAcWorldMap::HoveredItem() const
{
	if (Scripted)
	{
		// The same thing this frame: its kind and what names it.
		for (int32 I = 0; I < (int32)Info.Items.size(); ++I)
		{
			const ac::net::atlas::Item& It = Info.Items[size_t(I)];
			if (It.kind == Scripted->kind && It.ref == Scripted->ref && It.key == Scripted->key && It.region == Scripted->region) return I;
		}
		return INDEX_NONE;
	}
	if (!bPointerIn || bDragging || HoveredButton != INDEX_NONE) return INDEX_NONE;
	return ItemAt(Pointer);
}

const ac::net::atlas::Item* SAcWorldMap::Hovered() const
{
	const int32 I = HoveredItem();
	return I == INDEX_NONE ? nullptr : &Info.Items[size_t(I)];
}

FString SAcWorldMap::TooltipText() const
{
	const int32 I = HoveredItem();
	if (I == INDEX_NONE) return FString();
	const ac::net::atlas::Item& It = Info.Items[size_t(I)];
	FString S = Text(It.title);
	for (const std::string& L : It.lines) S += TEXT("\n") + Text(L);
	return S;
}

// MARK: - Layout

void SAcWorldMap::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	Size = ViewSize;
	const float Y = ViewSize.Y - FooterH + (FooterH - ButtonH) / 2;
	auto Button = [&](int32 RowId, float X, float W, const FString& S) {
		FTarget T;
		T.Box = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + ButtonH));
		T.Row = RowId;
		T.Label = S;
		Targets.Add(T);
	};
	Button(RowFly, ViewSize.X - Margin - 120, 120, TEXT("FLY"));
	Button(RowClose, ViewSize.X - Margin - 120 - 10 - 110, 110, TEXT("CLOSE"));
	EnsureView(ViewSize);
}

int32 SAcWorldMap::ButtonAt(const FVector2f Local) const
{
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(Local)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

FReply SAcWorldMap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PressedButton = ButtonAt(Local);
		bDown = PressedButton == INDEX_NONE;
		bDragging = false;
		DownAt = Local;
		Pointer = Local;
		if (bDown) return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
	}
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcWorldMap::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	FReply Reply = FReply::Handled();
	if (bDown) Reply.ReleaseMouseCapture();
	const bool bWasDrag = bDragging;
	const bool bWasDown = bDown;
	bDown = false;
	bDragging = false;
	const int32 Was = PressedButton;
	PressedButton = INDEX_NONE;
	const int32 Hit = ButtonAt(Local);
	if (Was != INDEX_NONE && Hit == Was && Targets.IsValidIndex(Hit))
	{
		if (Targets[Hit].Row == RowFly) Fly();
		else if (Targets[Hit].Row == RowClose)
		{
			Close();
			return Reply;
		}
	}
	else if (bWasDown && !bWasDrag)
	{
		// A click on the map: a pad picks it, a mission or a site steers the compass.
		Scripted.Reset();
		const int32 I = ItemAt(Local);
		if (I != INDEX_NONE)
		{
			const ac::net::atlas::Item& It = Info.Items[size_t(I)];
			if (It.kind == ac::net::atlas::Kind::pad && Info.World)
			{
				PickPad(Text(Info.World->regions[size_t(It.region)].id), Text(It.key));
			}
			else if (It.kind == ac::net::atlas::Kind::mission || It.kind == ac::net::atlas::Kind::site)
			{
				OnTargetDelegate.ExecuteIfBound(It);
			}
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return Reply;
}

FReply SAcWorldMap::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (bDown)
	{
		if (!bDragging && FVector2f::Distance(Local, DownAt) > 4) bDragging = true;
		if (bDragging)
		{
			MapView.pan(V2(Local - Pointer));
			MapView.clamp(WorldBounds);
			bViewSet = true;
			GlideSeconds = 0;
		}
	}
	if (FVector2f::Distance(Local, Pointer) > 0.5f) Scripted.Reset();
	Pointer = Local;
	bPointerIn = true;
	HoveredButton = ButtonAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SAcWorldMap::OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const float Delta = Event.GetWheelDelta();
	if (Delta != 0 && Size.X > 0)
	{
		GlideSeconds = 0;
		MapView.zoomAt(V2(Local), V2(Size), FMath::Pow(ac::net::atlas::View::notch, Delta), Floor(Size));
		MapView.clamp(WorldBounds);
		bViewSet = true;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Handled();
}

void SAcWorldMap::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	bPointerIn = false;
	HoveredButton = INDEX_NONE;
	PressedButton = INDEX_NONE;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FCursorReply SAcWorldMap::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	if (bDragging) return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	if (HoveredButton != INDEX_NONE) return FCursorReply::Cursor(EMouseCursor::Hand);
	const int32 I = HoveredItem();
	if (I != INDEX_NONE)
	{
		const ac::net::atlas::Kind K = Info.Items[size_t(I)].kind;
		if (K == ac::net::atlas::Kind::pad || K == ac::net::atlas::Kind::mission || K == ac::net::atlas::Kind::site) return FCursorReply::Cursor(EMouseCursor::Hand);
	}
	return FCursorReply::Cursor(EMouseCursor::GrabHand);
}

// MARK: - Paint

int32 SAcWorldMap::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	const FGeometry& G = Geometry;
	const ac::Vec2 Screen = V2(View);
	auto At = [&](const ac::Vec2 World) { return F(MapView.toScreen(World, Screen)); };
	const float Z = float(MapView.scale);
	// An icon's size: grows with the zoom, readable at any.
	const float U = FMath::Clamp(Z * 1.4f, 8.f, 16.f);

	Box(Out, Layer, G, FBox2f(FVector2f::ZeroVector, View), FAcHudStyle::Srgb(0.012, 0.02, 0.035, 0.97));
	const ac::net::WorldMessage* W = Info.World ? &*Info.World : nullptr;
	const int32 Here = W ? (int32)W->here : INDEX_NONE;
	const int32 To = W && W->flyingTo ? (int32)*W->flyingTo : INDEX_NONE;

	// The regions: their pictures where seen, dark with an outline where not.
	int32 L = Layer + 1;
	Pictures = 0;
	for (const ac::net::atlas::Place& P : Places)
	{
		const FBox2f R(At(P.corner()), At(P.corner() + ac::Vec2(P.width, P.depth)));
		const FSlateBrush* Brush = P.seen ? Picture(P.id) : nullptr;
		if (Brush)
		{
			FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(FVector2D(R.GetSize()), FSlateLayoutTransform(FVector2D(R.Min))), Brush,
				ESlateDrawEffect::None, FLinearColor::White);
			Pictures += 1;
		}
		else if (P.seen)
		{
			Box(Out, L, G, R, StyleTint(W->regions[size_t(P.index)].style));
		}
		else
		{
			Box(Out, L, G, R, FAcHudStyle::Srgb(0.03, 0.04, 0.055, 1));
		}
	}
	// The ringshadow band where it sweeps.
	for (const std::vector<ac::Vec2>& Band : Info.Bands)
	{
		TArray<FVector2f> Poly;
		for (const ac::Vec2& Q : Band) Poly.Add(At(Q));
		Fill(Out, L + 1, G, Poly, FAcHudStyle::Srgb(0.04, 0.07, 0.2, 0.42));
	}
	// The outlines and names.
	for (const ac::net::atlas::Place& P : Places)
	{
		const FBox2f R(At(P.corner()), At(P.corner() + ac::Vec2(P.width, P.depth)));
		const bool bHere = int32(P.index) == Here, bTo = int32(P.index) == To;
		const FLinearColor Rim = bTo ? FAcHudStyle::Amber() : bHere ? FAcHudStyle::Cyan()
			: P.seen ? FAcHudStyle::Srgb(0.55, 0.65, 0.72, 0.35) : FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.75);
		Frame(Out, L + 2, G, R, Rim, bHere || bTo ? 2.0f : 1.0f);
		const float NameSize = FMath::Clamp(R.GetSize().X / 14.f, 12.f, 20.f);
		Shadowed(Out, L + 3, G, Label(Text(P.name).ToUpper(), NameSize, P.seen ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.5, 0.6, 0.68),
			EAcHAlign::Left, EAcVAlign::Top), R.Min + FVector2f(10, 8));
		if (!P.seen)
		{
			Label(TEXT("UNCHARTED"), FMath::Max(10.f, NameSize - 6), FAcHudStyle::Srgb(0.4, 0.48, 0.55), EAcHAlign::Left, EAcVAlign::Top,
				EAcFontWeight::SemiBold).Paint(Out, L + 3, G, R.Min + FVector2f(10, 10 + NameSize + 4));
		}
		if (bHere || bTo)
		{
			Shadowed(Out, L + 3, G, Label(bTo ? TEXT("FLYING HERE") : TEXT("YOU ARE HERE"), 11, bTo ? FAcHudStyle::Amber() : FAcHudStyle::Cyan(),
				EAcHAlign::Left, EAcVAlign::Top), R.Min + FVector2f(10, 10 + NameSize + 4));
		}
	}

	// The items, last kinds first: the pilot on top.
	L += 5;
	const FString TargetKey = Info.Target;
	for (int32 I = (int32)Info.Items.size() - 1; I >= 0; --I)
	{
		const ac::net::atlas::Item& It = Info.Items[size_t(I)];
		const FVector2f C = At(It.at);
		if (C.X < -60 || C.Y < -60 || C.X > View.X + 60 || C.Y > View.Y + 60) continue;
		const float Ground = float(It.radius) * Z;
		switch (It.kind)
		{
		case ac::net::atlas::Kind::zone:
		{
			// A canyon zone (Z2): a ring round its prize, orange; red while a
			// wave is out or it has fallen; three chevrons inward.
			const FLinearColor Ink = It.flag ? BroodRed() : FAcHudStyle::Srgb(1.0, 0.6, 0.22);
			const float R = FMath::Max(Ground, U * 1.2f);
			Outlined(Out, L, G, Circle(C, R, 40), WithAlpha(Ink, 0.85f), It.flag ? 2.f : 1.5f);
			for (int32 K = 0; K < 3; ++K)
			{
				const float A = PI / 2 + K * 2 * PI / 3;
				const FVector2f D(FMath::Cos(A), FMath::Sin(A));
				const FVector2f N(-D.Y, D.X);
				const FVector2f Tip = C + D * (R - 7.f);
				Outlined(Out, L + 1, G, {Tip + D * 5.f + N * 4.f, Tip, Tip + D * 5.f - N * 4.f}, Ink, 1.5f);
			}
			break;
		}
		case ac::net::atlas::Kind::frontier:
		{
			// P1: a frontier pilot, a dot in its paint ringed dark, a white
			// ring round it; its callsign and level beside it zoomed in.
			const FLinearColor Ink = FAcHudStyle::TeamPaint(It.paint);
			const float R = U * 0.42f;
			Fill(Out, L, G, Circle(C, R + 1.5f, 16), FLinearColor(0, 0, 0, 0.85f));
			Fill(Out, L + 1, G, Circle(C, R, 16), Ink);
			Lines(Out, L + 2, G, Circle(C, R + 3.f, 16), FAcHudStyle::Srgb(0.92, 0.97, 1, 0.8), 1.f);
			if (Z >= 2)
			{
				Shadowed(Out, L + 3, G, Label(Text(It.title), 10, Ink, EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold),
					C + FVector2f(R + 6, 0));
			}
			break;
		}
		case ac::net::atlas::Kind::contract:
		{
			const float R = It.flag ? 8.f : 0.f;
			if (R > 0)
			{
				Outlined(Out, L, G, Circle(C, R), Kstr(), 2.f);
				Box(Out, L + 2, G, FBox2f(C - FVector2f(2.5f, 2.5f), C + FVector2f(2.5f, 2.5f)), Kstr());
			}
			break;
		}
		case ac::net::atlas::Kind::well:
		{
			const float R = FMath::Max(Ground, U * 0.32f);
			Outlined(Out, L, G, Circle(C, R, 16), FAcHudStyle::Srgb(0.88, 0.92, 0.96), 1.5f);
			Box(Out, L + 2, G, FBox2f(C - FVector2f(1.5f, 1.5f), C + FVector2f(1.5f, 1.5f)), FAcHudStyle::Srgb(0.88, 0.92, 0.96));
			break;
		}
		case ac::net::atlas::Kind::deposit:
		{
			// Sized by the ore left; hollow once mined out.
			const float D = U * (0.22f + 0.33f * FMath::Sqrt(float(It.size)));
			if (It.size > 0) Fill(Out, L, G, Diamond(C, D + 1.2f), FLinearColor(0, 0, 0, 0.75f));
			if (It.size > 0) Fill(Out, L + 1, G, Diamond(C, D), OreViolet());
			else Outlined(Out, L, G, Diamond(C, U * 0.25f), FAcHudStyle::Srgb(0.6, 0.55, 0.65, 0.8), 1.f);
			if (It.flag) Outlined(Out, L + 2, G, Circle(C, D + 4, 16), BroodRed(), 1.5f);
			break;
		}
		case ac::net::atlas::Kind::crater:
		{
			const float R = FMath::Max(Ground, U * 0.5f);
			Outlined(Out, L, G, Circle(C, R, 20), FAcHudStyle::Amber(), 1.5f);
			Fill(Out, L + 2, G, Diamond(C, U * 0.25f), FAcHudStyle::Amber());
			break;
		}
		case ac::net::atlas::Kind::starfall:
		{
			const float R = U * 0.7f;
			Outlined(Out, L, G, Circle(C, R, 20), FAcHudStyle::Amber(), 2.f);
			Outlined(Out, L, G, {C - FVector2f(R, R) * 0.7f, C + FVector2f(R, R) * 0.7f}, FAcHudStyle::Amber(), 1.5f);
			Outlined(Out, L, G, {C + FVector2f(-R, R) * 0.7f, C + FVector2f(R, -R) * 0.7f}, FAcHudStyle::Amber(), 1.5f);
			break;
		}
		case ac::net::atlas::Kind::building:
		{
			// The realm's cyan; the pilot's own brighter; a site still building dimmer.
			const float H = FMath::Max(Ground * 0.8f, U * 0.42f);
			const FBox2f B(C - FVector2f(H, H), C + FVector2f(H, H));
			const bool bBuilt = It.size >= 1;
			Box(Out, L, G, B.ExpandBy(1.5f), FLinearColor(0, 0, 0, 0.8f));
			Box(Out, L + 1, G, B, FAcHudStyle::Srgb(0.05, 0.22, 0.32, bBuilt ? 0.92 : 0.6));
			Frame(Out, L + 2, G, B, It.own ? FAcHudStyle::Ice() : FAcHudStyle::Cyan(), It.own ? 2.f : 1.5f);
			if (It.building && H >= 7)
			{
				const FString Initial = Text(ac::Simulation::title(*It.building)).Left(1);
				Label(Initial, FMath::Min(H * 1.3f, 16.f), FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center).Paint(Out, L + 3, G, C + FVector2f(0, 0.5f));
			}
			break;
		}
		case ac::net::atlas::Kind::bugs:
		{
			const float R = FMath::Max(Ground + U * 0.35f, U * 0.55f);
			Fill(Out, L, G, Circle(C, R, 20), WithAlpha(BroodRed(), 0.22f));
			Outlined(Out, L + 1, G, Circle(C, R, 20), BroodRed(), 1.5f);
			Shadowed(Out, L + 3, G, Label(FString::Printf(TEXT("%d"), int32(It.size)), 12, BroodRed(), EAcHAlign::Center, EAcVAlign::Center), C);
			break;
		}
		case ac::net::atlas::Kind::pilot:
		case ac::net::atlas::Kind::merc:
		{
			const bool bMerc = It.kind == ac::net::atlas::Kind::merc;
			const float R = U * 0.38f;
			Fill(Out, L, G, Circle(C, R + 1.5f, 16), FLinearColor(0, 0, 0, 0.85f));
			Fill(Out, L + 1, G, Circle(C, R, 16), bMerc ? MercOrange() : FAcHudStyle::Srgb(0.86, 0.9, 1));
			if (bMerc && It.own) Lines(Out, L + 2, G, Circle(C, R + 2.5f, 16), FAcHudStyle::Ice(), 1.f);
			if (bMerc && Z >= 3)
			{
				Shadowed(Out, L + 3, G, Label(Text(It.title), 10, MercOrange(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold),
					C + FVector2f(R + 5, 0));
			}
			break;
		}
		case ac::net::atlas::Kind::pad:
		{
			const ac::net::WorldMessage::Region* G2 = W ? &W->regions[size_t(It.region)] : nullptr;
			int32 PadIndex = INDEX_NONE;
			for (int32 Q = 0; G2 && Q < (int32)G2->pads.size(); ++Q) if (Text(G2->pads[Q].name) == Text(It.key)) PadIndex = Q;
			const bool bPicked = int32(It.region) == Pick.Region && PadIndex == Pick.Pad;
			// F2: the realm's Landing Pads the Dropship flies to, cyan squares;
			// one in a region not been to yet dim; a region file's pad (where
			// new pilots come down) a small dim circle, not flown to.
			const bool bFlyTo = Pickable(int32(It.region), PadIndex);
			const float R = It.own ? FMath::Max(Ground, U * 0.5f) : FMath::Max(Ground * 0.7f, U * 0.3f);
			const FLinearColor Ring = bPicked ? FAcHudStyle::Ice()
				: It.own ? (bFlyTo ? FAcHudStyle::Cyan() : FAcHudStyle::Srgb(0.35, 0.55, 0.62, 0.7))
				: FAcHudStyle::Srgb(0.75, 0.7, 0.5, 0.55);
			if (It.own) Outlined(Out, L, G, {C + FVector2f(-R, -R), C + FVector2f(R, -R), C + FVector2f(R, R), C + FVector2f(-R, R), C + FVector2f(-R, -R)}, Ring, 2.f);
			else Outlined(Out, L, G, Circle(C, R, 20), Ring, 2.f);
			Box(Out, L + 2, G, FBox2f(C - FVector2f(2, 2), C + FVector2f(2, 2)), Ring);
			if (bPicked) Outlined(Out, L + 2, G, Circle(C, R + 5, 24), FAcHudStyle::Cyan(), 1.5f);
			// The name and the flight's MH from the world's zoom to a region's
			// (closer in, the tooltip names it: the labels would pile on the
			// buildings by it); always the picked one; none in a region not
			// been to.
			const ac::net::atlas::Place* In = ac::net::atlas::placeOf(Places, It.region);
			if ((bFlyTo && Z >= 2.5f && Z < 6.f && In && In->seen) || bPicked)
			{
				const FAcHudText Name = Label(Text(It.title), 12, bPicked ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.98, 0.92, 0.7), EAcHAlign::Left, EAcVAlign::Center);
				Shadowed(Out, L + 3, G, Name, C + FVector2f(R + 6, 0));
				if (const FString Price = PriceText(int32(It.region)); !Price.IsEmpty())
				{
					Shadowed(Out, L + 3, G, Label(Price, 10, Short(int32(It.region)) ? FAcHudStyle::Srgb(1.0, 0.45, 0.4) : FAcHudStyle::Srgb(0.7, 0.78, 0.86),
						EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold), C + FVector2f(R + 10 + Name.Measure().X, 1));
				}
			}
			break;
		}
		case ac::net::atlas::Kind::site:
		{
			const float D = U * 0.5f;
			Outlined(Out, L, G, Diamond(C, D), It.flag ? FAcHudStyle::Srgb(0.7, 0.62, 0.4, 0.85) : FAcHudStyle::LevelGold(), It.flag ? 1.5f : 2.f);
			if (!It.flag) Box(Out, L + 2, G, FBox2f(C - FVector2f(2, 2), C + FVector2f(2, 2)), FAcHudStyle::LevelGold());
			if (TargetKey == Text(It.key)) Outlined(Out, L + 2, G, Circle(C, D + 6, 24), FAcHudStyle::Ice(), 1.5f);
			break;
		}
		case ac::net::atlas::Kind::mission:
		{
			const FLinearColor Tier = AcMissions::TierColor(It.tier);
			const float R = FMath::Max(Ground, U * 0.55f);
			Fill(Out, L, G, Circle(C, R, 28), WithAlpha(Tier, 0.16f));
			Outlined(Out, L + 1, G, Circle(C, R, 28), Tier, 2.f);
			Fill(Out, L + 2, G, Diamond(C, 4), Tier);
			if (TargetKey == FString::Printf(TEXT("mission:%lld"), (long long)It.ref)) Outlined(Out, L + 2, G, Circle(C, R + 5, 28), FAcHudStyle::Ice(), 1.5f);
			if (Z >= 2)
			{
				Shadowed(Out, L + 3, G, Label(Text(It.title), 11, Tier, EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold), C + FVector2f(0, R + 4));
			}
			break;
		}
		case ac::net::atlas::Kind::you:
		{
			// An arrow the way the Prospector faces, in a ring.
			const FVector2f Dir(FMath::Cos(float(It.heading)), FMath::Sin(float(It.heading)));
			const FVector2f Side(-Dir.Y, Dir.X);
			const float S = U * 0.75f;
			const TArray<FVector2f> Arrow{C + Dir * S, C - Dir * S * 0.6f + Side * S * 0.6f, C - Dir * S * 0.25f, C - Dir * S * 0.6f - Side * S * 0.6f};
			Outlined(Out, L, G, Circle(C, S + 4, 24), FAcHudStyle::Cyan(), 1.5f);
			TArray<FVector2f> Edge = Arrow;
			Edge.Add(Arrow[0]);
			Lines(Out, L + 1, G, Edge, FLinearColor(0, 0, 0, 0.9f), 3.f);
			Fill(Out, L + 2, G, {Arrow[0], Arrow[1], Arrow[2]}, FAcHudStyle::Ice());
			Fill(Out, L + 2, G, {Arrow[0], Arrow[2], Arrow[3]}, FAcHudStyle::Ice());
			break;
		}
		}
	}
	L += 6;

	// The tooltip: by the pointer (or by the item when a script hovers it).
	if (const int32 Hov = HoveredItem(); Hov != INDEX_NONE)
	{
		const ac::net::atlas::Item& It = Info.Items[size_t(Hov)];
		const FVector2f Anchor = Scripted ? At(It.at) : Pointer;
		TArray<FAcHudText> Rows;
		FLinearColor TitleColor = FAcHudStyle::Text();
		switch (It.kind)
		{
		case ac::net::atlas::Kind::merc: TitleColor = MercOrange(); break;
		case ac::net::atlas::Kind::bugs: TitleColor = BroodRed(); break;
		case ac::net::atlas::Kind::deposit: TitleColor = OreViolet(); break;
		case ac::net::atlas::Kind::site: TitleColor = FAcHudStyle::LevelGold(); break;
		case ac::net::atlas::Kind::pad: TitleColor = FAcHudStyle::KeyYellow(); break;
		case ac::net::atlas::Kind::mission: TitleColor = AcMissions::TierColor(It.tier); break;
		case ac::net::atlas::Kind::building: TitleColor = FAcHudStyle::Cyan(); break;
		case ac::net::atlas::Kind::crater:
		case ac::net::atlas::Kind::starfall: TitleColor = FAcHudStyle::Amber(); break;
		case ac::net::atlas::Kind::contract: TitleColor = Kstr(); break;
		case ac::net::atlas::Kind::zone: TitleColor = It.flag ? BroodRed() : FAcHudStyle::Srgb(1.0, 0.6, 0.22); break;
		case ac::net::atlas::Kind::frontier: TitleColor = FAcHudStyle::TeamPaint(It.paint); break;
		default: break;
		}
		Rows.Add(Label(Text(It.title), 16, TitleColor, EAcHAlign::Left, EAcVAlign::Top));
		for (const std::string& Line : It.lines)
		{
			Rows.Add(Label(Text(Line), 13, FAcHudStyle::Srgb(0.82, 0.88, 0.94), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium));
		}
		if (It.kind == ac::net::atlas::Kind::pad) Rows.Add(Label(TEXT("Click to pick it, then FLY"), 11, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium));
		if (It.kind == ac::net::atlas::Kind::mission || It.kind == ac::net::atlas::Kind::site)
		{
			Rows.Add(Label(TEXT("Click to steer the compass to it"), 11, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium));
		}
		float Wd = 0, Ht = 0;
		for (const FAcHudText& T : Rows)
		{
			const FVector2f M = T.Measure();
			Wd = FMath::Max(Wd, M.X);
			Ht += M.Y + 3;
		}
		const FVector2f BoxSize(Wd + 24, Ht + 16);
		FVector2f Min = Anchor + FVector2f(18, 14);
		if (Min.X + BoxSize.X > View.X - 8) Min.X = Anchor.X - 18 - BoxSize.X;
		if (Min.Y + BoxSize.Y > View.Y - FooterH - 4) Min.Y = Anchor.Y - 14 - BoxSize.Y;
		Min.X = FMath::Max(Min.X, 8.f);
		Min.Y = FMath::Max(Min.Y, HeaderH + 4);
		const FBox2f Tip(Min, Min + BoxSize);
		Box(Out, L, G, Tip, FAcHudStyle::Srgb(0.02, 0.04, 0.07, 0.94));
		Frame(Out, L + 1, G, Tip, WithAlpha(TitleColor, 0.8f), 1.5f);
		float Y = Min.Y + 8;
		for (const FAcHudText& T : Rows)
		{
			T.Paint(Out, L + 2, G, FVector2f(Min.X + 12, Y));
			Y += T.Measure().Y + 3;
		}
	}
	L += 4;

	// The header: the title and the keys.
	const FBox2f Head(FVector2f::ZeroVector, FVector2f(View.X, HeaderH));
	Box(Out, L, G, Head, FAcHudStyle::Srgb(0.02, 0.035, 0.06, 0.88));
	Box(Out, L + 1, G, FBox2f(FVector2f(0, HeaderH - 1), FVector2f(View.X, HeaderH)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.35));
	Label(TEXT("WORLD MAP"), 26, FAcHudStyle::Ice(), EAcHAlign::Left, EAcVAlign::Top).Paint(Out, L + 2, G, FVector2f(Margin, 12));
	FString Where = TEXT("M or Esc closes the map.");
	if (W && Here >= 0 && Here < (int32)W->regions.size())
	{
		Where = FString::Printf(TEXT("You are in %s. Wheel zooms, drag pans, hover for details. Arrows pick a pad, Return flies, M or Esc closes."),
			*Text(W->regions[Here].name));
	}
	Label(Where, 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, G, FVector2f(Margin, 46));
	if (!W)
	{
		Label(TEXT("Waiting for the world…"), 15, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Medium)
			.Paint(Out, L + 2, G, View * 0.5f);
	}

	// The footer: the state, then the buttons.
	const FBox2f Foot(FVector2f(0, View.Y - FooterH), View);
	Box(Out, L, G, Foot, FAcHudStyle::Srgb(0.02, 0.035, 0.06, 0.88));
	Box(Out, L + 1, G, FBox2f(Foot.Min, FVector2f(View.X, Foot.Min.Y + 1)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.35));
	FLinearColor StateColor;
	const FString State = StateLine(StateColor);
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Medium).Paint(Out, L + 2, G, FVector2f(Margin, View.Y - FooterH / 2));
	const bool bCanFly = W && Info.FlyingTo.IsEmpty() && AtPad() && Pick.Region != INDEX_NONE && !PickIsHere() && !Short(Pick.Region);
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		const FTarget& T = Targets[I];
		const bool bHover = I == HoveredButton;
		const bool bPress = I == PressedButton && bHover;
		const bool bLit = T.Row == RowFly && bCanFly;
		FLinearColor Fill2 = FAcHudStyle::Srgb(0.05, 0.09, 0.14, 1);
		FLinearColor Rim = FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8);
		FLinearColor Ink = T.Row == RowFly && !bCanFly ? FAcHudStyle::Srgb(0.45, 0.5, 0.55) : FAcHudStyle::Srgb(0.75, 0.85, 0.92);
		if (bLit)
		{
			Fill2 = FAcHudStyle::Srgb(0.08, 0.3, 0.42, 1);
			Rim = FAcHudStyle::Cyan();
			Ink = FAcHudStyle::Text();
		}
		if (bHover) Rim = FAcHudStyle::Ice();
		if (bPress) Fill2 = FAcHudStyle::Srgb(0.12, 0.4, 0.55, 1);
		Box(Out, L + 2, G, T.Box, Fill2);
		Frame(Out, L + 3, G, T.Box, Rim, bLit || bHover ? 1.5f : 1.0f);
		Label(T.Label, 16, Ink, EAcHAlign::Center, EAcVAlign::Center).Paint(Out, L + 4, G, T.Box.GetCenter() + FVector2f(0, 0.5f));
	}
	return L + 6;
}
