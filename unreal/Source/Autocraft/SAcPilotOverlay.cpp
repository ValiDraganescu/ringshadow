#include "SAcPilotOverlay.h"

#include "AcConsolePaint.h"
#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcIcons.h"

#include "Framework/Application/SlateApplication.h"
#include "Layout/Clipping.h"
#include "Rendering/SlateRenderer.h"

#include <cmath>

/// Drawing in Swift points, y up from the view's bottom: `FAcConsolePaint`
/// for fills, rounded rectangles and labels, plus polylines with SpriteKit's
/// glow and filled polygons. Each call takes the next layer.
struct FAcPilotPen
{
	FSlateWindowElementList& Out;
	const FGeometry& Geometry;
	int32 Layer;
	double H;

	template <class F>
	void Paint(F&& Fn)
	{
		FAcConsolePaint P(Out, Geometry, Layer, H);
		Fn(P);
		Layer = P.NextLayer();
	}

	FVector2f Slate(FVector2D P) const { return FVector2f((float)P.X, (float)(H - P.Y)); }

	/// A polyline (`SKShapeNode` stroke) with `glowWidth` as a soft halo.
	void Lines(const TArray<FVector2D>& Points, const FLinearColor& Color, double Width, double Glow = 0, bool bRoundCaps = false)
	{
		if (Points.Num() < 2 || Color.A <= 0) return;
		TArray<FVector2f> S;
		S.Reserve(Points.Num());
		for (const FVector2D& Pt : Points) S.Add(Slate(Pt));
		auto Line = [&](const FLinearColor& C, double W)
		{
			FSlateDrawElement::MakeLines(Out, ++Layer, Geometry.ToPaintGeometry(), S, ESlateDrawEffect::None, C, true, (float)W);
		};
		if (Glow > 0)
		{
			Line(FAcConsolePaint::WithAlpha(Color, Color.A * 0.22), Width + 2 * Glow);
			Line(FAcConsolePaint::WithAlpha(Color, Color.A * 0.3), Width + Glow);
		}
		Line(Color, Width);
		if (bRoundCaps)
		{
			Disc(Points[0], Width / 2, Color);
			Disc(Points.Last(), Width / 2, Color);
		}
	}

	/// A convex polygon filled.
	void Poly(const TArray<FVector2D>& Points, const FLinearColor& Color)
	{
		if (Color.A <= 0) return;
		Poly(Points, TArray<FLinearColor>{Color});
	}

	/// A convex polygon, each corner its own colour (blended across); one
	/// colour fills it all.
	void Poly(const TArray<FVector2D>& Points, const TArray<FLinearColor>& Colors)
	{
		if (Points.Num() < 3 || Colors.IsEmpty() || !FSlateApplication::IsInitialized()) return;
		const FSlateRenderTransform& T = Geometry.GetAccumulatedRenderTransform();
		// The white brush sits in Slate's atlas: sample the middle of its own region.
		const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FAcHudStyle::White());
		const FSlateShaderResourceProxy* Proxy = Handle.GetResourceProxy();
		const FVector2f Uv = Proxy ? Proxy->StartUV + Proxy->SizeUV * 0.5f : FVector2f(0.5f, 0.5f);
		TArray<FSlateVertex> V;
		TArray<SlateIndex> I;
		for (int32 K = 0; K < Points.Num(); ++K)
		{
			const FColor C = Colors[FMath::Min(K, Colors.Num() - 1)].ToFColor(true);
			V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Slate(Points[K]), Uv, C));
		}
		for (int32 K = 1; K + 1 < V.Num(); ++K)
		{
			I.Add(0);
			I.Add((SlateIndex)K);
			I.Add((SlateIndex)(K + 1));
		}
		FSlateDrawElement::MakeCustomVerts(Out, ++Layer, Handle, V, I, nullptr, 0, 0);
	}

	static TArray<FVector2D> Circle(FVector2D C, double R, int32 N = 48)
	{
		TArray<FVector2D> P;
		for (int32 K = 0; K <= N; ++K)
		{
			const double A = 2 * PI * K / N;
			P.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
		}
		return P;
	}

	void Disc(FVector2D C, double R, const FLinearColor& Color)
	{
		TArray<FVector2D> P = Circle(C, R, 16);
		P.Pop();
		Poly(P, Color);
	}

	/// A label (SpriteKit-aligned) with `PilotOverlay.shadow` if asked.
	void Label(const FString& Text, double Size, const FLinearColor& Color, FVector2D At, EAcHAlign HA = EAcHAlign::Center,
		EAcVAlign VA = EAcVAlign::Baseline, bool bShadow = true, bool bNames = true, float Opacity = 1,
		TOptional<FLinearColor> Accent = {})
	{
		if (Text.IsEmpty() || Opacity <= 0) return;
		FAcHudText T(Text, FAcHudStyle::Font((float)Size), Color, HA, VA);
		T.Accent = bNames ? Accent.Get(FAcHudStyle::NameAccent()) : FLinearColor::Transparent;
		if (bShadow)
		{
			T.ShadowOffset = FVector2f(1.2f, 1.2f);
			T.ShadowColor = FLinearColor(0, 0, 0, 0.75f);
		}
		Paint([&](FAcConsolePaint& P) { P.Text(T, At, Opacity); });
	}

	void Fill(const FAcRect& R, const FLinearColor& C) { Paint([&](FAcConsolePaint& P) { P.Fill(R, C); }); }
	void FillRounded(const FAcRect& R, double Radius, const FLinearColor& C)
	{
		Paint([&](FAcConsolePaint& P) { P.FillRounded(R, Radius, C); });
	}
	void Stroke(const FAcRect& R, double Radius, const FLinearColor& C, double Width, double Glow = 0)
	{
		Paint([&](FAcConsolePaint& P) { P.Stroke(R, Radius, C, Width, Glow); });
	}
	/// `SKShapeNode(rect:cornerRadius:)` filled and stroked.
	void Plate(const FAcRect& R, double Radius, const FLinearColor& Fill, const FLinearColor& Stroke, double Width)
	{
		FillRounded(R, Radius, Fill);
		this->Stroke(R, Radius, Stroke, Width);
	}
	void Image(const FSlateBrush* Brush, const FAcRect& R, const FLinearColor& Tint = FLinearColor::White)
	{
		Paint([&](FAcConsolePaint& P) { P.Image(Brush, R, Tint); });
	}

	/// Clip what follows to `R` (points, y up) until `PopClip`.
	void PushClip(const FAcRect& R)
	{
		const FVector2f A = Geometry.LocalToAbsolute(Slate(FVector2D(R.MinX(), R.MaxY())));
		const FVector2f B = Geometry.LocalToAbsolute(Slate(FVector2D(R.MaxX(), R.MinY())));
		Out.PushClip(FSlateClippingZone(FSlateRect(A.X, A.Y, B.X, B.Y)));
	}
	void PopClip() { Out.PopClip(); }
};

namespace
{
	FLinearColor Srgb(double R, double G, double B, double A = 1) { return FAcHudStyle::Srgb(R, G, B, A); }
	FLinearColor White(double W, double A = 1) { return Srgb(W, W, W, A); }
	FLinearColor Alpha(const FLinearColor& C, double A) { return FAcConsolePaint::WithAlpha(C, A); }

	/// SpriteKit's `.easeOut` (a quadratic ease).
	double EaseOut(double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		return 1 - (1 - T) * (1 - T);
	}

	/// An action's fade: 1 until `Wait`, then down to 0 over `Fade`.
	double FadeAfter(double T, double Wait, double Fade) { return T < Wait ? 1 : FMath::Max(0.0, 1 - (T - Wait) / Fade); }

	/// Health bar colours: green over half, amber over a quarter, red.
	FLinearColor Health(double F, const FLinearColor& Red)
	{
		return F > 0.5 ? Srgb(0.3, 0.9, 0.4) : F > 0.25 ? SAcPilotOverlay::BlockedColor() : Red;
	}

	constexpr double HitWaitKill = 0.35, HitWait = 0.1, HitFadeKill = 0.35, HitFade = 0.15;
}

FLinearColor SAcPilotOverlay::LineColor() { return Srgb(0.45, 0.8, 1, 0.9); }
FLinearColor SAcPilotOverlay::ReadyColor() { return Srgb(0.45, 1, 0.55); }
FLinearColor SAcPilotOverlay::BlockedColor() { return Srgb(1, 0.72, 0.25); }
FLinearColor SAcPilotOverlay::EnemyColor() { return Srgb(1, 0.3, 0.25); }
FLinearColor SAcPilotOverlay::PanelFill() { return Srgb(0.02, 0.05, 0.09, 0.72); }
FLinearColor SAcPilotOverlay::PanelStroke() { return Srgb(0.3, 0.55, 0.75, 0.55); }

FLinearColor SAcPilotOverlay::ToneColor(const EAcTone Tone)
{
	switch (Tone)
	{
	case EAcTone::Ready:
	case EAcTone::Working: return ReadyColor();
	case EAcTone::Blocked: return BlockedColor();
	case EAcTone::Enemy: return EnemyColor();
	default: return LineColor();
	}
}

void SAcPilotOverlay::Construct(const FArguments& InArgs)
{
	bPanel = InArgs._Panel;
	SetVisibility(EVisibility::HitTestInvisible);
}

void SAcPilotOverlay::Reset()
{
	bHasInfo = false;
	LastHp.Reset();
	HurtSince = -10;
	HitSince = -10;
	ShownHit = 0;
	Floats.Reset();
	ShownNote.Reset();
	ScaleFrom = ScaleTo = 1;
	ScaleSince = -10;
}

double SAcPilotOverlay::ReticleScale(const double Now) const
{
	return FMath::Lerp(ScaleFrom, ScaleTo, FMath::Clamp((Now - ScaleSince) / 0.12, 0.0, 1.0));
}

void SAcPilotOverlay::Show(const FAcPilotInfo& In, const double Now)
{
	Clock = Now;
	// Open over something to work; closed in on an enemy in range.
	const double Open = In.Tone == EAcTone::Ready ? 1.25 : In.Tone == EAcTone::Enemy ? 0.8 : 1;
	const double Scale = ReticleScale(Now);
	if (FMath::Abs(Scale - Open) > 0.01 && ScaleTo != Open)
	{
		ScaleFrom = Scale;
		ScaleTo = Open;
		ScaleSince = Now;
	}
	if (In.Hit && In.Hit->Serial != ShownHit)
	{
		// `markHit`: the ticks flash, the damage floats off (KILLED for a kill).
		ShownHit = In.Hit->Serial;
		HitSince = Now;
		bHitKill = In.Hit->bKill;
		const double D = In.Hit->Damage;
		const FString Text = In.Hit->bKill ? FString(TEXT("KILLED"))
			: TEXT("-") + (FMath::RoundToDouble(D) == D ? FString::Printf(TEXT("%lld"), (long long)D) : FString::Printf(TEXT("%.1f"), D));
		Floats.Add({Text, In.Hit->bKill, Now});
	}
	if (!HoldMarkAge) Floats.RemoveAll([Now](const FFloat& F) { return Now - F.Born > (F.bKill ? 1.0 : 0.55); });
	if (In.Note != ShownNote)
	{
		ShownNote = In.Note;
		NoteSince = Now;
	}
	if (LastHp && In.Hp < *LastHp - 0.01) HurtSince = Now;
	LastHp = In.Hp;
	Info = In;
	bHasInfo = true;
}

int32 SAcPilotOverlay::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	if (!bHasInfo) return Layer;
	const double Now = FPlatformTime::Seconds();
	const FVector2D Size(Geometry.GetLocalSize());
	const double W = Size.X, H = Size.Y;
	if (W <= 0 || H <= 0) return Layer;
	FAcPilotPen P{Out, Geometry, Layer, H};
	const FVector2D C(W / 2, H / 2);

	// Inside a building: a soft green vignette round the world (it still
	// reads, the dashboard untouched), its name over it. Over a dashboard
	// the sides fade out down to it and there is no bottom band.
	if (Info.Inside)
	{
		const bool bDash = !View.IsEmpty();
		const FAcRect R = bDash ? View : FAcRect(0, 0, W, H);
		const double Band = FMath::Min(R.W, R.H) * 0.22;
		const FLinearColor Edge = Srgb(0.02, 0.14, 0.08, 0.55), Clear = Srgb(0.02, 0.14, 0.08, 0);
		const FAcRect In = R.Inset(Band, Band);
		const FVector2D O[4] = {{R.MinX(), R.MinY()}, {R.MaxX(), R.MinY()}, {R.MaxX(), R.MaxY()}, {R.MinX(), R.MaxY()}};
		const FVector2D I[4] = {{In.MinX(), In.MinY()}, {In.MaxX(), In.MinY()}, {In.MaxX(), In.MaxY()}, {In.MinX(), In.MaxY()}};
		// The corners' colours: the bottom ones clear over a dashboard.
		const FLinearColor At[4] = {bDash ? Clear : Edge, bDash ? Clear : Edge, Edge, Edge};
		for (int32 K = bDash ? 1 : 0; K < 4; ++K)
		{
			const int32 N = (K + 1) % 4;
			P.Poly({O[K], O[N], I[N], I[K]}, TArray<FLinearColor>{At[K], At[N], Clear, Clear});
		}
		P.Label(*Info.Inside, 30, Srgb(0.5, 1, 0.55, 0.9), FVector2D(C.X, C.Y + 70), EAcHAlign::Center, EAcVAlign::Baseline, false);
	}

	// The hurt flash: a red band round the view, fading over half a second.
	if (const double T = bHoldHurt ? 0.05 : Now - HurtSince; T >= 0 && T < 0.5)
	{
		const double A = 0.9 * (1 - T / 0.5);
		const TArray<FVector2D> Edge = {FVector2D(0, 0), FVector2D(W, 0), FVector2D(W, H), FVector2D(0, H), FVector2D(0, 0)};
		P.Lines(Edge, Srgb(1, 0.15, 0.1, 0.8 * A), 26, 20);
	}

	if (Info.Menu) PaintMenu(P, H);
	// The note over the reticle.
	if (ShownNote)
	{
		const double Opacity = bHoldNote ? 1 : FadeAfter(Now - NoteSince, 1.4, 0.5);
		P.Label(*ShownNote, 18, BlockedColor(), FVector2D(C.X, C.Y + 84), EAcHAlign::Center, EAcVAlign::Baseline, true, true,
			(float)Opacity);
	}
	// A gain: "+25 ORE" rising over the reticle, held 1.2 s, fading 0.6 s.
	if (const double T = Now - GainSince; !GainText.IsEmpty() && T >= 0 && T < 1.8)
	{
		const double E = 1 - FMath::Pow(1 - FMath::Clamp(T / 0.6, 0.0, 1.0), 3.0);
		const float A = (float)(T < 0.12 ? T / 0.12 : FadeAfter(T, 1.2, 0.6));
		// Ore lilac, MH mirror-silver (docs/naming.md).
		const FLinearColor Tint = GainText.EndsWith(TEXT(" MH")) ? Srgb(0.84, 0.92, 0.98) : Srgb(0.78, 0.66, 1);
		P.Label(GainText, 34, Tint, FVector2D(C.X, C.Y + 110 + 26 * E), EAcHAlign::Center, EAcVAlign::Baseline, true, true, A);
		// The realm's cut of it (C1), in the realm's gold.
		if (!GainNote.IsEmpty())
		{
			P.Label(GainNote, 17, Srgb(1.0, 0.82, 0.42), FVector2D(C.X, C.Y + 86 + 26 * E), EAcHAlign::Center, EAcVAlign::Baseline, true,
				true, A);
		}
	}
	if (Info.Feed) PaintFeed(P, W, H, Now);
	// C2: the build wheel takes the reticle's place while it is open.
	if (!Info.Wheel) PaintReticle(P, C, Now);
	PaintSight(P, W, H);
	PaintMarks(P, C, Now);
	PaintCompass(P, W, H);
	if (Info.Wheel) PaintWheel(P, View.IsEmpty() ? FAcRect(0, 0, W, H) : View);
	if (bPanel)
	{
		PaintPanel(P, W);
		// The keys, bottom left, bottom line first.
		TArray<FString> Lines;
		Info.Help.ParseIntoArray(Lines, TEXT("\n"), true);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			P.Label(Lines[I], 13, White(0.85, 0.85), FVector2D(18, 22 + I * 17), EAcHAlign::Left);
		}
	}
	return P.Layer;
}

void SAcPilotOverlay::PaintReticle(FAcPilotPen& P, const FVector2D C, const double Now) const
{
	const double S = ReticleScale(Now);
	const FLinearColor Tone = ToneColor(Info.Tone);
	auto At = [&](double X, double Y) { return C + FVector2D(X, Y) * S; };
	// Four corner brackets and a centre pip; a soldier's open with its
	// cone (its edge, past the brackets' inner 6).
	const double Open = Info.Cone ? FMath::Max(0.0, *Info.Cone * P.H / 2 / S - 6) : 0;
	for (const FVector2D Sign : {FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
	{
		P.Lines({At(Sign.X * (14 + Open), Sign.Y * (6 + Open)), At(Sign.X * (14 + Open), Sign.Y * (14 + Open)),
					At(Sign.X * (6 + Open), Sign.Y * (14 + Open))},
			Tone, 1.6 * S, 1.2 * S);
	}
	P.Disc(C, 1.6 * S, LineColor());
	// The work ring: from the top, clockwise.
	if (Info.Progress)
	{
		const double Pr = FMath::Clamp(*Info.Progress, 0.0, 1.0);
		P.Lines(FAcPilotPen::Circle(C, 30 * S, 64), White(1, 0.12), 4 * S);
		TArray<FVector2D> Arc;
		const int32 N = FMath::Max(2, (int32)FMath::CeilToInt(64 * Pr));
		for (int32 K = 0; K <= N; ++K)
		{
			const double A = PI / 2 - Pr * 2 * PI * K / N;
			Arc.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * 30 * S);
		}
		if (Pr > 0) P.Lines(Arc, ReadyColor(), 4 * S, 2 * S, true);
		P.Label(FString::Printf(TEXT("%s  %d%%"), *Info.ProgressLabel, (int32)(Pr * 100)), 17 * S, ReadyColor(), At(0, 46),
			EAcHAlign::Center, EAcVAlign::Center);
	}
	// The prompt under it, on a dark plate (only without a console).
	if (bPanel && Info.Prompt)
	{
		const double Size = 17 * S;
		const double Wd = FAcConsolePaint::TextWidth(*Info.Prompt, Size) + 22 * S;
		const FVector2D Pc = At(0, -52);
		const FAcRect Plate(Pc.X - Wd / 2, Pc.Y - 12 * S, Wd, 24 * S);
		P.Plate(Plate, 5 * S, FLinearColor(0, 0, 0, 0.55f), White(1, 0.12), 0.8 * S);
		P.Label(*Info.Prompt, Size, Tone, Pc, EAcHAlign::Center, EAcVAlign::Center);
	}
	// A vehicle's gun marker: where its weapon really points.
	if (Info.Gun)
	{
		const double Hh = P.H / 2;
		const FVector2D G(C.X + Info.Gun->X * Hh, Hh + Info.Gun->Y * Hh);
		P.Lines(FAcPilotPen::Circle(G, 9, 40), Alpha(Tone, 0.9), 1.6, 1);
		P.Disc(G, 1.6, Tone);
	}
}

void SAcPilotOverlay::PaintSight(FAcPilotPen& P, const double W, const double H) const
{
	if (!Info.Sight) return;
	// Under the compass, clear of the target.
	const FAcSightInfo& S = *Info.Sight;
	const FVector2D O(W / 2, H - 132);
	const FLinearColor Color = !S.bInRange ? BlockedColor() : S.bFriend ? ReadyColor() : EnemyColor();
	P.Label(S.Title.ToUpper(), 17, Color, O + FVector2D(-90, 13), EAcHAlign::Left, EAcVAlign::Baseline, true, false);
	P.Label(FString::Printf(TEXT("%.1f"), S.Distance), 14, White(0.9, 0.9), O + FVector2D(90, 13), EAcHAlign::Right,
		EAcVAlign::Baseline, true, false);
	P.Plate(FAcRect(O.X - 90, O.Y, 180, 8), 1.5, FLinearColor(0, 0, 0, 0.55f), White(1, 0.25), 0.8);
	const double F = FMath::Clamp(S.Hp / FMath::Max(S.MaxHp, 1.0), 0.0, 1.0);
	P.Fill(FAcRect(O.X - 88.5, O.Y + 1.5, 177 * F, 5), Health(F, EnemyColor()));
}

void SAcPilotOverlay::PaintMarks(FAcPilotPen& P, const FVector2D C, const double Now) const
{
	// The hit marker: four ticks round the pip, flashed per hit.
	const double T = HoldMarkAge ? *HoldMarkAge : Now - HitSince;
	const double Wait = bHitKill ? HitWaitKill : HitWait, Fade = bHitKill ? HitFadeKill : HitFade;
	if (T >= 0 && T < Wait + Fade)
	{
		const double A = FadeAfter(T, Wait, Fade);
		const double S = FMath::Lerp(bHitKill ? 1.7 : 1.25, 1.0, FMath::Clamp(T / 0.08, 0.0, 1.0));
		const FLinearColor Color = Alpha(bHitKill ? EnemyColor() : FLinearColor::White, A);
		for (const FVector2D Sign : {FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
		{
			P.Lines({C + Sign * 3.5 * S, C + Sign * 8.5 * S}, Color, (bHitKill ? 3 : 2) * S, 1 * S, true);
		}
	}
	// The damage floating off it; KILLED over the reticle.
	for (const FFloat& Fl : Floats)
	{
		const double Age = HoldMarkAge ? *HoldMarkAge : Now - Fl.Born;
		if (Fl.bKill)
		{
			const FVector2D At = C + FVector2D(0, 64 + 8 * EaseOut(Age / 1.0));
			P.Label(Fl.Text, 18, EnemyColor(), At, EAcHAlign::Center, EAcVAlign::Center, true, true, (float)FadeAfter(Age, 0.6, 0.4));
		}
		else
		{
			const double E = EaseOut(Age / 0.55);
			const FVector2D At = C + FVector2D(18 + 10 * E, 8 + 22 * E);
			P.Label(Fl.Text, 15, Srgb(1, 0.9, 0.55), At, EAcHAlign::Left, EAcVAlign::Center, true, true, (float)FadeAfter(Age, 0.25, 0.3));
		}
	}
}

void SAcPilotOverlay::PaintCompass(FAcPilotPen& P, const double W, const double H) const
{
	const FVector2D O(W / 2, H - 64 - 16);
	const double Half = TapeWidth / 2, PerDeg = TapeWidth / TapeSpan;
	P.Plate(FAcRect(O.X - Half, O.Y - 14, TapeWidth, 28), 3, PanelFill(), PanelStroke(), 1);
	// Compass: 0° is north (−Z, up the top-down view), 90° east (+X).
	const double Deg = (Info.Heading + PI / 2) * 180 / PI;
	const double Wrapped = FMath::Fmod(FMath::Fmod(Deg, 360.0) + 360, 360.0);
	const double Shift = -Wrapped * PerDeg;
	P.PushClip(FAcRect(O.X - Half + 2, O.Y - 14, TapeWidth - 4, 28));
	// Ticks every 15°, labels every 45°, over 720° so it wraps.
	static const TMap<int32, FString> Names = {{0, TEXT("N")}, {45, TEXT("NE")}, {90, TEXT("E")}, {135, TEXT("SE")},
		{180, TEXT("S")}, {225, TEXT("SW")}, {270, TEXT("W")}, {315, TEXT("NW")}};
	for (int32 D = -360; D <= 360; D += 15)
	{
		const double X = O.X + Shift + D * PerDeg;
		if (X < O.X - Half - 20 || X > O.X + Half + 20) continue;
		const bool bMajor = D % 45 == 0;
		P.Fill(FAcRect(X - 0.5, O.Y - 13, 1, bMajor ? 9 : 5), Alpha(LineColor(), bMajor ? 0.9 : 0.5));
		if (const FString* N = bMajor ? Names.Find((D + 360) % 360) : nullptr)
		{
			P.Label(*N, N->Len() == 1 ? 15 : 12, *N == TEXT("N") ? BlockedColor() : White(0.9, 0.9), FVector2D(X, O.Y + 4),
				EAcHAlign::Center, EAcVAlign::Center, false, false);
		}
	}
	TOptional<double> GoalX;
	if (Info.Goal)
	{
		const ac::Vec2 D(Info.Goal->At.x - Info.Position.x, Info.Goal->At.y - Info.Position.y);
		const double R = std::remainder(std::atan2(D.y, D.x) - Info.Heading, 2 * PI);
		GoalX = FMath::Clamp(R * 180 / PI * PerDeg, -Half + 8, Half - 8);
		// The goal: a diamond on the tape.
		const FVector2D G(O.X + *GoalX, O.Y);
		const TArray<FVector2D> Diamond = {G + FVector2D(0, -12), G + FVector2D(6, -4), G + FVector2D(0, 4), G + FVector2D(-6, -4)};
		P.Poly(Diamond, ReadyColor());
		TArray<FVector2D> Ring = Diamond;
		Ring.Add(Diamond[0]);
		P.Lines(Ring, FLinearColor::White, 1, 2);
	}
	// KSTR contracts here: green hollow diamonds, the distance over each;
	// Citadel missions (K2): a filled diamond in the tier's colour, "MISSION".
	TArray<TTuple<double, double, const FAcPilotGoal*>> MarkX;
	for (const FAcPilotGoal& M : Info.Marks)
	{
		const ac::Vec2 D(M.At.x - Info.Position.x, M.At.y - Info.Position.y);
		const double R = std::remainder(std::atan2(D.y, D.x) - Info.Heading, 2 * PI);
		const double X = FMath::Clamp(R * 180 / PI * PerDeg, -Half + 8, Half - 8);
		const FVector2D G(O.X + X, O.Y);
		const TArray<FVector2D> Diamond = {G + FVector2D(0, -11), G + FVector2D(6, -3), G + FVector2D(0, 5), G + FVector2D(-6, -3), G + FVector2D(0, -11)};
		if (M.bMission)
		{
			P.Poly(TArray<FVector2D>{Diamond[0], Diamond[1], Diamond[2], Diamond[3]}, M.Color);
			P.Lines(Diamond, FLinearColor::White, 1, 1.5);
		}
		else
		{
			P.Lines(Diamond, Srgb(0.45, 1, 0.55), 2.2, 1.5);
		}
		MarkX.Add({X, M.Distance, &M});
	}
	P.PopClip();
	for (const auto& [X, Distance, M] : MarkX)
	{
		P.Label(FString::Printf(TEXT("%s  %lld"), M->bMission ? TEXT("MISSION") : TEXT("KSTR"), (long long)FMath::RoundToDouble(Distance)), 13,
			M->bMission ? M->Color : Srgb(0.45, 1, 0.55), FVector2D(O.X + X, O.Y + 30));
	}
	if (GoalX)
	{
		P.Label(FString::Printf(TEXT("%s  %lld"), *Info.Goal->Label, (long long)FMath::RoundToDouble(Info.Goal->Distance)), 14,
			ReadyColor(), FVector2D(O.X + *GoalX, O.Y - 36));
	}
	// The centre notch and the heading in degrees.
	P.Fill(FAcRect(O.X - 1, O.Y - 16, 2, 32), FLinearColor::White);
	P.Label(FString::Printf(TEXT("%03d°"), (int32)FMath::RoundToDouble(Wrapped) % 360), 12, White(0.85), O + FVector2D(Half + 24, -5),
		EAcHAlign::Center, EAcVAlign::Baseline, false, false);
}

void SAcPilotOverlay::PaintPanel(FAcPilotPen& P, const double W) const
{
	// The unit's panel, bottom centre (only without a console).
	const double Pw = 360, Ph = 74;
	const FVector2D O(W / 2 - Pw / 2, 18);
	auto At = [&](double X, double Y) { return O + FVector2D(X, Y); };
	// A cut-corner console plate.
	const TArray<FVector2D> Outline = {At(0, 0), At(Pw, 0), At(Pw, Ph - 14), At(Pw - 14, Ph), At(14, Ph), At(0, Ph - 14)};
	P.Poly(Outline, PanelFill());
	TArray<FVector2D> Edge = Outline;
	Edge.Add(Outline[0]);
	P.Lines(Edge, PanelStroke(), 1.2);
	P.Label(Info.Name, 22, White(0.95), At(16, Ph - 30), EAcHAlign::Left, EAcVAlign::Baseline, false);
	// Hit points: a segmented bar.
	P.Plate(FAcRect(O.X + 16, O.Y + 14, 200, 12), 2, FLinearColor(0, 0, 0, 0.5f), PanelStroke(), 1);
	const double F = FMath::Clamp(Info.Hp / FMath::Max(Info.MaxHp, 1.0), 0.0, 1.0);
	P.Fill(FAcRect(O.X + 16, O.Y + 14, 200 * F, 12), Health(F, Srgb(1, 0.25, 0.2)));
	for (int32 K = 1; K < 10; ++K) P.Fill(FAcRect(O.X + 16 + K * 20, O.Y + 14, 1, 12), FLinearColor(0, 0, 0, 0.5f));
	P.Label(FString::Printf(TEXT("%lld / %lld"), (long long)FMath::RoundToDouble(Info.Hp), (long long)Info.MaxHp), 13, White(0.85),
		At(16, 32), EAcHAlign::Left, EAcVAlign::Baseline, false, false);
	// The cargo bay: an icon and the load.
	P.Plate(FAcRect(O.X + Pw - 124, O.Y + 12, 108, 48), 3, FLinearColor(0, 0, 0, 0.4f), PanelStroke(), 1);
	P.Label(TEXT("CARGO"), 11, White(0.7), At(Pw - 70, 46), EAcHAlign::Center, EAcVAlign::Baseline, false, false);
	if (Info.Cargo > 0)
	{
		const FVector2D Ic = At(Pw - 98, 28);
		P.Image(Info.bHydrogen ? FAcHudStyle::HydrogenIcon() : FAcHudStyle::OreIcon(), FAcRect(Ic.X - 13, Ic.Y - 13, 26, 26));
		// Silver for MH, opal violet for ore.
		P.Label(FString::Printf(TEXT("%lld"), (long long)Info.Cargo), 24,
			Info.bHydrogen ? Srgb(0.84, 0.9, 0.98) : Srgb(0.78, 0.66, 1), At(Pw - 80, 28), EAcHAlign::Left, EAcVAlign::Center, false, false);
	}
	else
	{
		P.Label(TEXT("empty"), 16, White(0.55), At(Pw - 80, 28), EAcHAlign::Left, EAcVAlign::Center, false, false);
	}
}

void SAcPilotOverlay::PaintMenu(FAcPilotPen& P, const double H) const
{
	// The build menu, left of the reticle: a panel of numbered lines.
	const TArray<FAcPilotMenuLine>& Menu = *Info.Menu;
	const FVector2D O(24, H / 2 + 120);
	const double RowH = 24, Wd = 300, Hh = (Menu.Num() + 1) * RowH + 18;
	P.Plate(FAcRect(O.X, O.Y - Hh + 8, Wd, Hh), 4, PanelFill(), PanelStroke(), 1.2);
	P.Label(TEXT("BUILD  (number to pick, B to close)"), 14, LineColor(), O + FVector2D(12, -14), EAcHAlign::Left,
		EAcVAlign::Baseline, false, false);
	for (int32 I = 0; I < Menu.Num(); ++I)
	{
		const bool bOk = Menu[I].bOk;
		// Dimmed with the line when it can't be afforded.
		P.Label(Menu[I].Line, 16, bOk ? FLinearColor::White : White(0.5), O + FVector2D(12, -14 - (I + 1) * RowH), EAcHAlign::Left,
			EAcVAlign::Baseline, false, true, 1, bOk ? FAcHudStyle::NameAccent() : Alpha(FAcHudStyle::NameAccent(), 0.5));
	}
}

void SAcPilotOverlay::PaintWheel(FAcPilotPen& P, const FAcRect& Glass) const
{
	// The ring of buildings round the glass's middle, the first slice at
	// the top and the rest clockwise; the pick lit and pushed out, its name,
	// price and state in the hub; the stick's needle on the hub's rim.
	const FAcBuildWheelInfo& Wh = *Info.Wheel;
	const int32 N = Wh.Slices.Num();
	if (N == 0) return;
	// A little under the glass's middle: the banners hang from its top.
	const FVector2D C(Glass.X + Glass.W / 2, Glass.Y + Glass.H / 2 - 14);
	// Sized to the glass (its hint line under it).
	const double Out = FMath::Clamp(Glass.H / 2 - 92, 120.0, WheelOuter), In = Out * WheelInner / WheelOuter;

	const double Half = UE_DOUBLE_PI / N, Gap = 0.012;
	auto Dir = [](double A) { return FVector2D(std::sin(A), std::cos(A)); };
	P.Disc(C, Out + 22, Srgb(0.0, 0.01, 0.02, 0.30));
	P.Lines(FAcPilotPen::Circle(C, Out + 22), Alpha(PanelStroke(), 0.45), 1);
	for (int32 I = 0; I < N; ++I)
	{
		const FAcWheelSlice& S = Wh.Slices[I];
		const bool bPick = I == Wh.Pick, bBuild = S.State == FAcWheelSlice::EState::Build;
		const double Mid = FAcBuildWheel::SliceAngle(I, N), Push = bPick ? 10 : 0;
		const double R0 = In + Push, R1 = Out + Push, A0 = Mid - Half + Gap, A1 = Mid + Half - Gap;
		const FLinearColor Fill = bPick ? (bBuild ? Srgb(0.08, 0.32, 0.42, 0.88) : Srgb(0.30, 0.18, 0.04, 0.88))
			: bBuild ? Srgb(0.05, 0.13, 0.19, 0.80) : Srgb(0.03, 0.05, 0.07, 0.72);
		// The annular wedge in convex quads.
		const int32 Steps = FMath::Max(2, int32((A1 - A0) / 0.06));
		TArray<FVector2D> Rim;
		for (int32 K = 0; K <= Steps; ++K)
		{
			const double A = A0 + (A1 - A0) * K / Steps, B = A0 + (A1 - A0) * (K + 1) / Steps;
			if (K < Steps) P.Poly({C + Dir(A) * R0, C + Dir(A) * R1, C + Dir(B) * R1, C + Dir(B) * R0}, Fill);
			Rim.Add(C + Dir(A) * R1);
		}
		const FLinearColor Edge = bPick ? (bBuild ? FAcHudStyle::Cyan() : BlockedColor()) : Alpha(PanelStroke(), 0.7);
		P.Lines(Rim, Edge, bPick ? 3 : 1.2, bPick ? 6 : 0);
		// Its icon and number key.
		const FVector2D Ic = C + Dir(Mid) * ((R0 + R1) / 2 + 6);
		const double Is = bPick ? 54 : 44;
		const float Dim = bBuild ? 1.f : 0.38f;
		if (const FSlateBrush* B = FAcIcons::Brush(S.Icon))
		{
			P.Image(B, FAcRect(Ic.X - Is / 2, Ic.Y - Is / 2, Is, Is), FLinearColor(Dim, Dim, Dim, bBuild ? 1.f : 0.7f));
		}
		else
		{
			P.Label(S.Title.Left(2), 18, White(0.9, Dim), Ic, EAcHAlign::Center, EAcVAlign::Center, false, false);
		}
		const FVector2D Nk = C + Dir(Mid) * (R0 + 14);
		if (S.Number > 0) P.Label(FString::FromInt(S.Number), 12, White(0.75, 0.8), Nk, EAcHAlign::Center, EAcVAlign::Center, false, false);
		if (S.State == FAcWheelSlice::EState::Soon)
		{
			P.Label(TEXT("SOON"), 11, White(0.6, 0.8), C + Dir(Mid) * (R1 - 14), EAcHAlign::Center, EAcVAlign::Center, false, false);
		}
	}
	// The hub.
	P.Disc(C, In - 8, Srgb(0.01, 0.03, 0.05, 0.9));
	P.Lines(FAcPilotPen::Circle(C, In - 8), Alpha(PanelStroke(), 0.9), 1.2);
	if (Wh.Pick >= 0 && Wh.Pick < N)
	{
		const FAcWheelSlice& S = Wh.Slices[Wh.Pick];
		const bool bBuild = S.State == FAcWheelSlice::EState::Build;
		P.Label(S.Title, S.Title.Len() > 11 ? 14 : 17, FLinearColor::White, C + FVector2D(0, 14), EAcHAlign::Center,
			EAcVAlign::Baseline, true, false);
		FString Cost = FString::Printf(TEXT("%d ORE"), S.Ore);
		if (S.Hydrogen > 0) Cost += FString::Printf(TEXT(" · %d MH"), S.Hydrogen);
		P.Label(Cost, 13, Srgb(0.78, 0.66, 1), C + FVector2D(0, -4), EAcHAlign::Center, EAcVAlign::Baseline, true, false);
		if (!S.Tag.IsEmpty())
		{
			P.Label(S.Tag, 12, bBuild ? FAcHudStyle::Cyan() : BlockedColor(), C + FVector2D(0, -22), EAcHAlign::Center,
				EAcVAlign::Baseline, true, false);
		}
	}
	else
	{
		P.Label(TEXT("BUILD"), 18, White(0.95), C + FVector2D(0, 4), EAcHAlign::Center, EAcVAlign::Baseline, true, false);
		P.Label(TEXT("move the mouse"), 11, White(0.7), C + FVector2D(0, -14), EAcHAlign::Center, EAcVAlign::Baseline, true, false);
	}
	// The stick's needle: a tick on the hub's rim, out as far as it leans.
	if (const double L = Wh.Stick.Size(); L > 1)
	{
		const FVector2D D = Wh.Stick / L;
		const double Lean = FMath::Clamp(L / FAcBuildWheel::Reach, 0.0, 1.0);
		const FLinearColor Tone = Wh.Pick >= 0 ? FAcHudStyle::Cyan() : White(0.6, 0.8);
		P.Lines({C + D * (In - 8), C + D * (In - 8 + 10 * Lean + 2)}, Tone, 3, 3, true);
	}
	// A locked pick's reason in full ("This region has its Citadel: one a region").
	if (Wh.Pick >= 0 && Wh.Pick < N && Wh.Slices[Wh.Pick].State != FAcWheelSlice::EState::Build && !Wh.Slices[Wh.Pick].Why.IsEmpty())
	{
		P.Label(Wh.Slices[Wh.Pick].Why, 15, BlockedColor(), C - FVector2D(0, Out + 40), EAcHAlign::Center, EAcVAlign::Baseline, true,
			false);
	}
	P.Label(TEXT("CLICK BUILDS  ·  B OR ESC CLOSES  ·  NUMBER KEYS PICK"), 12, White(0.8, 0.9), C - FVector2D(0, Out + 60),
		EAcHAlign::Center, EAcVAlign::Baseline, true, false);
}

void SAcPilotOverlay::PaintFeed(FAcPilotPen& P, const double W, const double H, const double Now) const
{
	// The gun's camera as a monitor shows it: the glass the world shows
	// through, a cold tint and scanlines over it, dark brackets at its
	// corners, a LIVE pip; its name top left, the arc gauge bottom centre.
	const FAcTurretFeed& F = *Info.Feed;
	const FAcRect R = View.IsEmpty() ? FAcRect(0, 0, W, H) : View;
	P.Fill(R, Srgb(0.05, 0.22, 0.26, 0.10));
	// The scanlines: one serpentine line (its turns under the frame's sides).
	TArray<FVector2D> Scan;
	int32 Row = 0;
	for (double Y = R.MaxY() - 2; Y > R.MinY(); Y -= 3, ++Row)
	{
		const bool bRight = Row % 2 == 0;
		Scan.Add(FVector2D(bRight ? R.MinX() : R.MaxX(), Y));
		Scan.Add(FVector2D(bRight ? R.MaxX() : R.MinX(), Y));
	}
	P.Lines(Scan, Srgb(0.0, 0.05, 0.06, 0.16), 1);
	// The corner brackets.
	const double In = 22, Len = 54;
	const FLinearColor Dark = Srgb(0.01, 0.02, 0.03, 0.85), Edge = Alpha(LineColor(), 0.55);
	for (const FVector2D Sign : {FVector2D(-1, 1), FVector2D(1, 1), FVector2D(1, -1), FVector2D(-1, -1)})
	{
		const FVector2D K(Sign.X < 0 ? R.MinX() + In : R.MaxX() - In, Sign.Y < 0 ? R.MinY() + In : R.MaxY() - In);
		const TArray<FVector2D> Corner = {K - FVector2D(0, Sign.Y * Len), K, K - FVector2D(Sign.X * Len, 0)};
		P.Lines(Corner, Dark, 7);
		P.Lines(Corner, Edge, 1.2);
	}
	// LIVE, its pip blinking once a second, top right; the gun top left;
	// both under the HUD's top bars (the region box, the wallet).
	const double Top = R.MaxY() - In - 92;
	if (std::fmod(Now, 1.0) < 0.6) P.Disc(FVector2D(R.MaxX() - In - 66, Top + 6), 5, EnemyColor());
	P.Label(TEXT("LIVE"), 16, White(0.95), FVector2D(R.MaxX() - In - 54, Top), EAcHAlign::Left);
	P.Label(F.Title, 20, White(0.95), FVector2D(R.MinX() + In + 14, Top), EAcHAlign::Left, EAcVAlign::Baseline, true, false);
	if (!F.Marks.IsEmpty())
	{
		P.Label(F.Marks, 13, Alpha(LineColor(), 0.95), FVector2D(R.MinX() + In + 14, Top - 20), EAcHAlign::Left, EAcVAlign::Baseline,
			true, false);
	}

	// At the bottom edge a slim strip (the dashboard is down at a gun): the
	// unit's hit points and the keys. The lower middle of the frame stays
	// clear down to it: the gun's barrels and muzzle ring are there.
	const double StripH = 40;
	P.Fill(FAcRect(R.MinX(), R.MinY(), R.W, StripH), Srgb(0.01, 0.03, 0.05, 0.78));
	P.Fill(FAcRect(R.MinX(), R.MinY() + StripH - 1, R.W, 1), Edge);
	const double HpF = FMath::Clamp(Info.Hp / FMath::Max(Info.MaxHp, 1.0), 0.0, 1.0);
	const FAcRect HpBar(R.MinX() + In + 14, R.MinY() + 14, 200, 12);
	P.Plate(HpBar, 2, FLinearColor(0, 0, 0, 0.5f), PanelStroke(), 1);
	P.Fill(FAcRect(HpBar.MinX(), HpBar.MinY(), HpBar.W * HpF, HpBar.H), HpF > 0.5 ? Srgb(0.3, 0.9, 0.4) : HpF > 0.25 ? BlockedColor() : EnemyColor());
	P.Label(FString::Printf(TEXT("%s  %lld / %lld"), *Info.Name, (long long)FMath::RoundToDouble(Info.Hp), (long long)Info.MaxHp), 13, White(0.9),
		FVector2D(HpBar.MaxX() + 12, HpBar.MinY() + 1), EAcHAlign::Left, EAcVAlign::Baseline, true, false);
	P.Label(TEXT("Click · FIRE      Esc · DISENGAGE"), 15, White(0.95), FVector2D(R.MaxX() - In - 14, R.MinY() + 14), EAcHAlign::Right,
		EAcVAlign::Baseline, true, false);

	// The arc gauge, lower left beside the heat: the gun's 220 degrees as a
	// band (the hull's 140 below it dark), the hard stops in red, the aim as
	// a needle.
	const double Rad = 58;
	const double PipW = 9, PipH = 16, Gap = 4;
	const double BarW = FAcTurretFeed::HeatPips * (PipW + Gap) - Gap;
	const double PipsX = R.MinX() + In + 14;
	const FVector2D G(PipsX + BarW + 40 + Rad, R.MinY() + StripH + 26 + Rad);
	auto Arc = [&](const double From, const double To, const double Radius)
	{
		TArray<FVector2D> A;
		const int32 N = FMath::Max(2, int32(FMath::Abs(To - From) / 0.06));
		for (int32 K = 0; K <= N; ++K)
		{
			const double T = From + (To - From) * K / N;
			A.Add(G + FVector2D(FMath::Cos(T), FMath::Sin(T)) * Radius);
		}
		return A;
	};
	const double Lo = GaugeAngle(F.ArcHalf), Hi = GaugeAngle(-F.ArcHalf);
	P.Lines(Arc(Lo, Hi - 2 * UE_DOUBLE_PI, Rad), Srgb(0.2, 0.08, 0.06, 0.55), 6);
	P.Lines(Arc(Lo, Hi, Rad), Srgb(0.02, 0.05, 0.07, 0.7), 10);
	P.Lines(Arc(Lo, Hi, Rad), Alpha(LineColor(), 0.75), 2, 1);
	for (const double Stop : {Lo, Hi})
	{
		const FVector2D D(FMath::Cos(Stop), FMath::Sin(Stop));
		P.Lines({G + D * (Rad - 9), G + D * (Rad + 9)}, EnemyColor(), 3, 1.5);
	}
	const bool bAtStop = FMath::Abs(F.Off) >= F.ArcHalf - 1e-3;
	const double Needle = GaugeAngle(F.Off);
	const FVector2D N(FMath::Cos(Needle), FMath::Sin(Needle));
	P.Lines({G + N * 10, G + N * (Rad + 4)}, bAtStop ? EnemyColor() : ReadyColor(), 2.4, 1.4, true);
	P.Disc(G, 3.5, LineColor());
	// The pitch: a short scale right of the dial, its limits at the ends.
	const double PitchX = G.X + Rad + 30, PitchLo = G.Y - Rad * 0.55, PitchHi = G.Y + Rad * 0.75;
	P.Lines({FVector2D(PitchX, PitchLo), FVector2D(PitchX, PitchHi)}, Alpha(LineColor(), 0.75), 2);
	const double Pt = (F.Pitch - F.PitchMin) / FMath::Max(F.PitchMax - F.PitchMin, 1e-3);
	const double PitchY = FMath::Lerp(PitchLo, PitchHi, FMath::Clamp(Pt, 0.0, 1.0));
	P.Lines({FVector2D(PitchX - 7, PitchY), FVector2D(PitchX + 7, PitchY)}, ReadyColor(), 2.4, 1.2);
	// Heat pips left of the dial, the cooldown under them.
	for (int32 K = 0; K < FAcTurretFeed::HeatPips; ++K)
	{
		const bool bLit = K < F.Heat;
		const FLinearColor Hot = K >= FAcTurretFeed::HeatPips - 2 ? EnemyColor() : BlockedColor();
		P.Plate(FAcRect(PipsX + K * (PipW + Gap), G.Y, PipW, PipH), 1.5, bLit ? Hot : FLinearColor(0, 0, 0, 0.45f), Alpha(Hot, 0.6), 0.8);
	}
	P.Plate(FAcRect(PipsX, G.Y - 10, BarW, 4), 1, FLinearColor(0, 0, 0, 0.5f), White(1, 0.2), 0.6);
	P.Fill(FAcRect(PipsX, G.Y - 10, BarW * F.Ready, 4), F.Ready >= 1 ? ReadyColor() : BlockedColor());
	P.Label(TEXT("HEAT"), 11, White(0.75), FVector2D(PipsX, G.Y + PipH + 6), EAcHAlign::Left, EAcVAlign::Baseline, true, false);
}
