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
// A world that lasts (docs/mmorpg-m3.md, S4): the server keeps the world in
// `SavePath` (`--save`; the game's own `SaveFile()`,
// `~/Library/Application Support/Autocraft/Unreal/mmo/first.sqlite`) and
// seats the pilot by `PilotKey`, the one the game keeps in `PilotKeyFile()`
// (`LoadPilotKey`; the log shows only its first 8 hex digits). A save that
// does not open (not a database, held by another server) fails the start
// with the server's own reason and its log's path.
//
// The world's sky (docs/mmorpg-m6.md, C1): the server keeps one clock and
// one weather for the world, and each snapshot carries them (`net::Sky`).
// `Sky()` is that sky at the moment drawn: its time runs on with the frames
// and eases toward each snapshot's (a jump past 5 s is taken at once), so
// the sun never steps. The daylight and the weather actors draw it in place
// of the game's own clock (`AAcDaylight::SetWorldSky`), and the mirror's
// sight at night follows its hour. `-AcMmoHour=H` and `-AcMmoWeather=storm`
// (dev, shots) hold the server's hour and weather (`--hour`, `--weather`).
//
// Lag to feel it (on 127.0.0.1 there is none): the console variable
// `ac.MmoLag "delay_ms jitter_ms loss_pct"` (each way; 0 or empty: off) or
// the flag `-AcMmoLag=150,20,5`, which sets it at the start. The lag
// simulator sits on the client's end and delays both ways.
//
// No world and no engine objects: the automation test (AcMmoTests.cpp)
// drives it as the subsystem does.
//
// The world runs on (S2, docs/mmorpg.md "The world runs on"): with a save,
// the server is started detached (`--detached`, a session of its own) and
// outlives the game, its quit or its crash; it writes a run file beside the
// save (`first.run`, net::RunFile). `Start` reads it first: a live server
// of this game's build is reattached to (its port and token from the file),
// an older one (another protocol or build) is asked to save and stop and a
// new one started. `Stop` hands the unit back, says goodbye and leaves it
// running. Tests and hidden runs keep it attached (`AttachedServer`): it
// ends with the game. `RunningServer` and `StopWorldServer` are the home
// screen's "World server running" line and its STOP.
#pragma once

#include "CoreMinimal.h"

#include "NetClient.h"
#include "NetInterpolator.h"
#include "NetLagSimulator.h"
#include "NetLocalServer.h"
#include "NetPredictor.h"
#include "NetRunFile.h"
#include "NetSocket.h"
#include "Types.h"

#include "Async/Future.h"

#include <memory>
#include <optional>
#include <vector>

/// W4: a region of the world as the game draws it round the pilot's: its
/// map, its centre on the world's grid (cells, `ac::net::atlas::Place`), and
/// the sides (bits 1 north, 2 east, 4 south, 8 west) with no region beyond
/// it, the world's rim, where the ground's own border scenery goes.
struct FAcRegionAround
{
	FString Id;
	int64 Index = INDEX_NONE;
	ac::Vec2 Centre{0, 0};
	std::shared_ptr<const ac::MapDefinition> Map;
	uint8 Rim = 0;
};

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

	/// Where a packaged Mac game carries the server, its worlds and regions:
	/// `Ringshadow.app/Contents/Resources/server` (build_release.sh mac).
	/// Empty in editor runs and on other platforms.
	static FString BundledServerDir();
	/// `autocraft-server`: `<BundledServerDir>/autocraft-server` in a packaged
	/// Mac game, else the repo's `unreal/server/build/autocraft-server`.
	static FString ServerProgram();
	/// The server's log: `~/Library/Logs/Autocraft/server.log`.
	static FString ServerLog();
	/// The world the game plays: `unreal/server/worlds/first.json`, or
	/// `<BundledServerDir>/worlds/first.json` in a packaged Mac game.
	static FString DefaultWorld();

	/// The MMO mode's own folder: `~/Library/Application
	/// Support/Autocraft/Unreal/mmo`. Tests never use it: `-AcMmoDir=PATH`
	/// puts it there, `-AcSaveDir=DIR` or `-AcMmoShots=DIR` in `DIR/mmo`, and any other
	/// `-AcFlowTest` run in a scratch folder.
	static FString Directory();
	/// The pilot's key: `<Directory>/pilot.key` (32 hex digits).
	static FString PilotKeyFile();
	/// The world's save: `<Directory>/<world id>.sqlite` (`first.sqlite`).
	static FString SaveFile(const FString& World = DefaultWorld());
	/// The key kept in `File`; none there: a new random one, written there
	/// (its folder made). Unset, with `Why`, when the file does not read or
	/// cannot be written (the key is never replaced on its own).
	static std::optional<ac::net::PilotKey> LoadPilotKey(const FString& File, FString* Why = nullptr);
	/// Deletes the save `File`, and its `-wal` and `-shm` files when there,
	/// each by its full name. False, with `Why`, when one is still there.
	/// A world server still running on it (S2) is asked to save and stop
	/// first (`StopWorldServer`).
	static bool DeleteSave(const FString& File, FString* Why = nullptr);

	/// The server is started attached (it ends with the game, as before S2):
	/// `-AcMmoServerAttached`, or any test, shot or automation run unless it
	/// says `-AcMmoServerDetached` (the one scenario that checks the
	/// reattach). Detached otherwise, when there is a save.
	static bool AttachedServer();
	/// The world server running detached on the save `File` (its run file,
	/// checked live; a stale one removed). Unset: none.
	static std::optional<ac::net::RunFile> RunningServer(const FString& File = SaveFile());
	/// Asks the world server on `File` to save and stop, and waits for it.
	/// True when none runs now (or none ran); false, with `Why`, when it
	/// did not stop in time.
	static bool StopWorldServer(const FString& File = SaveFile(), FString* Why = nullptr);

	/// Starts the server on the world file and connects to it. `Now`:
	/// seconds on any steady clock (the same one `Tick` gets). False
	/// (`Failed`, with the message) when it does not start.
	bool Start(double Now, const FString& World = DefaultWorld());
	/// The world file the local server runs.
	const FString& WorldPath() const { return WorldFile; }
	/// Hands the driven unit back; an attached server is asked to stop
	/// (SIGTERM) and waited for, a detached one is told goodbye and left
	/// running.
	void Stop();
	/// This start reattached to a server already running (S2).
	bool Reattached() const { return bReattached; }
	/// The server runs detached (it outlives the game).
	bool Detached() const { return bDetached; }

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
	/// Shots only (-AcMmoShots=DIR): moves the world's clock on the server
	/// at once, `clock T`, `hour H` (on to the next H) or `ahead S`, through
	/// its `--dev-control` file (DIR/mmo/dev-control.txt). False when the run
	/// is not a shot run (the game never has it) or the file was not written.
	bool DevClock(const FString& Line);
	/// The pilot's key sent in the Join (unset: a new random one at each
	/// start, a new pilot).
	std::optional<ac::net::PilotKey> PilotKey;
	/// The world's save, passed as `--save` (empty: nothing kept).
	FString SavePath;

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
	/// The world's sky at the moment drawn (unset: none came, or not a world).
	const std::optional<ac::net::Sky>& Sky() const { return SkyNow; }
	/// The driven unit's prediction.
	const ac::net::Predictor& Prediction() const { return Predictor; }
	void ClearPredictionStats() { Predictor.clearStats(); }

	// MARK: Seamless regions (W4)

	/// The pilot's region (its map null: the mirror's) and the regions
	/// round it whose maps are built (drawn beyond its walls). Empty out of a
	/// world. Built off the game's thread as the pilot arrives; the four
	/// across its passages also get a mirror made ahead (`Client::prepare`).
	TOptional<FAcRegionAround> HereGround() const;
	/// The region the mirror is of: its index in the World message (the
	/// message's `here` may say the next region a frame before its
	/// RegionChange comes; INDEX_NONE: none yet).
	int64 MirrorIndex() const;
	const TArray<FAcRegionAround>& Around() const { return AroundNow; }
	/// A seamless crossing just came (an edge, the same unit walking on):
	/// how far the new region's centre is from the old one's (cells), once.
	/// The game moves the world's origin by that much in the same frame.
	TOptional<ac::Vec2> TakeSeamless();
	/// The world frame (AcSpace.h, `Origin`): the centre on the world's grid
	/// of the first region of the session (0 until known), and the pilot's
	/// region's centre less it: where its ground lies in Unreal space.
	ac::Vec2 SessionCentre() const { return Session.Get(ac::Vec2(0, 0)); }
	ac::Vec2 FrameOrigin() const;
	/// The last region change was a seamless crossing (`TakeSeamless`), and
	/// whether its mirror was made ahead.
	bool LastWasSeamless() const { return bLastSeamless; }
	/// Seconds since the last region change (or the Welcome): what is made
	/// ahead waits a while after it, so the frames round a crossing stay
	/// clear (W4).
	double SinceRegionChange() const { return LastNow - RegionChangedAt; }
	bool LastTookPrepared() const { return Net && Net->tookPrepared(); }

	// MARK: The world

	/// A flight leaves from within this many cells of a pad's centre (the
	/// server's `WorldParams::padReach`; it decides, this only says why not).
	static constexpr double PadReach = ac::region::padReach;
	/// The world map, the newest (null: not told yet).
	const ac::net::WorldMessage* World() const;
	/// The pilot's skill tree (docs/prospector-progression.md): the newest
	/// Progress message (null: not a world, or not told yet), its XP as the
	/// newest snapshot has it, and what happened to it since the last call
	/// (a level, a region, a feat). Decoded only: the screens come in P5.
	const ac::net::ProgressMessage* Progress() const;
	std::optional<double> Xp() const;
	std::vector<ac::net::ProgressMessage::News> TakeProgressNews();
	/// The skill screen's asks (P5): a point into `Skill`, the last point in
	/// it back, every point back (at the pilot's Citadel), an active into
	/// slot `Index` (0 Q, 1 C, 2 G; unset: empty it). False, with `Why`: not
	/// connected, not a world. The server's refusals: `SkillRefusal` (and a note).
	bool Spend(ac::Skill Skill, FString* Why = nullptr);
	bool Undo(ac::Skill Skill, FString* Why = nullptr);
	bool Respec(FString* Why = nullptr);
	/// Drops KSTR contract `Id` (docs/contracts.md); the server's refusal
	/// comes back as `SkillRefusal` and a note. False, with `Why`: not connected.
	bool Abandon(uint32 Id, FString* Why = nullptr);
	bool Slot(int32 Index, std::optional<ac::Skill> Skill, FString* Why = nullptr);
	/// The server's last refusal of a skill ask (empty: none since the last ask).
	const FString& SkillRefusal() const { return LastSkillRefusal; }
	/// Skill asks the server kept since the start.
	int32 SkillsKept() const { return SkillsDone; }
	/// The rig screen's asks (P7, docs/prospector-progression.md "Foundry and
	/// Lab"): the Foundry makes `Mark` of `Module`, the Lab researches it,
	/// it is fitted from the stores (0: taken off). False, with `Why`: not
	/// connected, not a world. The server's refusals: `RigRefusal` (and a note).
	bool Make(ac::Module Module, int64 Mark, FString* Why = nullptr);
	bool Research(ac::Module Module, int64 Mark, FString* Why = nullptr);
	bool Fit(ac::Module Module, int64 Mark, FString* Why = nullptr);
	/// The server's last refusal of a rig ask (empty: none since the last ask).
	const FString& RigRefusal() const { return LastRigRefusal; }
	/// The Citadel panel's asks (C1, docs/prospector-progression.md "The
	/// realm's tax"): give ore and MH to the realm wallet. False, with
	/// `Why`: not connected, not a world. The server's refusals:
	/// `CitadelRefusal` (and a note); `CitadelKept` counts the kept asks.
	bool Donate(int64 Ore, int64 Hydrogen, FString* Why = nullptr);
	const FString& CitadelRefusal() const { return LastCitadelRefusal; }
	int32 CitadelKept() const { return CitadelDone; }
	/// The Citadel shop's asks (C2, docs/prospector-progression.md "The
	/// Citadel shop"): one blueprint copy, one consumable; answered as the
	/// donations (`CitadelRefusal`, `CitadelKept`).
	bool BuyBlueprint(ac::StructureKind Kind, FString* Why = nullptr);
	bool BuyItem(ac::Item Item, FString* Why = nullptr);
	/// D1: the realm's Citadel turrets take their next mark of `Stat`, paid
	/// from the realm wallet; answered as the donations.
	bool UpgradeCitadel(ac::DefenceStat Stat, FString* Why = nullptr);
	/// D3: placed turret `Turret` takes its next mark of `Stat`, paid from
	/// the pilot's own wallet. The server's refusals: `TurretRefusal` (and a
	/// note); `TurretKept` counts the kept asks.
	bool UpgradeTurret(int64 Turret, ac::DefenceStat Stat, FString* Why = nullptr);
	const FString& TurretRefusal() const { return LastTurretRefusal; }
	int32 TurretKept() const { return TurretDone; }
	/// C2: use a consumable in the field (keys 1-4). The server's refusal
	/// comes back as a note; `ItemsUsed` counts the kept ones.
	bool UseItem(ac::Item Item, FString* Why = nullptr);
	int32 ItemsUsed() const { return ItemDone; }
	/// K2, Citadel missions (docs/missions.md): sign up for an offer on the
	/// board (at the realm's Citadel), drop one from the Journal. A refused
	/// sign-up is answered as the Citadel's asks (`CitadelRefusal`); a
	/// refused drop comes back as a note. `TakeAccepted` hands over the
	/// missions the server has just signed the pilot up for (the flow plays
	/// the sting and the narration once each).
	bool TakeMission(uint32 Mission, FString* Why = nullptr);
	/// H2, mercenaries (the Citadel's MERCS tab): hire one, dismiss one, send
	/// one to a field (`ProgressMessage::mercFields`); answered as the
	/// Citadel's asks (`CitadelRefusal`, `CitadelKept`).
	bool Merc(const ac::net::RequestMessage::Merc& Ask, FString* Why = nullptr);
	bool AbandonMission(uint32 Mission, FString* Why = nullptr);
	TArray<uint32> TakeAccepted();
	/// The region the pilot is in (in flight: the one it lifted off from).
	const ac::net::WorldMessage::Region* Here() const;
	/// In flight: the region it flies to.
	const ac::net::WorldMessage::Region* FlyingTo() const;
	/// In flight, or come into a region whose first snapshot has not shown
	/// the driven unit yet: the pilot keeps its seat, nothing to drive.
	bool Travelling() const { return FlyingTo() != nullptr || bArriving; }
	/// M2: the compass's target picked on the world map (a mission or a
	/// blueprint site): its region id, its ground point there, its label and
	/// colour, and what it is ("mission:7", a site's id; the map rings it).
	/// Unset: none (the compass points home or at ore).
	struct FMapTarget
	{
		FString Region;
		ac::Vec2 At{0, 0};
		FString Label;
		FLinearColor Color = FLinearColor::White;
		FString Key;
	};
	void SetMapTarget(const TOptional<FMapTarget>& Target) { MapTarget = Target; }
	const TOptional<FMapTarget>& GetMapTarget() const { return MapTarget; }
	/// Region changes so far.
	int64 Visit() const { return Net ? Net->visit() : 0; }
	/// The pad of this region nearest the driven unit, and how far (cells).
	struct FPadNear
	{
		FString Name;
		double Distance = 0;
		/// A flight leaves from within this of it (P6): Pad link's reach; a
		/// pad of the pilot's own (`building`) also its radius + 2.
		double Reach = ac::region::padReach;
		/// The pilot's own (its Spacedock, a Homing beacon's building).
		std::optional<ac::StructureKind> Building;
		bool Within() const { return Distance <= Reach; }
	};
	/// The pad the driven unit can fly from (the nearest within its reach),
	/// else the nearest: the server's rule (`WorldMap::fly`).
	std::optional<FPadNear> NearestPad() const;
	/// Pad link's reach (the server's `padReach`), from the pilot's ranks.
	double PadReachNow() const;
	/// Ringwalker: learned (unset: not), and the seconds until it is ready
	/// (0: a flight from anywhere now).
	std::optional<double> RingwalkerIn() const;
	/// What a flight from here to region `RegionId` costs in MH (F1,
	/// `ac::Flight`: a base and a share a grid step; the server's price).
	/// Unset: no world yet, or no such region.
	std::optional<int64> FlightPrice(const FString& RegionId) const;
	/// The wallet's MH now (the mirror's side bank). Unset: nothing seen yet.
	std::optional<int64> HydrogenNow() const;
	/// Why a `Kind` may not stand at `At` (ground) in this region: its
	/// footprint touches a pad's plate (the server's rule,
	/// `region::touchesPadPlate`; nil: clear, or nothing known yet).
	std::optional<std::string> PadPlateRefusal(ac::StructureKind Kind, ac::Vec2 At) const;
	/// Asks to fly to `Pad` of region `Region` (the world map's pick). False,
	/// with `Why`, when it is plainly refused here (not at a pad, in flight);
	/// the server may still refuse it (a note, and `FlightRefusal`).
	bool Fly(const FString& Region, const FString& Pad, FString* Why = nullptr);
	/// The server's last refusal of a flight (empty: none since the last ask).
	const FString& FlightRefusal() const { return LastFlightRefusal; }

	/// Failed, with `Why` for the banner (the server stopped). Also before a
	/// `Start` the game could not make (no pilot key).
	void Fail(const FString& Why);

private:
	TOptional<FMapTarget> MapTarget;
	void Connect(double Now);
	void SendInputs(double Now);
	void Blend(double Now);
	void ReadReplies();
	/// Reads `ac.MmoLag` and applies it when it changed.
	void ReadLag();
	/// A RegionChange built a new mirror.
	void OnRegionChange(const ac::net::RegionChange& Change);
	/// W4: builds the maps (and the mirrors across the passages) of the
	/// regions round the pilot's, off the game's thread; takes what is done.
	void PrepareAround();
	/// The world's region files, by id (read once from `WorldFile`).
	const TMap<FString, FString>& RegionTexts();
	struct FRegionBuilt
	{
		FString Id;
		std::shared_ptr<const ac::MapDefinition> Map;
		std::unique_ptr<ac::net::Mirror> Mirror;
	};
	TMap<FString, std::shared_ptr<const ac::MapDefinition>> BuiltMaps;
	TMap<FString, TFuture<FRegionBuilt*>> Building;
	/// Regions a mirror was made ahead for since the last region change.
	TSet<FString> MirrorsMade;
	TMap<FString, FString> Texts;
	bool bTextsRead = false;
	TArray<FAcRegionAround> AroundNow;
	int64 AroundFor = -2;
	TOptional<ac::Vec2> SeamlessShift;
	/// The region the mirror is of (empty: the Welcome's, `here` then).
	std::string MirrorRegion;
	TOptional<ac::Vec2> Session;
	double RegionChangedAt = 0;
	bool bLastSeamless = false;

	EPhase CurrentPhase = EPhase::Off;
	FString StatusMessage;
	ac::net::LocalServer Server;
	std::string Token;
	/// The server's port: from its `PORT` line, or its run file.
	uint16 ServerPort = 0;
	bool bDetached = false;
	bool bReattached = false;
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
	/// The sky drawn, and the newest snapshot's server time and when it came.
	std::optional<ac::net::Sky> SkyNow;
	double SkyAnchor = 0;
	double SkyAnchorAt = 0;
	int64 SkySnapshot = -1;
	const ac::net::Mirror* SkyMirror = nullptr;
	/// Moves the sky drawn on to `Now` (`Blend`).
	void FollowSky(ac::net::Mirror& M, double Now, double Dt);
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
	/// Skill asks (request ids), the last refusal, the ones kept.
	TSet<uint32> SkillsAsked;
	FString LastSkillRefusal;
	int32 SkillsDone = 0;
	/// Checks a skill ask can go (connected, a world).
	bool CanAskSkills(FString* Why) const;
	void AskedSkill(uint32 Id, const FString& What);
	/// Rig asks (request ids) and the last refusal.
	TSet<uint32> RigAsked;
	FString LastRigRefusal;
	/// Citadel asks (request ids), the last refusal, the ones kept.
	TSet<uint32> CitadelAsked;
	FString LastCitadelRefusal;
	/// D3: the turret card's asks.
	TSet<uint32> TurretAsked;
	FString LastTurretRefusal;
	int32 TurretDone = 0;
	int32 CitadelDone = 0;
	/// Consumable uses asked (request ids), and the ones kept.
	TSet<uint32> ItemAsked;
	int32 ItemDone = 0;
	/// Mission asks (request id → mission, and whether a sign-up), and the
	/// sign-ups kept, not handed over yet.
	TMap<uint32, TPair<uint32, bool>> MissionAsked;
	TArray<uint32> MissionsAccepted;
	bool AskRig(const TCHAR* Verb, ac::Module Module, int64 Mark, FString* Why);
	std::vector<ac::GameEvent> Events;
};
