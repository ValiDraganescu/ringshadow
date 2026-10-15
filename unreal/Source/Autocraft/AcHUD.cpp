#include "AcHUD.h"

#include "AcDaylight.h"
#include "AcHudStyle.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcModelCatalog.h"
#include "AcWorldRenderer.h"
#include "SAcResourceBar.h"
#include "SAcRoot.h"
#include "SAcScoreboard.h"
#include "TagSight.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Slate/SRetainerWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Widgets/SWindow.h"

static TAutoConsoleVariable<float> CVarAcHudScale(
	TEXT("ac.HudScale"), 0.0f,
	TEXT("Pixels per HUD point (Swift point). 0: the window's backing scale (2 on Retina)."));

AAcHUD::AAcHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

AAcHUD* AAcHUD::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? Cast<AAcHUD>(PC->GetHUD()) : nullptr;
}

void AAcHUD::BeginPlay()
{
	Super::BeginPlay();
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim) return;
	Attach();
	if (!RootWidget) return;
	FrameHandle = Sim->AddFrameListener(EAcFrameStage::Hud, FAcFrameEvent::FDelegate::CreateUObject(this, &AAcHUD::OnFrame));
	StartedHandle = Sim->OnGameStarted.AddUObject(this, &AAcHUD::OnGameStarted);
}

void AAcHUD::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		Sim->RemoveFrameListener(EAcFrameStage::Hud, FrameHandle);
		Sim->OnGameStarted.Remove(StartedHandle);
	}
	if (RootWidget && GEngine && GEngine->GameViewport)
	{
		if (Fader) GEngine->GameViewport->RemoveViewportWidgetContent(Fader.ToSharedRef());
		else GEngine->GameViewport->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
	}
	Fader.Reset();
	RootWidget.Reset();
	Super::EndPlay(Reason);
}

void AAcHUD::Attach()
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!Viewport || !FSlateApplication::IsInitialized()) return;
	TWeakObjectPtr<AAcHUD> Self(this);
	TWeakObjectPtr<UGameViewportClient> WeakViewport(Viewport);
	RootWidget = SNew(SAcRoot)
		.PixelsPerPoint_Lambda([Self] { return Self.IsValid() ? Self->PixelsPerPoint() : 2.0f; })
		.EngineScale_Lambda([WeakViewport]
		{
			return WeakViewport.IsValid() ? UWidgetLayoutLibrary::GetViewportScale(WeakViewport.Get()) : 1.0f;
		});
	Viewport->AddViewportWidgetContent(RootWidget.ToSharedRef(), 10);
	UE_LOG(LogAutocraft, Log, TEXT("hud: attached"));
}

void AAcHUD::SetFade(const float Opacity)
{
	UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (!RootWidget || !Viewport) return;
	if (!Fader)
	{
		Viewport->RemoveViewportWidgetContent(RootWidget.ToSharedRef());
		Fader = SNew(SRetainerWidget).RenderOnPhase(false).RenderOnInvalidation(false).StatId(TEXT("AcHudFade"))
		[
			RootWidget.ToSharedRef()
		];
		Fader->SetRetainedRendering(true);
		Viewport->AddViewportWidgetContent(Fader.ToSharedRef(), 10);
	}
	Fader->SetRenderOpacity(FMath::Clamp(Opacity, 0.f, 1.f));
}

float AAcHUD::PixelsPerPoint() const
{
	static const float FromCommandLine = []
	{
		float S = 0;
		FParse::Value(FCommandLine::Get(), TEXT("AcHudScale="), S);
		return S;
	}();
	if (FromCommandLine > 0) return FromCommandLine;
	if (const float S = CVarAcHudScale.GetValueOnGameThread(); S > 0) return S;
	const UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
	if (const TSharedPtr<SWindow> Window = Viewport ? Viewport->GetWindow() : nullptr)
	{
		return FMath::Max(1.0f, Window->GetDPIScaleFactor());
	}
	return FMath::Max(1.0f, FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(0, 0));
}

FVector2f AAcHUD::PointsFromPixels(const FVector2D Pixels) const
{
	return FVector2f(Pixels / PixelsPerPoint());
}

bool AAcHUD::ProjectToPoints(const FVector& World, FVector2f& OutPoints) const
{
	FVector2D Pixels;
	if (!PlayerOwner || !PlayerOwner->ProjectWorldLocationToScreen(World, Pixels, false)) return false;
	OutPoints = PointsFromPixels(Pixels);
	return true;
}

void AAcHUD::SetDriving(const bool bDriving)
{
	if (RootWidget) RootWidget->SetDriving(bDriving);
}

void AAcHUD::OnGameStarted(UAcSimSubsystem& Sim)
{
	if (RootWidget) RootWidget->Reset();
}

void AAcHUD::OnFrame(const FAcFrame& Frame)
{
	if (!RootWidget || !Frame.State) return;
	if (!bLoggedScale)
	{
		bLoggedScale = true;
		const UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
		FVector2D Size(0, 0);
		if (Viewport) Viewport->GetViewportSize(Size);
		UE_LOG(LogAutocraft, Log, TEXT("hud: viewport %.0fx%.0f px, %.2f px per point, engine UI scale %.3f"), Size.X, Size.Y,
			PixelsPerPoint(), Viewport ? UWidgetLayoutLibrary::GetViewportScale(Viewport) : 0.0f);
	}
	const double Now = FPlatformTime::Seconds();
	bool bWallet = true;
	if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		RootWidget->SetMmo(Sim->IsMmo());
		// The MMORPG test: a new region's mirror (an edge crossing, a
		// landing, a resume) holds the map's start stock until its first
		// players come; the wallet shown stays the last one till then
		// (docs/mmorpg-m5.md, E3).
		if (Sim->IsMmo())
		{
			const FAcMmoClient* Mmo = Sim->MmoClient();
			const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
			bWallet = M && M->playersKnown() && Frame.State == &M->simulation().state;
		}
		// The realm wallet (C1), beside the pilot's.
		TOptional<TPair<int64, int64>> Realm;
		if (const FAcMmoClient* Mmo = Sim->IsMmo() ? Sim->MmoClient() : nullptr)
		{
			if (const ac::net::ProgressMessage* P = Mmo->Progress()) Realm = TPair<int64, int64>(P->realmWallet.ore, P->realmWallet.hydrogen);
		}
		if (RootWidget->ResourceBar()) RootWidget->ResourceBar()->SetRealm(Realm);
		// The MMORPG test: a Prospector in flight between pads is in no
		// region's state, but still the pilot's.
		int64 Away = 0;
		if (const FAcMmoClient* Mmo = Sim->MmoClient(); Mmo && Mmo->Travelling())
		{
			Away = 1;
			for (const ac::Unit& U : Frame.State->units)
			{
				if (U.owner == Frame.LocalPlayer) Away = 0;
			}
		}
		if (RootWidget->ScoreboardWidget()) RootWidget->ScoreboardWidget()->SetAway(Away);
	}
	RootWidget->Update(*Frame.State, Frame.LocalPlayer, Frame.RealDelta, Now, bWallet);

	// Deposits the team sees: "+N" 4 cells over the ground there. Driving,
	// only those within the driven unit's own sight (its Spotter picks
	// counted), not every one the team sees across the map.
	TOptional<TPair<ac::Vec2, double>> PilotSight;
	// The MMORPG test: the pilot's own load is the large gain over the
	// reticle (SAcPilotOverlay), not a second "+N" at the Citadel.
	int64 OwnGain = INDEX_NONE;
	if (const AAcPilotPawn* Pawn = AAcPilotPawn::Find(this); Pawn && Pawn->Driving())
	{
		if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->IsRunning())
		{
			if (Sim->IsMmo()) OwnGain = Pawn->Driven();
			const ac::Simulation& S = Sim->Simulation();
			if (const std::optional<ac::Unit> U = S.pilotUnit())
			{
				const ac::Boost B = S.boost(ac::Stat::Sight{}, *U);
				const double Reach = ac::Rules::vision(U->kind);
				const double Night = ac::Rules::sightScale(ac::Rules::hourAt(S.state.time, S.state.dayLength, S.state.heldHour));
				PilotSight = TPair<ac::Vec2, double>(U->position, (B != ac::Boost::none ? B.apply(Reach) : Reach) * Night);
			}
		}
	}
	const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
	for (const ac::GameEvent& E : Frame.Events)
	{
		const auto* D = E.as<ac::GameEvent::Deposited>();
		if (!D || D->unit == OwnGain) continue;
		if (PilotSight && ac::length(D->at - PilotSight->Key) > PilotSight->Value) continue;
		const double Ground = Terrain ? Terrain->FieldHeight(D->at) : 0.0;
		FVector2f At;
		if (ProjectToPoints(AcSpace::ToWorld(D->at, Ground + 4.0), At))
		{
			RootWidget->Popup(D->amount, At, Now);
		}
	}

	// H2: the mercenaries here, any pilot's: their orange paint and a tag
	// over each one drawn ("MERC · FLINT"); P1's frontier pilots in theirs.
	// A tag only over a unit the pilot sees (TagSight.h): drawn, in its
	// sight (driving: the night's circle, the day's region), with no ground
	// or building between the camera and the unit's head.
	TArray<TPair<FString, FVector2f>> Tags;
	TMap<int64, int32> Paint;
	UAcWorldRenderer* Renderer = UAcWorldRenderer::Get(this);
	const ac::GameState& Shown = *Frame.State;
	std::vector<ac::TagSight::Block> Blocks;
	if (Renderer)
		for (const ac::Structure& B : Shown.structures)
		{
			const FAcModelInfo* Model = Renderer->ModelOf(B.id);
			const double Base = Terrain ? Terrain->FieldHeight(B.position) : 0.0;
			const double Top = Model && Model->Bounds.IsValid ? AcSpace::ToCells(Model->Bounds.Max.Z) : 2.0;
			Blocks.push_back({B.position, ac::Rules::radius(B.kind) * 0.85, Base, Base + Top});
		}
	FVector EyeW = FVector::ZeroVector;
	if (PlayerOwner && PlayerOwner->PlayerCameraManager) EyeW = PlayerOwner->PlayerCameraManager->GetCameraLocation();
	const ac::Vec2 Eye = AcSpace::ToSim(EyeW);
	const double EyeUp = AcSpace::HeightToSim(EyeW);
	// Driving: as far as the veil's light reaches (AAcPilotAids::Veil): the
	// sky's reach by day, the pilot's own sight circle as the dark comes.
	std::optional<std::pair<ac::Vec2, double>> Sight;
	if (PilotSight)
	{
		const AAcDaylight* Day = AAcDaylight::Find(GetWorld());
		const double Reach = Day ? Day->GetSkyTones().Reach : 110.0;
		const double Dark = Day ? FMath::Clamp(double(Day->GetDark()), 0.0, 1.0) : 0.0;
		Sight = std::make_pair(PilotSight->Key, FMath::Lerp(FMath::Max(Reach, PilotSight->Value), PilotSight->Value, Dark));
	}
	auto Ground = [&](const ac::Vec2 P) { return Terrain ? Terrain->FieldHeight(P) : 0.0; };
	auto TagAt = [&](const int64 Id, FVector2f& At) {
		const ac::Unit* U = nullptr;
		for (const ac::Unit& X : Shown.units)
			if (X.id == Id) U = &X;
		// Not drawn: aboard, in a Derrick or a Bastion, or out of the picture.
		if (!U || U->task == ac::Unit::Task::aboard || U->task == ac::Unit::Task::inDerrick || U->task == ac::Unit::Task::inBastion) return false;
		const TConstArrayView<FTransform> World = Renderer->PartWorld(Id);
		if (World.Num() == 0) return false;
		if (!ac::TagSight::seen(U->position, 2.0, Eye, EyeUp, Sight, Ground, Blocks)) return false;
		return ProjectToPoints(World[0].GetLocation() + FVector(0, 0, AcSpace::ToCm(2.4)), At);
	};
	if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->IsMmo() && Renderer)
	{
		const FAcMmoClient* Mmo = Sim->MmoClient();
		if (const ac::net::ProgressMessage* P = Mmo ? Mmo->Progress() : nullptr)
		{
			// P1: the frontier pilots here, in their paints: "VESPER · LV 14".
			if (const std::optional<ac::net::Sky>& Sky = Mmo->Sky(); Sky && Mmo->World())
			{
				for (const ac::net::Sky::Frontier& F : Sky->frontier)
				{
					if (F.unit == 0 || F.region != Mmo->World()->here) continue;
					std::string Name = "PILOT";
					int32 Team = 2;
					for (const ac::net::ProgressMessage::FrontierPilot& X : P->frontier)
						if (X.id == F.id)
						{
							Name = X.callsign;
							Team = X.paint;
						}
					Paint.Add(F.unit, Team);
					FVector2f At;
					if (TagAt(F.unit, At))
					{
						Tags.Add({FString::Printf(TEXT("%s · LV %lld"), *FString(UTF8_TO_TCHAR(Name.c_str())).ToUpper(), (long long)F.level), At});
					}
				}
			}
			for (const auto& [Id, Name] : P->mercTags)
			{
				Paint.Add(Id, UAcWorldRenderer::MercenaryTeam);
				FVector2f At;
				if (TagAt(Id, At))
				{
					Tags.Add({FString::Printf(TEXT("MERC · %s"), *FString(UTF8_TO_TCHAR(Name.c_str())).ToUpper()), At});
				}
			}
		}
	}
	if (Renderer) Renderer->SetTeamPaint(MoveTemp(Paint));
	RootWidget->SetTags(MoveTemp(Tags));
}
