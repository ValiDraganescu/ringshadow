// `FAcMmoClient`: the game's end of the MMORPG test mode (docs/mmorpg-m1.md,
// T5; docs/networking.md, "The local server", "The client"). It starts
// `autocraft-server` (unreal/server, built by `make -C unreal/server`) as a
// hidden child on the map asked for, hands it a one-time token on its
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

	/// Starts the server on `Map` and connects to it. `Now`: seconds on any
	/// steady clock (the same one `Tick` gets). False (`Failed`, with the
	/// message) when it does not start.
	bool Start(const ac::MapChoice& Map, double Now);
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

	/// The unit the pilot wants to drive: sends a take (or, with none, a
	/// hand back) once. The pilot pawn dives in when the mirror says the
	/// server gave it.
	void Want(std::optional<int64> Unit);
	std::optional<int64> Wanted() const { return WantedUnit; }
	/// The driven unit, as the server says.
	std::optional<int64> Driven() const;

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

private:
	void Fail(const FString& Why);
	void Connect(double Now);
	void SendInputs(double Now);
	void Blend(double Now);
	void ReadReplies();
	/// Reads `ac.MmoLag` and applies it when it changed.
	void ReadLag();

	EPhase CurrentPhase = EPhase::Off;
	FString StatusMessage;
	ac::net::LocalServer Server;
	std::string Token;
	ac::MapChoice Map;
	double StartedAt = 0;
	double ConnectAt = 0;
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
	/// The requests in flight: id → the unit asked for (none: a hand back).
	TMap<uint32, std::optional<int64>> Asked;
	TArray<FString> Notes;
	std::vector<ac::GameEvent> Events;
};
