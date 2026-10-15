// The MMORPG test mode's client (AcMmo.h) against the real local server, as
// the game runs it: `autocraft-server --world` on the first world started as
// a child (build it first: `make -C unreal/server`), the token, the port, the
// connection, the Prospector the server seats the pilot in at the start pad,
// 2 s of forward input at 60 Hz, and the mirror's Prospector where the server
// moved it. One isolated moment of a few seconds, in real time; the server
// stops at the end. CrossEdgeEast drives the Prospector east over the start
// region's edge and FlyToPad flies it from the start pad to Cold Fork's pad:
// each checks the client's mirror is the new region's, driving it there
// (about 30 s and 10 s). Run hidden:
//   UnrealEditorBG.app/.../UnrealEditor Autocraft.uproject -nullrhi -unattended -nosplash -nosound
//     -ExecCmds="Automation RunTests Autocraft.Mmo; Quit" -TestExit="Automation Test Queue Empty" -abslog=PATH
//     -AcMmoLag=150,20,5
// PredictUnderLag drives under that lag (it sets 150 20 5 itself without the
// flag): the driven Prospector moves on the first input, a round trip before
// the server could move it.
#include "AcMmo.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

	/// Whether `U` can stand all along the straight line from `From` to `To`.
	bool OpenLane(const ac::Simulation& Sim, const ac::Unit& U, const ac::Vec2 From, const ac::Vec2 To)
	{
		const double Length = ac::distance(From, To);
		for (double D = 0; D <= Length; D += 0.25)
			if (!Sim.pilotCanStand(U, From + (To - From) * (D / FMath::Max(Length, 1e-6)))) return false;
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoDriveTest, "Autocraft.Mmo.DriveProspector", Flags)
bool FAcMmoDriveTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	if (!Client.Start(FPlatformTime::Seconds()))
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
	if (!Client.Start(FPlatformTime::Seconds()))
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

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// Started, and seated in the Prospector at the start pad.
	bool StartSeated(FAutomationTestBase& Test, FAcMmoClient& Client)
	{
		if (!Client.Start(FPlatformTime::Seconds()))
		{
			Test.AddError(FString::Printf(TEXT("the local server did not start: %s"), *Client.Message()));
			return false;
		}
		const bool bSeated = RunUntil(Client, [&] {
			return Client.Phase() == FAcMmoClient::EPhase::Failed
				|| (Client.Driven() && Client.Driven() == Client.Wanted() && Client.Here() && !Client.Travelling());
		}, 15.0);
		if (!bSeated || Client.Phase() == FAcMmoClient::EPhase::Failed)
		{
			Test.AddError(FString::Printf(TEXT("not seated: phase %d, %s"), (int32)Client.Phase(), *Client.Message()));
			return false;
		}
		return true;
	}

	/// The client in `RegionId`, driving its Prospector there: the mirror,
	/// the RegionChange and the world map all say so.
	void CheckArrived(FAutomationTestBase& Test, FAcMmoClient& Client, const FString& RegionId, const FString& MapWas)
	{
		const ac::net::Mirror* M = Client.Mirror();
		const ac::net::Client* Net = Client.Connection();
		Test.TestTrue(TEXT("a mirror"), M != nullptr);
		if (!M || !Net) return;
		Test.TestTrue(TEXT("a RegionChange came"), Net->regionChange().has_value());
		if (Net->regionChange()) Test.TestEqual(TEXT("into the region"), Utf(Net->regionChange()->regionId), RegionId);
		Test.TestEqual(TEXT("the world map says so"), Client.Here() ? Utf(Client.Here()->id) : FString(), RegionId);
		Test.TestNotEqual(TEXT("the mirror is the new region's map"), Utf(M->region().mapName), MapWas);
		Test.TestFalse(TEXT("built from its region file"), M->region().regionFile.empty());
		Test.TestTrue(TEXT("the mirror's map is the one described"), M->matches());
		Test.TestEqual(TEXT("the snapshots are the new visit's"), (int64)ac::net::visitOf(M->game()), Client.Visit());
		const std::optional<int64> Id = Client.Driven();
		Test.TestTrue(TEXT("driving there"), Id.has_value() && Id == Client.Wanted());
		const ac::Unit* U = Id ? M->unit(*Id) : nullptr;
		Test.TestTrue(TEXT("its unit in the mirror"), U != nullptr);
		if (!U) return;
		Test.TestEqual(TEXT("a Prospector"), (int32)U->kind, (int32)ac::UnitKind::prospector);
		Test.TestTrue(TEXT("the mirror's pilot drives it"), M->simulation().pilot && M->simulation().pilot->unit == *Id);
		int32 Others = 0;
		for (const ac::Unit& O : M->simulation().state.units) Others += O.id != *Id;
		Test.TestEqual(TEXT("no other units (wild ground, none of the old region's)"), Others, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoCrossEdgeTest, "Autocraft.Mmo.CrossEdgeEast", Flags)
bool FAcMmoCrossEdgeTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	ON_SCOPE_EXIT { Client.Stop(); };
	if (!StartSeated(*this, Client)) return false;
	TestEqual(TEXT("starts in the start region"), Utf(Client.Here()->id), FString(TEXT("start")));
	const FString MapWas = Utf(Client.Mirror()->region().mapName);
	const int64 VisitWas = Client.Visit();
	const int64 GenerationWas = Client.Generation();
	// East to the east edge's opening (rows 41 to 55 of the region file, -7
	// to 7 on the centred ground), the way the pilot pawn steers: into the
	// mirror's pilot, each frame. The opening has the start's MH well in its
	// mouth and boulders either side (W4b): the row is the one nearest the
	// middle with an open lane to the edge (the mirror's own ground says),
	// reached 6 cells east of the pad first. Where it was every 5 s, for a
	// run that does not cross.
	std::optional<ac::Vec2> Turn, Edge;
	{
		const ac::net::Mirror* M = Client.Mirror();
		const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
		if (!U)
		{
			AddError(TEXT("no Prospector to drive"));
			return false;
		}
		const ac::Simulation& Sim = M->simulation();
		const ac::GroundRect& B = Sim.map->bounds;
		const double Middle = (B.minZ + B.maxZ) / 2;
		for (double Off = 0; Off <= 6.0 && !Edge; Off += 0.5)
			for (const double Row : {Middle - Off, Middle + Off})
			{
				const ac::Vec2 A(U->position.x + 6, Row), E(B.maxX - 0.5, Row);
				if (!Edge && OpenLane(Sim, *U, U->position, A) && OpenLane(Sim, *U, A, E))
				{
					Turn = A;
					Edge = ac::Vec2(B.maxX + 4, Row);
				}
			}
		if (!Edge)
		{
			AddError(TEXT("no open lane through the east opening"));
			return false;
		}
		AddInfo(FString::Printf(TEXT("the lane: row %.1f"), Edge->y));
	}
	FString Trail;
	double TrailAt = FPlatformTime::Seconds();
	const bool bCrossed = RunUntil(Client, [&] {
		if (Client.Phase() == FAcMmoClient::EPhase::Failed) return true;
		if (Client.Visit() > VisitWas && !Client.Travelling()) return true;
		ac::net::Mirror* M = Client.Mirror();
		ac::Simulation* S = M ? &M->simulation() : nullptr;
		const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
		if (S && S->pilot && U)
		{
			if (Turn && U->position.x >= Turn->x - 0.5) Turn.reset();
			const ac::Vec2 To = Turn ? *Turn : *Edge;
			S->pilot->walk = ac::Vec2(1, 0);
			S->pilot->heading = std::atan2(To.y - U->position.y, To.x - U->position.x);
			if (FPlatformTime::Seconds() >= TrailAt)
			{
				TrailAt += 5.0;
				Trail += FString::Printf(TEXT(" (%.1f,%.1f)"), U->position.x, U->position.y);
			}
		}
		return false;
	}, 60.0);
	if (!bCrossed || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		const ac::net::Mirror* M = Client.Mirror();
		const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
		AddError(FString::Printf(TEXT("did not cross: phase %d, %s; visit %lld, unit at %.1f,%.1f; every 5 s:%s"), (int32)Client.Phase(),
			*Client.Message(), (long long)Client.Visit(), U ? U->position.x : 0.0, U ? U->position.y : 0.0, *Trail));
		return false;
	}
	// The World message may come a moment after the RegionChange, and the new
	// region's first snapshot after both: the mirror made ahead (W4) drives the
	// carried-over Prospector until it comes.
	RunUntil(Client, [&] {
		return Client.Here() && Utf(Client.Here()->id) == TEXT("tern-ridge") && Client.Mirror()
			&& ac::net::visitOf(Client.Mirror()->game()) == Client.Visit();
	}, 3.0);
	CheckArrived(*this, Client, TEXT("tern-ridge"), MapWas);
	TestEqual(TEXT("one region change"), Client.Visit(), VisitWas + 1);
	TestTrue(TEXT("the game rebuilds on the new mirror"), Client.Generation() > GenerationWas);
	if (const ac::net::Mirror* M = Client.Mirror(); M && Client.Driven())
	{
		if (const ac::Unit* U = M->unit(*Client.Driven()))
		{
			const double In = U->position.x - M->simulation().map->bounds.minX;
			AddInfo(FString::Printf(TEXT("came in %.2f cells from the west edge"), In));
			TestTrue(TEXT("came in at the neighbour's west edge"), In < 8.0);
		}
	}
	// Away from a pad: the flight is refused here, with the reason.
	FString Why;
	TestFalse(TEXT("no flight away from a pad"), Client.Fly(TEXT("start"), TEXT("landing"), &Why));
	TestTrue(TEXT("says why"), Why.Contains(TEXT("pad")));
	AddInfo(FString::Printf(TEXT("refused: %s"), *Why));
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoFlyTest, "Autocraft.Mmo.FlyToPad", Flags)
bool FAcMmoFlyTest::RunTest(const FString&)
{
	FAcMmoClient Client;
	ON_SCOPE_EXIT { Client.Stop(); };
	// A flight costs MH (F1): a new pilot has none until a Derrick pumps. F2:
	// Landing Pads put up by the server's dev control.
	const FString Control = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AcMmoFlyTest.control"));
	IFileManager::Get().Delete(*Control, false, true, true);
	Client.ExtraServerArgs = {"--dev-hydrogen", "100", "--dev-control", std::string(TCHAR_TO_UTF8(*Control))};
	if (!StartSeated(*this, Client)) return false;
	const FString MapWas = Utf(Client.Mirror()->region().mapName);
	const int64 VisitWas = Client.Visit();
	FString Why;
	TestFalse(TEXT("no Landing Pad yet: no flight"), Client.Fly(TEXT("cold-fork"), TEXT("Landing Pad 1"), &Why));
	AddInfo(FString::Printf(TEXT("refused: %s"), *Why));
	FFileHelper::SaveStringToFile(TEXT("ferry pad here\nferry pad cold-fork\n"), *Control);
	const bool bPads = RunUntil(Client, [&] {
		const std::optional<FAcMmoClient::FPadNear> Near = Client.NearestPad();
		return Near && Near->Within() && Client.Seen("cold-fork");
	}, 5.0);
	TestTrue(TEXT("on a Landing Pad, Cold Fork seen"), bPads);
	const std::optional<FAcMmoClient::FPadNear> Near = Client.NearestPad();
	TestTrue(TEXT("at the start's Landing Pad"), Near && Near->Name == TEXT("Landing Pad 1") && Near->Within());
	// The price, a grid step away: 10 + 5 MH, paid at the call.
	TestEqual(TEXT("a flight to Cold Fork costs 15 MH"), Client.FlightPrice(TEXT("cold-fork")).value_or(-1), int64(15));
	// (The wallet: 100 MH and what the first contracts paid by now.)
	const int64 HydrogenWas = Client.HydrogenNow().value_or(-1);
	TestTrue(TEXT("at least 100 MH in the wallet"), HydrogenWas >= 100);
	TestFalse(TEXT("a region file's pad is no Landing Pad"), Client.Fly(TEXT("cold-fork"), TEXT("landing"), &Why));
	const bool bAsked = Client.Fly(TEXT("cold-fork"), TEXT("Landing Pad 1"), &Why);
	TestTrue(FString::Printf(TEXT("the flight asked for (%s)"), *Why), bAsked);
	if (!bAsked) return false;
	const bool bLifted = RunUntil(Client, [&] { return Client.FlyingTo() != nullptr || !Client.FlightRefusal().IsEmpty(); }, 5.0);
	TestTrue(FString::Printf(TEXT("aboard (%s)"), *Client.FlightRefusal()), bLifted && Client.FlyingTo());
	if (Client.FlyingTo()) TestEqual(TEXT("flying to Cold Fork"), Utf(Client.FlyingTo()->id), FString(TEXT("cold-fork")));
	TestTrue(TEXT("travelling: no unit to drive"), Client.Travelling());
	TestTrue(TEXT("the ferry followed"), Client.Ferry().IsSet());
	const double CalledAt = FPlatformTime::Seconds();
	// The approach: in Cold Fork, nothing driven yet.
	const bool bSwitched = RunUntil(Client, [&] { return Client.Phase() == FAcMmoClient::EPhase::Failed || Client.Visit() > VisitWas; }, 30.0);
	TestTrue(TEXT("into Cold Fork at the approach"), bSwitched && Client.Visit() > VisitWas);
	TestTrue(TEXT("still aboard at the approach"), Client.Travelling() && Client.FlyingTo());
	TestTrue(TEXT("the approach's change seamless"), Client.LastWasSeamless());
	const bool bLanded = RunUntil(Client, [&] {
		return Client.Phase() == FAcMmoClient::EPhase::Failed || (!Client.Travelling() && Client.Driven());
	}, 15.0);
	if (!bLanded || Client.Phase() == FAcMmoClient::EPhase::Failed)
	{
		AddError(FString::Printf(TEXT("did not land: phase %d, %s, visit %lld"), (int32)Client.Phase(), *Client.Message(), (long long)Client.Visit()));
		return false;
	}
	AddInfo(FString::Printf(TEXT("landed %.1f s after the call"), FPlatformTime::Seconds() - CalledAt));
	RunUntil(Client, [&] { return Client.Here() && Utf(Client.Here()->id) == TEXT("cold-fork") && !Client.FlyingTo(); }, 3.0);
	CheckArrived(*this, Client, TEXT("cold-fork"), MapWas);
	TestTrue(TEXT("no longer flying"), Client.FlyingTo() == nullptr && !Client.Ferry().IsSet());
	const std::optional<FAcMmoClient::FPadNear> There = Client.NearestPad();
	TestTrue(TEXT("on Cold Fork's Landing Pad"), There && There->Name == TEXT("Landing Pad 1") && There->Within());
	TestEqual(TEXT("15 MH paid for the flight"), HydrogenWas - Client.HydrogenNow().value_or(-1), int64(15));
	TestTrue(TEXT("still connected"), Client.Phase() == FAcMmoClient::EPhase::Playing);
	return true;
}

#endif
