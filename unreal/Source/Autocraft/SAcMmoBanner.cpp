#include "SAcMmoBanner.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "Rendering/DrawElements.h"

namespace
{
	constexpr float Pad = 10.0f;
	constexpr float LineH = 22.0f;
	constexpr float SmallH = 16.0f;
	constexpr float Top = 64.0f;

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

void SAcMmoBanner::SetStats(const FString& Text)
{
	if (Text == Stats) return;
	Stats = Text;
	Invalidate(EInvalidateWidgetReason::Paint);
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
	TArray<FAcHudText> Lines;
	const FLinearColor Cyan = FAcHudStyle::Srgb(0.55, 0.9, 1.0);
	const FLinearColor Red = FAcHudStyle::Srgb(1.0, 0.4, 0.35);
	const FLinearColor Amber = FAcHudStyle::Srgb(1.0, 0.8, 0.35);
	// The place first, small (in the small lines' height).
	const int32 Small0 = Place.IsEmpty() ? 0 : 1;
	if (!Place.IsEmpty())
	{
		Lines.Add(FAcHudText(Place, FAcHudStyle::Font(13), bPlaceFlying ? Amber : FAcHudStyle::Srgb(0.8, 0.93, 1.0), EAcHAlign::Left, EAcVAlign::Top));
	}
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
	if (!Stats.IsEmpty()) Lines.Add(FAcHudText(Stats, FAcHudStyle::Font(12), FAcHudStyle::Srgb(0.6, 0.7, 0.75), EAcHAlign::Left, EAcVAlign::Top));
	if (Lines.Num() == 0) return Layer;
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
