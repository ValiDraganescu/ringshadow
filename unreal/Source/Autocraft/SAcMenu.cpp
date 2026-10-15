#include "SAcMenu.h"

#include "AcHudStyle.h"
#include "AcHudText.h"
#include "AcSettings.h"
#include "AcLauncherArt.h"
#include "AcPilotText.h"
#include "PlayerSettings.h"

#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	// The panel, in Swift points (the HUD root's units).
	constexpr float PanelW = 440;
	constexpr float Pad = 32;
	constexpr float ButtonH = 44;
	constexpr float ButtonGap = 10;
	constexpr float CardH = 66;
	constexpr float RowH = 40;
	constexpr float ValueW = 56;
	constexpr float LookMin = 0.25f, LookMax = 3.f;
	// U2: the Settings page (its choices and the lag's warning need the
	// room) and the key card.
	constexpr float SettingsW = 520;
	constexpr float SettingsLabelW = 150;
	constexpr float HeadH = 22;
	constexpr float NoteH = 18;
	constexpr float KeyCardW = 640;
	constexpr float KeyColW = 170;
	constexpr float KeyRowH = 22;
	constexpr float KeyHeadH = 28;
	// The home screen (H1): a wider panel for the wordmark, ENTER THE REACH
	// tall, the rest shorter.
	constexpr float HomeW = 460;
	constexpr float PrimaryH = 76;
	constexpr float HomeButtonH = 40;
	constexpr float GroupGap = 26;
	constexpr float GroupHeadH = 22;
	/// X2: the away lines' block (a line and the what, in up to two).
	constexpr float AwayH = 62;

	/// Full size up to a 1000-point-high window; past it the panel grows
	/// with the window (a 2560x1600 window at 1x: 1.6), so it never shrinks
	/// to a corner of a big screen.
	float MaxScale(const FVector2f View) { return FMath::Max(1.f, View.Y / 1000.f); }

	float PanelWidth(const EAcMenuMode Mode, const bool bPage) { return Mode == EAcMenuMode::Home && !bPage ? HomeW : PanelW; }

	/// The brand's wordmark face (the website's: Exo 2 ExtraBold, cut from
	/// the variable font in Content-src/fonts), read from the file as the
	/// launcher art is; Barlow Condensed Bold when it is missing.
	FSlateFontInfo Wordmark(const float SwiftPoints)
	{
		static const FString File =
			FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Content-src/fonts/Exo2-ExtraBold.ttf")));
		static const bool bHave = IFileManager::Get().FileExists(*File);
		return bHave ? FSlateFontInfo(File, SwiftPoints * 72.0f / 96.0f) : FAcHudStyle::Font(SwiftPoints);
	}

	void Box(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C)
	{
		const FVector2f S = B.GetSize();
		FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(S), FSlateLayoutTransform(FVector2D(B.Min))),
			FAcHudStyle::White(), ESlateDrawEffect::None, C);
	}

	/// An outline `W` points wide inside `B` (square corners, as SAcNewGame).
	void Frame(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FBox2f& B, const FLinearColor& C, float W = 1)
	{
		const FVector2f S = B.GetSize();
		Box(Out, Layer, G, FBox2f(B.Min, B.Min + FVector2f(S.X, W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Min.X, B.Max.Y - W), B.Max), C);
		Box(Out, Layer, G, FBox2f(B.Min + FVector2f(0, W), FVector2f(B.Min.X + W, B.Max.Y - W)), C);
		Box(Out, Layer, G, FBox2f(FVector2f(B.Max.X - W, B.Min.Y + W), FVector2f(B.Max.X, B.Max.Y - W)), C);
	}

	FAcHudText Label(const FString& S, float Size, const FLinearColor& C, EAcHAlign H = EAcHAlign::Left,
		EAcVAlign V = EAcVAlign::Top, EAcFontWeight Weight = EAcFontWeight::Bold)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		return T;
	}
}

void SAcMenu::Construct(const FArguments& InArgs)
{
	MenuMode = InArgs._Mode;
	OnPickDelegate = InArgs._OnPick;
	OnSoundDelegate = InArgs._OnSound;
	Art = MakeShared<FAcLauncherArt>();
	// Drift and crossfade repaint every frame, only while the home art shows.
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcMenu::OnArtTimer));
}

bool SAcMenu::HasArt() const
{
	return Art.IsValid() && Art->Num() > 0 && Art->Brush(Art->Start()) != nullptr;
}

EActiveTimerReturnType SAcMenu::OnArtTimer(double, float)
{
	// A pinned image (-AcLauncherArt=N, stills) does not move.
	if (ArtShowing() && Art->Pinned() == INDEX_NONE) Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

void SAcMenu::SetNowPlaying(const FString& Title)
{
	if (Title == NowPlaying) return;
	NowPlaying = Title;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::SetGame(const FString& Title, const FString& Detail)
{
	GameTitle = Title;
	GameDetail = Detail;
	if (!bSettings) Highlighted = FMath::Clamp(Highlighted, 0, Buttons().Num() - 1);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::SetReach(const FString& Line)
{
	if (Line == ReachLine) return;
	ReachLine = Line;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::SetAway(const FString& Ran, const FString& What)
{
	if (Ran == AwayRanLine && What == AwayWhatLine) return;
	AwayRanLine = Ran;
	AwayWhatLine = What;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::SetServer(const FString& Line)
{
	if (Line == ServerLine) return;
	ServerLine = Line;
	if (!bSettings) Highlighted = FMath::Clamp(Highlighted, 0, Buttons().Num() - 1);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::SetMode(const EAcMenuMode InMode)
{
	MenuMode = InMode;
	bSettings = false;
	bKeyCard = false;
	bConfirmNewWorld = false;
	Dragging = INDEX_NONE;
	Highlighted = 0;
	Pressed = INDEX_NONE;
	Invalidate(EInvalidateWidgetReason::Paint);
}

TArray<SAcMenu::FButton> SAcMenu::Buttons() const
{
	if (bSettings) return {};
	if (bConfirmNewWorld)
	{
		return {{EAcMenuAction::MmoNewWorld, TEXT("DELETE AND START OVER")}, {EAcMenuAction::Back, TEXT("BACK")}};
	}
	if (MenuMode == EAcMenuMode::Pause)
	{
		return {{EAcMenuAction::Resume, TEXT("RESUME")}, {EAcMenuAction::Settings, TEXT("SETTINGS")},
			{EAcMenuAction::QuitToHome, TEXT("QUIT TO HOME")}};
	}
	TArray<FButton> B;
	// The MMORPG (docs/mmorpg-m1.md, the local server's world) first (H1):
	// it resumes the saved world, or starts one.
	B.Add({EAcMenuAction::Mmo, TEXT("ENTER THE REACH"), true});
	// The same, its saved world deleted first (docs/mmorpg-m3.md, S4).
	B.Add({EAcMenuAction::MmoNewWorld, TEXT("NEW WORLD")});
	// S2: the world's server, running on with the game away; STOP saves it
	// and ends it (the next ENTER starts it again).
	if (!ServerLine.IsEmpty())
	{
		FButton Stop{EAcMenuAction::StopServer, TEXT("STOP")};
		Stop.bServer = true;
		B.Add(Stop);
	}
	// The strategy game below.
	if (!GameTitle.IsEmpty()) B.Add({EAcMenuAction::Resume, TEXT("RESUME SKIRMISH"), false, false, true});
	B.Add({EAcMenuAction::NewGame, TEXT("NEW SKIRMISH"), false, false, GameTitle.IsEmpty()});
	B.Add({EAcMenuAction::Settings, TEXT("SETTINGS"), false, true});
	B.Add({EAcMenuAction::QuitApp, TEXT("QUIT"), false, true});
	return B;
}

// MARK: - Settings

int32 SAcMenu::SliderCount() { return int32(EAcSettingsRow::Look) + 1; }

FString SAcMenu::SliderName(const int32 I)
{
	static const TCHAR* Names[] = {TEXT("MASTER"), TEXT("MUSIC"), TEXT("VOICE"), TEXT("EFFECTS"), TEXT("ENVIRONMENT"), TEXT("MOUSE TURN")};
	return Names[FMath::Clamp(I, 0, 5)];
}

float SAcMenu::SliderGet(const int32 I)
{
	const UAcSettings& S = UAcSettings::Get();
	switch (EAcSettingsRow(I))
	{
	case EAcSettingsRow::Master: return FMath::Clamp(S.MasterVolume, 0.f, 1.f);
	case EAcSettingsRow::Music: return S.bMusic ? FMath::Clamp(S.MusicVolume, 0.f, 1.f) : 0.f;
	case EAcSettingsRow::Voice: return FMath::Clamp(S.VoiceVolume, 0.f, 1.f);
	case EAcSettingsRow::Effects: return FMath::Clamp(S.SfxVolume, 0.f, 1.f);
	case EAcSettingsRow::Environment: return FMath::Clamp(S.AmbientVolume, 0.f, 1.f);
	default: return FMath::Clamp((S.LookSpeed - LookMin) / (LookMax - LookMin), 0.f, 1.f);
	}
}

void SAcMenu::SliderSet(const int32 I, float F)
{
	UAcSettings& S = UAcSettings::Get();
	F = FMath::Clamp(F, 0.f, 1.f);
	switch (EAcSettingsRow(I))
	{
	case EAcSettingsRow::Master: S.MasterVolume = F; break;
	case EAcSettingsRow::Music: S.MusicVolume = F; S.bMusic = F > 0.f; break;
	case EAcSettingsRow::Voice: S.VoiceVolume = F; break;
	case EAcSettingsRow::Effects: S.SfxVolume = F; break;
	case EAcSettingsRow::Environment: S.AmbientVolume = F; break;
	default: S.LookSpeed = LookMin + F * (LookMax - LookMin); break;
	}
}

FString SAcMenu::SliderText(const int32 I)
{
	if (EAcSettingsRow(I) == EAcSettingsRow::Look) return FString::Printf(TEXT("%.2f×"), UAcSettings::Get().LookSpeed);
	return FString::Printf(TEXT("%d"), ac::PlayerSettings::percent(SliderGet(I)));
}

FString SAcMenu::ChoiceName(const EAcSettingsRow Row)
{
	switch (Row)
	{
	case EAcSettingsRow::Screen: return TEXT("SCREEN");
	case EAcSettingsRow::Resolution: return TEXT("RESOLUTION");
	default: return TEXT("LAG SIMULATOR");
	}
}

FIntPoint SAcMenu::PendingResolution() const
{
	return Resolutions.IsValidIndex(PendingRes) ? Resolutions[PendingRes] : UAcSettings::Desktop();
}

FString SAcMenu::ChoiceText(const EAcSettingsRow Row) const
{
	switch (Row)
	{
	case EAcSettingsRow::Screen: return bPendingFull ? TEXT("FULL SCREEN") : TEXT("WINDOW");
	case EAcSettingsRow::Resolution:
	{
		const FIntPoint R = PendingResolution();
		const bool bNative = R == UAcSettings::Desktop();
		return FString::Printf(TEXT("%d × %d%s"), R.X, R.Y, bNative ? TEXT("  NATIVE") : TEXT(""));
	}
	default: return UAcSettings::Get().bMmoLag ? TEXT("150 MS · 5% LOST") : TEXT("OFF");
	}
}

void SAcMenu::Choose(const EAcSettingsRow Row, const int32 Step)
{
	switch (Row)
	{
	case EAcSettingsRow::Screen:
	{
		// The other mode's list; the size kept, or the nearest it offers.
		const FIntPoint Was = PendingResolution();
		bPendingFull = !bPendingFull;
		Resolutions = UAcSettings::Resolutions(bPendingFull);
		std::vector<ac::Resolution> List;
		for (const FIntPoint& R : Resolutions) List.push_back({R.X, R.Y});
		PendingRes = ac::ScreenChoices::index(List, {Was.X, Was.Y});
		break;
	}
	case EAcSettingsRow::Resolution:
		if (Resolutions.Num() > 0) PendingRes = (FMath::Max(PendingRes, 0) + Step + Resolutions.Num()) % Resolutions.Num();
		break;
	case EAcSettingsRow::Lag:
		UAcSettings::Get().bMmoLag = !UAcSettings::Get().bMmoLag;
		UAcSettings::ApplyLag();
		break;
	default: break;
	}
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::ReadScreen()
{
	const UAcSettings& S = UAcSettings::Get();
	bPendingFull = S.GetFullscreenMode() != EWindowMode::Windowed;
	Resolutions = UAcSettings::Resolutions(bPendingFull);
	FIntPoint Now = S.GetScreenResolution();
	if (Now.X <= 0 || Now.Y <= 0 || bPendingFull && S.GetFullscreenMode() == EWindowMode::WindowedFullscreen) Now = UAcSettings::Desktop();
	std::vector<ac::Resolution> List;
	for (const FIntPoint& R : Resolutions) List.push_back({R.X, R.Y});
	PendingRes = ac::ScreenChoices::index(List, {Now.X, Now.Y});
}

bool SAcMenu::ScreenChanged() const
{
	const UAcSettings& S = UAcSettings::Get();
	const bool bFull = S.GetFullscreenMode() != EWindowMode::Windowed;
	if (bFull != bPendingFull) return true;
	FIntPoint Now = S.GetScreenResolution();
	if (Now.X <= 0 || Now.Y <= 0 || bFull && S.GetFullscreenMode() == EWindowMode::WindowedFullscreen) Now = UAcSettings::Desktop();
	return Now != PendingResolution();
}

void SAcMenu::Ok()
{
	if (ScreenChanged())
	{
		UAcSettings& S = UAcSettings::Get();
		S.SetFullscreenMode(bPendingFull ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
		S.SetScreenResolution(PendingResolution());
		UAcSettings::ApplyScreen();
	}
	Back();
}

void SAcMenu::Adjust(const int32 Step)
{
	if (!bSettings || bKeyCard) return;
	const EAcSettingsRow Row = SettingsRow();
	if (Row == EAcSettingsRow::KeyCard || Row == EAcSettingsRow::Ok)
	{
		// KEY CARD and OK share a row: Left and Right move between them.
		Highlighted = int32(Step < 0 ? EAcSettingsRow::KeyCard : EAcSettingsRow::Ok);
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	if (Highlighted >= SliderCount())
	{
		Choose(Row, Step);
		return;
	}
	if (Row == EAcSettingsRow::Look)
	{
		// The mouse turn in 0.25× steps (1/11 of its range).
		const float Unit = 0.25f / (LookMax - LookMin);
		SliderSet(Highlighted, FMath::RoundToFloat(SliderGet(Highlighted) / Unit + Step) * Unit);
	}
	else SliderSet(Highlighted, float(ac::PlayerSettings::stepVolume(SliderGet(Highlighted), Step)));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::OpenSettings()
{
	const int32 I = Buttons().IndexOfByPredicate([](const FButton& B) { return B.Action == EAcMenuAction::Settings; });
	if (I != INDEX_NONE) Pick(I);
}

void SAcMenu::OpenKeyCard()
{
	if (!bSettings) return;
	bKeyCard = true;
	Highlighted = 0;
	Pressed = INDEX_NONE;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::HighlightRow(const EAcSettingsRow Row)
{
	if (!bSettings || bKeyCard) return;
	Highlighted = int32(Row);
	Invalidate(EInvalidateWidgetReason::Paint);
}

bool SAcMenu::Highlight(const EAcMenuAction Action)
{
	const int32 I = Buttons().IndexOfByPredicate([Action](const FButton& B) { return B.Action == Action; });
	if (I == INDEX_NONE) return false;
	Highlighted = I;
	Invalidate(EInvalidateWidgetReason::Paint);
	return true;
}

bool SAcMenu::Back()
{
	if (bConfirmNewWorld)
	{
		bConfirmNewWorld = false;
		Highlight(EAcMenuAction::MmoNewWorld);
		return true;
	}
	if (!bSettings) return false;
	if (bKeyCard)
	{
		bKeyCard = false;
		Highlighted = int32(EAcSettingsRow::KeyCard);
		Invalidate(EInvalidateWidgetReason::Paint);
		return true;
	}
	if (UAcSettings::Remembers()) UAcSettings::Store();
	bSettings = false;
	Dragging = INDEX_NONE;
	Highlighted = Buttons().IndexOfByPredicate([](const FButton& B) { return B.Action == EAcMenuAction::Settings; });
	Highlighted = FMath::Max(Highlighted, 0);
	Invalidate(EInvalidateWidgetReason::Paint);
	return true;
}

void SAcMenu::DragTo(const FVector2f Local)
{
	if (!Tracks.IsValidIndex(Dragging)) return;
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	const FBox2f& T = Tracks[Dragging];
	SliderSet(Dragging, (P.X - T.Min.X) / FMath::Max(T.GetSize().X, 1.f));
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::Move(const int32 Step)
{
	const int32 N = bKeyCard ? 1 : bSettings ? int32(EAcSettingsRow::Ok) + 1 : Buttons().Num();
	// KEY CARD and OK share the last row: Down from either wraps to the top.
	if (bSettings && !bKeyCard && Step > 0 && Highlighted == int32(EAcSettingsRow::KeyCard))
	{
		Highlighted = 0;
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	if (bSettings && !bKeyCard && Step < 0 && Highlighted == int32(EAcSettingsRow::Ok))
	{
		Highlighted = int32(EAcSettingsRow::Lag);
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	Highlighted = (Highlighted + Step + N) % N;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcMenu::PickHighlighted()
{
	Pick(Highlighted);
}

void SAcMenu::Pick(const int32 Index)
{
	if (bSettings)
	{
		if (bKeyCard)
		{
			if (Index == 0)
			{
				OnSoundDelegate.ExecuteIfBound(EAcMenuAction::Back);
				Back();
			}
			return;
		}
		switch (EAcSettingsRow(Index))
		{
		case EAcSettingsRow::Screen:
		case EAcSettingsRow::Resolution:
		case EAcSettingsRow::Lag:
			OnSoundDelegate.ExecuteIfBound(EAcMenuAction::Settings);
			Choose(EAcSettingsRow(Index), 1);
			break;
		case EAcSettingsRow::KeyCard:
			OnSoundDelegate.ExecuteIfBound(EAcMenuAction::Settings);
			OpenKeyCard();
			break;
		case EAcSettingsRow::Ok:
			OnSoundDelegate.ExecuteIfBound(EAcMenuAction::Back);
			Ok();
			break;
		default: break;
		}
		return;
	}
	const TArray<FButton> B = Buttons();
	if (!B.IsValidIndex(Index)) return;
	// A sound for every pick (the flow picks which by the action).
	OnSoundDelegate.ExecuteIfBound(B[Index].Action);
	if (B[Index].Action == EAcMenuAction::Back)
	{
		Back();
		return;
	}
	if (B[Index].Action == EAcMenuAction::MmoNewWorld && !bConfirmNewWorld)
	{
		// Ask first: it deletes the saved world. BACK is the safe default.
		bConfirmNewWorld = true;
		Highlighted = 1;
		Pressed = INDEX_NONE;
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	bConfirmNewWorld = false;
	if (B[Index].Action == EAcMenuAction::Settings)
	{
		bSettings = true;
		bKeyCard = false;
		ReadScreen();
		Highlighted = 0;
		Invalidate(EInvalidateWidgetReason::Paint);
		return;
	}
	OnPickDelegate.ExecuteIfBound(B[Index].Action);
}

// MARK: - Layout

void SAcMenu::Layout(const FVector2f ViewSize) const
{
	Boxes.Reset();
	Tracks.Reset();
	Headings.Reset();
	WarnAt = ScreenNoteAt = FVector2f::ZeroVector;
	auto Fit = [&](const float W, const float Y) {
		PanelSize = FVector2f(W, Y);
		PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 40) / PanelSize.X, (ViewSize.Y - 40) / PanelSize.Y), 0.3f, MaxScale(ViewSize));
		PanelOrigin = (ViewSize - PanelSize * PanelScale) * 0.5f;
		if (ArtShowing()) PanelOrigin.X = FMath::Min(ViewSize.X * 0.08f, ViewSize.X - PanelSize.X * PanelScale - 20.f);
	};
	if (bKeyCard)
	{
		// The groups' rows (painted from the same list), then BACK.
		float Y = 84 + NoteH + 8;
		for (const AcPilotText::FKeyGroup& G : AcPilotText::KeyCard()) Y += KeyHeadH + G.Rows.Num() * KeyRowH;
		Y += 14;
		Boxes.Add(FBox2f(FVector2f(Pad, Y), FVector2f(KeyCardW - Pad, Y + ButtonH)));
		Fit(KeyCardW, Y + ButtonH + Pad);
		return;
	}
	if (bSettings)
	{
		const float W = SettingsW;
		float Y = 84;
		auto Head = [&](const TCHAR* Title) {
			Headings.Add({Title, FVector2f(Pad, Y)});
			Y += HeadH;
		};
		auto Row = [&] {
			Boxes.Add(FBox2f(FVector2f(Pad, Y), FVector2f(W - Pad, Y + RowH)));
			Y += RowH + 4;
		};
		for (int32 I = 0; I < SliderCount(); ++I)
		{
			if (I == int32(EAcSettingsRow::Master)) Head(TEXT("SOUND"));
			if (I == int32(EAcSettingsRow::Look))
			{
				Y += 8;
				Head(TEXT("CONTROLS"));
			}
			const float Mid = Y + RowH / 2;
			Tracks.Add(FBox2f(FVector2f(Pad + SettingsLabelW, Mid - 3), FVector2f(W - Pad - ValueW, Mid + 3)));
			Row();
		}
		Y += 8;
		Head(TEXT("SCREEN"));
		Row();
		Row();
		ScreenNoteAt = FVector2f(Pad + 2, Y - 2);
		Y += NoteH;
		Y += 8;
		Head(TEXT("TESTING"));
		Row();
		WarnAt = FVector2f(Pad + 2, Y - 2);
		Y += NoteH + 14;
		// KEY CARD and OK, halves of one row.
		const float Mid = W * 0.5f;
		Boxes.Add(FBox2f(FVector2f(Pad, Y), FVector2f(Mid - ButtonGap * 0.5f, Y + ButtonH)));
		Boxes.Add(FBox2f(FVector2f(Mid + ButtonGap * 0.5f, Y), FVector2f(W - Pad, Y + ButtonH)));
		Fit(W, Y + ButtonH + Pad);
		return;
	}
	const bool bHome = MenuMode == EAcMenuMode::Home && !bConfirmNewWorld;
	const float W = PanelWidth(MenuMode, bConfirmNewWorld);
	float Y = bHome ? 132 : 84;
	CardBox = FBox2f(FVector2f::ZeroVector, FVector2f::ZeroVector);
	GroupAt = FVector2f::ZeroVector;
	AwayAt = FVector2f::ZeroVector;
	if (bConfirmNewWorld) Y = 84 + 3 * 22 + 18;
	else if (!bHome && !GameTitle.IsEmpty())
	{
		CardBox = FBox2f(FVector2f(Pad, Y), FVector2f(W - Pad, Y + CardH));
		Y += CardH + 22;
	}
	const TArray<FButton> B = Buttons();
	for (int32 I = 0; I < B.Num(); ++I)
	{
		if (bHome && B[I].bGroup)
		{
			// The strategy game: its heading, then its card when there is a game.
			Y += GroupGap - ButtonGap;
			GroupAt = FVector2f(Pad, Y);
			Y += GroupHeadH;
			if (!GameTitle.IsEmpty())
			{
				CardBox = FBox2f(FVector2f(Pad, Y), FVector2f(W - Pad, Y + CardH));
				Y += CardH + ButtonGap;
			}
		}
		const float H = !bHome ? ButtonH : B[I].bPrimary ? PrimaryH : HomeButtonH;
		if (B[I].bHalf && I > 0 && B[I - 1].bHalf)
		{
			// The second half of the row above.
			FBox2f& Left = Boxes.Last();
			const float Mid = (Pad + W - Pad) * 0.5f;
			Left.Max.X = Mid - ButtonGap * 0.5f;
			Boxes.Add(FBox2f(FVector2f(Mid + ButtonGap * 0.5f, Left.Min.Y), FVector2f(W - Pad, Left.Max.Y)));
			continue;
		}
		if (bHome && B[I].bHalf) Y += GroupGap - ButtonGap;
		Boxes.Add(FBox2f(FVector2f(Pad, Y), FVector2f(W - Pad, Y + H)));
		Y += H + ButtonGap;
		// X2: the away lines under the way in.
		if (bHome && B[I].bPrimary && !AwayRanLine.IsEmpty())
		{
			AwayAt = FVector2f(Pad + 2, Y - 2);
			Y += AwayH;
		}
	}
	PanelSize = FVector2f(W, Y - ButtonGap + Pad);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 40) / PanelSize.X, (ViewSize.Y - 40) / PanelSize.Y), 0.3f, MaxScale(ViewSize));
	PanelOrigin = (ViewSize - PanelSize * PanelScale) * 0.5f;
	if (ArtShowing()) PanelOrigin.X = FMath::Min(ViewSize.X * 0.08f, ViewSize.X - PanelSize.X * PanelScale - 20.f);
}

int32 SAcMenu::ButtonAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Boxes.Num(); ++I)
	{
		if (Boxes[I].IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input (modal)

FReply SAcMenu::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
		Pressed = ButtonAt(Local);
		Invalidate(EInvalidateWidgetReason::Paint);
		if (bSettings && !bKeyCard && Pressed != INDEX_NONE && Pressed < SliderCount())
		{
			// A slider row: the value under the pointer, then dragged.
			Highlighted = Dragging = Pressed;
			Pressed = INDEX_NONE;
			DragTo(Local);
			return FReply::Handled().CaptureMouse(SharedThis(this));
		}
	}
	return FReply::Handled();
}

FReply SAcMenu::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Dragging != INDEX_NONE)
	{
		Dragging = INDEX_NONE;
		return FReply::Handled().ReleaseMouseCapture();
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Pressed != INDEX_NONE)
	{
		const int32 Hit = ButtonAt(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
		const int32 Was = Pressed;
		Pressed = INDEX_NONE;
		Invalidate(EInvalidateWidgetReason::Paint);
		const bool bChoice = bSettings && !bKeyCard && Hit >= int32(EAcSettingsRow::Screen) && Hit <= int32(EAcSettingsRow::Lag);
		if (Hit == Was && bChoice && Boxes.IsValidIndex(Hit))
		{
			// A choice: its left half steps back, its right half on.
			const FVector2f P = (FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())) - PanelOrigin) / PanelScale;
			const float Split = Boxes[Hit].Min.X + SettingsLabelW + (Boxes[Hit].Max.X - Boxes[Hit].Min.X - SettingsLabelW) * 0.5f;
			OnSoundDelegate.ExecuteIfBound(EAcMenuAction::Settings);
			Choose(EAcSettingsRow(Hit), P.X < Split ? -1 : 1);
		}
		else if (Hit == Was) Pick(Hit);
	}
	return FReply::Handled();
}

FReply SAcMenu::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Dragging != INDEX_NONE)
	{
		DragTo(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
		return FReply::Handled();
	}
	const int32 Hit = ButtonAt(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())));
	if (Hit != INDEX_NONE && Hit != Highlighted)
	{
		Highlighted = Hit;
		Invalidate(EInvalidateWidgetReason::Paint);
	}
	return FReply::Handled();
}

void SAcMenu::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Pressed = INDEX_NONE;
}

FCursorReply SAcMenu::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	const bool bOver = ButtonAt(FVector2f(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()))) != INDEX_NONE;
	return FCursorReply::Cursor(bOver ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

namespace
{
	constexpr double ArtSlot = 12.5;   // an image takes over every 12.5 s ...
	constexpr double ArtFade = 1.5;    // ... fading in over 1.5 s, so each is on screen 14 s
	constexpr float ArtZoom = 0.06f;
	constexpr float ArtPan = 0.025f;
	constexpr double ArtSwing = 60.0;  // a lone image: in and back out in a minute
}

/// The key art, aspect-fill with a slow zoom and pan, crossfading to the next; then the
/// left shade, the vignette and the now-playing line.
void SAcMenu::PaintArt(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& G, const FVector2f View) const
{
	// The engine's clock: real time in play, 1/30 s a frame in a fixed-step recording.
	const double Now = FApp::GetCurrentTime();
	if (ArtStart < 0) ArtStart = Now;
	const bool bPinned = Art->Pinned() != INDEX_NONE;
	const double E = bPinned ? 0.0 : Now - ArtStart;
	const int32 N = Art->Num();
	// One image (H1's key art) drifts in and out over ArtSwing and never cuts.
	const bool bOne = N == 1;
	const int32 Idx = bOne ? Art->Start() : Art->Start() + (int32)FMath::FloorToDouble(E / ArtSlot);
	const double Local = bOne ? E : E - FMath::FloorToDouble(E / ArtSlot) * ArtSlot;

	Out.PushClip(FSlateClippingZone(G.GetLayoutBoundingRect()));
	// `Seen`: seconds this image has been up; 14 s are drifted through.
	auto Draw = [&](const int32 I, const double Seen, const float Alpha, const int32 Sub)
	{
		const FSlateBrush* B = Art->Brush(I);
		if (!B) return;
		const FVector2f Px = B->ImageSize;
		const float Prog = bPinned ? 0.3f
			: bOne                 ? (float)(0.5 - 0.5 * FMath::Cos(2 * PI * Seen / ArtSwing))
			                       : (float)FMath::Clamp(Seen / (ArtSlot + ArtFade), 0.0, 1.0);
		const float Cover = FMath::Max(View.X / Px.X, View.Y / Px.Y) * (1.f + ArtZoom * Prog);
		const FVector2f Size = Px * Cover;
		// Alternate the pan direction by image; it never exceeds the crop the zoom leaves.
		const float Dir = (I % 2 == 0) ? 1.f : -1.f;
		FVector2f Pan(Dir * ArtPan * Prog * View.X, -Dir * ArtPan * 0.5f * Prog * View.Y);
		const FVector2f Slack = (Size - View) * 0.5f;
		Pan.X = FMath::Clamp(Pan.X, -Slack.X, Slack.X);
		Pan.Y = FMath::Clamp(Pan.Y, -Slack.Y, Slack.Y);
		const FVector2f At = (View - Size) * 0.5f + Pan;
		// No pixel snapping: the drift is under a pixel a frame, and a 1280x720 image
		// stretched to the window stepped visibly from one whole pixel to the next.
		FSlateDrawElement::MakeBox(Out, Layer + Sub, G.ToPaintGeometry(FVector2D(Size), FSlateLayoutTransform(FVector2D(At))), B,
			ESlateDrawEffect::NoPixelSnapping, FLinearColor(1, 1, 1, Alpha));
	};
	Box(Out, Layer, G, FBox2f(FVector2f::ZeroVector, View), FLinearColor::Black);
	if (N > 1 && !bPinned && Local < ArtFade && E >= ArtFade)
	{
		Draw(Idx - 1, Local + ArtSlot, 1.f, 0);
		Draw(Idx, Local, (float)(Local / ArtFade), 1);
	}
	else
	{
		Draw(Idx, Local, 1.f, 0);
	}
	Out.PopClip();

	// Shades: left ~45% for the menu, a vignette at the edges, the bottom for the song line.
	// One smooth texture per shade (strips of boxes showed their seams):
	// white, alpha falling 1 to 0 along X (`Ramp`), stretched and turned.
	static TStrongObjectPtr<UTexture2D> Ramp;
	static FSlateBrush RampBrush;
	if (!Ramp)
	{
		FImage Img(256, 1, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		FColor* Px = reinterpret_cast<FColor*>(Img.RawData.GetData());
		for (int32 X = 0; X < 256; ++X)
		{
			const float A = 1.f - FMath::SmoothStep(0.f, 1.f, X / 255.f);
			Px[X] = FColor(255, 255, 255, (uint8)FMath::RoundToInt(A * 255));
		}
		Ramp.Reset(FAcHudStyle::HudTexture(Img));
		if (Ramp)
		{
			Ramp->Filter = TF_Bilinear;
			Ramp->AddressX = TA_Clamp;
			Ramp->UpdateResource();
			RampBrush.SetResourceObject(Ramp.Get());
			RampBrush.ImageSize = FVector2D(256, 1);
			RampBrush.DrawAs = ESlateBrushDrawType::Image;
		}
	}
	// Dark at `Dark` (an edge: 0 left, 1 right, 2 top, 3 bottom) at alpha A, clear at the far side.
	auto Shade = [&](const FBox2f R, const int32 Dark, const float A)
	{
		if (!Ramp) return;
		const FVector2f S = R.GetSize();
		// The ramp runs along X from its dark end; turn it to face `Dark`.
		const float Angle = Dark == 0 ? 0.f : Dark == 1 ? PI : Dark == 2 ? PI / 2 : -PI / 2;
		const bool bV = Dark >= 2;
		const FVector2f Len = bV ? FVector2f(S.Y, S.X) : S;
		const FSlateRenderTransform Turn{FQuat2f(Angle)};
		const FVector2f Centre = R.GetCenter();
		const FGeometry Child = G.MakeChild(Len, FSlateLayoutTransform(FVector2f(Centre - Len * 0.5f)), Turn, FVector2f(0.5f, 0.5f));
		FSlateDrawElement::MakeBox(Out, Layer + 2, Child.ToPaintGeometry(), &RampBrush, ESlateDrawEffect::None, FLinearColor(0, 0, 0, A));
	};
	Shade(FBox2f(FVector2f::ZeroVector, FVector2f(View.X * 0.45f, View.Y)), 0, 0.82f);
	Shade(FBox2f(FVector2f(View.X * 0.82f, 0), View), 1, 0.35f);
	Shade(FBox2f(FVector2f::ZeroVector, FVector2f(View.X, View.Y * 0.14f)), 2, 0.35f);
	Shade(FBox2f(FVector2f(0, View.Y * 0.8f), View), 3, 0.55f);

	if (!NowPlaying.IsEmpty())
	{
		const float K = FMath::Clamp(View.Y / 1080.f, 0.6f, 2.f);
		const FGeometry P = G.MakeChild(FVector2D(600, 40), FSlateLayoutTransform(K, FVector2D(View.X * 0.08f, View.Y - 24 * K - 26 * K)));
		Label(TEXT("NOW PLAYING"), 11, FAcHudStyle::Srgb(0.55, 0.7, 0.8), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, Layer + 3, P, FVector2f(0, 0));
		Label(NowPlaying, 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, Layer + 3, P, FVector2f(0, 16));
	}
}

/// RINGSHADOW as the website draws it: Exo 2 ExtraBold, wide tracking, white
/// at the top shading to ice and soft blue, in a cyan glow. Shrunk to `Width`.
void SAcMenu::PaintWordmark(FSlateWindowElementList& Out, const int32 Layer, const FGeometry& G, const FVector2f Centre, const float Width) const
{
	float Size = 58;
	FAcHudText T(TEXT("RINGSHADOW"), Wordmark(Size), FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Top);
	T.Accent = FLinearColor::Transparent;
	T.Kern = Size * 0.1f;
	const float Wide = T.Measure().X;
	if (Wide > Width && Wide > 0)
	{
		Size *= Width / Wide;
		T.Font = Wordmark(Size);
		T.Kern = Size * 0.1f;
	}
	// The glow: the word a few times round itself, faint.
	FAcHudText Glow = T;
	Glow.Color = FAcHudStyle::Srgb(0.35, 0.85, 1, 0.07);
	for (int32 I = 0; I < 12; ++I)
	{
		const float A = I * 2 * PI / 12;
		const float R = I % 2 ? 5.f : 2.5f;
		Glow.Paint(Out, Layer, G, Centre + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R);
	}
	// The shading, top to bottom white, ice, soft blue: the word in thin
	// horizontal strips, each clipped to its own rows in its own colour.
	const float Cap = Size * 0.7f;
	constexpr int32 Strips = 14;
	const FLinearColor Soft = FAcHudStyle::Soft(), Ice = FAcHudStyle::Ice(), White = FLinearColor::White;
	for (int32 I = 0; I < Strips; ++I)
	{
		// Strip I covers [Y0, Y1] of the capitals; the first and last reach past them (the glyphs' overshoot).
		const float A = (float)I / Strips, B = (float)(I + 1) / Strips;
		const float Y0 = I == 0 ? Centre.Y - Size : Centre.Y + Cap * A;
		const float Y1 = I == Strips - 1 ? Centre.Y + Size : Centre.Y + Cap * B;
		const float Mid = (A + B) * 0.5f;
		const FLinearColor C = Mid < 0.45f ? FMath::Lerp(White, Ice, Mid / 0.45f) : FMath::Lerp(Ice, Soft, (Mid - 0.45f) / 0.55f);
		const FVector2f Min(G.LocalToAbsolute(FVector2D(0, Y0)));
		const FVector2f Max(G.LocalToAbsolute(FVector2D(G.GetLocalSize().X, Y1)));
		Out.PushClip(FSlateClippingZone(FSlateRect(Min.X, Min.Y, Max.X, Max.Y)));
		FAcHudText Over = T;
		Over.Color = C;
		Over.Paint(Out, Layer + 1, G, Centre);
		Out.PopClip();
	}
}

namespace
{
	/// A button of the Settings page or the key card (BACK, KEY CARD, OK).
	void PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FBox2f& B, const FString& Text, const bool bHot)
	{
		Box(Out, L, P, B, bHot ? FAcHudStyle::Srgb(0.08, 0.3, 0.42, 1) : FAcHudStyle::Srgb(0.05, 0.09, 0.14, 1));
		Frame(Out, L + 1, P, B, bHot ? FAcHudStyle::Cyan() : FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8), bHot ? 1.5f : 1.0f);
		Label(Text, 18, bHot ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.75, 0.85, 0.92), EAcHAlign::Center, EAcVAlign::Center)
			.Paint(Out, L + 2, P, B.GetCenter() + FVector2f(0, 0.5f));
	}

	/// A group's heading: small caps over a hairline to the panel's edge.
	void PaintHeading(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FString& Title, const FVector2f At, const float Right)
	{
		FAcHudText Head = Label(Title, 12, FAcHudStyle::Srgb(0.55, 0.7, 0.8), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold);
		Head.Kern = 1.5f;
		Head.Paint(Out, L, P, At);
		const float X = At.X + Head.Measure().X + 10;
		Box(Out, L, P, FBox2f(FVector2f(X, At.Y + 5), FVector2f(Right, At.Y + 6)), FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.6));
	}
}

void SAcMenu::PaintSettings(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const float W = PanelSize.X;
	Label(TEXT("SETTINGS"), 34, FAcHudStyle::Ice(), EAcHAlign::Center).Paint(Out, L, P, FVector2f(W / 2, 26));
	for (const TPair<FString, FVector2f>& H : Headings) PaintHeading(Out, L, P, H.Key, H.Value, W - Pad);
	const FLinearColor Soft = FAcHudStyle::Srgb(0.75, 0.85, 0.92);
	for (int32 I = 0; I <= int32(EAcSettingsRow::Lag) && I < Boxes.Num(); ++I)
	{
		const bool bHot = I == Highlighted;
		const FBox2f& R = Boxes[I];
		if (bHot) Box(Out, L, P, R, FAcHudStyle::Srgb(0.06, 0.14, 0.2, 1));
		if (bHot) Frame(Out, L + 1, P, R, FAcHudStyle::Cyan(), 1);
		const float Y = R.GetCenter().Y + 0.5f;
		if (I < SliderCount() && Tracks.IsValidIndex(I))
		{
			Label(SliderName(I), 15, bHot ? FAcHudStyle::Text() : Soft, EAcHAlign::Left, EAcVAlign::Center).Paint(Out, L + 2, P, FVector2f(R.Min.X + 12, Y));
			const FBox2f& T = Tracks[I];
			const float F = SliderGet(I);
			Box(Out, L + 1, P, T, FAcHudStyle::Srgb(0.12, 0.18, 0.24, 1));
			Box(Out, L + 2, P, FBox2f(T.Min, FVector2f(FMath::Lerp(T.Min.X, T.Max.X, F), T.Max.Y)), FAcHudStyle::Cyan());
			const float KX = FMath::Lerp(T.Min.X, T.Max.X, F);
			Box(Out, L + 3, P, FBox2f(FVector2f(KX - 4, T.Min.Y - 7), FVector2f(KX + 4, T.Max.Y + 7)),
				bHot ? FAcHudStyle::Text() : FAcHudStyle::Srgb(0.6, 0.72, 0.8));
			Label(SliderText(I), 14, FAcHudStyle::Text(), EAcHAlign::Right, EAcVAlign::Center, EAcFontWeight::Medium)
				.Paint(Out, L + 2, P, FVector2f(R.Max.X - 10, Y));
			continue;
		}
		// A choice: its name, then "‹  WHAT  ›" over the value's room.
		const EAcSettingsRow Row = EAcSettingsRow(I);
		Label(ChoiceName(Row), 15, bHot ? FAcHudStyle::Text() : Soft, EAcHAlign::Left, EAcVAlign::Center).Paint(Out, L + 2, P, FVector2f(R.Min.X + 12, Y));
		const float X0 = R.Min.X + SettingsLabelW, X1 = R.Max.X - 10;
		const FLinearColor Arrow = bHot ? FAcHudStyle::Cyan() : FAcHudStyle::Srgb(0.4, 0.55, 0.65);
		Label(TEXT("‹"), 20, Arrow, EAcHAlign::Left, EAcVAlign::Center).Paint(Out, L + 2, P, FVector2f(X0, Y - 1));
		Label(TEXT("›"), 20, Arrow, EAcHAlign::Right, EAcVAlign::Center).Paint(Out, L + 2, P, FVector2f(X1, Y - 1));
		const bool bLagOn = Row == EAcSettingsRow::Lag && UAcSettings::Get().bMmoLag;
		Label(ChoiceText(Row), 15, bLagOn ? FAcHudStyle::Srgb(1.0, 0.72, 0.3) : FAcHudStyle::Text(), EAcHAlign::Center, EAcVAlign::Center,
			EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f((X0 + X1) * 0.5f, Y));
	}
	// Under RESOLUTION: what OK does with the screen chosen.
	if (ScreenNoteAt != FVector2f::ZeroVector)
	{
		const bool bChanged = ScreenChanged();
		const FIntPoint D = UAcSettings::Desktop();
		const FString Note = bChanged ? FString(TEXT("OK applies the screen chosen; Esc keeps the one in use."))
			: !bPendingFull                ? FString::Printf(TEXT("A window this size; the display's own is %d × %d."), D.X, D.Y)
			: PendingResolution() != D     ? FString(TEXT("Full screen at this size switches the display to it."))
										   : FString(TEXT("Full screen at the display's own resolution."));
		Label(Note, 12, bChanged ? FAcHudStyle::Srgb(1.0, 0.72, 0.3) : FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L, P, ScreenNoteAt);
	}
	// Under the lag simulator: that it is for testing.
	if (WarnAt != FVector2f::ZeroVector)
	{
		Label(TEXT("For testing only: it delays and drops the game's packets to the world server."), 12, FAcHudStyle::Srgb(1.0, 0.72, 0.3),
			EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L, P, WarnAt);
	}
	if (Boxes.IsValidIndex(int32(EAcSettingsRow::Ok)))
	{
		PaintButton(Out, L, P, Boxes[int32(EAcSettingsRow::KeyCard)], TEXT("KEY CARD"), Highlighted == int32(EAcSettingsRow::KeyCard));
		PaintButton(Out, L, P, Boxes[int32(EAcSettingsRow::Ok)], TEXT("OK"), Highlighted == int32(EAcSettingsRow::Ok));
	}
}

void SAcMenu::PaintKeyCard(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const float W = PanelSize.X;
	Label(TEXT("KEY CARD"), 34, FAcHudStyle::Ice(), EAcHAlign::Center).Paint(Out, L, P, FVector2f(W / 2, 26));
	Label(TEXT("The keys as the HUD's TIP line names them."), 13, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L, P, FVector2f(W / 2, 80));
	float Y = 84 + NoteH + 8;
	for (const AcPilotText::FKeyGroup& G : AcPilotText::KeyCard())
	{
		PaintHeading(Out, L, P, G.Title, FVector2f(Pad, Y + 6), W - Pad);
		Y += KeyHeadH;
		for (const AcPilotText::FKeyRow& R : G.Rows)
		{
			const float Mid = Y + KeyRowH * 0.5f + 0.5f;
			Label(R.Key, 15, FAcHudStyle::Cyan(), EAcHAlign::Right, EAcVAlign::Center).Paint(Out, L, P, FVector2f(Pad + KeyColW, Mid));
			Label(R.What, 15, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Medium)
				.Paint(Out, L, P, FVector2f(Pad + KeyColW + 16, Mid));
			Y += KeyRowH;
		}
	}
	if (Boxes.Num() > 0) PaintButton(Out, L, P, Boxes[0], TEXT("BACK"), Highlighted == 0);
}

int32 SAcMenu::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	const bool bHome = MenuMode == EAcMenuMode::Home;
	if (ArtShowing())
	{
		PaintArt(Out, Layer, Geometry, View);
	}
	else
	{
		// The paused game behind: darker on the home screen.
		Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, bHome ? 0.72f : 0.5f));
	}
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// Over the key art the panel is glass, so the art shows through.
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.025, 0.045, 0.075, ArtShowing() ? 0.82 : 0.96));
	Frame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2);
	Frame(Out, Layer + 2, P, Panel.ExpandBy(-4), FAcHudStyle::Srgb(0.35, 0.85, 1, 0.28), 1);
	const int32 L = Layer + 3;

	if (bKeyCard)
	{
		PaintKeyCard(Out, L, P);
		return L + 4;
	}
	if (bSettings)
	{
		PaintSettings(Out, L, P);
		return L + 4;
	}

	if (bConfirmNewWorld)
	{
		Label(TEXT("NEW WORLD"), 34, FAcHudStyle::Ice(), EAcHAlign::Center).Paint(Out, L, P, FVector2f(PanelW / 2, 26));
		static const TCHAR* Lines[] = {TEXT("This deletes the saved MMORPG world:"), TEXT("your buildings, your wallet, where you stand."),
			TEXT("Your pilot stays, new at the start pad.")};
		for (int32 I = 0; I < 3; ++I)
		{
			Label(Lines[I], 15, I == 0 ? FAcHudStyle::Text() : FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L, P, FVector2f(Pad, 84 + I * 22));
		}
	}
	else if (bHome)
	{
		PaintWordmark(Out, L, P, FVector2f(PanelSize.X / 2, 30), PanelSize.X - 2 * Pad);
		Label(TEXT("A lone prospector under a ringed giant"), 16, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Top,
			EAcFontWeight::Medium)
			.Paint(Out, L, P, FVector2f(PanelSize.X / 2, 96));
	}
	else
	{
		Label(TEXT("PAUSED"), 34, FAcHudStyle::Ice(), EAcHAlign::Center).Paint(Out, L, P, FVector2f(PanelW / 2, 26));
	}

	if (!GameTitle.IsEmpty() && !bConfirmNewWorld)
	{
		Box(Out, L, P, CardBox, FAcHudStyle::Srgb(0.04, 0.07, 0.11, 1));
		Frame(Out, L + 1, P, CardBox, FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.6));
		Label(bHome ? TEXT("YOUR SKIRMISH") : TEXT("THIS GAME"), 11, FAcHudStyle::Srgb(0.55, 0.7, 0.8), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, CardBox.Min + FVector2f(14, 10));
		Label(GameTitle, 18, FAcHudStyle::Text()).Paint(Out, L + 2, P, CardBox.Min + FVector2f(14, 25));
		Label(GameDetail, 13, FAcHudStyle::Srgb(0.6, 0.68, 0.76), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, CardBox.Min + FVector2f(14, 47));
	}

	if (bHome && !bConfirmNewWorld && GroupAt != FVector2f::ZeroVector)
	{
		// The strategy game's heading: small caps over a hairline.
		FAcHudText Head = Label(TEXT("THE STRATEGY GAME"), 12, FAcHudStyle::Srgb(0.55, 0.7, 0.8), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::SemiBold);
		Head.Kern = 1.5f;
		Head.Paint(Out, L, P, GroupAt);
		const float X = GroupAt.X + Head.Measure().X + 10;
		Box(Out, L, P, FBox2f(FVector2f(X, GroupAt.Y + 5), FVector2f(PanelSize.X - Pad, GroupAt.Y + 6)), FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.6));
	}

	if (bHome && !bConfirmNewWorld && AwayAt != FVector2f::ZeroVector)
	{
		// X2: what the world did while the pilot was away: the time in amber,
		// the most telling things under it (wrapped in two at most).
		const float Width = PanelSize.X - 2 * Pad - 4;
		FAcHudText Ran = Label(AwayRanLine, 13, FAcHudStyle::Srgb(1.0, 0.72, 0.3), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold);
		Ran.Kern = 0.4f;
		Ran.Paint(Out, L, P, AwayAt);
		auto Wide = [&](const FString& S) { return Label(S, 12, FLinearColor::White, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Measure().X; };
		TArray<FString> Words;
		AwayWhatLine.ParseIntoArray(Words, TEXT(" "), true);
		TArray<FString> Rows;
		FString Row;
		for (const FString& Word : Words)
		{
			const FString Longer = Row.IsEmpty() ? Word : Row + TEXT(" ") + Word;
			if (Row.IsEmpty() || Wide(Longer) <= Width) Row = Longer;
			else
			{
				Rows.Add(Row);
				Row = Word;
			}
		}
		if (!Row.IsEmpty()) Rows.Add(Row);
		if (Rows.Num() > 2)
		{
			FString Last = Rows[1];
			while (!Last.IsEmpty() && Wide(Last + TEXT("…")) > Width) Last.LeftChopInline(1);
			Rows.SetNum(1);
			Rows.Add(Last + TEXT("…"));
		}
		for (int32 K = 0; K < Rows.Num(); ++K)
		{
			Label(Rows[K], 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L, P, AwayAt + FVector2f(0, 20 + K * 17));
		}
	}

	const TArray<FButton> B = Buttons();
	for (int32 I = 0; I < Boxes.Num() && I < B.Num(); ++I)
	{
		const bool bHot = I == Highlighted;
		const bool bDown = I == Pressed;
		const bool bPrimary = bHome && B[I].bPrimary;
		FLinearColor Fill = FAcHudStyle::Srgb(0.05, 0.09, 0.14, 1);
		FLinearColor Rim = FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8);
		FLinearColor Ink = FAcHudStyle::Srgb(0.75, 0.85, 0.92);
		if (bPrimary)
		{
			// The way in stands out even when not highlighted: a cyan rim.
			Fill = FAcHudStyle::Srgb(0.05, 0.16, 0.24, 1);
			Rim = FAcHudStyle::Srgb(0.35, 0.85, 1, 0.75);
			Ink = FAcHudStyle::Text();
		}
		if (bHot)
		{
			Fill = bPrimary ? FAcHudStyle::Srgb(0.09, 0.36, 0.5, 1) : FAcHudStyle::Srgb(0.08, 0.3, 0.42, 1);
			Rim = FAcHudStyle::Cyan();
			Ink = FAcHudStyle::Text();
		}
		if (bDown) Fill = FAcHudStyle::Srgb(0.12, 0.4, 0.55, 1);
		const FBox2f& R = Boxes[I];
		Box(Out, L, P, R, Fill);
		Frame(Out, L + 1, P, R, Rim, bHot || bPrimary ? 1.5f : 1.0f);
		if (bPrimary)
		{
			// A bright edge down the left, as the HUD's selected cards.
			Box(Out, L + 2, P, FBox2f(R.Min, FVector2f(R.Min.X + 4, R.Max.Y)), FAcHudStyle::Cyan());
			FAcHudText Title = Label(B[I].Label, 28, Ink, EAcHAlign::Center, EAcVAlign::Center);
			Title.Kern = 2.f;
			const bool bLine = !ReachLine.IsEmpty();
			Title.Paint(Out, L + 2, P, FVector2f(R.GetCenter().X, R.Min.Y + (bLine ? 30 : R.GetSize().Y / 2)));
			if (bLine)
			{
				FAcHudText Sub = Label(ReachLine, 12, FAcHudStyle::Soft(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::SemiBold);
				Sub.Kern = 1.2f;
				Sub.Paint(Out, L + 2, P, FVector2f(R.GetCenter().X, R.Max.Y - 17));
			}
			continue;
		}
		if (bHome && B[I].bServer)
		{
			// A green light, the line, STOP at the right.
			const float Mid = R.GetCenter().Y;
			Box(Out, L + 2, P, FBox2f(FVector2f(R.Min.X + 14, Mid - 4), FVector2f(R.Min.X + 22, Mid + 4)), FAcHudStyle::Srgb(0.45, 0.95, 0.62));
			Label(ServerLine, 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, FVector2f(R.Min.X + 32, Mid + 0.5f));
			FAcHudText Stop = Label(B[I].Label, 15, Ink, EAcHAlign::Right, EAcVAlign::Center);
			Stop.Kern = 1.5f;
			Stop.Paint(Out, L + 2, P, FVector2f(R.Max.X - 16, Mid + 0.5f));
			continue;
		}
		Label(B[I].Label, bHome && B[I].bHalf ? 16 : 18, Ink, EAcHAlign::Center, EAcVAlign::Center)
			.Paint(Out, L + 2, P, R.GetCenter() + FVector2f(0, 0.5f));
	}
	return L + 4;
}
