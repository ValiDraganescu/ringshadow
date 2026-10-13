// The MMORPG test mode's client (AcMmo.h) against the real local server, as
// the game runs it: `autocraft-server` started as a child (build it first:
// `make -C unreal/server`), the token, the port, the connection, the
// Prospector taken on the Welcome, 2 s of forward input at 60 Hz, and the
// mirror's Prospector where the server moved it. One isolated moment of a
// few seconds, in real time; the server stops at the end. Run hidden:
//   UnrealEditorBG.app/.../UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Mmo; Quit" -TestExit="Automation Test Queue Empty" -abslog=PATH
//     -AcMmoLag=150,20,5
// PredictUnderLag drives under that lag (it sets 150 20 5 itself without the
// flag): the driven Prospector moves on the first input, a round trip before
// the server could move it.
#include "AcMmo.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

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

	/// The unit as the game draws it: the mirror's state.
	const ac::Unit* Drawn(const ac::net::Mirror& M, const int64 Id)
	{
		const std::optional<int64_t> I = M.simulation().unitIndex(Id);
		return I ? &M.simulation().state.units[size_t(*I)] : nullptr;
	}

	/// The heading with the most open ground ahead of `U`, and how much.
	double ClearestHeading(const ac::Simulation& Sim, const ac::Unit& U, double& Best)
	{
		double Heading = 0;
		Best = -1;
		for (int32 K = 0; K < 16; ++K)
		{
			const double H = K * 2.0 * PI / 16;
			double Clear = 0;
			while (Clear < 12 && Sim.pilotCanStand(U, U.position + ac::Vec2(std::cos(H), std::sin(H)) * (Clear + 0.25))) Clear += 0.25;
			if (Clear > Best)
			{
				Best = Clear;
				Heading = H;
			}
		}
		return Heading;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoPredictTest, "Autocraft.Mmo.PredictUnderLag", Flags)
bool FAcMmoPredictTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	const ac::MapChoice Map{ac::MapStyle::openField, ac::MapSize::small, 2};
	if (!Client.Start(Map, FPlatformTime::Seconds()))
	{
		AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
		return false;
	}
	// The lag: -AcMmoLag (Start read it), else 150 ms, 20 ms jitter, 5% lost.
	IConsoleVariable* LagVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.MmoLag"));
	const FString LagWas = LagVar ? LagVar->GetString() : FString();
	ac::net::LagSettings Lag;
	const bool bOwnLag = LagVar && (!FAcMmoClient::ParseLag(LagWas, Lag) || Lag.delay <= 0);
	if (bOwnLag) LagVar->Set(TEXT("150 20 5"), ECVF_SetByCode);
	ON_SCOPE_EXIT
	{
		if (bOwnLag) LagVar->Set(*LagWas, ECVF_SetByCode);
	};
	const bool bDriving = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed
			|| (Client.Driven() && Client.Driven() == Client.Wanted() && Client.Prediction().predicted());
	}, 20.0);
	if (!bDriving || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("no Prospector driven: phase %d, %s"), (int32)Client.Phase(), *Client.Message()));
		return false;
	}
	TestTrue(TEXT("lag on the connection"), Client.Lag().delay >= 0.1);
	ac::net::Mirror& M = *Client.Mirror();
	const int64 Id = *Client.Driven();
	// Settled: the snapshots after the take in.
	RunUntil(Client, [] { return false; }, 0.5);
	const ac::Unit Start = *Client.Prediction().predicted();
	const ac::Vec2 ServerFrom = M.unit(Id)->position;
	double Best = 0;
	const double Heading = ClearestHeading(M.simulation(), Start, Best);
	auto Steer = [&](const ac::Vec2 Walk)
	{
		if (ac::Simulation& S = M.simulation(); S.pilot)
		{
			S.pilot->walk = Walk;
			S.pilot->heading = Heading;
		}
	};

	// The first frame that sends an input: the drawn Prospector moves on it.
	const int64 TickBefore = Client.ClientTick();
	bool bSent = false;
	for (int32 I = 0; I < 240 && !bSent; ++I)
	{
		Steer(ac::Vec2(1, 0));
		Client.Tick(FPlatformTime::Seconds());
		bSent = Client.ClientTick() > TickBefore;
		if (!bSent) FPlatformProcess::Sleep(1.0f / 120.0f);
	}
	TestTrue(TEXT("an input went out"), bSent);
	const ac::Unit* First = Drawn(M, Id);
	TestTrue(TEXT("drawn"), First != nullptr);
	if (!First) return false;
	const double FirstStep = ac::distance(First->position, Start.position);
	const double ServerFirst = ac::distance(M.unit(Id)->position, ServerFrom);
	TestTrue(TEXT("the drawn Prospector moved on the first input frame"), FirstStep > 0.01);
	TestTrue(TEXT("before the server's copy did (no round trip yet)"), ServerFirst < 1e-6);

	// Two seconds ahead, then the keys let go and the server catches up.
	Client.ClearPredictionStats();
	RunUntil(Client, [&] {
		Steer(ac::Vec2(1, 0));
		return false;
	}, 2.0);
	const ac::net::Predictor::Stats Driving = Client.Prediction().stats();
	const double Walked = ac::distance(Client.Prediction().predicted()->position, Start.position);
	RunUntil(Client, [&] {
		Steer(ac::Vec2(0, 0));
		return false;
	}, 1.5);
	const ac::Unit* Server = M.unit(Id);
	const ac::Unit* Shown = Drawn(M, Id);
	TestTrue(TEXT("still in the mirror"), Server && Shown);
	if (!Server || !Shown) return false;
	const double Gap = ac::distance(Client.Prediction().predicted()->position, Server->position);
	const double DrawnGap = ac::distance(Shown->position, Server->position);
	const std::optional<double> Rtt = Client.RoundTrip();
	AddInfo(FString::Printf(TEXT("first input frame moved it %.3f cells (server's copy %.3f); drove %.2f cells in 2 s at RTT %.0f ms"),
		FirstStep, ServerFirst, Walked, Rtt.value_or(0) * 1000));
	AddInfo(FString::Printf(TEXT("%lld corrections (mean %.4f, max %.4f cells), %lld snaps, drawn unit eased %.5f cells a frame; at rest %.4f cells off (drawn %.4f)"),
		(long long)Driving.corrections, Driving.corrections ? Driving.errorSum / Driving.corrections : 0.0, Driving.errorMax,
		(long long)Driving.snaps, Driving.frames ? Driving.easedSum / Driving.frames : 0.0, Gap, DrawnGap));
	TestTrue(TEXT("a round trip of the lag set"), Rtt && *Rtt > 0.25);
	TestTrue(TEXT("it drove"), Walked > 3.0);
	TestTrue(TEXT("at rest, the prediction is the server's"), Gap < 0.02);
	TestTrue(TEXT("and so is the drawn unit"), DrawnGap < 0.02);
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	Client.Stop();
	return true;
}

#endif
