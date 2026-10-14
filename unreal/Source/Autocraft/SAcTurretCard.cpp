#include "SAcTurretCard.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

namespace
{
	// The panel, in the HUD root's points (as SAcCitadelScreen).
	constexpr float Pad = 24;
	constexpr float HeaderH = 104;
	constexpr float CardW = 212;
	constexpr float CardH = 268;
	constexpr float Gap = 14;
	constexpr float FootH = 56;
	constexpr float ButtonH = 40;

	enum ERow : int32
	{
		RowClose = 0,
		RowUpgrade = 1,
	};

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C)
	{
		if (C.A <= 0) return;
		const FVector2f S = B.GetSize();
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(S), FSlateLayoutTransform(FVector2D(B.Min))),
			FAcHudStyle::White(), ESlateDrawEffect::None, C);
	}

	void CutFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W,
		float Cut)
	{
		const TArray<FVector2f> P{
			FVector2f(B.Min.X + Cut, B.Min.Y), FVector2f(B.Max.X, B.Min.Y), FVector2f(B.Max.X, B.Max.Y - Cut),
			FVector2f(B.Max.X - Cut, B.Max.Y), FVector2f(B.Min.X, B.Max.Y), FVector2f(B.Min.X, B.Min.Y + Cut), FVector2f(B.Min.X + Cut, B.Min.Y)};
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
	}

	/// A mark: a chevron pointing up, as the ones on the turret's base.
	void Chevron(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f At, float W, const FLinearColor& C, float Thick)
	{
		const TArray<FVector2f> P{FVector2f(At.X - W / 2, At.Y + W * 0.32f), FVector2f(At.X, At.Y - W * 0.18f),
			FVector2f(At.X + W / 2, At.Y + W * 0.32f)};
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, Thick);
	}

	FAcHudText Label(const FString& S, float Size, const FLinearColor& C, EAcHAlign H = EAcHAlign::Left,
		EAcVAlign V = EAcVAlign::Top, EAcFontWeight Weight = EAcFontWeight::Bold, float Kern = 0)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		T.Kern = Kern;
		return T;
	}

	FString Grouped(int64 N) { return FText::AsNumber(N).ToString(); }

	FLinearColor Dim() { return FAcHudStyle::Srgb(0.42, 0.5, 0.57); }
	FLinearColor Faint() { return FAcHudStyle::Srgb(0.26, 0.32, 0.38); }
	FLinearColor Steel() { return FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8); }
	FLinearColor Refused() { return FAcHudStyle::Srgb(1.0, 0.55, 0.42); }
	FLinearColor Ready() { return FAcHudStyle::Srgb(0.45, 0.95, 0.62); }
	/// The amalgam's heat: the turrets' own colour (the Gatling Gun's furnace).
	FLinearColor Heat() { return FAcHudStyle::Srgb(1.0, 0.55, 0.25); }
	/// The pilot's wallet: its prices.
	FLinearColor Wallet() { return FAcHudStyle::Srgb(1.0, 0.82, 0.42); }

	/// A stat's colour (`ac::DefenceStat`'s order): the Citadel's DEFENCE tab's, and air reach's sky blue.
	FLinearColor StatColor(int32 Stat)
	{
		switch (Stat)
		{
		case 0: return FAcHudStyle::Amber();
		case 1: return FAcHudStyle::Srgb(1.0, 0.45, 0.3);
		case 2: return FAcHudStyle::Ice();
		default: return FAcHudStyle::Srgb(0.55, 0.78, 1.0);
		}
	}

	const TCHAR* StatBlurb(int32 Stat)
	{
		switch (Stat)
		{
		case 0: return TEXT("shots a second");
		case 1: return TEXT("a shot");
		case 2: return TEXT("cells from its edge");
		default: return TEXT("cells into the air");
		}
	}
}

void SAcTurretCard::Construct(const FArguments& InArgs)
{
	OnUpgradeDelegate = InArgs._OnUpgrade;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcTurretCard::Animate));
}

SAcTurretCard::~SAcTurretCard() = default;

double SAcTurretCard::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcTurretCard::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

void SAcTurretCard::SetInfo(const FAcTurretCardInfo& In)
{
	if (In == Info) return;
	Info = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcTurretCard::SetRefusal(const FString& Why)
{
	Refusal = Why;
	if (!Why.IsEmpty()) Kept.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcTurretCard::SetKept()
{
	Refusal.Reset();
	Kept = FString::Printf(TEXT("Upgraded: %s. Paid from your wallet; every pilot here is told."), *Upgraded);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcTurretCard::UpgradeWhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.Lines.IsValidIndex(Index)) return TEXT("Nothing there");
	if (!Info.WhyNot.IsEmpty()) return Info.WhyNot;
	const FAcDefenceLine& X = Info.Lines[Index];
	if (X.Mark >= X.Most) return FString::Printf(TEXT("%s is at its last mark"), *X.Name.Left(1).Append(X.Name.Mid(1).ToLower()));
	if (X.Ore > Info.Ore) return FString::Printf(TEXT("Your wallet is short: %s ore needed, you have %s"), *Grouped(X.Ore), *Grouped(Info.Ore));
	if (X.Hydrogen > Info.Hydrogen)
		return FString::Printf(TEXT("Your wallet is short: %s MH needed, you have %s"), *Grouped(X.Hydrogen), *Grouped(Info.Hydrogen));
	return FString();
}

void SAcTurretCard::Upgrade(const int32 Index)
{
	if (const FString Why = UpgradeWhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	const FAcDefenceLine& X = Info.Lines[Index];
	Refusal.Reset();
	Kept.Reset();
	Upgraded = FString::Printf(TEXT("%s, mark %lld"), *X.Name.ToLower(), (long long)(X.Mark + 1));
	OnUpgradeDelegate.ExecuteIfBound(Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcTurretCard::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcTurretCard::Key(const FKey& InKey)
{
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four};
	static const FKey Numpad[] = {EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour};
	for (int32 I = 0; I < FMath::Min(Info.Lines.Num(), 4); ++I)
	{
		if (InKey == Digits[I] || InKey == Numpad[I])
		{
			Upgrade(I);
			return true;
		}
	}
	return false;
}

// MARK: - Layout

void SAcTurretCard::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	Cards.Reset();
	const int32 N = FMath::Max(3, Info.Lines.Num());
	PanelW = Pad * 2 + N * CardW + (N - 1) * Gap;
	Targets.Add(FTarget{FBox2f(FVector2f(PanelW - Pad - 40, 22), FVector2f(PanelW - Pad, 54)), RowClose, 0});
	const float Top = HeaderH + 14;
	for (int32 I = 0; I < Info.Lines.Num(); ++I)
	{
		const float X = Pad + I * (CardW + Gap);
		const FBox2f C(FVector2f(X, Top), FVector2f(X + CardW, Top + CardH));
		Cards.Add(C);
		Targets.Add(FTarget{FBox2f(FVector2f(C.Min.X + 16, C.Max.Y - 16 - ButtonH), FVector2f(C.Max.X - 16, C.Max.Y - 16)), RowUpgrade, I});
	}
	FootBox = FBox2f(FVector2f(Pad, Top + CardH + 16), FVector2f(PanelW - Pad, Top + CardH + 16 + FootH));
	PanelSize = FVector2f(PanelW, FootBox.Max.Y + 16);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcTurretCard::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

FReply SAcTurretCard::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Pressed = TargetAt(Local);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcTurretCard::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcTurretCard::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const int32 Hit = TargetAt(Local);
	const int32 Was = Pressed;
	Pressed = INDEX_NONE;
	if (Hit != INDEX_NONE && Hit == Was && Targets.IsValidIndex(Hit))
	{
		const FTarget T = Targets[Hit];
		if (T.Row == RowClose) Close();
		else Upgrade(T.Index);
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

FReply SAcTurretCard::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	Hovered = TargetAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcTurretCard::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcTurretCard::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

void SAcTurretCard::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const int32 Target, const FString& Text,
	const bool bLit, const FLinearColor& Accent) const
{
	if (!Targets.IsValidIndex(Target)) return;
	const FBox2f B = Targets[Target].Box;
	const bool bHover = Target == Hovered;
	const bool bDown = Target == Pressed && bHover;
	FLinearColor Fill = FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1);
	FLinearColor Rim = Steel();
	FLinearColor Ink = Dim();
	if (bLit)
	{
		Fill = FAcHudStyle::Blend(FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1), 0.24, Accent);
		Rim = Accent;
		Ink = FAcHudStyle::Text();
	}
	if (bHover) Rim = bLit ? FAcHudStyle::Blend(Accent, 0.5, FLinearColor::White) : FAcHudStyle::Ice();
	if (bDown) Fill = FAcHudStyle::Blend(Fill, 0.25, Accent);
	Box(Out, L + 1, P, B, Fill);
	CutFrame(Out, L + 2, P, B, Rim, bLit || bHover ? 1.5f : 1.0f, 6);
	Label(Text, 15, Ink, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0.6f).Paint(Out, L + 3, P, B.GetCenter());
}

int32 SAcTurretCard::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	const float In = FMath::Clamp(float(Clock() - OpenedAt) / 0.18f, 0.f, 1.f);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.5f * In));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// Dark glass, a steel rim with cut corners, the amalgam's heat inside it.
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.02, 0.036, 0.062, 0.97));
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)), WithAlpha(Heat(), 0.035f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), WithAlpha(Heat(), 0.3f), 1, 14);
	const int32 L = Layer + 3;

	// The header: its name and role, who built it; its hp and kills on the right.
	Label(Info.Role, 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.0f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 22));
	FAcHudText Title = Label(Info.Name.IsEmpty() ? FString(TEXT("TURRET")) : Info.Name, 32, Heat(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::Bold, 0.5f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	Title.Paint(Out, L, P, FVector2f(Pad + 2, 40));
	const FString By = Info.Builder.IsEmpty() ? FString(TEXT("the realm's · every pilot's"))
											  : FString::Printf(TEXT("built by %s · every pilot's"), *Info.Builder);
	Label(By, 13, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f).Paint(Out, L, P, FVector2f(Pad + 4, 78));
	const float RX = PanelW - Pad - 56;
	const float Frac = float(FMath::Clamp(Info.Hp / FMath::Max(1.0, Info.MaxHp), 0.0, 1.0));
	const FBox2f Bar(FVector2f(RX - 200, 34), FVector2f(RX, 44));
	Box(Out, L, P, Bar, FAcHudStyle::Srgb(0.06, 0.1, 0.14, 1));
	Box(Out, L + 1, P, FBox2f(Bar.Min, FVector2f(Bar.Min.X + Bar.GetSize().X * Frac, Bar.Max.Y)),
		Frac > 0.5f ? Ready() : Frac > 0.25f ? FAcHudStyle::Amber() : Refused());
	Label(TEXT("HP"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Center, EAcFontWeight::SemiBold, 1.0f)
		.Paint(Out, L, P, FVector2f(Bar.Min.X - 8, Bar.GetCenter().Y));
	Label(FString::Printf(TEXT("%s / %s"), *Grouped(int64(FMath::CeilToDouble(Info.Hp))), *Grouped(int64(Info.MaxHp))), 13, FAcHudStyle::Text(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(RX, 50));
	Label(TEXT("KILLS"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.0f).Paint(Out, L, P, FVector2f(RX - 70, 74));
	Label(Grouped(Info.Kills), 20, FAcHudStyle::Text(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L, P, FVector2f(RX, 68));
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Row == RowClose) PaintButton(Out, L, P, I, TEXT("ESC"), false, Heat());
	}
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), WithAlpha(Heat(), 0.2f));

	// A card per stat: its marks as chevrons, now and next, the price.
	if (Info.Lines.IsEmpty())
	{
		Label(TEXT("Waiting for the server…"), 15, Dim(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L, P, FVector2f(PanelW / 2, HeaderH + 14 + CardH / 2));
	}
	for (int32 I = 0; I < Info.Lines.Num() && I < Cards.Num(); ++I)
	{
		const FBox2f B = Cards[I];
		const FAcDefenceLine& X = Info.Lines[I];
		const int32 Stat = Info.Stats.IsValidIndex(I) ? Info.Stats[I] : I;
		const FLinearColor C = StatColor(Stat);
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Box(Out, L + 1, P, FBox2f(B.Min + FVector2f(10, 10), FVector2f(B.Min.X + 14, B.Min.Y + 28)), C);
		Label(X.Name, 14, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 12));
		const FBox2f Chip(FVector2f(B.Max.X - 32, B.Min.Y + 10), FVector2f(B.Max.X - 12, B.Min.Y + 30));
		CutFrame(Out, L + 2, P, Chip, FAcHudStyle::Ice(), 1, 4);
		Label(FString::Printf(TEXT("%d"), I + 1), 12, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 3, P, Chip.GetCenter());
		for (int32 K = 0; K < X.Most; ++K)
		{
			const bool bLit = K < X.Mark;
			Chevron(Out, L + 2, P, FVector2f(B.Min.X + 34 + K * 28, B.Min.Y + 48), 20, bLit ? C : WithAlpha(Steel(), 0.4f), bLit ? 3.0f : 1.5f);
		}
		Label(FString::Printf(TEXT("MARK %lld OF %lld"), (long long)X.Mark, (long long)X.Most), 11, Dim(), EAcHAlign::Left, EAcVAlign::Center,
			EAcFontWeight::SemiBold, 1.0f).Paint(Out, L + 2, P, FVector2f(B.Min.X + 22 + X.Most * 28 + 4, B.Min.Y + 50));
		Label(TEXT("NOW"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 70));
		Label(X.Now, 28, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.4f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 84));
		Label(StatBlurb(Stat), 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, B.Min + FVector2f(22, 118));
		Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 16, B.Min.Y + 142), FVector2f(B.Max.X - 16, B.Min.Y + 143)), WithAlpha(Steel(), 0.5f));
		if (X.Mark < X.Most)
		{
			Label(FString::Printf(TEXT("MARK %lld"), (long long)(X.Mark + 1)), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f)
				.Paint(Out, L + 2, P, B.Min + FVector2f(22, 152));
			Label(X.Next, 20, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, B.Min + FVector2f(96, 147));
			const FString Price = X.Hydrogen > 0 ? FString::Printf(TEXT("%s ore · %s MH"), *Grouped(X.Ore), *Grouped(X.Hydrogen))
												 : FString::Printf(TEXT("%s ore"), *Grouped(X.Ore));
			Label(Price, 14, Wallet(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, B.Min + FVector2f(22, 178));
			Label(TEXT("from your wallet"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, B.Min + FVector2f(22, 197));
		}
		else
		{
			Label(TEXT("LAST MARK"), 14, Ready(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 162));
		}
		const int32 T = Targets.IndexOfByPredicate([&](const FTarget& G) { return G.Row == RowUpgrade && G.Index == I; });
		PaintButton(Out, L, P, T, X.Mark < X.Most ? TEXT("UPGRADE") : TEXT("DONE"), UpgradeWhyNot(I).IsEmpty(), C);
	}

	// The foot: what happened or why not, the keys.
	const FBox2f F = FootBox;
	Box(Out, L, P, FBox2f(FVector2f(Pad, F.Min.Y - 8), FVector2f(PanelW - Pad, F.Min.Y - 7)), WithAlpha(Heat(), 0.14f));
	FString State = FString::Printf(TEXT("Paid from your own wallet (%s ore · %s MH), no tax: any pilot may upgrade the realm's turrets."),
		*Grouped(Info.Ore), *Grouped(Info.Hydrogen));
	FLinearColor StateColor = FAcHudStyle::Soft();
	if (!Refusal.IsEmpty())
	{
		State = Refusal;
		StateColor = Refused();
	}
	else if (!Kept.IsEmpty())
	{
		State = Kept;
		StateColor = Ready();
	}
	else if (!Upgraded.IsEmpty())
	{
		State = FString::Printf(TEXT("Upgrading the %s…"), *Upgraded);
	}
	else if (!Info.WhyNot.IsEmpty())
	{
		State = Info.WhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	Label(State, 14, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 4));
	Label(TEXT("Felled, a turret loses its marks."), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 26));
	const float KX = PanelW - Pad - 4;
	Label(FString::Printf(TEXT("1-%d UPGRADE · CLICK UPGRADES"), FMath::Max(1, Info.Lines.Num())), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top,
		EAcFontWeight::SemiBold, 0.8f).Paint(Out, L, P, FVector2f(KX, F.Min.Y + 8));
	Label(TEXT("U OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 28));
	return L + 24;
}
