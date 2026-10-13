#include "AcMmo.h"

#include "AcLog.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#include <string>
#include <unistd.h>

namespace
{
	TAutoConsoleVariable<FString> CVarMmoLag(TEXT("ac.MmoLag"), TEXT(""),
		TEXT("Lag on the MMORPG test's connection, each way: \"delay_ms jitter_ms loss_pct\" (e.g. 150 20 5); empty or 0: none. ")
		TEXT("-AcMmoLag=150,20,5 sets it at the start."),
		ECVF_Default);

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	std::string Utf8(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }

	/// "~/Library/…" for the player's eyes.
	FString Tilde(const FString& Path)
	{
		const FString Home = FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT(""));
		return Path.StartsWith(Home) ? TEXT("~/") + Path.RightChop(Home.Len()) : Path;
	}
}

FAcMmoClient::FAcMmoClient() = default;

FAcMmoClient::~FAcMmoClient()
{
	Stop();
}

FString FAcMmoClient::ServerProgram()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("server"), TEXT("build"), TEXT("autocraft-server")));
}

FString FAcMmoClient::ServerLog()
{
	return FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Library"), TEXT("Logs"), TEXT("Autocraft"), TEXT("server.log"));
}

FString FAcMmoClient::DefaultWorld()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("server"), TEXT("worlds"), TEXT("first.json")));
}

void FAcMmoClient::Fail(const FString& Why)
{
	CurrentPhase = EPhase::Failed;
	StatusMessage = Why;
	UE_LOG(LogAutocraft, Warning, TEXT("mmo: %s"), *Why);
	Server.stop(3.0);
}

bool FAcMmoClient::Start(const double Now, const FString& World)
{
	Stop();
	WorldFile = World;
	Interp.reset();
	Predictor.reset();
	LastBlend = 0;
	// -AcMmoLag=150,20,5: the setting at the start (the console changes it later).
	static bool bLagFlagRead = false;
	if (FString Flag; !bLagFlagRead && FParse::Value(FCommandLine::Get(), TEXT("AcMmoLag="), Flag, false))
	{
		CVarMmoLag->Set(*Flag, ECVF_SetByCommandline);
	}
	bLagFlagRead = true;
	ReadLag();
	Net.Reset();
	SeenMirror = nullptr;
	SeenGame = -1;
	WantedUnit.reset();
	bAutoTaken = false;
	HandBacks = 0;
	Takes = 0;
	bAdopt = true;
	AdoptOld.reset();
	Asked.Reset();
	BuildsAsked.Reset();
	BuildRefusals.Reset();
	FlightsAsked.Reset();
	LastFlightRefusal.Reset();
	bArriving = false;
	Notes.Reset();
	Events.clear();
	const FString Program = ServerProgram();
	if (!FPaths::FileExists(Program))
	{
		// The packaged game has no server yet (packaging is a later task).
		UE_LOG(LogAutocraft, Warning, TEXT("mmo: no server program at %s (build it: make -C unreal/server; packaged games do not carry it yet)"),
			*Program);
		Fail(FString::Printf(TEXT("The local server is not built. Build it with: make -C unreal/server\n(looked for %s)"), *Tilde(Program)));
		return false;
	}
	if (!FPaths::FileExists(WorldFile))
	{
		Fail(FString::Printf(TEXT("No world file at %s."), *Tilde(WorldFile)));
		return false;
	}
	// The one-time token: random, as text on the server's standard input.
	uint8_t Bytes[16];
	if (!ac::net::secureRandom(Bytes, sizeof Bytes))
	{
		Fail(TEXT("No random token for the local server."));
		return false;
	}
	Token.clear();
	static const char Hex[] = "0123456789abcdef";
	for (const uint8_t B : Bytes)
	{
		Token.push_back(Hex[B >> 4]);
		Token.push_back(Hex[B & 15]);
	}
	const std::vector<std::string> Args = {"--world", Utf8(WorldFile), "--bind", "127.0.0.1", "--port", "0", "--parent-pid", std::to_string(getpid())};
	if (!Server.start(Utf8(Program), Args, Token))
	{
		Fail(FString::Printf(TEXT("The local server did not start: %s.\nIts log: %s"), *Text(Server.error()), *Tilde(ServerLog())));
		return false;
	}
	UE_LOG(LogAutocraft, Log, TEXT("mmo: started %s on world %s (pid %d), log %s"), *Program, *Tilde(WorldFile), Server.pid(), *ServerLog());
	CurrentPhase = EPhase::Starting;
	StatusMessage = TEXT("Starting the local server…");
	StartedAt = Now;
	return true;
}

void FAcMmoClient::Stop()
{
	if (CurrentPhase == EPhase::Off) return;
	if (Net && Net->connected())
	{
		// The unit goes back to its AI; the server tells its other clients.
		if (Driven()) Net->handBack();
		Net->flush(LastNow);
	}
	const bool bRunning = Server.pid() > 0;
	const bool bClean = Server.stop(3.0);
	if (bRunning) UE_LOG(LogAutocraft, Log, TEXT("mmo: local server stopped (%s)"), bClean ? TEXT("clean") : *Text(Server.error()));
	Net.Reset();
	LagIo.Reset();
	Socket.close();
	Interp.reset();
	Predictor.reset();
	SeenMirror = nullptr;
	SeenGame = -1;
	WantedUnit.reset();
	bArriving = false;
	FlightsAsked.Reset();
	CurrentPhase = EPhase::Off;
	StatusMessage.Reset();
}

void FAcMmoClient::Connect(const double Now)
{
	if (!Socket.open(ac::net::Address::loopback(0)))
	{
		Fail(TEXT("No UDP socket for the connection."));
		return;
	}
	ac::net::ClientConfig Config;
	Config.server = ac::net::Address::loopback(Server.port());
	Config.token.assign(Token.begin(), Token.end());
	// The lag setting's simulator between them (none set: it passes all on at once).
	LagIo = MakeUnique<ac::net::LagSimulator>(Socket, uint64(FPlatformTime::Cycles64()));
	LagIo->setDefault(LagNow);
	Net = MakeUnique<ac::net::Client>(*LagIo, Config);
	Net->onSnapshot = [this](const ac::net::SnapshotPart& Part)
	{
		// The server's clock, and where everyone stood in this snapshot.
		Interp.heard(Part.snapshot.serverTick, LastNow);
		if (const ac::net::Mirror* M = Net->mirror()) Interp.record(M->serverTick(), M->simulation().state.units);
	};
	Net->onRegionChange = [this](const ac::net::RegionChange& Change) { OnRegionChange(Change); };
	Net->connect(Now);
	ConnectAt = Now;
	NextInputAt = 0;
	CurrentPhase = EPhase::Connecting;
	StatusMessage = FString::Printf(TEXT("Connecting to the local server (port %d)…"), (int32)Server.port());
	UE_LOG(LogAutocraft, Log, TEXT("mmo: server on port %d, connecting"), (int32)Server.port());
}

void FAcMmoClient::Tick(const double Now)
{
	LastNow = Now;
	if (CurrentPhase == EPhase::Off || CurrentPhase == EPhase::Failed) return;
	ReadLag();
	const ac::net::LocalServer::State State = Server.poll();
	if (State == ac::net::LocalServer::State::exited)
	{
		Fail(FString::Printf(TEXT("The local server stopped (%s).\nIts log: %s"), *Text(Server.error()), *Tilde(ServerLog())));
		return;
	}
	if (CurrentPhase == EPhase::Starting)
	{
		if (State == ac::net::LocalServer::State::running) Connect(Now);
		else if (Now - StartedAt > StartSeconds)
		{
			Fail(FString::Printf(TEXT("The local server did not give its port in %.0f s.\nIts log: %s"), StartSeconds, *Tilde(ServerLog())));
		}
		if (CurrentPhase != EPhase::Connecting) return;
	}
	if (TestReadEvery > 0 && Now - LastRead < TestReadEvery)
	{
		Net->flush(Now);
		return;
	}
	LastRead = Now;
	Net->receive(Now);
	if (!Net->disconnectReason().empty())
	{
		Fail(FString::Printf(TEXT("Lost the local server: %s.\nIts log: %s"), *Text(Net->disconnectReason()), *Tilde(ServerLog())));
		return;
	}
	const ac::net::Mirror* M = Net->mirror();
	if (!M || M->snapshotId() == 0)
	{
		// (A new region's mirror waits for its first snapshot: no deadline.)
		if (CurrentPhase != EPhase::Playing && Now - ConnectAt > StartSeconds)
		{
			Fail(FString::Printf(TEXT("The local server did not welcome us in %.0f s.\nIts log: %s"), StartSeconds, *Tilde(ServerLog())));
			return;
		}
	}
	else
	{
		if (!M->matches())
		{
			Fail(TEXT("The local server's map is not the one this game builds (another version?)."));
			return;
		}
		// (A RegionChange moved the generation already: its first snapshot
		// does not again.)
		if (M != SeenMirror || (SeenGame >= 0 && M->game() != SeenGame))
		{
			MirrorGeneration += 1;
			UE_LOG(LogAutocraft, Log, TEXT("mmo: welcomed on %s, game %lld, %d units in view"), *Text(M->region().mapName), (long long)M->game(),
				(int32)M->simulation().state.units.size());
		}
		SeenMirror = M;
		SeenGame = M->game();
		if (bArriving && M->driven())
		{
			bArriving = false;
			UE_LOG(LogAutocraft, Log, TEXT("mmo: arrived, driving #%lld"), (long long)*M->driven());
		}
		// In a world the server seats the pilot at the start and after a
		// flight (a RegionChange says so itself): that unit is the one
		// wanted. Only then: a snapshot sent before a hand back was read
		// still shows the unit driven, and must not take it again.
		if (bAdopt && Net->world() && M->driven() && M->driven() != AdoptOld)
		{
			bAdopt = false;
			WantedUnit = M->driven();
			bAutoTaken = true;
			UE_LOG(LogAutocraft, Log, TEXT("mmo: seated by the server in #%lld"), (long long)*M->driven());
		}
		if (CurrentPhase != EPhase::Playing)
		{
			CurrentPhase = EPhase::Playing;
			StatusMessage.Reset();
		}
		if (bAutoTake && !bAutoTaken && !M->driven())
		{
			// A Prospector of the pilot's side, the first the mirror holds.
			for (const ac::Unit& U : M->simulation().state.units)
			{
				if (U.owner == ac::Pilot::player && U.kind == ac::UnitKind::prospector && M->simulation().canDrive(U))
				{
					bAutoTaken = true;
					Want(U.id);
					break;
				}
			}
		}
	}
	// The driven unit put right by what the server said.
	if (ac::net::Mirror* Mm = Net->mirror()) Predictor.reconcile(*Mm);
	ReadReplies();
	for (ac::GameEvent& E : Net->events) Events.push_back(std::move(E));
	Net->events.clear();
	SendInputs(Now);
	Net->flush(Now);
	Blend(Now);
}

void FAcMmoClient::SendInputs(const double Now)
{
	if (!Net->welcome()) return;
	if (NextInputAt <= 0) NextInputAt = Now;
	int32 Sent = 0;
	while (Now >= NextInputAt)
	{
		ac::net::PilotInput& In = Net->steer;
		ac::net::Mirror* M = Net->mirror();
		ac::Simulation* S = M ? &M->simulation() : nullptr;
		if (S && S->pilot && WantedUnit && S->pilot->unit == *WantedUnit)
		{
			// What the pilot pawn wrote this frame, as it writes it in the RTS game.
			ac::Pilot& P = *S->pilot;
			In.walk = P.walk;
			In.heading = P.heading;
			In.act = P.act;
			In.ability = P.ability;
			In.hold = P.hold;
			In.mend = P.mend;
			In.crosshair = P.crosshair;
			// Presses go once.
			P.act = false;
			P.ability = false;
		}
		else
		{
			// Not steering: stand, facing where it faces.
			const ac::Unit* U = M && M->driven() ? M->unit(*M->driven()) : nullptr;
			In = ac::net::PilotInput();
			In.heading = U ? U->heading : 0.0;
		}
		// Applied to the driven unit at once (it is drawn in the present).
		if (const std::optional<ac::net::PilotInput> Out = Net->sendInput(); Out && M) Predictor.input(*M, *Out);
		NextInputAt += 1.0 / InputHz;
		if (++Sent >= 6)
		{
			// A long frame: the next tick from now, not a burst to catch up.
			NextInputAt = Now + 1.0 / InputHz;
			break;
		}
	}
}

void FAcMmoClient::Blend(const double Now)
{
	ac::net::Mirror* M = Mirror();
	const double Dt = LastBlend > 0 ? FMath::Clamp(Now - LastBlend, 0.0, 0.25) : 0.0;
	LastBlend = Now;
	if (!M || M->snapshotId() == 0) return;
	if (const std::optional<double> At = Interp.renderTick(Now))
	{
		Interp.apply(*At, M->simulation().state.units, M->driven());
	}
	// The driven one in the present, a correction eased over a few frames.
	Predictor.ease(Dt);
	Predictor.draw(*M);
}

bool FAcMmoClient::ParseLag(const FString& Text, ac::net::LagSettings& Out)
{
	Out = ac::net::LagSettings();
	FString Clean = Text.TrimStartAndEnd().Replace(TEXT(","), TEXT(" "));
	if (Clean.IsEmpty() || Clean.Equals(TEXT("off"), ESearchCase::IgnoreCase)) return true;
	TArray<FString> Parts;
	Clean.ParseIntoArrayWS(Parts);
	if (Parts.Num() > 3) return false;
	double Values[3] = {0, 0, 0};
	for (int32 I = 0; I < Parts.Num(); ++I)
	{
		if (!Parts[I].IsNumeric()) return false;
		Values[I] = FCString::Atod(*Parts[I]);
		if (!(Values[I] >= 0)) return false;
	}
	Out.delay = Values[0] / 1000.0;
	Out.jitter = Values[1] / 1000.0;
	Out.loss = FMath::Min(Values[2], 100.0) / 100.0;
	return true;
}

void FAcMmoClient::ReadLag()
{
	const FString Setting = CVarMmoLag.GetValueOnGameThread();
	if (Setting == LagText) return;
	LagText = Setting;
	ac::net::LagSettings Next;
	if (!ParseLag(Setting, Next))
	{
		Notes.Add(FString::Printf(TEXT("ac.MmoLag: \"%s\" does not read (delay_ms jitter_ms loss_pct)"), *Setting));
		return;
	}
	LagNow = Next;
	if (LagIo) LagIo->setDefault(LagNow);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: lag %.0f ms each way, up to %.0f ms more, %.0f%% lost"), LagNow.delay * 1000,
		LagNow.jitter * 1000, LagNow.loss * 100);
}

std::optional<double> FAcMmoClient::RoundTrip() const
{
	const ac::net::ConnectionStats* Stats = Net ? Net->endpoint().stats() : nullptr;
	if (!Stats || !(Stats->rtt > 0)) return std::nullopt;
	return Stats->rtt;
}

void FAcMmoClient::ReadReplies()
{
	for (auto It = Net->replies.begin(); It != Net->replies.end();)
	{
		const ac::net::ReplyMessage& R = It->second;
		if (BuildsAsked.Remove(R.request) > 0)
		{
			if (!R.done)
			{
				BuildRefusals.Add(Text(R.reason));
				UE_LOG(LogAutocraft, Log, TEXT("mmo: build %u refused: %s"), R.request, *Text(R.reason));
			}
			It = Net->replies.erase(It);
			continue;
		}
		if (FlightsAsked.Remove(R.request) > 0)
		{
			if (!R.done)
			{
				bAdopt = false;
				LastFlightRefusal = Text(R.reason);
				Notes.Add(FString::Printf(TEXT("Not flown: %s"), *LastFlightRefusal));
				UE_LOG(LogAutocraft, Log, TEXT("mmo: flight %u refused: %s"), R.request, *LastFlightRefusal);
			}
			It = Net->replies.erase(It);
			continue;
		}
		const std::optional<int64>* What = Asked.Find(R.request);
		if (!R.done)
		{
			const bool bTake = What && What->has_value();
			Notes.Add(FString::Printf(TEXT("%s: %s"), bTake ? TEXT("Not taken") : TEXT("Not handed back"), *Text(R.reason)));
			UE_LOG(LogAutocraft, Log, TEXT("mmo: request %u refused: %s"), R.request, *Text(R.reason));
			// The take was refused: no longer wanted.
			if (bTake && WantedUnit == **What) WantedUnit.reset();
		}
		Asked.Remove(R.request);
		It = Net->replies.erase(It);
	}
}

void FAcMmoClient::Want(const std::optional<int64> Unit)
{
	if (!Net || !Net->welcome())
	{
		Notes.Add(TEXT("Not connected to the local server yet"));
		return;
	}
	if (Unit == WantedUnit) return;
	WantedUnit = Unit;
	// The player's choice from now on.
	bAdopt = false;
	// Nothing to hand back: the server took it already (it died, say).
	if (!Unit && !Driven()) return;
	const uint32 Id = Unit ? Net->take(*Unit) : Net->handBack();
	if (!Unit) HandBacks += 1;
	else Takes += 1;
	Asked.Add(Id, Unit);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: request %u: %s"), Id, Unit ? *FString::Printf(TEXT("take #%lld"), (long long)*Unit) : TEXT("hand back"));
}

std::optional<int64> FAcMmoClient::Driven() const
{
	const ac::net::Mirror* M = Mirror();
	return M ? M->driven() : std::nullopt;
}

std::vector<ac::GameEvent> FAcMmoClient::TakeEvents()
{
	std::vector<ac::GameEvent> Out = std::move(Events);
	Events.clear();
	return Out;
}

bool FAcMmoClient::Build(const ac::StructureKind Kind, const ac::Vec2 At)
{
	if (!Net || !Net->welcome()) return false;
	const uint32 Id = Net->build(Kind, At);
	BuildsAsked.Add(Id);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: build %u asked: %s at %.2f,%.2f"), Id, *Text(std::string(ac::rawValue(Kind))), At.x, At.y);
	return true;
}

TArray<FString> FAcMmoClient::TakeBuildRefusals()
{
	return MoveTemp(BuildRefusals);
}

TArray<FString> FAcMmoClient::TakeNotes()
{
	TArray<FString> Out = MoveTemp(Notes);
	Notes.Reset();
	return Out;
}

// MARK: - The world

void FAcMmoClient::OnRegionChange(const ac::net::RegionChange& Change)
{
	// A new region: what was blended and predicted was the old one's.
	Interp.reset();
	Predictor.reset();
	LastBlend = 0;
	WantedUnit = Change.driven;
	bAutoTaken = true;
	bAdopt = false;
	bArriving = true;
	// The old mirror is gone: the game rebuilds on this one now, before its
	// first snapshot (its map is built; its units come with the snapshots).
	SeenMirror = Net->mirror();
	SeenGame = -1;
	MirrorGeneration += 1;
	UE_LOG(LogAutocraft, Log, TEXT("mmo: region change %lld: into %s (%s), driving #%lld"), (long long)Change.visit, *Text(Change.regionId),
		*Text(Change.region.mapName), (long long)Change.driven.value_or(-1));
}

const ac::net::WorldMessage* FAcMmoClient::World() const
{
	return Net && Net->world() ? &*Net->world() : nullptr;
}

const ac::net::WorldMessage::Region* FAcMmoClient::Here() const
{
	const ac::net::WorldMessage* W = World();
	if (!W || W->here < 0 || W->here >= (int64)W->regions.size()) return nullptr;
	return &W->regions[size_t(W->here)];
}

const ac::net::WorldMessage::Region* FAcMmoClient::FlyingTo() const
{
	const ac::net::WorldMessage* W = World();
	if (!W || !W->flyingTo || *W->flyingTo < 0 || *W->flyingTo >= (int64)W->regions.size()) return nullptr;
	return &W->regions[size_t(*W->flyingTo)];
}

std::optional<FAcMmoClient::FPadNear> FAcMmoClient::NearestPad() const
{
	const ac::net::WorldMessage::Region* R = Here();
	const ac::net::Mirror* M = Mirror();
	if (!R || !M || !M->driven() || Travelling()) return std::nullopt;
	const ac::Unit* U = M->unit(*M->driven());
	if (!U) return std::nullopt;
	// The server's rule: pads on this map's ground, the nearest centre.
	const std::optional<ac::region::PadNear> Near =
		ac::region::nearestPad(ac::net::padsOnGround(*R, M->simulation().map->bounds), U->position);
	if (!Near) return std::nullopt;
	return FPadNear{Text(Near->name), Near->distance};
}

bool FAcMmoClient::Fly(const FString& Region, const FString& Pad, FString* Why)
{
	auto No = [&](const FString& Reason) {
		if (Why) *Why = Reason;
		UE_LOG(LogAutocraft, Log, TEXT("mmo: no flight to %s %s: %s"), *Region, *Pad, *Reason);
		return false;
	};
	if (!Net || !Net->welcome() || !World()) return No(TEXT("Not connected to the world yet"));
	if (FlyingTo()) return No(FString::Printf(TEXT("Already flying to %s"), *Text(FlyingTo()->name)));
	if (Travelling()) return No(TEXT("Still landing"));
	if (!FlightsAsked.IsEmpty()) return No(TEXT("A flight is already asked for"));
	const std::optional<FPadNear> Near = NearestPad();
	if (!Near) return No(TEXT("Nothing driven to fly"));
	if (Near->Distance > PadReach)
	{
		return No(FString::Printf(TEXT("Drive onto a pad to fly: pad %s is %.0f cells away"), *Near->Name, Near->Distance));
	}
	LastFlightRefusal.Reset();
	const uint32 Id = Net->fly(Utf8(Region), Utf8(Pad));
	FlightsAsked.Add(Id);
	// A landing in this region brings a new unit with no RegionChange.
	bAdopt = true;
	AdoptOld = Driven();
	UE_LOG(LogAutocraft, Log, TEXT("mmo: request %u: fly from pad %s to %s %s"), Id, *Near->Name, *Region, *Pad);
	return true;
}
