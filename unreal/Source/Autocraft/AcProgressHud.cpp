#include "AcProgressHud.h"

#include "AcCanyon.h"

#include "AcMissions.h"

#include "AcAudioDirector.h"
#include "AcConsole.h"
#include "AcHUD.h"
#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcLeveling.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSites.h"
#include "SAcConsole.h"
#include "SAcRoot.h"
#include "SAcSkillScreen.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

#include "Brood.h"
#include "Defence.h"
#include "Pilot.h"
#include "RegionFile.h"
#include "Simulation.h"
#include "WindowMaps.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>

namespace
{
	/// A slot's square, the gap between, points.
	constexpr float Cell = 76;
	constexpr float CellGap = 14;
	/// The slots and the stacks stand this far either side of the view's
	/// middle (U1): clear of the pawn, which stands there.
	constexpr float PawnClear = 190;
	/// A toast's widest, points.
	constexpr float ToastMaxW = 520;

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

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

	/// The plates' cut corners (Chrome): top left and bottom right clipped by `Cut`.
	void CutFrame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W,
		float Cut)
	{
		const TArray<FVector2f> P{
			FVector2f(B.Min.X + Cut, B.Min.Y), FVector2f(B.Max.X, B.Min.Y), FVector2f(B.Max.X, B.Max.Y - Cut),
			FVector2f(B.Max.X - Cut, B.Max.Y), FVector2f(B.Min.X, B.Max.Y), FVector2f(B.Min.X, B.Min.Y + Cut), FVector2f(B.Min.X + Cut, B.Min.Y)};
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, C, true, W);
	}

	/// An arc round `C` of radius `R`, from twelve o'clock clockwise over `Share` of the turn.
	void Arc(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f C, float R, double Share,
		const FLinearColor& Color, float W)
	{
		if (Share <= 0) return;
		const int32 N = FMath::Max(2, FMath::CeilToInt(48 * Share));
		TArray<FVector2f> P;
		for (int32 I = 0; I <= N; ++I)
		{
			const double A = -UE_DOUBLE_HALF_PI + UE_DOUBLE_TWO_PI * Share * I / N;
			P.Add(C + FVector2f(float(std::cos(A)), float(std::sin(A))) * R);
		}
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), P, ESlateDrawEffect::None, Color, true, W);
	}

	FAcHudText Label(const FString& S, float Size, const FLinearColor& C, EAcHAlign H = EAcHAlign::Center,
		EAcVAlign V = EAcVAlign::Center, EAcFontWeight Weight = EAcFontWeight::Bold, float Kern = 0.6f)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		T.Kern = Kern;
		return T;
	}

	FString Upper(const std::string& S) { return Text(S).ToUpper(); }
}

// MARK: - The widget

void SAcProgressHud::Construct(const FArguments&)
{
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAcProgressHud::SetSlots(const TOptional<std::array<FAcSlotView, 3>>& Slots, const FAcCabLayout& Layout)
{
	const bool bSame = Row == Slots && Lay.Size == Layout.Size && Lay.FaceTop == Layout.FaceTop && Lay.Window.X == Layout.Window.X
		&& Lay.Window.W == Layout.Window.W;
	if (bSame) return;
	Row = Slots;
	Lay = Layout;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::SetItems(const TArray<FAcItemView>& Items)
{
	if (Stacks == Items) return;
	Stacks = Items;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::SetOpening(const TOptional<FOpening>& In)
{
	if (Opening == In) return;
	Opening = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::SetRespawn(const TOptional<FRespawn>& In)
{
	if (Respawn == In) return;
	Respawn = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::SetBoss(const TOptional<FBoss>& In)
{
	if (BossBar == In) return;
	BossBar = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::PaintBoss(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// Under the compass and the target's plate (U1): her name and the hp in
	// one line over a slim bar, on one glass plate; the banner slot under it.
	const FBoss& B = *BossBar;
	const float W = FMath::RoundHalfFromZero(FMath::Min(Size.X * 0.36f, 580.f));
	const float X = FMath::RoundHalfFromZero(Size.X / 2 - W / 2);
	const float Top = BossTop;
	const float Y = Top + 24;
	const float H = 10;
	const FLinearColor Violet = FAcHudStyle::Srgb(0.78, 0.36, 1.0);
	const FLinearColor Teal = FAcHudStyle::Srgb(0.3, 0.95, 0.85);
	const double Share = FMath::Clamp(B.Hp / FMath::Max(1.0, B.Max), 0.0, 1.0);
	const FBox2f Frame(FVector2f(X - 10, Top), FVector2f(X + W + 10, Y + H + 8));
	Box(Out, Layer, G, Frame, WithAlpha(FAcHudStyle::Glass(), 0.8f));
	CutFrame(Out, Layer + 2, G, Frame, WithAlpha(B.bRoaring ? Teal : Violet, B.bRoaring ? 1.0f : 0.55f), B.bRoaring ? 2.f : 1.f, 8);
	FString Name = B.Name;
	if (!B.Sub.IsEmpty()) Name += TEXT("  ·  ") + B.Sub;
	Label(Name, 15, B.Hp > 0 ? Violet : WithAlpha(FAcHudStyle::Soft(), 0.8f), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f)
		.Paint(Out, Layer + 2, G, FVector2f(X, Top + 4));
	const FString Numbers = B.Hp > 0 ? FString::Printf(TEXT("%s / %s"), *FText::AsNumber(FMath::CeilToInt64(B.Hp)).ToString(),
											*FText::AsNumber(FMath::RoundToInt64(B.Max)).ToString())
									 : FString(TEXT("DOWN"));
	Label(Numbers, 13, WithAlpha(FAcHudStyle::Text(), 0.9f), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Medium, 0.6f)
		.Paint(Out, Layer + 2, G, FVector2f(X + W, Top + 6));
	Box(Out, Layer + 1, G, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), WithAlpha(Violet, 0.14f));
	if (Share > 0) Box(Out, Layer + 1, G, FBox2f(FVector2f(X, Y), FVector2f(X + float(W * Share), Y + H)), WithAlpha(Violet, 0.92f));
	// A teal vein along the bar's top edge, as on her body.
	if (Share > 0) Box(Out, Layer + 2, G, FBox2f(FVector2f(X, Y), FVector2f(X + float(W * Share), Y + 2)), WithAlpha(Teal, 0.8f));
	// Quarter marks.
	for (int32 Q = 1; Q < 4; ++Q)
		Box(Out, Layer + 2, G, FBox2f(FVector2f(X + W * Q / 4.f - 1, Y), FVector2f(X + W * Q / 4.f + 1, Y + H)), WithAlpha(FAcHudStyle::Glass(), 0.7f));
}

void SAcProgressHud::SetCanyon(const TOptional<FCanyon>& In)
{
	if (CanyonPanel == In) return;
	CanyonPanel = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcProgressHud::PaintCanyon(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// Under the compass (or the boss bar), the boss bar's glass: the zone and
	// its wave on the first line, what it is doing under it, and while a wave
	// is out the prize taken against the fail mark as a slim bar.
	const FCanyon& C = *CanyonPanel;
	const float W = FMath::RoundHalfFromZero(FMath::Min(Size.X * 0.32f, 520.f));
	const float X = FMath::RoundHalfFromZero(Size.X / 2 - W / 2);
	const float Top = CanyonTop();
	const FLinearColor Orange = FAcHudStyle::Srgb(1.0, 0.6, 0.22);
	const FLinearColor Red = FAcHudStyle::Srgb(1.0, 0.32, 0.26);
	const FLinearColor Green = FAcHudStyle::Srgb(0.45, 1, 0.55);
	const FLinearColor Ink = C.bAlert ? Red : C.bHeld ? Green : Orange;
	const FBox2f Frame(FVector2f(X - 10, Top), FVector2f(X + W + 10, Top + CanyonHeight));
	Box(Out, Layer, G, Frame, WithAlpha(FAcHudStyle::Glass(), 0.8f));
	CutFrame(Out, Layer + 2, G, Frame, WithAlpha(Ink, C.bAlert ? 0.95f : 0.55f), C.bAlert ? 2.f : 1.f, 8);
	Label(C.Name.ToUpper(), 15, Ink, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, Layer + 2, G, FVector2f(X, Top + 5));
	Label(C.Title, 14, WithAlpha(FAcHudStyle::Text(), 0.95f), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, Layer + 2, G, FVector2f(X + W, Top + 6));
	Label(C.Line, 13, WithAlpha(FAcHudStyle::Text(), 0.85f), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, Layer + 2, G, FVector2f(X, Top + 26));
	if (C.Taken)
	{
		// The prize's ore taken: amber, red past three quarters of the mark.
		const float Y = Top + CanyonHeight - 9, H = 4;
		const double Share = FMath::Clamp(*C.Taken, 0.0, 1.0);
		Box(Out, Layer + 1, G, FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), WithAlpha(Orange, 0.16f));
		if (Share > 0)
			Box(Out, Layer + 1, G, FBox2f(FVector2f(X, Y), FVector2f(X + float(W * Share), Y + H)), WithAlpha(Share >= 0.75 ? Red : Orange, 0.95f));
	}
}

void SAcProgressHud::Toast(const FString& Title, const FString& Sub, const FLinearColor& Accent, const double Now)
{
	ac::ToastLane::Toast T;
	T.title = TCHAR_TO_UTF8(*Title);
	T.sub = TCHAR_TO_UTF8(*Sub);
	T.accent = {Accent.R, Accent.G, Accent.B, Accent.A};
	Lane.push(std::move(T), Now);
	Invalidate(EInvalidateWidgetReason::Paint);
	if (!Timer.IsValid()) Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcProgressHud::Animate));
}

void SAcProgressHud::Gain(const FString& Unit, const int64 Amount, const int64 Tax, const double Now)
{
	// Ore in the Prospector's violet, MH in ice.
	const FLinearColor Ink = Unit == TEXT("MH") ? FAcHudStyle::Srgb(0.84, 0.92, 0.98) : FAcHudStyle::Srgb(0.78, 0.66, 1);
	Lane.gain(TCHAR_TO_UTF8(*Unit), Amount, Tax, {Ink.R, Ink.G, Ink.B, Ink.A}, Now);
	Invalidate(EInvalidateWidgetReason::Paint);
	if (!Timer.IsValid()) Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcProgressHud::Animate));
}

TArray<FString> SAcProgressHud::ToastTitles() const
{
	TArray<FString> Out;
	for (const ac::ToastLane::Toast& T : Lane.shown) Out.Add(UTF8_TO_TCHAR(T.title.c_str()));
	return Out;
}

EActiveTimerReturnType SAcProgressHud::Animate(double, float)
{
	Lane.step(FPlatformTime::Seconds());
	Invalidate(EInvalidateWidgetReason::Paint);
	if (!Lane.empty()) return EActiveTimerReturnType::Continue;
	Timer.Reset();
	return EActiveTimerReturnType::Stop;
}

int32 SAcProgressHud::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle&, bool) const
{
	const FVector2f Size(Geometry.GetLocalSize());
	if (Size.X <= 0 || Size.Y <= 0) return Layer;
	if (Row) PaintSlots(Out, Layer, Geometry, Size);
	if (Opening) PaintOpening(Out, Layer, Geometry, Size);
	if (Respawn) PaintRespawn(Out, Layer, Geometry, Size);
	if (BossBar) PaintBoss(Out, Layer + 2, Geometry, Size);
	if (CanyonPanel) PaintCanyon(Out, Layer + 2, Geometry, Size);
	PaintToasts(Out, Layer + 4, Geometry, Size, FPlatformTime::Seconds());
	return Layer + 8;
}

void SAcProgressHud::PaintSlots(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// Bottom centre: on the dashboard's brow over its middle face (Swift
	// points, y up, in the layout), else a fifth of the way up the view.
	// U1: left of the pawn, which stands in the middle; the stacks right of it.
	const float RowW = 3 * Cell + 2 * CellGap;
	const bool bCab = Lay.Size.X > 0 && Lay.FaceTop > 0;
	const float MidX = bCab ? float(Lay.Window.MidX()) : Size.X / 2;
	const float Bottom = bCab ? Size.Y - float(Lay.FaceTop) - 22 : Size.Y * 0.8f;
	const FVector2f O(FMath::RoundHalfFromZero(MidX - PawnClear - RowW), FMath::RoundHalfFromZero(Bottom - Cell));
	SlotsAt = FBox2f(O - FVector2f(0, 11), O + FVector2f(RowW, Cell));
	const FLinearColor Glass = FAcHudStyle::Glass();
	for (int32 I = 0; I < 3; ++I)
	{
		const FAcSlotView& S = (*Row)[I];
		const FBox2f B(O + FVector2f(I * (Cell + CellGap), 0), O + FVector2f(I * (Cell + CellGap) + Cell, Cell));
		const FVector2f C = B.GetCenter();
		const FLinearColor Accent = S.Skill ? SAcSkillScreen::BranchColor(ac::Progression::node(*S.Skill).branch) : FAcHudStyle::Soft();
		const bool bReady = S.Skill && S.Left <= 0;
		Box(Out, Layer, G, B, WithAlpha(Glass, S.bOpen ? 0.88f : 0.7f));
		if (bReady) Box(Out, Layer, G, B.ExpandBy(-2), WithAlpha(Accent, 0.10f));
		CutFrame(Out, Layer + 1, G, B, S.bOpen ? WithAlpha(Accent, bReady ? 0.95f : 0.55f) : WithAlpha(FAcHudStyle::Soft(), 0.25f),
			bReady ? 1.6f : 1.0f, 8);
		if (!S.bOpen)
		{
			// Closed: the level it opens at.
			Label(TEXT("LV"), 13, WithAlpha(FAcHudStyle::Soft(), 0.6f)).Paint(Out, Layer + 2, G, C + FVector2f(0, -11));
			Label(FString::FromInt(S.OpenLevel), 24, WithAlpha(FAcHudStyle::Soft(), 0.8f)).Paint(Out, Layer + 2, G, C + FVector2f(0, 10));
		}
		else if (S.Skill)
		{
			const FSlateBrush* Icon = SAcSkillScreen::Icon(*S.Skill);
			const float IconS = 46;
			if (Icon)
			{
				FSlateDrawElement::MakeBox(Out, Layer + 2,
					G.ToPaintGeometry(FVector2D(IconS, IconS), FSlateLayoutTransform(FVector2D(C - FVector2f(IconS / 2, IconS / 2)))), Icon,
					ESlateDrawEffect::None, bReady ? FAcHudStyle::Text() : WithAlpha(FAcHudStyle::Soft(), 0.22f));
			}
			// The cooldown: a ring that closes as it comes back, and its seconds.
			const float R = Cell / 2 - 7;
			if (!bReady && S.Cooldown > 0)
			{
				const double Back = FMath::Clamp(1.0 - S.Left / S.Cooldown, 0.0, 1.0);
				Arc(Out, Layer + 3, G, C, R, 1.0, WithAlpha(FAcHudStyle::Soft(), 0.3f), 4);
				Arc(Out, Layer + 3, G, C, R, Back, Accent, 4);
				Label(FString::Printf(TEXT("%d"), FMath::CeilToInt(S.Left)), 26, FAcHudStyle::Text(), EAcHAlign::Center, EAcVAlign::Center,
					EAcFontWeight::Bold, 0).Paint(Out, Layer + 4, G, C);
			}
		}
		else
		{
			Label(TEXT("EMPTY"), 13, WithAlpha(FAcHudStyle::Soft(), 0.6f)).Paint(Out, Layer + 2, G, C);
		}
		// The key cap on the top edge.
		const FString Key = Text(ac::Progression::slotKey(I));
		const FBox2f Cap(FVector2f(C.X - 13, B.Min.Y - 11), FVector2f(C.X + 13, B.Min.Y + 11));
		Box(Out, Layer + 3, G, Cap, S.bOpen ? FAcHudStyle::KeyYellow() : WithAlpha(FAcHudStyle::Soft(), 0.35f));
		Label(Key, 16, FAcHudStyle::Glass(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0).Paint(Out, Layer + 4, G, Cap.GetCenter());
	}
	if (!Stacks.IsEmpty()) PaintItems(Out, Layer, G, FVector2f(MidX + PawnClear - 18, O.Y), O.Y + Cell);
}

void SAcProgressHud::PaintItems(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f RowRight, const float RowBottom) const
{
	// Small cells right of the slots, sitting on the row's base: the key cap,
	// the count, the cooldown's seconds; lit while its effect runs.
	constexpr float S = 46;
	constexpr float Between = 6;
	StacksAt = FBox2f(FVector2f(RowRight.X + 18, RowBottom - S - 8), FVector2f(RowRight.X + 18 + Stacks.Num() * (S + Between) - Between, RowBottom));
	for (int32 I = 0; I < Stacks.Num(); ++I)
	{
		const FAcItemView& V = Stacks[I];
		const float X = RowRight.X + 18 + I * (S + Between);
		const FBox2f B(FVector2f(X, RowBottom - S), FVector2f(X + S, RowBottom));
		const FVector2f C = B.GetCenter();
		const bool bHas = V.Held > 0;
		const bool bCool = V.Left > 0;
		const FLinearColor Accent = V.Running > 0 ? FAcHudStyle::LevelGold() : FAcHudStyle::Cyan();
		Box(Out, Layer, G, B, WithAlpha(FAcHudStyle::Glass(), bHas ? 0.86f : 0.6f));
		if (V.Running > 0) Box(Out, Layer, G, B.ExpandBy(-2), WithAlpha(Accent, 0.18f));
		CutFrame(Out, Layer + 1, G, B, bHas ? WithAlpha(Accent, bCool ? 0.4f : 0.85f) : WithAlpha(FAcHudStyle::Soft(), 0.22f), 1.0f, 5);
		Label(V.Short, 9, WithAlpha(FAcHudStyle::Soft(), bHas ? 0.9f : 0.45f), EAcHAlign::Center, EAcVAlign::Baseline, EAcFontWeight::Bold, 0.6f)
			.Paint(Out, Layer + 2, G, FVector2f(C.X, B.Max.Y - 4));
		const FString Mid = V.Running > 0 ? FString::Printf(TEXT("%d"), FMath::CeilToInt(V.Running))
			: bCool ? FString::Printf(TEXT("%d"), FMath::CeilToInt(V.Left))
			: FString::Printf(TEXT("×%lld"), (long long)V.Held);
		Label(Mid, 15, V.Running > 0 ? Accent : bHas && !bCool ? FAcHudStyle::Text() : WithAlpha(FAcHudStyle::Soft(), 0.6f), EAcHAlign::Center,
			EAcVAlign::Center, EAcFontWeight::Bold, 0).Paint(Out, Layer + 2, G, C + FVector2f(0, -1));
		// The key cap on the top edge, small.
		const FBox2f Cap(FVector2f(C.X - 9, B.Min.Y - 8), FVector2f(C.X + 9, B.Min.Y + 8));
		Box(Out, Layer + 3, G, Cap, bHas ? FAcHudStyle::KeyYellow() : WithAlpha(FAcHudStyle::Soft(), 0.35f));
		Label(FString::FromInt(I + 1), 12, FAcHudStyle::Glass(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0)
			.Paint(Out, Layer + 4, G, Cap.GetCenter());
	}
}

void SAcProgressHud::PaintOpening(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// On the site's model (else round the reticle): a ring that closes as the site opens.
	const FVector2f Share = Opening->At.Get(FVector2f(0.5f, 0.5f));
	const FVector2f C(FMath::RoundHalfFromZero(Size.X * Share.X), FMath::RoundHalfFromZero(Size.Y * Share.Y));
	const float R = 44;
	if (Opening->Refusal.IsEmpty())
	{
		Arc(Out, Layer, G, C, R, 1.0, WithAlpha(FAcHudStyle::Glass(), 0.7f), 7);
		Arc(Out, Layer + 1, G, C, R, 1.0, WithAlpha(FAcHudStyle::LevelGold(), 0.25f), 4);
		Arc(Out, Layer + 2, G, C, R, FMath::Clamp(Opening->Share, 0.0, 1.0), FAcHudStyle::LevelGold(), 4);
	}
	const FString Line = Opening->Refusal.IsEmpty()
		? FString::Printf(TEXT("%s  ·  %d%%"), *Opening->Title, FMath::FloorToInt(FMath::Clamp(Opening->Share, 0.0, 1.0) * 100))
		: Opening->Refusal;
	const FAcHudText T = Label(Line, 15, Opening->Refusal.IsEmpty() ? FAcHudStyle::LevelGold() : FAcHudStyle::Amber(), EAcHAlign::Center,
		EAcVAlign::Top, EAcFontWeight::Bold, 0.8f);
	const FVector2f M = T.Measure();
	const FVector2f At(C.X, C.Y + R + 12);
	Box(Out, Layer, G, FBox2f(At - FVector2f(M.X / 2 + 12, 4), At + FVector2f(M.X / 2 + 12, M.Y + 4)), WithAlpha(FAcHudStyle::Glass(), 0.78f));
	T.Paint(Out, Layer + 3, G, At);
}

void SAcProgressHud::PaintRespawn(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// The view dimmed; over its middle what fell, when and where it is back.
	Box(Out, Layer, G, FBox2f(FVector2f::ZeroVector, Size), FLinearColor(0.f, 0.f, 0.02f, 0.62f));
	const FVector2f C(Size.X / 2, Size.Y * 0.4f);
	const float W = 620, H = Respawn->Kept.IsEmpty() ? 196 : 232;
	const FBox2f B(C - FVector2f(W / 2, H / 2), C + FVector2f(W / 2, H / 2));
	const FLinearColor Red = FAcHudStyle::Srgb(1, 0.36, 0.3);
	Box(Out, Layer + 1, G, B, WithAlpha(FAcHudStyle::Glass(), 0.9f));
	Box(Out, Layer + 2, G, FBox2f(B.Min, FVector2f(B.Max.X, B.Min.Y + 3)), Red);
	CutFrame(Out, Layer + 2, G, B, WithAlpha(Red, 0.7f), 1.5f, 12);
	Label(TEXT("PROSPECTOR DOWN"), 22, Red, EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Bold, 3.f).Paint(Out, Layer + 3, G, FVector2f(C.X, B.Min.Y + 22));
	Label(FString::Printf(TEXT("BACK IN %d s"), Respawn->Left), 46, FAcHudStyle::Text(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Bold, 1.f)
		.Paint(Out, Layer + 3, G, FVector2f(C.X, B.Min.Y + 58));
	Label(Respawn->Where, 20, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f)
		.Paint(Out, Layer + 3, G, FVector2f(C.X, B.Min.Y + 126));
	if (!Respawn->Kept.IsEmpty())
	{
		Label(Respawn->Kept, 17, FAcHudStyle::LevelGold(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Medium, 0.4f)
			.Paint(Out, Layer + 3, G, FVector2f(C.X, B.Min.Y + 166));
	}
	Label(TEXT("A new Prospector comes for you"), 14, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Baseline, EAcFontWeight::Medium, 0.4f)
		.Paint(Out, Layer + 3, G, FVector2f(C.X, B.Max.Y - 14));
}

void SAcProgressHud::PaintToasts(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size, const double Now) const
{
	// U1: the one lane, upper left under the region's name, off the subject
	// in the middle; newest on top, sliding in from the left.
	float Y = LaneTop;
	for (const ac::ToastLane::Toast& N : Lane.shown)
	{
		const float A = float(ac::ToastLane::alpha(N, Now));
		if (A <= 0) continue;
		const FLinearColor Accent(N.accent[0], N.accent[1], N.accent[2], N.accent[3]);
		FString Title = UTF8_TO_TCHAR(N.title.c_str());
		// The same news again (not a gain, which adds up): how many.
		if (N.unit.empty() && N.count > 1) Title += FString::Printf(TEXT("  ×%lld"), (long long)N.count);
		const FString SubText = UTF8_TO_TCHAR(N.sub.c_str());
		const FAcHudText TitleT = Label(Title, 17, WithAlpha(Accent, A), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f);
		const FAcHudText Sub = Label(SubText, 13, WithAlpha(FAcHudStyle::Text(), 0.9f * A), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::Medium, 0.4f);
		const float W = FMath::Min(ToastMaxW, FMath::Max(TitleT.Measure().X, SubText.IsEmpty() ? 0.f : Sub.Measure().X) + 34);
		const float H = SubText.IsEmpty() ? 32 : 50;
		const double Age = N.shownAt ? Now - *N.shownAt : 0;
		const float Slide = Age < 0.2 ? float(14 * (1 - Age / 0.2)) : 0;
		const FBox2f B(FVector2f(LaneLeft - Slide, Y), FVector2f(LaneLeft - Slide + W, Y + H));
		Box(Out, Layer, G, B, WithAlpha(FAcHudStyle::Glass(), 0.84f * A));
		Box(Out, Layer + 1, G, FBox2f(B.Min, FVector2f(B.Min.X + 3, B.Max.Y)), WithAlpha(Accent, A));
		CutFrame(Out, Layer + 1, G, B, WithAlpha(Accent, 0.5f * A), 1, 7);
		// Clipped to the plate: a long line ends at its edge.
		Out.PushClip(FSlateClippingZone(G.ToPaintGeometry(FVector2D(B.GetSize() - FVector2f(6, 0)), FSlateLayoutTransform(FVector2D(B.Min)))));
		TitleT.Paint(Out, Layer + 2, G, FVector2f(B.Min.X + 14, B.Min.Y + 7));
		if (!SubText.IsEmpty()) Sub.Paint(Out, Layer + 2, G, FVector2f(B.Min.X + 14, B.Min.Y + 29));
		Out.PopClip();
		Y += H + 6;
	}
}

// MARK: - The words

TOptional<FAcLevelInfo> AcProgressHud::Level(const FAcMmoClient& Mmo)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return {};
	const double Xp = Mmo.Xp().value_or(P->xp);
	FAcLevelInfo L;
	L.Level = (int32)ac::Progression::level(Xp);
	if (L.Level < ac::Progression::cap)
	{
		const double From = ac::Progression::xp(L.Level), To = ac::Progression::xp(L.Level + 1);
		L.Fraction = FMath::Clamp((Xp - From) / FMath::Max(To - From, 1.0), 0.0, 1.0);
		L.Text = FString::Printf(TEXT("%lld / %lld XP"), (long long)std::floor(Xp), (long long)To);
	}
	else
	{
		L.Fraction = 1;
		L.Text = TEXT("MAX LEVEL");
	}
	if (P->points > 0) L.Badge = FString::Printf(TEXT("%lld POINT%s  ·  K"), (long long)P->points, P->points == 1 ? TEXT("") : TEXT("S"));
	return L;
}

FString AcProgressHud::BannerSub(const int64 Before, const int64 Level)
{
	const int64 N = ac::Progression::points(Level) - ac::Progression::points(FMath::Max<int64>(1, Before));
	if (N <= 0) return TEXT("PRESS K FOR THE SKILL TREE");
	return FString::Printf(TEXT("+%lld SKILL POINT%s  ·  K"), (long long)N, N == 1 ? TEXT("") : TEXT("S"));
}

FString AcProgressHud::BannerGate(const int64 Before, const int64 Level)
{
	TArray<FString> Lines;
	auto Crossed = [&](const int64 At) { return At > Before && At <= Level; };
	for (int64 Tier = 1; Tier <= ac::Progression::capstone; ++Tier)
	{
		if (!Crossed(ac::Progression::gate(Tier).level)) continue;
		Lines.Add(Tier == ac::Progression::capstone ? FString(TEXT("CAPSTONES OPEN")) : FString::Printf(TEXT("TIER %lld OPEN"), (long long)Tier));
	}
	for (int64 Slot = 0; Slot < ac::Progression::slotCount; ++Slot)
	{
		if (Crossed(ac::Progression::slotLevel(Slot))) Lines.Add(FString::Printf(TEXT("SLOT %s OPEN"), *Text(ac::Progression::slotKey(Slot))));
	}
	return FString::Join(Lines, TEXT("  ·  "));
}

FString AcProgressHud::BlueprintRefusal(const FAcMmoClient* Mmo, const ac::StructureKind Kind)
{
	const std::optional<ac::ProspectorProgress> G = Mmo ? Progress(*Mmo) : std::nullopt;
	if (!G) return FString();
	// The server's own rule and words (`ProspectorProgress::buildRefusal`),
	// counting the realm's buildings of that kind across the world (R1: the
	// limits are the realm's).
	int64 Owned = 0;
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		if (const auto It = P->realm.find(Kind); It != P->realm.end()) Owned = It->second;
	}
	const std::optional<std::string> Why = G->buildRefusal(Kind, Owned);
	return Why ? Text(*Why) : FString();
}

int64 AcProgressHud::Copies(const FAcMmoClient* Mmo, const ac::StructureKind Kind)
{
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	if (!P) return 0;
	const auto It = P->blueprints.find(Kind);
	return It == P->blueprints.end() ? 0 : It->second;
}

FString AcProgressHud::BlueprintTag(const FAcMmoClient* Mmo, const ac::StructureKind Kind)
{
	const std::optional<ac::ProspectorProgress> G = Mmo ? Progress(*Mmo) : std::nullopt;
	if (!G || BlueprintRefusal(Mmo, Kind).IsEmpty()) return FString();
	if (G->copies(Kind) <= 0) return TEXT("NO BLUEPRINT");
	if (G->level() < G->floor(Kind)) return FString::Printf(TEXT("LV %lld"), (long long)G->floor(Kind));
	return TEXT("REALM LIMIT");
}

TOptional<SAcProgressHud::FBoss> AcProgressHud::Boss(const FAcMmoClient& Mmo)
{
	const ac::net::Mirror* M = Mmo.Mirror();
	if (!M) return {};
	for (const ac::Unit& U : M->simulation().state.units)
	{
		if (U.kind != ac::UnitKind::matriarch || !U.bug) continue;
		// She is sent from 60 cells off (her crown read from afar); the bar
		// shows near her (35 cells) or once she is wounded (a fight).
		const ac::Unit* Me = M->driven() ? M->unit(*M->driven()) : nullptr;
		const bool bNear = Me && ac::distance(Me->position, U.position) <= ac::Matriarch::roarSight + 10;
		if (!bNear && !(U.hp < ac::Rules::hp(U.kind) - 0.5)) continue;
		SAcProgressHud::FBoss B;
		B.Name = TEXT("THE MATRIARCH");
		B.Sub = U.hp > 0 ? FString(TEXT("BROOD QUEEN")) : FString(TEXT("BROOD QUEEN  ·  FELLED"));
		// Never past her max, nor 0 while she lives (the wire rounds hp).
		B.Max = ac::Rules::hp(ac::UnitKind::matriarch);
		B.Hp = ac::Rules::hpShown(U.hp, B.Max);
		B.bRoaring = U.bug->state == ac::Bug::State::roar;
		return B;
	}
	return {};
}

FString AcProgressHud::BuiltBy(const FAcMmoClient* Mmo, const int64 Id)
{
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	if (!P) return FString();
	const auto It = P->builders.find(Id);
	if (It == P->builders.end()) return FString();
	return It->second == P->callsign ? FString(TEXT("you")) : Text(It->second);
}

std::optional<ac::ProspectorProgress> AcProgressHud::Progress(const FAcMmoClient& Mmo)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return std::nullopt;
	ac::ProspectorProgress G;
	G.xp = Mmo.Xp().value_or(P->xp);
	G.ranks = P->ranks;
	G.slots = P->slots;
	G.blueprints = P->blueprints;
	G.sites = P->sitesFound;
	G.rig = P->rig;
	return G;
}

namespace
{
	/// A blueprint site of the world, from its region file.
	struct FPlace
	{
		FString Region;
		FString RegionName;
		FString Name;
		FVector2f At;
		/// Region cells from the north-west corner.
		ac::Vec2 Cell;
		ac::SiteKind Kind = ac::SiteKind::crate;
		ac::StructureKind Blueprint = ac::StructureKind::citadel;
	};

	/// "<region id>/<site name>" → the site, read once a world.
	const TMap<FString, FPlace>& Places(const FAcMmoClient& Mmo)
	{
		static FString CachedFor;
		static TMap<FString, FPlace> Cache;
		if (CachedFor == Mmo.WorldPath()) return Cache;
		CachedFor = Mmo.WorldPath();
		Cache.Reset();
		FString Json;
		TSharedPtr<FJsonObject> World;
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (!FFileHelper::LoadFileToString(Json, *CachedFor) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), World) || !World
			|| !World->TryGetArrayField(TEXT("grid"), Rows))
		{
			return Cache;
		}
		for (const TSharedPtr<FJsonValue>& Row : *Rows)
		{
			for (const TSharedPtr<FJsonValue>& Entry : Row->AsArray())
			{
				FString File;
				if (!Entry->TryGetString(File)) continue;
				FString Body;
				if (!FFileHelper::LoadFileToString(Body, *FPaths::Combine(FPaths::GetPath(CachedFor), File))) continue;
				const ac::region::Parsed Parsed = ac::region::parse(TCHAR_TO_UTF8(*Body));
				if (!Parsed.file) continue;
				const ac::region::RegionFile& F = *Parsed.file;
				const ac::MapCells Cells = ac::cells(F.size);
				for (const ac::region::Site& S : F.sites)
				{
					FPlace P;
					P.Region = Text(F.id);
					P.RegionName = Text(F.name);
					P.Name = Text(S.name);
					P.At = FVector2f(float(S.at.x / FMath::Max(1.0, Cells.width)), float(S.at.y / FMath::Max(1.0, Cells.depth)));
					P.Cell = S.at;
					P.Kind = S.kind;
					P.Blueprint = S.blueprint;
					Cache.Add(P.Region + TEXT("/") + P.Name, P);
				}
			}
		}
		return Cache;
	}
}

TArray<FAcWorldMapInfo::FSite> AcProgressHud::KnownSites(const FAcMmoClient& Mmo)
{
	TArray<FAcWorldMapInfo::FSite> Out;
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return Out;
	const TMap<FString, FPlace>& All = Places(Mmo);
	for (const std::string& Id : P->sitesKnown)
	{
		const FPlace* Place = All.Find(Text(Id));
		if (!Place) continue;
		FAcWorldMapInfo::FSite S;
		S.Region = Place->Region;
		S.At = Place->At;
		S.Kind = Place->Kind;
		S.bFound = P->sitesFound.count(Id) > 0;
		Out.Add(S);
	}
	return Out;
}

std::vector<ac::net::atlas::KnownSite> AcProgressHud::AtlasSites(const FAcMmoClient& Mmo)
{
	std::vector<ac::net::atlas::KnownSite> Out;
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return Out;
	const TMap<FString, FPlace>& All = Places(Mmo);
	for (const std::string& Id : P->sitesKnown)
	{
		const FPlace* Place = All.Find(Text(Id));
		if (!Place) continue;
		Out.push_back(ac::net::atlas::KnownSite{TCHAR_TO_UTF8(*Place->Region), TCHAR_TO_UTF8(*Place->Name), Place->Kind, Place->Blueprint, Place->Cell,
			P->sitesFound.count(Id) > 0});
	}
	return Out;
}

TArray<AcProgressHud::FSitePlace> AcProgressHud::SitesIn(const FAcMmoClient& Mmo, const FString& Region)
{
	TArray<FSitePlace> Out;
	for (const TPair<FString, FPlace>& It : Places(Mmo))
	{
		if (It.Value.Region != Region) continue;
		FSitePlace& S = Out.AddDefaulted_GetRef();
		S.Id = It.Key;
		S.Kind = It.Value.Kind;
		S.Cell = It.Value.Cell;
	}
	return Out;
}

void AcProgressHud::BlueprintSites(const FAcMmoClient& Mmo, std::map<ac::StructureKind, FString>& Sources,
	std::map<ac::StructureKind, int32>& Marked)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return;
	const TMap<FString, FPlace>& All = Places(Mmo);
	for (const std::string& Id : P->sitesFound)
	{
		if (const FPlace* Place = All.Find(Text(Id)); Place && !Sources.count(Place->Blueprint))
		{
			Sources[Place->Blueprint] = FString::Printf(TEXT("%s, %s"), *Place->Name, *Place->RegionName);
		}
	}
	for (const std::string& Id : P->sitesKnown)
	{
		const FPlace* Place = All.Find(Text(Id));
		if (Place && !P->sitesFound.count(Id) && !P->blueprints.count(Place->Blueprint)) Marked[Place->Blueprint] += 1;
	}
}

TArray<FAcWorldMapInfo::FMark> AcProgressHud::ContractMarks(const FAcMmoClient& Mmo)
{
	TArray<FAcWorldMapInfo::FMark> Out;
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return Out;
	const TMap<FString, FPlace>& All = Places(Mmo);
	for (const ac::net::Contract& C : P->contracts)
	{
		if (C.state != ac::net::Contract::State::open) continue;
		if (C.kind != ac::net::Contract::Kind::reach && C.kind != ac::net::Contract::Kind::drill) continue;
		FAcWorldMapInfo::FMark M;
		M.Title = Text(C.title);
		if (C.kind == ac::net::Contract::Kind::drill)
		{
			M.Region = Text(C.region.empty() ? C.target : C.region);
			Out.Add(M);
			continue;
		}
		// reach: "<region>/<site name>" or "<region>/pad/<pad name>".
		const size_t Slash = C.target.find('/');
		if (Slash == std::string::npos) continue;
		M.Region = Text(C.target.substr(0, Slash));
		const std::string Rest = C.target.substr(Slash + 1);
		if (Rest.rfind("pad/", 0) == 0)
		{
			if (const ac::net::WorldMessage* W = Mmo.World())
			{
				for (const ac::net::WorldMessage::Region& R : W->regions)
				{
					if (R.id != C.target.substr(0, Slash)) continue;
					const ac::MapCells Cells = ac::cells(R.size);
					for (const ac::net::WorldMessage::Pad& Q : R.pads)
					{
						if (Q.name == Rest.substr(4))
						{
							M.At = FVector2f(float(Q.at.x / FMath::Max(1.0, Cells.width)), float(Q.at.y / FMath::Max(1.0, Cells.depth)));
						}
					}
				}
			}
		}
		else if (const FPlace* Place = All.Find(Text(C.target)))
		{
			M.At = Place->At;
		}
		Out.Add(M);
	}
	// L1: KSTR's next goal, when it has a place (a well, the Citadel, a
	// site, the nest).
	if (P->goal && P->goal->marked)
	{
		if (const ac::net::WorldMessage* W = Mmo.World())
		{
			for (const ac::net::WorldMessage::Region& R : W->regions)
			{
				if (R.id != P->goal->region) continue;
				const ac::MapCells Cells = ac::cells(R.size);
				FAcWorldMapInfo::FMark M;
				M.Title = Text(P->goal->title);
				M.Region = Text(R.id);
				M.At = FVector2f(float(FMath::Clamp((P->goal->at.x + Cells.width / 2.0) / FMath::Max(1.0, Cells.width), 0.0, 1.0)),
					float(FMath::Clamp((P->goal->at.y + Cells.depth / 2.0) / FMath::Max(1.0, Cells.depth), 0.0, 1.0)));
				Out.Add(M);
			}
		}
	}
	// The first ten minutes' steps with no place of their own: the pad to
	// build by (2), the nearest ore (3), the nearest well, then the Derrick
	// on it (5), in the pilot's region.
	const ac::net::Contract* Step = FirstStep(Mmo);
	const ac::net::WorldMessage::Region* Here = Mmo.Here();
	const ac::net::Mirror* Mir = Mmo.Mirror();
	if (Step && Here && Mir && Mir->simulation().map && Mir->driven() && Mir->unit(*Mir->driven()))
	{
		const ac::Unit& U = *Mir->unit(*Mir->driven());
		const auto& St = Mir->simulation().state;
		const ac::GroundRect& B = Mir->simulation().map->bounds;
		const ac::MapCells Cells = ac::cells(Here->size);
		TOptional<ac::Vec2> Best;
		auto Nearer = [&](const ac::Vec2 At) {
			if (!Best || ac::distance(At, U.position) < ac::distance(*Best, U.position)) Best = At;
		};
		if (Step->step == 3)
		{
			for (const ac::OreDeposit& D : St.patches)
			{
				if (D.remaining > 0) Nearer(D.position);
			}
		}
		else if (Step->step == 5 && Step->have >= 1)
		{
			for (const ac::Structure& S : St.structures)
			{
				if (S.kind == ac::StructureKind::derrick && S.owner == U.owner) Nearer(S.position);
			}
		}
		else if (Step->step == 5 && St.wells)
		{
			for (const ac::Well& W : *St.wells)
			{
				if (W.remaining > 0) Nearer(W.position);
			}
		}
		FAcWorldMapInfo::FMark M;
		M.Title = Text(Step->title);
		M.Region = Text(Here->id);
		if (Best)
		{
			M.At = FVector2f(float((Best->x - B.minX) / FMath::Max(1.0, Cells.width)), float((Best->y - B.minZ) / FMath::Max(1.0, Cells.depth)));
		}
		else if (Step->step == 2 && !Here->pads.empty())
		{
			const ac::net::WorldMessage::Pad& Q = Here->pads.front();
			M.At = FVector2f(float(Q.at.x / FMath::Max(1.0, Cells.width)), float(Q.at.y / FMath::Max(1.0, Cells.depth)));
		}
		if (M.At) Out.Add(M);
	}
	return Out;
}

const ac::net::Contract* AcProgressHud::FirstStep(const FAcMmoClient& Mmo)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return nullptr;
	for (const ac::net::Contract& C : P->contracts)
	{
		if (C.step > 0 && C.state == ac::net::Contract::State::open) return &C;
	}
	return nullptr;
}

namespace
{
/// "Lander wreck" from "start/Lander wreck".
FString SiteName(const ac::net::Contract& C)
{
	const size_t Slash = C.target.rfind('/');
	return Text(Slash == std::string::npos ? C.target : C.target.substr(Slash + 1));
}
} // namespace

FString AcProgressHud::FirstHint(const ac::net::Contract& C)
{
	switch (C.step)
	{
	case 1: return FString::Printf(TEXT("WALK TO THE %s  ·  THE GOLD COLUMN"), *SiteName(C).ToUpper());
	case 2: return TEXT("B  BUILD A CITADEL BY THE PAD");
	case 3: return FString::Printf(TEXT("DRILL A LOAD, BRING IT HOME  %lld / %lld ORE"), (long long)C.have, (long long)C.need);
	case 4: return FString::Printf(TEXT("HOLD CLICK ON THE %s  ·  5 S"), *SiteName(C).ToUpper());
	case 5: return C.have >= 1 ? FString(TEXT("DERRICK UP  ·  CLICK IT TO PUMP ONCE")) : FString(TEXT("B  BUILD A DERRICK ON A WELL"));
	case 6: return TEXT("K  SPEND A SKILL POINT");
	default: return FString();
	}
}

FString AcProgressHud::FirstTip(const ac::net::Contract& C)
{
	switch (C.step)
	{
	case 1: return FString::Printf(TEXT("WASD  walk to the gold column   ·   the %s opens at a touch"), *SiteName(C));
	case 2: return TEXT("B  build menu   ·   pick the Citadel   ·   click  place it near the pad");
	case 3: return TEXT("Hold click  drill ore at a deposit   ·   walk into your Citadel to unload");
	case 4: return FString::Printf(TEXT("Walk up to the %s   ·   hold click on it for 5 s to open it"), *SiteName(C));
	case 5: return C.have >= 1 ? FString(TEXT("click  enter your Derrick: it pumps a load of MH"))
							   : FString(TEXT("B  build menu   ·   pick the Derrick   ·   click  place it on a well"));
	case 6: return TEXT("K  skill tree   ·   click a skill to spend your point");
	default: return FString();
	}
}

TArray<FAcPilotGoal> AcProgressHud::ContractGoals(const FAcMmoClient& Mmo, const ac::Vec2 From)
{
	TArray<FAcPilotGoal> Out;
	const ac::net::WorldMessage::Region* Here = Mmo.Here();
	const ac::net::Mirror* M = Mmo.Mirror();
	if (!Here || !M || !M->simulation().map) return Out;
	const ac::GroundRect& B = M->simulation().map->bounds;
	const ac::MapCells Cells = ac::cells(Here->size);
	for (const FAcWorldMapInfo::FMark& K : ContractMarks(Mmo))
	{
		if (!K.At || K.Region != Text(Here->id)) continue;
		const ac::Vec2 At(B.minX + K.At->X * Cells.width, B.minZ + K.At->Y * Cells.depth);
		Out.Add(FAcPilotGoal{TEXT("KSTR"), At, ac::distance(At, From)});
	}
	return Out;
}

FString AcProgressHud::Countdown(const double Seconds)
{
	const int64 S = FMath::Max<int64>(0, (int64)std::ceil(Seconds));
	if (S >= 3600) return FString::Printf(TEXT("%lld:%02lld:%02lld"), (long long)(S / 3600), (long long)(S / 60 % 60), (long long)(S % 60));
	return FString::Printf(TEXT("%lld:%02lld"), (long long)(S / 60), (long long)(S % 60));
}

FAcContractsInfo AcProgressHud::Contracts(const FAcMmoClient& Mmo, const bool bArmed)
{
	FAcContractsInfo Out;
	Out.bArmed = bArmed;
	const ac::net::ProgressMessage* P = Mmo.Progress();
	if (!P) return Out;
	auto RegionName = [&](const std::string& Id) {
		if (const ac::net::WorldMessage* W = Mmo.World())
		{
			for (const ac::net::WorldMessage::Region& R : W->regions)
			{
				if (R.id == Id) return Upper(R.name);
			}
		}
		return Upper(Id);
	};
	const double Sky = Mmo.Sky() ? Mmo.Sky()->time : 0;
	using K = ac::net::Contract::Kind;
	for (const ac::net::Contract& C : P->contracts)
	{
		FAcContractRow Row;
		Row.State = C.state == ac::net::Contract::State::done ? FAcContractRow::EState::Done
			: C.state == ac::net::Contract::State::abandoned ? FAcContractRow::EState::Abandoned : FAcContractRow::EState::Open;
		Row.Title = Text(C.title);
		Row.Pitch = Text(C.pitch);
		Row.Share = C.need > 0 ? double(C.have) / double(C.need) : 0;
		Row.Pay = FString::Printf(TEXT("+%lld XP"), (long long)std::llround(C.xp));
		if (C.hydrogen > 0) Row.Pay += FString::Printf(TEXT("  ·  +%lld MH"), (long long)C.hydrogen);
		const long long Have = (long long)C.have, Need = (long long)C.need;
		// What is left to do, in the panel's words.
		const size_t Slash = C.target.find('/');
		const std::string Place = Slash == std::string::npos ? C.target : C.target.substr(C.target.rfind('/') + 1);
		switch (C.kind)
		{
		case K::deliverOre: Row.Progress = FString::Printf(TEXT("%lld / %lld ORE HOME"), Have, Need); break;
		case K::deliverHydrogen: Row.Progress = FString::Printf(TEXT("%lld / %lld MH HOME"), Have, Need); break;
		case K::drill: Row.Progress = FString::Printf(TEXT("%lld / %lld LOADS · %s"), Have, Need, *RegionName(C.target)); break;
		case K::sweep: Row.Progress = FString::Printf(TEXT("%lld / %lld LOADS IN SHADOW"), Have, Need); break;
		case K::reach:
			Row.Progress = FString::Printf(TEXT("REACH %s · %s"), *Upper(Place), *RegionName(C.region.empty() ? C.target.substr(0, Slash) : C.region));
			break;
		case K::build:
		{
			const std::optional<ac::StructureKind> Kind = ac::parse<ac::StructureKind>(C.target);
			Row.Progress = FString::Printf(TEXT("BUILD %s  %lld / %lld"), Kind ? *Upper(ac::Simulation::title(*Kind)) : *Upper(C.target), Have, Need);
			break;
		}
		case K::skill: Row.Progress = TEXT("SPEND A SKILL POINT"); break;
		}
		if (C.step > 0)
		{
			// The first ten minutes: its hint in gold; the next step follows at once.
			Row.bFirst = true;
			Row.Progress = FirstHint(C);
			if (Row.State == FAcContractRow::EState::Open) Out.FirstStep = C.step;
		}
		else if (Row.State != FAcContractRow::EState::Open)
		{
			const FString In = P->contractsDawn > Sky ? FString::Printf(TEXT(", IN %s"), *Countdown(P->contractsDawn - Sky)) : FString();
			Row.Dawn = (Row.State == FAcContractRow::EState::Done ? TEXT("NEW ONE AT DAWN") : TEXT("BACK AT DAWN")) + In;
		}
		Out.Rows.Add(Row);
	}
	// L1: KSTR's next goal.
	if (P->goal)
	{
		Out.bNext = true;
		Out.NextTitle = Text(P->goal->title);
		Out.NextHint = Text(P->goal->hint);
		Out.NextPitch = Text(P->goal->pitch);
	}
	return Out;
}

void UAcProgressHudSubsystem::ToggleAbandon()
{
	ArmedAt = AbandonArmed() ? -1 : FPlatformTime::Seconds();
	UE_LOG(LogAutocraft, Log, TEXT("contracts: abandon %s"), ArmedAt < 0 ? TEXT("disarmed") : TEXT("armed"));
}

bool UAcProgressHudSubsystem::AbandonArmed() const { return ArmedAt >= 0 && FPlatformTime::Seconds() - ArmedAt < 6; }

void UAcProgressHudSubsystem::ToggleContracts()
{
	bContractsOpen = !bContractsOpen;
	ContractsChangedAt = -1;
	UE_LOG(LogAutocraft, Log, TEXT("contracts: panel %s"), bContractsOpen ? TEXT("whole") : TEXT("compact"));
}

bool UAcProgressHudSubsystem::ContractsExpanded() const
{
	// A contract that changed shows whole for this long.
	constexpr double ContractsShown = 6.0;
	return bContractsOpen || AbandonArmed() || (ContractsChangedAt >= 0 && FPlatformTime::Seconds() - ContractsChangedAt < ContractsShown);
}

bool UAcProgressHudSubsystem::Abandon(const int32 Index)
{
	ArmedAt = -1;
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr;
	if (!P || Index < 0 || Index >= (int32)P->contracts.size()) return false;
	const ac::net::Contract& C = P->contracts[size_t(Index)];
	if (C.state != ac::net::Contract::State::open) return false;
	FString Why;
	const bool bAsked = Mmo->Abandon(C.id, &Why);
	UE_LOG(LogAutocraft, Log, TEXT("contracts: abandon %u (%s)%s"), C.id, *Text(C.title), bAsked ? TEXT("") : *(TEXT(": ") + Why));
	return bAsked;
}

// MARK: - The subsystem

UAcProgressHudSubsystem* UAcProgressHudSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAcProgressHudSubsystem>() : nullptr;
}

bool UAcProgressHudSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAcProgressHudSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		FrameHandle = Sim->AddFrameListener(EAcFrameStage::Hud, FAcFrameEvent::FDelegate::CreateUObject(this, &UAcProgressHudSubsystem::OnFrame));
	}
}

void UAcProgressHudSubsystem::Deinitialize()
{
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this)) Sim->RemoveFrameListener(EAcFrameStage::Hud, FrameHandle);
	if (AAcHUD* H = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = H->Root(); Root && Hud) Root->RemoveLayer(Hud.ToSharedRef());
	}
	Hud.Reset();
	Super::Deinitialize();
}

bool UAcProgressHudSubsystem::Mount()
{
	if (Hud) return true;
	AAcHUD* H = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = H ? H->Root() : nullptr;
	if (!Root) return false;
	Root->AddLayer(HudLayer)[SAssignNew(Hud, SAcProgressHud)];
	return true;
}

void UAcProgressHudSubsystem::OnFrame(const FAcFrame& Frame)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().bServerLevels = Mmo != nullptr;
	if (!Mount()) return;
	if (!Mmo)
	{
		Hud->SetSlots({}, FAcCabLayout());
		if (UAcConsoleSubsystem* C = UAcConsoleSubsystem::Get(this); C && C->Console()) C->Console()->SetContracts({});
		Shown = -1;
		ArmedAt = -1;
		return;
	}
	News(*Mmo);
	// The mirror runs the server's rules with the pilot's progress too (its
	// full hp with Plating, world prices, its Hab Domes as depots): the
	// server's Region sets the same on its sim. Its buildings: the side's
	// in the region (the wire names no builder).
	if (ac::net::Mirror* M = Mmo->Mirror())
	{
		ac::Simulation& S = M->simulation();
		const std::optional<ac::ProspectorProgress> G = AcProgressHud::Progress(*Mmo);
		if (S.progress != G) S.setProgress(G);
		std::set<int64_t> Mine;
		for (const ac::Structure& B : S.state.structures)
		{
			if (B.owner == ac::Pilot::player) Mine.insert(B.id);
		}
		if (S.pilotBuildings != Mine) S.pilotBuildings = std::move(Mine);
	}
	// Fallen: the respawn screen, counting down to the server's time.
	TOptional<SAcProgressHud::FRespawn> Down;
	const ac::net::ProgressMessage* Fell = Mmo->Progress();
	if (const std::optional<ac::net::ProgressMessage::Respawn> Real = Fell ? Fell->respawn : std::nullopt; Real || RespawnPreview)
	{
		const ac::net::ProgressMessage::Respawn& R = Real ? *Real : *RespawnPreview;
		SAcProgressHud::FRespawn V;
		V.Left = (int32)FMath::Max(0.0, std::ceil(R.at - (Mmo->Sky() ? Mmo->Sky()->time : R.at)));
		V.Where = !R.home ? FString(TEXT("AT THE LANDING PAD")) : FString::Printf(TEXT("AT YOUR %s"), *Upper(ac::Simulation::title(*R.home)));
		const ac::net::WorldMessage::Region* Here = Mmo->Here();
		if (!Here || Here->id != R.region)
		{
			if (const ac::net::WorldMessage* W = Mmo->World())
			{
				for (const ac::net::WorldMessage::Region& G : W->regions)
				{
					if (G.id == R.region) V.Where += TEXT(", ") + Upper(G.name);
				}
			}
		}
		if (R.cargo > 0) V.Kept = FString::Printf(TEXT("Second wind kept %lld %s"), (long long)R.cargo, R.hydrogen ? TEXT("MH") : TEXT("ore"));
		Down = V;
	}
	Hud->SetRespawn(Down);
	// The Matriarch in the mirror (in sight, or close enough to fight): her
	// boss bar (N3).
	Hud->SetBoss(AcProgressHud::Boss(*Mmo));
	// A canyon zone near (Z2): its wave panel, its warnings.
	{
		TOptional<SAcProgressHud::FCanyon> Panel;
		if (const ac::net::Sky::Zone* Z = AcCanyon::Near(*Mmo))
		{
			const FAcZonePlace* Place = AcCanyon::Place(*Mmo, *Z);
			SAcProgressHud::FCanyon V;
			V.Name = Place ? Place->Name : FString(TEXT("Canyon"));
			V.Title = AcCanyon::Title(*Z);
			V.Line = AcCanyon::Line(*Z, Mmo->Sky() ? Mmo->Sky()->time : 0);
			if (Z->ends) V.Taken = double(Z->taken) / double(FMath::Max<int64>(1, Z->mark));
			V.bAlert = Z->ends.has_value() || Z->state == ac::net::Sky::Zone::State::failed;
			V.bHeld = Z->state == ac::net::Sky::Zone::State::held;
			Panel = V;
		}
		Hud->SetCanyon(Panel);
		CanyonWarnings(*Mmo);
	}
	// The banner slot: under the compass (or the boss bar); it waits while a
	// screen covers the view (U1).
	if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
	{
		const AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
		L->PlaceBanner(Hud->BannerTop(), Pilot && Pilot->ScreenOpen());
	}
	// The KSTR contracts on the dashboard's right panel.
	if (UAcConsoleSubsystem* C = UAcConsoleSubsystem::Get(this); C && C->Console())
	{
		TOptional<FAcContractsInfo> Panel;
		if (Mmo->Progress() && (!Mmo->Progress()->contracts.empty() || Mmo->Progress()->goal))
		{
			// A contract new, done or dropped (not its progress, a load at a
			// time: U1) opens the panel a while.
			FString Seen;
			for (const ac::net::Contract& K : Mmo->Progress()->contracts)
			{
				Seen += FString::Printf(TEXT("%u:%d;"), K.id, int32(K.state));
				ContractTitles.Add(K.id, Upper(K.title));
			}
			if (Seen != ContractsSeen)
			{
				if (!ContractsSeen.IsEmpty()) ContractsChangedAt = FPlatformTime::Seconds();
				ContractsSeen = Seen;
			}
			Panel = AcProgressHud::Contracts(*Mmo, AbandonArmed());
			Panel->bExpanded = ContractsExpanded();
		}
		// A step of the first ten minutes opens: the radio calls it.
		const ac::net::Contract* Step = AcProgressHud::FirstStep(*Mmo);
		if (Step && Step->id != FirstSeen)
		{
			Hud->Toast(FString::Printf(TEXT("KSTR: %s"), *Upper(Step->title)), AcProgressHud::FirstHint(*Step), FAcHudStyle::LevelGold(),
				FPlatformTime::Seconds());
			UE_LOG(LogAutocraft, Log, TEXT("contracts: first ten minutes step %d, %s"), int32(Step->step), *Text(Step->title));
		}
		FirstSeen = Step ? Step->id : 0;
		// L1: a new goal from KSTR: the radio calls it (its title, its line).
		const ac::net::ProgressMessage* Pm = Mmo->Progress();
		const FString Goal = Pm && Pm->goal ? Text(Pm->goal->key + "/" + Pm->goal->title) : FString();
		if (!Goal.IsEmpty() && Goal != GoalSeen)
		{
			Hud->Toast(FString::Printf(TEXT("KSTR  ·  NEXT: %s"), *Upper(Pm->goal->title)), Text(Pm->goal->pitch), FAcHudStyle::LevelGold(),
				FPlatformTime::Seconds());
			UE_LOG(LogAutocraft, Log, TEXT("contracts: next goal %s, %s"), *Text(Pm->goal->key), *Text(Pm->goal->title));
			// A2: KSTR's announcer reads it.
			// (Not a wait for a level, "level:N": nothing to read yet.)
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this); Audio && Pm->goal->key.rfind("level:", 0) != 0)
				Audio->SayRadio(TEXT("goal-") + Text(Pm->goal->key).ToLower());
		}
		if (Pm) GoalSeen = Goal;
		C->Console()->SetContracts(Panel);
	}

	// The slot row while driving the pilot's Prospector (not in flight).
	TOptional<std::array<FAcSlotView, 3>> Row;
	const AAcPilotPawn* Pawn = AAcPilotPawn::Find(this);
	const ac::net::ProgressMessage* P = Mmo->Progress();
	if (P && Pawn && Pawn->Driving() && !Pawn->InFlight() && !Mmo->Travelling())
	{
		const int64 Level = ac::Progression::level(Mmo->Xp().value_or(P->xp));
		std::array<FAcSlotView, 3> Slots;
		for (int32 I = 0; I < 3; ++I)
		{
			FAcSlotView& S = Slots[I];
			S.OpenLevel = (int32)ac::Progression::slotLevel(I);
			S.bOpen = Level >= S.OpenLevel;
			S.Skill = P->slots[I];
			if (S.Skill)
			{
				const ac::SkillNode& N = ac::Progression::node(*S.Skill);
				S.Cooldown = N.active ? N.active->cooldown : 0;
				const auto Ready = P->ready.find(*S.Skill);
				// Whole tenths: the row repaints when the number moves.
				if (Ready != P->ready.end()) S.Left = FMath::Max(0.0, std::ceil((Ready->second - Frame.Time) * 10) / 10);
			}
		}
		Row = Slots;
	}
	const UAcConsoleSubsystem* ConsoleSub = UAcConsoleSubsystem::Get(this);
	const TSharedPtr<SAcConsole> Console = ConsoleSub ? ConsoleSub->Console() : nullptr;
	Hud->SetSlots(Row, Console ? Console->Layout() : FAcCabLayout());
	// C2: the consumables' stacks beside the slots, their cooldowns and
	// what runs (whole tenths of world time: a repaint when they move).
	TArray<FAcItemView> Items;
	if (Row)
	{
		const double SkyTime = Mmo->Sky() ? Mmo->Sky()->time : 0;
		auto Tenths = [](double X) { return FMath::Max(0.0, std::ceil(X * 10) / 10); };
		static const TCHAR* const Shorts[] = {TEXT("KIT"), TEXT("MH"), TEXT("SHIELD"), TEXT("FLARE")};
		for (const ac::ItemInfo& I : ac::Shop::items())
		{
			FAcItemView V;
			V.Short = Shorts[FMath::Clamp(int32(I.item), 0, 3)];
			const auto Held = P->items.find(I.item);
			V.Held = Held == P->items.end() ? 0 : Held->second;
			V.Cooldown = I.cooldown;
			if (const auto Ready = P->itemReady.find(I.item); Ready != P->itemReady.end()) V.Left = Tenths(Ready->second - SkyTime);
			if (I.item == ac::Item::shieldCharge && P->shieldUntil > 0) V.Running = Tenths(P->shieldUntil - SkyTime);
			if (I.item == ac::Item::surveyFlare && P->flare) V.Running = Tenths(P->flare->until - SkyTime);
			Items.Add(V);
		}
	}
	Hud->SetItems(Items);

	// A site being opened: the hold ring (whole percents: a repaint each).
	TOptional<SAcProgressHud::FOpening> Opening;
	if (P && P->opening && Row)
	{
		const ac::net::ProgressMessage::Opening& O = *P->opening;
		SAcProgressHud::FOpening V;
		const int32 Slash = O.site.find('/') == std::string::npos ? -1 : int32(O.site.find('/'));
		V.Title = FString(UTF8_TO_TCHAR(O.site.substr(size_t(Slash + 1)).c_str())).ToUpper();
		V.Refusal = Text(O.refusal);
		const double Sky = Mmo->Sky() ? Mmo->Sky()->time : O.since;
		V.Share = O.needs > 0 ? std::floor(FMath::Clamp((Sky - O.since) / O.needs, 0.0, 1.0) * 100) / 100 : 0;
		// On the site's model when it is in view (W2), in whole hundredths.
		const AAcSites* Sites = AAcSites::Find(GetWorld());
		const TOptional<FVector> Where = Sites ? Sites->Where(UTF8_TO_TCHAR(O.site.c_str())) : TOptional<FVector>();
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		FVector2D Screen;
		int32 ViewW = 0, ViewH = 0;
		if (PC) PC->GetViewportSize(ViewW, ViewH);
		if (Where && PC && ViewW > 0 && ViewH > 0 && PC->ProjectWorldLocationToScreen(*Where, Screen))
		{
			const FVector2f OnView(float(Screen.X / ViewW), float(Screen.Y / ViewH));
			if (OnView.X > 0.08f && OnView.X < 0.92f && OnView.Y > 0.08f && OnView.Y < 0.85f)
			{
				V.At = FVector2f(FMath::RoundToFloat(OnView.X * 100) / 100, FMath::RoundToFloat(OnView.Y * 100) / 100);
			}
		}
		Opening = V;
	}
	Hud->SetOpening(Opening);
}

void UAcProgressHudSubsystem::CanyonWarnings(FAcMmoClient& Mmo)
{
	// Z2: the zone's banner on coming into its region; a toast and the swarm
	// when a wave comes (from which tunnels), when one is beaten, when the
	// opening wave breaks through.
	using FZone = ac::net::Sky::Zone;
	const FZone* Z = AcCanyon::Near(Mmo);
	const ac::net::WorldMessage* W = Mmo.World();
	if (!Z || !W)
	{
		CanyonSeen.Reset();
		if (!Z) CanyonBanner = -1;
		return;
	}
	const FAcZonePlace* Place = AcCanyon::Place(Mmo, *Z);
	const FString Name = Place ? Place->Name.ToUpper() : FString(TEXT("THE CANYON"));
	const double Now = FPlatformTime::Seconds();
	const double Clock = Mmo.Sky() ? Mmo.Sky()->time : 0;
	if (Z->region == W->here && CanyonBanner != Z->region)
	{
		CanyonBanner = Z->region;
		if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
			L->ShowBanner(Name, TEXT("CANYON ZONE  ·  ") + AcCanyon::Title(*Z), AcCanyon::Line(*Z, Clock), false, FAcHudStyle::Srgb(1.0, 0.6, 0.22));
	}
	else if (Z->region != W->here)
	{
		CanyonBanner = -1;
	}
	FCanyonSeen Was;
	const bool bKnew = CanyonSeen.IsSet() && CanyonSeen->Region == Z->region && CanyonSeen->Zone == Z->zone;
	if (bKnew) Was = *CanyonSeen;
	FCanyonSeen Is;
	Is.Region = Z->region;
	Is.Zone = Z->zone;
	Is.State = uint8(Z->state);
	Is.Wave = Z->wave;
	Is.bOut = Z->ends.has_value();
	Is.Ends = Z->ends.value_or(0);
	CanyonSeen = Is;
	if (!bKnew) return;
	const FLinearColor Red = FAcHudStyle::Srgb(1.0, 0.32, 0.26);
	if (Is.bOut && (!Was.bOut || Was.Wave != Is.Wave || FMath::Abs(Was.Ends - Is.Ends) > 5))
	{
		// A wave comes.
		const FString Title = Z->state == FZone::State::waiting
			? FString::Printf(TEXT("%s: THE OPENING WAVE"), *Name)
			: FString::Printf(TEXT("%s: WAVE %lld OF %lld%s"), *Name, (long long)Z->wave, (long long)Z->waves, Z->boss ? TEXT(", MINI-BOSS") : TEXT(""));
		const FString Sub = FString::Printf(TEXT("%lld bugs from %s%s"), (long long)Z->left, *AcCanyon::Tunnels(*Z, Place),
			Z->state == FZone::State::waiting ? TEXT("  ·  beat it to start the canyon's game") : TEXT(""));
		Hud->Toast(Title, Sub, Red, Now);
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this))
		{
			Audio->PlayMenu("broodswarm", 0.7f);
			// A2: KSTR calls it.
			if (Place && !Place->Id.IsEmpty()) Audio->SayRadio(FString::Printf(TEXT("canyon-%s-wave"), *Place->Id));
		}
		UE_LOG(LogAutocraft, Log, TEXT("canyon: %s"), *(Title + TEXT(" · ") + Sub));
	}
	else if (Was.bOut && !Is.bOut)
	{
		if (Z->state == FZone::State::running && Is.Wave == Was.Wave + 1)
		{
			Hud->Toast(FString::Printf(TEXT("%s: WAVE %lld BEATEN"), *Name, (long long)Was.Wave), AcCanyon::Line(*Z, Clock),
				FAcHudStyle::Srgb(0.45, 1, 0.55), Now);
			UE_LOG(LogAutocraft, Log, TEXT("canyon: wave %lld beaten"), (long long)Was.Wave);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this); Audio && Place && !Place->Id.IsEmpty())
				Audio->SayRadio(FString::Printf(TEXT("canyon-%s-beaten"), *Place->Id));
		}
		else if (Z->state == FZone::State::waiting && Was.State == uint8(FZone::State::waiting))
		{
			Hud->Toast(FString::Printf(TEXT("%s: THE OPENING WAVE GOT THROUGH"), *Name), AcCanyon::Line(*Z, Clock), FAcHudStyle::Amber(), Now);
		}
	}
}

void UAcProgressHudSubsystem::News(FAcMmoClient& Mmo)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	const double Now = FPlatformTime::Seconds();
	int64 Reached = -1;
	for (const ac::net::ProgressMessage::News& N : Mmo.TakeProgressNews()) NewsItem(Mmo, N, Now, Reached);
	const int64 Level = P ? ac::Progression::level(Mmo.Xp().value_or(P->xp)) : -1;
	// The first progress sets where the pilot is: no banner for a resumed pilot.
	if (Shown < 0)
	{
		Shown = Level;
		return;
	}
	if (Reached > Shown)
	{
		UE_LOG(LogAutocraft, Log, TEXT("progress: prospector level %lld (was %lld)"), (long long)Reached, (long long)Shown);
		if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
		{
			L->ShowBanner(AcLeveling::BannerTitle(ac::UnitKind::prospector, (int32)Reached), AcProgressHud::BannerSub(Shown, Reached),
				AcProgressHud::BannerGate(Shown, Reached));
		}
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().LevelUp();
		Shown = Reached;
	}
	else if (Level >= 0 && Level < Shown)
	{
		// A new world, a respec of the save: start again from here.
		Shown = Level;
	}
}

void UAcProgressHudSubsystem::PreviewNews(const ac::net::ProgressMessage::News& N)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	int64 Reached = -1;
	if (Mmo && Mount()) NewsItem(*Mmo, N, FPlatformTime::Seconds(), Reached);
}

void UAcProgressHudSubsystem::PreviewRespawn(const std::optional<ac::net::ProgressMessage::Respawn>& R) { RespawnPreview = R; }

void UAcProgressHudSubsystem::NewsItem(FAcMmoClient& Mmo, const ac::net::ProgressMessage::News& N, const double Now, int64& Reached)
{
	const ac::net::ProgressMessage* P = Mmo.Progress();
	using K = ac::net::ProgressMessage::NewsKind;
	const FString Xp = N.xp > 0 ? FString::Printf(TEXT("+%lld XP"), (long long)std::llround(N.xp)) : FString();
	switch (N.kind)
	{
	case K::level:
		Reached = FMath::Max<int64>(Reached, N.value);
		break;
	case K::blueprint:
	{
		const std::optional<ac::StructureKind> Kind = ac::parse<ac::StructureKind>(N.id);
		const FString Name = Kind ? Upper(ac::Simulation::title(*Kind)) : Text(N.id).ToUpper();
		const int64 Level = P ? ac::Progression::level(P->xp) : 1;
		const FString Sub = N.value > Level ? FString::Printf(TEXT("Opens at level %lld"), (long long)N.value) : FString(TEXT("Ready: B builds it"));
		Hud->Toast(FString::Printf(TEXT("BLUEPRINT FOUND: %s"), *Name), Xp.IsEmpty() ? Sub : Sub + TEXT("  ·  ") + Xp,
			FAcHudStyle::LevelGold(), Now);
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Blueprint);
		break;
	}
	case K::chip:
	{
		const ac::net::WorldMessage::Region* Here = Mmo.Here();
		const FString Where = Here ? FString::Printf(TEXT("A site marked in %s"), *Text(Here->name)) : FString(TEXT("A site marked on the map"));
		Hud->Toast(TEXT("SURVEY CHIP"), Xp.IsEmpty() ? Where : Where + TEXT("  ·  ") + Xp, FAcHudStyle::Cyan(), Now);
		break;
	}
	case K::region:
	{
		if (Xp.IsEmpty()) break;
		FString Name = Text(N.id);
		if (const ac::net::WorldMessage* W = Mmo.World())
		{
			for (const ac::net::WorldMessage::Region& R : W->regions)
			{
				if (R.id == N.id) Name = Text(R.name);
			}
		}
		Hud->Toast(TEXT("REGION DISCOVERED"), Name + TEXT("  ·  ") + Xp, FAcHudStyle::Ice(), Now);
		break;
	}
	case K::pad:
		if (!Xp.IsEmpty()) Hud->Toast(TEXT("FIRST LANDING"), Xp, FAcHudStyle::Ice(), Now);
		break;
	case K::feat:
		if (!Xp.IsEmpty()) Hud->Toast(TEXT("FEAT"), Xp, FAcHudStyle::Amber(), Now);
		break;
	case K::reset:
		Hud->Toast(TEXT("SKILLS RESET"), FString::Printf(TEXT("%lld points to spend  ·  K"), (long long)N.value), FAcHudStyle::Energy(), Now);
		break;
	case K::site:
		// A site opened: its blueprint copy comes as its own news (C2).
		break;
	case K::contract:
	{
		// A KSTR contract done: its title as the news tells it (U1), else
		// from the slots, or as last seen.
		FString Title = TEXT("CONTRACT");
		if (const FString* Was = ContractTitles.Find(uint32(FCString::Atoi64(*Text(N.id))))) Title = *Was;
		if (P && N.by.empty())
		{
			for (const ac::net::Contract& C : P->contracts)
			{
				if (std::to_string(C.id) == N.id) Title = Upper(C.title);
			}
		}
		FString Pay = Xp;
		if (N.value > 0) Pay += FString::Printf(TEXT("%s+%lld MH"), Pay.IsEmpty() ? TEXT("") : TEXT("  ·  "), (long long)N.value);
		if (!N.by.empty()) Title = Upper(N.by);
		Hud->Toast(FString::Printf(TEXT("CONTRACT DONE: %s"), *Title), Pay, FAcHudStyle::Srgb(0.45, 1, 0.55), Now);
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		break;
	}
	case K::made:
	case K::researched:
	{
		// P7: the Foundry made a module; the Lab opened a mark (the rig screen, J).
		const std::optional<ac::Module> M = ac::parse<ac::Module>(N.id);
		const FString Name = (M ? Upper(ac::Modules::title(*M)) : Text(N.id).ToUpper()) + TEXT(" ") + Upper(ac::Modules::markName(N.value));
		if (N.kind == K::made)
		{
			Hud->Toast(FString::Printf(TEXT("MODULE MADE: %s"), *Name), TEXT("Waiting in your stores  ·  J fits it at home"), FAcHudStyle::Amber(), Now);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Made);
		}
		else
		{
			Hud->Toast(FString::Printf(TEXT("RESEARCHED: %s"), *Name), TEXT("The Foundry can make it now  ·  J"), FAcHudStyle::Srgb(0.74, 0.56, 1), Now);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Researched);
		}
		break;
	}
	case K::built:
	{
		// R1: a building of the realm finished, by this pilot or another.
		const std::optional<ac::StructureKind> Kind = ac::parse<ac::StructureKind>(N.id);
		const FString Name = Kind ? Upper(ac::Simulation::title(*Kind)) : Text(N.id).ToUpper();
		const FString By = N.by.empty() ? FString(TEXT("a pilot")) : (P && N.by == P->callsign ? FString(TEXT("you")) : Text(N.by));
		const FString Where = N.value == 1 ? FString(TEXT("here")) : FString(TEXT("in the realm"));
		Hud->Toast(FString::Printf(TEXT("%s BUILT"), *Name), FString::Printf(TEXT("Built by %s  ·  %s  ·  every pilot's"), *By, *Where),
			FAcHudStyle::Srgb(0.45, 1, 0.55), Now);
		break;
	}
	case K::goal:
	{
		// L1: KSTR's next goal done: its title, the pay; the next one follows.
		FString Pay = Xp;
		if (N.value > 0) Pay += FString::Printf(TEXT("%s+%lld MH"), Pay.IsEmpty() ? TEXT("") : TEXT("  ·  "), (long long)N.value);
		Hud->Toast(FString::Printf(TEXT("GOAL DONE: %s"), *Upper(N.by)), Pay, FAcHudStyle::Srgb(0.45, 1, 0.55), Now);
		UE_LOG(LogAutocraft, Log, TEXT("progress: goal %s done: %s"), *Text(N.id), *Text(N.by));
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this))
		{
			Audio->Rules().News(FAcAudioRules::ENews::Paid);
			// A2: KSTR says so (the next goal's own line follows when it comes).
			Audio->SayRadio(TEXT("goal-done"));
		}
		break;
	}
	case K::mission:
	{
		// K2: a Citadel mission done: its title as the news tells it (U1)
		// or from the Journal, the pay.
		FString Title = N.by.empty() ? FString(TEXT("MISSION")) : Upper(N.by);
		FString Pay = Xp;
		if (P)
		{
			for (const ac::net::CitadelMission& M : P->journal)
			{
				if (std::to_string(M.id) != N.id) continue;
				if (N.by.empty()) Title = Upper(M.title);
				Pay = AcMissions::Reward(M);
			}
		}
		Hud->Toast(FString::Printf(TEXT("MISSION DONE: %s"), *Title), Pay + TEXT("  ·  L"), FAcHudStyle::LevelGold(), Now);
		UE_LOG(LogAutocraft, Log, TEXT("progress: mission %s done: %s"), *Text(N.id), *Title);
		if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		break;
	}
	case K::merc:
	{
		// H2: the mercenaries: what they delivered while the pilot was away,
		// one gone for its wage, one down.
		const FLinearColor Orange = FAcHudStyle::Srgb(1.0, 0.6, 0.22);
		const FString Name = Text(N.by).ToUpper();
		if (N.id == "away")
		{
			AwayOre = N.value;
			AwayAt = Now;
			Hud->Toast(TEXT("WHILE YOU WERE AWAY"), FString::Printf(TEXT("Mercs delivered %s ore"), *FText::AsNumber(N.value).ToString()), Orange,
				Now);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		}
		else if (N.id == "left")
		{
			Hud->Toast(FString::Printf(TEXT("MERCENARY LEFT: %s"), *Name),
				FString::Printf(TEXT("Unpaid at dawn: the wage is %lld MH  ·  H hires at the Citadel"), (long long)N.value), FAcHudStyle::Amber(), Now);
		}
		else if (N.id == "down")
		{
			Hud->Toast(FString::Printf(TEXT("MERCENARY DOWN: %s"), *Name),
				FString::Printf(TEXT("Back at the Citadel, at work again in %lld minutes"), (long long)(N.value / 60)), Orange, Now);
		}
		UE_LOG(LogAutocraft, Log, TEXT("progress: merc %s %s %lld"), *Text(N.id), *Text(N.by), (long long)N.value);
		break;
	}
	case K::upgraded:
	{
		// D3: a turret here took a mark ("gatling/air"), and who paid.
		const std::string Id = N.id;
		const size_t Slash = Id.find('/');
		const std::optional<ac::StructureKind> Kind = ac::parse<ac::StructureKind>(Id.substr(0, Slash));
		const std::optional<ac::DefenceStat> Stat =
			Slash == std::string::npos ? std::nullopt : ac::parse<ac::DefenceStat>(Id.substr(Slash + 1));
		const FString Name = Kind ? Upper(ac::Simulation::title(*Kind)) : FString(TEXT("TURRET"));
		const FString What = Stat ? Upper(ac::Defence::title(*Stat)) : FString(TEXT("A STAT"));
		const FString By = N.by.empty() ? FString(TEXT("a pilot")) : (P && N.by == P->callsign ? FString(TEXT("you")) : Text(N.by));
		Hud->Toast(FString::Printf(TEXT("%s UPGRADED"), *Name), FString::Printf(TEXT("%s mark %lld  ·  upgraded by %s"), *What,
			(long long)N.value, *By), FAcHudStyle::Srgb(1.0, 0.55, 0.25), Now);
		UE_LOG(LogAutocraft, Log, TEXT("progress: %s upgraded by %s, mark %lld"), *Text(N.id), *Text(N.by), (long long)N.value);
		break;
	}
	case K::canyon:
	{
		// Z2: a wave beaten (the fighter's share), the canyon held or fallen
		// (to every pilot: the banner slot), spared (she is down).
		const FString Zone = Text(N.by);
		const std::string Id = N.id;
		if (Id.rfind("wave/", 0) == 0)
		{
			// "wave/<n>/<MH>[/<blueprint>]".
			TArray<FString> Parts;
			Text(Id).ParseIntoArray(Parts, TEXT("/"));
			const int64 Wave = Parts.Num() > 1 ? FCString::Atoi64(*Parts[1]) : 0;
			const int64 Mh = Parts.Num() > 2 ? FCString::Atoi64(*Parts[2]) : 0;
			FString Sub = FString::Printf(TEXT("+%s ore  ·  +%s MH"), *FText::AsNumber(N.value).ToString(), *FText::AsNumber(Mh).ToString());
			if (!Xp.IsEmpty()) Sub += TEXT("  ·  ") + Xp;
			if (Parts.Num() > 3)
				if (const std::optional<ac::StructureKind> Kind = ac::parse<ac::StructureKind>(TCHAR_TO_UTF8(*Parts[3])))
					Sub += FString::Printf(TEXT("  ·  a %s blueprint"), *Text(ac::Simulation::title(*Kind)));
			Hud->Toast(FString::Printf(TEXT("YOUR SHARE: %s WAVE %lld"), *Zone.ToUpper(), (long long)Wave), Sub, FAcHudStyle::LevelGold(), Now + 0.01);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		}
		else if (Id == "held" || Id == "failed")
		{
			const bool bHeld = Id == "held";
			// A2: KSTR reads the news.
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this))
				if (const FString Zid = AcCanyon::IdOf(Mmo, Zone); !Zid.IsEmpty())
					Audio->SayRadio(FString::Printf(TEXT("canyon-%s-%s"), *Zid, bHeld ? TEXT("held") : TEXT("fallen")));
			const FString Title = bHeld ? FString::Printf(TEXT("%s HELD"), *Zone.ToUpper()) : FString::Printf(TEXT("%s HAS FALLEN"), *Zone.ToUpper());
			const FString Sub = bHeld ? (N.value > 0 ? FString::Printf(TEXT("KSTR: the canyon is the pilots' for a world day  ·  %lld fought the last wave"),
														   (long long)N.value)
													 : FString(TEXT("KSTR: the canyon is the pilots' for a world day")))
									  : FString::Printf(TEXT("KSTR: wave %lld broke through  ·  the Matriarch comes for its turrets"), (long long)N.value);
			if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
				L->ShowBanner(Title, Sub, FString(), false, bHeld ? FAcHudStyle::Srgb(0.45, 1, 0.55) : FAcHudStyle::Srgb(1.0, 0.32, 0.26));
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		}
		else if (Id == "spared")
		{
			Hud->Toast(FString::Printf(TEXT("%s BROKEN THROUGH"), *Zone.ToUpper()),
				FString::Printf(TEXT("Wave %lld got through; the Matriarch is down, the turrets stand  ·  wave 1 again"), (long long)N.value),
				FAcHudStyle::Amber(), Now);
		}
		UE_LOG(LogAutocraft, Log, TEXT("progress: canyon %s %s, %lld"), *Text(N.id), *Zone, (long long)N.value);
		break;
	}
	case K::matriarch:
	{
		// N3: the Matriarch down (to every pilot), a fighter's share
		// ("share/<MH>/<blueprint>", the ore in `value`), her return.
		const FLinearColor Violet = FAcHudStyle::Srgb(0.82, 0.42, 1.0);
		const std::string Id = N.id;
		if (Id == "down")
		{
			const FString By = N.by.empty() ? FString() : (P && N.by == P->callsign ? FString(TEXT("you")) : Text(N.by));
			const FString Who = N.value == 1 ? FString(TEXT("Felled by one pilot"))
											 : FString::Printf(TEXT("Felled by %lld pilots"), (long long)N.value);
			// The banner slot, not a toast (U1).
			if (UAcLevelingSubsystem* L = UAcLevelingSubsystem::Get(this))
				L->ShowBanner(TEXT("THE MATRIARCH IS DOWN"), By.IsEmpty() ? Who : Who + TEXT("  ·  the hardest blows: ") + By, FString(), false, Violet);
			if (UAcAudioDirector* Audio = UAcAudioDirector::Get(this)) Audio->Rules().News(FAcAudioRules::ENews::Paid);
		}
		else if (Id.rfind("share/", 0) == 0)
		{
			const size_t Slash = Id.find('/', 6);
			const int64 Mh = FCString::Atoi64(*Text(Id.substr(6, Slash == std::string::npos ? std::string::npos : Slash - 6)));
			const std::optional<ac::StructureKind> Kind =
				Slash == std::string::npos ? std::nullopt : ac::parse<ac::StructureKind>(Id.substr(Slash + 1));
			FString Sub = FString::Printf(TEXT("+%s ore  ·  +%s MH"), *FText::AsNumber(N.value).ToString(), *FText::AsNumber(Mh).ToString());
			if (Kind) Sub += FString::Printf(TEXT("  ·  a %s blueprint"), *Text(ac::Simulation::title(*Kind)));
			Hud->Toast(TEXT("YOUR SHARE OF THE MATRIARCH"), Sub, FAcHudStyle::LevelGold(), Now + 0.01);
		}
		else if (Id == "back")
		{
			FString Where = TEXT("her region");
			if (const ac::net::WorldMessage* W = Mmo.World())
				for (const ac::net::WorldMessage::Region& R : W->regions)
					if (R.nest) Where = Text(R.name);
			Hud->Toast(TEXT("THE MATRIARCH STIRS"), FString::Printf(TEXT("She is back in her nest in %s, and the nest grows again"), *Where), Violet,
				Now);
		}
		UE_LOG(LogAutocraft, Log, TEXT("progress: matriarch %s by %s, %lld"), *Text(N.id), *Text(N.by), (long long)N.value);
		break;
	}
	case K::frontier:
	{
		// P1: the frontier pilots: a KSTR radio line ("KSTR · VESPER"), a
		// deed ("Vesper built a Hab Dome in Rust Hollow"), one landed. In its
		// paint.
		FString Callsign = TEXT("FRONTIER");
		FLinearColor Ink = FAcHudStyle::Srgb(0.45, 1, 0.55);
		if (P)
			for (const ac::net::ProgressMessage::FrontierPilot& X : P->frontier)
				if (int64(X.id) == N.value)
				{
					Callsign = Text(X.callsign).ToUpper();
					Ink = FAcHudStyle::TeamPaint(X.paint);
				}
		const std::string Id = N.id;
		if (Id == "radio") Hud->Toast(FString::Printf(TEXT("KSTR  ·  %s"), *Callsign), Text(N.by), Ink, Now);
		else if (Id == "deed") Hud->Toast(TEXT("ON THE FRONTIER"), Text(N.by), Ink, Now);
		else if (Id == "come") Hud->Toast(FString::Printf(TEXT("%s LANDED"), *Callsign), Text(N.by), Ink, Now);
		UE_LOG(LogAutocraft, Log, TEXT("progress: frontier %s: %s"), *Text(N.id), *Text(N.by));
		break;
	}
	default:
		break;
	}
}
