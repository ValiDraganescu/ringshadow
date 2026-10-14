#include "SAcCitadelScreen.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

namespace
{
	// The panel, in the HUD root's points (as SAcRigScreen).
	constexpr float PanelW = 1040;
	constexpr float Pad = 26;
	constexpr float HeaderH = 92;
	constexpr float Gap = 16;
	constexpr float WalletW = 440;
	constexpr float BodyH = 300;
	constexpr float FootH = 56;
	constexpr float ButtonW = 120;
	constexpr float ButtonH = 50;

	enum ERow : int32
	{
		RowDonate = 1,
		RowTab = 2,
		RowClose = 3,
		RowBuyBlueprint = 4,
		RowBuyItem = 5,
	};

	// The shop's rows (C2).
	constexpr float BlueprintsW = 620;
	constexpr float BlueprintRowH = 30;
	constexpr float ItemRowH = 62;
	constexpr float BuyW = 70;
	constexpr float BuyH = 24;

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

	/// The plates' cut corners (Chrome): a frame with its top-left and
	/// bottom-right corners clipped by `Cut` points.
	void CutFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W,
		float Cut)
	{
		const TArray<FVector2f> P{
			FVector2f(B.Min.X + Cut, B.Min.Y), FVector2f(B.Max.X, B.Min.Y), FVector2f(B.Max.X, B.Max.Y - Cut),
			FVector2f(B.Max.X - Cut, B.Max.Y), FVector2f(B.Min.X, B.Max.Y), FVector2f(B.Min.X, B.Min.Y + Cut), FVector2f(B.Min.X + Cut, B.Min.Y)};
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
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
	/// The realm's gold: the Citadel's, the tax's.
	FLinearColor RealmGold() { return FAcHudStyle::Srgb(1.0, 0.82, 0.42); }

	/// "1,000 ore", "250 MH", "1,000 ore and 250 MH".
	FString Purse(int64 Ore, int64 Hydrogen)
	{
		if (Ore > 0 && Hydrogen > 0) return FString::Printf(TEXT("%s ore and %s MH"), *Grouped(Ore), *Grouped(Hydrogen));
		if (Hydrogen > 0) return FString::Printf(TEXT("%s MH"), *Grouped(Hydrogen));
		return FString::Printf(TEXT("%s ore"), *Grouped(Ore));
	}
}

void SAcCitadelScreen::Construct(const FArguments& InArgs)
{
	OnDonateDelegate = InArgs._OnDonate;
	OnBuyDelegate = InArgs._OnBuy;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	// The flashes fade: a repaint a frame while open.
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcCitadelScreen::Animate));
}

SAcCitadelScreen::~SAcCitadelScreen() = default;

double SAcCitadelScreen::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcCitadelScreen::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

void SAcCitadelScreen::SetInfo(const FAcCitadelScreenInfo& In)
{
	if (In == Info) return;
	if (Info.bKnown && In.RealmOre + In.RealmHydrogen > Info.RealmOre + Info.RealmHydrogen) GrewAt = Clock();
	Info = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetRefusal(const FString& Why)
{
	Refusal = Why;
	if (!Why.IsEmpty()) Kept.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetKept()
{
	Refusal.Reset();
	Kept = !Bought.IsEmpty()
		? FString::Printf(TEXT("Bought: %s. Paid from your wallet."), *Bought)
		: FString::Printf(TEXT("Given to the realm: %s. Every pilot's Citadel thanks you."), *Purse(Asked.Key, Asked.Value));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetTab(const ETab InTab)
{
	if (CurrentTab == InTab) return;
	CurrentTab = InTab;
	Refusal.Reset();
	Kept.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::BuyWhyNot(const bool bItem, const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	const TArray<FAcShopLine>& Lines = bItem ? Info.Items : Info.Blueprints;
	if (!Lines.IsValidIndex(Index)) return TEXT("Nothing there");
	if (!Info.ShopWhyNot.IsEmpty()) return Info.ShopWhyNot;
	const FAcShopLine& X = Lines[Index];
	if (X.Stack > 0 && X.Held >= X.Stack) return FString::Printf(TEXT("%s: you hold %lld, the most"), *X.Name, (long long)X.Stack);
	if (X.Ore > Info.Ore) return FString::Printf(TEXT("Not enough ore: %s, you have %s"), *Grouped(X.Ore), *Grouped(Info.Ore));
	if (X.Hydrogen > Info.Hydrogen) return FString::Printf(TEXT("Not enough MH: %s, you have %s"), *Grouped(X.Hydrogen), *Grouped(Info.Hydrogen));
	return FString();
}

void SAcCitadelScreen::Buy(const bool bItem, const int32 Index)
{
	if (const FString Why = BuyWhyNot(bItem, Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	const FAcShopLine& X = (bItem ? Info.Items : Info.Blueprints)[Index];
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought = bItem ? X.Name : X.Name + TEXT(" blueprint");
	OnBuyDelegate.ExecuteIfBound(bItem, Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

TPair<int64, int64> SAcCitadelScreen::Amount(const int32 Index) const
{
	const bool bOre = Index < 3;
	const int32 Step = Index % 3;
	const int64 N = Step == 0 ? 100 : Step == 1 ? 1000 : (bOre ? Info.Ore : Info.Hydrogen);
	return bOre ? TPair<int64, int64>(N, 0) : TPair<int64, int64>(0, N);
}

FString SAcCitadelScreen::WhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.DonateWhyNot.IsEmpty()) return Info.DonateWhyNot;
	const TPair<int64, int64> A = Amount(Index);
	if (A.Key + A.Value <= 0) return Index < 3 ? TEXT("No ore in your wallet to give") : TEXT("No MH in your wallet to give");
	if (A.Key > Info.Ore) return FString::Printf(TEXT("Not enough ore: you have %s"), *Grouped(Info.Ore));
	if (A.Value > Info.Hydrogen) return FString::Printf(TEXT("Not enough MH: you have %s"), *Grouped(Info.Hydrogen));
	return FString();
}

void SAcCitadelScreen::Donate(const int32 Index)
{
	if (Index < 0 || Index >= DonateButtons) return;
	if (const FString Why = WhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = Amount(Index);
	Bought.Reset();
	OnDonateDelegate.ExecuteIfBound(Asked.Key, Asked.Value);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcCitadelScreen::Key(const FKey& InKey)
{
	if (InKey == EKeys::Tab)
	{
		SetTab(CurrentTab == ETab::Realm ? ETab::Shop : ETab::Realm);
		return true;
	}
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight};
	static const FKey Numpad[] = {EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight};
	// REALM: 1-6 give; SHOP: 1-8 buy a blueprint.
	const int32 Count = CurrentTab == ETab::Realm ? DonateButtons : Info.Blueprints.Num();
	for (int32 I = 0; I < FMath::Min(Count, 8); ++I)
	{
		if (InKey == Digits[I] || InKey == Numpad[I])
		{
			if (CurrentTab == ETab::Realm) Donate(I);
			else Buy(false, I);
			return true;
		}
	}
	return false;
}

// MARK: - Layout

void SAcCitadelScreen::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	auto Add = [&](const FBox2f& B, int32 RowId, int32 Index) {
		FTarget T;
		T.Box = B;
		T.Row = RowId;
		T.Index = Index;
		Targets.Add(T);
	};
	// The header: the tabs (REALM, SHOP; the upgrades later), the close on the right.
	const float TabsX = 560;
	Add(FBox2f(FVector2f(TabsX, 30), FVector2f(TabsX + 110, 66)), RowTab, int32(ETab::Realm));
	Add(FBox2f(FVector2f(TabsX + 120, 30), FVector2f(TabsX + 230, 66)), RowTab, int32(ETab::Shop));
	Add(FBox2f(FVector2f(PanelW - Pad - 40, 26), FVector2f(PanelW - Pad, 58)), RowClose, 0);

	const float Top = HeaderH + 16;
	WalletBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + WalletW, Top + BodyH));
	DonateBox = FBox2f(FVector2f(Pad + WalletW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	BlueprintBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + BlueprintsW, Top + BodyH));
	ItemBox = FBox2f(FVector2f(Pad + BlueprintsW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	if (CurrentTab == ETab::Realm)
	{
		// The donate buttons: a row of ore, a row of MH.
		for (int32 I = 0; I < DonateButtons; ++I)
		{
			const float X = DonateBox.Min.X + 96 + (I % 3) * (ButtonW + 12);
			const float Y = DonateBox.Min.Y + 112 + (I / 3) * (ButtonH + 22);
			Add(FBox2f(FVector2f(X, Y), FVector2f(X + ButtonW, Y + ButtonH)), RowDonate, I);
		}
	}
	else
	{
		// A BUY on each blueprint's row and each consumable's.
		for (int32 I = 0; I < Info.Blueprints.Num(); ++I)
		{
			const float Y = BlueprintBox.Min.Y + 46 + I * BlueprintRowH + (BlueprintRowH - BuyH) / 2;
			Add(FBox2f(FVector2f(BlueprintBox.Max.X - 18 - BuyW, Y), FVector2f(BlueprintBox.Max.X - 18, Y + BuyH)), RowBuyBlueprint, I);
		}
		for (int32 I = 0; I < Info.Items.Num(); ++I)
		{
			const float Y = ItemBox.Min.Y + 44 + I * ItemRowH + (ItemRowH - BuyH) / 2;
			Add(FBox2f(FVector2f(ItemBox.Max.X - 18 - BuyW, Y), FVector2f(ItemBox.Max.X - 18, Y + BuyH)), RowBuyItem, I);
		}
	}
	FootBox = FBox2f(FVector2f(Pad, WalletBox.Max.Y + 16), FVector2f(PanelW - Pad, WalletBox.Max.Y + 16 + FootH));
	PanelSize = FVector2f(PanelW, FootBox.Max.Y + 18);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcCitadelScreen::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

void SAcCitadelScreen::Press(const FTarget& T)
{
	switch (T.Row)
	{
	case RowDonate: Donate(T.Index); break;
	case RowTab: SetTab(ETab(T.Index)); break;
	case RowBuyBlueprint: Buy(false, T.Index); break;
	case RowBuyItem: Buy(true, T.Index); break;
	case RowClose: Close(); break;
	default: break;
	}
}

FReply SAcCitadelScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Pressed = TargetAt(Local);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcCitadelScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcCitadelScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const int32 Hit = TargetAt(Local);
	const int32 Was = Pressed;
	Pressed = INDEX_NONE;
	if (Hit != INDEX_NONE && Hit == Was && Targets.IsValidIndex(Hit))
	{
		Press(Targets[Hit]);
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

FReply SAcCitadelScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	Hovered = TargetAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcCitadelScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcCitadelScreen::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcCitadelScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	// The world goes on behind, darkened.
	const float In = FMath::Clamp(float(Clock() - OpenedAt) / 0.18f, 0.f, 1.f);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.62f * In));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// The panel: dark glass, a steel rim with cut corners, the realm's gold inside it.
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.02, 0.036, 0.062, 0.97));
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)),
			WithAlpha(RealmGold(), 0.03f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), WithAlpha(RealmGold(), 0.28f), 1, 14);
	const int32 L = Layer + 3;
	PaintHeader(Out, L, P);
	switch (CurrentTab)
	{
	case ETab::Realm: PaintRealm(Out, L, P); break;
	case ETab::Shop: PaintShop(Out, L, P); break;
	}
	PaintFoot(Out, L, P);
	return L + 24;
}

void SAcCitadelScreen::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FTarget& T, const FString& Text,
	const bool bLit, const FLinearColor& Accent) const
{
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& X) { return X.Row == T.Row && X.Index == T.Index; });
	const bool bHover = Index == Hovered;
	const bool bDown = Index == Pressed && bHover;
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
	Box(Out, L + 1, P, T.Box, Fill);
	CutFrame(Out, L + 2, P, T.Box, Rim, bLit || bHover ? 1.5f : 1.0f, 6);
	Label(Text, 16, Ink, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0.6f).Paint(Out, L + 3, P, T.Box.GetCenter());
}

void SAcCitadelScreen::PaintHeader(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	Label(TEXT("THE REALM'S"), 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 24));
	FAcHudText Title = Label(TEXT("CITADEL"), 34, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.5f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	Title.Paint(Out, L, P, FVector2f(Pad + 2, 44));
	if (!Info.Builder.IsEmpty())
	{
		Label(FString::Printf(TEXT("built by %s · every pilot's"), *Info.Builder), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::SemiBold, 0.4f).Paint(Out, L, P, FVector2f(Pad + 186, 58));
	}
	// The tabs: REALM and SHOP, the open one lit (D1's upgrades join them).
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowTab) continue;
		const bool bOn = T.Index == int32(CurrentTab);
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == &T;
		const FLinearColor C = bOn ? FAcHudStyle::Text() : bHover ? FAcHudStyle::Ice() : Dim();
		static const TCHAR* const Names[] = {TEXT("REALM"), TEXT("SHOP")};
		Label(Names[T.Index], 21, C, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 1, P, T.Box.GetCenter());
		if (bOn)
		{
			const FBox2f U(FVector2f(T.Box.Min.X + 8, T.Box.Max.Y + 2), FVector2f(T.Box.Max.X - 8, T.Box.Max.Y + 5));
			Box(Out, L + 1, P, U, RealmGold());
		}
	}
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowClose) PaintButton(Out, L, P, T, TEXT("ESC"), false, RealmGold());
	}
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), WithAlpha(RealmGold(), 0.2f));
}

void SAcCitadelScreen::PaintRealm(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	auto Plate = [&](const FBox2f& B, const TCHAR* Title) {
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Label(Title, 13, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	};

	// The realm wallet: its ore and MH, large; the tax; what this pilot gave.
	const FBox2f W = WalletBox;
	Plate(W, TEXT("REALM WALLET"));
	const float Grow = FMath::Clamp(1.f - float(Clock() - GrewAt) / 1.4f, 0.f, 1.f);
	if (Grow > 0) Box(Out, L + 1, P, W.ExpandBy(-2), WithAlpha(RealmGold(), 0.12f * Grow));
	auto Stock = [&](float Y, const TCHAR* What, int64 N, const FLinearColor& C) {
		Box(Out, L + 1, P, FBox2f(FVector2f(W.Min.X + 18, Y + 4), FVector2f(W.Min.X + 22, Y + 44)), C);
		Label(What, 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(W.Min.X + 32, Y));
		Label(Info.bKnown ? Grouped(N) : FString(TEXT("—")), 30, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.4f)
			.Paint(Out, L + 2, P, FVector2f(W.Min.X + 32, Y + 13));
	};
	Stock(W.Min.Y + 44, TEXT("STARDUST ORE"), Info.RealmOre, FAcHudStyle::Amber());
	Stock(W.Min.Y + 106, TEXT("METALLIC HYDROGEN"), Info.RealmHydrogen, FAcHudStyle::Cyan());
	Box(Out, L + 1, P, FBox2f(FVector2f(W.Min.X + 18, W.Min.Y + 170), FVector2f(W.Max.X - 18, W.Min.Y + 171)), WithAlpha(Steel(), 0.5f));
	Label(FString::Printf(TEXT("TAX %lld%%"), (long long)Info.TaxPercent), 15, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 182));
	Label(TEXT("of every unload, here and at the Hab Domes"), 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L + 2, P, FVector2f(W.Min.X + 98, W.Min.Y + 183));
	Label(TEXT("YOU GAVE"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 218));
	Label(FString::Printf(TEXT("in tax  %s ore · %s MH"), *Grouped(Info.TaxedOre), *Grouped(Info.TaxedHydrogen)), 15, FAcHudStyle::Text(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 236));
	Label(FString::Printf(TEXT("donated  %s ore · %s MH"), *Grouped(Info.DonatedOre), *Grouped(Info.DonatedHydrogen)), 15, FAcHudStyle::Text(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 258));

	// Donate: from the pilot's own wallet, 100, 1,000 or all.
	const FBox2f D = DonateBox;
	Plate(D, TEXT("DONATE"));
	Label(FString::Printf(TEXT("YOUR WALLET   ORE %s  ·  MH %s"), *Grouped(Info.Ore), *Grouped(Info.Hydrogen)), 14, FAcHudStyle::Soft(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 2, P, D.Min + FVector2f(18, 44));
	Label(TEXT("Gifts to the realm wallet: the Citadel's upgrades are bought from it."), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::Medium).Paint(Out, L + 2, P, D.Min + FVector2f(18, 70));
	Label(TEXT("ORE"), 15, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L + 2, P, FVector2f(D.Min.X + 22, D.Min.Y + 112 + ButtonH / 2));
	Label(TEXT("MH"), 15, FAcHudStyle::Cyan(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L + 2, P, FVector2f(D.Min.X + 22, D.Min.Y + 112 + ButtonH + 22 + ButtonH / 2));
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowDonate) continue;
		const int32 Step = T.Index % 3;
		const FString Text = Step == 0 ? FString(TEXT("100")) : Step == 1 ? FString(TEXT("1,000")) : FString(TEXT("ALL"));
		PaintButton(Out, L, P, T, Text, WhyNot(T.Index).IsEmpty(), T.Index < 3 ? FAcHudStyle::Amber() : FAcHudStyle::Cyan());
		Label(FString::Printf(TEXT("%d"), T.Index + 1), 10, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 4, P, FVector2f(T.Box.Max.X - 6, T.Box.Min.Y + 3));
	}
	if (!Info.DonateWhyNot.IsEmpty())
	{
		Label(Info.DonateWhyNot, 14, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(D.Min.X + 18, D.Max.Y - 34));
	}
}

void SAcCitadelScreen::PaintShop(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	auto Plate = [&](const FBox2f& B, const TCHAR* Title) {
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Label(Title, 13, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	};
	auto Price = [](const FAcShopLine& X) {
		return X.Hydrogen > 0 ? FString::Printf(TEXT("%s ore · %s MH"), *Grouped(X.Ore), *Grouped(X.Hydrogen))
							  : FString::Printf(TEXT("%s ore"), *Grouped(X.Ore));
	};
	auto Find = [&](int32 Row, int32 Index) -> const FTarget* {
		return Targets.FindByPredicate([&](const FTarget& T) { return T.Row == Row && T.Index == Index; });
	};

	// Blueprints: every building's, the copies held, the floor, the next copy's price.
	const FBox2f B = BlueprintBox;
	Plate(B, TEXT("BLUEPRINTS"));
	Label(FString::Printf(TEXT("YOUR WALLET   ORE %s  ·  MH %s"), *Grouped(Info.Ore), *Grouped(Info.Hydrogen)), 12, FAcHudStyle::Soft(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 2, P, FVector2f(B.Max.X - 18, B.Min.Y + 15));
	for (int32 I = 0; I < Info.Blueprints.Num(); ++I)
	{
		const FAcShopLine& X = Info.Blueprints[I];
		const float Y = B.Min.Y + 46 + I * BlueprintRowH;
		const float Mid = Y + BlueprintRowH / 2;
		if (I % 2 == 0) Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 10, Y), FVector2f(B.Max.X - 10, Y + BlueprintRowH)), WithAlpha(Steel(), 0.08f));
		Label(FString::Printf(TEXT("%d"), I + 1), 10, Faint(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 18, Mid));
		Label(X.Name, 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 34, Mid));
		Label(FString::Printf(TEXT("×%lld"), (long long)X.Held), 16, X.Held > 0 ? Ready() : Faint(), EAcHAlign::Left, EAcVAlign::Center,
			EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(B.Min.X + 180, Mid));
		if (X.OpensAt > 0)
		{
			Label(FString::Printf(TEXT("opens at level %lld"), (long long)X.OpensAt), 13, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Center,
				EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(B.Min.X + 230, Mid));
		}
		Label(Price(X), 14, FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(B.Max.X - 30 - BuyW, Mid));
		if (const FTarget* T = Find(RowBuyBlueprint, I)) PaintButton(Out, L, P, *T, TEXT("BUY"), BuyWhyNot(false, I).IsEmpty(), RealmGold());
	}

	// Supplies: the four consumables, their stacks, their keys in the field.
	const FBox2f S = ItemBox;
	Plate(S, TEXT("SUPPLIES"));
	for (int32 I = 0; I < Info.Items.Num(); ++I)
	{
		const FAcShopLine& X = Info.Items[I];
		const float Y = S.Min.Y + 44 + I * ItemRowH;
		if (I > 0) Box(Out, L + 1, P, FBox2f(FVector2f(S.Min.X + 14, Y), FVector2f(S.Max.X - 14, Y + 1)), WithAlpha(Steel(), 0.35f));
		const FBox2f Chip(FVector2f(S.Min.X + 18, Y + 9), FVector2f(S.Min.X + 38, Y + 29));
		CutFrame(Out, L + 2, P, Chip, FAcHudStyle::Ice(), 1, 4);
		Label(X.Key, 12, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold).Paint(Out, L + 3, P, Chip.GetCenter());
		Label(X.Name, 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 9));
		Label(FString::Printf(TEXT("%lld/%lld"), (long long)X.Held, (long long)X.Stack), 14, X.Held > 0 ? Ready() : Faint(), EAcHAlign::Right,
			EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(S.Max.X - 26 - BuyW, Y + 10));
		Label(X.Note, 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 30));
		Label(Price(X), 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 45));
		if (const FTarget* T = Find(RowBuyItem, I)) PaintButton(Out, L, P, *T, TEXT("BUY"), BuyWhyNot(true, I).IsEmpty(), FAcHudStyle::Cyan());
	}
}

void SAcCitadelScreen::PaintFoot(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f F = FootBox;
	Box(Out, L, P, FBox2f(FVector2f(Pad, F.Min.Y - 8), FVector2f(PanelW - Pad, F.Min.Y - 7)), WithAlpha(RealmGold(), 0.14f));
	FString State = TEXT("Every unload pays the realm its cut; a gift goes all to it.");
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
	else if (!Bought.IsEmpty())
	{
		State = FString::Printf(TEXT("Buying %s…"), *Bought);
	}
	else if (Asked.Key + Asked.Value > 0)
	{
		State = FString::Printf(TEXT("Giving %s…"), *Purse(Asked.Key, Asked.Value));
	}
	else if (CurrentTab == ETab::Shop && !Info.ShopWhyNot.IsEmpty())
	{
		State = Info.ShopWhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	else if (CurrentTab == ETab::Shop)
	{
		State = TEXT("Paid from your own wallet, no tax. A blueprint builds once; each copy costs more.");
	}
	const bool bShop = CurrentTab == ETab::Shop;
	Label(bShop ? TEXT("SHOP") : TEXT("REALM"), 12, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 6));
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 26));
	const float KX = PanelW - Pad - 4;
	Label(bShop ? TEXT("1-8 BUY A BLUEPRINT · CLICK BUYS · TAB REALM") : TEXT("1 2 3 ORE · 4 5 6 MH · CLICK GIVES · TAB SHOP"), 11, Dim(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 8));
	Label(TEXT("H OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 28));
}
