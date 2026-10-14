#include "AcProgressHud.h"

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
#include "SAcConsole.h"
#include "SAcRoot.h"
#include "SAcSkillScreen.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

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
	/// A toast stays this long (the last 0.6 s fading), seconds.
	constexpr double ToastSeconds = 4.5;
	/// At most this many toasts at once (the oldest goes).
	constexpr int32 MaxToasts = 4;
	/// A slot's square, the gap between, points.
	constexpr float Cell = 76;
	constexpr float CellGap = 14;

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

void SAcProgressHud::Toast(const FString& Title, const FString& Sub, const FLinearColor& Accent, const double Now)
{
	Notes.Insert(FNote{Title, Sub, Accent, Now}, 0);
	if (Notes.Num() > MaxToasts) Notes.SetNum(MaxToasts);
	Invalidate(EInvalidateWidgetReason::Paint);
	if (!Timer.IsValid()) Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcProgressHud::Animate));
}

EActiveTimerReturnType SAcProgressHud::Animate(double, float)
{
	const double Now = FPlatformTime::Seconds();
	Notes.RemoveAll([Now](const FNote& N) { return Now - N.At > ToastSeconds; });
	Invalidate(EInvalidateWidgetReason::Paint);
	if (!Notes.IsEmpty()) return EActiveTimerReturnType::Continue;
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
	PaintToasts(Out, Layer + 4, Geometry, Size, FPlatformTime::Seconds());
	return Layer + 8;
}

void SAcProgressHud::PaintSlots(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// Bottom centre: on the dashboard's brow over its middle face (Swift
	// points, y up, in the layout), else a fifth of the way up the view.
	const float RowW = 3 * Cell + 2 * CellGap;
	const bool bCab = Lay.Size.X > 0 && Lay.FaceTop > 0;
	const float MidX = bCab ? float(Lay.Window.MidX()) : Size.X / 2;
	const float Bottom = bCab ? Size.Y - float(Lay.FaceTop) - 22 : Size.Y * 0.8f;
	const FVector2f O(FMath::RoundHalfFromZero(MidX - RowW / 2), FMath::RoundHalfFromZero(Bottom - Cell));
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
}

void SAcProgressHud::PaintOpening(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FVector2f Size) const
{
	// Round the reticle (the view's middle): a ring that closes as the site opens.
	const FVector2f C(FMath::RoundHalfFromZero(Size.X / 2), FMath::RoundHalfFromZero(Size.Y / 2));
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
	// Under the level banner's place (74 % up the view), newest on top.
	float Y = FMath::RoundHalfFromZero(Size.Y * 0.36f);
	for (const FNote& N : Notes)
	{
		const double T = Now - N.At;
		if (T < 0 || T > ToastSeconds) continue;
		const float A = float(T < 0.2 ? T / 0.2 : T > ToastSeconds - 0.6 ? (ToastSeconds - T) / 0.6 : 1.0);
		const FAcHudText Title = Label(N.Title, 20, WithAlpha(N.Accent, A), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f);
		const FAcHudText Sub = Label(N.Sub, 14, WithAlpha(FAcHudStyle::Text(), 0.9f * A), EAcHAlign::Center, EAcVAlign::Top,
			EAcFontWeight::Medium, 0.4f);
		const float W = FMath::Max(Title.Measure().X, N.Sub.IsEmpty() ? 0.f : Sub.Measure().X) + 56;
		const float H = N.Sub.IsEmpty() ? 38 : 58;
		// Slides down into place.
		const float Drop = T < 0.2 ? float(8 * (1 - T / 0.2)) : 0;
		const FBox2f B(FVector2f(Size.X / 2 - W / 2, Y - Drop), FVector2f(Size.X / 2 + W / 2, Y - Drop + H));
		Box(Out, Layer, G, B, WithAlpha(FAcHudStyle::Glass(), 0.84f * A));
		Box(Out, Layer + 1, G, FBox2f(B.Min, FVector2f(B.Min.X + 3, B.Max.Y)), WithAlpha(N.Accent, A));
		CutFrame(Out, Layer + 1, G, B, WithAlpha(N.Accent, 0.6f * A), 1, 8);
		Title.Paint(Out, Layer + 2, G, FVector2f(B.GetCenter().X, B.Min.Y + 8));
		if (!N.Sub.IsEmpty()) Sub.Paint(Out, Layer + 2, G, FVector2f(B.GetCenter().X, B.Min.Y + 33));
		Y += H + 8;
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
	// counting the pilot's buildings of that kind in this region.
	int64 Owned = 0;
	if (const ac::net::Mirror* M = Mmo->Mirror())
	{
		for (const ac::Structure& B : M->simulation().state.structures) Owned += B.kind == Kind && B.owner == ac::Pilot::player ? 1 : 0;
	}
	const std::optional<std::string> Why = G->buildRefusal(Kind, Owned);
	return Why ? Text(*Why) : FString();
}

FString AcProgressHud::BlueprintTag(const FAcMmoClient* Mmo, const ac::StructureKind Kind)
{
	const std::optional<ac::ProspectorProgress> G = Mmo ? Progress(*Mmo) : std::nullopt;
	if (!G || BlueprintRefusal(Mmo, Kind).IsEmpty()) return FString();
	if (!G->blueprints.count(Kind)) return TEXT("NO BLUEPRINT");
	if (G->level() < G->floor(Kind)) return FString::Printf(TEXT("LV %lld"), (long long)G->floor(Kind));
	return TEXT("LIMIT");
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
	return Out;
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
		}
		if (Row.State != FAcContractRow::EState::Open)
		{
			const FString In = P->contractsDawn > Sky ? FString::Printf(TEXT(", IN %s"), *Countdown(P->contractsDawn - Sky)) : FString();
			Row.Dawn = (Row.State == FAcContractRow::EState::Done ? TEXT("NEW ONE AT DAWN") : TEXT("BACK AT DAWN")) + In;
		}
		Out.Rows.Add(Row);
	}
	return Out;
}

void UAcProgressHudSubsystem::ToggleAbandon()
{
	ArmedAt = AbandonArmed() ? -1 : FPlatformTime::Seconds();
	UE_LOG(LogAutocraft, Log, TEXT("contracts: abandon %s"), ArmedAt < 0 ? TEXT("disarmed") : TEXT("armed"));
}

bool UAcProgressHudSubsystem::AbandonArmed() const { return ArmedAt >= 0 && FPlatformTime::Seconds() - ArmedAt < 6; }

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
	// The KSTR contracts on the dashboard's right panel.
	if (UAcConsoleSubsystem* C = UAcConsoleSubsystem::Get(this); C && C->Console())
	{
		TOptional<FAcContractsInfo> Panel;
		if (Mmo->Progress() && !Mmo->Progress()->contracts.empty()) Panel = AcProgressHud::Contracts(*Mmo, AbandonArmed());
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
		Opening = V;
	}
	Hud->SetOpening(Opening);
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
		// A site opened: its blueprint comes as its own news; one already
		// known paid ore and MH instead.
		if (N.value == 1)
		{
			Hud->Toast(TEXT("BLUEPRINT ALREADY KNOWN"), FString::Printf(TEXT("+%lld ore  ·  +%lld MH"), (long long)ac::net::kSiteOre,
				(long long)ac::net::kSiteHydrogen), FAcHudStyle::Soft(), Now);
		}
		break;
	case K::contract:
	{
		// A KSTR contract done: its title from the slots.
		FString Title = TEXT("CONTRACT");
		if (P)
		{
			for (const ac::net::Contract& C : P->contracts)
			{
				if (std::to_string(C.id) == N.id) Title = Upper(C.title);
			}
		}
		FString Pay = Xp;
		if (N.value > 0) Pay += FString::Printf(TEXT("%s+%lld MH"), Pay.IsEmpty() ? TEXT("") : TEXT("  ·  "), (long long)N.value);
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
	default:
		break;
	}
}
