#include "SAcWorldMap.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "Rendering/DrawElements.h"

#include "WindowMaps.h"

namespace
{
	// The panel, in the HUD root's points (as SAcNewGame).
	constexpr float PanelW = 780;
	constexpr float Pad = 28;
	constexpr float TileGap = 10;
	constexpr float ButtonH = 34;
	constexpr float PadHit = 22;

	enum ERow : int32
	{
		RowPad = 1,
		RowFly = 10,
		RowClose = 11,
	};

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

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

	/// The ground's colour of a style, dark (the tile's fill).
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
}

void SAcWorldMap::Construct(const FArguments& InArgs)
{
	OnFlyDelegate = InArgs._OnFly;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
}

void SAcWorldMap::SetInfo(const FAcWorldMapInfo& In)
{
	const bool bSame = In.World == Info.World && In.NearPad == Info.NearPad && In.FlyingTo == Info.FlyingTo
		&& (In.NearDistance.has_value() == Info.NearDistance.has_value())
		&& (!In.NearDistance || FMath::Abs(*In.NearDistance - *Info.NearDistance) < 0.5);
	Info = In;
	if (Pick.Region != INDEX_NONE && (!Info.World || Pick.Region >= (int32)Info.World->regions.size()
		|| Pick.Pad >= (int32)Info.World->regions[Pick.Region].pads.size()))
	{
		Pick = FPick();
	}
	// The keys start on the pilot's own pad: ringed from the first look.
	if (Pick.Region == INDEX_NONE && Info.World && Info.FlyingTo.IsEmpty())
	{
		Pick = HerePad();
		if (Pick.Region != INDEX_NONE) Invalidate(EInvalidateWidgetReason::Paint);
	}
	if (!bSame) Invalidate(EInvalidateWidgetReason::Paint);
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
			Pick = FPick{R, P};
			Refusal.Reset();
			Invalidate(EInvalidateWidgetReason::Paint);
			return true;
		}
	}
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
	if (!Info.World) return FPick();
	const int32 Here = (int32)Info.World->here;
	if (Here < 0 || Here >= (int32)Info.World->regions.size()) return FPick();
	const ac::net::WorldMessage::Region& G = Info.World->regions[Here];
	if (G.pads.empty()) return FPick();
	for (int32 P = 0; P < (int32)G.pads.size(); ++P)
	{
		if (Text(G.pads[P].name) == Info.NearPad) return FPick{Here, P};
	}
	return FPick{Here, 0};
}

bool SAcWorldMap::PickIsHere() const
{
	if (!Info.World || Pick.Region == INDEX_NONE || !AtPad()) return false;
	const FPick Mine = HerePad();
	return Mine.Region == Pick.Region && Mine.Pad == Pick.Pad && Text(Info.World->regions[Pick.Region].pads[Pick.Pad].name) == Info.NearPad;
}

FVector2f SAcWorldMap::GridPoint(const int32 Region, const int32 PadIndex) const
{
	const ac::net::WorldMessage::Region& G = Info.World->regions[Region];
	const ac::MapCells Cells = ac::cells(G.size);
	const ac::net::WorldMessage::Pad& Q = G.pads[PadIndex];
	// Tiles are 144 × 96 (wider than deep): x in those proportions.
	const float X = float(G.column) + float(Q.at.x / FMath::Max(1.0, double(Cells.width)));
	const float Y = float(G.row) + float(Q.at.y / FMath::Max(1.0, double(Cells.depth)));
	return FVector2f(X * 1.5f, Y);
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
	// The nearest pad that way: along the arrow counts once, across it
	// twice; a pad more across than along only when none is nearer the line.
	const FVector2f From = GridPoint(Pick.Region, Pick.Pad);
	const FVector2f Dir = FVector2f((float)DX, (float)DY).GetSafeNormal();
	FPick Best, Wide;
	float BestScore = TNumericLimits<float>::Max();
	float WideScore = TNumericLimits<float>::Max();
	for (int32 R = 0; R < (int32)Info.World->regions.size(); ++R)
	{
		for (int32 P = 0; P < (int32)Info.World->regions[R].pads.size(); ++P)
		{
			if (R == Pick.Region && P == Pick.Pad) continue;
			const FVector2f D = GridPoint(R, P) - From;
			const float Along = FVector2f::DotProduct(D, Dir);
			const float Across = FMath::Abs(D.X * Dir.Y - D.Y * Dir.X);
			if (Along <= 0.01f) continue;
			const float Score = Along + 2 * Across;
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
	if (Best.Region == INDEX_NONE) return false;
	Pick = Best;
	Refusal.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
	return true;
}

void SAcWorldMap::Fly()
{
	if (!Info.World || Pick.Region == INDEX_NONE)
	{
		SetRefusal(TEXT("Pick a pad to fly to first"));
		return;
	}
	if (PickIsHere())
	{
		SetRefusal(FString::Printf(TEXT("You are at pad %s: pick another pad to fly to"), *Info.NearPad));
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

FString SAcWorldMap::StateLine(FLinearColor& Color) const
{
	Color = FAcHudStyle::Srgb(0.78, 0.84, 0.9);
	if (!Info.FlyingTo.IsEmpty())
	{
		Color = FAcHudStyle::Amber();
		return FString::Printf(TEXT("Flying to %s…"), *Info.FlyingTo);
	}
	if (!Refusal.IsEmpty())
	{
		Color = FAcHudStyle::Srgb(1.0, 0.45, 0.4);
		return Refusal;
	}
	if (!Info.World) return TEXT("Waiting for the world…");
	if (!Info.NearDistance) return TEXT("Nothing driven: no flight.");
	if (!AtPad())
	{
		return FString::Printf(TEXT("Not at a pad: pad %s is %.0f cells away. Drive onto a pad to fly."), *Info.NearPad, *Info.NearDistance);
	}
	if (Pick.Region == INDEX_NONE || PickIsHere()) return FString::Printf(TEXT("At pad %s: pick a pad to fly to."), *Info.NearPad);
	const ac::net::WorldMessage::Region& G = Info.World->regions[Pick.Region];
	return FString::Printf(TEXT("At pad %s: FLY to pad %s in %s."), *Info.NearPad, *Text(G.pads[Pick.Pad].name), *Text(G.name));
}

// MARK: - Layout

void SAcWorldMap::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	Tiles.Reset();
	float Y = 92;
	const int32 Cols = Info.World ? FMath::Max<int32>(1, (int32)Info.World->columns) : 3;
	const int32 Rows = Info.World ? FMath::Max<int32>(1, (int32)Info.World->rows) : 3;
	// Tiles as wide as fit, the regions' shape (Medium: 144 × 96).
	const float TileW = (PanelW - 2 * Pad - (Cols - 1) * TileGap) / Cols;
	const float TileH = TileW * 96.0f / 144.0f;
	GridBox = FBox2f(FVector2f(Pad, Y), FVector2f(PanelW - Pad, Y + Rows * TileH + (Rows - 1) * TileGap));
	if (Info.World)
	{
		for (int32 R = 0; R < (int32)Info.World->regions.size(); ++R)
		{
			const ac::net::WorldMessage::Region& G = Info.World->regions[R];
			const FVector2f Min(Pad + G.column * (TileW + TileGap), Y + G.row * (TileH + TileGap));
			const FBox2f Tile(Min, Min + FVector2f(TileW, TileH));
			Tiles.Add(Tile);
			// The pads: where they stand on the region's ground, below the name.
			const ac::MapCells Cells = ac::cells(G.size);
			const FBox2f Ground(Tile.Min + FVector2f(8, 44), Tile.Max - FVector2f(8, 8));
			for (int32 P = 0; P < (int32)G.pads.size(); ++P)
			{
				const ac::net::WorldMessage::Pad& Q = G.pads[P];
				const FVector2f At = Ground.Min + FVector2f(float(Q.at.x / double(Cells.width)) * Ground.GetSize().X,
					float(Q.at.y / double(Cells.depth)) * Ground.GetSize().Y);
				const FString Name = Text(Q.name);
				const float W = Label(Name, 13, FLinearColor::White).Measure().X + 22;
				FTarget T;
				T.Box = FBox2f(At - FVector2f(PadHit / 2, PadHit / 2), At + FVector2f(W, PadHit / 2));
				T.Row = RowPad;
				T.Region = R;
				T.Pad = P;
				T.Label = Name;
				Targets.Add(T);
			}
		}
	}
	Y = GridBox.Max.Y + 16;
	// The state line, then the buttons.
	Y += 22 + 14;
	auto Button = [&](int32 RowId, float X, float W, const FString& S) {
		FTarget T;
		T.Box = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + ButtonH));
		T.Row = RowId;
		T.Label = S;
		Targets.Add(T);
	};
	Button(RowFly, PanelW - Pad - 120, 120, TEXT("FLY"));
	Button(RowClose, PanelW - Pad - 120 - 10 - 110, 110, TEXT("CLOSE"));
	Y += ButtonH + 24;
	PanelSize = FVector2f(PanelW, Y);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 40) / PanelSize.X, (ViewSize.Y - 40) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = (ViewSize - PanelSize * PanelScale) * 0.5f;
}

int32 SAcWorldMap::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

FReply SAcWorldMap::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Pressed = TargetAt(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
	}
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcWorldMap::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const int32 Hit = TargetAt(Local);
	const int32 Was = Pressed;
	Pressed = INDEX_NONE;
	if (Hit != INDEX_NONE && Hit == Was && Targets.IsValidIndex(Hit))
	{
		const FTarget T = Targets[Hit];
		if (T.Row == RowPad)
		{
			Pick = FPick{T.Region, T.Pad};
			Refusal.Reset();
		}
		else if (T.Row == RowFly)
		{
			Fly();
		}
		else if (T.Row == RowClose)
		{
			Close();
			return FReply::Handled();
		}
	}
	else if (Was == INDEX_NONE && Hit == INDEX_NONE)
	{
		// A click off the panel closes it.
		const FVector2f P = (Local - PanelOrigin) / PanelScale;
		if (!FBox2f(FVector2f::ZeroVector, PanelSize).IsInsideOrOn(P))
		{
			Close();
			return FReply::Handled();
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

FReply SAcWorldMap::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const int32 Hit = TargetAt(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
	if (Hit != Hovered)
	{
		Hovered = Hit;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Handled();
}

void SAcWorldMap::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcWorldMap::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcWorldMap::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.55f));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// The panel: dark glass, a steel rim and a cyan line inside it (Chrome).
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.025, 0.045, 0.075, 0.96));
	Frame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2);
	Frame(Out, Layer + 2, P, Panel.ExpandBy(-4), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.28), 1);
	const int32 L = Layer + 3;

	const ac::net::WorldMessage* W = Info.World ? &*Info.World : nullptr;
	const int32 Here = W ? (int32)W->here : INDEX_NONE;
	const int32 To = W && W->flyingTo ? (int32)*W->flyingTo : INDEX_NONE;
	Label(TEXT("WORLD MAP"), 26, FAcHudStyle::Ice(), EAcHAlign::Left, EAcVAlign::Top).Paint(Out, L, P, FVector2f(Pad, 24));
	FString Where = TEXT("M or Esc closes the map.");
	if (W && Here >= 0 && Here < (int32)W->regions.size())
	{
		Where = FString::Printf(TEXT("You are in %s. Arrows pick a pad, Return flies, M or Esc closes."), *Text(W->regions[Here].name));
	}
	Label(Where, 15, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L, P, FVector2f(Pad, 58));

	// The tiles.
	if (!W) Label(TEXT("Waiting for the world…"), 15, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Medium)
		.Paint(Out, L, P, GridBox.GetCenter());
	for (int32 R = 0; W && R < Tiles.Num(); ++R)
	{
		const ac::net::WorldMessage::Region& G = W->regions[R];
		const FBox2f& T = Tiles[R];
		const bool bHere = R == Here;
		const bool bTo = R == To;
		Box(Out, L, P, T, StyleTint(G.style));
		const FLinearColor Rim = bTo ? FAcHudStyle::Amber() : bHere ? FAcHudStyle::Cyan() : FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.7);
		Frame(Out, L + 1, P, T, Rim, bHere || bTo ? 2.0f : 1.0f);
		Label(Text(G.name), 15, bHere ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.8, 0.86, 0.92), EAcHAlign::Left, EAcVAlign::Top)
			.Paint(Out, L + 2, P, T.Min + FVector2f(9, 7));
		Label(FString::Printf(TEXT("%s · %s"), *Text(ac::title(G.style)), *Text(ac::title(G.size))), 12, FAcHudStyle::Srgb(0.5, 0.6, 0.68),
			EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, T.Min + FVector2f(9, 26));
		if (bHere || bTo)
		{
			Label(bTo ? TEXT("FLYING HERE") : TEXT("YOU ARE HERE"), 11, bTo ? FAcHudStyle::Amber() : FAcHudStyle::Cyan(), EAcHAlign::Right,
				EAcVAlign::Baseline).Paint(Out, L + 2, P, T.Max - FVector2f(8, 7));
		}
	}
	// The pads.
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		const FTarget& T = Targets[I];
		if (T.Row != RowPad) continue;
		const bool bPicked = T.Region == Pick.Region && T.Pad == Pick.Pad;
		const bool bHover = I == Hovered;
		const FVector2f At(T.Box.Min.X + PadHit / 2, T.Box.GetCenter().Y);
		if (bPicked || bHover)
		{
			Box(Out, L + 2, P, T.Box, bPicked ? FAcHudStyle::Srgb(0.08, 0.3, 0.42, 0.9) : FAcHudStyle::Srgb(0.05, 0.12, 0.18, 0.8));
			Frame(Out, L + 3, P, T.Box, bPicked ? FAcHudStyle::Cyan() : FAcHudStyle::Ice(), 1);
		}
		const FLinearColor Dot = bPicked ? FAcHudStyle::Ice() : FAcHudStyle::KeyYellow();
		Box(Out, L + 4, P, FBox2f(At - FVector2f(5, 5), At + FVector2f(5, 5)), FLinearColor(0, 0, 0, 0.85f));
		Box(Out, L + 5, P, FBox2f(At - FVector2f(4, 4), At + FVector2f(4, 4)), Dot);
		Label(T.Label, 13, bPicked ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.9, 0.88, 0.7), EAcHAlign::Left, EAcVAlign::Center)
			.Paint(Out, L + 5, P, FVector2f(At.X + 10, At.Y));
	}

	// The state.
	FLinearColor StateColor;
	const FString State = StateLine(StateColor);
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L, P, FVector2f(Pad, GridBox.Max.Y + 16));

	// The buttons: FLY lit when a flight can be asked for.
	const bool bCanFly = W && Info.FlyingTo.IsEmpty() && AtPad() && Pick.Region != INDEX_NONE && !PickIsHere();
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		const FTarget& T = Targets[I];
		if (T.Row < RowFly) continue;
		const bool bHover = I == Hovered;
		const bool bDown = I == Pressed && bHover;
		const bool bLit = T.Row == RowFly && bCanFly;
		FLinearColor Fill = FAcHudStyle::Srgb(0.05, 0.09, 0.14, 1);
		FLinearColor Rim = FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8);
		FLinearColor Ink = T.Row == RowFly && !bCanFly ? FAcHudStyle::Srgb(0.45, 0.5, 0.55) : FAcHudStyle::Srgb(0.75, 0.85, 0.92);
		if (bLit)
		{
			Fill = FAcHudStyle::Srgb(0.08, 0.3, 0.42, 1);
			Rim = FAcHudStyle::Cyan();
			Ink = FAcHudStyle::Text();
		}
		if (bHover) Rim = FAcHudStyle::Ice();
		if (bDown) Fill = FAcHudStyle::Srgb(0.12, 0.4, 0.55, 1);
		Box(Out, L, P, T.Box, Fill);
		Frame(Out, L + 1, P, T.Box, Rim, bLit || bHover ? 1.5f : 1.0f);
		Label(T.Label, 16, Ink, EAcHAlign::Center, EAcVAlign::Center).Paint(Out, L + 2, P, T.Box.GetCenter() + FVector2f(0, 0.5f));
	}
	return L + 8;
}
