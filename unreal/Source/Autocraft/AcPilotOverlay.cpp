#include "AcPilotOverlay.h"

#include "AcMissions.h"

#include "AcConsole.h"
#include "AcHUD.h"
#include "AcLog.h"
#include "AcPilotCamera.h"
#include "AcPilotPawn.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "SAcConsole.h"
#include "SAcPilotOverlay.h"
#include "SAcRoot.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Defence.h"
#include "Rules.h"
#include "Simulation.h"
#include "Types.h"

#include <cmath>

namespace
{
	/// `notePilot`: a note stays 1.8 s.
	constexpr double NoteSeconds = 1.8;
	/// Stills: marks shown this long after they land (the ticks at full, the
	/// damage just off the pip).
	constexpr double HeldMarkAge = 0.06;

	FAutoConsoleCommandWithWorldAndArgs GPilotMark(TEXT("ac.PilotMark"),
		TEXT("ac.PilotMark DAMAGE [kill]: a hit mark on the cockpit's reticle, as a landed shot makes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UAcPilotOverlaySubsystem* O = UAcPilotOverlaySubsystem::Get(World))
			{
				O->Mark(Args.Num() > 0 ? FCString::Atod(*Args[0]) : 6.0, Args.Contains(TEXT("kill")));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GPilotNote(TEXT("ac.PilotNote"), TEXT("ac.PilotNote TEXT: a note over the cockpit's reticle."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UAcPilotOverlaySubsystem* O = UAcPilotOverlaySubsystem::Get(World)) O->Note(FString::Join(Args, TEXT(" ")));
		}));
}

UAcPilotOverlaySubsystem* UAcPilotOverlaySubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAcPilotOverlaySubsystem>() : nullptr;
}

bool UAcPilotOverlaySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAcPilotOverlaySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const TCHAR* Cmd = FCommandLine::Get();
	FString Mark;
	if (FParse::Value(Cmd, TEXT("AcPilotMark="), Mark))
	{
		FAcHitMark M;
		M.Serial = 1;
		M.bKill = Mark.Equals(TEXT("kill"), ESearchCase::IgnoreCase);
		M.Damage = M.bKill ? 0 : FCString::Atod(*Mark);
		HeldMark = M;
	}
	bHeldHurt = FParse::Param(Cmd, TEXT("AcPilotHurt"));
	FParse::Value(Cmd, TEXT("AcPilotNote="), HeldNote);
	FParse::Value(Cmd, TEXT("AcPilotRun="), RunFor);
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		FrameHandle = Sim->AddFrameListener(EAcFrameStage::Hud, FAcFrameEvent::FDelegate::CreateUObject(this, &UAcPilotOverlaySubsystem::OnFrame));
	}
}

void UAcPilotOverlaySubsystem::Deinitialize()
{
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this)) Sim->RemoveFrameListener(EAcFrameStage::Hud, FrameHandle);
	if (AAcPilotPawn* Pawn = NotePawn.Get()) Pawn->OnNote.Remove(NoteHandle);
	Detach();
	Super::Deinitialize();
}

void UAcPilotOverlaySubsystem::Note(const FString& Text)
{
	NoteText = Text;
	NoteUntil = FPlatformTime::Seconds() + NoteSeconds;
}

void UAcPilotOverlaySubsystem::SetBuild(const bool bMenu, const TOptional<ac::StructureKind> Placing, const TOptional<ac::Vec2> Spot,
	const TOptional<FString> Refusal)
{
	Inputs.bBuildMenu = bMenu;
	Inputs.Placing = Placing;
	Inputs.PlacingSpot = Spot;
	Inputs.PlacingRefusal = Refusal;
}

void UAcPilotOverlaySubsystem::Mark(const double Damage, const bool bKill)
{
	FAcHitMark M;
	M.Serial = Inputs.Hit ? Inputs.Hit->Serial + 1 : 1;
	M.Damage = Damage;
	M.bKill = bKill;
	Inputs.Hit = M;
}

void UAcPilotOverlaySubsystem::Attach(const bool bPanel)
{
	AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return;
	if (!Widget)
	{
		Root->AddLayer(Layer)[SAssignNew(Widget, SAcPilotOverlay).Panel(bPanel)];
	}
	Widget->Reset();
	Widget->SetPanel(bPanel);
	Widget->Hold(HeldMark ? TOptional<double>(HeldMarkAge) : TOptional<double>(), bHeldHurt, !HeldNote.IsEmpty());
	Widget->SetVisibility(EVisibility::HitTestInvisible);
	Hud->SetDriving(true);
}

void UAcPilotOverlaySubsystem::Detach()
{
	if (Widget) Widget->SetVisibility(EVisibility::Collapsed);
	if (AAcHUD* Hud = AAcHUD::Get(this)) Hud->SetDriving(false);
	if (UAcConsoleSubsystem* Console = UAcConsoleSubsystem::Get(this))
	{
		Console->SetPilotInfo({});
		Console->SetLook(EAcConsoleStyle::Rts, EAcCabLamps::Idle, true);
	}
}

void UAcPilotOverlaySubsystem::GunFeed(const ac::Simulation& S, const AAcPilotPawn& Pawn, const FAcFrame& Frame, FAcPilotInfo& Info)
{
	const int32 Gun = Pawn.AtGun();
	const std::optional<ac::Structure> C = Gun != INDEX_NONE ? S.state.structure(Pawn.GunCitadel()) : std::nullopt;
	if (!C)
	{
		GunShots.Reset();
		GunShotsOf = {0, INDEX_NONE};
		return;
	}
	if (GunShotsOf != TPair<int64, int32>(C->id, Gun))
	{
		GunShots.Reset();
		GunShotsOf = {C->id, Gun};
	}
	// C2: its rounds (`Fired`), each put down to the corner gun nearest
	// its muzzle.
	for (const ac::GameEvent& E : Frame.AllEvents)
	{
		const ac::GameEvent::Fired* Fi = E.as<ac::GameEvent::Fired>();
		if (!Fi || Fi->shooter != C->id) continue;
		int32 Best = 0;
		double BestD = 1e18;
		for (int32 K = 0; K < 4; ++K)
		{
			const double D = ac::distance(ac::Defence::cornerGun(C->position, K), ac::Vec2(Fi->muzzle.x, Fi->muzzle.y));
			if (D < BestD)
			{
				BestD = D;
				Best = K;
			}
		}
		if (Best == Gun) GunShots.Add(S.state.time);
	}
	GunShots.RemoveAll([&S](const double T) { return S.state.time - T > 5.0; });
	Info.Feed = AcPilotText::Feed(*C, Gun, Pawn.Yaw(), Pawn.ViewPitch(), GunShots, S.state.time);
	// The ballistic impact marker: the aim's round flown to its first hit,
	// on the screen.
	// (The muzzle over the Citadel's ground, as the core fires it.)
	const ac::Vec3 Muzzle = ac::Defence::gunMuzzle(C->position, Gun, Pawn.Yaw(), Pawn.ViewPitch()) + ac::Vec3(0, 0, S.groundZ(C->position));
	if (const TOptional<ac::Vec3> Hit = AcPilotText::BallisticImpact(S, Muzzle, ac::Rules::roundVelocity(Pawn.Yaw(), Pawn.ViewPitch()), C->id))
	{
		const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		FVector2D Px;
		int32 Vw = 0, Vh = 0;
		if (PC) PC->GetViewportSize(Vw, Vh);
		if (PC && Vh > 0 && PC->ProjectWorldLocationToScreen(AcSpace::ToWorld(ac::Vec2(Hit->x, Hit->y), Hit->z), Px, false))
		{
			Info.Feed->Impact = FVector2D((Px.X - Vw * 0.5) / (Vh * 0.5), -(Px.Y - Vh * 0.5) / (Vh * 0.5));
			const ac::Vec3 D = *Hit - Muzzle;
			Info.Feed->ImpactCells = std::sqrt(D.x * D.x + D.y * D.y + D.z * D.z);
		}
	}
}

void UAcPilotOverlaySubsystem::DummyTags(const ac::Simulation& S, FAcPilotInfo& Info) const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	int32 Vw = 0, Vh = 0;
	if (PC) PC->GetViewportSize(Vw, Vh);
	if (!PC || Vh <= 0) return;
	for (const ac::Structure& B : S.state.structures)
	{
		if (B.kind != ac::StructureKind::dummy || !B.complete() || ac::distance(B.position, Info.Position) > 60) continue;
		// Over the post's top, where its bar stands.
		const double Top = S.groundZ(B.position) + ac::Rules::height(B.kind) + 0.75;
		FVector2D Px;
		if (!PC->ProjectWorldLocationToScreen(AcSpace::ToWorld(B.position, Top), Px, false)) continue;
		if (Px.X < 0 || Px.Y < 0 || Px.X > Vw || Px.Y > Vh) continue;
		FAcDummyTag T;
		T.At = FVector2D((Px.X - Vw * 0.5) / (Vh * 0.5), -(Px.Y - Vh * 0.5) / (Vh * 0.5));
		T.Hits = B.hits;
		T.bDown = B.downUntil.has_value();
		Info.Dummies.Add(T);
	}
}

void UAcPilotOverlaySubsystem::OnFrame(const FAcFrame& Frame)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	AAcPilotPawn* Pawn = AAcPilotPawn::Find(this);
	if (!Sim || !Sim->IsRunning() || !Frame.State) return;
	if (Pawn && NotePawn.Get() != Pawn)
	{
		if (AAcPilotPawn* Old = NotePawn.Get()) Old->OnNote.Remove(NoteHandle);
		NoteHandle = Pawn->OnNote.AddUObject(this, &UAcPilotOverlaySubsystem::Note);
		NotePawn = Pawn;
	}
	const ac::Simulation& S = Sim->Simulation();
	const TOptional<FAcPilotFrame> F = Pawn && Pawn->Driving() ? FAcPilotFrame::Of(S) : TOptional<FAcPilotFrame>();
	// -AcPilotPullOut: the camera has left the unit; the HUD goes with it.
	// A pad flight (AcPilotFlight.h): the HUD comes back at the touch down.
	if (!F || F->Unit.id != Pawn->Driven() || Pawn->PulledOut() || Pawn->InFlight())
	{
		// (Off a gun the dashboard is up again, C1.)
		if (UAcConsoleSubsystem* Console = UAcConsoleSubsystem::Get(this)) Console->SetDashboardHidden(false);
		if (DrivenId != INDEX_NONE)
		{
			DrivenId = INDEX_NONE;
			Shown.Reset();
			Inputs = FAcPilotInputs();
			NoteText.Reset();
			Aim.Reset();
			Detach();
		}
		return;
	}
	const ac::Unit& U = F->Unit;
	if (RunUntil >= 0)
	{
		// -AcPilotRun: the staged still runs on a moment, then holds again.
		// The act key held meanwhile (`stagePilot("mining")`).
		const bool bDone = Sim->GameTime() >= RunUntil;
		Sim->SetPaused(bDone);
		Pawn->Press(!bDone);
		if (bDone) RunUntil = -2;
	}
	const UAcConsoleSubsystem* ConsoleSub = UAcConsoleSubsystem::Get(this);
	const bool bConsole = ConsoleSub && ConsoleSub->Console().IsValid();
	if (DrivenId != U.id && GFrameCounter - Sim->SeamlessFrame() <= 2)
	{
		// The same machine under its new id after a seamless region change
		// (W4): the overlay stays as it is, its console art not baked again
		// (K3: its copy back held the render thread 18 ms at every crossing).
		DrivenId = U.id;
	}
	if (DrivenId != U.id)
	{
		// A new ride (`takeOver`: `pilotHit = nil`).
		DrivenId = U.id;
		Inputs.Hit.Reset();
		Aim.Reset();
		Attach(!bConsole);
		if (HeldMark) Inputs.Hit = HeldMark;
		if (RunFor > 0 && RunUntil < 0) RunUntil = Sim->GameTime() + RunFor;
		UE_LOG(LogAutocraft, Log, TEXT("pilot overlay: %s #%lld, %s"), *AcPilotText::Title(U.kind), (long long)U.id,
			bConsole ? TEXT("on the console") : TEXT("with its own panel"));
	}

	// `markPilotHit`: the driven unit's landed hits, with the damage the
	// sight's target was worth before the step. A staged still marks it at
	// the shot instead, as Swift's stage does (`stageUnit` calls
	// `markPilotHit` with the shot): a shell or a flame lands after the still.
	const bool bAtShot = Pawn->Staged();
	for (const ac::GameEvent& E : Frame.AllEvents)
	{
		int64 Target = INDEX_NONE;
		if (bAtShot)
		{
			const ac::GameEvent::Shot* Sh = E.as<ac::GameEvent::Shot>();
			if (Sh && Sh->unit == U.id) Target = Sh->target;
		}
		else if (const ac::GameEvent::Landed* L = E.as<ac::GameEvent::Landed>(); L && L->unit == U.id)
		{
			Target = L->target;
		}
		if (Target == INDEX_NONE) continue;
		bool bAlive = false;
		if (const std::optional<ac::Unit> V = S.state.unit(Target)) bAlive = V->hp > 0;
		else if (const std::optional<ac::Structure> B = S.state.structure(Target)) bAlive = B->hp > 0;
		double Damage = 0;
		if (Aim && Aim->Key == Target) Damage = Aim->Value;
		else if (const std::optional<double> D = S.strikeDamage(U, Target)) Damage = *D;
		Mark(Damage, !bAlive);
		// The still is taken a moment later: the mark held fresh, as drawn
		// in Swift's still (made at the shot, drawn the same frame).
		if (bAtShot && Widget) Widget->Hold(TOptional<double>(HeldMarkAge), bHeldHurt, !HeldNote.IsEmpty());
	}
	// The MMORPG mode: the load the pilot unloaded, a toast in the one lane
	// (U1: gains add up, "+72 ORE", the realm's share under it).
	const UAcProgressHudSubsystem* ProgressHud = UAcProgressHudSubsystem::Get(this);
	if (const TSharedPtr<SAcProgressHud> Lane = Sim->IsMmo() && ProgressHud ? ProgressHud->Widget() : nullptr)
	{
		for (const ac::GameEvent& E : Frame.Events)
		{
			const ac::GameEvent::Deposited* D = E.as<ac::GameEvent::Deposited>();
			if (D && D->unit == U.id)
			{
				// What reached the wallet, and the realm's tax (C1): "+22 ORE",
				// "3 TO THE REALM".
				const bool bMh = D->hydrogen || bLoadWasHydrogen;
				Lane->Gain(bMh ? TEXT("MH") : TEXT("ORE"), D->amount - D->tax, D->tax, FPlatformTime::Seconds());
			}
		}
	}
	if (U.carrying > 0) bLoadWasHydrogen = U.hydrogen == true;
	Aim.Reset();
	if (S.pilotShoots(U) && F->Sight)
	{
		Aim = TPair<int64, double>(F->Sight->target, S.strikeDamage(U, F->Sight->target).value_or(0));
	}

	const double Now = FPlatformTime::Seconds();
	if (!HeldNote.IsEmpty()) Inputs.Note = HeldNote;
	else if (NoteText && NoteUntil > Now) Inputs.Note = NoteText;
	else
	{
		NoteText.Reset();
		Inputs.Note.Reset();
	}
	// C1: at a gun the view is the gun's, first person.
	Inputs.bThirdPerson = Pawn->Eye().bThirdPerson && Pawn->AtGun() == INDEX_NONE;
	Inputs.bMmo = Sim->IsMmo();
	Inputs.Mmo = Sim->MmoClient();
	FAcPilotInfo Info = AcPilotText::Info(S, *F, Inputs);
	GunFeed(S, *Pawn, Frame, Info);
	Info.Wheel = Wheel;
	DummyTags(S, Info);
	if (Inputs.Mmo)
	{
		Info.Marks = AcProgressHud::ContractGoals(*Inputs.Mmo, U.position);
		Info.Marks.Append(AcMissions::Goals(*Inputs.Mmo, U.position));
		// The target picked on the world map (M2), in its region: the needle's.
		const TOptional<FAcMmoClient::FMapTarget>& T = Inputs.Mmo->GetMapTarget();
		const ac::net::WorldMessage::Region* Here = Inputs.Mmo->Here();
		if (T && Here && FString(UTF8_TO_TCHAR(Here->id.c_str())) == T->Region)
		{
			FAcPilotGoal Goal{T->Label, T->At, ac::distance(T->At, U.position)};
			Goal.Color = T->Color;
			Goal.bMission = T->Key.StartsWith(TEXT("mission:"));
			Info.Goal = Goal;
		}
	}
	if (GunMarker) Info.Gun = GunMarker();
	else Info.Gun = Pawn->GunMarker(); // E4's marker, computed by the pawn each frame
	Info.Reticle = Pawn->ReticleMarker();
	Info.bReticleHidden = Pawn->ReticleHidden();
	// A soldier's gun: the brackets open with its cone, moving and firing
	// (docs/shooter-feel.md).
	if (S.pilotShoots(U) && ac::Rules::stats(U.kind).bio)
	{
		const ac::UnitStats W = S.weapon(U);
		if (W.falloff > 0)
		{
			const double Half = FMath::DegreesToRadians(FAcPilotCamera::FovDegrees * 0.5);
			Info.Cone = std::tan(S.cone(U, W, S.pilotPace)) / std::tan(Half);
		}
	}
	if (Widget)
	{
		// The world over the dashboard: the full width, down to its highest brow.
		FAcRect View;
		if (bConsole)
		{
			const FAcCabLayout& L = ConsoleSub->Console()->Layout();
			const double Low = FMath::Max(L.Brow[0].Y, L.Brow[3].Y);
			View = FAcRect(0, Low, L.Size.X, L.Size.Y - Low);
		}
		// C1: at a corner gun the dashboard is down; the feed has the whole view.
		Widget->SetView(Pawn->AtGun() != INDEX_NONE ? FAcRect() : View);
		Widget->Show(Info, Now);
	}

	if (UAcConsoleSubsystem* Console = UAcConsoleSubsystem::Get(this))
	{
		// `HUD.applyPilot`: the cab's frame in first person in a cab, the
		// frame round the view only from behind.
		Console->SetLook(Info.bCab ? EAcConsoleStyle::Cab : EAcConsoleStyle::Cockpit, Info.Lamps, Info.bThirdPerson);
		Console->SetPilotInfo(Info.Console);
		Console->SetDashboardHidden(Pawn->AtGun() != INDEX_NONE);
	}
	Shown = MoveTemp(Info);
}
