#include "SAcSkillScreen.h"

#include "AcConsoleInfo.h"
#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcIcons.h"
#include "AcLog.h"

#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/SlateBrush.h"

#include "Simulation.h"

#include <algorithm>
#include <cmath>

namespace
{
	// The panel, in the HUD root's points (as SAcWorldMap).
	constexpr float PanelW = 1360;
	constexpr float Pad = 26;
	constexpr float HeaderH = 92;
	constexpr float GateW = 78;
	constexpr float TileW = 56;
	constexpr float TileH = 80;
	constexpr float TileGap = 5;
	constexpr float ColW = 3 * TileW + 2 * TileGap;
	constexpr float ColGap = 12;
	constexpr float BranchHeadH = 48;
	constexpr float RowGap = 14;
	constexpr float DetailW = 300;
	constexpr float SlotH = 64;
	constexpr float SlotW = 236;
	constexpr float ButtonH = 34;
	/// The learn flash, seconds.
	constexpr double FlashSeconds = 0.75;
	/// A drag starts this far from the press, points.
	constexpr float DragStart = 6;

	enum ERow : int32
	{
		RowTile = 1,
		RowSlot = 2,
		RowTab = 3,
		RowLearn = 4,
		RowUndo = 5,
		RowRespec = 6,
		RowClose = 7,
		RowCard = 8,
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

	void Line(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, FVector2f A, FVector2f B, const FLinearColor& C, float W)
	{
		FSlateDrawElement::MakeLines(Out, Layer, G.ToPaintGeometry(), TArray<FVector2f>{A, B}, ESlateDrawEffect::None, C, true, W);
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
		std::array<FSlateBrush, 65> Icons;
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
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/Textures/Skills/skill_icons.png")));
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *File))
		{
			A.Atlas = FAcHudStyle::ImportHudTexture(TArrayView64<const uint8>(Bytes.GetData(), Bytes.Num()));
		}
		if (A.Atlas)
		{
			A.Atlas->AddToRoot();
			for (int32 I = 0; I < 65; ++I)
			{
				const ac::SkillNode& N = ac::Progression::node(ac::Skill(I));
				int32 Column = 0;
				for (const ac::SkillNode& M : ac::Progression::nodes())
				{
					if (M.skill == N.skill) break;
					if (M.branch == N.branch) Column += 1;
				}
				const float U0 = Column / 13.0f, V0 = int32(N.branch) / 5.0f;
				FSlateBrush& B = A.Icons[I];
				B.DrawAs = ESlateBrushDrawType::Image;
				B.SetResourceObject(A.Atlas);
				B.ImageSize = FVector2D(128, 128);
				B.SetUVRegion(FBox2f(FVector2f(U0, V0), FVector2f(U0 + 1 / 13.0f, V0 + 1 / 5.0f)));
			}
		}
		else
		{
			UE_LOG(LogAutocraft, Warning, TEXT("skills: no icons at %s (uv run unreal/Tools/hud/skill_icons.py)"), *File);
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

	FString Seconds(double S) { return FString::Printf(TEXT("%d s"), FMath::Max(0, FMath::CeilToInt(S))); }

	FString Grouped(int64 N) { return FText::AsNumber(N).ToString(); }

	FString BuildingName(ac::StructureKind K) { return Text(ac::Simulation::title(K)); }

	// The palette's dark tones.
	FLinearColor Ink() { return FAcHudStyle::Srgb(0.78, 0.86, 0.92); }
	FLinearColor Dim() { return FAcHudStyle::Srgb(0.42, 0.5, 0.57); }
	FLinearColor Faint() { return FAcHudStyle::Srgb(0.26, 0.32, 0.38); }
	FLinearColor Steel() { return FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8); }
	FLinearColor Refused() { return FAcHudStyle::Srgb(1.0, 0.55, 0.42); }
	FLinearColor Ready() { return FAcHudStyle::Srgb(0.45, 0.95, 0.62); }
}

// MARK: - Construction and the data

void SAcSkillScreen::Construct(const FArguments& InArgs)
{
	OnSpendDelegate = InArgs._OnSpend;
	OnUndoDelegate = InArgs._OnUndo;
	OnSlotDelegate = InArgs._OnSlot;
	OnRespecDelegate = InArgs._OnRespec;
	OnRigDelegate = InArgs._OnRig;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	Art();
	// The screen breathes (the points badge, the flashes): a repaint a frame while open.
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcSkillScreen::Animate));
}

SAcSkillScreen::~SAcSkillScreen() = default;

double SAcSkillScreen::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcSkillScreen::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

const FSlateBrush* SAcSkillScreen::Icon(const ac::Skill Skill)
{
	FArt& A = Art();
	const int32 I = int32(Skill);
	return A.Atlas && I >= 0 && I < 65 ? &A.Icons[I] : nullptr;
}

FLinearColor SAcSkillScreen::BranchColor(const ac::Branch Branch)
{
	switch (Branch)
	{
	case ac::Branch::deepVein: return FAcHudStyle::LevelGold();
	case ac::Branch::quicksilver: return FAcHudStyle::Cyan();
	case ac::Branch::torchwright: return FAcHudStyle::Srgb(1, 0.52, 0.26);
	case ac::Branch::wayfinder: return FAcHudStyle::Srgb(0.45, 0.93, 0.62);
	case ac::Branch::hardshell: return FAcHudStyle::Srgb(0.74, 0.56, 1);
	}
	return FAcHudStyle::Cyan();
}

void SAcSkillScreen::SetInfo(const FAcSkillScreenInfo& In)
{
	if (In == Info) return;
	// A rank more in a node: its learn flash.
	if (In.Progress)
	{
		const double Now = Clock();
		for (const auto& [Skill, Rank] : In.Progress->ranks)
		{
			if (Info.Progress && Rank > Info.Progress->rank(Skill)) Flashes[Skill] = Now;
		}
		if (Info.Progress && In.Progress->rank(Pick) != Info.Progress->rank(Pick)) Refusal.Reset();
	}
	Info = In;
	if (!bPickSet && Info.Progress)
	{
		// The first pick: the first node a point can go in, else the first learned.
		std::optional<ac::Skill> First;
		for (const ac::SkillNode& N : ac::Progression::nodes())
		{
			if (!Info.Progress->spendRefusal(N.skill)) { First = N.skill; break; }
		}
		if (!First && !Info.Progress->ranks.empty()) First = Info.Progress->ranks.begin()->first;
		Pick = First.value_or(ac::Skill::quickDrill);
		bPickSet = true;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcSkillScreen::SetRefusal(const FString& Why)
{
	if (Why == Refusal) return;
	Refusal = Why;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcSkillScreen::SetTab(const ETab To)
{
	CurrentTab = To;
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
	Dragging.reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcSkillScreen::NextTab()
{
	// Skills, blueprints, then the rig screen when there is one.
	if (CurrentTab == ETab::Blueprints && OnRigDelegate.IsBound()) OnRigDelegate.Execute();
	else SetTab(CurrentTab == ETab::Skills ? ETab::Blueprints : ETab::Skills);
}

void SAcSkillScreen::PickSkill(const ac::Skill To)
{
	if (Pick != To) Refusal.Reset();
	Pick = To;
	bPickSet = true;
	Invalidate(EInvalidateWidgetReason::Paint);
}

// MARK: - The tree's grid

SAcSkillScreen::FCell SAcSkillScreen::CellOf(const ac::Skill Skill)
{
	const ac::SkillNode& N = ac::Progression::node(Skill);
	FCell C;
	C.Column = int32(N.branch);
	C.Row = int32(N.tier) - 1;
	for (const ac::SkillNode& M : ac::Progression::nodes())
	{
		if (M.skill == Skill) break;
		if (M.branch == N.branch && M.tier == N.tier) C.Place += 1;
	}
	return C;
}

std::optional<ac::Skill> SAcSkillScreen::SkillAt(const int32 Column, const int32 Row, const int32 Place)
{
	int32 Seen = 0;
	for (const ac::SkillNode& M : ac::Progression::nodes())
	{
		if (int32(M.branch) != Column || int32(M.tier) - 1 != Row) continue;
		if (Seen++ == Place) return M.skill;
	}
	return std::nullopt;
}

void SAcSkillScreen::MovePick(const int32 DX, const int32 DY)
{
	if (CurrentTab != ETab::Skills) return;
	FCell C = CellOf(Pick);
	if (DY != 0)
	{
		const int32 Row = FMath::Clamp(C.Row + DY, 0, 4);
		if (Row != C.Row)
		{
			// Into the capstone row: its one tile; out of it: the middle one.
			C.Place = Row == 4 ? 0 : C.Row == 4 ? 1 : C.Place;
			C.Row = Row;
		}
	}
	if (DX != 0)
	{
		const int32 Place = C.Place + DX;
		if (Place >= 0 && Place < RowSize(C.Row))
		{
			C.Place = Place;
		}
		else if (C.Column + DX >= 0 && C.Column + DX < 5)
		{
			C.Column += DX;
			C.Place = DX > 0 ? 0 : RowSize(C.Row) - 1;
		}
	}
	if (const std::optional<ac::Skill> S = SkillAt(C.Column, C.Row, C.Place)) PickSkill(*S);
}

// MARK: - The asks

FString SAcSkillScreen::WhyNot(const ac::Skill Skill) const
{
	if (!Info.Progress) return TEXT("Waiting for the server");
	const std::optional<std::string> Why = Info.Progress->spendRefusal(Skill);
	return Why ? Text(*Why) : FString();
}

void SAcSkillScreen::Learn()
{
	if (CurrentTab != ETab::Skills) return;
	const FString Why = WhyNot(Pick);
	if (!Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	OnSpendDelegate.ExecuteIfBound(Pick);
}

void SAcSkillScreen::Undo()
{
	if (CurrentTab != ETab::Skills) return;
	if (!Info.UndoLeft.count(Pick))
	{
		SetRefusal(TEXT("Only a point spent in the last 60 s comes back free"));
		return;
	}
	Refusal.Reset();
	OnUndoDelegate.ExecuteIfBound(Pick);
}

void SAcSkillScreen::SlotPicked(const int32 Slot)
{
	if (CurrentTab != ETab::Skills || !Info.Progress) return;
	const ac::SkillNode& N = ac::Progression::node(Pick);
	if (!N.active)
	{
		SetRefusal(FString::Printf(TEXT("%s is not an active: only actives go in a slot"), *Text(N.title)));
		return;
	}
	// Its own slot again: out of it.
	if (Info.Progress->slots[Slot] == Pick)
	{
		OnSlotDelegate.ExecuteIfBound(Slot, std::nullopt);
		return;
	}
	if (const std::optional<std::string> Why = Info.Progress->slotRefusal(Slot, Pick))
	{
		SetRefusal(Text(*Why));
		return;
	}
	Refusal.Reset();
	OnSlotDelegate.ExecuteIfBound(Slot, Pick);
}

void SAcSkillScreen::Respec()
{
	if (!Info.RespecWhyNot.IsEmpty())
	{
		SetRefusal(Info.RespecWhyNot);
		return;
	}
	OnRespecDelegate.ExecuteIfBound();
}

void SAcSkillScreen::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcSkillScreen::Key(const FKey& InKey)
{
	if (InKey == EKeys::Tab) { NextTab(); return true; }
	if (InKey == EKeys::Left || InKey == EKeys::A) { MovePick(-1, 0); return true; }
	if (InKey == EKeys::Right || InKey == EKeys::D) { MovePick(1, 0); return true; }
	if (InKey == EKeys::Up || InKey == EKeys::W) { MovePick(0, -1); return true; }
	if (InKey == EKeys::Down || InKey == EKeys::S) { MovePick(0, 1); return true; }
	if (InKey == EKeys::Enter || InKey == EKeys::SpaceBar) { Learn(); return true; }
	if (InKey == EKeys::BackSpace || InKey == EKeys::Delete) { Undo(); return true; }
	if (InKey == EKeys::Q) { SlotPicked(0); return true; }
	if (InKey == EKeys::C) { SlotPicked(1); return true; }
	if (InKey == EKeys::G) { SlotPicked(2); return true; }
	return false;
}

// MARK: - Layout

void SAcSkillScreen::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	Tiles.clear();
	auto Add = [&](const FBox2f& B, int32 RowId, int32 Index) {
		FTarget T;
		T.Box = B;
		T.Row = RowId;
		T.Index = Index;
		Targets.Add(T);
	};
	// The header: the tabs in the middle, the respec and close on the right.
	const float TabsX = 640;
	Add(FBox2f(FVector2f(TabsX, 30), FVector2f(TabsX + 96, 66)), RowTab, 0);
	Add(FBox2f(FVector2f(TabsX + 110, 30), FVector2f(TabsX + 250, 66)), RowTab, 1);
	if (OnRigDelegate.IsBound()) Add(FBox2f(FVector2f(TabsX + 264, 30), FVector2f(TabsX + 330, 66)), RowTab, 2);
	Add(FBox2f(FVector2f(PanelW - Pad - 40, 26), FVector2f(PanelW - Pad, 58)), RowClose, 0);
	Add(FBox2f(FVector2f(PanelW - Pad - 40 - 12 - 200, 26), FVector2f(PanelW - Pad - 52, 58)), RowRespec, 0);

	const float Top = HeaderH + 14;
	const float TreeX = Pad + GateW;
	const float TreeW = 5 * ColW + 4 * ColGap;
	const float RowsH = 5 * TileH + 4 * RowGap;
	TreeBox = FBox2f(FVector2f(Pad, Top), FVector2f(TreeX + TreeW, Top + BranchHeadH + RowsH));
	DetailBox = FBox2f(FVector2f(PanelW - Pad - DetailW, Top), FVector2f(PanelW - Pad, TreeBox.Max.Y));
	SlotBox = FBox2f(FVector2f(Pad, TreeBox.Max.Y + 18), FVector2f(PanelW - Pad, TreeBox.Max.Y + 18 + SlotH));

	if (CurrentTab == ETab::Skills)
	{
		for (const ac::SkillNode& N : ac::Progression::nodes())
		{
			const FCell C = CellOf(N.skill);
			const float ColX = TreeX + C.Column * (ColW + ColGap);
			const float X = C.Row == 4 ? ColX + (ColW - TileW) / 2 : ColX + C.Place * (TileW + TileGap);
			const float Y = Top + BranchHeadH + C.Row * (TileH + RowGap);
			const FBox2f B(FVector2f(X, Y), FVector2f(X + TileW, Y + TileH));
			Tiles[N.skill] = B;
			Add(B, RowTile, int32(N.skill));
		}
		// The detail's buttons.
		const float BY = DetailBox.Max.Y - 16 - ButtonH;
		Add(FBox2f(FVector2f(DetailBox.Min.X + 16, BY), FVector2f(DetailBox.Max.X - 16 - 112, BY + ButtonH)), RowLearn, 0);
		Add(FBox2f(FVector2f(DetailBox.Max.X - 16 - 104, BY), FVector2f(DetailBox.Max.X - 16, BY + ButtonH)), RowUndo, 0);
		// The slots.
		for (int32 S = 0; S < ac::Progression::slotCount; ++S)
		{
			const float X = SlotBox.Min.X + 96 + S * (SlotW + 12);
			Add(FBox2f(FVector2f(X, SlotBox.Min.Y + 4), FVector2f(X + SlotW, SlotBox.Max.Y - 4)), RowSlot, S);
		}
	}
	else
	{
		// Three by three cards over the tree and the detail.
		const auto& Kinds = ac::Progression::blueprintKinds();
		const int32 Count = (int32)Kinds.size() + 1;
		const float W = (PanelW - 2 * Pad - 2 * 16) / 3;
		const float H = (TreeBox.Max.Y - Top - 2 * 16) / 3;
		for (int32 I = 0; I < Count; ++I)
		{
			const float X = Pad + (I % 3) * (W + 16);
			const float Y = Top + (I / 3) * (H + 16);
			Add(FBox2f(FVector2f(X, Y), FVector2f(X + W, Y + H)), RowCard, I);
		}
	}
	PanelSize = FVector2f(PanelW, SlotBox.Max.Y + 22);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcSkillScreen::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

void SAcSkillScreen::Press(const FTarget& T)
{
	switch (T.Row)
	{
	case RowTile:
		// A click on the picked node learns a rank; on another, picks it.
		if (ac::Skill(T.Index) == Pick) Learn();
		else PickSkill(ac::Skill(T.Index));
		break;
	case RowLearn: Learn(); break;
	case RowUndo: Undo(); break;
	case RowRespec: Respec(); break;
	case RowClose: Close(); break;
	case RowTab:
		if (T.Index == 2) OnRigDelegate.ExecuteIfBound();
		else SetTab(T.Index == 0 ? ETab::Skills : ETab::Blueprints);
		break;
	case RowSlot:
		if (Info.Progress && ac::Progression::node(Pick).active && Info.Progress->rank(Pick) > 0) SlotPicked(T.Index);
		else if (Info.Progress && Info.Progress->slots[T.Index]) PickSkill(*Info.Progress->slots[T.Index]);
		break;
	default: break;
	}
}

FReply SAcSkillScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const int32 Hit = TargetAt(Local);
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Pressed = Hit;
		PressAt = Local;
	}
	else if (Event.GetEffectingButton() == EKeys::RightMouseButton && Targets.IsValidIndex(Hit) && Targets[Hit].Row == RowTile)
	{
		// A right click takes a fresh point back.
		PickSkill(ac::Skill(Targets[Hit].Index));
		Undo();
	}
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcSkillScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcSkillScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Handled();
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	const int32 Hit = TargetAt(Local);
	const int32 Was = Pressed;
	Pressed = INDEX_NONE;
	if (Dragging)
	{
		// A learned active dropped on a slot.
		const ac::Skill Held = *Dragging;
		Dragging.reset();
		if (Targets.IsValidIndex(Hit) && Targets[Hit].Row == RowSlot)
		{
			PickSkill(Held);
			SlotPicked(Targets[Hit].Index);
		}
		Invalidate(EInvalidateWidgetReason::Paint);
		return FReply::Handled();
	}
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

FReply SAcSkillScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	PointerAt = Local;
	const int32 Hit = TargetAt(Local);
	if (Hit != Hovered) Hovered = Hit;
	// A press on a learned active that moves is a drag toward the slots.
	if (!Dragging && Targets.IsValidIndex(Pressed) && Targets[Pressed].Row == RowTile && Event.IsMouseButtonDown(EKeys::LeftMouseButton)
		&& (Local - PressAt).Size() / PanelScale > DragStart && Info.Progress)
	{
		const ac::Skill S = ac::Skill(Targets[Pressed].Index);
		if (ac::Progression::node(S).active && Info.Progress->rank(S) > 0)
		{
			Dragging = S;
			PickSkill(S);
		}
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcSkillScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
	Dragging.reset();
}

FCursorReply SAcSkillScreen::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	if (Dragging) return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcSkillScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
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
	// A faint wash from the top, as the plates have.
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)),
			FAcHudStyle::Srgb(0.35, 0.85, 1, 0.035f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.26), 1, 14);
	const int32 L = Layer + 3;
	PaintHeader(Out, L, P);
	if (CurrentTab == ETab::Skills)
	{
		PaintTree(Out, L, P);
		PaintDetail(Out, L, P);
		PaintSlots(Out, L, P);
	}
	else
	{
		PaintBlueprints(Out, L, P);
	}
	// A held active follows the pointer.
	if (Dragging)
	{
		const FVector2f At = (PointerAt - PanelOrigin) / PanelScale;
		const FBox2f B(At - FVector2f(22, 22), At + FVector2f(22, 22));
		const FLinearColor C = BranchColor(ac::Progression::node(*Dragging).branch);
		Glow(Out, L + 20, P, B, WithAlpha(C, 0.5f), 14);
		Box(Out, L + 21, P, B, FAcHudStyle::Srgb(0.03, 0.06, 0.1, 0.92));
		Frame(Out, L + 22, P, B, C, 1.5f);
		Image(Out, L + 22, P, Icon(*Dragging), B.ExpandBy(-6), C);
	}
	return L + 24;
}

void SAcSkillScreen::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FTarget& T, const FString& Text,
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

void SAcSkillScreen::PaintHeader(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const double Now = Clock();
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	// The level and the XP bar.
	Label(TEXT("PROSPECTOR"), 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 24));
	const int64 Level = G ? G->level() : 1;
	FAcHudText LevelText = Label(FString::Printf(TEXT("LEVEL %lld"), (long long)Level), 34, FAcHudStyle::LevelGold(), EAcHAlign::Left,
		EAcVAlign::Top, EAcFontWeight::Bold, 0.5f);
	LevelText.ShadowOffset = FVector2f(0, 2);
	LevelText.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	LevelText.Paint(Out, L, P, FVector2f(Pad + 2, 44));
	const float BarX = Pad + 156, BarW = 270, BarY = 58;
	double Share = 1;
	FString XpLine = TEXT("MAX LEVEL");
	if (G && Level < ac::Progression::cap)
	{
		const double From = ac::Progression::xp(Level), To = ac::Progression::xp(Level + 1);
		Share = FMath::Clamp((G->xp - From) / FMath::Max(1.0, To - From), 0.0, 1.0);
		XpLine = FString::Printf(TEXT("%s / %s XP TO LEVEL %lld"), *Grouped(int64(G->xp - From)), *Grouped(int64(To - From)), (long long)(Level + 1));
	}
	Box(Out, L, P, FBox2f(FVector2f(BarX, BarY), FVector2f(BarX + BarW, BarY + 8)), FAcHudStyle::Srgb(0.06, 0.1, 0.15, 1));
	const FBox2f Fill(FVector2f(BarX, BarY), FVector2f(BarX + BarW * float(Share), BarY + 8));
	Glow(Out, L, P, Fill, WithAlpha(FAcHudStyle::LevelGold(), 0.25f), 6);
	Box(Out, L + 1, P, Fill, FAcHudStyle::LevelGold());
	Box(Out, L + 2, P, FBox2f(Fill.Min, FVector2f(Fill.Max.X, Fill.Min.Y + 2)), FAcHudStyle::Srgb(1, 0.95, 0.75, 0.6f));
	Frame(Out, L + 2, P, FBox2f(FVector2f(BarX, BarY), FVector2f(BarX + BarW, BarY + 8)).ExpandBy(1), FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.5f));
	Label(XpLine, 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f).Paint(Out, L, P, FVector2f(BarX, BarY + 16));
	Label(TEXT("XP"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f).Paint(Out, L, P, FVector2f(BarX, 36));

	// The points badge: it breathes while points wait.
	const int64 Points = G ? G->unspent() : 0;
	const FBox2f Badge(FVector2f(BarX + BarW + 22, 34), FVector2f(BarX + BarW + 22 + 132, 70));
	if (Points > 0)
	{
		const float Breath = 0.5f + 0.5f * FMath::Sin(float(Now) * 3.2f);
		Glow(Out, L, P, Badge, WithAlpha(FAcHudStyle::LevelGold(), 0.28f + 0.22f * Breath), 16);
		Box(Out, L + 1, P, Badge, FAcHudStyle::Blend(FAcHudStyle::Srgb(0.05, 0.06, 0.06, 1), 0.3, FAcHudStyle::LevelGold()));
		CutFrame(Out, L + 2, P, Badge, FAcHudStyle::LevelGold(), 1.5f, 8);
		Label(FString::Printf(TEXT("%lld %s"), (long long)Points, Points == 1 ? TEXT("POINT") : TEXT("POINTS")), 19, FAcHudStyle::Text(),
			EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 3, P, Badge.GetCenter());
	}
	else
	{
		CutFrame(Out, L + 2, P, Badge, Faint(), 1, 8);
		Label(TEXT("NO POINTS"), 17, Dim(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 3, P, Badge.GetCenter());
	}
	if (G)
	{
		Label(FString::Printf(TEXT("%lld SPENT OF %lld"), (long long)G->spent(), (long long)ac::Progression::points(Level)), 11, Dim(),
			EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f).Paint(Out, L, P, FVector2f(Badge.GetCenter().X, Badge.Max.Y + 6));
	}

	// The tabs.
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowTab) continue;
		const bool bOn = T.Index < 2 && (T.Index == 0) == (CurrentTab == ETab::Skills);
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == &T;
		const FLinearColor C = bOn ? FAcHudStyle::Text() : bHover ? FAcHudStyle::Ice() : Dim();
		Label(T.Index == 0 ? TEXT("SKILLS") : T.Index == 1 ? TEXT("BLUEPRINTS") : TEXT("RIG"), 21, C, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.6f)
			.Paint(Out, L + 1, P, T.Box.GetCenter());
		if (bOn)
		{
			const FBox2f U(FVector2f(T.Box.Min.X + 8, T.Box.Max.Y + 2), FVector2f(T.Box.Max.X - 8, T.Box.Max.Y + 5));
			Glow(Out, L, P, U, WithAlpha(FAcHudStyle::Cyan(), 0.5f), 8);
			Box(Out, L + 1, P, U, FAcHudStyle::Cyan());
		}
	}
	Label(TEXT("TAB"), 10, Faint(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.0f)
		.Paint(Out, L, P, FVector2f(Targets[0].Box.Max.X + 7, 44));

	// The respec and close.
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowRespec)
		{
			const bool bCan = Info.RespecWhyNot.IsEmpty() && G;
			const FString S = Info.bRespecFree ? FString(TEXT("RESPEC · FREE"))
				: FString::Printf(TEXT("RESPEC · %s MH"), *Grouped(Info.RespecPrice));
			PaintButton(Out, L, P, T, S, bCan, FAcHudStyle::Energy());
			if (!bCan && !Info.RespecWhyNot.IsEmpty())
			{
				Label(Info.RespecWhyNot, 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Medium)
					.Paint(Out, L, P, FVector2f(T.Box.Max.X, T.Box.Max.Y + 7));
			}
		}
		else if (T.Row == RowClose)
		{
			PaintButton(Out, L, P, T, TEXT("ESC"), false, FAcHudStyle::Cyan());
		}
	}
	// The rule under the header.
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.18f));
}

void SAcSkillScreen::PaintTree(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	const int64 Level = G ? G->level() : 1;
	const float Top = TreeBox.Min.Y;
	const float TreeX = Pad + GateW;
	const float RowsTop = Top + BranchHeadH;

	// The gates down the left, a band a row.
	for (int32 Row = 0; Row < 5; ++Row)
	{
		const int64 Tier = Row + 1;
		const ac::Gate Gate = ac::Progression::gate(Tier);
		const bool bOpen = Level >= Gate.level;
		const float Y = RowsTop + Row * (TileH + RowGap);
		Box(Out, L, P, FBox2f(FVector2f(Pad, Y - 4), FVector2f(TreeBox.Max.X, Y + TileH + 4)),
			FAcHudStyle::Srgb(0.35, 0.85, 1, Row % 2 == 0 ? 0.03f : 0.015f));
		const FLinearColor Head = bOpen ? FAcHudStyle::Ice() : Dim();
		Label(Tier == ac::Progression::capstone ? TEXT("CAPSTONE") : FString::Printf(TEXT("TIER %lld"), (long long)Tier), 14, Head,
			EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 1, P, FVector2f(Pad + 4, Y + 10));
		Label(FString::Printf(TEXT("LEVEL %lld"), (long long)Gate.level), 12, bOpen ? FAcHudStyle::LevelGold() : Faint(), EAcHAlign::Left,
			EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 1, P, FVector2f(Pad + 4, Y + 32));
		if (Gate.points > 0)
		{
			Label(FString::Printf(TEXT("%lld IN BRANCH"), (long long)Gate.points), 11, bOpen ? Ink() : Faint(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::SemiBold, 0.5f).Paint(Out, L + 1, P, FVector2f(Pad + 4, Y + 50));
		}
	}

	// The branches: a head, a spine, the tiles.
	for (int32 Column = 0; Column < 5; ++Column)
	{
		const ac::Branch B = ac::Branch(Column);
		const FLinearColor C = BranchColor(B);
		const float ColX = TreeX + Column * (ColW + ColGap);
		const int64 Spent = G ? G->spent(B) : 0;
		const int64 Full = ac::Progression::branchPoints(B);
		Label(Upper(ac::Progression::branch(B).title), 16, Spent > 0 ? C : FAcHudStyle::Blend(C, 0.45, Dim()), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::Bold, 1.4f).Paint(Out, L + 1, P, FVector2f(ColX + 2, Top + 4));
		Label(FString::Printf(TEXT("%lld"), (long long)Spent), 22, Spent > 0 ? FAcHudStyle::Text() : Faint(), EAcHAlign::Right, EAcVAlign::Top,
			EAcFontWeight::Bold).Paint(Out, L + 1, P, FVector2f(ColX + ColW - 2, Top + 1));
		// The branch's fill: a bar of its 37 points.
		const FBox2f Bar(FVector2f(ColX + 2, Top + 28), FVector2f(ColX + ColW - 2, Top + 31));
		Box(Out, L + 1, P, Bar, FAcHudStyle::Srgb(0.08, 0.12, 0.17, 1));
		const FBox2f Lit(Bar.Min, FVector2f(Bar.Min.X + Bar.GetSize().X * float(Spent) / float(FMath::Max<int64>(1, Full)), Bar.Max.Y));
		if (Spent > 0)
		{
			Glow(Out, L + 1, P, Lit, WithAlpha(C, 0.35f), 5);
			Box(Out, L + 2, P, Lit, C);
		}
		// The spine down the column, lit as far as the branch is open.
		int32 OpenRows = 0;
		for (int32 Row = 0; Row < 5; ++Row)
		{
			const ac::Gate Gate = ac::Progression::gate(Row + 1);
			if (Level >= Gate.level && (G ? G->spentBelow(B, Row + 1) : 0) >= Gate.points) OpenRows = Row + 1;
		}
		const float SpineX = ColX + ColW / 2;
		const float Y0 = RowsTop + TileH / 2, Y1 = RowsTop + 4 * (TileH + RowGap) + TileH / 2;
		Line(Out, L + 1, P, FVector2f(SpineX, Y0), FVector2f(SpineX, Y1), FAcHudStyle::Srgb(0.2, 0.27, 0.34, 0.7f), 2);
		if (OpenRows > 1)
		{
			const float YLit = RowsTop + (OpenRows - 1) * (TileH + RowGap) + TileH / 2;
			Line(Out, L + 1, P, FVector2f(SpineX, Y0), FVector2f(SpineX, YLit), WithAlpha(C, 0.75f), 2);
		}
		// A row the branch has not opened yet is dimmed, with what it waits for.
		for (int32 Row = OpenRows; Row < 5; ++Row)
		{
			const float Y = RowsTop + Row * (TileH + RowGap);
			const ac::Gate Gate = ac::Progression::gate(Row + 1);
			if (Level >= Gate.level && Row > 0)
			{
				const int64 Need = Gate.points - (G ? G->spentBelow(B, Row + 1) : 0);
				Label(FString::Printf(TEXT("%lld MORE"), (long long)Need), 10, FAcHudStyle::Blend(C, 0.4, Dim()), EAcHAlign::Center,
					EAcVAlign::Center, EAcFontWeight::SemiBold, 0.8f).Paint(Out, L + 8, P, FVector2f(SpineX, Y - RowGap / 2 - 0.5f));
			}
		}
	}

	for (const auto& [Skill, Tile] : Tiles) PaintTile(Out, L + 2, P, Skill, Tile);
}

void SAcSkillScreen::PaintTile(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const ac::Skill Skill, const FBox2f& B) const
{
	const ac::SkillNode& N = ac::Progression::node(Skill);
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	const FLinearColor C = BranchColor(N.branch);
	const int64 Rank = G ? G->rank(Skill) : 0;
	const bool bFull = Rank >= N.ranks;
	const std::optional<std::string> Why = G ? G->spendRefusal(Skill) : std::optional<std::string>("");
	// Open: a point could go in (or would, with a point to spend).
	const bool bOpen = G && (!Why || *Why == "No point to spend");
	const bool bLearned = Rank > 0;
	const bool bPicked = Skill == Pick;
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& T) { return T.Row == RowTile && T.Index == int32(Skill); });
	const bool bHover = Index == Hovered && !Dragging;
	const bool bCap = N.tier == ac::Progression::capstone;
	const double Now = Clock();
	const auto Flash = Flashes.find(Skill);
	const float Flashing = Flash != Flashes.end() ? FMath::Clamp(1.f - float(Now - Flash->second) / float(FlashSeconds), 0.f, 1.f) : 0.f;

	FLinearColor Fill = FAcHudStyle::Srgb(0.03, 0.05, 0.08, 1);
	FLinearColor Rim = FAcHudStyle::Srgb(0.18, 0.23, 0.29, 1);
	FLinearColor IconColor = FAcHudStyle::Srgb(0.3, 0.36, 0.42, 1);
	FLinearColor Name = Faint();
	if (bOpen && !bLearned)
	{
		Fill = FAcHudStyle::Srgb(0.035, 0.07, 0.11, 1);
		Rim = WithAlpha(C, 0.8f);
		IconColor = Ink();
		Name = Ink();
	}
	if (bLearned)
	{
		Fill = FAcHudStyle::Blend(FAcHudStyle::Srgb(0.03, 0.05, 0.08, 1), bFull ? 0.3 : 0.18, C);
		Rim = C;
		IconColor = bFull ? FAcHudStyle::Blend(C, 0.45, FLinearColor::White) : C;
		Name = FAcHudStyle::Text();
	}
	// The glows: learned ones softly, the hovered and picked brighter, a learn flash brightest.
	if (bLearned) Glow(Out, L, P, B, WithAlpha(C, bFull ? 0.32f : 0.2f), 10);
	if (bHover || bPicked) Glow(Out, L, P, B, WithAlpha(bPicked ? FAcHudStyle::Ice() : C, bPicked ? 0.42f : 0.3f), bPicked ? 14 : 11);
	if (Flashing > 0) Glow(Out, L, P, B, FLinearColor(1, 1, 1, 0.8f * Flashing), 8 + 22 * (1 - Flashing));
	Box(Out, L + 1, P, B, Fill);
	if (bCap)
	{
		// A capstone's tile is framed twice.
		CutFrame(Out, L + 2, P, B.ExpandBy(3), WithAlpha(Rim, Rim.A * 0.55f), 1, 8);
	}
	CutFrame(Out, L + 2, P, B, bHover && !bLearned ? FAcHudStyle::Blend(Rim, 0.4, FLinearColor::White) : Rim, bLearned || bOpen ? 1.5f : 1.0f, 7);
	if (bPicked)
	{
		CutFrame(Out, L + 3, P, B.ExpandBy(4), FAcHudStyle::Ice(), 1.5f, 9);
	}
	// An active is marked with a little key corner.
	if (N.active)
	{
		const TArray<FVector2f> Corner{FVector2f(B.Max.X - 13, B.Min.Y + 1), FVector2f(B.Max.X - 1, B.Min.Y + 1), FVector2f(B.Max.X - 1, B.Min.Y + 13)};
		FSlateDrawElement::MakeLines(Out, L + 3, P.ToPaintGeometry(), Corner, ESlateDrawEffect::None,
			bLearned ? C : WithAlpha(IconColor, 0.7f), true, 2);
		if (G)
		{
			if (const std::optional<int64_t> S = G->slotOf(Skill))
			{
				Label(Text(ac::Progression::slotKey(*S)), 11, FAcHudStyle::KeyYellow(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold)
					.Paint(Out, L + 4, P, B.Min + FVector2f(5, 4));
			}
		}
	}
	// The icon.
	const float IconSize = 36;
	const FBox2f IconBox(FVector2f(B.GetCenter().X - IconSize / 2, B.Min.Y + 7), FVector2f(B.GetCenter().X + IconSize / 2, B.Min.Y + 7 + IconSize));
	Image(Out, L + 3, P, Icon(Skill), IconBox, Flashing > 0 ? FAcHudStyle::Blend(IconColor, Flashing, FLinearColor::White) : IconColor);
	// The name, on up to two lines.
	TArray<FString> Lines = Wrap(Text(N.title), 11, TileW - 4, EAcFontWeight::SemiBold);
	if (Lines.Num() > 2) Lines.SetNum(2);
	const float NameY = Lines.Num() > 1 ? B.Min.Y + 46 : B.Min.Y + 52;
	for (int32 I = 0; I < Lines.Num(); ++I)
	{
		Label(Lines[I], 11, Name, EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 3, P,
			FVector2f(B.GetCenter().X, NameY + I * 11));
	}
	// The rank pips along the foot.
	const float PipW = N.ranks > 3 ? 7 : 9, PipGap = 2.5f;
	const float RowW = N.ranks * PipW + (N.ranks - 1) * PipGap;
	for (int64 R = 0; R < N.ranks; ++R)
	{
		const FBox2f Pip(FVector2f(B.GetCenter().X - RowW / 2 + R * (PipW + PipGap), B.Max.Y - 7),
			FVector2f(B.GetCenter().X - RowW / 2 + R * (PipW + PipGap) + PipW, B.Max.Y - 4));
		Box(Out, L + 3, P, Pip, R < Rank ? (bFull ? FAcHudStyle::Blend(C, 0.4, FLinearColor::White) : C) : FAcHudStyle::Srgb(0.16, 0.21, 0.27, 1));
	}
}

void SAcSkillScreen::PaintDetail(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const ac::SkillNode& N = ac::Progression::node(Pick);
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	const FLinearColor C = BranchColor(N.branch);
	const int64 Rank = G ? G->rank(Pick) : 0;
	const FBox2f D = DetailBox;
	// The panel: a darker glass, a branch-coloured edge.
	Box(Out, L, P, D, FAcHudStyle::Srgb(0.015, 0.028, 0.05, 0.96));
	CutFrame(Out, L + 1, P, D, Steel(), 1, 12);
	Box(Out, L + 1, P, FBox2f(D.Min + FVector2f(12, 0), FVector2f(D.Max.X - 12, D.Min.Y + 2)), C);

	float Y = D.Min.Y + 18;
	// The icon, big, in its frame.
	const FBox2f IconFrame(FVector2f(D.Min.X + 16, Y), FVector2f(D.Min.X + 16 + 72, Y + 72));
	Glow(Out, L + 1, P, IconFrame, WithAlpha(C, Rank > 0 ? 0.4f : 0.15f), 14);
	Box(Out, L + 2, P, IconFrame, FAcHudStyle::Blend(FAcHudStyle::Srgb(0.03, 0.05, 0.08, 1), Rank > 0 ? 0.25 : 0.08, C));
	CutFrame(Out, L + 3, P, IconFrame, C, 1.5f, 9);
	Image(Out, L + 3, P, Icon(Pick), IconFrame.ExpandBy(-10), Rank > 0 ? FAcHudStyle::Blend(C, 0.3, FLinearColor::White) : Ink());
	const float TX = IconFrame.Max.X + 14;
	const TArray<FString> TitleLines = Wrap(Text(N.title), 24, D.Max.X - 16 - TX, EAcFontWeight::Bold);
	float TY = Y + 2;
	for (const FString& T : TitleLines)
	{
		Label(T, 24, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(TX, TY));
		TY += 24;
	}
	const FString Where = N.tier == ac::Progression::capstone
		? FString::Printf(TEXT("%s · CAPSTONE"), *Upper(ac::Progression::branch(N.branch).title))
		: FString::Printf(TEXT("%s · TIER %lld"), *Upper(ac::Progression::branch(N.branch).title), (long long)N.tier);
	Label(Where, 12, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 2, P, FVector2f(TX, TY + 4));
	const FString Kind = N.active ? FString::Printf(TEXT("ACTIVE · %s COOLDOWN"), *Seconds(N.active->cooldown).ToUpper())
		: N.ranks == 1 ? FString(TEXT("PASSIVE")) : FString::Printf(TEXT("PASSIVE · %lld RANKS"), (long long)N.ranks);
	Label(Kind, 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f).Paint(Out, L + 2, P, FVector2f(TX, TY + 22));
	Y = FMath::Max(IconFrame.Max.Y, TY + 36) + 16;

	// What it does.
	for (const FString& Line2 : Wrap(Text(N.text), 15, D.GetSize().X - 32))
	{
		Label(Line2, 15, Ink(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(D.Min.X + 16, Y));
		Y += 19;
	}
	Y += 12;

	// Each rank: learned lit, the next marked, the rest grey.
	if (N.ranks > 1)
	{
		Label(FString::Printf(TEXT("RANK %lld OF %lld"), (long long)Rank, (long long)N.ranks), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::Bold, 1.0f).Paint(Out, L + 2, P, FVector2f(D.Min.X + 16, Y));
		Y += 22;
		for (int64 R = 1; R <= N.ranks; ++R)
		{
			const bool bHave = R <= Rank;
			const bool bNext = R == Rank + 1;
			const FBox2f Row(FVector2f(D.Min.X + 12, Y - 3), FVector2f(D.Max.X - 12, Y + 19));
			if (bNext) Box(Out, L + 1, P, Row, FAcHudStyle::Srgb(0.35, 0.85, 1, 0.07f));
			const FBox2f Mark(FVector2f(D.Min.X + 18, Y + 3), FVector2f(D.Min.X + 28, Y + 13));
			if (bHave)
			{
				Box(Out, L + 2, P, Mark, C);
			}
			else
			{
				Frame(Out, L + 2, P, Mark, bNext ? FAcHudStyle::Ice() : Faint(), 1.5f);
			}
			Label(Text(ac::Progression::rankText(Pick, R)), 14, bHave ? FAcHudStyle::Text() : bNext ? FAcHudStyle::Ice() : Dim(), EAcHAlign::Left,
				EAcVAlign::Top, bHave || bNext ? EAcFontWeight::SemiBold : EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(D.Min.X + 38, Y + 1));
			if (bNext)
			{
				Label(TEXT("NEXT"), 10, FAcHudStyle::KeyYellow(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
					.Paint(Out, L + 2, P, FVector2f(D.Max.X - 18, Y + 3));
			}
			Y += 24;
		}
		Y += 6;
	}

	// The state: why not, or how to.
	const FString Why = WhyNot(Pick);
	const float StateY = D.Max.Y - 16 - ButtonH - 12;
	FString State;
	FLinearColor StateColor = Dim();
	if (!Refusal.IsEmpty())
	{
		State = Refusal;
		StateColor = Refused();
	}
	else if (G && N.active && Rank > 0)
	{
		const std::optional<int64_t> S = G->slotOf(Pick);
		State = S ? FString::Printf(TEXT("In slot %s. Q, C or G moves it; its own key takes it out."), *Text(ac::Progression::slotKey(*S)))
				  : FString(TEXT("Press Q, C or G to put it in a slot, or drag it there."));
		StateColor = FAcHudStyle::KeyYellow();
	}
	else if (!Why.IsEmpty())
	{
		State = Why;
		StateColor = Why.StartsWith(TEXT("Full")) ? Ready() : FAcHudStyle::Amber();
	}
	else if (G)
	{
		State = FString::Printf(TEXT("Click or Return: 1 of your %lld points."), (long long)G->unspent());
		StateColor = FAcHudStyle::Soft();
	}
	const TArray<FString> StateLines = Wrap(State, 14, D.GetSize().X - 32);
	float SY = StateY - (StateLines.Num() - 1) * 18 - 8;
	for (const FString& S : StateLines)
	{
		Label(S, 14, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(D.Min.X + 16, SY));
		SY += 18;
	}

	// LEARN and UNDO.
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowLearn)
		{
			PaintButton(Out, L + 2, P, T, Rank >= N.ranks ? FString(TEXT("LEARNED")) : FString(TEXT("LEARN")), Why.IsEmpty(), C);
		}
		else if (T.Row == RowUndo)
		{
			const auto Left = Info.UndoLeft.find(Pick);
			const bool bCan = Left != Info.UndoLeft.end();
			PaintButton(Out, L + 2, P, T, bCan ? FString::Printf(TEXT("UNDO %s"), *Seconds(Left->second).ToUpper()) : FString(TEXT("UNDO")), bCan,
				FAcHudStyle::Amber());
		}
	}
}

void SAcSkillScreen::PaintSlots(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	const int64 Level = G ? G->level() : 1;
	Box(Out, L, P, FBox2f(FVector2f(Pad, SlotBox.Min.Y - 10), FVector2f(PanelW - Pad, SlotBox.Min.Y - 9)), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.14f));
	Label(TEXT("ACTIVES"), 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f)
		.Paint(Out, L, P, FVector2f(Pad + 4, SlotBox.Min.Y + 12));
	Label(TEXT("Q · C · G"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(Pad + 4, SlotBox.Min.Y + 34));
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		const FTarget& T = Targets[I];
		if (T.Row != RowSlot) continue;
		const int32 S = T.Index;
		const bool bOpen = Level >= ac::Progression::slotLevel(S);
		const std::optional<ac::Skill> In = G ? G->slots[S] : std::nullopt;
		const bool bHover = I == Hovered;
		const bool bTarget = Dragging && bOpen && bHover;
		const FLinearColor C = In ? BranchColor(ac::Progression::node(*In).branch) : FAcHudStyle::Cyan();
		if (bTarget || (bHover && bOpen)) Glow(Out, L, P, T.Box, WithAlpha(C, bTarget ? 0.5f : 0.25f), 12);
		Box(Out, L + 1, P, T.Box, bOpen ? FAcHudStyle::Srgb(0.03, 0.055, 0.09, 1) : FAcHudStyle::Srgb(0.025, 0.035, 0.05, 1));
		CutFrame(Out, L + 2, P, T.Box, bOpen ? (In ? C : Steel()) : Faint(), bOpen ? 1.5f : 1.0f, 8);
		// The key cap.
		const FBox2f Cap(FVector2f(T.Box.Min.X + 10, T.Box.GetCenter().Y - 15), FVector2f(T.Box.Min.X + 40, T.Box.GetCenter().Y + 15));
		Box(Out, L + 2, P, Cap, bOpen ? FAcHudStyle::Srgb(0.12, 0.1, 0.04, 1) : FAcHudStyle::Srgb(0.06, 0.07, 0.08, 1));
		Frame(Out, L + 3, P, Cap, bOpen ? FAcHudStyle::KeyYellow() : Faint(), 1.5f);
		Label(Text(ac::Progression::slotKey(S)), 19, bOpen ? FAcHudStyle::KeyYellow() : Faint(), EAcHAlign::Center, EAcVAlign::Center)
			.Paint(Out, L + 3, P, Cap.GetCenter());
		const float X = Cap.Max.X + 12;
		if (!bOpen)
		{
			Label(TEXT("CLOSED"), 15, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 3, P, FVector2f(X, T.Box.Min.Y + 11));
			Label(FString::Printf(TEXT("Opens at level %lld"), (long long)ac::Progression::slotLevel(S)), 13, Faint(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 3, P, FVector2f(X, T.Box.Min.Y + 31));
		}
		else if (In)
		{
			const FBox2f IconBox(FVector2f(X, T.Box.GetCenter().Y - 17), FVector2f(X + 34, T.Box.GetCenter().Y + 17));
			Image(Out, L + 3, P, Icon(*In), IconBox, C);
			Label(Text(ac::Progression::node(*In).title), 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold)
				.Paint(Out, L + 3, P, FVector2f(IconBox.Max.X + 10, T.Box.Min.Y + 10));
			Label(FString::Printf(TEXT("%s cooldown"), *Seconds(ac::Progression::node(*In).active->cooldown)), 12, Dim(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 3, P, FVector2f(IconBox.Max.X + 10, T.Box.Min.Y + 31));
		}
		else
		{
			Label(TEXT("EMPTY"), 15, Ink(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 3, P, FVector2f(X, T.Box.Min.Y + 11));
			Label(TEXT("Pick a learned active, press its key"), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L + 3, P, FVector2f(X, T.Box.Min.Y + 31));
		}
	}
	// The keys, on the right.
	const float KX = PanelW - Pad - 4;
	Label(TEXT("CLICK OR RETURN LEARNS · ARROWS MOVE · BACKSPACE UNDOES"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, SlotBox.Min.Y + 14));
	Label(TEXT("TAB BLUEPRINTS · J RIG · K OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, SlotBox.Min.Y + 34));
}

void SAcSkillScreen::PaintBlueprints(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const ac::ProspectorProgress* G = Info.Progress ? &*Info.Progress : nullptr;
	const int64 Level = G ? G->level() : 1;
	const auto& Kinds = ac::Progression::blueprintKinds();
	int32 Found = 0;
	for (const ac::StructureKind K : Kinds) Found += G && G->copies(K) > 0 ? 1 : 0;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		const FTarget& T = Targets[I];
		if (T.Row != RowCard) continue;
		const bool bHeldBack = T.Index >= (int32)Kinds.size();
		const ac::StructureKind K = bHeldBack ? ac::StructureKind::garrison : Kinds[T.Index];
		// C2: held means a copy or more (each builds once).
		const bool bFound = !bHeldBack && G && G->copies(K) > 0;
		const int64 Floor = FMath::Max<int64>(1, ac::Progression::blueprintFloor(K) - Info.FloorCut);
		const bool bReady = bFound && Level >= Floor;
		const bool bHover = I == Hovered;
		const FLinearColor Accent = bReady ? Ready() : bFound ? FAcHudStyle::Amber() : Dim();
		const FBox2f B = T.Box;
		if (bReady) Glow(Out, L, P, B, WithAlpha(Ready(), bHover ? 0.3f : 0.16f), 14);
		Box(Out, L + 1, P, B, bFound ? FAcHudStyle::Srgb(0.03, 0.055, 0.085, 1) : FAcHudStyle::Srgb(0.022, 0.032, 0.048, 1));
		CutFrame(Out, L + 2, P, B, bFound ? WithAlpha(Accent, 0.9f) : (bHover ? Steel() : Faint()), bFound ? 1.5f : 1.0f, 12);
		// The building's icon: lit when found, a dark shape with a "?" when not.
		const float S = B.GetSize().Y - 36;
		const FBox2f IconBox(FVector2f(B.Min.X + 18, B.Min.Y + 18), FVector2f(B.Min.X + 18 + S, B.Min.Y + 18 + S));
		const FSlateBrush* Pic = FAcIcons::Brush(FAcConsoleInfo::StructureIconName(int32(K)));
		Box(Out, L + 2, P, IconBox, FAcHudStyle::Srgb(0.015, 0.025, 0.04, 1));
		Frame(Out, L + 3, P, IconBox, bFound ? WithAlpha(Accent, 0.5f) : Faint(), 1);
		if (bFound)
		{
			Image(Out, L + 3, P, Pic, IconBox.ExpandBy(-4), bReady ? FLinearColor::White : FLinearColor(0.75f, 0.75f, 0.75f, 1));
		}
		else
		{
			Image(Out, L + 3, P, Pic, IconBox.ExpandBy(-4), FLinearColor(0.02f, 0.03f, 0.045f, 1));
			Label(TEXT("?"), 46, Faint(), EAcHAlign::Center, EAcVAlign::Center).Paint(Out, L + 4, P, IconBox.GetCenter());
		}
		const float X = IconBox.Max.X + 18;
		const float W = B.Max.X - 16 - X;
		Label(BuildingName(K).ToUpper(), 24, bFound ? FAcHudStyle::Text() : Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f)
			.Paint(Out, L + 3, P, FVector2f(X, B.Min.Y + 20));
		FString Chip;
		if (bHeldBack) Chip = TEXT("HELD BACK");
		else if (bReady) Chip = FString::Printf(TEXT("×%lld READY · B BUILDS IT"), (long long)G->copies(K));
		else if (bFound) Chip = FString::Printf(TEXT("×%lld · OPENS AT LEVEL %lld"), (long long)G->copies(K), (long long)Floor);
		else Chip = TEXT("NONE HELD");
		Label(Chip, 13, Accent, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f).Paint(Out, L + 3, P, FVector2f(X, B.Min.Y + 52));
		if (!bHeldBack)
		{
			Label(Floor > 1 ? FString::Printf(TEXT("Floor: level %lld"), (long long)Floor) : FString(TEXT("Floor: none")), 13, Ink(),
				EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 3, P, FVector2f(X, B.Min.Y + 74));
		}
		FString From;
		if (bHeldBack) From = TEXT("No blueprint in this world: other kinds stay in the strategy game.");
		else if (bFound)
		{
			const auto Source = Info.Sources.find(K);
			From = Source != Info.Sources.end() && !Source->second.IsEmpty()
				? FString::Printf(TEXT("Found: %s. More copies at the Citadel's shop (H)."), *Source->second)
				: FString(TEXT("More copies at the Citadel's shop (H)."));
		}
		else
		{
			const auto Marked = Info.SitesMarked.find(K);
			const int32 N = Marked != Info.SitesMarked.end() ? Marked->second : 0;
			From = N > 0 ? FString::Printf(TEXT("%d %s marked on the world map, or buy a copy at the Citadel (H)."), N, N == 1 ? TEXT("site") : TEXT("sites"))
				: FString(TEXT("Buy a copy at the Citadel (H), or find one at a site."));
		}
		float FY = B.Min.Y + 96;
		for (const FString& Line2 : Wrap(From, 13, W))
		{
			Label(Line2, 13, bFound ? FAcHudStyle::Soft() : Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 3, P,
				FVector2f(X, FY));
			FY += 17;
		}
	}
	// The count, under the cards.
	Label(FString::Printf(TEXT("%d OF %d BLUEPRINTS HELD"), Found, (int32)Kinds.size()), 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::Bold, 1.4f).Paint(Out, L, P, FVector2f(Pad + 4, SlotBox.Min.Y + 12));
	Label(TEXT("A blueprint builds once. Sites give a copy; the Citadel's shop (H) sells more, dearer each time."), 13, Dim(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L, P, FVector2f(Pad + 4, SlotBox.Min.Y + 34));
	Label(TEXT("TAB RIG · K OR ESC CLOSES"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(PanelW - Pad - 4, SlotBox.Min.Y + 34));
}
