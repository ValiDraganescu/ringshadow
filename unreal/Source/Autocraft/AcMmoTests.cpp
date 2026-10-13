// The MMORPG test mode's client (AcMmo.h) against the real local server, as
// the game runs it: `autocraft-server` started as a child (build it first:
// `make -C unreal/server`), the token, the port, the connection, the
// Prospector taken on the Welcome, 2 s of forward input at 60 Hz, and the
// mirror's Prospector where the server moved it. One isolated moment of a
// few seconds, in real time; the server stops at the end. Run hidden:
//   UnrealEditorBG.app/.../UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Mmo; Quit" -TestExit="Automation Test Queue Empty" -abslog=PATH
#include "AcMmo.h"

#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#include "Simulation.h"

#include <cmath>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/// Ticks the client at about 120 frames a second, in real time, until
	/// `Done` or `Seconds` pass.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoDriveTest, "Autocraft.Mmo.DriveProspector", Flags)
bool FAcMmoDriveTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	const ac::MapChoice Map{ac::MapStyle::openField, ac::MapSize::small, 2};
	if (!Client.Start(Map, FPlatformTime::Seconds()))
	{
		AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
		return false;
	}
	// The Welcome, the first snapshot, and the Prospector it asked for.
	const bool bDriving = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed || (Client.Driven() && Client.Driven() == Client.Wanted());
	}, 15.0);
	if (!bDriving || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("no Prospector driven: phase %d, %s"), (int32)Client.Phase(), *Client.Message()));
		return false;
	}
	ac::net::Mirror& M = *Client.Mirror();
	const int64 Id = *Client.Driven();
	const ac::Unit* Start = M.unit(Id);
	TestTrue(TEXT("the mirror holds the driven unit"), Start != nullptr);
	if (!Start) return false;
	TestEqual(TEXT("a Prospector"), (int32)Start->kind, (int32)ac::UnitKind::prospector);
	TestEqual(TEXT("of the pilot's side"), (int64)Start->owner, (int64)ac::Pilot::player);
	TestTrue(TEXT("the mirror's pilot is it"), M.simulation().pilot && M.simulation().pilot->unit == Id);
	const ac::Vec2 From = Start->position;
	// Where the ground is open longest, as the mirror's map says.
	double Heading = 0, Best = -1;
	for (int32 K = 0; K < 16; ++K)
	{
		const double H = K * 2.0 * PI / 16;
		double Clear = 0;
		while (Clear < 12 && M.simulation().pilotCanStand(*Start, From + ac::Vec2(std::cos(H), std::sin(H)) * (Clear + 0.25))) Clear += 0.25;
		if (Clear > Best)
		{
			Best = Clear;
			Heading = H;
		}
	}
	// Forward for 2 s, as the pilot pawn writes it into the mirror's pilot.
	const int64 TickBefore = Client.ClientTick();
	RunUntil(Client, [&] {
		if (ac::Simulation& S = M.simulation(); S.pilot)
		{
			S.pilot->walk = ac::Vec2(1, 0);
			S.pilot->heading = Heading;
		}
		return false;
	}, 2.0);
	const int64 Sent = Client.ClientTick() - TickBefore;
	if (M.simulation().pilot) M.simulation().pilot->walk = ac::Vec2(0, 0);
	// The rest of the inputs used and the snapshots after them in.
	RunUntil(Client, [] { return false; }, 0.6);
	const ac::Unit* Now = M.unit(Id);
	TestTrue(TEXT("still in the mirror"), Now != nullptr);
	if (!Now) return false;
	const double Walked = ac::distance(Now->position, From);
	AddInfo(FString::Printf(TEXT("drove %.2f cells in 2 s (%lld inputs sent, %.1f cells clear ahead)"), Walked, (long long)Sent, Best));
	TestTrue(TEXT("about 60 inputs a second"), Sent >= 100 && Sent <= 130);
	TestTrue(TEXT("the mirror's Prospector moved"), Walked > 3.0);
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	Client.Stop();
	TestTrue(TEXT("stopped"), Client.Phase() == FAcMmoClient::EPhase::Off);
	return true;
}

#endif
