// The editor's Play and Stop, twice, in the playground (`ac.Playground 1`).
// Stop marks every object under the PIE world as garbage, and a rooted one
// there fails `check(!IsRooted())` (UWorld::MarkObjectsPendingKill): the
// editor died so on 2026-10-17 with the pilot hall's submix, which the
// audio device roots while it is registered (AcPilotAudio.cpp, MakeHall).
// Each cycle asks that nothing of ours under the PIE world is rooted before Stop,
// and after the two that the halls are let go (none rooted, none piling up).
// Needs an audio device (the hall is made only with one), so it runs with
// -deterministicaudio (the offline mixer: nothing reaches the speakers):
//   uv run unreal/Tools/hidden_run.py --out DIR --tests Autocraft.Editor.PieCycle --sound-offline
#include "Misc/AutomationTest.h"

#include "AcLog.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Sound/SoundSubmix.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"

namespace
{
	constexpr int32 Cycles = 2;
	constexpr double PlaySeconds = 4.0, StartTimeout = 120.0, StopTimeout = 30.0;

	struct FHalls
	{
		int32 Alive = 0, Rooted = 0;
	};

	FHalls CountHalls()
	{
		FHalls H;
		for (TObjectIterator<USoundSubmix> It; It; ++It)
		{
			if (!It->GetName().StartsWith(TEXT("AcPilotHall"))) continue;
			H.Alive += 1;
			if (It->IsRooted()) H.Rooted += 1;
		}
		return H;
	}

	/// Made by this module: the object, or one of its outers, is of a class
	/// of /Script/Autocraft. (The engine roots some of its own while playing
	/// and lets them go as the world is cleaned up: the nav system's
	/// CrowdManager.)
	bool Ours(const UObject* O)
	{
		for (; O && !O->IsA<UWorld>(); O = O->GetOuter())
		{
			if (O->GetClass()->GetOutermost()->GetFName() == TEXT("/Script/Autocraft")) return true;
		}
		return false;
	}

	/// A latent step that waits for `Done`, failing after `Timeout` seconds.
	TFunction<bool()> WaitFor(FAutomationTestBase* Test, const TCHAR* What, double Timeout, TFunction<bool()> Done)
	{
		return [Test, What, Timeout, Done, Since = TOptional<double>()]() mutable
		{
			if (!Since) Since = FPlatformTime::Seconds();
			if (Done()) return true;
			if (FPlatformTime::Seconds() - *Since < Timeout) return false;
			Test->AddError(FString::Printf(TEXT("pie cycle: timed out waiting for %s"), What));
			return true;
		};
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcPieCycleTest, "Autocraft.Editor.PieCycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAcPieCycleTest::RunTest(const FString&)
{
	if (!GEditor)
	{
		AddError(TEXT("no editor"));
		return false;
	}
	IConsoleVariable* Playground = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Playground"));
	if (!TestNotNull(TEXT("ac.Playground"), Playground)) return false;
	const int32 WasPlayground = Playground->GetInt();
	Playground->Set(1, ECVF_SetByCode);
	TSharedRef<TArray<FHalls>> After = MakeShared<TArray<FHalls>>();
	for (int32 Cycle = 0; Cycle < Cycles; ++Cycle)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([]
		{
			FRequestPlaySessionParams Params;
			GEditor->RequestPlaySession(Params);
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitFor(this, TEXT("Play"), StartTimeout,
			[] { return GEditor->PlayWorld != nullptr; })));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitFor(this, TEXT("nothing"), PlaySeconds + 1,
			[Since = TOptional<double>()]() mutable
			{
				if (!Since) Since = FPlatformTime::Seconds();
				return FPlatformTime::Seconds() - *Since >= PlaySeconds;
			})));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Cycle]
		{
			UWorld* World = GEditor->PlayWorld;
			if (!World)
			{
				AddError(TEXT("pie cycle: the PIE world went away while playing"));
				return true;
			}
			int32 Objects = 0, Rooted = 0;
			ForEachObjectWithOuter(World, [&](UObject* O)
			{
				Objects += 1;
				if (!O->IsRooted() || !Ours(O)) return;
				Rooted += 1;
				// Logged as well: Stop would fail its check before the test reports.
				const FString Says = FString::Printf(TEXT("pie cycle %d: rooted under the PIE world: %s"), Cycle + 1, *O->GetFullName());
				UE_LOG(LogAutocraft, Error, TEXT("%s"), *Says);
				AddError(Says);
			}, true);
			const FHalls H = CountHalls();
			UE_LOG(LogAutocraft, Log, TEXT("pie cycle %d: playing %s, %d objects under the world, %d rooted; halls %d (%d rooted)"),
				Cycle + 1, *World->GetMapName(), Objects, Rooted, H.Alive, H.Rooted);
			GEditor->RequestEndPlayMap();
			return true;
		}));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand(WaitFor(this, TEXT("Stop"), StopTimeout,
			[] { return GEditor->PlayWorld == nullptr && !GEditor->IsPlaySessionInProgress(); })));
		ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, Cycle, After]
		{
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			const FHalls H = CountHalls();
			After->Add(H);
			UE_LOG(LogAutocraft, Log, TEXT("pie cycle %d: stopped; halls left %d (%d rooted)"), Cycle + 1, H.Alive, H.Rooted);
			return true;
		}));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, After, Playground, WasPlayground]
	{
		Playground->Set(WasPlayground, ECVF_SetByCode);
		if (After->Num() != Cycles) return true;
		TestEqual(TEXT("halls rooted after Stop"), (*After)[Cycles - 1].Rooted, 0);
		TestTrue(TEXT("halls do not pile up across Play/Stop"), (*After)[Cycles - 1].Alive <= (*After)[0].Alive);
		return true;
	}));
	return true;
}

#endif
