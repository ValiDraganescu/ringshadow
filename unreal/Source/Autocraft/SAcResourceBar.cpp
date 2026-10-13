#include "SAcResourceBar.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"

void SAcResourceBar::Construct(const FArguments& InArgs)
{
	SetCanTick(false);
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAcResourceBar::Update(const int64 Ore, const int64 InHydrogen, const int64 InSupplyUsed, const int64 InSupplyCap,
	const double RealDelta)
{
	if (bFlashGains && ShownOre >= 0)
	{
		const double Now = FPlatformTime::Seconds();
		if (Ore > TargetOre)
		{
			GainOre = Ore - TargetOre;
			GainOreAt = Now;
		}
		if (InHydrogen > Hydrogen)
		{
			GainHydrogen = InHydrogen - Hydrogen;
			GainHydrogenAt = Now;
		}
	}
	TargetOre = Ore;
	Hydrogen = InHydrogen;
	SupplyUsed = InSupplyUsed;
	SupplyCap = InSupplyCap;
	if (ShownOre < 0)
	{
		ShownOre = Ore;
		Pending = 0.0;
	}
	else
	{
		// Swift steps once per HUD update at 60 Hz: the same here, whatever
		// the frame rate (at most 6 steps a frame).
		Pending = FMath::Min(Pending + RealDelta, 0.1);
		while (Pending >= 1.0 / 60.0 && ShownOre != TargetOre)
		{
			Pending -= 1.0 / 60.0;
			ShownOre = ShownOre + FMath::Max<int64>(1, (TargetOre - ShownOre) / 3);
			ShownOre = FMath::Min(ShownOre, TargetOre);
		}
		if (ShownOre == TargetOre) Pending = 0.0;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcResourceBar::SetShowSupply(const bool bShow)
{
	if (bShow == bShowSupply) return;
	bShowSupply = bShow;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SAcResourceBar::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	FAcHudStyle::PaintPlate(Out, Layer, Geometry, EAcPlate::Bar, FSlateRect(0, 0, Width, Height));

	// HUD.buildBar: icons centred at x 28, 136, 238; labels from x 44, 152,
	// 256, centred 1 point under the middle.
	const float Cy = Height / 2;
	auto Icon = [&](const FSlateBrush* Brush, float X)
	{
		FSlateDrawElement::MakeBox(Out, Layer + 1,
			Geometry.ToPaintGeometry(FVector2D(24, 24), FSlateLayoutTransform(FVector2D(X - 12, Cy - 12))), Brush);
	};
	Icon(FAcHudStyle::OreIcon(), 28);
	Icon(FAcHudStyle::HydrogenIcon(), 136);
	if (bShowSupply) Icon(FAcHudStyle::SupplyIcon(), 238);

	const FSlateFontInfo Font = FAcHudStyle::Font(24);
	auto Label = [&](const FString& Text, float X)
	{
		FAcHudText(Text, Font, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Center)
			.Paint(Out, Layer + 1, Geometry, FVector2f(X, Cy + 1));
	};
	// A gain: the plate's cell lit a moment, "+N" after the count (1.6 s).
	const double Now = FPlatformTime::Seconds();
	auto Flash = [&](const double At, const int64 Gain, const float X0, const float LabelX, const FString& Count)
	{
		const double T = Now - At;
		if (!bFlashGains || T < 0 || T > 1.6) return;
		const float A = (float)(T < 1.0 ? 1.0 : 1 - (T - 1.0) / 0.6);
		const FLinearColor Glow = FAcHudStyle::Srgb(0.45, 1, 0.55, 0.28 * A);
		FSlateDrawElement::MakeBox(Out, Layer + 1, Geometry.ToPaintGeometry(FVector2D(100, Height - 10), FSlateLayoutTransform(FVector2D(X0, 5))),
			FAcHudStyle::White(), ESlateDrawEffect::None, Glow);
		const float W = FAcHudText(Count, Font, FLinearColor::White).Measure().X;
		FAcHudText(FString::Printf(TEXT("+%lld"), (long long)Gain), FAcHudStyle::Font(15), FAcHudStyle::Srgb(0.55, 1, 0.6),
			EAcHAlign::Left, EAcVAlign::Center).Paint(Out, Layer + 2, Geometry, FVector2f(LabelX + W + 5, Cy + 1), A);
	};
	const FString OreCount = FString::Printf(TEXT("%lld"), (long long)FMath::Max<int64>(ShownOre, 0));
	const FString HydrogenCount = FString::Printf(TEXT("%lld"), (long long)Hydrogen);
	Flash(GainOreAt, GainOre, 10, 44, OreCount);
	Flash(GainHydrogenAt, GainHydrogen, 118, 152, HydrogenCount);
	Label(OreCount, 44);
	Label(HydrogenCount, 152);
	if (bShowSupply) Label(FString::Printf(TEXT("%lld/%lld"), (long long)SupplyUsed, (long long)SupplyCap), 256);
	return Layer + 3;
}
