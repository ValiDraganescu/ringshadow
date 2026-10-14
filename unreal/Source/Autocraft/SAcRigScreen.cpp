#include "SAcRigScreen.h"

#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcLog.h"

#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

#include <array>
#include <cmath>

namespace
{
	// The panel, in the HUD root's points (as SAcSkillScreen).
	constexpr float PanelW = 1360;
	constexpr float Pad = 26;
	constexpr float HeaderH = 92;
	constexpr float Gap = 16;
	constexpr float FitW = 476;
	constexpr float FoundryW = 492;
	constexpr float LabW = PanelW - 2 * Pad - 2 * Gap - FitW - FoundryW;
	constexpr float HeadH = 100;
	constexpr float RowH = 88;
	constexpr float RowGap = 8;
	constexpr float Inset = 10;
	constexpr float CellGap = 8;
	constexpr float ChipW = 56;
	constexpr float ChipH = 68;
	constexpr float ChipGap = 6;
	constexpr float FootH = 56;
	/// A cell's flash when it changed for the better, seconds.
	constexpr double FlashSeconds = 0.9;
	/// The atlas: the five modules, the Foundry, the Lab, the fitting.
	constexpr int32 AtlasCells = 8;

	enum ERow : int32
	{
		RowCell = 1,
		RowTab = 2,
		RowClose = 3,
	};

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

	/// A rectangle's outline `W` points wide, inside it.
	void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W = 1)
	{
		const FVector2f S = B.GetSize();
		Box(Out, Layer, G, FBox2f(B.Min, B.Min + FVector2f(S.X, W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Min.X, B.Max.Y - W), B.Max), C);
		Box(Out, Layer, G, FBox2f(B.Min + FVector2f(0, W), FVector2f(B.Min.X + W, B.Max.Y - W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Max.X - W, B.Min.Y + W), FVector2f(B.Max.X, B.Max.Y - W)), C);
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
		EAcVAlign V = EAcVAlign::Center, EAcFontWeight Weight = EAcFontWeight::Bold, float Kern = 0)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		T.Kern = Kern;
		return T;
	}

	/// `S` cut into lines no wider than `Width` at `Size`.
	TArray<FString> Wrap(const FString& S, float Size, float Width, EAcFontWeight Weight = EAcFontWeight::Medium)
	{
		TArray<FString> Words;
		S.ParseIntoArrayWS(Words);
		TArray<FString> Lines;
		FString Now;
		for (const FString& W : Words)
		{
			const FString Try = Now.IsEmpty() ? W : Now + TEXT(" ") + W;
			if (!Now.IsEmpty() && Label(Try, Size, FLinearColor::White, EAcHAlign::Left, EAcVAlign::Top, Weight).Measure().X > Width)
			{
				Lines.Add(Now);
				Now = W;
			}
			else
			{
				Now = Try;
			}
		}
		if (!Now.IsEmpty()) Lines.Add(Now);
		return Lines;
	}

	// MARK: The art: the icons' atlas and a soft glow

	struct FArt
	{
		UTexture2D* Atlas = nullptr;
		std::array<FSlateBrush, AtlasCells> Icons;
		UTexture2D* GlowTexture = nullptr;
		FSlateBrush Glow;
		bool bTried = false;
	};

	FArt& Art()
	{
		static FArt A;
		if (A.bTried) return A;
		A.bTried = true;
		const FString File = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/Textures/Skills/module_icons.png")));
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *File))
		{
			A.Atlas = FAcHudStyle::ImportHudTexture(TArrayView64<const uint8>(Bytes.GetData(), Bytes.Num()));
		}
		if (A.Atlas)
		{
			A.Atlas->AddToRoot();
			for (int32 I = 0; I < AtlasCells; ++I)
			{
				const float U0 = I / float(AtlasCells);
				FSlateBrush& B = A.Icons[I];
				B.DrawAs = ESlateBrushDrawType::Image;
				B.SetResourceObject(A.Atlas);
				B.ImageSize = FVector2D(128, 128);
				B.SetUVRegion(FBox2f(FVector2f(U0, 0), FVector2f(U0 + 1 / float(AtlasCells), 1)));
			}
		}
		else
		{
			UE_LOG(LogAutocraft, Warning, TEXT("rig: no icons at %s (uv run unreal/Tools/hud/module_icons.py)"), *File);
		}
		// A soft round glow, drawn as a box: its corners round, its middle stretched.
		constexpr int32 G = 64;
		FImage Image(G, G, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		TArrayView64<FColor> Px = Image.AsBGRA8();
		for (int32 Y = 0; Y < G; ++Y)
		{
			for (int32 X = 0; X < G; ++X)
			{
				const float D = FVector2f((X + 0.5f) / G - 0.5f, (Y + 0.5f) / G - 0.5f).Size() * 2;
				const float T = FMath::Clamp(1 - D, 0.f, 1.f);
				Px[Y * G + X] = FColor(255, 255, 255, uint8(FMath::RoundToInt(255 * T * T)));
			}
		}
		A.GlowTexture = FAcHudStyle::HudTexture(Image);
		if (A.GlowTexture)
		{
			A.GlowTexture->AddToRoot();
			A.Glow.DrawAs = ESlateBrushDrawType::Box;
			A.Glow.SetResourceObject(A.GlowTexture);
			A.Glow.Margin = FMargin(0.5f);
		}
		return A;
	}

	const FSlateBrush* AtlasIcon(const int32 I)
	{
		FArt& A = Art();
		return A.Atlas && I >= 0 && I < AtlasCells ? &A.Icons[I] : nullptr;
	}

	/// A soft glow `Reach` points round `B`.
	void Glow(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float Reach)
	{
		FArt& A = Art();
		if (!A.GlowTexture || C.A <= 0) return;
		A.Glow.ImageSize = FVector2D(2 * Reach, 2 * Reach);
		const FBox2f R = B.ExpandBy(Reach);
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(R.GetSize()), FSlateLayoutTransform(FVector2D(R.Min))), &A.Glow,
			ESlateDrawEffect::None, C);
	}

	void Image(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FSlateBrush* Brush, const FBox2f& B, const FLinearColor& C)
	{
		if (!Brush || C.A <= 0) return;
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(B.GetSize()), FSlateLayoutTransform(FVector2D(B.Min))), Brush,
			ESlateDrawEffect::None, C);
	}

	FString Upper(const std::string& S) { return Text(S).ToUpper(); }

	FString Grouped(int64 N) { return FText::AsNumber(N).ToString(); }

	/// "1 min", "3 min", "30 min", "60 s".
	FString Duration(double S)
	{
		return S >= 60 ? FString::Printf(TEXT("%d min"), FMath::RoundToInt(S / 60)) : FString::Printf(TEXT("%d s"), FMath::RoundToInt(S));
	}

	/// "Drill head Mk II".
	FString ModuleMark(ac::Module M, int64 Mark) { return Text(ac::Modules::title(M) + " " + ac::Modules::markName(Mark)); }

	// The palette's dark tones (SAcSkillScreen's).
	FLinearColor Ink() { return FAcHudStyle::Srgb(0.78, 0.86, 0.92); }
	FLinearColor Dim() { return FAcHudStyle::Srgb(0.42, 0.5, 0.57); }
	FLinearColor Faint() { return FAcHudStyle::Srgb(0.26, 0.32, 0.38); }
	FLinearColor Steel() { return FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8); }
	FLinearColor Refused() { return FAcHudStyle::Srgb(1.0, 0.55, 0.42); }
	FLinearColor Ready() { return FAcHudStyle::Srgb(0.45, 0.95, 0.62); }
	FLinearColor Dark() { return FAcHudStyle::Srgb(0.03, 0.05, 0.08, 1); }
	/// Each panel's accent: the fitting cyan, the Foundry's melt, the Lab violet.
	FLinearColor Molten() { return FAcHudStyle::Srgb(1, 0.6, 0.26); }
	FLinearColor Violet() { return FAcHudStyle::Srgb(0.74, 0.56, 1); }
	FLinearColor Accent(SAcRigScreen::EPanel P)
	{
		switch (P)
		{
		case SAcRigScreen::EPanel::Fitting: return FAcHudStyle::Cyan();
		case SAcRigScreen::EPanel::Foundry: return Molten();
		case SAcRigScreen::EPanel::Lab: return Violet();
		}
		return FAcHudStyle::Cyan();
	}

	/// A job's bar: `Share` done, its colour, a bright leading edge.
	void JobBar(FSlateWindowElementList& Out, int32 L, const FGeometry& P, const FBox2f& Bar, double Share, const FLinearColor& C)
	{
		Box(Out, L, P, Bar, FAcHudStyle::Srgb(0.06, 0.1, 0.15, 1));
		const FBox2f Fill(Bar.Min, FVector2f(Bar.Min.X + Bar.GetSize().X * float(FMath::Clamp(Share, 0.0, 1.0)), Bar.Max.Y));
		if (Fill.GetSize().X > 0)
		{
			Glow(Out, L, P, Fill, WithAlpha(C, 0.3f), 5);
			Box(Out, L + 1, P, Fill, C);
			Box(Out, L + 2, P, FBox2f(Fill.Min, FVector2f(Fill.Max.X, Fill.Min.Y + 1.5f)), FAcHudStyle::Blend(C, 0.5, FLinearColor::White));
		}
	}
}

// MARK: - Construction and the data

void SAcRigScreen::Construct(const FArguments& InArgs)
{
	OnMakeDelegate = InArgs._OnMake;
	OnResearchDelegate = InArgs._OnResearch;
	OnFitDelegate = InArgs._OnFit;
	OnTabDelegate = InArgs._OnTab;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	Art();
	// The countdowns run and the flashes fade: a repaint a frame while open.
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcRigScreen::Animate));
}

SAcRigScreen::~SAcRigScreen() = default;

double SAcRigScreen::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcRigScreen::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

const FSlateBrush* SAcRigScreen::Icon(const ac::Module Module) { return AtlasIcon(int32(Module)); }
const FSlateBrush* SAcRigScreen::FoundryIcon() { return AtlasIcon(5); }
const FSlateBrush* SAcRigScreen::LabIcon() { return AtlasIcon(6); }

FLinearColor SAcRigScreen::MarkColor(const int64 Mark)
{
	switch (Mark)
	{
	case 1: return FAcHudStyle::Srgb(0.62, 0.74, 0.84);
	case 2: return FAcHudStyle::Cyan();
	case 3: return FAcHudStyle::LevelGold();
	default: return Dim();
	}
}

FString SAcRigScreen::Countdown(const double Seconds)
{
	const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
	return S >= 60 ? FString::Printf(TEXT("%d:%02d"), S / 60, S % 60) : FString::Printf(TEXT("%d s"), S);
}

void SAcRigScreen::SetInfo(const FAcRigScreenInfo& In)
{
	if (In == Info) return;
	// What got better since: a module in the stores, a mark opened, a slot
	// changed. Its cell flashes, and an old refusal is stale.
	if (In.Rig && Info.Rig)
	{
		const double Now = Clock();
		bool bChanged = false;
		for (const ac::Module M : ac::Modules::all())
		{
			for (int64 Mark = 1; Mark <= ac::Modules::marks; ++Mark)
			{
				if (In.Rig->stock(M, Mark) > Info.Rig->stock(M, Mark))
				{
					Flashes.Add(CellIndex({M, EPanel::Fitting, Mark}), Now);
					Flashes.Add(CellIndex({M, EPanel::Foundry, Mark}), Now);
					bChanged = true;
				}
			}
			if (In.Rig->researchedMark(M) > Info.Rig->researchedMark(M))
			{
				Flashes.Add(CellIndex({M, EPanel::Lab, In.Rig->researchedMark(M)}), Now);
				bChanged = true;
			}
			if (In.Rig->fittedMark(M) != Info.Rig->fittedMark(M))
			{
				if (In.Rig->fittedMark(M) > 0) Flashes.Add(CellIndex({M, EPanel::Fitting, In.Rig->fittedMark(M)}), Now);
				bChanged = true;
			}
		}
		if (bChanged || In.Rig->making != Info.Rig->making || In.Rig->research != Info.Rig->research) Refusal.Reset();
	}
	Info = In;
	if (!bPickSet && Info.Rig)
	{
		// The first pick: a chip in stock, else the first mark the Foundry
		// can make, else the drill head's Mk I there.
		std::optional<FCell> First;
		for (int32 Row = 0; Row < 5 && !First; ++Row)
		{
			for (int32 Column = 0; Column < RowCells && !First; ++Column)
			{
				const FCell C = CellAt(Row, Column);
				if (C.Panel == EPanel::Fitting && Info.Rig->stock(C.Module, C.Mark) > 0 && WhyNot(C).IsEmpty()) First = C;
			}
		}
		for (int32 Row = 0; Row < 5 && !First; ++Row)
		{
			for (int32 Column = 3; Column < 6 && !First; ++Column)
			{
				if (WhyNot(CellAt(Row, Column)).IsEmpty()) First = CellAt(Row, Column);
			}
		}
		Pick = First.value_or(FCell{ac::Module::drillHead, EPanel::Foundry, 1});
		bPickSet = true;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcRigScreen::SetRefusal(const FString& Why)
{
	if (Why == Refusal) return;
	Refusal = Why;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcRigScreen::PickCell(const FCell& To)
{
	if (!(Pick == To)) Refusal.Reset();
	Pick = To;
	bPickSet = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

// MARK: - The cells

SAcRigScreen::FCell SAcRigScreen::CellAt(const int32 Row, const int32 Column)
{
	FCell C;
	C.Module = ac::Modules::all()[FMath::Clamp(Row, 0, 4)];
	if (Column < 3)
	{
		C.Panel = EPanel::Fitting;
		C.Mark = Column + 1;
	}
	else if (Column < 6)
	{
		C.Panel = EPanel::Foundry;
		C.Mark = Column - 2;
	}
	else
	{
		C.Panel = EPanel::Lab;
		C.Mark = FMath::Min<int64>(Column - 4, 3);
	}
	return C;
}

int32 SAcRigScreen::ColumnOf(const FCell& Cell)
{
	switch (Cell.Panel)
	{
	case EPanel::Fitting: return int32(Cell.Mark) - 1;
	case EPanel::Foundry: return int32(Cell.Mark) + 2;
	case EPanel::Lab: return int32(Cell.Mark) + 4;
	}
	return 0;
}

int32 SAcRigScreen::CellIndex(const FCell& Cell) { return int32(Cell.Module) * RowCells + ColumnOf(Cell); }

void SAcRigScreen::MovePick(const int32 DX, const int32 DY)
{
	const int32 Row = FMath::Clamp(int32(Pick.Module) + DY, 0, 4);
	const int32 Column = FMath::Clamp(ColumnOf(Pick) + DX, 0, RowCells - 1);
	PickCell(CellAt(Row, Column));
}

// MARK: - The asks

FString SAcRigScreen::WhyNot(const FCell& Cell) const
{
	if (!Info.Rig) return TEXT("Waiting for the server");
	const ac::Rig& R = *Info.Rig;
	std::optional<std::string> Why;
	switch (Cell.Panel)
	{
	case EPanel::Fitting:
		// The fitted chip takes it off; another fits from the stores. The
		// server checks the stores first, then the place.
		Why = R.fitRefusal(Cell.Module, R.fittedMark(Cell.Module) == Cell.Mark ? 0 : Cell.Mark);
		if (!Why && !Info.FitWhyNot.IsEmpty()) return Info.FitWhyNot;
		break;
	case EPanel::Foundry:
		if (!Info.bFoundry) return TEXT("Build a Foundry first");
		Why = R.makeRefusal(Cell.Module, Cell.Mark, Info.Ore, Info.Hydrogen);
		break;
	case EPanel::Lab:
		if (!Info.bLab) return TEXT("Build a Lab first");
		Why = R.researchRefusal(Cell.Module, Cell.Mark, Info.Hydrogen);
		break;
	}
	return Why ? Text(*Why) : FString();
}

void SAcRigScreen::Act()
{
	const FString Why = WhyNot(Pick);
	if (!Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	switch (Pick.Panel)
	{
	case EPanel::Fitting:
		OnFitDelegate.ExecuteIfBound(Pick.Module, Info.Rig->fittedMark(Pick.Module) == Pick.Mark ? 0 : Pick.Mark);
		break;
	case EPanel::Foundry: OnMakeDelegate.ExecuteIfBound(Pick.Module, Pick.Mark); break;
	case EPanel::Lab: OnResearchDelegate.ExecuteIfBound(Pick.Module, Pick.Mark); break;
	}
}

void SAcRigScreen::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcRigScreen::Key(const FKey& InKey)
{
	if (InKey == EKeys::Tab) { OnTabDelegate.ExecuteIfBound(0); return true; }
	if (InKey == EKeys::Left || InKey == EKeys::A) { MovePick(-1, 0); return true; }
	if (InKey == EKeys::Right || InKey == EKeys::D) { MovePick(1, 0); return true; }
	if (InKey == EKeys::Up || InKey == EKeys::W) { MovePick(0, -1); return true; }
	if (InKey == EKeys::Down || InKey == EKeys::S) { MovePick(0, 1); return true; }
	if (InKey == EKeys::Enter || InKey == EKeys::SpaceBar) { Act(); return true; }
	return false;
}

// MARK: - Layout

void SAcRigScreen::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	Cells.Reset();
	Rows.Reset();
	auto Add = [&](const FBox2f& B, int32 RowId, int32 Index) {
		FTarget T;
		T.Box = B;
		T.Row = RowId;
		T.Index = Index;
		Targets.Add(T);
	};
	// The header: the tabs where the skill screen has them, the close on the right.
	const float TabsX = 640;
	Add(FBox2f(FVector2f(TabsX, 30), FVector2f(TabsX + 96, 66)), RowTab, 0);
	Add(FBox2f(FVector2f(TabsX + 110, 30), FVector2f(TabsX + 250, 66)), RowTab, 1);
	Add(FBox2f(FVector2f(TabsX + 264, 30), FVector2f(TabsX + 330, 66)), RowTab, 2);
	Add(FBox2f(FVector2f(PanelW - Pad - 40, 26), FVector2f(PanelW - Pad, 58)), RowClose, 0);

	const float Top = HeaderH + 16;
	const float RowsTop = Top + HeadH;
	const float Bottom = RowsTop + 5 * RowH + 4 * RowGap + 12;
	const float FitX = Pad, FoundryX = FitX + FitW + Gap, LabX = FoundryX + FoundryW + Gap;
	PanelBoxes[0] = FBox2f(FVector2f(FitX, Top), FVector2f(FitX + FitW, Bottom));
	PanelBoxes[1] = FBox2f(FVector2f(FoundryX, Top), FVector2f(FoundryX + FoundryW, Bottom));
	PanelBoxes[2] = FBox2f(FVector2f(LabX, Top), FVector2f(LabX + LabW, Bottom));
	FootBox = FBox2f(FVector2f(Pad, Bottom + 16), FVector2f(PanelW - Pad, Bottom + 16 + FootH));

	for (int32 Row = 0; Row < 5; ++Row)
	{
		const float Y = RowsTop + Row * (RowH + RowGap);
		Rows.Add(FBox2f(FVector2f(FitX + Inset, Y), FVector2f(FitX + FitW - Inset, Y + RowH)));
		// The fitting's chips, right in its row.
		const float ChipsX = FitX + FitW - Inset - 8 - (3 * ChipW + 2 * ChipGap);
		const float ChipY = Y + (RowH - ChipH) / 2;
		for (int32 Column = 0; Column < RowCells; ++Column)
		{
			const FCell C = CellAt(Row, Column);
			FBox2f B;
			if (C.Panel == EPanel::Fitting)
			{
				const float X = ChipsX + Column * (ChipW + ChipGap);
				B = FBox2f(FVector2f(X, ChipY), FVector2f(X + ChipW, ChipY + ChipH));
			}
			else if (C.Panel == EPanel::Foundry)
			{
				const float W = (FoundryW - 2 * Inset - 2 * CellGap) / 3;
				const float X = FoundryX + Inset + (Column - 3) * (W + CellGap);
				B = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + RowH));
			}
			else
			{
				const float W = (LabW - 2 * Inset - CellGap) / 2;
				const float X = LabX + Inset + (Column - 6) * (W + CellGap);
				B = FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + RowH));
			}
			Cells.Add(CellIndex(C), B);
			Add(B, RowCell, CellIndex(C));
		}
	}
	PanelSize = FVector2f(PanelW, FootBox.Max.Y + 18);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcRigScreen::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

void SAcRigScreen::Press(const FTarget& T)
{
	switch (T.Row)
	{
	case RowCell:
	{
		// A click on the picked cell asks; on another, picks it and asks.
		const FCell C = CellAt(T.Index / RowCells, T.Index % RowCells);
		PickCell(C);
		Act();
		break;
	}
	case RowTab:
		if (T.Index < 2) OnTabDelegate.ExecuteIfBound(T.Index);
		break;
	case RowClose: Close(); break;
	default: break;
	}
}

FReply SAcRigScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Pressed = TargetAt(Local);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcRigScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcRigScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
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

FReply SAcRigScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	Hovered = TargetAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcRigScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcRigScreen::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcRigScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	// The world goes on behind, darkened.
	const float In = FMath::Clamp(float(Clock() - OpenedAt) / 0.18f, 0.f, 1.f);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.62f * In));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// The panel: dark glass, a steel rim with cut corners, a cyan line inside it (Chrome).
	Glow(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.2, 0.6, 0.9, 0.16f), 28);
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.02, 0.036, 0.062, 0.97));
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)),
			FAcHudStyle::Srgb(0.35, 0.85, 1, 0.035f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.26), 1, 14);
	const int32 L = Layer + 3;
	PaintHeader(Out, L, P);
	PaintPanel(Out, L, P, EPanel::Fitting);
	PaintPanel(Out, L, P, EPanel::Foundry);
	PaintPanel(Out, L, P, EPanel::Lab);
	PaintFoot(Out, L, P);
	return L + 24;
}

void SAcRigScreen::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FTarget& T, const FString& Text,
	const bool bLit, const FLinearColor& Accent) const
{
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& X) { return X.Row == T.Row && X.Index == T.Index; });
	const bool bHover = Index == Hovered;
	const bool bDown = Index == Pressed && bHover;
	FLinearColor Fill = FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1);
	FLinearColor Rim = Steel();
	FLinearColor Text2 = Dim();
	if (bLit)
	{
		Fill = FAcHudStyle::Blend(FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1), 0.28, Accent);
		Rim = Accent;
		Text2 = FAcHudStyle::Text();
		if (bHover) Glow(Out, L, P, T.Box, WithAlpha(Accent, 0.35f), 12);
	}
	if (bHover) Rim = bLit ? FAcHudStyle::Blend(Accent, 0.5, FLinearColor::White) : FAcHudStyle::Ice();
	if (bDown) Fill = FAcHudStyle::Blend(Fill, 0.25, Accent);
	Box(Out, L + 1, P, T.Box, Fill);
	CutFrame(Out, L + 2, P, T.Box, Rim, bLit || bHover ? 1.5f : 1.0f, 6);
	Label(Text, 15, Text2, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0.6f).Paint(Out, L + 3, P, T.Box.GetCenter());
}

void SAcRigScreen::PaintHeader(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	Label(TEXT("PROSPECTOR"), 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 24));
	FAcHudText Title = Label(TEXT("THE RIG"), 34, FAcHudStyle::LevelGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.5f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	Title.Paint(Out, L, P, FVector2f(Pad + 2, 44));

	// The wallet: what the Foundry and the Lab are paid from.
	const float WX = Pad + 190;
	auto Purse = [&](float X, const TCHAR* What, int64 N, const FLinearColor& C) {
		const FBox2f B(FVector2f(X, 32), FVector2f(X + 150, 72));
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 1));
		CutFrame(Out, L + 1, P, B, WithAlpha(C, 0.55f), 1, 7);
		Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 10, B.Min.Y + 10), FVector2f(B.Min.X + 14, B.Max.Y - 10)), C);
		Label(What, 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(B.Min.X + 24, B.Min.Y + 5));
		Label(Info.Rig ? Grouped(N) : FString(TEXT("—")), 19, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.4f)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 24, B.Min.Y + 17));
	};
	Purse(WX, TEXT("STARDUST ORE"), Info.Ore, FAcHudStyle::Amber());
	Purse(WX + 162, TEXT("METALLIC HYDROGEN"), Info.Hydrogen, FAcHudStyle::Cyan());

	// The tabs: the skill screen's two, and this one lit.
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowTab) continue;
		const bool bOn = T.Index == 2;
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == &T;
		const FLinearColor C = bOn ? FAcHudStyle::Text() : bHover ? FAcHudStyle::Ice() : Dim();
		const TCHAR* Names[] = {TEXT("SKILLS"), TEXT("BLUEPRINTS"), TEXT("RIG")};
		Label(Names[T.Index], 21, C, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 1, P, T.Box.GetCenter());
		if (bOn)
		{
			const FBox2f U(FVector2f(T.Box.Min.X + 8, T.Box.Max.Y + 2), FVector2f(T.Box.Max.X - 8, T.Box.Max.Y + 5));
			Glow(Out, L, P, U, WithAlpha(FAcHudStyle::Cyan(), 0.5f), 8);
			Box(Out, L + 1, P, U, FAcHudStyle::Cyan());
		}
	}
	Label(TEXT("TAB"), 10, Faint(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.0f)
		.Paint(Out, L, P, FVector2f(Targets[0].Box.Max.X + 7, 44));
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowClose) PaintButton(Out, L, P, T, TEXT("ESC"), false, FAcHudStyle::Cyan());
	}
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.18f));
}

void SAcRigScreen::PaintPanel(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const EPanel Which) const
{
	const FBox2f D = PanelBoxes[int32(Which)];
	const FLinearColor C = Accent(Which);
	const bool bOpen = Which == EPanel::Fitting || (Which == EPanel::Foundry ? Info.bFoundry : Info.bLab);
	const ac::Rig* R = Info.Rig ? &*Info.Rig : nullptr;
	// The panel: a darker glass, its accent along the top.
	Box(Out, L, P, D, FAcHudStyle::Srgb(0.015, 0.028, 0.05, 0.96));
	CutFrame(Out, L + 1, P, D, Steel(), 1, 12);
	Box(Out, L + 1, P, FBox2f(D.Min + FVector2f(12, 0), FVector2f(D.Max.X - 12, D.Min.Y + 2)), bOpen ? C : Faint());

	// The head: its icon, its name, what it is doing.
	const FBox2f IconFrame(FVector2f(D.Min.X + 14, D.Min.Y + 16), FVector2f(D.Min.X + 14 + 56, D.Min.Y + 16 + 56));
	Glow(Out, L + 1, P, IconFrame, WithAlpha(C, bOpen ? 0.3f : 0.08f), 12);
	Box(Out, L + 2, P, IconFrame, FAcHudStyle::Blend(Dark(), bOpen ? 0.2 : 0.05, C));
	CutFrame(Out, L + 3, P, IconFrame, bOpen ? C : Faint(), 1.5f, 8);
	const FSlateBrush* Head = Which == EPanel::Fitting ? AtlasIcon(7) : Which == EPanel::Foundry ? FoundryIcon() : LabIcon();
	Image(Out, L + 3, P, Head, IconFrame.ExpandBy(-8), bOpen ? FAcHudStyle::Blend(C, 0.3, FLinearColor::White) : Faint());
	const float TX = IconFrame.Max.X + 14;
	const TCHAR* Name = Which == EPanel::Fitting ? TEXT("FITTING") : Which == EPanel::Foundry ? TEXT("FOUNDRY") : TEXT("LAB");
	Label(Name, 21, bOpen ? FAcHudStyle::Text() : Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f)
		.Paint(Out, L + 2, P, FVector2f(TX, D.Min.Y + 16));

	const float SubY = D.Min.Y + 46;
	const float BarY = D.Min.Y + 74;
	const float BarR = D.Max.X - 16;
	if (Which == EPanel::Fitting)
	{
		int32 Fitted = 0;
		if (R)
		{
			for (const ac::Module M : ac::Modules::all()) Fitted += R->fittedMark(M) > 0 ? 1 : 0;
		}
		Label(FString::Printf(TEXT("%d OF 5 SLOTS"), Fitted), 13, C, EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
			.Paint(Out, L + 2, P, FVector2f(BarR, D.Min.Y + 22));
		const bool bHome = Info.FitWhyNot.IsEmpty();
		const FString Sub = bHome ? FString(TEXT("At home: a chip in stock fits it, the lit one takes it off"))
								  : Info.FitWhyNot;
		TArray<FString> Lines = Wrap(Sub, 13, BarR - TX, EAcFontWeight::SemiBold);
		if (Lines.Num() > 2) Lines.SetNum(2);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			Label(Lines[I], 13, bHome ? Ready() : FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, FVector2f(TX, SubY + 17 * I));
		}
	}
	else
	{
		const std::optional<ac::ModuleJob>& Job = !R ? std::optional<ac::ModuleJob>() : Which == EPanel::Foundry ? R->making : R->research;
		if (!bOpen)
		{
			Label(Which == EPanel::Foundry ? TEXT("Build a Foundry first") : TEXT("Build a Lab first"), 13, FAcHudStyle::Amber(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(TX, SubY));
		}
		else if (Job)
		{
			const double Total = Which == EPanel::Foundry ? ac::Modules::makeTime(Job->mark) : ac::Modules::researchTime(Job->mark);
			const double Left = Job->done - Info.Now;
			const FLinearColor MC = MarkColor(Job->mark);
			Label(FString::Printf(TEXT("%s %s"), Which == EPanel::Foundry ? TEXT("MAKING") : TEXT("RESEARCHING"), *ModuleMark(Job->module, Job->mark).ToUpper()),
				13, MC, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.8f).Paint(Out, L + 2, P, FVector2f(TX, SubY));
			Label(Countdown(Left), 19, FAcHudStyle::Text(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold)
				.Paint(Out, L + 2, P, FVector2f(BarR, D.Min.Y + 18));
			JobBar(Out, L + 2, P, FBox2f(FVector2f(TX, BarY), FVector2f(BarR, BarY + 6)), 1 - Left / FMath::Max(1.0, Total), C);
		}
		else
		{
			Label(Which == EPanel::Foundry ? TEXT("Idle: one module at a time, on the world's clock") : TEXT("Idle: one mark at a time, Mk II first"), 13,
				Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(TX, SubY));
			Label(TEXT("IDLE"), 13, Ready(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 2, P, FVector2f(BarR, D.Min.Y + 22));
			JobBar(Out, L + 2, P, FBox2f(FVector2f(TX, BarY), FVector2f(BarR, BarY + 6)), 0, C);
		}
	}

	// The rows: a band each, the module's cells.
	for (int32 Row = 0; Row < 5; ++Row)
	{
		const ac::Module M = ac::Modules::all()[Row];
		const FBox2f Band(FVector2f(D.Min.X + 4, Rows[Row].Min.Y - 3), FVector2f(D.Max.X - 4, Rows[Row].Max.Y + 3));
		Box(Out, L + 1, P, Band, FAcHudStyle::Srgb(0.35, 0.85, 1, Row % 2 == 0 ? 0.03f : 0.015f));
		if (Which == EPanel::Fitting) PaintFitting(Out, L + 2, P, M, Rows[Row]);
		const int32 From = Which == EPanel::Fitting ? 0 : Which == EPanel::Foundry ? 3 : 6;
		const int32 To = Which == EPanel::Fitting ? 3 : Which == EPanel::Foundry ? 6 : 8;
		for (int32 Column = From; Column < To; ++Column)
		{
			const FCell Cell = CellAt(Row, Column);
			const FBox2f* B = Cells.Find(CellIndex(Cell));
			if (!B) continue;
			if (Which == EPanel::Fitting) PaintChip(Out, L + 2, P, Cell, *B);
			else PaintMark(Out, L + 2, P, Cell, *B);
		}
	}
	// A panel with no building: its cells greyed under a word of what to do.
	if (!bOpen)
	{
		const FBox2f Rows2(FVector2f(D.Min.X, Rows[0].Min.Y - 4), FVector2f(D.Max.X, Rows[4].Max.Y + 4));
		Box(Out, L + 12, P, Rows2, FAcHudStyle::Srgb(0.01, 0.018, 0.03, 0.72f));
		const FBox2f Note(FVector2f(Rows2.GetCenter().X - 150, Rows2.GetCenter().Y - 44), FVector2f(Rows2.GetCenter().X + 150, Rows2.GetCenter().Y + 44));
		Box(Out, L + 13, P, Note, FAcHudStyle::Srgb(0.025, 0.04, 0.065, 0.98));
		CutFrame(Out, L + 14, P, Note, WithAlpha(FAcHudStyle::Amber(), 0.8f), 1.5f, 10);
		Label(Which == EPanel::Foundry ? TEXT("BUILD A FOUNDRY FIRST") : TEXT("BUILD A LAB FIRST"), 17, FAcHudStyle::Amber(), EAcHAlign::Center,
			EAcVAlign::Center, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 15, P, Note.GetCenter() - FVector2f(0, 14));
		Label(TEXT("Its blueprint, then B builds it"), 13, Ink(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Medium)
			.Paint(Out, L + 15, P, Note.GetCenter() + FVector2f(0, 14));
	}
}

void SAcRigScreen::PaintFitting(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const ac::Module M, const FBox2f& Row) const
{
	const int64 Mark = Info.Rig ? Info.Rig->fittedMark(M) : 0;
	const FLinearColor C = MarkColor(Mark);
	// The slot: its icon, lit in its mark's colour when fitted.
	const FBox2f IconBox(FVector2f(Row.Min.X + 4, Row.GetCenter().Y - 32), FVector2f(Row.Min.X + 68, Row.GetCenter().Y + 32));
	if (Mark > 0) Glow(Out, L, P, IconBox, WithAlpha(C, 0.28f), 10);
	Box(Out, L + 1, P, IconBox, Mark > 0 ? FAcHudStyle::Blend(Dark(), 0.22, C) : Dark());
	CutFrame(Out, L + 2, P, IconBox, Mark > 0 ? C : FAcHudStyle::Srgb(0.18, 0.23, 0.29, 1), Mark > 0 ? 1.5f : 1.0f, 8);
	Image(Out, L + 2, P, SAcRigScreen::Icon(M), IconBox.ExpandBy(-9), Mark > 0 ? FAcHudStyle::Blend(C, 0.35, FLinearColor::White) : Dim());
	if (Mark > 0)
	{
		// The mark's pips under the icon.
		for (int64 I = 0; I < ac::Modules::marks; ++I)
		{
			const float X = IconBox.GetCenter().X - 13 + I * 9.5f;
			Box(Out, L + 3, P, FBox2f(FVector2f(X, IconBox.Max.Y - 6), FVector2f(X + 7, IconBox.Max.Y - 3)),
				I < Mark ? C : FAcHudStyle::Srgb(0.16, 0.21, 0.27, 1));
		}
	}
	const float X = IconBox.Max.X + 14;
	const float W = Row.Max.X - 8 - (3 * ChipW + 2 * ChipGap) - 10 - X;
	Label(Upper(ac::Modules::title(M)), 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, FVector2f(X, Row.Min.Y + 12));
	if (Mark > 0)
	{
		Label(Upper(ac::Modules::markName(Mark)) + TEXT(" FITTED"), 11, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
			.Paint(Out, L + 2, P, FVector2f(X, Row.Min.Y + 34));
		TArray<FString> Lines = Wrap(Text(ac::Modules::text(M, Mark)), 13, W);
		if (Lines.Num() > 2) Lines.SetNum(2);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			Label(Lines[I], 13, Ink(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(X, Row.Min.Y + 50 + 16 * I));
		}
	}
	else
	{
		Label(TEXT("EMPTY SLOT"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 2, P, FVector2f(X, Row.Min.Y + 34));
		TArray<FString> Lines = Wrap(Text(ac::Modules::text(M, 1)) + TEXT(" at Mk I"), 13, W);
		if (Lines.Num() > 2) Lines.SetNum(2);
		for (int32 I = 0; I < Lines.Num(); ++I)
		{
			Label(Lines[I], 13, Faint(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(X, Row.Min.Y + 50 + 16 * I));
		}
	}
}

void SAcRigScreen::PaintChip(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FCell& Cell, const FBox2f& B) const
{
	const ac::Rig* R = Info.Rig ? &*Info.Rig : nullptr;
	const int64 Count = R ? R->stock(Cell.Module, Cell.Mark) : 0;
	const bool bFitted = R && R->fittedMark(Cell.Module) == Cell.Mark;
	const bool bCan = WhyNot(Cell).IsEmpty();
	const bool bPicked = Pick == Cell;
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& T) { return T.Row == RowCell && T.Index == CellIndex(Cell); });
	const bool bHover = Index == Hovered;
	const FLinearColor C = MarkColor(Cell.Mark);
	float Flashing = 0;
	if (const double* At = Flashes.Find(CellIndex(Cell))) Flashing = FMath::Clamp(1.f - float(Clock() - *At) / float(FlashSeconds), 0.f, 1.f);

	FLinearColor Fill = Dark();
	FLinearColor Rim = FAcHudStyle::Srgb(0.18, 0.23, 0.29, 1);
	FLinearColor Ink2 = Faint();
	if (Count > 0)
	{
		Fill = FAcHudStyle::Srgb(0.035, 0.07, 0.11, 1);
		Rim = WithAlpha(C, 0.75f);
		Ink2 = Ink();
	}
	if (bFitted)
	{
		Fill = FAcHudStyle::Blend(Dark(), 0.3, C);
		Rim = C;
		Ink2 = FAcHudStyle::Text();
		Glow(Out, L, P, B, WithAlpha(C, 0.3f), 9);
	}
	if (bHover || bPicked) Glow(Out, L, P, B, WithAlpha(bPicked ? FAcHudStyle::Ice() : C, bPicked ? 0.4f : 0.28f), bPicked ? 12 : 10);
	if (Flashing > 0) Glow(Out, L, P, B, FLinearColor(1, 1, 1, 0.8f * Flashing), 6 + 18 * (1 - Flashing));
	Box(Out, L + 1, P, B, Fill);
	CutFrame(Out, L + 2, P, B, bHover && !bFitted ? FAcHudStyle::Blend(Rim, 0.4, FLinearColor::White) : Rim, bFitted || Count > 0 ? 1.5f : 1.0f, 6);
	if (bPicked) CutFrame(Out, L + 3, P, B.ExpandBy(3), FAcHudStyle::Ice(), 1.5f, 8);
	Label(Upper(ac::Modules::markName(Cell.Mark)), 11, bFitted || Count > 0 ? C : Faint(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Bold, 0.8f)
		.Paint(Out, L + 3, P, FVector2f(B.GetCenter().X, B.Min.Y + 7));
	if (bFitted && Count == 0)
	{
		// The fitted one with none left in the stores: the module itself, in its mark's colour.
		const FBox2f IconBox(B.GetCenter() - FVector2f(13, 12), B.GetCenter() + FVector2f(13, 14));
		Image(Out, L + 3, P, SAcRigScreen::Icon(Cell.Module), IconBox, FAcHudStyle::Blend(C, 0.35, FLinearColor::White));
	}
	else
	{
		Label(Count > 0 ? FString::Printf(TEXT("×%lld"), (long long)Count) : FString(TEXT("—")), 19, Ink2, EAcHAlign::Center, EAcVAlign::Center,
			EAcFontWeight::Bold).Paint(Out, L + 3, P, FVector2f(B.GetCenter().X, B.GetCenter().Y + 1));
	}
	const FString Foot = bFitted ? FString(TEXT("OFF")) : Count > 0 ? FString(TEXT("FIT")) : FString();
	if (!Foot.IsEmpty())
	{
		Label(Foot, 10, bCan ? (bFitted ? FAcHudStyle::Text() : Ready()) : Dim(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f)
			.Paint(Out, L + 3, P, FVector2f(B.GetCenter().X, B.Max.Y - 17));
	}
	if (bFitted)
	{
		// The fitted chip's corner, as an active's key corner on a skill tile.
		const TArray<FVector2f> Corner{FVector2f(B.Max.X - 11, B.Min.Y + 1), FVector2f(B.Max.X - 1, B.Min.Y + 1), FVector2f(B.Max.X - 1, B.Min.Y + 11)};
		FSlateDrawElement::MakeLines(Out, L + 3, P.ToPaintGeometry(), Corner, ESlateDrawEffect::None, C, true, 2);
	}
}

void SAcRigScreen::PaintMark(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FCell& Cell, const FBox2f& B) const
{
	const ac::Rig* R = Info.Rig ? &*Info.Rig : nullptr;
	const bool bFoundry = Cell.Panel == EPanel::Foundry;
	const FLinearColor A = Accent(Cell.Panel);
	const FLinearColor C = MarkColor(Cell.Mark);
	const FString Why = WhyNot(Cell);
	const bool bCan = Why.IsEmpty();
	const bool bPicked = Pick == Cell;
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& T) { return T.Row == RowCell && T.Index == CellIndex(Cell); });
	const bool bHover = Index == Hovered;
	const std::optional<ac::ModuleJob>& Job = !R ? std::optional<ac::ModuleJob>() : bFoundry ? R->making : R->research;
	const bool bRunning = Job && Job->module == Cell.Module && Job->mark == Cell.Mark;
	const int64 Researched = R ? R->researchedMark(Cell.Module) : 1;
	const bool bOpened = Researched >= Cell.Mark;
	const bool bDone = !bFoundry && bOpened;
	float Flashing = 0;
	if (const double* At = Flashes.Find(CellIndex(Cell))) Flashing = FMath::Clamp(1.f - float(Clock() - *At) / float(FlashSeconds), 0.f, 1.f);

	// A mark the Foundry can't make yet, or the Lab has done: quieter.
	const bool bQuiet = (bFoundry && !bOpened) || bDone;
	FLinearColor Fill = bQuiet ? FAcHudStyle::Srgb(0.022, 0.035, 0.055, 1) : FAcHudStyle::Srgb(0.03, 0.055, 0.09, 1);
	FLinearColor Rim = bQuiet ? FAcHudStyle::Srgb(0.16, 0.2, 0.25, 1) : WithAlpha(C, 0.6f);
	if (bDone)
	{
		Fill = FAcHudStyle::Blend(Dark(), 0.16, Ready());
		Rim = WithAlpha(Ready(), 0.7f);
	}
	if (bRunning)
	{
		Fill = FAcHudStyle::Blend(Dark(), 0.2, A);
		Rim = A;
		const float Breath = 0.5f + 0.5f * FMath::Sin(float(Clock()) * 3.0f);
		Glow(Out, L, P, B, WithAlpha(A, 0.18f + 0.14f * Breath), 10);
	}
	if (bCan && bHover) Glow(Out, L, P, B, WithAlpha(A, 0.3f), 11);
	if (bPicked) Glow(Out, L, P, B, WithAlpha(FAcHudStyle::Ice(), 0.35f), 12);
	if (Flashing > 0) Glow(Out, L, P, B, FLinearColor(1, 1, 1, 0.8f * Flashing), 6 + 18 * (1 - Flashing));
	Box(Out, L + 1, P, B, Fill);
	CutFrame(Out, L + 2, P, B, bHover ? FAcHudStyle::Blend(Rim, 0.4, FLinearColor::White) : Rim, bRunning || bCan ? 1.5f : 1.0f, 8);
	if (bPicked) CutFrame(Out, L + 3, P, B.ExpandBy(3), FAcHudStyle::Ice(), 1.5f, 10);

	// The mark and its time.
	const float X0 = B.Min.X + 12, X1 = B.Max.X - 10;
	Label(Upper(ac::Modules::markName(Cell.Mark)), 15, bQuiet && !bDone ? Dim() : C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 3, P, FVector2f(X0, B.Min.Y + 9));
	const double Time = bFoundry ? ac::Modules::makeTime(Cell.Mark) : ac::Modules::researchTime(Cell.Mark);
	if (!bDone)
	{
		Label(Duration(Time), 12, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f).Paint(Out, L + 3, P, FVector2f(X1, B.Min.Y + 11));
	}

	// The price: each part red when the wallet is short.
	if (!bDone)
	{
		const int64 Ore = bFoundry ? ac::Modules::ore(Cell.Mark) : 0;
		const int64 Mh = bFoundry ? ac::Modules::hydrogen(Cell.Mark) : ac::Modules::researchHydrogen(Cell.Mark);
		float PX = X0;
		const float PY = B.Min.Y + 34;
		auto Part = [&](int64 N, const TCHAR* Unit, bool bShort, const FLinearColor& Dot) {
			Box(Out, L + 3, P, FBox2f(FVector2f(PX, PY + 4), FVector2f(PX + 6, PY + 10)), bQuiet ? Faint() : Dot);
			PX += 10;
			FAcHudText T = Label(FString::Printf(TEXT("%s %s"), *Grouped(N), Unit), 13, bQuiet ? Dim() : bShort ? Refused() : Ink(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::SemiBold);
			T.Paint(Out, L + 3, P, FVector2f(PX, PY));
			PX += T.Measure().X + 12;
		};
		if (Ore > 0) Part(Ore, TEXT("ore"), Info.Ore < Ore, FAcHudStyle::Amber());
		Part(Mh, TEXT("MH"), Info.Hydrogen < Mh, FAcHudStyle::Cyan());
	}

	// The foot: its state, or the job's bar.
	const float FY = B.Max.Y - 24;
	if (bRunning)
	{
		const double Left = Job->done - Info.Now;
		JobBar(Out, L + 3, P, FBox2f(FVector2f(X0, FY + 12), FVector2f(X1, FY + 16)), 1 - Left / FMath::Max(1.0, Time), A);
		Label(bFoundry ? TEXT("MAKING") : TEXT("RESEARCHING"), 11, A, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
			.Paint(Out, L + 3, P, FVector2f(X0, FY - 4));
		Label(Countdown(Left), 12, FAcHudStyle::Text(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 3, P, FVector2f(X1, FY - 5));
		return;
	}
	FString State;
	FLinearColor StateColor = Dim();
	if (bDone)
	{
		State = TEXT("RESEARCHED");
		StateColor = Ready();
		// A check mark.
		const FVector2f K(X0 + 4, B.Min.Y + 46);
		FSlateDrawElement::MakeLines(Out, L + 3, P.ToPaintGeometry(), TArray<FVector2f>{K + FVector2f(-4, 0), K + FVector2f(0, 5), K + FVector2f(10, -7)},
			ESlateDrawEffect::None, Ready(), true, 2.5f);
		Label(TEXT("Open at the Foundry"), 13, Ink(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 3, P, FVector2f(X0 + 18, B.Min.Y + 38));
	}
	else if (bFoundry && !bOpened)
	{
		State = TEXT("LAB FIRST");
	}
	else if (!bFoundry && Researched + 1 < Cell.Mark)
	{
		State = TEXT("AFTER MK II");
	}
	else if (Job)
	{
		State = bFoundry ? TEXT("FOUNDRY BUSY") : TEXT("LAB BUSY");
	}
	else if (bCan)
	{
		State = bFoundry ? TEXT("MAKE  ›") : TEXT("RESEARCH  ›");
		StateColor = FAcHudStyle::Blend(A, 0.25, FLinearColor::White);
	}
	else
	{
		State = Why.Contains(TEXT("ore")) || Why.Contains(TEXT("MH")) ? FString(TEXT("SHORT")) : FString(TEXT("—"));
		StateColor = Why.Contains(TEXT("ore")) || Why.Contains(TEXT("MH")) ? Refused() : Dim();
	}
	Label(State, 11, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f).Paint(Out, L + 3, P, FVector2f(X0, FY));
	if (bFoundry && R)
	{
		// What the stores hold of it.
		const int64 Have = R->stock(Cell.Module, Cell.Mark) + (R->fittedMark(Cell.Module) == Cell.Mark ? 1 : 0);
		if (Have > 0)
		{
			Label(FString::Printf(TEXT("HAVE %lld"), (long long)Have), 10, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 0.8f)
				.Paint(Out, L + 3, P, FVector2f(X1, FY + 1));
		}
	}
}

void SAcRigScreen::PaintFoot(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f F = FootBox;
	Box(Out, L, P, FBox2f(FVector2f(Pad, F.Min.Y - 8), FVector2f(PanelW - Pad, F.Min.Y - 7)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.14f));
	// The picked cell: what it asks, or why not.
	const FString Why = WhyNot(Pick);
	const ac::Rig* R = Info.Rig ? &*Info.Rig : nullptr;
	FString Head = ModuleMark(Pick.Module, Pick.Mark).ToUpper();
	FString State;
	FLinearColor StateColor = FAcHudStyle::Soft();
	if (!Refusal.IsEmpty())
	{
		State = Refusal;
		StateColor = Refused();
	}
	else if (!Why.IsEmpty())
	{
		State = Why;
		StateColor = Why.Contains(TEXT("already")) ? Ready() : FAcHudStyle::Amber();
	}
	else if (R)
	{
		const FString What = Text(ac::Modules::text(Pick.Module, Pick.Mark));
		switch (Pick.Panel)
		{
		case EPanel::Fitting:
			State = R->fittedMark(Pick.Module) == Pick.Mark
				? FString(TEXT("Click or Return takes it off, back into the stores."))
				: FString::Printf(TEXT("Click or Return fits it from the stores: %s."), *What);
			break;
		case EPanel::Foundry:
			State = FString::Printf(TEXT("Click or Return makes one: %s ore, %s MH, %s. It waits in your stores: %s."), *Grouped(ac::Modules::ore(Pick.Mark)),
				*Grouped(ac::Modules::hydrogen(Pick.Mark)), *Duration(ac::Modules::makeTime(Pick.Mark)), *What);
			break;
		case EPanel::Lab:
			State = FString::Printf(TEXT("Click or Return researches it: %s MH, %s. Then the Foundry can make it."),
				*Grouped(ac::Modules::researchHydrogen(Pick.Mark)), *Duration(ac::Modules::researchTime(Pick.Mark)));
			break;
		}
	}
	const TCHAR* Where = Pick.Panel == EPanel::Fitting ? TEXT("FITTING") : Pick.Panel == EPanel::Foundry ? TEXT("FOUNDRY") : TEXT("LAB");
	Label(FString::Printf(TEXT("%s · %s"), Where, *Head), 12, Accent(Pick.Panel), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 6));
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 26));
	const float KX = PanelW - Pad - 4;
	Label(TEXT("CLICK OR RETURN ASKS · ARROWS MOVE"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 8));
	Label(TEXT("TAB SKILLS · J OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 28));
}
