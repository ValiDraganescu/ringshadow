// `FAcMmoClient`: the game's end of the MMORPG test mode (docs/mmorpg-m1.md,
// T5; docs/networking.md, "The local server", "The client"). It starts
// `autocraft-server` (unreal/server, built by `make -C unreal/server`) as a
// hidden child on the world file (docs/worlds.md; `--world`, the first
// world by default), hands it a one-time token on its
// standard input, reads its port, connects to it on 127.0.0.1 through
// AutocraftNet (`net::Client` on a `UdpSocket`) and keeps the mirror of the
// region (`net::Mirror`) that `UAcSimSubsystem` hands the game in place of
// its own simulation. Each frame (`Tick`) it reads what came, sends the
// pilot's input at 60 Hz (from the mirror's `pilot`, which the pilot pawn
// writes as it does in the RTS game), and draws every unit but the driven
// one 100 ms in the past (`net::Interpolator`). The driven one is drawn in
// the present: each input moves it at once and each snapshot puts it right
// (`net::Predictor`).
//
// The world (docs/mmorpg-m2.md): the server seats the pilot in a Prospector
// at the start pad. Across an edge or after a flight a RegionChange brings a
// new mirror: the prediction and the blending start over and `Generation`
// moves at once, so the game rebuilds on the new region (the old mirror is
// gone). `Travelling` holds from a flight's lift-off (or a RegionChange)
// until the new region's first snapshot shows the unit driven there.
//
// Leveling (docs/mmorpg-m3.md, S3): the pilot's levels come with the
// snapshots (its side's `Player::pilot` in the mirror is its own record), so
// the HUD's level, XP bar and cards read the mirror as in the RTS game. A
// card picked is a Pick request (`Pick`); the server applies it and the next
// snapshot shows it. `-AcMmoDevXp=XP` (dev, as -AcEarn): the server gives a
// new pilot's Prospector that much XP when it seats it (`--dev-xp`).
//
// Lag to feel it (on 127.0.0.1 there is none): the console variable
// `ac.MmoLag "delay_ms jitter_ms loss_pct"` (each way; 0 or empty: off) or
// the flag `-AcMmoLag=150,20,5`, which sets it at the start. The lag
// simulator sits on the client's end and delays both ways.
//
// No world and no engine objects: the automation test (AcMmoTests.cpp)
// drives it as the subsystem does.
//
// Packaged games: the server is not packaged yet (only the editor's run
// finds it, under the repo); the start says so in the log.
#pragma once

#include "CoreMinimal.h"

#include "NetClient.h"
#include "NetInterpolator.h"
#include "NetLagSimulator.h"
#include "NetLocalServer.h"
#include "NetPredictor.h"
#include "NetSocket.h"
#include "Types.h"

#include <memory>
#include <optional>
#include <vector>

class AUTOCRAFT_API FAcMmoClient
{
public:
	enum class EPhase : uint8
	{
		/// Not started, or stopped.
		Off,
		/// The server process is starting (waiting for its port).
		Starting,
		/// Connecting, or connected and waiting for the Welcome's first units.
		Connecting,
		/// The mirror is up.
		Playing,
		/// It did not start, or it stopped: `Message()` says why.
		Failed,
	};

	/// The client ticks per second (the server steps at 60 Hz too).
	static constexpr double InputHz = 60.0;
	/// Seconds the server may take to print its port, and to welcome us.
	static constexpr double StartSeconds = 10.0;

	FAcMmoClient();
	~FAcMmoClient();

	/// `autocraft-server` in the repo: `unreal/server/build/autocraft-server`.
	static FString ServerProgram();
	/// The server's log: `~/Library/Logs/Autocraft/server.log`.
	static FString ServerLog();
	/// The world the game plays: `unreal/server/worlds/first.json`.
	static FString DefaultWorld();

	/// Starts the server on the world file and connects to it. `Now`:
	/// seconds on any steady clock (the same one `Tick` gets). False
	/// (`Failed`, with the message) when it does not start.
	bool Start(double Now, const FString& World = DefaultWorld());
	/// Hands the driven unit back, asks the server to stop (SIGTERM) and
	/// waits for it.
	void Stop();

	/// One frame: reads, sends the inputs due at 60 Hz, puts it on the wire,
	/// and blends the others into the mirror's state for the moment drawn.
	void Tick(double Now);

	EPhase Phase() const { return CurrentPhase; }
	/// What to show: where it is ("Starting the local server…"), or why it
	/// failed (with the server log's path).
	const FString& Message() const { return StatusMessage; }

	/// The region's mirror (null until the Welcome).
	ac::net::Mirror* Mirror() { return Net ? Net->mirror() : nullptr; }
	const ac::net::Mirror* Mirror() const { return Net ? Net->mirror() : nullptr; }
	/// Changes each time a new mirror is made (a Welcome) or its game
	/// changes: the game rebuilds what it draws.
	int64 Generation() const { return MirrorGeneration; }

	/// The events received since the last call (fog-gated by the server).
	std::vector<ac::GameEvent> TakeEvents();
	/// Refusals and notes for the player since the last call ("that unit
	/// cannot be driven").
	TArray<FString> TakeNotes();

	/// Build a `Kind` at `At` with the driven Prospector (a Build request),
	/// paid from the wallet; the scaffold comes with the snapshots. False:
	/// not connected. The server's refusals: `TakeBuildRefusals`.
	bool Build(ac::StructureKind Kind, ac::Vec2 At);
	/// The server's reasons for builds refused since the last call ("Not
	/// enough ore"), for the pilot's screen as the game's own refusals.
	TArray<FString> TakeBuildRefusals();

	/// Pick a level-up card: `Perk` for `Kind` (a Pick request); the server
	/// keeps it and the snapshots show it. False, with `Why`: not connected,
	/// a pick on its way already, or not on offer in the mirror. The
	/// server's refusals come as notes ("Not picked: …") and `PickRefusal`.
	bool Pick(ac::UnitKind Kind, ac::Perk Perk, FString* Why = nullptr);
	/// A pick asked for and not answered yet.
	bool PickInFlight() const { return PicksAsked.Num() > 0; }
	/// The server's last refusal of a pick (empty: none since the last ask).
	const FString& PickRefusal() const { return LastPickRefusal; }
	/// Picks the server kept since the start.
	int32 PicksKept() const { return PicksDone; }
	/// Tests and dev: more arguments for the server program at `Start`
	/// (`--dev-xp 360`).
	std::vector<std::string> ExtraServerArgs;
	/// The pilot's key sent in the Join (unset: a new random one at each
	/// start, a new pilot).
	std::optional<ac::net::PilotKey> PilotKey;

	/// The unit the pilot wants to drive: sends a take (or, with none, a
	/// hand back) once. The pilot pawn dives in when the mirror says the
	/// server gave it.
	void Want(std::optional<int64> Unit);
	std::optional<int64> Wanted() const { return WantedUnit; }
	/// The driven unit, as the server says.
	std::optional<int64> Driven() const;
	/// Hand backs sent since the start (tests: none on a region change).
	int32 HandBacksSent() const { return HandBacks; }
	/// Takes sent since the start (tests: none to be seated at the start).
	int32 TakesSent() const { return Takes; }
	/// Tests: read the network only this often (seconds; 0: every frame), as
	/// slow frames do, so what came together is handled in one frame (a
	/// RegionChange and its region's first snapshot).
	double TestReadEvery = 0;

	/// On the Welcome's first units, ask for a Prospector of the pilot's
	/// side (on by default).
	bool bAutoTake = true;

	/// The client's tick (inputs sent), the port, the server's pid.
	int64 ClientTick() const { return Net ? Net->clientTick() : 0; }
	uint16 Port() const { return Server.port(); }
	const ac::net::Client* Connection() const { return Net.Get(); }

	/// The smoothed round trip, seconds (none before the first ack).
	std::optional<double> RoundTrip() const;
	/// The lag added on the client's end, each way (zero: none).
	const ac::net::LagSettings& Lag() const { return LagNow; }
	/// The lag setting as the console variable writes it ("150 20 5",
	/// "150,20,5"; "", "0" or "off": none). False when it does not read.
	static bool ParseLag(const FString& Text, ac::net::LagSettings& Out);
	/// The driven unit's prediction.
	const ac::net::Predictor& Prediction() const { return Predictor; }
	void ClearPredictionStats() { Predictor.clearStats(); }

	// MARK: The world

	/// A flight leaves from within this many cells of a pad's centre (the
	/// server's `WorldParams::padReach`; it decides, this only says why not).
	static constexpr double PadReach = ac::region::padReach;
	/// The world map, the newest (null: not told yet).
	const ac::net::WorldMessage* World() const;
	/// The region the pilot is in (in flight: the one it lifted off from).
	const ac::net::WorldMessage::Region* Here() const;
	/// In flight: the region it flies to.
	const ac::net::WorldMessage::Region* FlyingTo() const;
	/// In flight, or come into a region whose first snapshot has not shown
	/// the driven unit yet: the pilot keeps its seat, nothing to drive.
	bool Travelling() const { return FlyingTo() != nullptr || bArriving; }
	/// Region changes so far.
	int64 Visit() const { return Net ? Net->visit() : 0; }
	/// The pad of this region nearest the driven unit, and how far (cells).
	struct FPadNear
	{
		FString Name;
		double Distance = 0;
	};
	std::optional<FPadNear> NearestPad() const;
	/// Asks to fly to `Pad` of region `Region` (the world map's pick). False,
	/// with `Why`, when it is plainly refused here (not at a pad, in flight);
	/// the server may still refuse it (a note, and `FlightRefusal`).
	bool Fly(const FString& Region, const FString& Pad, FString* Why = nullptr);
	/// The server's last refusal of a flight (empty: none since the last ask).
	const FString& FlightRefusal() const { return LastFlightRefusal; }

private:
	void Fail(const FString& Why);
	void Connect(double Now);
	void SendInputs(double Now);
	void Blend(double Now);
	void ReadReplies();
	/// Reads `ac.MmoLag` and applies it when it changed.
	void ReadLag();
	/// A RegionChange built a new mirror.
	void OnRegionChange(const ac::net::RegionChange& Change);

	EPhase CurrentPhase = EPhase::Off;
	FString StatusMessage;
	ac::net::LocalServer Server;
	std::string Token;
	FString WorldFile;
	double StartedAt = 0;
	double ConnectAt = 0;
	/// Came into a region (RegionChange), its unit not seen driven yet.
	bool bArriving = false;
	/// Flights asked for (request ids), and the last one refused.
	TSet<uint32> FlightsAsked;
	FString LastFlightRefusal;
	ac::net::UdpSocket Socket;
	/// Between the socket and the connection: the lag setting.
	TUniquePtr<ac::net::LagSimulator> LagIo;
	ac::net::LagSettings LagNow;
	FString LagText;
	TUniquePtr<ac::net::Client> Net;
	ac::net::Interpolator Interp;
	ac::net::Predictor Predictor;
	double LastBlend = 0;
	/// The mirror last seen and its game (for `Generation`).
	const ac::net::Mirror* SeenMirror = nullptr;
	int64 SeenGame = -1;
	int64 MirrorGeneration = 0;
	/// Inputs: the next one's due time.
	double NextInputAt = 0;
	double LastNow = 0;
	std::optional<int64> WantedUnit;
	bool bAutoTaken = false;
	int32 HandBacks = 0;
	int32 Takes = 0;
	/// Take the unit the server seats the pilot in (the start, a flight's
	/// landing) as the one wanted, once, if it is not `AdoptOld`.
	bool bAdopt = false;
	std::optional<int64> AdoptOld;
	double LastRead = 0;
	/// The requests in flight: id → the unit asked for (none: a hand back).
	TMap<uint32, std::optional<int64>> Asked;
	TArray<FString> Notes;
	/// Builds asked for (request ids), and the refusals not taken yet.
	TSet<uint32> BuildsAsked;
	TArray<FString> BuildRefusals;
	/// Picks asked for (request ids), the last refusal, the ones kept.
	TSet<uint32> PicksAsked;
	FString LastPickRefusal;
	int32 PicksDone = 0;
	std::vector<ac::GameEvent> Events;
};
