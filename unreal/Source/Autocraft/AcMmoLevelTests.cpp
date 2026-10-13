// Leveling in the MMORPG mode (AcMmo.h `Pick`, docs/mmorpg-m3.md S3) against
// the real local server on the first world: the server stages the level
// (`--dev-xp 60`, as the dev XP cheat: the new pilot's Prospector seated at
// the start pad with 60 XP, level 2); the mirror shows level 2 and level 2's
// two cards; a card not on offer is refused here; the first card is picked
// through a Pick request; the mirror shows it kept and nothing more waiting.
// One isolated moment of a few seconds, in real time. Run hidden as
// AcMmoTests.cpp says (`Automation RunTests Autocraft.Mmo.LevelPick`).
#include "AcLeveling.h"
#include "AcMmo.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#include "Leveling.h"
#include "Simulation.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	template <class F> bool RunUntil(FAcMmoClient& Client, F&& Done, const double Seconds)
	{
		const double End = FPlatformTime::Seconds() + Seconds;
		while (FPlatformTime::Seconds() < End)
		{
			Client.Tick(FPlatformTime::Seconds());
			if (Done()) return true;
			FPlatformProcess::Sleep(1.0f / 120.0f);
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoLevelPickTest, "Autocraft.Mmo.LevelPick", Flags)
bool FAcMmoLevelPickTest::RunTest(const FString&)
{
	constexpr ac::UnitKind Prospector = ac::UnitKind::prospector;
	FAcMmoClient Client;
	Client.ExtraServerArgs = {"--dev-xp", "60"};
	if (!Client.Start(FPlatformTime::Seconds()))
	{
		AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
		return false;
	}
	auto Levels = [&]() -> ac::PilotRecord
	{
		return Client.Mirror() ? Client.Mirror()->simulation().levels() : ac::PilotRecord();
	};
	const bool bLeveled = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed
			|| (Client.Driven() && Client.Mirror() && Client.Mirror()->unit(*Client.Driven()) && Levels().level(Prospector) == 2);
	}, 15.0);
	if (!bLeveled || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("no level 2 Prospector driven: phase %d, %s, level %lld"), (int32)Client.Phase(), *Client.Message(),
			(long long)Levels().level(Prospector)));
		return false;
	}
	TestEqual(TEXT("60 XP, from the server"), Levels().xp(Prospector), 60.0);
	// The cards the HUD shows (the driven kind's offer).
	const TOptional<FAcPickOffer> Offer = FAcPickOffer::Of(Levels(), Prospector);
	if (!Offer)
	{
		AddError(TEXT("no cards on offer at level 2"));
		return false;
	}
	TestEqual(TEXT("level 2's cards"), Offer->Level, 2);
	TestTrue(TEXT("Quick drill or Light frame"),
		Offer->Perks == TArray<ac::Perk>({ac::Perk::prospectorQuickDrill, ac::Perk::prospectorLightFrame}));

	// Not on offer: refused here, nothing sent.
	FString Why;
	TestFalse(TEXT("level 4's card refused"), Client.Pick(Prospector, ac::Perk::prospectorBigHaul, &Why));
	TestEqual(TEXT("why"), Why, FString(TEXT("Not on offer")));
	TestFalse(TEXT("nothing on its way"), Client.PickInFlight());

	// The first card, through the server.
	const ac::Perk Card = Offer->Perks[0];
	TestTrue(TEXT("asked"), Client.Pick(Prospector, Card, &Why));
	TestFalse(TEXT("a second pick waits for the first"), Client.Pick(Prospector, Offer->Perks[1], &Why));
	const bool bKept = RunUntil(Client, [&] {
		return Client.PicksKept() == 1 && Levels().picks(Prospector) == std::vector<ac::Perk>{Card};
	}, 5.0);
	TestTrue(TEXT("the pick is kept and shows in the mirror"), bKept);
	TestTrue(TEXT("no refusal"), Client.PickRefusal().IsEmpty());
	TestTrue(TEXT("nothing more waiting"), !FAcPickOffer::Of(Levels(), Prospector).IsSet());
	TestTrue(TEXT("the mirror's driven Prospector has it on"), Client.Mirror()->simulation().active(Card));
	AddInfo(FString::Printf(TEXT("picked %s at level %lld"), *AcLeveling::Name(Card), (long long)Levels().level(Prospector)));
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	Client.Stop();
	return true;
}

#endif
