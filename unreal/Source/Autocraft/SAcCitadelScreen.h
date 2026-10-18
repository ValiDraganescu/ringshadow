// `SAcCitadelScreen`: the realm's Citadel in the MMORPG mode (H, at the
// realm's Citadel; docs/prospector-progression.md "The realm's tax", C1).
// Drawn by hand in the HUD's chrome, the rig screen's sibling:
//
//   THE REALM'S CITADEL  built by Pilot 045B        REALM                 ESC
//   ┌ REALM WALLET ───────────────┐ ┌ DONATE ─────────────────────────────┐
//   │ STARDUST ORE   1,240        │ │ YOUR WALLET  ORE 4,820 · MH 610     │
//   │ METALLIC HYDROGEN  56       │ │ ORE  [ 100 ] [ 1,000 ] [ ALL ]      │
//   │ TAX 10% of every unload …   │ │ MH   [ 100 ] [ 1,000 ] [ ALL ]      │
//   │ YOU GAVE  tax · donated     │ │                                     │
//   └─────────────────────────────┘ └─────────────────────────────────────┘
//   ┌ REALM PROJECTS: four cards, START ────────────────────────────────┐
//   ┌ STANDINGS · THIS WEEK ─────────────────────────── 3 d 4 h left ───┐
//   │ #  PILOT              ORE     BUGS   WAVES HELD   BUILT          │
//   │ 1  ● Vesper  frontier 18,400  12     0            2              │
//   │ …  the top 8 by ore, then ··· and the pilot's own row (gold)     │
//   └──────────────────────────────────────────────────────────────────┘
//   what was given, or why not                                  the keys
//
// Tabs: REALM, then SHOP (C2: blueprint copies and consumables from the
// pilot's own wallet, docs/prospector-progression.md "The Citadel shop"),
// MISSIONS, DEFENCE and MERCS, each its own body under the same header and
// foot. Tab switches them.
//
//   SHOP   ┌ BLUEPRINTS ──────────────────────────┐ ┌ SUPPLIES ───────────┐
//          │ Citadel   ×1        80 ore      [BUY] │ │ 1 Repair kit   [BUY]│
//          │ Hab Dome  ×0  LV 8  250 ore · 40 MH … │ │ 2/5 · 120 ore · 10 MH│
//          └───────────────────────────────────────┘ └─────────────────────┘
//
// MISSIONS (K2, docs/missions.md): the Citadel's board, two offers a tier,
// signed up for here (keys 1-6) and followed in the Journal (L).
//
//   MISSIONS ┌ EASY ─────────────┐ ┌ HARD ──────────────┐ ┌ PERILOUS ─────────┐
//            │ 1 SURVEY · Home    │ │ 3 HOLD · Tern Ridge │ │ 5 HUNT · Slag …    │
//            │ Mark the Ground    │ │ Hold the Line       │ │ Into the Nest      │
//            │ what to do …  TAKE │ │ …              TAKE │ │ …             TAKE │
//            └────────────────────┘ └─────────────────────┘ └────────────────────┘
//
// DEFENCE (D1, docs/mmorpg-m8.md): the Citadel's four corner turrets, a
// card per stat (fire rate, damage, range) with its marks, what the next one
// does and costs, bought from the realm wallet by any pilot (keys 1-3).
//
//   DEFENCE ┌ FIRE RATE ●●○ ────┐ ┌ DAMAGE ●○○ ───────┐ ┌ RANGE ○○○ ────────┐
//           │ 4.8 → 5.6 rounds/s │ │ 2.5 → 3.1 a round  │ │ 7.5 → 8.5 cells    │
//           │ 500 ore · 50 MH UP │ │ 500 ore · 50 MH UP │ │ 250 ore        UP  │
//           └────────────────────┘ └────────────────────┘ └────────────────────┘
//
// MERCS (H2, docs/prospector-progression.md "Mercenaries"): the pilot's
// mercenaries (what each does, what it delivered), HIRE with the fee, and
// the deposit fields one can be sent to (keys 1-8 pick one; SEND sends).
//
//   MERCS  ┌ YOUR MERCENARIES 1/2 ───────────────┐ ┌ DEPOSIT FIELDS ─────────┐
//          │ MERC · FLINT  mining Field 2 …       │ │ 1 Prospect Landing · F1 │
//          │   delivered 1,240 ore   [SEND][DISMISS]│ │ 2 …                     │
//          │ [HIRE] 1,000 ore · 100 MH · wage 8 MH │ │                         │
//          └──────────────────────────────────────┘ └─────────────────────────┘
//
// The screen only shows and asks: a donation goes to its owner (`OnDonate`),
// which asks the server and hands the answer back through `SetInfo` (a
// refusal through `SetRefusal`, a kept one through `SetKept`). The game goes
// on behind; the pilot's unit stands while it is open (AcGameFlow).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/// A line of the Citadel's shop (C2): a blueprint or a consumable.
struct FAcShopLine
{
	FString Name;
	/// What it does (a consumable), in a few words.
	FString Note;
	/// Held now, and the most held (a consumable; 0: no cap).
	int64 Held = 0;
	int64 Stack = 0;
	/// What the next one costs.
	int64 Ore = 0;
	int64 Hydrogen = 0;
	/// The level a blueprint's building opens at, when the pilot is below
	/// it (0: it builds now). Buying below the floor is allowed.
	int64 OpensAt = 0;
	/// A consumable's key in the field ("1").
	FString Key;
	/// C2: a blueprint the pilot has learned (the shop sells each once).
	bool bKnown = false;

	bool operator==(const FAcShopLine&) const = default;
};

/// An offer on the Citadel's board (K2).
struct FAcMissionLine
{
	uint32 Id = 0;
	/// "EASY", "HARD", "PERILOUS" (0, 1, 2).
	int32 Tier = 0;
	/// "SURVEY", and the region's name ("Rust Hollow"; empty: home).
	FString Kind;
	FString Where;
	FString Title;
	FString Instructions;
	/// "1,500 XP · 450 ore · 40 MH · Hab Dome blueprint ×1".
	FString Reward;

	bool operator==(const FAcMissionLine&) const = default;
};

/// A mercenary of the pilot's (H2).
struct FAcMercLine
{
	uint32 Id = 0;
	FString Name;
	/// "Mining Field 2 · Prospect Landing", "Down: back to work in 7 min".
	FString Doing;
	/// 0 working, 1 down, 2 its field mined out, 3 no Citadel to haul to.
	int32 State = 0;
	/// Its field's region id and index (`FAcMercField`).
	FString Region;
	int64 Field = 0;
	int64 Delivered = 0;
	/// N4: a guard (answers the hunt), and away on one now.
	bool bGuard = false;
	bool bHunting = false;

	bool operator==(const FAcMercLine&) const = default;
};

/// A deposit field a mercenary can be sent to (H2).
struct FAcMercField
{
	FString Region;
	/// "Prospect Landing · Field 2".
	FString Name;
	int64 Field = 0;
	int64 Deposits = 0;
	int64 Ore = 0;

	bool operator==(const FAcMercField&) const = default;
};

/// A stat of the Citadel's corner turrets on the DEFENCE tab (D1).
struct FAcDefenceLine
{
	/// "FIRE RATE", "DAMAGE", "RANGE".
	FString Name;
	/// Marks bought, and the most.
	int64 Mark = 0;
	int64 Most = 3;
	/// What it is now and with the next mark ("4.8 rounds a second").
	FString Now;
	FString Next;
	/// The next mark's price from the realm wallet.
	int64 Ore = 0;
	int64 Hydrogen = 0;

	bool operator==(const FAcDefenceLine&) const = default;
};

/// R2: a realm project on the REALM tab: what it does, the next one's price
/// (from the realm wallet), and the one running: who started it, seconds of
/// world time left (0: none running) out of a world day.
struct FAcProjectLine
{
	/// "BEACON".
	FString Title;
	/// "Dropship fares halved realm-wide".
	FString Effect;
	int64 Ore = 0;
	int64 Hydrogen = 0;
	bool bRunning = false;
	FString By;
	double Left = 0;
	double Day = 2400;

	bool operator==(const FAcProjectLine&) const = default;
};

/// S4: a pilot's row in the REALM tab's standings: its rank this week, its
/// name (a frontier pilot's callsign in its paint's way, a human's, "You"),
/// its four counts.
struct FAcStandingLine
{
	FString Name;
	bool bFrontier = false;
	bool bYou = false;
	/// A frontier pilot's paint, as its dot on the map (-1: a human's).
	int32 Paint = -1;
	int64 Rank = 0;
	int64 Ore = 0;
	int64 Bugs = 0;
	int64 Waves = 0;
	int64 Built = 0;

	bool operator==(const FAcStandingLine&) const = default;
};

/// N4: one of the hunt's party on the REALM tab: the caller, a frontier
/// defender (in its paint), a guard mercenary (whose).
struct FAcHuntPartyLine
{
	FString Name;
	/// "CALLER", "FRONTIER", "GUARD · Pilot 1A2B".
	FString Tag;
	/// A frontier pilot's paint (-1: none).
	int32 Paint = -1;
	bool bThere = false;
	bool bDown = false;

	bool operator==(const FAcHuntPartyLine&) const = default;
};

/// What the Citadel screen shows.
struct FAcCitadelScreenInfo
{
	/// S4: the most standings rows shown before the pilot's own.
	static constexpr int32 StandingsTop = 8;
	/// The server has told the realm's numbers (none yet: "Waiting…").
	bool bKnown = false;
	/// The realm wallet, and the tax in percent.
	int64 RealmOre = 0;
	int64 RealmHydrogen = 0;
	int64 TaxPercent = 0;
	/// What this pilot has given in all: in tax, and donated.
	int64 TaxedOre = 0;
	int64 TaxedHydrogen = 0;
	int64 DonatedOre = 0;
	int64 DonatedHydrogen = 0;
	/// The pilot's wallet where it is.
	int64 Ore = 0;
	int64 Hydrogen = 0;
	/// Who built the Citadel ("Pilot 045B", "you"; empty: not known).
	FString Builder;
	/// Why a donation can't go from here (empty: it can: at the Citadel).
	FString DonateWhyNot;
	/// C2, the shop: a line per blueprint (the build menu's order) and per
	/// consumable (keys 1-4); why nothing can be bought from here.
	TArray<FAcShopLine> Blueprints;
	TArray<FAcShopLine> Items;
	FString ShopWhyNot;
	/// K2, the board: the offers (by tier), the missions in the Journal
	/// and the most it holds, why none can be signed up for from here.
	TArray<FAcMissionLine> Offers;
	int32 Taken = 0;
	int32 Most = 3;
	FString MissionsWhyNot;
	/// H2, the mercenaries: the pilot's, the fields, the terms (how many at
	/// its level, the next one's fee, the wage at dawn, ore an hour, the
	/// level the next place opens at: 0 none), its level; why none can be
	/// hired from here.
	TArray<FAcMercLine> Mercs;
	TArray<FAcMercField> MercFields;
	int64 MercLimit = 0;
	int64 MercFeeOre = 0;
	int64 MercFeeHydrogen = 0;
	int64 MercWage = 0;
	double MercRate = 0;
	int64 MercNextLevel = 0;
	int64 Level = 1;
	FString MercsWhyNot;
	/// D1, the DEFENCE tab: the corner turrets' three stats; why none can
	/// be upgraded from here.
	TArray<FAcDefenceLine> Defence;
	FString DefenceWhyNot;
	/// R2, the REALM tab's projects, in `ac::RealmProject::Kind`'s order;
	/// why none can be started from here.
	TArray<FAcProjectLine> Projects;
	FString ProjectsWhyNot;
	/// S4, the REALM tab's standings: the top `StandingsTop` by ore, then
	/// the pilot's own when it is not among them; the week's time left.
	bool bStandingsKnown = false;
	TArray<FAcStandingLine> Standings;
	FString WeekLeft;
	/// N4, the REALM tab's hunt: told yet, on (gathering or the assault),
	/// its state ("NOT CALLED TODAY", "GATHERING", "THE ASSAULT", "SHE
	/// FELL", "THE HUNT FAILED") and its line (`net::huntLine`), where she
	/// is, why it can't be called now (empty: it can), the party.
	bool bHuntKnown = false;
	bool bHuntOn = false;
	FString HuntState;
	FString HuntLine;
	FString HuntWhere;
	FString HuntWhyNot;
	TArray<FAcHuntPartyLine> HuntParty;

	bool operator==(const FAcCitadelScreenInfo&) const = default;
};

DECLARE_DELEGATE_TwoParams(FAcOnDonate, int64 /*Ore*/, int64 /*Hydrogen*/);
DECLARE_DELEGATE_TwoParams(FAcOnShopBuy, bool /*bItem*/, int32 /*Index*/);
DECLARE_DELEGATE_OneParam(FAcOnMissionTake, int32 /*Index*/);
/// H2: hire (0), dismiss (1) or send (2) mercenary `Merc` (an index in
/// `Mercs`) to field `Field` (an index in `MercFields`); N4: hire a guard
/// (3), make mercenary `Merc` a guard (4, `Field` 1) or a miner (4, 0).
DECLARE_DELEGATE_ThreeParams(FAcOnMerc, int32 /*Act*/, int32 /*Merc*/, int32 /*Field*/);
DECLARE_DELEGATE_OneParam(FAcOnUpgrade, int32 /*Stat*/);
/// R2: start the realm project of kind `Kind` (`ac::RealmProject::Kind`).
DECLARE_DELEGATE_OneParam(FAcOnProject, int32 /*Kind*/);

class AUTOCRAFT_API SAcCitadelScreen : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SAcCitadelScreen) {}
		SLATE_EVENT(FAcOnDonate, OnDonate)
		SLATE_EVENT(FAcOnShopBuy, OnBuy)
		SLATE_EVENT(FAcOnMissionTake, OnTake)
		SLATE_EVENT(FAcOnMerc, OnMerc)
		SLATE_EVENT(FAcOnUpgrade, OnUpgrade)
		SLATE_EVENT(FAcOnProject, OnProject)
		SLATE_EVENT(FSimpleDelegate, OnHunt)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	/// The screen's tabs: the realm's wallet, the shop (C2), the missions
	/// board (K2), the corner turrets' upgrades (D1), the mercenaries (H2).
	enum class ETab : uint8 { Realm, Shop, Missions, Defence, Mercs };
	static constexpr int32 TabCount = 5;

	/// The donate buttons: 100, 1,000 and all, of ore (0-2) and of MH (3-5).
	static constexpr int32 DonateButtons = 6;

	void Construct(const FArguments& InArgs);
	virtual ~SAcCitadelScreen() override;

	/// What it shows now (each frame from the flow; repaints on a change).
	void SetInfo(const FAcCitadelScreenInfo& Info);
	const FAcCitadelScreenInfo& GetInfo() const { return Info; }
	/// Why the last ask was refused (empty: none). Shown at the foot.
	void SetRefusal(const FString& Why);
	const FString& RefusalText() const { return Refusal; }
	/// The server kept the last ask: the foot says what was given.
	void SetKept();
	const FString& KeptText() const { return Kept; }

	ETab Tab() const { return CurrentTab; }
	void SetTab(ETab InTab);

	/// The shop (C2): why line `Index` of the blueprints (or of the
	/// consumables, `bItem`) can't be bought now (empty: it can), and its
	/// ask (a click; keys 1-8 on the SHOP tab buy blueprints).
	FString BuyWhyNot(bool bItem, int32 Index) const;
	void Buy(bool bItem, int32 Index);
	/// The board (K2): why offer `Index` can't be signed up for now (empty:
	/// it can), and its ask (a click; keys 1-6 on the MISSIONS tab).
	FString TakeWhyNot(int32 Index) const;
	void Take(int32 Index);
	/// The mercenaries (H2): why one can't be hired, mercenary `Index`
	/// can't be sent to the picked field, or let go (empty: it can); the
	/// asks (clicks); the field picked (keys 1-8 on the MERCS tab).
	FString HireWhyNot() const;
	FString SendWhyNot(int32 Index) const;
	/// N4: `bGuard` hires a guard (it answers the hunt); `Role` makes
	/// mercenary `Index` a guard, or a miner again.
	void Hire(bool bGuard = false);
	void Role(int32 Index);
	void Send(int32 Index);
	void Dismiss(int32 Index);
	int32 PickedField() const { return Picked; }
	void PickField(int32 Index);
	/// The DEFENCE tab (D1): why stat `Index` (0 fire rate, 1 damage, 2
	/// range) can't take its next mark now (empty: it can), and its ask (a
	/// click; keys 1-3 on the DEFENCE tab).
	FString UpgradeWhyNot(int32 Index) const;
	void Upgrade(int32 Index);
	/// The REALM tab's projects (R2): why project `Index` can't be started
	/// now (empty: it can), and its ask (a click; keys 7-0 on the REALM tab).
	FString ProjectWhyNot(int32 Index) const;
	void StartProject(int32 Index);
	/// N4: why THE HUNT can't be called now (empty: it can), and its ask
	/// (a click on the REALM tab).
	FString HuntWhyNot() const;
	void CallHunt();

	/// What button `Index` gives now (ore, MH): "all" is the wallet's.
	TPair<int64, int64> Amount(int32 Index) const;
	/// Why button `Index` can't give (empty: it can).
	FString WhyNot(int32 Index) const;
	/// Button `Index`'s ask (a click, keys 1-6).
	void Donate(int32 Index);
	void Close();
	/// The keys the flow hands it: true when used.
	bool Key(const FKey& InKey);

	// SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
		FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(100, 100); }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override { return FReply::Handled(); }
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void OnMouseLeave(const FPointerEvent& Event) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
	/// Something clickable.
	struct FTarget
	{
		FBox2f Box;
		/// What it is (ERow in the .cpp).
		int32 Row = 0;
		/// A donate button's index, a tab's.
		int32 Index = 0;
	};

	void Layout(FVector2f ViewSize) const;
	int32 TargetAt(FVector2f LocalPoint) const;
	void Press(const FTarget& T);
	EActiveTimerReturnType Animate(double Now, float Dt);
	double Clock() const;

	void PaintHeader(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintRealm(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintShop(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	/// S4: the REALM tab's standings, under the projects.
	void PaintStandings(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	/// N4: THE HUNT, beside the standings.
	void PaintHunt(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintMissions(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintMercs(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintDefence(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintFoot(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P) const;
	void PaintButton(FSlateWindowElementList& Out, int32 Layer, const FGeometry& P, const FTarget& T, const FString& Label,
		bool bLit, const FLinearColor& Accent) const;

	FAcCitadelScreenInfo Info;
	ETab CurrentTab = ETab::Realm;
	FString Refusal;
	FString Kept;
	/// What the last ask gave (ore, MH), for the foot once kept.
	TPair<int64, int64> Asked{0, 0};
	/// What the last shop ask bought ("Hab Dome blueprint"; empty: the last
	/// ask was a donation).
	FString Bought;
	/// What the last board ask signed up for (its title; empty: none).
	FString Signed;
	/// What the last mercenary ask was ("Hired a mercenary."; empty: none),
	/// and the field picked on the MERCS tab.
	FString MercAsked;
	int32 Picked = 0;
	/// What the last DEFENCE ask upgraded ("fire rate, mark 2"; empty: none).
	FString Upgraded;
	/// R2: the project the last REALM ask started ("Beacon"; empty: none).
	FString Started;
	/// N4: the last ask called the hunt (where).
	FString HuntCalled;

	FAcOnDonate OnDonateDelegate;
	FAcOnShopBuy OnBuyDelegate;
	FAcOnMissionTake OnTakeDelegate;
	FAcOnMerc OnMercDelegate;
	FAcOnUpgrade OnUpgradeDelegate;
	FAcOnProject OnProjectDelegate;
	FSimpleDelegate OnHuntDelegate;
	FSimpleDelegate OnCloseDelegate;
	double OpenedAt = 0;
	/// When the realm wallet last grew (its flash).
	double GrewAt = -10;
	TSharedPtr<FActiveTimerHandle> Timer;

	mutable TArray<FTarget> Targets;
	mutable FBox2f WalletBox;
	mutable FBox2f DonateBox;
	mutable FBox2f ProjectsBox;
	mutable FBox2f StandingsBox;
	mutable FBox2f HuntBox;
	mutable FBox2f BlueprintBox;
	mutable FBox2f ItemBox;
	mutable FBox2f TierBox[3];
	mutable FBox2f MercBox;
	mutable FBox2f FieldBox;
	mutable FBox2f FootBox;
	mutable float PanelScale = 1.0f;
	mutable FVector2f PanelOrigin = FVector2f::ZeroVector;
	mutable FVector2f PanelSize = FVector2f::ZeroVector;

	int32 Hovered = INDEX_NONE;
	int32 Pressed = INDEX_NONE;
};
