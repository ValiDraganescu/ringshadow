#include "AcMmo.h"

#include "AcLog.h"

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformProperties.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Progression.h"
#include "Rules.h"

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

FString FAcMmoClient::BundledServerDir()
{
#if PLATFORM_MAC
	// A packaged Mac game: build_release.sh mac stages the server beside the
	// app's resources (docs/builds.md, "The server in the Mac package").
	if (FPlatformProperties::RequiresCookedData())
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::GetPath(FPlatformProcess::ExecutablePath()), TEXT(".."), TEXT("Resources"), TEXT("server")));
	}
#endif
	return FString();
}

FString FAcMmoClient::ServerProgram()
{
	if (const FString Bundled = BundledServerDir(); !Bundled.IsEmpty()) return FPaths::Combine(Bundled, TEXT("autocraft-server"));
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("server"), TEXT("build"), TEXT("autocraft-server")));
}

FString FAcMmoClient::ServerLog()
{
	return FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Library"), TEXT("Logs"), TEXT("Autocraft"), TEXT("server.log"));
}

FString FAcMmoClient::DefaultWorld()
{
	if (const FString Bundled = BundledServerDir(); !Bundled.IsEmpty()) return FPaths::Combine(Bundled, TEXT("worlds"), TEXT("first.json"));
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("server"), TEXT("worlds"), TEXT("first.json")));
}

FString FAcMmoClient::Directory()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FString Dir; FParse::Value(Cmd, TEXT("AcMmoDir="), Dir) && !Dir.IsEmpty())
	{
		return FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
	}
	if (FString Dir; FParse::Value(Cmd, TEXT("AcSaveDir="), Dir) && !Dir.IsEmpty())
	{
		return FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir), TEXT("mmo"));
	}
	if (FString Dir; FParse::Value(Cmd, TEXT("AcMmoShots="), Dir) && !Dir.IsEmpty())
	{
		// The shot harness (AcMmoShots.cpp): its own pilot and world.
		return FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir), TEXT("mmo"));
	}
	if (FString Steps; FParse::Value(Cmd, TEXT("AcFlowTest="), Steps))
	{
		// A test never touches the player's pilot or world.
		return FPaths::Combine(FPlatformProcess::UserTempDir(), TEXT("autocraft-test-mmo"));
	}
	return FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Library/Application Support/Autocraft/Unreal/mmo"));
}

FString FAcMmoClient::PilotKeyFile()
{
	return FPaths::Combine(Directory(), TEXT("pilot.key"));
}

FString FAcMmoClient::SaveFile(const FString& World)
{
	// The world's id is its file's name (first.json: "first"); the server
	// refuses another world's save.
	return FPaths::Combine(Directory(), FPaths::GetBaseFilename(World) + TEXT(".sqlite"));
}

std::optional<ac::net::PilotKey> FAcMmoClient::LoadPilotKey(const FString& File, FString* Why)
{
	auto No = [&](const FString& Reason) -> std::optional<ac::net::PilotKey>
	{
		if (Why) *Why = Reason;
		UE_LOG(LogAutocraft, Warning, TEXT("mmo: %s"), *Reason);
		return std::nullopt;
	};
	IFileManager& Files = IFileManager::Get();
	if (Files.FileExists(*File))
	{
		FString Hex;
		if (!FFileHelper::LoadFileToString(Hex, *File)) return No(FString::Printf(TEXT("The pilot key at %s does not read."), *Tilde(File)));
		const std::optional<ac::net::PilotKey> Key = ac::net::pilotKeyFromHex(Utf8(Hex.TrimStartAndEnd()));
		if (!Key) return No(FString::Printf(TEXT("The pilot key at %s is not 32 hex digits."), *Tilde(File)));
		UE_LOG(LogAutocraft, Log, TEXT("mmo: pilot %s (kept in %s)"), *Text(ac::net::pilotKeyShort(*Key)), *Tilde(File));
		return Key;
	}
	uint8_t Bytes[16];
	if (!ac::net::secureRandom(Bytes, sizeof Bytes)) return No(TEXT("No random source for a pilot key."));
	ac::net::PilotKey Key{};
	for (size_t I = 0; I < Key.size(); ++I) Key[I] = Bytes[I];
	Files.MakeDirectory(*FPaths::GetPath(File), true);
	if (!FFileHelper::SaveStringToFile(Text(ac::net::pilotKeyHex(Key)) + TEXT("\n"), *File))
	{
		return No(FString::Printf(TEXT("The pilot key could not be written to %s."), *Tilde(File)));
	}
	UE_LOG(LogAutocraft, Log, TEXT("mmo: new pilot %s (kept in %s)"), *Text(ac::net::pilotKeyShort(Key)), *Tilde(File));
	return Key;
}

bool FAcMmoClient::DeleteSave(const FString& File, FString* Why)
{
	IFileManager& Files = IFileManager::Get();
	bool bAll = true;
	for (const FString& Path : {File, File + TEXT("-wal"), File + TEXT("-shm")})
	{
		if (!Files.FileExists(*Path)) continue;
		if (Files.Delete(*Path, false, true, true)) UE_LOG(LogAutocraft, Log, TEXT("mmo: deleted %s"), *Tilde(Path));
		if (Files.FileExists(*Path))
		{
			bAll = false;
			if (Why) *Why = FString::Printf(TEXT("Could not delete %s."), *Tilde(Path));
			UE_LOG(LogAutocraft, Warning, TEXT("mmo: could not delete %s"), *Path);
		}
	}
	return bAll;
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
	PicksAsked.Reset();
	LastPickRefusal.Reset();
	PicksDone = 0;
	SkillsAsked.Reset();
	LastSkillRefusal.Reset();
	SkillsDone = 0;
	RigAsked.Reset();
	LastRigRefusal.Reset();
	FlightsAsked.Reset();
	LastFlightRefusal.Reset();
	bArriving = false;
	Notes.Reset();
	Events.clear();
	const FString Program = ServerProgram();
	if (!FPaths::FileExists(Program))
	{
		if (!BundledServerDir().IsEmpty())
		{
			// A packaged game: build_release.sh mac stages it; a Windows or Linux package has none yet.
			UE_LOG(LogAutocraft, Warning, TEXT("mmo: no server program at %s (the package does not carry it)"), *Program);
			Fail(FString::Printf(TEXT("The local server is missing from this package.\n(looked for %s)"), *Tilde(Program)));
			return false;
		}
		UE_LOG(LogAutocraft, Warning, TEXT("mmo: no server program at %s (build it: make -C unreal/server)"), *Program);
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
	std::vector<std::string> Args = {"--world", Utf8(WorldFile), "--bind", "127.0.0.1", "--port", "0", "--parent-pid", std::to_string(getpid())};
	// Dev: XP for a new pilot's Prospector, on the server (as -AcEarn).
	if (double DevXp = 0; FParse::Value(FCommandLine::Get(), TEXT("AcMmoDevXp="), DevXp) && DevXp > 0)
	{
		Args.push_back("--dev-xp");
		Args.push_back(std::to_string(DevXp));
	}
	// Dev, tests and shots: a new pilot has every blueprint (it builds at once).
	if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevBlueprints"))) Args.insert(Args.end(), {"--dev-blueprints", "1"});
	// Dev and shots: a new pilot owns a finished Citadel, Foundry and Lab, its rig stocked.
	if (FParse::Param(FCommandLine::Get(), TEXT("AcMmoDevRig"))) Args.insert(Args.end(), {"--dev-rig", "1"});
	// Dev and shots: the world's hour and weather held on the server.
	if (double Hour = -1; FParse::Value(FCommandLine::Get(), TEXT("AcMmoHour="), Hour) && Hour >= 0 && Hour <= 24)
	{
		Args.push_back("--hour");
		Args.push_back(std::to_string(Hour));
	}
	// Dev and shots: a ringshadow sweep this many seconds after the server starts.
	if (double In = -1; FParse::Value(FCommandLine::Get(), TEXT("AcMmoRingshadow="), In) && In >= 0)
	{
		Args.push_back("--ringshadow");
		Args.push_back(std::to_string(In));
	}
	// Dev, tests and shots: a starfall near the pilot this many seconds after
	// it comes (docs/weather.md "Starfall"), and the world's clock moved on to
	// an hour (it runs on from there).
	if (double In = -1; FParse::Value(FCommandLine::Get(), TEXT("AcMmoStarfall="), In) && In >= 0)
	{
		Args.push_back("--starfall");
		Args.push_back(std::to_string(In));
	}
	if (double Hour = -1; FParse::Value(FCommandLine::Get(), TEXT("AcMmoClockHour="), Hour) && Hour >= 0 && Hour <= 24)
	{
		Args.push_back("--clock-hour");
		Args.push_back(std::to_string(Hour));
	}
	if (FString Weather; FParse::Value(FCommandLine::Get(), TEXT("AcMmoWeather="), Weather) && !Weather.IsEmpty())
	{
		Args.push_back("--weather");
		Args.push_back(Utf8(Weather.ToLower()));
	}
	if (!SavePath.IsEmpty())
	{
		Args.push_back("--save");
		Args.push_back(Utf8(SavePath));
	}
	Args.insert(Args.end(), ExtraServerArgs.begin(), ExtraServerArgs.end());
	if (!Server.start(Utf8(Program), Args, Token))
	{
		Fail(FString::Printf(TEXT("The local server did not start: %s.\nIts log: %s"), *Text(Server.error()), *Tilde(ServerLog())));
		return false;
	}
	UE_LOG(LogAutocraft, Log, TEXT("mmo: started %s on world %s (pid %d), save %s, log %s"), *Program, *Tilde(WorldFile), Server.pid(),
		SavePath.IsEmpty() ? TEXT("none") : *Tilde(SavePath), *ServerLog());
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
	SkyNow.reset();
	SkyMirror = nullptr;
	SkySnapshot = -1;
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
	// Who the pilot is (the server seats it only after the Join): the key
	// set, or a new one each start (a new pilot at the start pad).
	const ac::net::PilotKey Key = PilotKey ? *PilotKey : ac::net::newPilotKey();
	Net->join(Key);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: join as pilot %s%s"), *Text(ac::net::pilotKeyShort(Key)), PilotKey ? TEXT("") : TEXT(" (a new one)"));
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
		// Before its port: it did not start (a save that does not open says why).
		const FString Why = Text(Server.error());
		Fail(CurrentPhase == EPhase::Starting ? FString::Printf(TEXT("The local server did not start: %s.\nIts log: %s"), *Why, *Tilde(ServerLog()))
		                                      : FString::Printf(TEXT("The local server stopped (%s).\nIts log: %s"), *Why, *Tilde(ServerLog())));
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
			In.slot = P.slot;
			// Presses go once.
			P.act = false;
			P.ability = false;
			P.slot = 0;
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
	FollowSky(*M, Now, Dt);
}

void FAcMmoClient::FollowSky(ac::net::Mirror& M, const double Now, const double Dt)
{
	const std::optional<ac::net::Sky>& Told = M.sky();
	if (!Told)
	{
		SkyNow.reset();
		return;
	}
	if (&M != SkyMirror || M.snapshotId() != SkySnapshot)
	{
		SkyMirror = &M;
		SkySnapshot = M.snapshotId();
		SkyAnchor = Told->time;
		SkyAnchorAt = Now;
	}
	// Where the server's clock is now, as far as the snapshots tell.
	const double Target = SkyAnchor + (Now - SkyAnchorAt);
	if (!SkyNow || FMath::Abs(Target - SkyNow->time) > 5.0)
	{
		if (!SkyNow) UE_LOG(LogAutocraft, Log, TEXT("mmo: the world's sky: hour %.2f%s, day %.0f s, weather seed %u%s"), Told->hour(),
			Told->heldHour ? TEXT(" (held)") : TEXT(""), Told->dayLength, Told->weatherSeed, Told->weatherHold ? TEXT(" (held)") : TEXT(""));
		SkyNow = *Told;
		SkyNow->time = Target;
	}
	else
	{
		// On with the frame, and a share of the way to the server's.
		const double T = SkyNow->time + Dt;
		SkyNow = *Told;
		SkyNow->time = T + (Target - T) * FMath::Min(1.0, 2.0 * Dt);
	}
	// The mirror's sight at night: the hour drawn.
	M.simulation().state.dayLength = SkyNow->dayLength;
	M.simulation().state.heldHour = SkyNow->hour();
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
		if (PicksAsked.Remove(R.request) > 0)
		{
			if (R.done)
			{
				PicksDone += 1;
				UE_LOG(LogAutocraft, Log, TEXT("mmo: pick %u kept"), R.request);
			}
			else
			{
				LastPickRefusal = Text(R.reason);
				Notes.Add(FString::Printf(TEXT("Not picked: %s"), *LastPickRefusal));
				UE_LOG(LogAutocraft, Log, TEXT("mmo: pick %u refused: %s"), R.request, *LastPickRefusal);
			}
			It = Net->replies.erase(It);
			continue;
		}
		if (SkillsAsked.Remove(R.request) > 0)
		{
			if (R.done)
			{
				SkillsDone += 1;
			}
			else
			{
				LastSkillRefusal = Text(R.reason);
				Notes.Add(LastSkillRefusal);
				UE_LOG(LogAutocraft, Log, TEXT("mmo: skill ask %u refused: %s"), R.request, *LastSkillRefusal);
			}
			It = Net->replies.erase(It);
			continue;
		}
		if (RigAsked.Remove(R.request) > 0)
		{
			if (!R.done)
			{
				LastRigRefusal = Text(R.reason);
				Notes.Add(LastRigRefusal);
				UE_LOG(LogAutocraft, Log, TEXT("mmo: rig ask %u refused: %s"), R.request, *LastRigRefusal);
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

bool FAcMmoClient::Pick(const ac::UnitKind Kind, const ac::Perk Perk, FString* Why)
{
	auto No = [&](const FString& Reason)
	{
		if (Why) *Why = Reason;
		return false;
	};
	const ac::net::Mirror* M = Mirror();
	if (!Net || !Net->welcome() || !M) return No(TEXT("Not connected to the local server"));
	if (PickInFlight()) return No(TEXT("A pick is on its way"));
	if (ac::kind(Perk) != Kind) return No(TEXT("Not a card of that kind"));
	if (const std::optional<std::string> Refused = M->simulation().levels().refusal(Perk)) return No(Text(*Refused));
	LastPickRefusal.Reset();
	const uint32 Id = Net->pick(Kind, Perk);
	PicksAsked.Add(Id);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: pick %u asked: %s for %s"), Id, *Text(std::string(ac::rawValue(Perk))),
		*Text(std::string(ac::rawValue(Kind))));
	return true;
}

bool FAcMmoClient::CanAskSkills(FString* Why) const
{
	auto No = [&](const TCHAR* Reason)
	{
		if (Why) *Why = Reason;
		return false;
	};
	if (!Net || !Net->welcome()) return No(TEXT("Not connected to the local server"));
	if (!Progress()) return No(TEXT("No skill tree here: not a world"));
	return true;
}

void FAcMmoClient::AskedSkill(const uint32 Id, const FString& What)
{
	LastSkillRefusal.Reset();
	SkillsAsked.Add(Id);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: skill ask %u: %s"), Id, *What);
}

bool FAcMmoClient::Spend(const ac::Skill Skill, FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	AskedSkill(Net->spend(Skill), FString::Printf(TEXT("spend %s"), *Text(std::string(ac::rawValue(Skill)))));
	return true;
}

bool FAcMmoClient::Undo(const ac::Skill Skill, FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	AskedSkill(Net->undo(Skill), FString::Printf(TEXT("undo %s"), *Text(std::string(ac::rawValue(Skill)))));
	return true;
}

bool FAcMmoClient::Respec(FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	AskedSkill(Net->respec(), TEXT("respec"));
	return true;
}

bool FAcMmoClient::Abandon(const uint32 Id, FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	AskedSkill(Net->abandon(Id), FString::Printf(TEXT("abandon contract %u"), Id));
	return true;
}

bool FAcMmoClient::Slot(const int32 Index, const std::optional<ac::Skill> Skill, FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	AskedSkill(Net->slot(Index, Skill), FString::Printf(TEXT("slot %d: %s"), Index,
		Skill ? *Text(std::string(ac::rawValue(*Skill))) : TEXT("empty")));
	return true;
}

bool FAcMmoClient::AskRig(const TCHAR* Verb, const ac::Module Module, const int64 Mark, FString* Why)
{
	if (!CanAskSkills(Why)) return false;
	const uint32 Id = FCString::Strcmp(Verb, TEXT("make")) == 0 ? Net->make(Module, Mark)
		: FCString::Strcmp(Verb, TEXT("research")) == 0 ? Net->research(Module, Mark) : Net->fit(Module, Mark);
	LastRigRefusal.Reset();
	RigAsked.Add(Id);
	UE_LOG(LogAutocraft, Log, TEXT("mmo: rig ask %u: %s %s Mk %lld"), Id, Verb, *Text(std::string(ac::rawValue(Module))), (long long)Mark);
	return true;
}

bool FAcMmoClient::Make(const ac::Module Module, const int64 Mark, FString* Why) { return AskRig(TEXT("make"), Module, Mark, Why); }

bool FAcMmoClient::Research(const ac::Module Module, const int64 Mark, FString* Why) { return AskRig(TEXT("research"), Module, Mark, Why); }

bool FAcMmoClient::Fit(const ac::Module Module, const int64 Mark, FString* Why) { return AskRig(TEXT("fit"), Module, Mark, Why); }

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

const ac::net::ProgressMessage* FAcMmoClient::Progress() const
{
	return Net && Net->progress() ? &*Net->progress() : nullptr;
}

std::optional<double> FAcMmoClient::Xp() const
{
	if (Net && Net->mirror() && Net->mirror()->xp()) return Net->mirror()->xp();
	if (const ac::net::ProgressMessage* P = Progress()) return P->xp;
	return std::nullopt;
}

std::vector<ac::net::ProgressMessage::News> FAcMmoClient::TakeProgressNews()
{
	std::vector<ac::net::ProgressMessage::News> Out;
	if (Net) Out.swap(Net->news);
	return Out;
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
	// The server's rule: pads on this map's ground within Pad link's reach;
	// the pilot's own within its building's radius + 2 too (P6).
	const std::vector<ac::region::Place> Ground = ac::net::padsOnGround(*R, M->simulation().map->bounds);
	const double Reach = PadReachNow();
	std::optional<FPadNear> Best;
	for (size_t I = 0; I < Ground.size() && I < R->pads.size(); ++I)
	{
		FPadNear P{Text(Ground[I].name), ac::distance(Ground[I].at, U->position), Reach, R->pads[I].building};
		if (P.Building) P.Reach = FMath::Max(Reach, ac::Rules::radius(*P.Building) + 2.0);
		// One it can fly from first, then the nearest.
		if (!Best || (P.Within() && !Best->Within()) || (P.Within() == Best->Within() && P.Distance < Best->Distance)) Best = P;
	}
	return Best;
}

double FAcMmoClient::PadReachNow() const
{
	double Reach = PadReach;
	if (const ac::net::ProgressMessage* P = Progress())
	{
		const auto It = P->ranks.find(ac::Skill::padLink);
		for (const ac::SkillEffect& E : ac::Progression::effects(ac::Skill::padLink, It == P->ranks.end() ? 0 : It->second))
		{
			if (const auto* L = E.as<ac::SkillEffect::PadLink>()) Reach = FMath::Max(Reach, L->radius);
		}
	}
	return Reach;
}

std::optional<double> FAcMmoClient::RingwalkerIn() const
{
	const ac::net::ProgressMessage* P = Progress();
	if (!P) return std::nullopt;
	const auto It = P->ranks.find(ac::Skill::ringwalker);
	if (It == P->ranks.end() || It->second <= 0) return std::nullopt;
	// World time; the message comes only on a change, so against the sky's clock.
	const double Now = SkyNow ? SkyNow->time : 0;
	return P->ringwalkerReady > Now ? P->ringwalkerReady - Now : 0.0;
}

std::optional<std::string> FAcMmoClient::PadPlateRefusal(const ac::StructureKind Kind, const ac::Vec2 At) const
{
	const ac::net::WorldMessage::Region* R = Here();
	const ac::net::Mirror* M = Mirror();
	if (!R || !M || !M->simulation().map) return std::nullopt;
	if (!ac::region::touchesPadPlate(ac::net::padsOnGround(*R, M->simulation().map->bounds), At, ac::Rules::radius(Kind)))
	{
		return std::nullopt;
	}
	return std::string(ac::region::padPlateRefusal);
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
	// Ringwalker: from anywhere once its cooldown is over (the server decides).
	const std::optional<double> Ring = RingwalkerIn();
	if (!Near->Within() && !(Ring && *Ring <= 0))
	{
		if (Ring) return No(FString::Printf(TEXT("Ringwalker is ready in %.0f s; or drive onto a pad: %s is %.0f cells away"), std::ceil(*Ring), *Near->Name, Near->Distance));
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
