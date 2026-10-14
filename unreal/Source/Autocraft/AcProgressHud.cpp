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
		Shown = -1;
		return;
	}
	News(*Mmo);

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
	for (const ac::net::ProgressMessage::News& N : Mmo.TakeProgressNews())
	{
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
		default:
			// Others' news (contracts) have their own HUD.
			break;
		}
	}
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
