#include "SAcJournalScreen.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

namespace
{
	// The panel, in the HUD root's points (as SAcCitadelScreen).
	constexpr float PanelW = 1100;
	constexpr float Pad = 26;
	constexpr float HeaderH = 92;
	constexpr float Gap = 16;
	constexpr float ListW = 300;
	constexpr float BodyH = 440;
	constexpr float FootH = 56;
	constexpr float EntryH = 66;
	constexpr float SideW = 270;
	constexpr float MapH = 180;
	// L1: the next goal's band under the header.
	constexpr float NextH = 92;

	enum ERow : int32
	{
		RowEntry = 1,
		RowPlay = 2,
		RowAbandon = 3,
		RowClose = 4,
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

	void Ring(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f At, float R, const FLinearColor& C, float W)
	{
		TArray<FVector2f> P;
		constexpr int32 N = 28;
		for (int32 I = 0; I <= N; ++I)
		{
			const float A = 2 * PI * I / N;
			P.Add(At + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
		}
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

	/// `S` in lines no wider than `Width` at `Size`, at most `Max` (the last
	/// one cut with an ellipsis).
	TArray<FString> Wrapped(const FString& S, float Size, float Width, int32 Max, EAcFontWeight Weight = EAcFontWeight::Medium)
	{
		auto W = [&](const FString& X) { return FAcHudText(X, FAcHudStyle::Font(Size, Weight), FLinearColor::White).Measure().X; };
		TArray<FString> Words;
		S.ParseIntoArray(Words, TEXT(" "), true);
		TArray<FString> Out;
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Longer = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (Line.IsEmpty() || W(Longer) <= Width) Line = Longer;
			else
			{
				Out.Add(Line);
				Line = Word;
			}
		}
		if (!Line.IsEmpty()) Out.Add(Line);
		if (Out.Num() <= Max) return Out;
		FString Last = Out[Max - 1];
		while (!Last.IsEmpty() && W(Last + TEXT("…")) > Width) Last.LeftChopInline(1);
		Out.SetNum(Max - 1);
		Out.Add(Last + TEXT("…"));
		return Out;
	}

	FLinearColor Dim() { return FAcHudStyle::Srgb(0.42, 0.5, 0.57); }
	FLinearColor Faint() { return FAcHudStyle::Srgb(0.26, 0.32, 0.38); }
	FLinearColor Steel() { return FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8); }
	FLinearColor Refused() { return FAcHudStyle::Srgb(1.0, 0.55, 0.42); }
	FLinearColor Ready() { return FAcHudStyle::Srgb(0.45, 0.95, 0.62); }
	/// The Journal's leather-and-brass: the Citadel's gold.
	FLinearColor Brass() { return FAcHudStyle::Srgb(1.0, 0.82, 0.42); }
	FLinearColor TierColor(int32 Tier)
	{
		return Tier == 0 ? FAcHudStyle::Srgb(0.45, 0.95, 0.62) : Tier == 1 ? FAcHudStyle::Srgb(1.0, 0.72, 0.3) : FAcHudStyle::Srgb(1.0, 0.36, 0.3);
	}
	const TCHAR* TierName(int32 Tier) { return Tier == 0 ? TEXT("EASY") : Tier == 1 ? TEXT("HARD") : TEXT("PERILOUS"); }
}

void SAcJournalScreen::Construct(const FArguments& InArgs)
{
	OnPlayDelegate = InArgs._OnPlay;
	OnAbandonDelegate = InArgs._OnAbandon;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcJournalScreen::Animate));
}

SAcJournalScreen::~SAcJournalScreen() = default;

double SAcJournalScreen::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcJournalScreen::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

void SAcJournalScreen::SetInfo(const FAcJournalInfo& In)
{
	if (In == Info) return;
	Info = In;
	// Keep the picked mission on its id.
	const int32 Same = Info.Entries.IndexOfByPredicate([&](const FAcJournalEntry& E) { return E.Id == PickedId; });
	SetPicked(Same != INDEX_NONE ? Same : FMath::Clamp(Pick, 0, FMath::Max(0, Info.Entries.Num() - 1)));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcJournalScreen::SetPicked(const int32 Index)
{
	const int32 Was = Pick;
	Pick = Info.Entries.IsEmpty() ? 0 : FMath::Clamp(Index, 0, Info.Entries.Num() - 1);
	const uint32 Id = Info.Entries.IsValidIndex(Pick) ? Info.Entries[Pick].Id : 0;
	if (Was != Pick || Id != PickedId) bArmed = false;
	PickedId = Id;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcJournalScreen::SetRefusal(const FString& Why)
{
	Refusal = Why;
	if (!Why.IsEmpty()) Note.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcJournalScreen::SetNote(const FString& InNote)
{
	Note = InNote;
	Refusal.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcJournalScreen::Play()
{
	if (!Info.Entries.IsValidIndex(Pick)) return;
	const FAcJournalEntry& E = Info.Entries[Pick];
	if (E.Narration.IsEmpty())
	{
		SetRefusal(TEXT("No narration for this one"));
		return;
	}
	bArmed = false;
	OnPlayDelegate.ExecuteIfBound(Pick);
}

void SAcJournalScreen::Abandon()
{
	if (!Info.Entries.IsValidIndex(Pick)) return;
	const FAcJournalEntry& E = Info.Entries[Pick];
	if (E.bDone)
	{
		SetRefusal(TEXT("That mission is done already: it leaves the Journal at dawn"));
		return;
	}
	if (!bArmed)
	{
		bArmed = true;
		SetNote(FString::Printf(TEXT("Abandon %s? X again drops it (nothing is paid back); any other key keeps it."), *E.Title));
		return;
	}
	bArmed = false;
	SetNote(FString::Printf(TEXT("Dropping %s…"), *E.Title));
	OnAbandonDelegate.ExecuteIfBound(Pick);
}

void SAcJournalScreen::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcJournalScreen::Key(const FKey& InKey)
{
	if (InKey == EKeys::X)
	{
		Abandon();
		return true;
	}
	if (bArmed)
	{
		bArmed = false;
		SetNote(TEXT("Kept."));
	}
	if (InKey == EKeys::Up || InKey == EKeys::W)
	{
		SetPicked(Pick - 1);
		return true;
	}
	if (InKey == EKeys::Down || InKey == EKeys::S || InKey == EKeys::Tab)
	{
		SetPicked(InKey == EKeys::Tab && Pick + 1 >= Info.Entries.Num() ? 0 : Pick + 1);
		return true;
	}
	if (InKey == EKeys::P || InKey == EKeys::Enter || InKey == EKeys::SpaceBar)
	{
		Play();
		return true;
	}
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six};
	for (int32 I = 0; I < 6; ++I)
	{
		if (InKey == Digits[I] && Info.Entries.IsValidIndex(I))
		{
			SetPicked(I);
			return true;
		}
	}
	return false;
}

// MARK: - Layout

void SAcJournalScreen::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	auto Add = [&](const FBox2f& B, int32 RowId, int32 Index) {
		FTarget T;
		T.Box = B;
		T.Row = RowId;
		T.Index = Index;
		Targets.Add(T);
	};
	Add(FBox2f(FVector2f(PanelW - Pad - 40, 26), FVector2f(PanelW - Pad, 58)), RowClose, 0);
	const float Top = HeaderH + 16 + (Info.bNext ? NextH + 12 : 0);
	ListBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + ListW, Top + BodyH));
	PageBox = FBox2f(FVector2f(Pad + ListW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	for (int32 I = 0; I < Info.Entries.Num(); ++I)
	{
		const float Y = ListBox.Min.Y + 42 + I * (EntryH + 6);
		if (Y + EntryH > ListBox.Max.Y) break;
		Add(FBox2f(FVector2f(ListBox.Min.X + 10, Y), FVector2f(ListBox.Max.X - 10, Y + EntryH)), RowEntry, I);
	}
	if (Info.Entries.IsValidIndex(Pick))
	{
		const float X = PageBox.Max.X - 18 - SideW;
		Add(FBox2f(FVector2f(X, PageBox.Min.Y + 18), FVector2f(PageBox.Max.X - 18, PageBox.Min.Y + 54)), RowPlay, Pick);
		if (!Info.Entries[Pick].bDone)
		{
			Add(FBox2f(FVector2f(X, PageBox.Max.Y - 16 - 34), FVector2f(PageBox.Max.X - 18, PageBox.Max.Y - 16)), RowAbandon, Pick);
		}
	}
	FootBox = FBox2f(FVector2f(Pad, ListBox.Max.Y + 16), FVector2f(PanelW - Pad, ListBox.Max.Y + 16 + FootH));
	PanelSize = FVector2f(PanelW, FootBox.Max.Y + 18);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcJournalScreen::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

void SAcJournalScreen::Press(const FTarget& T)
{
	switch (T.Row)
	{
	case RowEntry: SetPicked(T.Index); break;
	case RowPlay: Play(); break;
	case RowAbandon: Abandon(); break;
	case RowClose: Close(); break;
	default: break;
	}
}

FReply SAcJournalScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Pressed = TargetAt(Local);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcJournalScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcJournalScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
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

FReply SAcJournalScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	Hovered = TargetAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcJournalScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcJournalScreen::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcJournalScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	const float In = FMath::Clamp(float(Clock() - OpenedAt) / 0.18f, 0.f, 1.f);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.62f * In));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.02, 0.036, 0.062, 0.97));
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)), WithAlpha(Brass(), 0.03f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), WithAlpha(Brass(), 0.28f), 1, 14);
	const int32 L = Layer + 3;
	PaintHeader(Out, L, P);
	PaintNext(Out, L, P);
	PaintList(Out, L, P);
	PaintPage(Out, L, P);
	PaintFoot(Out, L, P);
	return L + 24;
}

void SAcJournalScreen::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FTarget& T, const FString& Text,
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
	Label(Text, 15, Ink, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0.8f).Paint(Out, L + 3, P, T.Box.GetCenter());
}

void SAcJournalScreen::PaintHeader(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	Label(TEXT("THE PROSPECTOR'S"), 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 24));
	FAcHudText Title = Label(TEXT("JOURNAL"), 34, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.5f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	Title.Paint(Out, L, P, FVector2f(Pad + 2, 44));
	int32 Taken = 0;
	for (const FAcJournalEntry& E : Info.Entries) Taken += E.bDone ? 0 : 1;
	Label(FString::Printf(TEXT("%d / %d missions · a Citadel's board (H)"), Taken, Info.Most), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::SemiBold, 0.4f)
		.Paint(Out, L, P, FVector2f(Pad + 196, 58));
	// The canyon zones' games, right of it, left of ESC (Z2).
	for (int32 K = 0; K < FMath::Min(2, Info.Canyons.Num()); ++K)
	{
		const FLinearColor Ink = Info.bCanyonAlert ? FAcHudStyle::Srgb(1.0, 0.32, 0.26) : FAcHudStyle::Srgb(1.0, 0.6, 0.22);
		Label(Info.Canyons[K], 13, Ink, EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f)
			.Paint(Out, L, P, FVector2f(PanelW - Pad - 56, 30 + K * 18));
	}
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowClose) PaintButton(Out, L, P, T, TEXT("ESC"), false, Brass());
	}
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), WithAlpha(Brass(), 0.2f));
}

void SAcJournalScreen::PaintNext(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	if (!Info.bNext) return;
	const FBox2f B(FVector2f(Pad, HeaderH + 16), FVector2f(PanelW - Pad, HeaderH + 16 + NextH));
	const FLinearColor Gold = FAcHudStyle::LevelGold();
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.09, 0.07, 0.03, 0.92));
	CutFrame(Out, L + 1, P, B, WithAlpha(Gold, 0.75f), 1.5f, 10);
	Box(Out, L + 1, P, FBox2f(B.Min, FVector2f(B.Min.X + 4, B.Max.Y)), Gold);
	const float X = B.Min.X + 20, Right = B.Max.X - 18;
	Label(TEXT("NEXT  ·  KSTR"), 11, Gold, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 10));
	if (!Info.NextPay.IsEmpty())
	{
		Label(Info.NextPay, 14, Brass(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(Right, B.Min.Y + 10));
	}
	// The title, what to do; why it matters; the radio's line.
	const float TitleW = FMath::Min(380.f, Right - X);
	const TArray<FString> Title = Wrapped(Info.NextTitle, 22, TitleW, 1, EAcFontWeight::Bold);
	Label(Title.IsEmpty() ? FString() : Title[0], 22, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.2f)
		.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 28));
	const float HX = X + TitleW + 16;
	const TArray<FString> Hint = Wrapped(Info.NextHint, 14, Right - HX, 1, EAcFontWeight::SemiBold);
	Label(Hint.IsEmpty() ? FString() : Hint[0], 14, Gold, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f)
		.Paint(Out, L + 2, P, FVector2f(HX, B.Min.Y + 34));
	const FString Why = Info.NextWhere.IsEmpty() ? Info.NextWhy : Info.NextWhy + TEXT("  ") + Info.NextWhere;
	const TArray<FString> WhyLines = Wrapped(Why, 14, Right - X, 1, EAcFontWeight::Medium);
	Label(WhyLines.IsEmpty() ? FString() : WhyLines[0], 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 56));
	const TArray<FString> Pitch = Wrapped(TEXT("\u201C") + Info.NextPitch + TEXT("\u201D"), 13, Right - X, 1, EAcFontWeight::Medium);
	Label(Pitch.IsEmpty() ? FString() : Pitch[0], 13, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 74));
}

void SAcJournalScreen::PaintList(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f B = ListBox;
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
	CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
	Label(TEXT("MISSIONS"), 13, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	if (Info.Entries.IsEmpty())
	{
		const TArray<FString> Lines = Wrapped(Info.bKnown ? TEXT("None yet. Sign up at a Citadel: H, then the MISSIONS tab.")
														  : TEXT("Waiting for the server…"),
			14, ListW - 40, 4);
		for (int32 K = 0; K < Lines.Num(); ++K)
		{
			Label(Lines[K], 14, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, B.Min + FVector2f(18, 52 + K * 19));
		}
		return;
	}
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowEntry) continue;
		const FAcJournalEntry& E = Info.Entries[T.Index];
		const bool bOn = T.Index == Pick;
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == &T;
		const FLinearColor C = TierColor(E.Tier);
		Box(Out, L + 1, P, T.Box, bOn ? WithAlpha(C, 0.12f) : bHover ? WithAlpha(Steel(), 0.18f) : WithAlpha(Steel(), 0.06f));
		if (bOn) CutFrame(Out, L + 2, P, T.Box, WithAlpha(C, 0.8f), 1.5f, 6);
		Box(Out, L + 2, P, FBox2f(T.Box.Min, FVector2f(T.Box.Min.X + 4, T.Box.Max.Y)), E.bDone ? Ready() : C);
		Label(FString::Printf(TEXT("%d"), T.Index + 1), 10, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 3, P, FVector2f(T.Box.Max.X - 8, T.Box.Min.Y + 6));
		const TArray<FString> Title = Wrapped(E.Title, 16, T.Box.GetSize().X - 40, 1, EAcFontWeight::Bold);
		Label(Title.IsEmpty() ? FString() : Title[0], 16, E.bDone ? Dim() : FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold)
			.Paint(Out, L + 3, P, T.Box.Min + FVector2f(14, 8));
		Label(FString::Printf(TEXT("%s · %s"), *E.Kind, TierName(E.Tier)), 10, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
			.Paint(Out, L + 3, P, T.Box.Min + FVector2f(14, 30));
		Label(E.Progress, 12, E.bDone ? Ready() : FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 3, P, FVector2f(T.Box.Max.X - 10, T.Box.Min.Y + 29));
		const FBox2f Bar(FVector2f(T.Box.Min.X + 14, T.Box.Max.Y - 12), FVector2f(T.Box.Max.X - 10, T.Box.Max.Y - 8));
		Box(Out, L + 3, P, Bar, WithAlpha(Steel(), 0.4f));
		Box(Out, L + 4, P, FBox2f(Bar.Min, FVector2f(FMath::Lerp(Bar.Min.X, Bar.Max.X, E.Share), Bar.Max.Y)), E.bDone ? Ready() : C);
	}
}

void SAcJournalScreen::PaintPage(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f B = PageBox;
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
	CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
	if (!Info.Entries.IsValidIndex(Pick))
	{
		Label(TEXT("Missions you sign up for at the Citadel show here: the briefing, what to do, the map, the narration."), 14, Dim(),
			EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, B.GetCenter());
		return;
	}
	const FAcJournalEntry& E = Info.Entries[Pick];
	const FLinearColor C = TierColor(E.Tier);
	const float X = B.Min.X + 22;
	const float TextW = B.GetSize().X - SideW - 3 * 18 - 4;
	// Its kind, tier and place; its title.
	Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 10, B.Min.Y + 14), FVector2f(B.Min.X + 14, B.Min.Y + 30)), C);
	Label(FString::Printf(TEXT("%s · %s%s"), *E.Kind, TierName(E.Tier), E.Where.IsEmpty() ? TEXT("") : *(TEXT(" · ") + E.Where)), 13, C,
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.4f)
		.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 14));
	FAcHudText Title = Label(E.Title, 26, E.bDone ? Ready() : FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.2f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.5f);
	Title.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 36));
	// The briefing, the officer's words: what the narration reads.
	auto Head = [&](const TCHAR* What, float Y) {
		Label(What, 11, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, FVector2f(X, Y));
	};
	Head(TEXT("BRIEFING"), B.Min.Y + 80);
	const TArray<FString> Brief = Wrapped(E.Briefing, 14, TextW, 6);
	for (int32 K = 0; K < Brief.Num(); ++K)
	{
		Label(Brief[K], 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 98 + K * 19));
	}
	Head(TEXT("WHAT TO DO"), B.Min.Y + 220);
	const TArray<FString> Do = Wrapped(E.Instructions, 14, TextW, 2, EAcFontWeight::SemiBold);
	for (int32 K = 0; K < Do.Num(); ++K)
	{
		Label(Do[K], 14, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 238 + K * 19));
	}
	Head(TEXT("PROGRESS"), B.Min.Y + 290);
	const FBox2f Bar(FVector2f(X, B.Min.Y + 310), FVector2f(X + TextW, B.Min.Y + 318));
	Box(Out, L + 2, P, Bar, WithAlpha(Steel(), 0.4f));
	Box(Out, L + 3, P, FBox2f(Bar.Min, FVector2f(FMath::Lerp(Bar.Min.X, Bar.Max.X, E.Share), Bar.Max.Y)), E.bDone ? Ready() : C);
	Label(E.bDone ? FString(TEXT("DONE · paid")) : E.Progress, 14, E.bDone ? Ready() : FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::SemiBold)
		.Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 324));
	Head(TEXT("REWARD"), B.Min.Y + 358);
	const TArray<FString> Pay = Wrapped(E.Reward, 14, TextW, 2, EAcFontWeight::SemiBold);
	for (int32 K = 0; K < Pay.Num(); ++K)
	{
		Label(Pay[K], 14, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(X, B.Min.Y + 376 + K * 19));
	}

	// The side: the narration, the map, abandoning.
	const float SX = B.Max.X - 18 - SideW;
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowPlay)
		{
			const bool bPlaying = !Info.Playing.IsEmpty() && Info.Playing == E.Narration;
			const FString Text = bPlaying ? FString(TEXT("PLAYING THE BRIEFING…")) : E.Seconds > 0
				? FString::Printf(TEXT("PLAY THE BRIEFING · %d s"), int32(FMath::RoundToDouble(E.Seconds)))
				: FString(TEXT("PLAY THE BRIEFING"));
			PaintButton(Out, L, P, T, Text, !E.Narration.IsEmpty(), Brass());
		}
		if (T.Row == RowAbandon) PaintButton(Out, L, P, T, bArmed ? TEXT("X AGAIN: ABANDON") : TEXT("ABANDON"), bArmed, Refused());
	}
	Label(TEXT("WHERE"), 11, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, FVector2f(SX, B.Min.Y + 66));
	const FBox2f MapBox(FVector2f(SX, B.Min.Y + 84), FVector2f(SX + SideW, B.Min.Y + 84 + MapH));
	PaintMap(Out, L + 1, P, E, MapBox);
	const TArray<FString> Bear = Wrapped(E.Bearing, 13, SideW, 2, EAcFontWeight::SemiBold);
	for (int32 K = 0; K < Bear.Num(); ++K)
	{
		Label(Bear[K], 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(SX, MapBox.Max.Y + 8 + K * 17));
	}
	if (E.bMarked)
	{
		Label(TEXT("The marker is on your minimap and compass."), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, FVector2f(SX, MapBox.Max.Y + 46));
	}
}

void SAcJournalScreen::PaintMap(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FAcJournalEntry& E, const FBox2f& B) const
{
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.015, 0.03, 0.05, 1));
	CutFrame(Out, L + 1, P, B, Steel(), 1, 8);
	if (Info.Columns <= 0 || Info.Rows <= 0) return;
	const FVector2f Cell((B.GetSize().X - 8) / Info.Columns, (B.GetSize().Y - 8) / Info.Rows);
	auto CellBox = [&](int32 Col, int32 Row) {
		const FVector2f Min = B.Min + FVector2f(4, 4) + FVector2f(Col * Cell.X, Row * Cell.Y);
		return FBox2f(Min + FVector2f(1, 1), Min + Cell - FVector2f(1, 1));
	};
	const FLinearColor C = TierColor(E.Tier);
	for (int32 R = 0; R < Info.Rows; ++R)
	{
		for (int32 Col = 0; Col < Info.Columns; ++Col)
		{
			const int32 I = R * Info.Columns + Col;
			if (!Info.Names.IsValidIndex(I) || Info.Names[I].IsEmpty()) continue;
			const FBox2f CB = CellBox(Col, R);
			const bool bTarget = E.bMarked && Col == E.Column && R == E.Row;
			Box(Out, L + 1, P, CB, bTarget ? WithAlpha(C, 0.16f) : FAcHudStyle::Srgb(0.05, 0.085, 0.12, 1));
			CutFrame(Out, L + 2, P, CB, bTarget ? C : WithAlpha(Steel(), 0.5f), bTarget ? 1.5f : 1.0f, 3);
			const TArray<FString> Name = Wrapped(Info.Names[I], 9, CB.GetSize().X - 6, 1, EAcFontWeight::SemiBold);
			Label(Name.IsEmpty() ? FString() : Name[0], 9, bTarget ? C : Faint(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
				.Paint(Out, L + 3, P, CB.Min + FVector2f(3, 2));
		}
	}
	// Where the pilot is.
	if (Info.bHere)
	{
		const FBox2f CB = CellBox(Info.HereColumn, Info.HereRow);
		const FVector2f At = CB.Min + CB.GetSize() * Info.HereAt;
		Box(Out, L + 4, P, FBox2f(At - FVector2f(3, 3), At + FVector2f(3, 3)), FAcHudStyle::Text());
		Ring(Out, L + 4, P, At, 6, WithAlpha(FAcHudStyle::Text(), 0.5f), 1);
	}
	if (!E.bMarked) return;
	const FBox2f CB = CellBox(E.Column, E.Row);
	if (E.bWholeRegion)
	{
		CutFrame(Out, L + 4, P, CB.ExpandBy(-3), C, 2, 3);
		return;
	}
	const FVector2f At = CB.Min + CB.GetSize() * E.At;
	const float Pulse = 0.6f + 0.4f * FMath::Sin(float(Clock() - OpenedAt) * 4.f);
	Ring(Out, L + 4, P, At, FMath::Max(5.f, E.Ring * CB.GetSize().X), WithAlpha(C, Pulse), 1.5f);
	const TArray<FVector2f> Diamond{At + FVector2f(0, -5), At + FVector2f(5, 0), At + FVector2f(0, 5), At + FVector2f(-5, 0), At + FVector2f(0, -5)};
	FSlateDrawElement::MakeLines(Out, L + 5, P.ToPaintGeometry(), Diamond, ESlateDrawEffect::None, C, true, 2.5f);
}

void SAcJournalScreen::PaintFoot(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f F = FootBox;
	Box(Out, L, P, FBox2f(FVector2f(Pad, F.Min.Y - 8), FVector2f(PanelW - Pad, F.Min.Y - 7)), WithAlpha(Brass(), 0.14f));
	FString State = TEXT("New offers on the Citadels' boards at every dawn; a done mission leaves the Journal then.");
	FLinearColor StateColor = FAcHudStyle::Soft();
	if (!Refusal.IsEmpty())
	{
		State = Refusal;
		StateColor = Refused();
	}
	else if (!Note.IsEmpty())
	{
		State = Note;
		StateColor = bArmed ? FAcHudStyle::Amber() : Ready();
	}
	Label(TEXT("JOURNAL"), 12, Brass(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 6));
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 26));
	const float KX = PanelW - Pad - 4;
	Label(TEXT("UP DOWN OR 1-3 PICK · P PLAYS · X ABANDONS"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 8));
	Label(TEXT("L OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 28));
}
