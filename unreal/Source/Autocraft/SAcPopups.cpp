#include "SAcPopups.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "Rendering/DrawElements.h"

void SAcPopups::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAcPopups::Add(const int64 Amount, const FVector2f At, const double Now)
{
	if (AddedThisFrame >= MaxPerFrame) return;
	++AddedThisFrame;
	Live.Add({Amount, At, Now});
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcPopups::SetTags(TArray<FTag> InTags)
{
	if (InTags.IsEmpty() && TagList.IsEmpty()) return;
	TagList = MoveTemp(InTags);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcPopups::Update(const double Now)
{
	Clock = Now;
	AddedThisFrame = 0;
	Live.RemoveAll([Now](const FPopup& P) { return Now - P.Born >= Life; });
	if (!Live.IsEmpty()) Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SAcPopups::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	// The mercenaries' tags: a dark chip, the name in their orange.
	for (const FTag& Tag : TagList)
	{
		FAcHudText T(Tag.Text, FAcHudStyle::Font(12, EAcFontWeight::Bold), FAcHudStyle::Srgb(1.0, 0.6, 0.22), EAcHAlign::Center,
			EAcVAlign::Center);
		T.Accent = FLinearColor::Transparent;
		T.Kern = 1.2f;
		const FVector2f Size = T.Measure() + FVector2f(14, 6);
		const FVector2f Min = Tag.At - Size * 0.5f;
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(FVector2D(Min))),
			FAcHudStyle::White(), ESlateDrawEffect::None, FLinearColor(0.02f, 0.02f, 0.03f, 0.62f));
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(FVector2D(Size.X, 1.5f), FSlateLayoutTransform(FVector2D(Min.X, Min.Y + Size.Y - 1.5f))),
			FAcHudStyle::White(), ESlateDrawEffect::None, FAcHudStyle::Srgb(0.92, 0.4, 0.06));
		T.Paint(Out, Layer + 1, Geometry, Tag.At);
	}
	const FSlateFontInfo Font = FAcHudStyle::Font(20);
	for (const FPopup& P : Live)
	{
		const double Age = FMath::Max(0.0, Clock - P.Born);
		const float Alpha = Age < FadeAfter ? 1.0f : (float)FMath::Clamp(1.0 - (Age - FadeAfter) / (Life - FadeAfter), 0.0, 1.0);
		const float Y = P.At.Y - Rise * (float)FMath::Min(Age / Life, 1.0);
		FAcHudText T(FString::Printf(TEXT("+%lld"), (long long)P.Amount), Font, FAcHudStyle::Deposit(), EAcHAlign::Center,
			EAcVAlign::Baseline);
		T.Accent = FLinearColor::Transparent;
		T.ShadowOffset = FVector2f(1.5f, 1.5f);
		T.ShadowColor = FLinearColor(0, 0, 0, 0.7f);
		T.Paint(Out, Layer, Geometry, FVector2f(P.At.X, Y), Alpha);
	}
	return Layer + 2;
}
