#include "SAcMmoBanner.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

namespace
{
	constexpr float Pad = 10.0f;
	constexpr float LineH = 22.0f;
	constexpr float SmallH = 16.0f;
	/// The status and the refusals: under the pilot's compass (its tape at
	/// 66-94 points down, a goal's mark to 120).
	constexpr float Top = 132.0f;
	/// The place's box: the view's top left, in from the view frame.
	constexpr float CornerX = 30.0f;
	constexpr float CornerY = 26.0f;

	void PaintRect(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f At, FVector2f Size, const FLinearColor& C)
	{
		if (Size.X <= 0 || Size.Y <= 0) return;
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(FVector2D(At))),
			FAcHudStyle::White(), ESlateDrawEffect::None, C);
	}
}

void SAcMmoBanner::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAcMmoBanner::SetStatus(const FString& Text, const bool bError)
{
	if (Text == Status && bError == bStatusError) return;
	Status = Text;
	bStatusError = bError;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMmoBanner::AddNote(const FString& Text, const double Now)
{
	// The same refusal again: shown longer, not twice.
	Notes.RemoveAll([&](const FNote& N) { return N.Text == Text; });
	Notes.Add({Text, Now + NoteSeconds});
	while (Notes.Num() > 3) Notes.RemoveAt(0);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMmoBanner::SetStats(const FString& Text, const bool bWarn)
{
	if (Text == Stats && bWarn == bStatsWarn) return;
	Stats = Text;
	bStatsWarn = bWarn;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMmoBanner::ShowTitle(const FString& Name, const FString& Style, const double Now)
{
	TitleName = Name;
	TitleStyle = Style;
	TitleSince = Now;
	Invalidate(EInvalidateWidgetReason::Paint);
	// Painted again every frame while it runs (Slate keeps a still widget's last paint).
	if (!TitleTimer.IsValid())
	{
		TitleTimer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcMmoBanner::AnimateTitle));
	}
}

EActiveTimerReturnType SAcMmoBanner::AnimateTitle(double CurrentTime, float DeltaTime)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	if (TitleShown(FPlatformTime::Seconds())) return EActiveTimerReturnType::Continue;
	TitleTimer.Reset();
	return EActiveTimerReturnType::Stop;
}

void SAcMmoBanner::PaintTitle(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& Geometry, const double Now) const
{
	if (!TitleShown(Now)) return;
	// 0.5 s in (rising a little), 2.5 s, 1 s out.
	const double T = Now - TitleSince;
	const float A = (float)(T < 0.5 ? T / 0.5 : T > 3.0 ? FMath::Max(0.0, 1 - (T - 3.0) / 1.0) : 1.0);
	const float Rise = (float)(T < 0.5 ? 8 * (1 - T / 0.5) : 0);
	const FVector2f View(Geometry.GetLocalSize());
	const float Y = View.Y * 0.27f + Rise;
	FAcHudText Name(TitleName.ToUpper(), FAcHudStyle::Font(46), FAcHudStyle::Srgb(0.93, 0.97, 1.0), EAcHAlign::Center, EAcVAlign::Baseline);
	Name.Kern = 4;
	Name.ShadowOffset = FVector2f(0, 2);
	Name.ShadowColor = FLinearColor(0, 0, 0, 0.75f);
	const FVector2f NameSize = Name.Measure();
	// A soft dark band behind, and amber rules either side of the style.
	const float BandW = NameSize.X + 140;
	PaintRect(Out, Layer, Geometry, FVector2f((View.X - BandW) / 2, Y - NameSize.Y - 6), FVector2f(BandW, NameSize.Y + 44),
		FAcHudStyle::Srgb(0.01, 0.03, 0.05, 0.55 * A));
	Name.Paint(Out, Layer + 1, Geometry, FVector2f(View.X / 2, Y), A);
	if (!TitleStyle.IsEmpty())
	{
		FAcHudText Style(TitleStyle.ToUpper(), FAcHudStyle::Font(17), FAcHudStyle::Srgb(1.0, 0.8, 0.35), EAcHAlign::Center, EAcVAlign::Baseline);
		Style.Kern = 3;
		Style.ShadowOffset = FVector2f(0, 1);
		Style.ShadowColor = FLinearColor(0, 0, 0, 0.75f);
		const float SW = Style.Measure().X;
		for (const float Side : {-1.f, 1.f})
		{
			PaintRect(Out, Layer + 1, Geometry, FVector2f(View.X / 2 + Side * (SW / 2 + 16) - (Side < 0 ? 40 : 0), Y + 23), FVector2f(40, 2),
				FAcHudStyle::Srgb(1.0, 0.8, 0.35, 0.8 * A));
		}
		Style.Paint(Out, Layer + 1, Geometry, FVector2f(View.X / 2, Y + 30), A);
	}
}

void SAcMmoBanner::SetPlace(const FString& Text, const bool bFlying)
{
	if (Text == Place && bFlying == bPlaceFlying) return;
	Place = Text;
	bPlaceFlying = bFlying;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMmoBanner::Update(const double Now)
{
	if (Notes.RemoveAll([&](const FNote& N) { return N.Until <= Now; }) > 0) Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SAcMmoBanner::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out,
	int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FLinearColor Cyan = FAcHudStyle::Srgb(0.55, 0.9, 1.0);
	const FLinearColor Red = FAcHudStyle::Srgb(1.0, 0.4, 0.35);
	const FLinearColor Amber = FAcHudStyle::Srgb(1.0, 0.8, 0.35);
	PaintTitle(Out, Layer, Geometry, FPlatformTime::Seconds());
	Layer += 2;
	// A box of lines at `P` (its left edge; centred across the view when unset).
	auto Box = [&](const TArray<FAcHudText>& Lines, const TArray<float>& Heights, const TOptional<FVector2f> At, const FLinearColor& Edge)
	{
		if (Lines.IsEmpty()) return;
		float W = 0, H = 0;
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			W = FMath::Max(W, Lines[I].Measure().X);
			H += Heights[I];
		}
		const FVector2f Size(W + 2 * Pad + 4, H + 2 * Pad - 4);
		const FVector2f View(Geometry.GetLocalSize());
		const FVector2f P = At ? *At : FVector2f((View.X - Size.X) * 0.5f, Top);
		PaintRect(Out, Layer, Geometry, P, Size, FAcHudStyle::Srgb(0.02, 0.05, 0.07, 0.9));
		PaintRect(Out, Layer + 1, Geometry, P, FVector2f(Size.X, 1), Edge);
		PaintRect(Out, Layer + 1, Geometry, P + FVector2f(0, Size.Y - 1), FVector2f(Size.X, 1), Edge);
		PaintRect(Out, Layer + 1, Geometry, P + FVector2f(2, 3), FVector2f(2, Size.Y - 6), Edge);
		float Y = Pad;
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			Lines[I].Paint(Out, Layer + 2, Geometry, P + FVector2f(Pad + 4, Y));
			Y += Heights[I];
		}
	};
	// The place and the connection: the top left.
	{
		TArray<FAcHudText> Lines;
		TArray<float> Heights;
		if (!Place.IsEmpty())
		{
			Lines.Add(FAcHudText(Place, FAcHudStyle::Font(14), bPlaceFlying ? Amber : FAcHudStyle::Srgb(0.8, 0.93, 1.0), EAcHAlign::Left, EAcVAlign::Top));
			Heights.Add(SmallH + 1);
		}
		if (!Stats.IsEmpty())
		{
			Lines.Add(FAcHudText(Stats, FAcHudStyle::Font(12), bStatsWarn ? Amber : FAcHudStyle::Srgb(0.6, 0.7, 0.75), EAcHAlign::Left, EAcVAlign::Top));
			Heights.Add(SmallH);
		}
		Box(Lines, Heights, FVector2f(CornerX, CornerY), Cyan.CopyWithNewOpacity(0.8f));
	}
	TArray<FAcHudText> Lines;
	const int32 Small0 = 0;
	if (!Status.IsEmpty())
	{
		TArray<FString> Parts;
		Status.ParseIntoArrayLines(Parts);
		for (const FString& P : Parts)
		{
			Lines.Add(FAcHudText(P, FAcHudStyle::Font(16), bStatusError ? Red : Cyan, EAcHAlign::Left, EAcVAlign::Top));
		}
	}
	for (const FNote& N : Notes) Lines.Add(FAcHudText(N.Text, FAcHudStyle::Font(16), Amber, EAcHAlign::Left, EAcVAlign::Top));
	const int32 Big = Lines.Num() - Small0;
	if (Lines.Num() == 0) return Layer + 2;
	float W = 0;
	for (const FAcHudText& L : Lines) W = FMath::Max(W, L.Measure().X);
	const float H = Small0 * SmallH + Big * LineH + (Lines.Num() > Small0 + Big ? SmallH : 0);
	const FVector2f Size(W + 2 * Pad + 4, H + 2 * Pad - 4);
	const FVector2f View(Geometry.GetLocalSize());
	const FVector2f P((View.X - Size.X) * 0.5f, Top);
	const FLinearColor Edge = (bStatusError && !Status.IsEmpty() ? Red : Cyan).CopyWithNewOpacity(0.8f);
	PaintRect(Out, Layer, Geometry, P, Size, FAcHudStyle::Srgb(0.02, 0.05, 0.07, 0.9));
	PaintRect(Out, Layer + 1, Geometry, P, FVector2f(Size.X, 1), Edge);
	PaintRect(Out, Layer + 1, Geometry, P + FVector2f(0, Size.Y - 1), FVector2f(Size.X, 1), Edge);
	PaintRect(Out, Layer + 1, Geometry, P + FVector2f(2, 3), FVector2f(2, Size.Y - 6), Edge);
	float Y = Pad;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		Lines[I].Paint(Out, Layer + 2, Geometry, P + FVector2f(Pad + 4, Y));
		Y += I < Small0 || I >= Small0 + Big ? SmallH : LineH;
	}
	return Layer + 2;
}
