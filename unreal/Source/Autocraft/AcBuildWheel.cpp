#include "AcBuildWheel.h"

#include "AcConsoleInfo.h"
#include "AcPilotText.h"
#include "AcProgressHud.h"

#include "Pilot.h"
#include "Simulation.h"

#include <cmath>

int32 FAcBuildWheel::Sector(const FVector2D Stick, const int32 N)
{
	if (N <= 0 || Stick.Size() < DeadZone) return INDEX_NONE;
	// Clockwise from the top: x right, y up.
	double A = std::atan2(Stick.X, Stick.Y);
	const double Slice = 2.0 * UE_DOUBLE_PI / N;
	// Slice 0 is centred on the top: shift by half a slice.
	A = std::fmod(A + Slice / 2 + 4.0 * UE_DOUBLE_PI, 2.0 * UE_DOUBLE_PI);
	return FMath::Clamp(int32(A / Slice), 0, N - 1);
}

void FAcBuildWheel::Move(const FVector2D Delta)
{
	Stick += Delta;
	if (const double L = Stick.Size(); L > Reach) Stick *= Reach / L;
}

TArray<FAcWheelSlice> FAcBuildWheel::Slices(const ac::Simulation& Sim, const FAcMmoClient* Mmo, const bool bMmo)
{
	TArray<FAcWheelSlice> Out;
	const TArray<ac::StructureKind> Kinds = AcPilotText::BuildKinds(bMmo);
	const ac::Player& Me = Sim.state.players[ac::Pilot::player];
	for (int32 I = 0; I < Kinds.Num(); ++I)
	{
		const ac::StructureKind K = Kinds[I];
		FAcWheelSlice S;
		S.Kind = K;
		// The number keys reach 1 to 9.
		S.Number = I < 9 ? I + 1 : 0;
		S.Title = AcPilotText::Title(K).ToUpper();
		S.Icon = FAcConsoleInfo::StructureIconName((int32)K);
		const ac::Price Cost = Sim.buildCost(K, ac::Pilot::player);
		S.Ore = (int32)Cost.ore;
		S.Hydrogen = (int32)Cost.hydrogen;
		// The blueprint first (none, under its floor, the realm's limit),
		// then the core's own refusal (the price short, a Hab Dome needed).
		const FString Lock = bMmo ? AcProgressHud::BlueprintRefusal(Mmo, K) : FString();
		if (!Lock.IsEmpty())
		{
			S.State = FAcWheelSlice::EState::Locked;
			S.Why = Lock;
			S.Tag = AcProgressHud::BlueprintTag(Mmo, K);
		}
		else if (const std::optional<std::string> Why = Sim.buildRefusal(K, ac::Pilot::player))
		{
			S.State = FAcWheelSlice::EState::Locked;
			S.Why = FString(UTF8_TO_TCHAR(Why->c_str()));
			TArray<FString> Short;
			if (Me.ore < S.Ore) Short.Add(FString::Printf(TEXT("%d ORE"), S.Ore));
			if (Me.hydrogen < S.Hydrogen) Short.Add(FString::Printf(TEXT("%d MH"), S.Hydrogen));
			S.Tag = Short.IsEmpty() ? S.Why.ToUpper() : TEXT("NEEDS ") + FString::Join(Short, TEXT(" AND "));
		}
		else if (bMmo)
		{
			const int64 N = AcProgressHud::Copies(Mmo, K);
			S.Tag = FString::Printf(TEXT("×%lld %s"), (long long)N, N == 1 ? TEXT("COPY") : TEXT("COPIES"));
		}
		Out.Add(S);
	}
	// The Target Dummy last (no number key), until the core has the kind.
	if (bMmo)
	{
		FAcWheelSlice D;
		D.Title = TEXT("TARGET DUMMY");
		D.Icon = TEXT("act.strike");
		D.Ore = 10;
		D.State = FAcWheelSlice::EState::Soon;
		D.Tag = TEXT("SOON");
		D.Why = TEXT("The Target Dummy comes soon");
		Out.Add(D);
	}
	return Out;
}
