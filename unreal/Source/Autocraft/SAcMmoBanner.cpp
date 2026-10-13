#include "SAcMmoBanner.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "Rendering/DrawElements.h"

namespace
{
	constexpr float Pad = 10.0f;
	constexpr float LineH = 22.0f;
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
	if (Lines.Num() == 0) return Layer;
	float W = 0;
	for (const FAcHudText& L : Lines) W = FMath::Max(W, L.Measure().X);
	const FVector2f Size(W + 2 * Pad + 4, Lines.Num() * LineH + 2 * Pad - 4);
	const FVector2f View(Geometry.GetLocalSize());
	const FVector2f P((View.X - Size.X) * 0.5f, Top);
	const FLinearColor Edge = (bStatusError && !Status.IsEmpty() ? Red : Cyan).CopyWithNewOpacity(0.8f);
	PaintRect(Out, Layer, Geometry, P, Size, FAcHudStyle::Srgb(0.02, 0.05, 0.07, 0.9));
	PaintRect(Out, Layer + 1, Geometry, P, FVector2f(Size.X, 1), Edge);
	PaintRect(Out, Layer + 1, Geometry, P + FVector2f(0, Size.Y - 1), FVector2f(Size.X, 1), Edge);
	PaintRect(Out, Layer + 1, Geometry, P + FVector2f(2, 3), FVector2f(2, Size.Y - 6), Edge);
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		Lines[I].Paint(Out, Layer + 2, Geometry, P + FVector2f(Pad + 4, Pad + I * LineH));
	}
	return Layer + 2;
}
