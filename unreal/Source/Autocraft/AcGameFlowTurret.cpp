// The turret card (UAcGameFlowSubsystem, D3; docs/mmorpg-m8.md): U near a
// Sentinel or a Gatling Gun of the realm opens it, its marks bought from the
// pilot's own wallet.

#include "AcGameFlow.h"

#include "AcHUD.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcPilotPawn.h"
#include "AcProgressHud.h"
#include "AcSimSubsystem.h"
#include "SAcRoot.h"

#include "Framework/Application/SlateApplication.h"

#include "Defence.h"
#include "Pilot.h"
#include "Rules.h"

#include <cmath>

namespace
{
	/// The Citadel screen's (one screen at a time).
	constexpr int32 TurretCardLayer = 73;
	/// Within this of the heading, a turret is under the crosshair.
	constexpr double CrosshairCone = 0.45;

	/// What a turret hits, for the card's header.
	const TCHAR* Role(const ac::StructureKind Kind)
	{
		return Kind == ac::StructureKind::sentinel ? TEXT("AA TOWER · THE AIR") : TEXT("GROUND AND CLOSE AIR");
	}

	FString Value(const ac::DefenceStat Stat, const ac::UnitStats& W)
	{
		switch (Stat)
		{
		case ac::DefenceStat::rate: return FString::Printf(TEXT("%.1f"), 1.0 / W.cooldown);
		case ac::DefenceStat::damage: return FString::Printf(TEXT("%.1f"), W.damage);
		case ac::DefenceStat::range: return FString::Printf(TEXT("%.1f"), W.range);
		case ac::DefenceStat::air: return FString::Printf(TEXT("%.1f"), W.airRange > 0 ? W.airRange : W.range);
		}
		return FString();
	}
}

std::optional<int64> UAcGameFlowSubsystem::NearTurret() const
{
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	if (!M || Mmo->Travelling()) return std::nullopt;
	const ac::GameState& St = M->simulation().state;
	const ac::Unit* U = M->driven() ? M->unit(*M->driven()) : nullptr;
	if (!U) return std::nullopt;
	// The one under the crosshair (closest to the heading), else the nearest.
	std::optional<int64> Aimed, Nearest;
	double AimedOff = CrosshairCone, NearestGap = 1e9;
	for (const ac::Structure& B : St.structures)
	{
		if (!ac::Defence::upgradable(B.kind) || !B.complete() || B.owner != U->owner) continue;
		const double Gap = ac::distance(U->position, B.position) - ac::Rules::radius(B.kind) - ac::Rules::radius(U->kind);
		if (Gap > ac::Defence::turretReach) continue;
		const double Off = std::abs(std::remainder(std::atan2(B.position.y - U->position.y, B.position.x - U->position.x) - U->heading, 2 * ac::pi));
		if (Off < AimedOff)
		{
			AimedOff = Off;
			Aimed = B.id;
		}
		if (Gap < NearestGap)
		{
			NearestGap = Gap;
			Nearest = B.id;
		}
	}
	return Aimed ? Aimed : Nearest;
}

bool UAcGameFlowSubsystem::OpenTurretCard()
{
	if (TurretCard) return true;
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsMmo() || Menu || Dialog) return false;
	const AAcHUD* Hud = AAcHUD::Get(this);
	const TSharedPtr<SAcRoot> Root = Hud ? Hud->Root() : nullptr;
	if (!Root) return false;
	const std::optional<int64> Id = NearTurret();
	if (!Id)
	{
		Sim->Refuse(FString::Printf(TEXT("U opens a turret's card: walk within %d cells of a Sentinel or a Gatling Gun"),
			int32(ac::Defence::turretReach)));
		return false;
	}
	CloseWorldMap();
	CloseSkillScreen();
	CloseRigScreen();
	CloseCitadelScreen();
	CloseJournalScreen();
	TurretCardId = *Id;
	TWeakObjectPtr<UAcGameFlowSubsystem> Weak(this);
	Root->AddLayer(TurretCardLayer)
	[
		SAssignNew(TurretCard, SAcTurretCard)
		.OnUpgrade_Lambda([Weak](int32 Index)
			{
				if (!Weak.IsValid() || !Weak->TurretCard) return;
				UAcSimSubsystem* S = UAcSimSubsystem::Get(Weak.Get());
				FAcMmoClient* Mmo = S ? S->MmoClient() : nullptr;
				const TArray<int32>& Stats = Weak->TurretCard->GetInfo().Stats;
				FString Why;
				if (!Mmo) Why = TEXT("Not in a world");
				else if (Stats.IsValidIndex(Index) && Mmo->UpgradeTurret(Weak->TurretCardId, ac::DefenceStat(Stats[Index]), &Why)) return;
				Weak->TurretCard->SetRefusal(Why.IsEmpty() ? FString(TEXT("Nothing there")) : Why);
			})
		.OnClose_Lambda([Weak]() { if (Weak.IsValid()) Weak->CloseTurretCard(); })
	];
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(TurretCard, EFocusCause::SetDirectly);
	// As the Citadel screen: the world goes on, the pilot's unit stands, the pointer is free.
	SetBlocked(true);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(true);
	const FAcMmoClient* Mmo = Sim->MmoClient();
	TurretRefusalShown = Mmo ? Mmo->TurretRefusal() : FString();
	TurretKeptShown = Mmo ? Mmo->TurretKept() : 0;
	TickTurretCard();
	UE_LOG(LogAutocraft, Log, TEXT("turret card: open on %lld"), (long long)TurretCardId);
	return true;
}

void UAcGameFlowSubsystem::CloseTurretCard()
{
	if (!TurretCard) return;
	if (const AAcHUD* Hud = AAcHUD::Get(this))
	{
		if (const TSharedPtr<SAcRoot> Root = Hud->Root()) Root->RemoveLayer(TurretCard.ToSharedRef());
	}
	TurretCard.Reset();
	SetBlocked(false);
	if (AAcPilotPawn* Pilot = AAcPilotPawn::Find(this)) Pilot->SetScreenOpen(false);
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
	UE_LOG(LogAutocraft, Log, TEXT("turret card: closed"));
}

void UAcGameFlowSubsystem::ToggleTurretCard()
{
	if (TurretCard) CloseTurretCard();
	else OpenTurretCard();
}

void UAcGameFlowSubsystem::TickTurretCard()
{
	if (!TurretCard) return;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim ? Sim->MmoClient() : nullptr;
	const ac::net::Mirror* M = Mmo ? Mmo->Mirror() : nullptr;
	const ac::GameState* St = M ? &M->simulation().state : nullptr;
	const std::optional<ac::Structure> B = St ? St->structure(TurretCardId) : std::nullopt;
	// Felled, or gone with a flight: the card goes too.
	if (!B || !ac::Defence::upgradable(B->kind) || Mmo->Travelling())
	{
		CloseTurretCard();
		return;
	}
	FAcTurretCardInfo Info;
	Info.Id = B->id;
	Info.Name = FString(UTF8_TO_TCHAR(ac::Simulation::title(B->kind).c_str())).ToUpper();
	Info.Role = Role(B->kind);
	Info.Hp = B->hp;
	Info.MaxHp = ac::Rules::hp(B->kind);
	Info.Builder = AcProgressHud::BuiltBy(Mmo, B->id);
	if (size_t(ac::Pilot::player) < St->players.size())
	{
		Info.Ore = St->players[ac::Pilot::player].ore;
		Info.Hydrogen = St->players[ac::Pilot::player].hydrogen;
	}
	if (const ac::net::ProgressMessage* P = Mmo->Progress())
	{
		Info.bKnown = true;
		if (const auto K = P->turretKills.find(B->id); K != P->turretKills.end()) Info.Kills = K->second;
	}
	// A card per stat it takes: now, and with the next mark (the core's `Defence`).
	const ac::TurretMarks Marks = B->marks.value_or(ac::TurretMarks{});
	const ac::UnitStats Base = *ac::Rules::turret(B->kind);
	const ac::UnitStats Now = ac::Defence::apply(Base, Marks);
	for (const ac::DefenceStat Stat : ac::allCases<ac::DefenceStat>())
	{
		if (!ac::Defence::takes(B->kind, Stat)) continue;
		ac::TurretMarks Up = Marks;
		Up.of(Stat) = FMath::Min<int64>(Up.of(Stat) + 1, ac::Defence::marksMax);
		FAcDefenceLine X;
		X.Name = FString(UTF8_TO_TCHAR(ac::Defence::title(Stat))).ToUpper();
		X.Mark = Marks.of(Stat);
		X.Most = ac::Defence::marksMax;
		X.Now = Value(Stat, Now);
		X.Next = Value(Stat, ac::Defence::apply(Base, Up));
		const ac::ShopPrice Price = ac::Defence::turretPrice(B->kind, Stat, X.Mark + 1);
		X.Ore = Price.ore;
		X.Hydrogen = Price.hydrogen;
		Info.Lines.Add(X);
		Info.Stats.Add(int32(Stat));
	}
	// The server checks the distance; the card says so ahead.
	if (const ac::Unit* U = M->driven() ? M->unit(*M->driven()) : nullptr)
	{
		const double Gap = ac::distance(U->position, B->position) - ac::Rules::radius(B->kind) - ac::Rules::radius(U->kind);
		if (Gap > ac::Defence::turretReach)
			Info.WhyNot = FString::Printf(TEXT("Too far: stand within %d cells of the %s"), int32(ac::Defence::turretReach),
				UTF8_TO_TCHAR(ac::Simulation::title(B->kind).c_str()));
	}
	TurretCard->SetInfo(Info);
	if (Mmo->TurretRefusal() != TurretRefusalShown)
	{
		TurretRefusalShown = Mmo->TurretRefusal();
		if (!TurretRefusalShown.IsEmpty()) TurretCard->SetRefusal(TurretRefusalShown);
	}
	if (Mmo->TurretKept() != TurretKeptShown)
	{
		TurretKeptShown = Mmo->TurretKept();
		TurretCard->SetKept();
	}
}
