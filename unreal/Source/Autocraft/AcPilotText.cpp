#include "AcPilotText.h"

#include "AcCitadelAct.h"
#include "AcMuleAct.h"
#include "Mule.h"
#include "AcShuttleAct.h"
#include "AcKindTable.h"
#include "AcProgressHud.h"
#include "Defence.h"
#include "Flight.h"
#include "Leveling.h"
#include "NetProtocol.h"
#include "Progression.h"
#include "Rules.h"

#include <cmath>

namespace
{
	FString Utf8(const std::string& S) { return UTF8_TO_TCHAR(S.c_str()); }

	const TCHAR* const Dot = TEXT("   ·   ");

	/// The move line, or the held one while the machine is held in place.
	FString MoveLine(const ac::Unit& U, const FAcPilotText& T)
	{
		return U.anchor.value_or(0) > 0 ? T.Held.Get(T.Move) : T.Move;
	}

	FString BuildLine() { return TEXT("B  build menu   ·   number  pick   ·   click  place"); }

	// The MMORPG Prospector's tips (`MmoTips`), the key card's too (U2).
	const TCHAR* const TipDrill = TEXT("Hold click  drill ore at a deposit");
	const TCHAR* const TipDerrick = TEXT("click  enter your Derrick   ·   R + hold click  repair it");
	const TCHAR* const TipRepair = TEXT("Hold click  repair a damaged building");
	const TCHAR* const TipMap = TEXT("M  world map: on a pad, fly to another region");
	const TCHAR* const TipView = TEXT("V  first or third person   ·   Esc  the top-down view");
	const TCHAR* const TipSkills = TEXT("K  skill tree   ·   Q C G  your actives   ·   1 2 3 4  supplies");
	const TCHAR* const TipCitadel = TEXT("H  at the Citadel: the realm's wallet, the shop, missions   ·   L  your Journal");
	const TCHAR* const TipMusic = TEXT("F7 F8 F9  music back, pause, next   ·   [ or ]  vote the track down or up");

	/// `transit`: a move under way that holds the unit, for the work ring.
	struct FTransit
	{
		FString Label;
		double Progress;
		TOptional<FString> Prompt;
	};
	TOptional<FTransit> Transit(const ac::Unit& U)
	{
		if (const std::optional<double> J = U.jump()) return FTransit{TEXT("JET PACK"), *J, {}};
		if (!U.anchor || *U.anchor <= 0 || *U.anchor >= 1) return {};
		const double S = *U.anchor;
		if (U.kind == ac::UnitKind::scorpion)
		{
			return U.anchored == true ? FTransit{TEXT("BURYING"), S, FString(TEXT("R  cancel"))}
									  : FTransit{TEXT("DIGGING OUT"), 1 - S, FString(TEXT("R  cancel"))};
		}
		return U.anchored == true ? FTransit{TEXT("ANCHORING"), S, FString(TEXT("R  cancel"))}
								  : FTransit{TEXT("PACKING UP"), 1 - S, FString(TEXT("R  cancel"))};
	}

	double Dist(ac::Vec2 A, ac::Vec2 B) { return std::hypot(A.x - B.x, A.y - B.y); }

	/// A builder's scaffold close by: from its edge, cells.
	constexpr double ScaffoldNear = 3.0;

	/// The building going up that the unit welds, else its side's scaffold
	/// it aims at or stands by (`ScaffoldNear`, the nearest).
	const ac::Structure* Welded(const ac::GameState& S, const ac::Unit& U, const std::optional<ac::PilotTarget>& Target)
	{
		if (!ac::PilotBuild::builds(U.kind)) return nullptr;
		auto Going = [&](const int64 Id) -> const ac::Structure* {
			for (const ac::Structure& B : S.structures)
			{
				if (B.id == Id) return B.complete() ? nullptr : &B;
			}
			return nullptr;
		};
		if (U.task == ac::Unit::Task::building && U.structure)
		{
			if (const ac::Structure* B = Going(*U.structure)) return B;
		}
		if (Target)
		{
			if (const ac::PilotTarget::Scaffold* T = Target->as<ac::PilotTarget::Scaffold>())
			{
				if (const ac::Structure* B = Going(T->_0)) return B;
			}
		}
		const ac::Structure* Near = nullptr;
		double NearEdge = ScaffoldNear;
		for (const ac::Structure& B : S.structures)
		{
			if (B.complete() || !S.allied(B.owner, U.owner)) continue;
			const double Edge = Dist(B.position, U.position) - ac::Rules::radius(B.kind);
			if (Edge <= NearEdge)
			{
				Near = &B;
				NearEdge = Edge;
			}
		}
		return Near;
	}
}

namespace
{
	/// Where a load is dropped off: the side's finished Citadel, and in a
	/// world every finished Hab Dome too, the realm's (R1, the core's depot rule).
	bool IsDepot(const ac::Simulation& Sim, const ac::Structure& B, const int64 Owner)
	{
		if (B.owner != Owner || !B.complete()) return false;
		return B.kind == ac::StructureKind::citadel || (B.kind == ac::StructureKind::habDome && Sim.progress);
	}

	/// "the Citadel", "the Citadel or a Hab Dome": where it unloads now.
	FString DepotWords(const ac::Simulation& Sim, const int64 Owner)
	{
		for (const ac::Structure& B : Sim.state.structures)
		{
			if (B.kind == ac::StructureKind::habDome && IsDepot(Sim, B, Owner)) return TEXT("your Citadel or a Hab Dome");
		}
		return TEXT("the Citadel");
	}
}

FAcPilotText FAcPilotText::Of(const ac::UnitKind Kind)
{
	const FString Walk = TEXT("WASD  walk   ·   mouse  look"), Fire = TEXT("act.strike");
	auto Act = [](const TCHAR* Title, const FString& Icon, const TCHAR* Detail) { return FAct{Title, Icon, Detail}; };
	FAcPilotText T;
	switch (Kind)
	{
	case ac::UnitKind::prospector:
		T.Aimed = TEXT("Hold click  strike");
		T.Help = TEXT("Hold click  drill, strike, repair   ·   click  enter a Derrick   ·   R + hold click  repair (a Derrick too)")
			TEXT("   ·   walk into the Citadel to unload");
		T.Move = Walk;
		break;
	case ac::UnitKind::ranger:
		T.Act = Act(TEXT("Fire"), Fire, TEXT("Hold click: fires at what the sight is on"));
		T.Aimed = TEXT("Hold click  fire");
		T.Help = TEXT("Hold click  fire at what the sight is on");
		T.Move = Walk;
		break;
	case ac::UnitKind::comet:
		T.Act = Act(TEXT("Fire"), Fire, TEXT("Hold click: both pistols fire at what the sight is on"));
		T.Aimed = TEXT("Hold click  fire");
		T.Help = TEXT("Hold click  fire both pistols   ·   walk into a cliff  jet pack over it");
		T.Move = Walk;
		break;
	case ac::UnitKind::firefly:
		T.Act = Act(TEXT("Flame"), Fire, TEXT("Hold click: burns everything on the line ahead"));
		T.Aimed = TEXT("Hold click  flame");
		T.Help = TEXT("Hold click  flame everything on the line (where the tail points: the ring)");
		T.Move = TEXT("W S  drive   ·   A D  turn the hull   ·   mouse  aim the tail (the ring shows the flame)");
		break;
	case ac::UnitKind::juggernaut:
		T.Act = Act(TEXT("Grenades"), Fire, TEXT("Hold click: Breacher grenades; what they hit is slowed"));
		T.Aimed = TEXT("Hold click  fire grenades");
		T.Help = TEXT("Hold click  fire grenades (they slow)");
		T.Move = Walk;
		break;
	case ac::UnitKind::longbow:
		T.Act = Act(TEXT("Fire"), Fire, TEXT("Hold click: plasma blobs from the packed railgun; anchored, a shell that splashes"));
		T.Aimed = TEXT("Hold click  fire");
		T.Help = TEXT("Hold click  fire   ·   R  anchored mode: range 13, nothing under 2, walks slowly");
		T.Move = TEXT("W S  drive   ·   A D  turn the hull   ·   mouse  aim the turret (the ring shows the gun)");
		T.Held = FString(TEXT("mouse  aim the turret   ·   R  tank mode to drive"));
		break;
	case ac::UnitKind::dropship:
		T.Act = Act(TEXT("Heal"), TEXT("act.heal"), TEXT("Hold click: heals the unit under the sight, using energy"));
		T.Aimed = TEXT("Hold click  heal");
		T.Help = TEXT("Hold click  heal what the sight is on   ·   R  load the unit below, or unload all");
		T.Move = TEXT("WASD  fly   ·   mouse  look");
		break;
	case ac::UnitKind::kestrel:
		T.Act = Act(TEXT("Rockets / Rail"), Fire, TEXT("Hold click: rockets at a ground target under the sight, a rail gun slug at a flyer"));
		T.Aimed = TEXT("Hold click  rockets / rail gun");
		T.Help = TEXT("Hold click  rockets at the ground, rail gun at flyers   ·   flies over cliffs");
		T.Move = TEXT("WASD  fly   ·   mouse  look");
		break;
	case ac::UnitKind::hailstorm:
		T.Act = Act(TEXT("Flak"), Fire, TEXT("Hold click: flak at the flyer the sight is on; it bursts round it"));
		T.Aimed = TEXT("Hold click  fire flak");
		T.Help = TEXT("Hold click  fire flak at flyers (air only; the burst hurts flyers round it)");
		T.Move = TEXT("W S  drive   ·   A D  turn the hull   ·   mouse  aim the mount (the ring shows the guns)");
		break;
	case ac::UnitKind::peregrine:
		T.Act = Act(TEXT("Missiles"), Fire, TEXT("Hold click: seeker missiles at the flyer the sight is on (air only)"));
		T.Aimed = TEXT("Hold click  fire missiles");
		T.Help = TEXT("Hold click  fire missiles at flyers (they fly: a fast target can outrun a salvo)   ·   flies over cliffs");
		T.Move = TEXT("WASD  fly   ·   mouse  look");
		break;
	case ac::UnitKind::atlas:
		T.Act = Act(TEXT("Cannons"), Fire, TEXT("Hold click: twin siege cannons at the ground; the shell splashes enemies"));
		T.Aimed = TEXT("Hold click  fire cannons");
		T.Help = TEXT("Hold click  fire the cannons (ground only, enemies round the target take splash)   ·   R  Quake stomp: 30 damage and a slow within 2 cells");
		T.Move = TEXT("W S  walk   ·   A D  turn the legs   ·   mouse  aim the torso (the ring shows the cannons)");
		break;
	case ac::UnitKind::scorpion:
		T.Act = Act(TEXT("Sting"), Fire, TEXT("Hold click, buried: locks on for a second, then one heavy sting; 25 s to reload"));
		T.Aimed = TEXT("Hold click  sting (buried)");
		T.Help = TEXT("R  bury / dig out (2 s each)   ·   buried, hold click  lock on and sting what the sight is on");
		T.Move = Walk;
		T.Held = FString(TEXT("hold click  sting   ·   R  dig out to move"));
		break;
	default:
		T.Aimed = TEXT("Hold click  fire");
		T.Help = TEXT("Hold click  fire at what the sight is on");
		T.Move = Walk;
		break;
	}
	return T;
}

TOptional<FAcPilotFrame> FAcPilotFrame::Of(const ac::Simulation& Sim)
{
	const std::optional<ac::Unit> U = Sim.pilotUnit();
	if (!U) return {};
	FAcPilotFrame F;
	F.Unit = *U;
	F.Target = Sim.pilotTarget(*U);
	F.Sight = Sim.pilotSight(*U);
	F.Ability = Sim.pilotAbility(*U);
	if (U->task == ac::Unit::Task::attacking && U->target) F.Patient = Sim.state.unit(*U->target);
	return F;
}

bool AcPilotText::HasCab(const ac::UnitKind Driven)
{
	// A kind without a cockpit of its own rides in its stand-in's (AcKindTable.h).
	return AcKinds::Look(AcKinds::CockpitKind(Driven)).bCab;
}

FString AcPilotText::Title(const ac::UnitKind Kind) { return Utf8(ac::Simulation::title(Kind)); }
FString AcPilotText::Title(const ac::StructureKind Kind) { return Utf8(ac::Simulation::title(Kind)); }

FString AcPilotText::FormatG(const double V)
{
	// %g with 6 significant digits, as Foundation's String(format:).
	char Buf[64];
	std::snprintf(Buf, sizeof(Buf), "%g", V);
	return UTF8_TO_TCHAR(Buf);
}

FString AcPilotText::Help(const ac::Unit& U)
{
	const FAcPilotText T = FAcPilotText::Of(U.kind);
	TArray<FString> Lines = {FString::Printf(TEXT("Tab  next %s   ·   V  view   ·   Esc  leave"), *Title(U.kind)), T.Help,
		ac::PilotBuild::builds(U.kind) ? BuildLine() : FString(), MoveLine(U, T)};
	Lines.RemoveAll([](const FString& L) { return L.IsEmpty(); });
	return FString::Join(Lines, TEXT("\n"));
}

FString AcPilotText::Hint(const ac::Unit& U)
{
	const FString Help = FAcPilotText::Of(U.kind).Help;
	const int32 At = Help.Find(Dot);
	return At == INDEX_NONE ? Help : Help.Left(At);
}

TArray<FString> AcPilotText::Tips(const ac::Unit& U)
{
	const FAcPilotText T = FAcPilotText::Of(U.kind);
	TArray<FString> Parts;
	T.Help.ParseIntoArray(Parts, Dot, false);
	if (Parts.Num() > 0) Parts.RemoveAt(0);
	TArray<FString> Tips = {MoveLine(U, T), FString::Join(Parts, Dot), ac::PilotBuild::builds(U.kind) ? BuildLine() : FString(),
		FString::Printf(TEXT("V  first or third person   ·   Tab  next %s"), *Title(U.kind)),
		TEXT("Esc or F  leave   ·   Space or E  same as a click"), TEXT("Option-scroll  hearing range"),
		TipMusic,
		TEXT("Z or X  pick a level-up card   ·   hold Option  free the pointer to click one")};
	Tips.RemoveAll([](const FString& L) { return L.IsEmpty(); });
	return Tips;
}

TArray<FString> AcPilotText::MmoTips(const ac::Simulation& Sim, const ac::Unit& U)
{
	const FAcPilotText T = FAcPilotText::Of(U.kind);
	if (!ac::PilotBuild::builds(U.kind)) return Tips(U);
	bool bOre = false, bCitadel = false, bDerrick = false, bHurt = false;
	for (const ac::OreDeposit& P : Sim.state.patches) bOre |= P.remaining > 0;
	for (const ac::Structure& B : Sim.state.structures)
	{
		if (B.owner != U.owner) continue;
		bCitadel |= B.kind == ac::StructureKind::citadel && B.complete();
		bDerrick |= B.kind == ac::StructureKind::derrick && B.complete();
		bHurt |= B.hp < ac::Rules::hp(B.kind) && B.complete();
	}
	TArray<FString> Tips = {MoveLine(U, T)};
	if (bOre) Tips.Add(TipDrill);
	const FString Depot = DepotWords(Sim, U.owner);
	Tips.Add(bCitadel ? FString::Printf(TEXT("Walk into  %s to unload the ore"), Depot == TEXT("the Citadel") ? TEXT("your Citadel") : *Depot)
					  : FString(TEXT("B  build a Citadel, to bring ore home")));
	if (bDerrick) Tips.Add(TipDerrick);
	else if (bHurt) Tips.Add(TipRepair);
	Tips.Add(BuildLine());
	Tips.Add(TipMap);
	Tips.Add(TipView);
	Tips.Add(TipSkills);
	if (bCitadel) Tips.Add(TipCitadel);
	return Tips;
}

TArray<AcPilotText::FKeyGroup> AcPilotText::KeyCard()
{
	// Each tip split at its dots, each part at its first double space: the
	// key, then what it does ("Hold click  drill ore at a deposit").
	auto Group = [](const TCHAR* Title, const TArray<FString>& Lines) {
		FKeyGroup G{Title, {}};
		for (const FString& Line : Lines)
		{
			TArray<FString> Parts;
			Line.ParseIntoArray(Parts, Dot, true);
			for (const FString& Part : Parts)
			{
				const int32 At = Part.Find(TEXT("  "));
				if (At == INDEX_NONE) G.Rows.Add({Part, FString()});
				else G.Rows.Add({Part.Left(At), Part.Mid(At + 2).TrimStart()});
			}
		}
		return G;
	};
	const FString Walk = FAcPilotText::Of(ac::UnitKind::prospector).Move;
	return {Group(TEXT("ON FOOT"), {Walk, TipDrill, TipRepair, TipDerrick, TEXT("Walk into  your Citadel to unload the ore")}),
		Group(TEXT("BUILDING"), {BuildLine()}),
		Group(TEXT("THE REACH"), {TipMap, TipView, TipSkills, TipCitadel}),
		Group(TEXT("MUSIC"), {TipMusic})};
}

TArray<ac::StructureKind> AcPilotText::BuildKinds(const bool bMmo)
{
	// The MMORPG mode: every blueprint's building, in a fixed order; those the
	// pilot lacks or is under the floor of greyed (`AcProgressHud::BlueprintRefusal`).
	TArray<ac::StructureKind> Out;
	// C2: the Target Dummy last (free: no blueprint, no number key).
	if (bMmo)
	{
		for (const ac::StructureKind K : ac::Progression::blueprintKinds()) Out.Add(K);
		Out.Add(ac::StructureKind::dummy);
	}
	else for (const ac::StructureKind K : ac::PilotBuild::kinds) Out.Add(K);
	return Out;
}

TOptional<FAcSightInfo> AcPilotText::Sight(const ac::Simulation& Sim, const std::optional<ac::PilotSight>& S)
{
	if (!S) return {};
	const FString Side = S->friend_ ? TEXT("") : TEXT("Enemy ");
	FAcSightInfo I;
	I.Distance = S->distance;
	I.Range = S->range;
	I.MinRange = S->minRange;
	I.bInRange = S->inRange;
	I.bFriend = S->friend_;
	if (const std::optional<ac::Unit> V = Sim.state.unit(S->target))
	{
		const bool bSlowed = V->slowUntil.value_or(0) > Sim.state.time;
		I.Title = Side + Title(V->kind) + (bSlowed ? TEXT(" (slowed)") : TEXT(""));
		I.Hp = FMath::Max(V->hp, 0.0);
		I.MaxHp = Sim.maxHP(*V);
		return I;
	}
	const std::optional<ac::Structure> B = Sim.state.structure(S->target);
	if (!B) return {};
	I.Title = Side + Title(B->kind);
	I.Hp = FMath::Max(B->hp, 0.0);
	I.MaxHp = ac::Rules::hp(B->kind);
	return I;
}

TOptional<FString> AcPilotText::Refusal(const ac::Unit& U, const FAcSightInfo& S)
{
	if (S.bFriend)
	{
		if (S.Hp >= S.MaxHp) return FString(TEXT("Full health"));
		if (U.energy.value_or(0) <= 0) return FString(TEXT("Out of energy"));
		if (S.bInRange) return {};
		return FString::Printf(TEXT("Out of reach  %.1f / %.0f"), S.Distance, S.Range);
	}
	if (S.Distance < S.MinRange) return FString::Printf(TEXT("Too close  %.1f / min %s"), S.Distance, *FormatG(S.MinRange));
	if (S.bInRange) return {};
	return FString::Printf(TEXT("Out of range  %.1f / %.0f"), S.Distance, S.Range);
}

FAcPilotInfo AcPilotText::Info(const ac::Simulation& Sim, const FAcPilotFrame& F, const FAcPilotInputs& In)
{
	const ac::GameState& S = Sim.state;
	const ac::Unit& U = F.Unit;
	const FAcPilotText Text = FAcPilotText::Of(U.kind);
	FAcPilotInfo Info;
	Info.Name = Title(U.kind).ToUpper();
	Info.Hp = FMath::Max(U.hp, 0.0);
	Info.MaxHp = Sim.maxHP(U);
	Info.Cargo = U.carrying;
	Info.bHydrogen = U.hydrogen == true;
	Info.Heading = U.look();
	Info.Position = U.position;
	Info.Help = Help(U);
	// C4: what the Mule holds, driving it or near one's own (the HUD's line).
	const AcMuleAct::FOwn MuleOwn = In.bMmo ? AcMuleAct::OwnOf(In.Mmo) : AcMuleAct::FOwn();
	if (U.kind == ac::UnitKind::mule) Info.MuleLoad = AcMuleAct::LoadLine(U);
	else if (const ac::Unit* Near = In.bMmo ? AcMuleAct::Shown(Sim, U, MuleOwn) : nullptr) Info.MuleLoad = AcMuleAct::LoadLine(*Near);
	// The jet pack's MH (F1b): what a jump burns, where the keys are told.
	if (const int64 Jet = Sim.pilotJumpHydrogen(U); Jet > 0 && Sim.pilotJumps(U))
	{
		Info.Help += FString::Printf(TEXT("   ·   walk into a cliff  jet pack over it, %lld MH"), static_cast<long long>(Jet));
	}
	using ETask = ac::Unit::Task;
	if (U.task == ETask::mining)
	{
		Info.Progress = 1 - FMath::Max(U.timer, 0.0) / ac::Rules::miningTime;
		Info.ProgressLabel = TEXT("DRILLING");
		Info.Tone = EAcTone::Working;
		Info.Prompt = FString(TEXT("Let go or walk away to stop"));
	}
	else if (U.task == ETask::inDerrick)
	{
		Info.Progress = 1 - FMath::Max(U.timer, 0.0) / ac::Rules::hydrogenTime;
		Info.ProgressLabel = TEXT("PUMPING MH");
		Info.Tone = EAcTone::Working;
		Info.Inside = FString(TEXT("INSIDE THE DERRICK"));
		Info.Prompt = FString(TEXT("Pumping MH: you step out with a full load"));
	}
	else if (const TOptional<FTransit> T = Transit(U))
	{
		Info.Progress = T->Progress;
		Info.ProgressLabel = T->Label;
		Info.Tone = EAcTone::Working;
		Info.Prompt = T->Prompt;
	}
	else
	{
		const std::optional<ac::PilotTarget>& Target = F.Target;
		// C1: nothing else to act on: a gun station or the Citadel's door.
		const TOptional<AcCitadelAct::FAct> AtCitadel = !Target && In.bMmo ? AcCitadelAct::Find(Sim, U) : TOptional<AcCitadelAct::FAct>();
		// C4, before the Citadel's: the Mule's perch and hopper, the Foundry's Mule.
		const TOptional<AcMuleAct::FAct> AtMule = !Target && In.bMmo ? AcMuleAct::Find(Sim, U, MuleOwn) : TOptional<AcMuleAct::FAct>();
		// C4: the walk-in Dropship's buttons, or a call from a Landing Pad.
		const TOptional<AcShuttleAct::FAct> AtShip = !Target && In.bMmo ? AcShuttleAct::Find(Sim, U) : TOptional<AcShuttleAct::FAct>();
		if (U.task == ETask::atGun)
		{
			Info.Prompt = FString(TEXT("Click  fire   ·   Esc  disengage"));
			Info.Tone = F.Sight && F.Sight->inRange ? EAcTone::Enemy : EAcTone::Ready;
		}
		else if (AtShip)
		{
			Info.Prompt = AtShip->Prompt();
			Info.Tone = AtShip->Why ? EAcTone::Blocked : EAcTone::Ready;
		}
		// Driving a Mule: the door with the crosshair on it before climbing off.
		else if (AtCitadel && U.kind == ac::UnitKind::mule)
		{
			Info.Prompt = AtCitadel->Prompt();
			Info.Tone = AtCitadel->Why ? EAcTone::Blocked : EAcTone::Ready;
		}
		else if (AtMule)
		{
			Info.Prompt = AtMule->Prompt();
			Info.Tone = AtMule->Why ? EAcTone::Blocked : EAcTone::Ready;
		}
		else if (AtCitadel)
		{
			Info.Prompt = AtCitadel->Prompt();
			Info.Tone = AtCitadel->Why ? EAcTone::Blocked : EAcTone::Ready;
		}
		else if (!Target)
		{
			if (Sim.pilot && Sim.pilot->mend && U.kind == ac::UnitKind::prospector) Info.Prompt = FString(TEXT("Nothing to repair ahead"));
			else if (U.carrying > 0 && MuleOwn.Mule) Info.Prompt = FString::Printf(TEXT("Tip it into your Mule's hopper, or walk into %s"), *DepotWords(Sim, U.owner));
			else if (U.carrying > 0) Info.Prompt = FString::Printf(TEXT("Walk into %s to unload"), *DepotWords(Sim, U.owner));
		}
		else if (Target->is<ac::PilotTarget::Enemy>())
		{
			Info.Prompt = U.anchor.value_or(0) >= 1 ? FString(TEXT("Hold click  fire shell")) : Text.Aimed;
			Info.Tone = EAcTone::Enemy;
		}
		else if (Target->is<ac::PilotTarget::Heal>())
		{
			Info.Prompt = Text.Aimed;
			Info.Tone = EAcTone::Ready;
		}
		else if (Target->is<ac::PilotTarget::Patch>())
		{
			Info.Prompt = FString(TEXT("Hold click  drill ore"));
			Info.Tone = EAcTone::Ready;
		}
		else if (Target->is<ac::PilotTarget::Scaffold>())
		{
			Info.Prompt = FString(TEXT("Click  carry on building"));
			Info.Tone = EAcTone::Ready;
		}
		else if (const ac::PilotTarget::Repair* R = Target->as<ac::PilotTarget::Repair>())
		{
			// It costs materials: the price, or why it can't go on.
			const std::optional<ac::Structure> B = Sim.state.structure(R->_0);
			if (const std::optional<std::string> Why = B ? Sim.repairRefusal(U, *B) : std::nullopt)
			{
				Info.Prompt = FString(UTF8_TO_TCHAR(Why->c_str()));
				Info.Tone = EAcTone::Blocked;
			}
			else
			{
				const TOptional<FString> Price = AcPilotText::RepairPrice(Sim, U, R->_0);
				Info.Prompt = Price ? FString::Printf(TEXT("Hold click  repair   ·   %s"), **Price) : FString(TEXT("Hold click  repair"));
				Info.Tone = EAcTone::Ready;
			}
		}
		else if (const ac::PilotTarget::Derrick* D = Target->as<ac::PilotTarget::Derrick>())
		{
			Info.Prompt = FString(D->busy ? TEXT("Derrick in use, wait") : TEXT("Click  enter the Derrick"));
			Info.Tone = D->busy ? EAcTone::Blocked : EAcTone::Ready;
		}
		else if (Target->is<ac::PilotTarget::Full>())
		{
			Info.Prompt = FString::Printf(TEXT("Hands full: unload at %s"), *DepotWords(Sim, U.owner));
			Info.Tone = EAcTone::Blocked;
		}
		// A damaged building behind what the click goes to (its Derrick,
		// ore): R takes the click for the repair.
		if (Target && (Target->is<ac::PilotTarget::Derrick>() || Target->is<ac::PilotTarget::Patch>() || Target->is<ac::PilotTarget::Full>())
			&& Sim.pilotRepair(U) && Info.Prompt)
		{
			const ac::PilotTarget Mend = *Sim.pilotRepair(U);
			const ac::PilotTarget::Repair* R = Mend.as<ac::PilotTarget::Repair>();
			const std::optional<ac::Structure> B = R ? Sim.state.structure(R->_0) : std::nullopt;
			const std::optional<std::string> Why = B ? Sim.repairRefusal(U, *B) : std::nullopt;
			const TOptional<FString> Price = R ? AcPilotText::RepairPrice(Sim, U, R->_0) : TOptional<FString>();
			Info.Prompt = *Info.Prompt + (Why ? FString::Printf(TEXT("   ·   R: %s"), UTF8_TO_TCHAR(Why->c_str()))
				: Price ? FString::Printf(TEXT("   ·   R + hold click  repair (%s)"), **Price) : FString(TEXT("   ·   R + hold click  repair")));
		}
	}
	// Walked into a cliff with too little MH for the jet pack (F1b): why,
	// for a moment after each try.
	if (Sim.pilotJumpDry && S.time - *Sim.pilotJumpDry < 1.5)
	{
		if (const std::optional<std::string> Why = ac::JetJump::refusal(Sim.pilotJumpHydrogen(U), Sim.pilotHydrogenOnHand(U)))
		{
			Info.Prompt = FString(UTF8_TO_TCHAR(Why->c_str()));
			Info.Tone = EAcTone::Blocked;
		}
	}
	// The building being welded, or a scaffold close by: its progress on the
	// work ring and the dashboard (up close its life bar is out of view;
	// docs/mmorpg-m5.md, E3).
	if (!Info.Progress)
	{
		if (const ac::Structure* B = Welded(S, U, F.Target))
		{
			Info.Progress = B->progress();
			Info.ProgressLabel = Title(B->kind).ToUpper();
			if (U.task == ETask::building)
			{
				Info.Tone = EAcTone::Working;
				if (!Info.Prompt) Info.Prompt = FString::Printf(TEXT("Welding the %s"), *Title(B->kind));
			}
		}
	}
	Info.Sight = Sight(Sim, F.Sight);
	// A building of the realm in sight (R1): who built it.
	if (Info.Sight && Info.Sight->bFriend && F.Sight && In.Mmo && Sim.state.structure(F.Sight->target))
	{
		if (const FString By = AcProgressHud::BuiltBy(In.Mmo, F.Sight->target); !By.IsEmpty())
		{
			Info.Sight->Title += FString::Printf(TEXT("  ·  built by %s"), *By);
		}
		// D3: a turret of the realm close enough to upgrade.
		const std::optional<ac::Structure> T = Sim.state.structure(F.Sight->target);
		if (!Info.Prompt && T && ac::Defence::upgradable(T->kind) && T->complete()
			&& ac::distance(U.position, T->position) - ac::Rules::radius(T->kind) - ac::Rules::radius(U.kind) <= ac::Defence::turretReach)
		{
			Info.Prompt = FString::Printf(TEXT("U  ·  the %s's card: its marks"), *Title(T->kind));
		}
	}
	if (!Info.Prompt && Info.Sight)
	{
		if (const TOptional<FString> Why = Refusal(U, *Info.Sight))
		{
			Info.Prompt = Why;
			Info.Tone = EAcTone::Blocked;
		}
	}
	Info.Hit = In.Hit;

	// `pilotBuildInfo`: the note, and the prompt for a building held out
	// (the ghost itself is E8's).
	Info.Note = In.Note;
	if (In.Placing)
	{
		if (!In.PlacingSpot)
		{
			Info.Prompt = FString(TEXT("Face an MH well"));
			Info.Tone = EAcTone::Blocked;
		}
		else
		{
			std::optional<std::string> Why = Sim.buildRefusal(*In.Placing, ac::Pilot::player);
			if (!Why) Why = Sim.placementRefusal(*In.Placing, *In.PlacingSpot);
			if (!Why && In.PlacingRefusal) Why = std::string(TCHAR_TO_UTF8(**In.PlacingRefusal));
			Info.Prompt = Why ? Utf8(*Why) : FString::Printf(TEXT("Click  build %s   ·   Esc  cancel"), *Title(*In.Placing));
			Info.Tone = Why ? EAcTone::Blocked : EAcTone::Ready;
		}
	}
	Info.bThirdPerson = In.bThirdPerson;
	Info.bCab = HasCab(U.kind) && !Info.bThirdPerson;
	Info.Lamps = Info.Tone == EAcTone::Working ? EAcCabLamps::Working
		: U.anchor.value_or(0) >= 1		   ? EAcCabLamps::Locked
											   : EAcCabLamps::Idle;
	Info.Console = Console(Sim, F, Info, In);

	// The compass points home with a load, else to the nearest field.
	auto Edge = [&U](const ac::Vec2 P, const double R) { return FMath::Max(0.0, Dist(P, U.position) - R); };
	const ac::Structure* Home = nullptr;
	if (U.kind == ac::UnitKind::prospector && U.carrying > 0)
	{
		for (const ac::Structure& B : S.structures)
		{
			if (IsDepot(Sim, B, U.owner) && (!Home || Dist(B.position, U.position) < Dist(Home->position, U.position)))
			{
				Home = &B;
			}
		}
	}
	if (Home)
	{
		Info.Goal = FAcPilotGoal{Title(Home->kind).ToUpper(), Home->position, Edge(Home->position, ac::Rules::radius(Home->kind))};
	}
	else if (U.kind == ac::UnitKind::prospector && U.task != ETask::mining && U.task != ETask::inDerrick)
	{
		const ac::OreDeposit* Near = nullptr;
		for (const ac::OreDeposit& P : S.patches)
		{
			if (P.remaining > 0 && (!Near || Dist(P.position, U.position) < Dist(Near->position, U.position))) Near = &P;
		}
		if (Near && Edge(Near->position, 1) > 1.5) Info.Goal = FAcPilotGoal{TEXT("ORE"), Near->position, Edge(Near->position, 1)};
	}
	return Info;
}

FAcLevelInfo AcPilotText::Level(const ac::Simulation& Sim, const ac::UnitKind Kind)
{
	const ac::PilotRecord& R = Sim.levels();
	FAcLevelInfo L;
	L.Level = (int32)R.level(Kind);
	const double Xp = R.xp(Kind);
	const double From = ac::Leveling::xp(L.Level);
	if (L.Level < ac::Leveling::cap)
	{
		const double To = ac::Leveling::xp(L.Level + 1);
		L.Fraction = FMath::Clamp((Xp - From) / FMath::Max(To - From, 1.0), 0.0, 1.0);
		L.Text = FString::Printf(TEXT("%lld / %lld XP"), (long long)std::floor(Xp), (long long)To);
	}
	else
	{
		L.Fraction = 1;
		L.Text = TEXT("MAX LEVEL");
	}
	return L;
}

FAcTurretFeed AcPilotText::Feed(const ac::Structure& Citadel, const int32 Gun, const double Yaw, const double Pitch,
	const TConstArrayView<double> Shots, const double Now)
{
	FAcTurretFeed Out;
	Out.Gun = FMath::Clamp(Gun, 0, 3);
	Out.Title = FString::Printf(TEXT("TURRET %d · %s · CITADEL"), Out.Gun + 1, AcCitadelAct::Compass(Out.Gun));
	Out.ArcHalf = ac::Defence::gunArcHalf;
	Out.Off = FMath::Clamp(std::remainder(Yaw - ac::Defence::cornerAngle(Out.Gun), 2.0 * UE_DOUBLE_PI), -Out.ArcHalf, Out.ArcHalf);
	Out.PitchMin = ac::Defence::gunPitchMin;
	Out.PitchMax = ac::Defence::gunPitchMax;
	Out.Pitch = FMath::Clamp(Pitch, Out.PitchMin, Out.PitchMax);
	const std::optional<ac::UnitStats> W = ac::Defence::weapon(Citadel);
	const double Cooldown = FMath::Max(W ? W->cooldown : ac::UnitStats::citadelGuns.cooldown, 1e-3);
	double Last = -1e9;
	for (const double T : Shots)
		if (T <= Now) Last = FMath::Max(Last, T);
	Out.Ready = FMath::Clamp((Now - Last) / Cooldown, 0.0, 1.0);
	// C2: the gun's own heat, overheat and spin.
	if (Citadel.guns)
	{
		const ac::CornerGun& G = (*Citadel.guns)[Out.Gun];
		Out.HeatLevel = FMath::Clamp(G.heat, 0.0, 1.0);
		Out.bOverheat = G.overheated;
		Out.Spin = FMath::Clamp(G.spin, 0.0, 1.0);
	}
	Out.Heat = Out.bOverheat ? FAcTurretFeed::HeatPips
		: FMath::Clamp(FMath::CeilToInt32(FAcTurretFeed::HeatPips * Out.HeatLevel - 1e-9), 0, FAcTurretFeed::HeatPips);
	const double Damage = W ? W->damage : ac::UnitStats::citadelGuns.damage;
	Out.Marks = FString::Printf(TEXT("%lld/s · %.1f · V0 %lld"), (long long)FMath::RoundToDouble(1.0 / Cooldown), Damage,
		(long long)FMath::RoundToDouble(ac::Rules::roundSpeed));
	return Out;
}

TOptional<ac::Vec3> AcPilotText::BallisticImpact(const ac::Simulation& Sim, const ac::Vec3 Muzzle, const ac::Vec3 Velocity, const int64 Skip)
{
	// The first thing at `P`: the ground under it, or a standing building's
	// box (its footprint's radius, its height).
	auto Inside = [&](const ac::Vec3 P, const double T)
	{
		if (P.z < Sim.groundZ(ac::Vec2(P.x, P.y))) return true;
		for (const ac::Structure& B : Sim.state.structures)
		{
			if (!(B.hp > 0) || B.downUntil || (B.id == Skip && T < ac::Rules::roundSelfGrace)) continue;
			const double R = ac::Rules::radius(B.kind), G = Sim.groundZ(B.position);
			if (std::abs(P.x - B.position.x) <= R && std::abs(P.y - B.position.y) <= R && P.z >= G
				&& P.z <= G + ac::Rules::height(B.kind) * FMath::Max(0.1, B.progress()))
			{
				return true;
			}
		}
		return false;
	};
	constexpr double Dt = 1.0 / 240.0;
	for (double T = Dt; T <= ac::Rules::roundLife; T += Dt)
	{
		if (!Inside(ac::Rules::roundAt(Muzzle, Velocity, T), T)) continue;
		double Lo = T - Dt, Hi = T;
		for (int32 K = 0; K < 10; ++K)
		{
			const double M = (Lo + Hi) / 2;
			(Inside(ac::Rules::roundAt(Muzzle, Velocity, M), M) ? Hi : Lo) = M;
		}
		return ac::Rules::roundAt(Muzzle, Velocity, Hi);
	}
	return {};
}

FString AcPilotText::Status(const FAcPilotFrame& F)
{
	const ac::Unit& U = F.Unit;
	using ETask = ac::Unit::Task;
	switch (U.task)
	{
	case ETask::mining: return TEXT("Drilling");
	case ETask::inDerrick: return TEXT("Inside the Derrick");
	case ETask::building: return TEXT("Building");
	case ETask::repairing: return TEXT("Repairing");
	case ETask::atGun: return TEXT("At a corner gun");
	default: break;
	}
	if (U.jumpFrom) return TEXT("Jet pack");
	if (U.anchor && *U.anchor > 0)
	{
		return U.anchored == true ? (*U.anchor >= 1 ? TEXT("Anchored") : TEXT("Anchoring")) : TEXT("Packing up");
	}
	if (F.Patient) return TEXT("Healing");
	return U.carrying > 0 ? TEXT("Carrying a load home") : TEXT("Ready");
}

FAcCardInfo AcPilotText::Console(const ac::Simulation& Sim, const FAcPilotFrame& F, const FAcPilotInfo& P,
	const FAcPilotInputs& In)
{
	const ac::Unit& U = F.Unit;
	FAcCardInfo Info;
	Info.Title = Title(U.kind);
	Info.Status = Status(F);
	Info.Hp = FMath::Max(U.hp, 0.0);
	Info.MaxHp = Sim.maxHP(U);
	Info.Icon = FAcConsoleInfo::IconName((int32)U.kind);
	Info.Progress = P.Progress;
	if (P.Progress) Info.ProgressLabel = P.ProgressLabel;
	if (ac::PilotBuild::builds(U.kind))
	{
		const bool bMh = U.hydrogen == true;
		Info.Cargo = U.carrying > 0 ? FString::Printf(TEXT("%lld %s"), (long long)U.carrying, bMh ? TEXT("MH") : TEXT("ore"))
									: FString(TEXT("empty"));
		if (U.carrying > 0) Info.CargoIcon = FString(bMh ? TEXT("well") : TEXT("ore"));
	}
	if (U.kind == ac::UnitKind::mule)
	{
		// C4: its cargo is its rider; the bay shows its load, out of its room.
		const ac::Haul H = U.haul.value_or(ac::Haul{});
		Info.Cargo = FString::Printf(TEXT("%lld ore  %lld MH  ·  %lld left"), (long long)H.ore, (long long)H.hydrogen, (long long)ac::Mule::room(U));
		Info.CargoIcon = FString(TEXT("ore"));
	}
	else if (U.cargo)
	{
		Info.Cargo = FString::Printf(TEXT("%lld / %lld"), (long long)Sim.slotsUsed(U), (long long)Sim.cargoSlots(U));
		if (!U.cargo->empty())
		{
			if (const std::optional<ac::Unit> First = Sim.state.unit(U.cargo->front()))
			{
				Info.CargoIcon = FAcConsoleInfo::IconName((int32)First->kind);
			}
		}
	}
	if (U.energy)
	{
		Info.Energy = *U.energy;
		Info.MaxEnergy = ac::Rules::maxEnergy;
	}
	Info.Note = P.Note;
	Info.Prompt = P.Prompt ? *P.Prompt : Hint(U);
	if (P.Prompt) Info.Tone = P.Tone;
	Info.Tips = In.bMmo ? MmoTips(Sim, U) : Tips(U);
	// The first ten minutes: the line under the card is the step's (O1).
	if (const ac::net::Contract* Step = In.Mmo && ac::PilotBuild::builds(U.kind) ? AcProgressHud::FirstStep(*In.Mmo) : nullptr)
	{
		Info.Tips = {AcProgressHud::FirstTip(*Step)};
	}
	Info.Level = Level(Sim, U.kind);

	if (!ac::PilotBuild::builds(U.kind))
	{
		// `soldierCard`: its act, lit while it applies to what the sight is
		// on, and its ability on R, greyed with the reason.
		if (const TOptional<FAcPilotText::FAct> Act = FAcPilotText::Of(U.kind).Act)
		{
			const FString Range = Sim.pilotShoots(U) ? TEXT(", range ") + FormatG(Sim.weapon(U).range) : FString();
			FAcCardButton B;
			B.Slot = 0;
			B.Key = TEXT("hold");
			B.Title = Act->Title;
			B.Icon = Act->Icon;
			B.bLit = F.Target.has_value();
			B.Detail = Act->Detail + Range;
			Info.Buttons.Add(B);
		}
		if (F.Ability)
		{
			// No icon: the button shows its title ("Anchor Mode", "Unload All").
			FAcCardButton B;
			B.Slot = 1;
			B.Key = TEXT("R");
			B.Title = Utf8(F.Ability->title);
			B.bEnabled = F.Ability->action.has_value();
			if (F.Ability->why) B.Why = Utf8(*F.Ability->why);
			Info.Buttons.Add(B);
		}
		return Info;
	}
	// The MMORPG mode: the build wheel (C2) replaces the list: the card
	// keeps the Prospector's own buttons.
	if ((In.bBuildMenu || In.Placing) && !In.bMmo)
	{
		int32 I = 0;
		for (const ac::StructureKind K : BuildKinds(In.bMmo))
		{
			const std::optional<std::string> Why = Sim.buildRefusal(K, ac::Pilot::player);
			FAcCardButton B;
			B.Slot = I;
			B.Key = FString::FromInt(I + 1);
			B.Title = Title(K);
			// C2: the blueprint copies held, each used up by one building.
			if (In.bMmo) B.Title += FString::Printf(TEXT("  ×%lld"), (long long)AcProgressHud::Copies(In.Mmo, K));
			B.Icon = FAcConsoleInfo::StructureIconName((int32)K);
			// The price the pilot pays: a world's prices with an MH part (P6),
			// Prefab and Efficient plans off it (the mirror has its progress).
			const ac::Price Cost = Sim.buildCost(K, ac::Pilot::player);
			B.Ore = (int32)Cost.ore;
			B.Hydrogen = (int32)Cost.hydrogen;
			B.Time = ac::Rules::buildTime(K);
			B.bEnabled = !Why;
			if (Why) B.Why = Utf8(*Why);
			if (In.bMmo && Why)
			{
				// The MMORPG test mode: the price that is short, as "needs 100 MH".
				const ac::Player& Me = Sim.state.players[ac::Pilot::player];
				TArray<FString> Short;
				if (Me.ore < B.Ore) Short.Add(FString::Printf(TEXT("%d ore"), B.Ore));
				if (Me.hydrogen < B.Hydrogen) Short.Add(FString::Printf(TEXT("%d MH"), B.Hydrogen));
				if (!Short.IsEmpty()) B.Why = TEXT("needs ") + FString::Join(Short, TEXT(" and "));
			}
			if (In.bMmo)
			{
				// No blueprint, or under its floor: that first.
				if (const FString Lock = AcProgressHud::BlueprintRefusal(In.Mmo, K); !Lock.IsEmpty())
				{
					B.bEnabled = false;
					B.Why = Lock;
					B.WhyShort = AcProgressHud::BlueprintTag(In.Mmo, K);
				}
			}
			B.bLit = In.Placing == K;
			Info.Buttons.Add(B);
			++I;
		}
		FAcCardButton Back;
		Back.Slot = 14;
		Back.Key = TEXT("Esc");
		Back.Title = TEXT("Back");
		Back.Icon = TEXT("act.back");
		Info.Buttons.Add(Back);
		Info.bBuildList = In.bMmo;
		return Info;
	}
	const std::optional<ac::PilotTarget>& T = F.Target;
	using ETask = ac::Unit::Task;
	auto Button = [](int32 Slot, const TCHAR* Key, const TCHAR* Title, const TCHAR* Icon, bool bLit, const TCHAR* Detail)
	{
		FAcCardButton B;
		B.Slot = Slot;
		B.Key = Key;
		B.Title = Title;
		B.Icon = Icon;
		B.bLit = bLit;
		B.Detail = FString(Detail);
		return B;
	};
	// C2: the cutter's reach in cells, edge to edge (its weapon line).
	const FString Strike = FString::Printf(TEXT("Hold click at an enemy: %s damage, reach %s cells"),
		*FormatG(Sim.weapon(U).damage), *FormatG(Sim.weapon(U).range));
	Info.Buttons = {
		Button(0, TEXT("hold"), TEXT("Drill ore"), TEXT("ore"), (T && T->is<ac::PilotTarget::Patch>()) || U.task == ETask::mining,
			TEXT("Hold click at an ore deposit")),
		Button(1, TEXT("click"), TEXT("Pump MH"), TEXT("derrick"), (T && T->is<ac::PilotTarget::Derrick>()) || U.task == ETask::inDerrick,
			TEXT("Click at your Derrick")),
		Button(2, TEXT("hold"), TEXT("Repair"), TEXT("act.repair"), (T && T->is<ac::PilotTarget::Repair>()) || U.task == ETask::repairing,
			TEXT("Hold click at a damaged building (hold R too at a Derrick)")),
		Button(3, TEXT("hold"), TEXT("Strike"), TEXT("act.strike"), T && T->is<ac::PilotTarget::Enemy>(), *Strike),
		Button(10, TEXT("B"), TEXT("Build"), TEXT("act.build"), (T && T->is<ac::PilotTarget::Scaffold>()) || U.task == ETask::building,
			TEXT("Open the buildings")),
	};
	return Info;
}

TOptional<FString> AcPilotText::RepairPrice(const ac::Simulation& Sim, const ac::Unit& U, const int64 Id)
{
	const std::optional<ac::Structure> B = Sim.state.structure(Id);
	if (!B) return {};
	const auto [Ore, Hydrogen] = ac::Simulation::repairCost(B->kind, 100, Sim.skilled(U), Sim.fullHP(*B));
	// Whole units from 10 up, a tenth under (the debt is paid in whole units).
	auto Amount = [](const double V) { return V >= 10 ? FString::Printf(TEXT("%.0f"), V) : FormatG(FMath::RoundToDouble(V * 10) / 10); };
	TArray<FString> Parts;
	if (Ore > 0) Parts.Add(Amount(Ore) + TEXT(" ore"));
	if (Hydrogen > 0) Parts.Add(Amount(Hydrogen) + TEXT(" MH"));
	if (Parts.IsEmpty()) return {};
	return FString::Join(Parts, TEXT(" ")) + TEXT(" / 100 HP");
}
