// Leveling in the MMORPG mode against the real local server on the first
// world. Since P3 a world has no level-up cards: the server owns the pilot's
// progression (Progression.h), and `--dev-xp` gives a new pilot that much of
// its XP at once. The server stages level 2 (`--dev-xp` = Progression::xp(2));
// the Progress message shows level 2 and its one skill point; a card is
// refused here (the driven kind has none on offer); the point goes into a
// tier 1 node through a Spend request; the Progress message shows the rank
// and no point left. One isolated moment of a few seconds, in real time. Run
// hidden as AcMmoTests.cpp says (`Automation RunTests Autocraft.Mmo.LevelPick`).
#include "AcLeveling.h"
#include "AcMmo.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#include "Leveling.h"
#include "Progression.h"
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
	const double Xp = ac::Progression::xp(2);
	FAcMmoClient Client;
	Client.ExtraServerArgs = {"--dev-xp", std::to_string(Xp)};
	if (!Client.Start(FPlatformTime::Seconds()))
	{
		AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
		return false;
	}
	const bool bLeveled = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed
			|| (Client.Driven() && Client.Mirror() && Client.Mirror()->unit(*Client.Driven()) && Client.Progress()
				&& Client.Progress()->level == 2);
	}, 15.0);
	if (!bLeveled || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("no level 2 pilot driving: phase %d, %s, level %lld"), (int32)Client.Phase(), *Client.Message(),
			Client.Progress() ? (long long)Client.Progress()->level : 0LL));
		return false;
	}
	TestEqual(TEXT("its XP, from the server"), Client.Progress()->xp, Xp);
	TestEqual(TEXT("level 2's one skill point"), Client.Progress()->points, int64(1));

	// No cards in a world: none on offer, refused here, nothing sent.
	const ac::PilotRecord Record = Client.Mirror()->simulation().levels();
	TestFalse(TEXT("no cards on offer"), FAcPickOffer::Of(Record, Prospector).IsSet());
	FString Why;
	TestFalse(TEXT("a card refused"), Client.Pick(Prospector, ac::Perk::prospectorQuickDrill, &Why));
	TestFalse(TEXT("nothing on its way"), Client.PickInFlight());
	AddInfo(FString::Printf(TEXT("card refused: %s"), *Why));

	// The point into the first tier 1 node with ranks to fill, through the server.
	std::optional<ac::Skill> Skill;
	for (const ac::SkillNode& N : ac::Progression::nodes())
		if (N.tier == 1 && !N.active)
		{
			Skill = N.skill;
			break;
		}
	if (!Skill)
	{
		AddError(TEXT("no tier 1 node"));
		return false;
	}
	TestTrue(TEXT("asked"), Client.Spend(*Skill, &Why));
	const bool bKept = RunUntil(Client, [&] {
		if (!Client.SkillRefusal().IsEmpty()) return true;
		const ac::net::ProgressMessage* P = Client.Progress();
		return Client.SkillsKept() == 1 && P && P->ranks.count(*Skill) && P->ranks.at(*Skill) == 1;
	}, 5.0);
	TestTrue(FString::Printf(TEXT("the point is kept and shows in the progress (%s)"), *Client.SkillRefusal()), bKept);
	TestTrue(TEXT("no refusal"), Client.SkillRefusal().IsEmpty());
	TestEqual(TEXT("no point left"), Client.Progress() ? Client.Progress()->points : -1, int64(0));
	AddInfo(FString::Printf(TEXT("spent on %s at level %lld"), *FString(UTF8_TO_TCHAR(ac::Progression::node(*Skill).title.c_str())),
		Client.Progress() ? (long long)Client.Progress()->level : 0LL));
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	Client.Stop();
	return true;
}

#endif
