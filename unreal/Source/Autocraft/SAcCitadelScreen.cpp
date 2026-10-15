#include "SAcCitadelScreen.h"

#include "AcHudStyle.h"
#include "AcHudText.h"

#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"

namespace
{
	// The panel, in the HUD root's points (as SAcRigScreen).
	constexpr float PanelW = 1040;
	constexpr float Pad = 26;
	constexpr float HeaderH = 92;
	constexpr float Gap = 16;
	constexpr float WalletW = 440;
	constexpr float BodyH = 300;
	constexpr float FootH = 56;
	constexpr float ButtonW = 120;
	constexpr float ButtonH = 50;
	/// R2: the REALM tab's projects, under the wallet and the donations.
	constexpr float ProjectsH = 196;
	/// S4: the REALM tab's standings, under the projects: a row a pilot,
	/// the top 8 and the pilot's own after a gap.
	constexpr float StandingRowH = 24;
	constexpr float StandingsH = 74 + 9 * StandingRowH + 12;
	/// N4: the standings' width; THE HUNT beside them.
	constexpr float StandingsW = 640;
	/// The hunt's party rows.
	constexpr float HuntRowH = 22;
	constexpr int32 HuntRows = 6;

	enum ERow : int32
	{
		RowDonate = 1,
		RowTab = 2,
		RowClose = 3,
		RowBuyBlueprint = 4,
		RowBuyItem = 5,
		RowTake = 6,
		RowHire = 7,
		RowSend = 8,
		RowDismiss = 9,
		RowField = 10,
		RowUpgrade = 11,
		RowProject = 12,
		RowRole = 13,
		RowHunt = 14,
	};

	// The mercenaries' rows (H2).
	constexpr float MercsW = 600;
	constexpr float MercRowH = 64;
	constexpr float FieldRowH = 30;
	constexpr int32 FieldRows = 8;

	/// The mercenaries' orange (their paint, `UAcWorldRenderer::MercenaryTeam`).
	FLinearColor MercOrange() { return FAcHudStyle::Srgb(1.0, 0.6, 0.22); }

	/// The tabs' names, in `ETab`'s order.
	const TCHAR* const TabNames[] = {TEXT("REALM"), TEXT("SHOP"), TEXT("MISSIONS"), TEXT("DEFENCE"), TEXT("MERCS")};

	// The shop's rows (C2).
	constexpr float BlueprintsW = 620;
	constexpr float BlueprintRowH = 27;
	constexpr float ItemRowH = 62;
	constexpr float BuyW = 70;
	constexpr float BuyH = 24;

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
		EAcVAlign V = EAcVAlign::Top, EAcFontWeight Weight = EAcFontWeight::Bold, float Kern = 0)
	{
		FAcHudText T(S, FAcHudStyle::Font(Size, Weight), C, H, V);
		T.Accent = FLinearColor::Transparent;
		T.Kern = Kern;
		return T;
	}

	FString Grouped(int64 N) { return FText::AsNumber(N).ToString(); }

	/// `S` in lines no wider than `Width` at `Size`, at most `Max` (the last
	/// one cut with an ellipsis).
	TArray<FString> Wrapped(const FString& S, float Size, float Width, int32 Max)
	{
		auto W = [&](const FString& X) { return FAcHudText(X, FAcHudStyle::Font(Size, EAcFontWeight::Medium), FLinearColor::White).Measure().X; };
		TArray<FString> Words;
		S.ParseIntoArray(Words, TEXT(" "), true);
		TArray<FString> Out;
		FString Line;
		for (const FString& Word : Words)
		{
			const FString Longer = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
			if (Line.IsEmpty() || W(Longer) <= Width) Line = Longer;
			else
			{
				Out.Add(Line);
				Line = Word;
			}
		}
		if (!Line.IsEmpty()) Out.Add(Line);
		if (Out.Num() <= Max) return Out;
		FString Last = Out[Max - 1];
		while (!Last.IsEmpty() && W(Last + TEXT("…")) > Width) Last.LeftChopInline(1);
		Out.SetNum(Max - 1);
		Out.Add(Last + TEXT("…"));
		return Out;
	}

	/// A tier's colour: easy green, hard amber, perilous red.
	FLinearColor TierColor(int32 Tier)
	{
		return Tier == 0 ? FAcHudStyle::Srgb(0.45, 0.95, 0.62) : Tier == 1 ? FAcHudStyle::Srgb(1.0, 0.72, 0.3) : FAcHudStyle::Srgb(1.0, 0.36, 0.3);
	}

	FLinearColor Dim() { return FAcHudStyle::Srgb(0.42, 0.5, 0.57); }
	FLinearColor Faint() { return FAcHudStyle::Srgb(0.26, 0.32, 0.38); }
	FLinearColor Steel() { return FAcHudStyle::Srgb(0.3, 0.42, 0.52, 0.8); }
	FLinearColor Refused() { return FAcHudStyle::Srgb(1.0, 0.55, 0.42); }
	FLinearColor Ready() { return FAcHudStyle::Srgb(0.45, 0.95, 0.62); }
	/// The realm's gold: the Citadel's, the tax's.
	FLinearColor RealmGold() { return FAcHudStyle::Srgb(1.0, 0.82, 0.42); }

	/// "1,000 ore", "250 MH", "1,000 ore and 250 MH".
	FString Purse(int64 Ore, int64 Hydrogen)
	{
		if (Ore > 0 && Hydrogen > 0) return FString::Printf(TEXT("%s ore and %s MH"), *Grouped(Ore), *Grouped(Hydrogen));
		if (Hydrogen > 0) return FString::Printf(TEXT("%s MH"), *Grouped(Hydrogen));
		return FString::Printf(TEXT("%s ore"), *Grouped(Ore));
	}
}

void SAcCitadelScreen::Construct(const FArguments& InArgs)
{
	OnDonateDelegate = InArgs._OnDonate;
	OnBuyDelegate = InArgs._OnBuy;
	OnTakeDelegate = InArgs._OnTake;
	OnMercDelegate = InArgs._OnMerc;
	OnUpgradeDelegate = InArgs._OnUpgrade;
	OnProjectDelegate = InArgs._OnProject;
	OnHuntDelegate = InArgs._OnHunt;
	OnCloseDelegate = InArgs._OnClose;
	SetCanTick(false);
	OpenedAt = Clock();
	// The flashes fade: a repaint a frame while open.
	Timer = RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAcCitadelScreen::Animate));
}

SAcCitadelScreen::~SAcCitadelScreen() = default;

double SAcCitadelScreen::Clock() const { return FPlatformTime::Seconds(); }

EActiveTimerReturnType SAcCitadelScreen::Animate(double, float)
{
	Invalidate(EInvalidateWidgetReason::Paint);
	return EActiveTimerReturnType::Continue;
}

void SAcCitadelScreen::SetInfo(const FAcCitadelScreenInfo& In)
{
	if (In == Info) return;
	if (Info.bKnown && In.RealmOre + In.RealmHydrogen > Info.RealmOre + Info.RealmHydrogen) GrewAt = Clock();
	Info = In;
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetRefusal(const FString& Why)
{
	Refusal = Why;
	if (!Why.IsEmpty()) Kept.Reset();
	if (!Why.IsEmpty()) HuntCalled.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetKept()
{
	Refusal.Reset();
	Kept = !HuntCalled.IsEmpty() ? FString::Printf(TEXT("THE HUNT is called: the realm's defenders and guards gather at the ring in %s."), *HuntCalled)
		: !Started.IsEmpty() ? FString::Printf(TEXT("Started: the realm's %s, for a world day. Paid from the realm wallet."), *Started)
		: !Upgraded.IsEmpty() ? FString::Printf(TEXT("Upgraded: the corner turrets' %s. Paid from the realm wallet."), *Upgraded)
		: !MercAsked.IsEmpty() ? MercAsked
		: !Signed.IsEmpty() ? FString::Printf(TEXT("Signed up: %s. It's in your Journal (L)."), *Signed)
		: !Bought.IsEmpty()
		? FString::Printf(TEXT("Bought: %s. Paid from your wallet."), *Bought)
		: FString::Printf(TEXT("Given to the realm: %s. Every pilot's Citadel thanks you."), *Purse(Asked.Key, Asked.Value));
	HuntCalled.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::SetTab(const ETab InTab)
{
	if (CurrentTab == InTab) return;
	CurrentTab = InTab;
	Refusal.Reset();
	Kept.Reset();
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::BuyWhyNot(const bool bItem, const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	const TArray<FAcShopLine>& Lines = bItem ? Info.Items : Info.Blueprints;
	if (!Lines.IsValidIndex(Index)) return TEXT("Nothing there");
	if (!Info.ShopWhyNot.IsEmpty()) return Info.ShopWhyNot;
	const FAcShopLine& X = Lines[Index];
	if (X.Stack > 0 && X.Held >= X.Stack) return FString::Printf(TEXT("%s: you hold %lld, the most"), *X.Name, (long long)X.Stack);
	if (X.Ore > Info.Ore) return FString::Printf(TEXT("Not enough ore: %s, you have %s"), *Grouped(X.Ore), *Grouped(Info.Ore));
	if (X.Hydrogen > Info.Hydrogen) return FString::Printf(TEXT("Not enough MH: %s, you have %s"), *Grouped(X.Hydrogen), *Grouped(Info.Hydrogen));
	return FString();
}

void SAcCitadelScreen::Buy(const bool bItem, const int32 Index)
{
	if (const FString Why = BuyWhyNot(bItem, Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	const FAcShopLine& X = (bItem ? Info.Items : Info.Blueprints)[Index];
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Signed.Reset();
	Bought = bItem ? X.Name : X.Name + TEXT(" blueprint");
	MercAsked.Reset();
	Upgraded.Reset();
	Started.Reset();
	OnBuyDelegate.ExecuteIfBound(bItem, Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

TPair<int64, int64> SAcCitadelScreen::Amount(const int32 Index) const
{
	const bool bOre = Index < 3;
	const int32 Step = Index % 3;
	const int64 N = Step == 0 ? 100 : Step == 1 ? 1000 : (bOre ? Info.Ore : Info.Hydrogen);
	return bOre ? TPair<int64, int64>(N, 0) : TPair<int64, int64>(0, N);
}

FString SAcCitadelScreen::WhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.DonateWhyNot.IsEmpty()) return Info.DonateWhyNot;
	const TPair<int64, int64> A = Amount(Index);
	if (A.Key + A.Value <= 0) return Index < 3 ? TEXT("No ore in your wallet to give") : TEXT("No MH in your wallet to give");
	if (A.Key > Info.Ore) return FString::Printf(TEXT("Not enough ore: you have %s"), *Grouped(Info.Ore));
	if (A.Value > Info.Hydrogen) return FString::Printf(TEXT("Not enough MH: you have %s"), *Grouped(Info.Hydrogen));
	return FString();
}

void SAcCitadelScreen::Donate(const int32 Index)
{
	if (Index < 0 || Index >= DonateButtons) return;
	if (const FString Why = WhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = Amount(Index);
	Bought.Reset();
	Signed.Reset();
	MercAsked.Reset();
	Upgraded.Reset();
	Started.Reset();
	OnDonateDelegate.ExecuteIfBound(Asked.Key, Asked.Value);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::TakeWhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.Offers.IsValidIndex(Index)) return TEXT("No offer there");
	if (!Info.MissionsWhyNot.IsEmpty()) return Info.MissionsWhyNot;
	if (Info.Taken >= Info.Most)
		return FString::Printf(TEXT("Your Journal holds %d missions: finish or abandon one (L)"), Info.Most);
	return FString();
}

void SAcCitadelScreen::Take(const int32 Index)
{
	if (const FString Why = TakeWhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed = Info.Offers[Index].Title;
	MercAsked.Reset();
	Upgraded.Reset();
	Started.Reset();
	OnTakeDelegate.ExecuteIfBound(Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::HireWhyNot() const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.MercsWhyNot.IsEmpty()) return Info.MercsWhyNot;
	if (Info.MercLimit <= 0) return TEXT("Mercenaries sign on with pilots of level 5 and up");
	if (Info.Mercs.Num() >= Info.MercLimit)
	{
		FString Why = FString::Printf(TEXT("You hold %d, the most at level %lld"), Info.Mercs.Num(), (long long)Info.Level);
		if (Info.MercNextLevel > 0) Why += FString::Printf(TEXT(": %lld at level %lld"), (long long)(Info.MercLimit + 1), (long long)Info.MercNextLevel);
		return Why;
	}
	if (Info.MercFeeOre > Info.Ore) return FString::Printf(TEXT("Not enough ore: %s, you have %s"), *Grouped(Info.MercFeeOre), *Grouped(Info.Ore));
	if (Info.MercFeeHydrogen > Info.Hydrogen)
		return FString::Printf(TEXT("Not enough MH: %s, you have %s"), *Grouped(Info.MercFeeHydrogen), *Grouped(Info.Hydrogen));
	return FString();
}

FString SAcCitadelScreen::SendWhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.Mercs.IsValidIndex(Index)) return TEXT("No mercenary there");
	if (!Info.MercFields.IsValidIndex(Picked)) return TEXT("No field to send it to: 1-8 picks one");
	const FAcMercLine& M = Info.Mercs[Index];
	const FAcMercField& F = Info.MercFields[Picked];
	if (M.Region == F.Region && M.Field == F.Field) return FString::Printf(TEXT("%s works %s already"), *M.Name, *F.Name);
	return FString();
}

void SAcCitadelScreen::Hire(const bool bGuard)
{
	if (const FString Why = HireWhyNot(); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	Upgraded.Reset();
	Started.Reset();
	MercAsked = bGuard
		? FString::Printf(TEXT("Hired a guard: it mines the field nearest the Citadel and answers THE HUNT. Paid %s."),
			  *Purse(Info.MercFeeOre, Info.MercFeeHydrogen))
		: FString::Printf(TEXT("Hired: on its way to the field nearest the Citadel. Paid %s."), *Purse(Info.MercFeeOre, Info.MercFeeHydrogen));
	OnMercDelegate.ExecuteIfBound(bGuard ? 3 : 0, INDEX_NONE, INDEX_NONE);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::Role(const int32 Index)
{
	if (!Info.Mercs.IsValidIndex(Index))
	{
		SetRefusal(TEXT("No mercenary there"));
		return;
	}
	const FAcMercLine& X = Info.Mercs[Index];
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	Upgraded.Reset();
	Started.Reset();
	MercAsked = X.bGuard ? FString::Printf(TEXT("%s is a miner again: it stays at its field when THE HUNT is called."), *X.Name)
						 : FString::Printf(TEXT("%s is a guard: it answers THE HUNT, then goes back to its field."), *X.Name);
	OnMercDelegate.ExecuteIfBound(4, Index, X.bGuard ? 0 : 1);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::Send(const int32 Index)
{
	if (const FString Why = SendWhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	Upgraded.Reset();
	Started.Reset();
	MercAsked = FString::Printf(TEXT("%s is off to %s."), *Info.Mercs[Index].Name, *Info.MercFields[Picked].Name);
	OnMercDelegate.ExecuteIfBound(2, Index, Picked);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::Dismiss(const int32 Index)
{
	if (!Info.Mercs.IsValidIndex(Index))
	{
		SetRefusal(TEXT("No mercenary there"));
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	Upgraded.Reset();
	Started.Reset();
	MercAsked = FString::Printf(TEXT("%s was let go. No refund."), *Info.Mercs[Index].Name);
	OnMercDelegate.ExecuteIfBound(1, Index, INDEX_NONE);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::PickField(const int32 Index)
{
	if (!Info.MercFields.IsValidIndex(Index)) return;
	Picked = Index;
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::UpgradeWhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.Defence.IsValidIndex(Index)) return TEXT("Nothing there");
	if (!Info.DefenceWhyNot.IsEmpty()) return Info.DefenceWhyNot;
	const FAcDefenceLine& X = Info.Defence[Index];
	if (X.Mark >= X.Most) return FString::Printf(TEXT("%s is at its last mark"), *X.Name.Left(1).Append(X.Name.Mid(1).ToLower()));
	if (X.Ore > Info.RealmOre)
		return FString::Printf(TEXT("The realm wallet is short: %s ore needed, it has %s"), *Grouped(X.Ore), *Grouped(Info.RealmOre));
	if (X.Hydrogen > Info.RealmHydrogen)
		return FString::Printf(TEXT("The realm wallet is short: %s MH needed, it has %s"), *Grouped(X.Hydrogen), *Grouped(Info.RealmHydrogen));
	return FString();
}

void SAcCitadelScreen::Upgrade(const int32 Index)
{
	if (const FString Why = UpgradeWhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	const FAcDefenceLine& X = Info.Defence[Index];
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	MercAsked.Reset();
	Upgraded = FString::Printf(TEXT("%s, mark %lld"), *X.Name.ToLower(), (long long)(X.Mark + 1));
	OnUpgradeDelegate.ExecuteIfBound(Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::ProjectWhyNot(const int32 Index) const
{
	if (!Info.bKnown) return TEXT("Waiting for the server…");
	if (!Info.Projects.IsValidIndex(Index)) return TEXT("Nothing there");
	const FAcProjectLine& X = Info.Projects[Index];
	if (X.bRunning) return FString::Printf(TEXT("The realm's %s runs already"), *X.Title.Left(1).Append(X.Title.Mid(1).ToLower()));
	if (!Info.ProjectsWhyNot.IsEmpty()) return Info.ProjectsWhyNot;
	if (X.Ore > Info.RealmOre)
		return FString::Printf(TEXT("The realm wallet is short: %s ore needed, it has %s"), *Grouped(X.Ore), *Grouped(Info.RealmOre));
	if (X.Hydrogen > Info.RealmHydrogen)
		return FString::Printf(TEXT("The realm wallet is short: %s MH needed, it has %s"), *Grouped(X.Hydrogen), *Grouped(Info.RealmHydrogen));
	return FString();
}

void SAcCitadelScreen::StartProject(const int32 Index)
{
	if (const FString Why = ProjectWhyNot(Index); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	const FAcProjectLine& X = Info.Projects[Index];
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	MercAsked.Reset();
	Upgraded.Reset();
	Started = X.Title.Left(1).Append(X.Title.Mid(1).ToLower());
	OnProjectDelegate.ExecuteIfBound(Index);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FString SAcCitadelScreen::HuntWhyNot() const
{
	if (!Info.bHuntKnown) return TEXT("Waiting for the server…");
	return Info.HuntWhyNot;
}

void SAcCitadelScreen::CallHunt()
{
	if (const FString Why = HuntWhyNot(); !Why.IsEmpty())
	{
		SetRefusal(Why);
		return;
	}
	Refusal.Reset();
	Kept.Reset();
	Asked = {0, 0};
	Bought.Reset();
	Signed.Reset();
	MercAsked.Reset();
	Upgraded.Reset();
	Started.Reset();
	HuntCalled = Info.HuntWhere.IsEmpty() ? FString(TEXT("her nest")) : Info.HuntWhere;
	OnHuntDelegate.ExecuteIfBound();
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SAcCitadelScreen::Close() { OnCloseDelegate.ExecuteIfBound(); }

bool SAcCitadelScreen::Key(const FKey& InKey)
{
	if (InKey == EKeys::Tab)
	{
		SetTab(ETab((int32(CurrentTab) + 1) % TabCount));
		return true;
	}
	static const FKey Digits[] = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight,
		EKeys::Nine};
	static const FKey Numpad[] = {EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine};
	// REALM: 1-6 give, 7-0 start a project (R2); SHOP: 1-9 buy a
	// blueprint; MISSIONS: 1-6 sign up; DEFENCE: 1-3 upgrade; MERCS: 1-8
	// pick a field.
	if (CurrentTab == ETab::Realm)
	{
		static const FKey ProjectKeys[] = {EKeys::Seven, EKeys::Eight, EKeys::Nine, EKeys::Zero};
		static const FKey ProjectPad[] = {EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine, EKeys::NumPadZero};
		for (int32 I = 0; I < FMath::Min(Info.Projects.Num(), 4); ++I)
			if (InKey == ProjectKeys[I] || InKey == ProjectPad[I])
			{
				StartProject(I);
				return true;
			}
	}
	const int32 Count = CurrentTab == ETab::Realm ? DonateButtons
		: CurrentTab == ETab::Shop ? Info.Blueprints.Num()
		: CurrentTab == ETab::Mercs ? FMath::Min(Info.MercFields.Num(), FieldRows)
		: CurrentTab == ETab::Defence ? Info.Defence.Num()
		: Info.Offers.Num();
	for (int32 I = 0; I < FMath::Min(Count, 9); ++I)
	{
		if (InKey == Digits[I] || InKey == Numpad[I])
		{
			if (CurrentTab == ETab::Realm) Donate(I);
			else if (CurrentTab == ETab::Shop) Buy(false, I);
			else if (CurrentTab == ETab::Mercs) PickField(I);
			else if (CurrentTab == ETab::Defence) Upgrade(I);
			else Take(I);
			return true;
		}
	}
	return false;
}

// MARK: - Layout

void SAcCitadelScreen::Layout(const FVector2f ViewSize) const
{
	Targets.Reset();
	auto Add = [&](const FBox2f& B, int32 RowId, int32 Index) {
		FTarget T;
		T.Box = B;
		T.Row = RowId;
		T.Index = Index;
		Targets.Add(T);
	};
	// The header: the tabs, each as wide as its name, the close on the right.
	float TabX = 450;
	for (int32 T = 0; T < TabCount; ++T)
	{
		const float W = Label(TabNames[T], 18, FLinearColor::White, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.4f).Measure().X + 22;
		Add(FBox2f(FVector2f(TabX, 30), FVector2f(TabX + W, 66)), RowTab, T);
		TabX += W + 6;
	}
	Add(FBox2f(FVector2f(PanelW - Pad - 40, 26), FVector2f(PanelW - Pad, 58)), RowClose, 0);

	const float Top = HeaderH + 16;
	WalletBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + WalletW, Top + BodyH));
	DonateBox = FBox2f(FVector2f(Pad + WalletW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	ProjectsBox = FBox2f(FVector2f(Pad, Top + BodyH + Gap), FVector2f(PanelW - Pad, Top + BodyH + Gap + ProjectsH));
	StandingsBox = FBox2f(FVector2f(Pad, ProjectsBox.Max.Y + Gap), FVector2f(Pad + StandingsW, ProjectsBox.Max.Y + Gap + StandingsH));
	HuntBox = FBox2f(FVector2f(StandingsBox.Max.X + Gap, StandingsBox.Min.Y), FVector2f(PanelW - Pad, StandingsBox.Max.Y));
	BlueprintBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + BlueprintsW, Top + BodyH));
	ItemBox = FBox2f(FVector2f(Pad + BlueprintsW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	const float TierW = (PanelW - 2 * Pad - 2 * Gap) / 3;
	for (int32 T = 0; T < 3; ++T)
	{
		const float X = Pad + T * (TierW + Gap);
		TierBox[T] = FBox2f(FVector2f(X, Top), FVector2f(X + TierW, Top + BodyH));
	}
	MercBox = FBox2f(FVector2f(Pad, Top), FVector2f(Pad + MercsW, Top + BodyH));
	FieldBox = FBox2f(FVector2f(Pad + MercsW + Gap, Top), FVector2f(PanelW - Pad, Top + BodyH));
	if (CurrentTab == ETab::Mercs)
	{
		// Each mercenary: SEND and DISMISS; HIRE at the foot; the fields.
		for (int32 I = 0; I < FMath::Min(Info.Mercs.Num(), 3); ++I)
		{
			const float Y = MercBox.Min.Y + 44 + I * MercRowH + MercRowH - 10 - BuyH;
			Add(FBox2f(FVector2f(MercBox.Max.X - 14 - 2 * BuyW - 26, Y), FVector2f(MercBox.Max.X - 14 - BuyW - 26, Y + BuyH)), RowSend, I);
			Add(FBox2f(FVector2f(MercBox.Max.X - 14 - BuyW - 16, Y), FVector2f(MercBox.Max.X - 14, Y + BuyH)), RowDismiss, I);
			// N4: its role, GUARD or MINER, left of SEND.
			Add(FBox2f(FVector2f(MercBox.Max.X - 14 - 3 * BuyW - 36, Y), FVector2f(MercBox.Max.X - 14 - 2 * BuyW - 36, Y + BuyH)), RowRole, I);
		}
		Add(FBox2f(FVector2f(MercBox.Min.X + 18, MercBox.Max.Y - 44), FVector2f(MercBox.Min.X + 18 + 96, MercBox.Max.Y - 14)), RowHire, 0);
		Add(FBox2f(FVector2f(MercBox.Min.X + 18 + 96 + 10, MercBox.Max.Y - 44), FVector2f(MercBox.Min.X + 18 + 96 + 10 + 150, MercBox.Max.Y - 14)),
			RowHire, 1);
		for (int32 I = 0; I < FMath::Min(Info.MercFields.Num(), FieldRows); ++I)
		{
			const float Y = FieldBox.Min.Y + 40 + I * FieldRowH;
			Add(FBox2f(FVector2f(FieldBox.Min.X + 10, Y), FVector2f(FieldBox.Max.X - 10, Y + FieldRowH)), RowField, I);
		}
	}
	else if (CurrentTab == ETab::Defence)
	{
		// An UPGRADE at the foot of each stat's card.
		for (int32 I = 0; I < FMath::Min(Info.Defence.Num(), 3); ++I)
		{
			const FBox2f& B = TierBox[I];
			Add(FBox2f(FVector2f(B.Max.X - 18 - 130, B.Max.Y - 18 - 40), FVector2f(B.Max.X - 18, B.Max.Y - 18)), RowUpgrade, I);
		}
	}
	else if (CurrentTab == ETab::Missions)
	{
		// A TAKE on each offer's card: two cards a tier.
		int32 Seen[3] = {0, 0, 0};
		for (int32 I = 0; I < Info.Offers.Num(); ++I)
		{
			const int32 T = FMath::Clamp(Info.Offers[I].Tier, 0, 2);
			const int32 N = Seen[T]++;
			if (N > 1) continue;
			const float CardTop = TierBox[T].Min.Y + 40 + N * 128;
			const float Y = CardTop + 120 - 8 - BuyH;
			Add(FBox2f(FVector2f(TierBox[T].Max.X - 12 - BuyW, Y), FVector2f(TierBox[T].Max.X - 12, Y + BuyH)), RowTake, I);
		}
	}
	else if (CurrentTab == ETab::Realm)
	{
		// The donate buttons: a row of ore, a row of MH.
		for (int32 I = 0; I < DonateButtons; ++I)
		{
			const float X = DonateBox.Min.X + 96 + (I % 3) * (ButtonW + 12);
			const float Y = DonateBox.Min.Y + 112 + (I / 3) * (ButtonH + 22);
			Add(FBox2f(FVector2f(X, Y), FVector2f(X + ButtonW, Y + ButtonH)), RowDonate, I);
		}
		// R2: a START at the foot of each project's card.
		const float CardW = (PanelW - 2 * Pad - 3 * Gap) / 4;
		for (int32 I = 0; I < FMath::Min(Info.Projects.Num(), 4); ++I)
		{
			const float X = Pad + I * (CardW + Gap);
			Add(FBox2f(FVector2f(X + CardW - 14 - 96, ProjectsBox.Max.Y - 14 - 32), FVector2f(X + CardW - 14, ProjectsBox.Max.Y - 14)),
				RowProject, I);
		}
		// N4: CALL THE HUNT at the hunt's foot.
		Add(FBox2f(FVector2f(HuntBox.Min.X + 18, HuntBox.Max.Y - 18 - 40), FVector2f(HuntBox.Max.X - 18, HuntBox.Max.Y - 18)), RowHunt, 0);
	}
	else
	{
		// A BUY on each blueprint's row and each consumable's.
		for (int32 I = 0; I < Info.Blueprints.Num(); ++I)
		{
			const float Y = BlueprintBox.Min.Y + 46 + I * BlueprintRowH + (BlueprintRowH - BuyH) / 2;
			Add(FBox2f(FVector2f(BlueprintBox.Max.X - 18 - BuyW, Y), FVector2f(BlueprintBox.Max.X - 18, Y + BuyH)), RowBuyBlueprint, I);
		}
		for (int32 I = 0; I < Info.Items.Num(); ++I)
		{
			const float Y = ItemBox.Min.Y + 44 + I * ItemRowH + (ItemRowH - BuyH) / 2;
			Add(FBox2f(FVector2f(ItemBox.Max.X - 18 - BuyW, Y), FVector2f(ItemBox.Max.X - 18, Y + BuyH)), RowBuyItem, I);
		}
	}
	// R2: the REALM tab's projects under its body.
	const float BodyEnd = CurrentTab == ETab::Realm ? StandingsBox.Max.Y : WalletBox.Max.Y;
	FootBox = FBox2f(FVector2f(Pad, BodyEnd + 16), FVector2f(PanelW - Pad, BodyEnd + 16 + FootH));
	PanelSize = FVector2f(PanelW, FootBox.Max.Y + 18);
	PanelScale = FMath::Clamp(FMath::Min((ViewSize.X - 32) / PanelSize.X, (ViewSize.Y - 32) / PanelSize.Y), 0.3f, 1.0f);
	PanelOrigin = ((ViewSize - PanelSize * PanelScale) * 0.5f).RoundToVector();
}

int32 SAcCitadelScreen::TargetAt(const FVector2f Local) const
{
	const FVector2f P = (Local - PanelOrigin) / PanelScale;
	for (int32 I = 0; I < Targets.Num(); ++I)
	{
		if (Targets[I].Box.IsInsideOrOn(P)) return I;
	}
	return INDEX_NONE;
}

// MARK: - Input

void SAcCitadelScreen::Press(const FTarget& T)
{
	switch (T.Row)
	{
	case RowDonate: Donate(T.Index); break;
	case RowTab: SetTab(ETab(T.Index)); break;
	case RowBuyBlueprint: Buy(false, T.Index); break;
	case RowBuyItem: Buy(true, T.Index); break;
	case RowTake: Take(T.Index); break;
	case RowHire: Hire(T.Index == 1); break;
	case RowRole: Role(T.Index); break;
	case RowHunt: CallHunt(); break;
	case RowSend: Send(T.Index); break;
	case RowDismiss: Dismiss(T.Index); break;
	case RowField: PickField(T.Index); break;
	case RowUpgrade: Upgrade(T.Index); break;
	case RowProject: StartProject(T.Index); break;
	case RowClose: Close(); break;
	default: break;
	}
}

FReply SAcCitadelScreen::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton) Pressed = TargetAt(Local);
	return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SAcCitadelScreen::OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return OnMouseButtonDown(Geometry, Event);
}

FReply SAcCitadelScreen::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
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

FReply SAcCitadelScreen::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	const FVector2f Local(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()));
	Hovered = TargetAt(Local);
	Invalidate(EInvalidateWidgetReason::Paint);
	return FReply::Handled();
}

void SAcCitadelScreen::OnMouseLeave(const FPointerEvent& Event)
{
	SLeafWidget::OnMouseLeave(Event);
	Hovered = INDEX_NONE;
	Pressed = INDEX_NONE;
}

FCursorReply SAcCitadelScreen::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
	return FCursorReply::Cursor(Hovered != INDEX_NONE ? EMouseCursor::Hand : EMouseCursor::Default);
}

// MARK: - Paint

int32 SAcCitadelScreen::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const FVector2f View(Geometry.GetLocalSize());
	Layout(View);
	// The world goes on behind, darkened.
	const float In = FMath::Clamp(float(Clock() - OpenedAt) / 0.18f, 0.f, 1.f);
	Box(Out, Layer, Geometry, FBox2f(FVector2f::ZeroVector, View), FLinearColor(0, 0, 0, 0.62f * In));
	const FGeometry P = Geometry.MakeChild(FVector2D(PanelSize), FSlateLayoutTransform(PanelScale, FVector2D(PanelOrigin)));
	const FBox2f Panel(FVector2f::ZeroVector, PanelSize);
	// The panel: dark glass, a steel rim with cut corners, the realm's gold inside it.
	Box(Out, Layer + 1, P, Panel, FAcHudStyle::Srgb(0.02, 0.036, 0.062, 0.97));
	for (int32 I = 0; I < 6; ++I)
	{
		Box(Out, Layer + 1, P, FBox2f(FVector2f(0, I * 14.f), FVector2f(PanelSize.X, (I + 1) * 14.f)),
			WithAlpha(RealmGold(), 0.03f * (1 - I / 6.f)));
	}
	CutFrame(Out, Layer + 2, P, Panel, FAcHudStyle::Srgb(0.42, 0.5, 0.58, 0.9), 2, 18);
	CutFrame(Out, Layer + 2, P, Panel.ExpandBy(-5), WithAlpha(RealmGold(), 0.28f), 1, 14);
	const int32 L = Layer + 3;
	PaintHeader(Out, L, P);
	switch (CurrentTab)
	{
	case ETab::Realm:
		PaintRealm(Out, L, P);
		PaintStandings(Out, L, P);
		PaintHunt(Out, L, P);
		break;
	case ETab::Shop: PaintShop(Out, L, P); break;
	case ETab::Missions: PaintMissions(Out, L, P); break;
	case ETab::Defence: PaintDefence(Out, L, P); break;
	case ETab::Mercs: PaintMercs(Out, L, P); break;
	}
	PaintFoot(Out, L, P);
	return L + 24;
}

void SAcCitadelScreen::PaintButton(FSlateWindowElementList& Out, const int32 L, const FGeometry& P, const FTarget& T, const FString& Text,
	const bool bLit, const FLinearColor& Accent) const
{
	const int32 Index = Targets.IndexOfByPredicate([&](const FTarget& X) { return X.Row == T.Row && X.Index == T.Index; });
	const bool bHover = Index == Hovered;
	const bool bDown = Index == Pressed && bHover;
	FLinearColor Fill = FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1);
	FLinearColor Rim = Steel();
	FLinearColor Ink = Dim();
	if (bLit)
	{
		Fill = FAcHudStyle::Blend(FAcHudStyle::Srgb(0.04, 0.075, 0.12, 1), 0.24, Accent);
		Rim = Accent;
		Ink = FAcHudStyle::Text();
	}
	if (bHover) Rim = bLit ? FAcHudStyle::Blend(Accent, 0.5, FLinearColor::White) : FAcHudStyle::Ice();
	if (bDown) Fill = FAcHudStyle::Blend(Fill, 0.25, Accent);
	Box(Out, L + 1, P, T.Box, Fill);
	CutFrame(Out, L + 2, P, T.Box, Rim, bLit || bHover ? 1.5f : 1.0f, 6);
	Label(Text, 16, Ink, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 0.6f).Paint(Out, L + 3, P, T.Box.GetCenter());
}

void SAcCitadelScreen::PaintHeader(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	Label(TEXT("THE REALM'S"), 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 2.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, 24));
	FAcHudText Title = Label(TEXT("CITADEL"), 34, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.5f);
	Title.ShadowOffset = FVector2f(0, 2);
	Title.ShadowColor = FLinearColor(0, 0, 0, 0.6f);
	Title.Paint(Out, L, P, FVector2f(Pad + 2, 44));
	if (!Info.Builder.IsEmpty())
	{
		Label(FString::Printf(TEXT("built by %s · every pilot's"), *Info.Builder), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top,
			EAcFontWeight::SemiBold, 0.4f).Paint(Out, L, P, FVector2f(Pad + 186, 58));
	}
	// The tabs: REALM and SHOP, the open one lit (D1's upgrades join them).
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowTab) continue;
		const bool bOn = T.Index == int32(CurrentTab);
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == &T;
		const FLinearColor C = bOn ? FAcHudStyle::Text() : bHover ? FAcHudStyle::Ice() : Dim();
		Label(TabNames[T.Index], 18, C, EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold, 1.4f).Paint(Out, L + 1, P, T.Box.GetCenter());
		if (bOn)
		{
			const FBox2f U(FVector2f(T.Box.Min.X + 8, T.Box.Max.Y + 2), FVector2f(T.Box.Max.X - 8, T.Box.Max.Y + 5));
			Box(Out, L + 1, P, U, RealmGold());
		}
	}
	for (const FTarget& T : Targets)
	{
		if (T.Row == RowClose) PaintButton(Out, L, P, T, TEXT("ESC"), false, RealmGold());
	}
	Box(Out, L, P, FBox2f(FVector2f(Pad, HeaderH), FVector2f(PanelW - Pad, HeaderH + 1)), WithAlpha(RealmGold(), 0.2f));
}

void SAcCitadelScreen::PaintRealm(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	auto Plate = [&](const FBox2f& B, const TCHAR* Title) {
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Label(Title, 13, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	};

	// The realm wallet: its ore and MH, large; the tax; what this pilot gave.
	const FBox2f W = WalletBox;
	Plate(W, TEXT("REALM WALLET"));
	const float Grow = FMath::Clamp(1.f - float(Clock() - GrewAt) / 1.4f, 0.f, 1.f);
	if (Grow > 0) Box(Out, L + 1, P, W.ExpandBy(-2), WithAlpha(RealmGold(), 0.12f * Grow));
	auto Stock = [&](float Y, const TCHAR* What, int64 N, const FLinearColor& C) {
		Box(Out, L + 1, P, FBox2f(FVector2f(W.Min.X + 18, Y + 4), FVector2f(W.Min.X + 22, Y + 44)), C);
		Label(What, 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(W.Min.X + 32, Y));
		Label(Info.bKnown ? Grouped(N) : FString(TEXT("—")), 30, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.4f)
			.Paint(Out, L + 2, P, FVector2f(W.Min.X + 32, Y + 13));
	};
	Stock(W.Min.Y + 44, TEXT("STARDUST ORE"), Info.RealmOre, FAcHudStyle::Amber());
	Stock(W.Min.Y + 106, TEXT("METALLIC HYDROGEN"), Info.RealmHydrogen, FAcHudStyle::Cyan());
	Box(Out, L + 1, P, FBox2f(FVector2f(W.Min.X + 18, W.Min.Y + 170), FVector2f(W.Max.X - 18, W.Min.Y + 171)), WithAlpha(Steel(), 0.5f));
	Label(FString::Printf(TEXT("TAX %lld%%"), (long long)Info.TaxPercent), 15, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 182));
	Label(TEXT("of every unload, here and at the Hab Domes"), 14, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
		.Paint(Out, L + 2, P, FVector2f(W.Min.X + 98, W.Min.Y + 183));
	Label(TEXT("YOU GAVE"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 218));
	Label(FString::Printf(TEXT("in tax  %s ore · %s MH"), *Grouped(Info.TaxedOre), *Grouped(Info.TaxedHydrogen)), 15, FAcHudStyle::Text(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 236));
	Label(FString::Printf(TEXT("donated  %s ore · %s MH"), *Grouped(Info.DonatedOre), *Grouped(Info.DonatedHydrogen)), 15, FAcHudStyle::Text(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(W.Min.X + 18, W.Min.Y + 258));

	// Donate: from the pilot's own wallet, 100, 1,000 or all.
	const FBox2f D = DonateBox;
	Plate(D, TEXT("DONATE"));
	Label(FString::Printf(TEXT("YOUR WALLET   ORE %s  ·  MH %s"), *Grouped(Info.Ore), *Grouped(Info.Hydrogen)), 14, FAcHudStyle::Soft(),
		EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 2, P, D.Min + FVector2f(18, 44));
	Label(TEXT("Gifts to the realm wallet: the Citadel's upgrades are bought from it."), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top,
		EAcFontWeight::Medium).Paint(Out, L + 2, P, D.Min + FVector2f(18, 70));
	Label(TEXT("ORE"), 15, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L + 2, P, FVector2f(D.Min.X + 22, D.Min.Y + 112 + ButtonH / 2));
	Label(TEXT("MH"), 15, FAcHudStyle::Cyan(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L + 2, P, FVector2f(D.Min.X + 22, D.Min.Y + 112 + ButtonH + 22 + ButtonH / 2));
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowDonate) continue;
		const int32 Step = T.Index % 3;
		const FString Text = Step == 0 ? FString(TEXT("100")) : Step == 1 ? FString(TEXT("1,000")) : FString(TEXT("ALL"));
		PaintButton(Out, L, P, T, Text, WhyNot(T.Index).IsEmpty(), T.Index < 3 ? FAcHudStyle::Amber() : FAcHudStyle::Cyan());
		Label(FString::Printf(TEXT("%d"), T.Index + 1), 10, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 4, P, FVector2f(T.Box.Max.X - 6, T.Box.Min.Y + 3));
	}
	if (!Info.DonateWhyNot.IsEmpty())
	{
		Label(Info.DonateWhyNot, 14, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(D.Min.X + 18, D.Max.Y - 34));
	}

	// R2: the realm's projects, a card each: what it does, its price from
	// the realm wallet, or the one running (who started it, time left).
	const float CardW = (PanelW - 2 * Pad - 3 * Gap) / 4;
	Label(TEXT("REALM PROJECTS  ·  a world day each, paid from the realm wallet"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Baseline,
		EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(Pad + 2, ProjectsBox.Min.Y + 2));
	static const TCHAR* const Keys[] = {TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0")};
	auto Find = [&](int32 Row, int32 Index) -> const FTarget* {
		for (const FTarget& T : Targets)
			if (T.Row == Row && T.Index == Index) return &T;
		return nullptr;
	};
	for (int32 I = 0; I < FMath::Min(Info.Projects.Num(), 4); ++I)
	{
		const FAcProjectLine& X = Info.Projects[I];
		const float Left = Pad + I * (CardW + Gap);
		const FBox2f B(FVector2f(Left, ProjectsBox.Min.Y + 8), FVector2f(Left + CardW, ProjectsBox.Max.Y));
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		if (X.bRunning) Box(Out, L + 1, P, B.ExpandBy(-2), WithAlpha(RealmGold(), 0.08f));
		CutFrame(Out, L + 1, P, B, X.bRunning ? WithAlpha(RealmGold(), 0.7f) : Steel(), 1, 10);
		Label(X.Title, 15, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.4f).Paint(Out, L + 2, P, B.Min + FVector2f(14, 12));
		Label(X.Effect, 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, B.Min + FVector2f(14, 38));
		if (X.bRunning)
		{
			const int32 Minutes = FMath::Max(0, int32(FMath::CeilToDouble(X.Left / 60.0)));
			Label(FString::Printf(TEXT("RUNNING  ·  %d min left"), Minutes), 13, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top,
				EAcFontWeight::Bold, 0.6f).Paint(Out, L + 2, P, B.Min + FVector2f(14, 70));
			Label(FString::Printf(TEXT("started by %s"), *X.By), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, B.Min + FVector2f(14, 92));
			// Time left, as a bar.
			const float Share = X.Day > 0 ? FMath::Clamp(float(X.Left / X.Day), 0.f, 1.f) : 0.f;
			const FBox2f Bar(FVector2f(B.Min.X + 14, B.Min.Y + 116), FVector2f(B.Max.X - 14, B.Min.Y + 122));
			Box(Out, L + 1, P, Bar, WithAlpha(Steel(), 0.5f));
			Box(Out, L + 2, P, FBox2f(Bar.Min, FVector2f(Bar.Min.X + (Bar.Max.X - Bar.Min.X) * Share, Bar.Max.Y)), RealmGold());
		}
		else
		{
			Label(FString::Printf(TEXT("%s ore  ·  %s MH"), *Grouped(X.Ore), *Grouped(X.Hydrogen)), 14, FAcHudStyle::Text(), EAcHAlign::Left,
				EAcVAlign::Top, EAcFontWeight::Bold, 0.4f).Paint(Out, L + 2, P, B.Min + FVector2f(14, 70));
			Label(TEXT("a fifth more each time"), 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L + 2, P, B.Min + FVector2f(14, 92));
		}
		if (const FTarget* T = Find(RowProject, I))
		{
			PaintButton(Out, L, P, *T, X.bRunning ? TEXT("RUNNING") : TEXT("START"), ProjectWhyNot(I).IsEmpty(), RealmGold());
			Label(Keys[I], 10, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold)
				.Paint(Out, L + 4, P, FVector2f(T->Box.Max.X - 6, T->Box.Min.Y + 3));
		}
	}
}

void SAcCitadelScreen::PaintStandings(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f B = StandingsBox;
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
	CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
	Label(TEXT("STANDINGS  ·  THIS WEEK"), 13, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f)
		.Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	Label(Info.bStandingsKnown ? Info.WeekLeft : FString(TEXT("Waiting for the server…")), 13,
		FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f).Paint(Out, L + 2, P, FVector2f(B.Max.X - 18, B.Min.Y + 15));
	// The columns: rank, pilot, then the four counts right-aligned.
	const float XRank = B.Min.X + 30, XName = B.Min.X + 64;
	const float XCols[] = {B.Min.X + 352, B.Min.X + 432, B.Min.X + 540, B.Max.X - 24};
	static const TCHAR* const Heads[] = {TEXT("ORE"), TEXT("BUGS"), TEXT("WAVES HELD"), TEXT("BUILT")};
	const float HeadY = B.Min.Y + 46;
	Label(TEXT("#"), 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(XRank, HeadY));
	Label(TEXT("PILOT"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, FVector2f(XName, HeadY));
	for (int32 C = 0; C < 4; ++C)
	{
		Label(Heads[C], 11, C == 0 ? FAcHudStyle::Amber() : Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f)
			.Paint(Out, L + 2, P, FVector2f(XCols[C], HeadY));
	}
	if (Info.bStandingsKnown && Info.Standings.Num() == 0)
	{
		Label(TEXT("Nothing counted yet this week."), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, FVector2f(XName, B.Min.Y + 74));
	}
	float Y = B.Min.Y + 68;
	for (int32 I = 0; I < Info.Standings.Num(); ++I)
	{
		const FAcStandingLine& X = Info.Standings[I];
		// The pilot's own row past the top: after a gap.
		if (I == FAcCitadelScreenInfo::StandingsTop)
		{
			Label(TEXT("···"), 14, Faint(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(XName, Y + 4));
			Y += 12;
		}
		const FBox2f Row(FVector2f(B.Min.X + 10, Y), FVector2f(B.Max.X - 10, Y + StandingRowH));
		if (X.bYou) Box(Out, L + 1, P, Row, WithAlpha(RealmGold(), 0.14f));
		else if (I % 2 == 0) Box(Out, L + 1, P, Row, WithAlpha(Steel(), 0.08f));
		const float Mid = Y + StandingRowH / 2;
		const FLinearColor Ink = X.bYou ? RealmGold() : FAcHudStyle::Text();
		Label(FString::Printf(TEXT("%lld"), (long long)X.Rank), 14, X.Rank <= 3 ? RealmGold() : Dim(), EAcHAlign::Right, EAcVAlign::Center,
			EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(XRank, Mid));
		// As the map draws them: a frontier pilot's dot in its paint, a
		// human's pale.
		const FLinearColor Dot = X.bFrontier ? FAcHudStyle::TeamPaint(X.Paint) : FAcHudStyle::Srgb(0.86, 0.9, 1);
		Box(Out, L + 2, P, FBox2f(FVector2f(XName, Mid - 5), FVector2f(XName + 10, Mid + 5)), FLinearColor(0, 0, 0, 0.85f));
		Box(Out, L + 3, P, FBox2f(FVector2f(XName + 1.5f, Mid - 3.5f), FVector2f(XName + 8.5f, Mid + 3.5f)), Dot);
		FAcHudText Name = Label(X.Name, 15, Ink, EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold);
		Name.Paint(Out, L + 2, P, FVector2f(XName + 18, Mid));
		if (X.bFrontier)
		{
			Label(TEXT("FRONTIER"), 10, X.Paint >= 0 ? WithAlpha(Dot, 0.85f) : Dim(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold, 1.2f)
				.Paint(Out, L + 2, P, FVector2f(XName + 18 + Name.Measure().X + 10, Mid + 1));
		}
		const int64 Counts[] = {X.Ore, X.Bugs, X.Waves, X.Built};
		for (int32 C = 0; C < 4; ++C)
		{
			Label(Grouped(Counts[C]), C == 0 ? 15 : 14, Counts[C] > 0 ? (C == 0 ? Ink : FAcHudStyle::Text()) : Faint(), EAcHAlign::Right,
				EAcVAlign::Center, C == 0 ? EAcFontWeight::Bold : EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(XCols[C], Mid));
		}
		Y += StandingRowH;
	}
}

void SAcCitadelScreen::PaintHunt(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f B = HuntBox;
	const FLinearColor Blood = FAcHudStyle::Srgb(1.0, 0.42, 0.32);
	Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
	if (Info.bHuntOn) Box(Out, L + 1, P, B.ExpandBy(-2), WithAlpha(Blood, 0.07f));
	CutFrame(Out, L + 1, P, B, Info.bHuntOn ? WithAlpha(Blood, 0.75f) : Steel(), 1, 10);
	Label(TEXT("THE HUNT"), 13, Blood, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	Label(Info.HuntWhere.IsEmpty() ? FString(TEXT("the Matriarch")) : FString::Printf(TEXT("the Matriarch  ·  %s"), *Info.HuntWhere), 12, Dim(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f).Paint(Out, L + 2, P, FVector2f(B.Max.X - 18, B.Min.Y + 15));
	if (!Info.bHuntKnown)
	{
		Label(TEXT("Waiting for the server…"), 14, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, B.Min + FVector2f(18, 50));
		return;
	}
	Label(Info.HuntState, 17, Info.bHuntOn ? Blood : FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, B.Min + FVector2f(18, 44));
	// The line under it, in two when long.
	FString First = Info.HuntLine, Second;
	if (int32 Colon = INDEX_NONE; Info.HuntLine.Len() > 42 && Info.HuntLine.FindChar(TEXT(':'), Colon))
	{
		First = Info.HuntLine.Left(Colon + 1);
		Second = Info.HuntLine.Mid(Colon + 1).TrimStart();
	}
	Label(First, 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, B.Min + FVector2f(18, 70));
	if (!Second.IsEmpty())
		Label(Second, 13, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, B.Min + FVector2f(18, 88));
	// The party: the caller, the defenders, the guards; at the ring or on
	// the way, or down.
	float Y = B.Min.Y + 114;
	if (Info.HuntParty.Num() == 0)
	{
		Label(Info.bHuntOn ? TEXT("No defenders or guards answered.") : TEXT("Frontier defenders and your guards answer the call."), 12, Dim(),
			EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(B.Min.X + 18, Y));
	}
	for (int32 I = 0; I < FMath::Min(Info.HuntParty.Num(), HuntRows); ++I)
	{
		const FAcHuntPartyLine& X = Info.HuntParty[I];
		const float Mid = Y + HuntRowH / 2;
		const FLinearColor Dot = X.Paint >= 0 ? FAcHudStyle::TeamPaint(X.Paint) : X.Tag.StartsWith(TEXT("GUARD")) ? MercOrange() : FAcHudStyle::Srgb(0.86, 0.9, 1);
		Box(Out, L + 2, P, FBox2f(FVector2f(B.Min.X + 18, Mid - 5), FVector2f(B.Min.X + 28, Mid + 5)), FLinearColor(0, 0, 0, 0.85f));
		Box(Out, L + 3, P, FBox2f(FVector2f(B.Min.X + 19.5f, Mid - 3.5f), FVector2f(B.Min.X + 26.5f, Mid + 3.5f)), Dot);
		FAcHudText Name = Label(X.Name, 14, X.bDown ? Faint() : FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold);
		Name.Paint(Out, L + 2, P, FVector2f(B.Min.X + 36, Mid));
		Label(X.Tag, 10, Dim(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold, 1.0f)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 36 + Name.Measure().X + 8, Mid + 1));
		const FString Where = X.bDown ? FString(TEXT("DOWN")) : X.bThere ? FString(TEXT("AT THE RING")) : FString(TEXT("ON THE WAY"));
		Label(Where, 10, X.bDown ? Refused() : X.bThere ? Ready() : FAcHudStyle::Amber(), EAcHAlign::Right, EAcVAlign::Center, EAcFontWeight::Bold, 1.0f)
			.Paint(Out, L + 2, P, FVector2f(B.Max.X - 18, Mid + 1));
		Y += HuntRowH;
	}
	if (Info.HuntParty.Num() > HuntRows)
	{
		Label(FString::Printf(TEXT("and %d more"), Info.HuntParty.Num() - HuntRows), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 36, Y + 2));
	}
	for (const FTarget& T : Targets)
	{
		if (T.Row != RowHunt) continue;
		const FString Why = HuntWhyNot();
		PaintButton(Out, L, P, T, Info.bHuntOn ? TEXT("THE HUNT IS ON") : TEXT("CALL THE HUNT"), Why.IsEmpty(), Blood);
		if (!Why.IsEmpty() && !Info.bHuntOn)
		{
			Label(Why, 11, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Baseline, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, FVector2f(T.Box.Min.X, T.Box.Min.Y - 6));
		}
	}
}

void SAcCitadelScreen::PaintShop(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	auto Plate = [&](const FBox2f& B, const TCHAR* Title) {
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Label(Title, 13, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	};
	auto Price = [](const FAcShopLine& X) {
		return X.Hydrogen > 0 ? FString::Printf(TEXT("%s ore · %s MH"), *Grouped(X.Ore), *Grouped(X.Hydrogen))
							  : FString::Printf(TEXT("%s ore"), *Grouped(X.Ore));
	};
	auto Find = [&](int32 Row, int32 Index) -> const FTarget* {
		return Targets.FindByPredicate([&](const FTarget& T) { return T.Row == Row && T.Index == Index; });
	};

	// Blueprints: every building's, the copies held, the floor, the next copy's price.
	const FBox2f B = BlueprintBox;
	Plate(B, TEXT("BLUEPRINTS"));
	Label(FString::Printf(TEXT("YOUR WALLET   ORE %s  ·  MH %s"), *Grouped(Info.Ore), *Grouped(Info.Hydrogen)), 12, FAcHudStyle::Soft(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 2, P, FVector2f(B.Max.X - 18, B.Min.Y + 15));
	for (int32 I = 0; I < Info.Blueprints.Num(); ++I)
	{
		const FAcShopLine& X = Info.Blueprints[I];
		const float Y = B.Min.Y + 46 + I * BlueprintRowH;
		const float Mid = Y + BlueprintRowH / 2;
		if (I % 2 == 0) Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 10, Y), FVector2f(B.Max.X - 10, Y + BlueprintRowH)), WithAlpha(Steel(), 0.08f));
		Label(FString::Printf(TEXT("%d"), I + 1), 10, Faint(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 18, Mid));
		Label(X.Name, 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 2, P, FVector2f(B.Min.X + 34, Mid));
		Label(FString::Printf(TEXT("×%lld"), (long long)X.Held), 16, X.Held > 0 ? Ready() : Faint(), EAcHAlign::Left, EAcVAlign::Center,
			EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(B.Min.X + 180, Mid));
		if (X.OpensAt > 0)
		{
			Label(FString::Printf(TEXT("opens at level %lld"), (long long)X.OpensAt), 13, FAcHudStyle::Amber(), EAcHAlign::Left, EAcVAlign::Center,
				EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(B.Min.X + 230, Mid));
		}
		Label(Price(X), 14, FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(B.Max.X - 30 - BuyW, Mid));
		if (const FTarget* T = Find(RowBuyBlueprint, I)) PaintButton(Out, L, P, *T, TEXT("BUY"), BuyWhyNot(false, I).IsEmpty(), RealmGold());
	}

	// Supplies: the four consumables, their stacks, their keys in the field.
	const FBox2f S = ItemBox;
	Plate(S, TEXT("SUPPLIES"));
	for (int32 I = 0; I < Info.Items.Num(); ++I)
	{
		const FAcShopLine& X = Info.Items[I];
		const float Y = S.Min.Y + 44 + I * ItemRowH;
		if (I > 0) Box(Out, L + 1, P, FBox2f(FVector2f(S.Min.X + 14, Y), FVector2f(S.Max.X - 14, Y + 1)), WithAlpha(Steel(), 0.35f));
		const FBox2f Chip(FVector2f(S.Min.X + 18, Y + 9), FVector2f(S.Min.X + 38, Y + 29));
		CutFrame(Out, L + 2, P, Chip, FAcHudStyle::Ice(), 1, 4);
		Label(X.Key, 12, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold).Paint(Out, L + 3, P, Chip.GetCenter());
		Label(X.Name, 16, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 9));
		Label(FString::Printf(TEXT("%lld/%lld"), (long long)X.Held, (long long)X.Stack), 14, X.Held > 0 ? Ready() : Faint(), EAcHAlign::Right,
			EAcVAlign::Top, EAcFontWeight::Bold).Paint(Out, L + 2, P, FVector2f(S.Max.X - 26 - BuyW, Y + 10));
		Label(X.Note, 12, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 30));
		Label(Price(X), 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(S.Min.X + 46, Y + 45));
		if (const FTarget* T = Find(RowBuyItem, I)) PaintButton(Out, L, P, *T, TEXT("BUY"), BuyWhyNot(true, I).IsEmpty(), FAcHudStyle::Cyan());
	}
}

void SAcCitadelScreen::PaintMissions(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	static const TCHAR* const Tiers[] = {TEXT("EASY"), TEXT("HARD"), TEXT("PERILOUS")};
	static const TCHAR* const Blurbs[] = {TEXT("close to home"), TEXT("past the borders"), TEXT("bug country")};
	for (int32 T = 0; T < 3; ++T)
	{
		const FBox2f B = TierBox[T];
		const FLinearColor C = TierColor(T);
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Box(Out, L + 1, P, FBox2f(B.Min + FVector2f(10, 10), FVector2f(B.Min.X + 14, B.Min.Y + 28)), C);
		Label(Tiers[T], 14, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.8f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 12));
		Label(Blurbs[T], 12, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.4f).Paint(Out, L + 2, P, FVector2f(B.Max.X - 14, B.Min.Y + 14));
	}
	Label(FString::Printf(TEXT("JOURNAL %d/%d"), Info.Taken, Info.Most), 12, Info.Taken >= Info.Most ? FAcHudStyle::Amber() : FAcHudStyle::Soft(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, FVector2f(PanelW - Pad, HeaderH + 2));
	int32 Seen[3] = {0, 0, 0};
	for (int32 I = 0; I < Info.Offers.Num(); ++I)
	{
		const FAcMissionLine& X = Info.Offers[I];
		const int32 T = FMath::Clamp(X.Tier, 0, 2);
		const int32 N = Seen[T]++;
		if (N > 1) continue;
		const FBox2f B = TierBox[T];
		const FBox2f Card(FVector2f(B.Min.X + 8, B.Min.Y + 40 + N * 128), FVector2f(B.Max.X - 8, B.Min.Y + 40 + N * 128 + 120));
		const FLinearColor C = TierColor(T);
		Box(Out, L + 1, P, Card, WithAlpha(C, 0.05f));
		CutFrame(Out, L + 2, P, Card, WithAlpha(C, 0.45f), 1, 6);
		const FBox2f Chip(Card.Min + FVector2f(8, 8), Card.Min + FVector2f(26, 26));
		CutFrame(Out, L + 3, P, Chip, FAcHudStyle::Ice(), 1, 4);
		Label(FString::Printf(TEXT("%d"), I + 1), 11, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 4, P, Chip.GetCenter());
		Label(X.Where.IsEmpty() ? X.Kind : X.Kind + TEXT(" · ") + X.Where, 11, Dim(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold, 1.0f)
			.Paint(Out, L + 3, P, FVector2f(Card.Min.X + 34, Chip.GetCenter().Y));
		Label(X.Title, 17, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold)
			.Paint(Out, L + 3, P, FVector2f(Card.Min.X + 10, Card.Min.Y + 32));
		const TArray<FString> Lines = Wrapped(X.Instructions, 12, Card.GetSize().X - 20, 2);
		for (int32 K = 0; K < Lines.Num(); ++K)
		{
			Label(Lines[K], 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L + 3, P, FVector2f(Card.Min.X + 10, Card.Min.Y + 56 + K * 15));
		}
		const TArray<FString> Pay = Wrapped(X.Reward, 11, Card.GetSize().X - 30 - BuyW, 2);
		for (int32 K = 0; K < Pay.Num(); ++K)
		{
			Label(Pay[K], 11, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
				.Paint(Out, L + 3, P, FVector2f(Card.Min.X + 10, Card.Max.Y - 36 + K * 14));
		}
		if (const FTarget* Tg = Targets.FindByPredicate([&](const FTarget& G) { return G.Row == RowTake && G.Index == I; }))
		{
			PaintButton(Out, L + 2, P, *Tg, TEXT("TAKE"), TakeWhyNot(I).IsEmpty(), C);
		}
	}
	if (Info.Offers.IsEmpty())
	{
		Label(Info.bKnown ? TEXT("The board is empty until dawn.") : TEXT("Waiting for the server…"), 15, Dim(), EAcHAlign::Center, EAcVAlign::Center,
			EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(PanelW / 2, TierBox[0].GetCenter().Y));
	}
}

void SAcCitadelScreen::PaintMercs(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	auto Plate = [&](const FBox2f& B, const FString& Title, const FLinearColor& C) {
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		Label(Title, 13, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.6f).Paint(Out, L + 2, P, B.Min + FVector2f(18, 14));
	};
	auto Find = [&](int32 Row, int32 Index) -> const FTarget* {
		return Targets.FindByPredicate([&](const FTarget& T) { return T.Row == Row && T.Index == Index; });
	};

	// The pilot's mercenaries: one row each, then the open and locked places.
	const FBox2f M = MercBox;
	Plate(M, FString::Printf(TEXT("YOUR MERCENARIES  %d/%lld"), Info.Mercs.Num(), (long long)Info.MercLimit), MercOrange());
	Label(FString::Printf(TEXT("YOUR WALLET   ORE %s  ·  MH %s"), *Grouped(Info.Ore), *Grouped(Info.Hydrogen)), 12, FAcHudStyle::Soft(),
		EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.6f).Paint(Out, L + 2, P, FVector2f(M.Max.X - 18, M.Min.Y + 15));
	for (int32 I = 0; I < 3; ++I)
	{
		const float Y = M.Min.Y + 44 + I * MercRowH;
		const FBox2f Row(FVector2f(M.Min.X + 10, Y), FVector2f(M.Max.X - 10, Y + MercRowH - 4));
		if (I < Info.Mercs.Num())
		{
			const FAcMercLine& X = Info.Mercs[I];
			const FLinearColor C = X.State == 0 ? Ready() : X.State == 1 ? MercOrange() : FAcHudStyle::Amber();
			Box(Out, L + 1, P, Row, WithAlpha(MercOrange(), 0.05f));
			Box(Out, L + 1, P, FBox2f(Row.Min, FVector2f(Row.Min.X + 4, Row.Max.Y)), MercOrange());
			FAcHudText Name = Label(FString::Printf(TEXT("MERC · %s"), *X.Name.ToUpper()), 16, MercOrange(), EAcHAlign::Left, EAcVAlign::Top,
				EAcFontWeight::Bold, 1.0f);
			Name.Paint(Out, L + 2, P, Row.Min + FVector2f(14, 7));
			// N4: a guard's tag (and on the hunt now).
			if (X.bGuard)
			{
				Label(X.bHunting ? TEXT("GUARD  ·  ON THE HUNT") : TEXT("GUARD"), 11, X.bHunting ? Refused() : RealmGold(), EAcHAlign::Left,
					EAcVAlign::Top, EAcFontWeight::Bold, 1.2f).Paint(Out, L + 2, P, Row.Min + FVector2f(14 + Name.Measure().X + 12, 11));
			}
			Label(X.Doing, 13, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, Row.Min + FVector2f(14, 31));
			Label(FString::Printf(TEXT("delivered %s ore"), *Grouped(X.Delivered)), 12, FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Top,
				EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(Row.Max.X - 8, Row.Min.Y + 9));
			if (const FTarget* T = Find(RowSend, I)) PaintButton(Out, L, P, *T, TEXT("SEND"), SendWhyNot(I).IsEmpty(), MercOrange());
			if (const FTarget* T = Find(RowDismiss, I)) PaintButton(Out, L, P, *T, TEXT("DISMISS"), true, Refused());
			if (const FTarget* T = Find(RowRole, I)) PaintButton(Out, L, P, *T, X.bGuard ? TEXT("GUARD") : TEXT("MINER"), true, RealmGold());
		}
		else
		{
			CutFrame(Out, L + 1, P, Row, WithAlpha(Steel(), 0.4f), 1, 6);
			const int64 Opens = I < Info.MercLimit ? 0 : (I == 0 ? 5 : I == 1 ? 15 : 30);
			const FString Text = Opens > 0 ? FString::Printf(TEXT("A place opens at level %lld"), (long long)Opens) : FString(TEXT("A place open: hire below"));
			Label(Text, 13, Opens > 0 ? Faint() : Dim(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, FVector2f(Row.Min.X + 14, Row.GetCenter().Y));
		}
	}
	if (const FTarget* G = Find(RowHire, 1)) PaintButton(Out, L, P, *G, TEXT("HIRE A GUARD"), HireWhyNot().IsEmpty(), RealmGold());
	if (const FTarget* H = Find(RowHire, 0))
	{
		PaintButton(Out, L, P, *H, TEXT("HIRE"), HireWhyNot().IsEmpty(), MercOrange());
		const FTarget* T = Find(RowHire, 1) ? Find(RowHire, 1) : H;
		Label(FString::Printf(TEXT("%s  ·  then %lld MH a day at dawn"), *Purse(Info.MercFeeOre, Info.MercFeeHydrogen), (long long)Info.MercWage), 14,
			FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(T->Box.Max.X + 14, T->Box.Min.Y - 1));
		Label(FString::Printf(TEXT("each mines %s ore an hour: your hour, over a day"), *Grouped(int64(FMath::RoundToDouble(Info.MercRate)))), 12, Dim(),
			EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
			.Paint(Out, L + 2, P, FVector2f(T->Box.Max.X + 14, T->Box.Min.Y + 17));
	}

	// The fields with ore by a realm Citadel, the picked one lit.
	const FBox2f F = FieldBox;
	Plate(F, TEXT("DEPOSIT FIELDS"), RealmGold());
	for (int32 I = 0; I < FMath::Min(Info.MercFields.Num(), FieldRows); ++I)
	{
		const FAcMercField& X = Info.MercFields[I];
		const FTarget* T = Find(RowField, I);
		if (!T) continue;
		const bool bOn = I == Picked;
		const bool bHover = Targets.IsValidIndex(Hovered) && &Targets[Hovered] == T;
		if (bOn) Box(Out, L + 1, P, T->Box, WithAlpha(MercOrange(), 0.14f));
		else if (bHover) Box(Out, L + 1, P, T->Box, WithAlpha(Steel(), 0.2f));
		else if (I % 2 == 0) Box(Out, L + 1, P, T->Box, WithAlpha(Steel(), 0.08f));
		if (bOn) Box(Out, L + 1, P, FBox2f(T->Box.Min, FVector2f(T->Box.Min.X + 3, T->Box.Max.Y)), MercOrange());
		const float Mid = T->Box.GetCenter().Y;
		Label(FString::Printf(TEXT("%d"), I + 1), 10, bOn ? MercOrange() : Faint(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::SemiBold)
			.Paint(Out, L + 2, P, FVector2f(T->Box.Min.X + 10, Mid));
		Label(X.Name, 14, bOn ? FAcHudStyle::Text() : FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 2, P, FVector2f(T->Box.Min.X + 26, Mid));
		Label(FString::Printf(TEXT("%lld · %s ore"), (long long)X.Deposits, *Grouped(X.Ore)), 12, Dim(), EAcHAlign::Right, EAcVAlign::Center,
			EAcFontWeight::SemiBold).Paint(Out, L + 2, P, FVector2f(T->Box.Max.X - 10, Mid));
	}
	if (Info.MercFields.IsEmpty())
	{
		Label(Info.bKnown ? TEXT("No field with ore by a realm Citadel.") : TEXT("Waiting for the server…"), 14, Dim(), EAcHAlign::Center,
			EAcVAlign::Center, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, F.GetCenter());
	}
}

void SAcCitadelScreen::PaintDefence(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	// The corner turrets: a card per stat, its marks as pips, now and next.
	static const FLinearColor Colors[] = {FAcHudStyle::Amber(), FAcHudStyle::Srgb(1.0, 0.45, 0.3), FAcHudStyle::Ice()};
	static const TCHAR* const Blurbs[] = {TEXT("rounds a second, each turret"), TEXT("a round"), TEXT("cells from each turret")};
	Label(TEXT("FOUR CORNER TURRETS · GROUND AND AIR"), 12, FAcHudStyle::Soft(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::Bold, 1.0f)
		.Paint(Out, L + 2, P, FVector2f(PanelW - Pad, HeaderH + 2));
	for (int32 I = 0; I < 3; ++I)
	{
		const FBox2f B = TierBox[I];
		const FLinearColor C = Colors[I];
		Box(Out, L, P, B, FAcHudStyle::Srgb(0.03, 0.055, 0.09, 0.9));
		CutFrame(Out, L + 1, P, B, Steel(), 1, 10);
		if (!Info.Defence.IsValidIndex(I))
		{
			Label(TEXT("Waiting for the server…"), 14, Dim(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::SemiBold)
				.Paint(Out, L + 2, P, B.GetCenter());
			continue;
		}
		const FAcDefenceLine& X = Info.Defence[I];
		Box(Out, L + 1, P, FBox2f(B.Min + FVector2f(10, 10), FVector2f(B.Min.X + 14, B.Min.Y + 28)), C);
		Label(X.Name, 15, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.8f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 12));
		const FBox2f Chip(FVector2f(B.Max.X - 34, B.Min.Y + 10), FVector2f(B.Max.X - 14, B.Min.Y + 30));
		CutFrame(Out, L + 2, P, Chip, FAcHudStyle::Ice(), 1, 4);
		Label(FString::Printf(TEXT("%d"), I + 1), 12, FAcHudStyle::Ice(), EAcHAlign::Center, EAcVAlign::Center, EAcFontWeight::Bold)
			.Paint(Out, L + 3, P, Chip.GetCenter());
		// The marks: a pip each, lit when bought.
		for (int32 K = 0; K < X.Most; ++K)
		{
			const FBox2f Pip(FVector2f(B.Min.X + 22 + K * 30, B.Min.Y + 42), FVector2f(B.Min.X + 46 + K * 30, B.Min.Y + 50));
			Box(Out, L + 2, P, Pip, K < X.Mark ? C : WithAlpha(Steel(), 0.35f));
		}
		Label(FString::Printf(TEXT("MARK %lld OF %lld"), (long long)X.Mark, (long long)X.Most), 11, Dim(), EAcHAlign::Left, EAcVAlign::Center,
			EAcFontWeight::SemiBold, 1.0f).Paint(Out, L + 2, P, FVector2f(B.Min.X + 22 + X.Most * 30 + 6, B.Min.Y + 46));
		Label(TEXT("NOW"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 70));
		Label(X.Now, 30, FAcHudStyle::Text(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 0.4f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 84));
		Label(Blurbs[I], 12, FAcHudStyle::Soft(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium).Paint(Out, L + 2, P, B.Min + FVector2f(22, 122));
		Box(Out, L + 1, P, FBox2f(FVector2f(B.Min.X + 18, B.Min.Y + 150), FVector2f(B.Max.X - 18, B.Min.Y + 151)), WithAlpha(Steel(), 0.5f));
		if (X.Mark < X.Most)
		{
			Label(FString::Printf(TEXT("MARK %lld"), (long long)(X.Mark + 1)), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold, 1.2f)
				.Paint(Out, L + 2, P, B.Min + FVector2f(22, 162));
			Label(X.Next, 20, C, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold)
				.Paint(Out, L + 2, P, B.Min + FVector2f(22, 178));
			const FString Price = X.Hydrogen > 0 ? FString::Printf(TEXT("%s ore · %s MH"), *Grouped(X.Ore), *Grouped(X.Hydrogen))
												 : FString::Printf(TEXT("%s ore"), *Grouped(X.Ore));
			Label(Price, 14, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L + 2, P, B.Min + FVector2f(22, 210));
			Label(TEXT("from the realm wallet"), 11, Dim(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Medium)
				.Paint(Out, L + 2, P, B.Min + FVector2f(22, 230));
		}
		else
		{
			Label(TEXT("LAST MARK"), 14, Ready(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f).Paint(Out, L + 2, P, B.Min + FVector2f(22, 170));
		}
		if (const FTarget* T = Targets.FindByPredicate([&](const FTarget& G) { return G.Row == RowUpgrade && G.Index == I; }))
		{
			PaintButton(Out, L, P, *T, X.Mark < X.Most ? TEXT("UPGRADE") : TEXT("DONE"), UpgradeWhyNot(I).IsEmpty(), C);
		}
	}
}

void SAcCitadelScreen::PaintFoot(FSlateWindowElementList& Out, const int32 L, const FGeometry& P) const
{
	const FBox2f F = FootBox;
	Box(Out, L, P, FBox2f(FVector2f(Pad, F.Min.Y - 8), FVector2f(PanelW - Pad, F.Min.Y - 7)), WithAlpha(RealmGold(), 0.14f));
	FString State = TEXT("Every unload pays the realm its cut; a gift goes all to it.");
	FLinearColor StateColor = FAcHudStyle::Soft();
	if (!Refusal.IsEmpty())
	{
		State = Refusal;
		StateColor = Refused();
	}
	else if (!Kept.IsEmpty())
	{
		State = Kept;
		StateColor = Ready();
	}
	else if (!Signed.IsEmpty())
	{
		State = FString::Printf(TEXT("Signing up: %s…"), *Signed);
	}
	else if (!Bought.IsEmpty())
	{
		State = FString::Printf(TEXT("Buying %s…"), *Bought);
	}
	else if (Asked.Key + Asked.Value > 0)
	{
		State = FString::Printf(TEXT("Giving %s…"), *Purse(Asked.Key, Asked.Value));
	}
	else if (CurrentTab == ETab::Shop && !Info.ShopWhyNot.IsEmpty())
	{
		State = Info.ShopWhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	else if (CurrentTab == ETab::Shop)
	{
		State = TEXT("Paid from your own wallet, no tax. A blueprint builds once; each copy costs more.");
	}
	else if (!MercAsked.IsEmpty())
	{
		State = TEXT("Asking the Citadel…");
	}
	else if (!Started.IsEmpty())
	{
		State = FString::Printf(TEXT("Starting the realm's %s…"), *Started);
	}
	else if (!Upgraded.IsEmpty())
	{
		State = FString::Printf(TEXT("Upgrading the corner turrets' %s…"), *Upgraded);
	}
	else if (CurrentTab == ETab::Defence && !Info.DefenceWhyNot.IsEmpty())
	{
		State = Info.DefenceWhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	else if (CurrentTab == ETab::Defence)
	{
		State = FString::Printf(TEXT("Paid from the realm wallet (%s ore · %s MH): any pilot may upgrade the realm's turrets."),
			*Grouped(Info.RealmOre), *Grouped(Info.RealmHydrogen));
	}
	else if (CurrentTab == ETab::Mercs && !Info.MercsWhyNot.IsEmpty())
	{
		State = Info.MercsWhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	else if (CurrentTab == ETab::Mercs)
	{
		State = TEXT("In a day a mercenary mines what you mine in an hour, even with the game closed. Its ore is yours, taxed; no XP.");
	}
	else if (CurrentTab == ETab::Missions && !Info.MissionsWhyNot.IsEmpty())
	{
		State = Info.MissionsWhyNot;
		StateColor = FAcHudStyle::Amber();
	}
	else if (CurrentTab == ETab::Missions)
	{
		State = FString::Printf(TEXT("Up to %d at once. Your Journal (L) keeps their briefings, places and narrations. New offers at dawn."), Info.Most);
	}
	static const TCHAR* const TabKeys[] = {TEXT("1 2 3 ORE · 4 5 6 MH · CLICK GIVES · TAB SHOP"), TEXT("1-9 BUY A BLUEPRINT · CLICK BUYS · TAB MISSIONS"),
		TEXT("1-6 SIGN UP · CLICK TAKES · TAB DEFENCE"), TEXT("1-3 UPGRADE · CLICK UPGRADES · TAB MERCS"),
		TEXT("1-8 PICK A FIELD · CLICK HIRES, SENDS, DISMISSES · TAB REALM")};
	Label(TabNames[int32(CurrentTab)], 12, RealmGold(), EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::Bold, 1.2f)
		.Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 6));
	Label(State, 15, StateColor, EAcHAlign::Left, EAcVAlign::Top, EAcFontWeight::SemiBold).Paint(Out, L, P, FVector2f(Pad + 4, F.Min.Y + 26));
	const float KX = PanelW - Pad - 4;
	Label(TabKeys[int32(CurrentTab)], 11, Dim(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 8));
	Label(TEXT("H OR ESC CLOSES · THE WORLD GOES ON"), 11, Faint(), EAcHAlign::Right, EAcVAlign::Top, EAcFontWeight::SemiBold, 0.8f)
		.Paint(Out, L, P, FVector2f(KX, F.Min.Y + 28));
}
