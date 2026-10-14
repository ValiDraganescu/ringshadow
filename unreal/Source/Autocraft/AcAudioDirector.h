// The game's sound effects (GAME-LAYER.md §2.10, chunk C7): the port of the
// Swift `AudioDirector` (Sources/Autocraft/Audio.swift) and the gains of
// `SoundBoard.swift`.
//
// Two layers:
// - `FAcAudioRules`, plain C++ with no audio in it: which event and which
//   state change plays what (the event table), the per-name per-second
//   limiter, the report-in and Oracle cool-downs, the shells' delayed bursts,
//   the loops wanted from state (wind, drill and weld, hum, the nearest 6
//   mini guns, the heal beam). It says what to play as cues and loops; the
//   automation tests (`Autocraft.Audio.Rules.*`) drive it with events.
// - `UAcAudioDirector`, the world subsystem that plays them: the sound bank
//   from `Content/Audio/Sounds.json` (made by Tools/Editor/import_sounds.py),
//   the voice pools (USoundConcurrency: 6 for deposits and voices, 12 for
//   the fight, 4 in your ear, oldest stopped), one audio component per loop,
//   and the top-down hearing: gain from where the sound is on screen
//   (`ac::Listener::viewGain` through the RTS pawn's `ac::FreeView`), pan by
//   placing the sound on a unit circle round the listener at the angle of
//   its screen X, as Swift does (`Audio.place`). It takes the sim's
//   fog-gated events at the `Audio` frame stage.
//
// Command line:
//   -AcHearAll               hear the whole map (`Listener.local` off).
//   -AcAudioLog              log every second which sounds started
//                            (`audio: 12s gun 9, deposit 1 | loops ...`) and a
//                            total at quit. Also the cvar `ac.AudioLog 1`.
//   -AcAudioRecord=PATH      record the mix (master submix) to a 16-bit WAV:
//   -AcAudioRecordAt=S       from S game seconds in (default 1)
//   -AcAudioRecordSeconds=N  for N seconds (default 20), then quit (with
//                            -AcShot, the shot quits once both are written).
//   -AcAudioQuitAt=S         quit after S seconds of play (with -AcAudioLog).
//   Run it with -deterministicaudio -UseFixedTimeStep -FPS=60 for an offline
//   render: the non-realtime mixer renders exactly one frame of audio a frame
//   and nothing reaches the speakers. That mixer renders spatialized
//   sources silent (UE 5.8: it reports 2 channels with an 8-entry channel
//   array; `ac.AudioFlat -1` shows it), so offline every sound plays unplaced: the
//   gains and the mix are the game's, the left-right pan is not
//   (`ac.AudioFlat 1` does the same in real time).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Hearing.h"
#include "SimdMath.h"
#include "Types.h"
#include "Vision.h"

#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "AcAudioDirector.generated.h"

class UAudioComponent;
class USoundAttenuation;
struct FAcVoice3D;
class USoundConcurrency;
class USoundWave;
struct FAcFrame;

namespace ac { class Simulation; }

/// The Swift gains (`AudioDirector`'s statics, Audio.swift:38-92), the voice
/// pools and the rates of the limiter.
namespace AcSound
{
	/// The master volume (`AudioDirector.Settings.volume`); the music's is
	/// `MusicVolume` under it (`Settings.music`).
	inline constexpr float Volume = 0.6f;
	inline constexpr float MusicVolume = 0.5f;
	/// The world's sounds (units, buildings, the wind and rain; not the
	/// in-ear ones) under the top-down view and while driving: quieter
	/// from above, louder in the cockpit, where the driven unit hears only
	/// as far as it sees (`UAcPilotAudio::HearingRange`).
	inline constexpr float RtsWorldGain = 0.7f;
	inline constexpr float DriveWorldGain = 1.2f;
	/// How far a sound carries while driving, times the driven unit's
	/// hearing (1.5 × its sight): the blasts, cannons, shells and deaths
	/// 1.33 (2 × sight), the drills, welders, steps and hums 0.67 (its
	/// sight), the rest 1. The top-down view hears what it shows.
	AUTOCRAFT_API float Carry(FName Sound);

	/// The Prospector's drill: 30% under the Swift game's 0.25.
	inline constexpr float DrillGain = 0.175f;
	inline constexpr float WeldLoopGain = 0.4f;
	inline constexpr float FireflyEngineGain = 0.22f;
	inline constexpr float DropshipEngineGain = 0.24f;
	inline constexpr float KestrelEngineGain = 0.22f;
	inline constexpr float HailstormEngineGain = 0.26f;
	inline constexpr float RocketGain = 0.45f;
	inline constexpr float FlakGain = 0.85f;
	inline constexpr float FlakHitGain = 0.7f;
	inline constexpr float MissileGain = 0.35f;
	inline constexpr float SentinelTrackGain = 0.3f;
	inline constexpr float HealGain = 0.35f;
	inline constexpr float TankCannonGain = 0.7f;
	inline constexpr float MinigunGain = 0.3f;
	inline constexpr int32 MinigunsAtOnce = 6;
	inline constexpr float AnchorHitGain = 0.8f;
	inline constexpr float AnchorGain = 0.75f;
	inline constexpr float HumGain = 0.11f;
	inline constexpr float WindGain = 0.16f;
	/// The wind's two layers, panned apart for width.
	inline constexpr float WindPan = 0.6f;
	/// The weather's beds (docs/weather.md): light rain and the downpour,
	/// crossfaded on the rain rate; the wind rises in a storm by this much.
	/// (2026-10-12: new, darker takes, low-passed at 7 kHz; the first ones
	/// were a clipped hiss 13 dB over the wind. Light rain now sits about
	/// 2 dB under the wind, the downpour about 4 dB over it.)
	inline constexpr float RainGain = 0.7f;
	inline constexpr float DownpourGain = 0.3f;
	inline constexpr float StormWindLift = 0.7f;
	/// A strike's thunder: a close one's crack, a far one's roll.
	inline constexpr float ThunderGain = 1.1f;
	inline constexpr float BoomGain = 1.0f;
	inline constexpr float HabDomeBoomGain = 0.7f;
	inline constexpr float HitMarkGain = 0.5f;
	inline constexpr float HitClankGain = 0.55f;
	inline constexpr float LockOnGain = 0.28f;
	inline constexpr float BreathGain = 0.5f;
	inline constexpr float KillMarkGain = 0.34f;
	inline constexpr float LevelUpGain = 0.5f;
	inline constexpr float PickGain = 0.35f;
	inline constexpr float AlertGain = 0.7f;
	inline constexpr float DepositGain = 0.45f;
	inline constexpr float VoiceGain = 0.6f;
	inline constexpr float DeathGain = 0.55f;
	inline constexpr float BlastGain = 0.5f;
	inline constexpr float GrenadeGain = 0.45f;
	/// The Atlas's Quake stomp: the shell burst's deep thud (no sound of its own yet).
	inline constexpr float StompGain = 0.9f;
	inline constexpr float DirtHitGain = 0.3f;
	inline constexpr float StepGain = 0.22f;
	/// The Atlas's own sounds (Tools/AudioGen/atlas.manifest.json) and the Scorpion's
	/// (scorpion.manifest.json): a cannon salvo, a footfall, the driven engine; the sting,
	/// the dig in and out, the lock-on chirp, the legs.
	inline constexpr float AtlasShotGain = 0.35f;
	inline constexpr float AtlasFootGain = 0.5f;
	inline constexpr float AtlasEngineGain = 0.3f;
	inline constexpr float ScorpionStingGain = 0.3f;
	inline constexpr float ScorpionDigGain = 0.6f;
	inline constexpr float ScorpionLockGain = 0.5f;
	inline constexpr float ScorpionEngineGain = 1.0f;
	inline constexpr float JetGain = 0.4f;
	/// A pad flight and the Prospector's jet pack (F1b; Tools/AudioGen/
	/// flight.manifest.json), in the ear: set from each take's loudness
	/// after the import's 0.8 peak, for about -18 LUFS (the cruise a bed
	/// under it, the dry click a short tick that the meter reads low).
	inline constexpr float FlightSpoolGain = 0.45f;
	inline constexpr float FlightRoarGain = 0.5f;
	inline constexpr float FlightCruiseGain = 0.6f;
	inline constexpr float FlightRetroGain = 0.25f;
	inline constexpr float FlightTouchGain = 0.35f;
	inline constexpr float JetPuffGain = 0.3f;
	inline constexpr float JetDryGain = 0.6f;
	/// The MMORPG's moments (docs/mmorpg-m7.md J1; Tools/AudioGen/
	/// moments.manifest.json). Set from each take's loudness after the
	/// import's peak normalising (unreal/Tools/Audio/check_sounds.py): the
	/// beds about as loud as the wind, the stings as the old level-up, the
	/// starfall's boom a few dB under a close blast.
	/// The ringshadow: the drone swelling as the band comes, the whoosh of
	/// its edge, the cold bed under it, the sun's return, the flaring ore.
	inline constexpr float ShadowDroneGain = 0.15f;
	inline constexpr float ShadowBedGain = 0.15f;
	inline constexpr float ShadowPassGain = 0.3f;
	inline constexpr float ShadowReleaseGain = 0.23f;
	inline constexpr float ShimmerGain = 0.07f;
	/// How far the drone reaches out to the band's edge (cells; about 30 s
	/// of its sweep) and the flaring ore heard (cells).
	inline constexpr double ShadowDroneCells = 90.0;
	inline constexpr double ShimmerCells = 14.0;
	/// The starfall: the streak (its take rises for this many seconds to
	/// the impact), the boom, its echo off the hills (this long after), the
	/// hot crater. Heard across the region and the next: from `FarCells`
	/// off at `FarGain` of their gain; the boom and echo arrive at the
	/// speed of sound (a cell is a metre).
	inline constexpr float StreakGain = 0.5f;
	inline constexpr double StreakSeconds = 6.8;
	inline constexpr float StarBoomGain = 1.0f;
	inline constexpr float StarEchoGain = 0.6f;
	inline constexpr double StarEchoAfter = 1.6;
	inline constexpr float CraterGain = 0.2f;
	inline constexpr double FarCells = 500.0;
	inline constexpr float FarGain = 0.3f;
	inline constexpr double SoundCellsPerSecond = 343.0;
	/// The progression's news: the Prospector's level-up sting, a blueprint
	/// found, a contract paid, a module made, a mark researched.
	inline constexpr float RankUpGain = 0.25f;
	inline constexpr float NewsGain = 0.45f;
	/// The home screen (H1): ENTER THE REACH swells in with the ringshadow's
	/// release; the other buttons tick with the level-up card's pick.
	inline constexpr float MenuEnterGain = 0.35f;
	inline constexpr float MenuPickGain = 0.3f;

	/// Seconds between report-in lines, between the Oracle's warnings,
	/// between a driven vehicle's engine sounds; a Sentinel opening fire
	/// after this long quiet swings its head and locks on.
	inline constexpr double VoiceEvery = 3.0;
	inline constexpr double AlertEvery = 30.0;
	inline constexpr double EngineEvery = 2.0;
	inline constexpr double SentinelQuiet = 3.0;

	/// Voices per pool (Audio.swift:211-213).
	inline constexpr int32 DepositVoices = 6;
	inline constexpr int32 CombatVoices = 12;
	inline constexpr int32 InEarVoices = 4;

	/// Below this a sound is not started (Swift's `r > 0.001`).
	inline constexpr float Audible = 0.001f;

	/// What every sound is multiplied by: `Volume` × the player's
	/// `UAcSettings::MasterVolume` (0…1), live.
	AUTOCRAFT_API float MasterGain();
	/// The music's own gain under it: `MusicVolume` × `UAcSettings::MusicVolume`,
	/// 0 with `bMusic` off.
	AUTOCRAFT_API float MusicGain();
	/// The master gain times the player's effects / environment volume.
	AUTOCRAFT_API float SfxGain();
	AUTOCRAFT_API float AmbientGain();
}

/// Which voices a one-shot plays on: deposits and voice lines, the fight
/// (so a firefight never cuts off a deposit or a report-in), or centred in
/// your ear (the Oracle, the driven unit's marks).
enum class EAcSoundPool : uint8
{
	Deposit,
	Combat,
	InEar,
};

/// A one-shot to start now.
struct FAcSoundCue
{
	/// The bank name (`gun`, `deposit`...).
	FName Name;
	/// The Swift gain times the reach (no master volume).
	float Volume = 0.f;
	/// Where it is heard (ground, sim cells); unused in your ear.
	ac::Vec2 At;
	EAcSoundPool Pool = EAcSoundPool::Deposit;
	/// Heard from this side whatever the distance (-1 left … 1 right): a
	/// starfall far past the driven unit's hearing.
	TOptional<float> Pan;
};

/// A loop the state wants playing this frame.
struct FAcLoopWant
{
	/// Stable per loop: what it belongs to and which loop it is.
	int64 Key = 0;
	FName Name;
	/// Which variant of `Name` (a Prospector's drill keeps its own).
	int32 Variant = 0;
	/// The Swift gain times the fade times the reach.
	float Volume = 0.f;
	ac::Vec2 At;
	/// Height of the sound over the ground, cells (Swift's `lift`).
	float Lift = 0.7f;
	/// In your ear (the heal beam) or a fixed pan (`Pan`, the wind).
	bool bInEar = false;
	TOptional<float> Pan;
	/// Seconds into the loop it starts at (the wind's second layer: half way).
	float StartFraction = 0.f;
	/// The world around (wind, hum): the environment volume, not the effects'.
	bool bAmbient = false;
};

/// Everything the Swift `AudioDirector` decides, with no audio in it.
class AUTOCRAFT_API FAcAudioRules
{
public:
	/// How much of a sound at a ground point reaches the ears, 0…1
	/// (`AudioDirector.reach`), for a sound that carries `Carry` times the
	/// hearing range (`AcSound::Carry`). Default: all of it.
	std::function<float(ac::Vec2, float Carry)> Reach = [](ac::Vec2, float) { return 1.f; };
	/// How many variants a sound has (0: none on hand, it never plays).
	std::function<int32(FName)> Variants = [](FName) { return 1; };
	/// The local player: its buildings hit wake the Oracle.
	int64 LocalPlayer = 0;

	/// Seconds of play (`AudioDirector.clock`: the frames' real time).
	double Clock() const { return Now; }
	void Advance(double Dt) { Now += Dt; }

	/// One frame's fog-gated events (`GameController.swift:320-389`).
	/// `Sim` is the whole truth after the steps (shooters, towers, targets).
	void Consume(const std::vector<ac::GameEvent>& Events, const ac::Simulation& Sim);
	/// The jet packs and anchors that changed since the last frame
	/// (`GameController.swift:390-410`). As Swift, over every unit of the
	/// whole truth (`State`, not the shown game), sounding only those the
	/// team hears: its own and the others' in `Sight` (none: all heard).
	/// So a Comet already in the air, or a Longbow already anchored, that
	/// walks into sight makes no sound; from the shown game it did.
	void Watch(const ac::GameState& State, const ac::Sight* Sight = nullptr);
	/// The shells' and flak's bursts that are due now.
	void Due();
	/// The loops for this frame (`AudioDirector.update`): fades move by `Dt`.
	void Loops(const ac::GameState& Shown, double Dt, std::vector<FAcLoopWant>& Out);

	// The event table, one per Swift function (Audio.swift:489-650).
	void Deposited(ac::Vec2 At);
	void Trained(ac::UnitKind Kind, ac::Vec2 At);
	void Shot(std::optional<ac::UnitKind> Kind, bool bAnchored, std::optional<ac::Vec2> From, ac::Vec2 At, int64 Victim,
		bool bStructure, bool bMinigun = false, bool bTower = false);
	void Died(ac::UnitKind Kind, ac::Vec2 At);
	void Destroyed(ac::StructureKind Kind, ac::Vec2 At);
	void Grenade(ac::Vec2 At);
	/// An Atlas's Quake stomp at `At`.
	void Stomp(ac::Vec2 At);
	void Jump(bool bLanding, ac::Vec2 At);
	void Anchor(bool bUp, ac::Vec2 At);
	/// A Scorpion digging in or out, and locking onto a victim.
	void Burrow(bool bUp, ac::Vec2 At);
	void ScorpionLock(ac::Vec2 At);
	/// An Atlas's foot came down at `At` (`UAcEffects::Footfall`).
	void Footfall(ac::Vec2 At);
	// The driven unit's sounds (the pilot chunk calls these; `Consume`
	// already plays its misses, hits and level-ups).
	void HitMark(bool bKill, bool bStructure);
	void DirtHit(ac::Vec2 At);
	void Step();
	void Breath();
	void LockOn();
	void LevelUp();
	void Picked();
	void PullAway(ac::UnitKind Kind);
	void Heal(bool bOn) { bHealing = bOn; }
	/// The weather now (AAcWeather): the rain beds and the wind follow it.
	void Weather(double Rain, double Storm) { RainRate = Rain; Storminess = Storm; }
	/// A lightning strike's thunder reaching the ears now: a crack within
	/// 2 km, a roll further out, quieter with distance.
	void Thunder(double DistanceKm, double Power);

	// The MMORPG's moments (docs/mmorpg-m7.md J1).
	/// The ringshadow sweep at the ears (docs/weather.md), each frame:
	/// `ToStart` seconds until the next sweep begins; `EdgeAhead` how far
	/// its leading edge still is (cells along the band; unset: no band now,
	/// negative: past); the ears' `Cover` (0-1); `Flare` the nearest
	/// deposit in hearing that flares (cover at least the bonus's). The
	/// drone swells from the warning as the edge comes, its whoosh plays as
	/// the cover passes a half, the cold bed follows the cover, the release
	/// plays as it drops under a half again.
	struct FShadowHear
	{
		double ToStart = 1e9;
		std::optional<double> EdgeAhead;
		double Cover = 0;
		std::optional<ac::Vec2> Flare;
	};
	void Ringshadow(const FShadowHear& H);
	/// The starfalls told, each frame: seconds to the impact (negative
	/// after), the impact on the ears' region's ground, its distance from
	/// the ears (cells) and the side it is on (`Pan`); `bCrater`: it landed
	/// here and its crater is hot, `Age` seconds old. The streak starts as
	/// it comes in, the boom and its echo arrive after the impact.
	struct FStarHear
	{
		uint32 Id = 0;
		double ToImpact = 0;
		ac::Vec2 At;
		double Distance = 0;
		float Pan = 0;
		bool bCrater = false;
		double Age = 0;
	};
	void Starfalls(const std::vector<FStarHear>& Falls);
	/// The progression's news (`UAcProgressHudSubsystem::NewsItem`).
	enum class ENews : uint8 { Blueprint, Paid, Made, Researched };
	void News(ENews N);
	/// A pad flight's moments (F1b; `AAcPilotPawn::FlightView`): the
	/// engines spooling up on the pad, the lift-off's roar, the retro
	/// thrust coming down, the touchdown's thud and dust.
	enum class EFlight : uint8 { Spool, LiftOff, Retro, Touchdown };
	void Flight(EFlight Moment);
	/// The cruise's wind and engine loop, while the ship is up.
	void Cruise(bool bOn) { bCruising = bOn; }
	/// The jet pack walked into a cliff with no MH to burn, at sim second
	/// `When` (`Simulation::pilotJumpDry`): a dry click, at most every 0.8 s.
	void JetDry(double When);

	/// The cues decided since the last `TakeCues`.
	std::vector<FAcSoundCue> TakeCues();
	/// A new game: drop the loops' memory and the jumps and anchors seen.
	void Reset();

	/// The sound a shooter fires, its gain and how many may start a
	/// second (the `switch` in `AudioDirector.shot`).
	struct FWeapon { const char* Name; float Gain; int32 PerSecond; };
	static FWeapon WeaponOf(std::optional<ac::UnitKind> Kind, bool bAnchored, bool bTower);
	/// `<kind>death`, and the report-in line `v<kind>`.
	static const char* DeathOf(ac::UnitKind Kind);
	static const char* VoiceOf(ac::UnitKind Kind);

	/// The driven unit (its misses and hits play in your ear), if any.
	std::optional<int64> PilotUnit;
	/// Where the driven gun's misses land (`sightPoint(reach:)`, E7): the
	/// dust's sound plays there, not where the sim spent the round.
	std::optional<ac::Vec2> PilotSpent;
	/// The MMORPG mode: a level is the Prospector's, from the server's news
	/// (`UAcLevelingSubsystem` plays `LevelUp`), not a kind's `LeveledUp`.
	bool bServerLevels = false;

private:
	/// True if fewer than `PerSecond` of this sound started in the last second.
	bool Allow(FName Sound, int32 PerSecond);
	bool OneShot(FName Sound, ac::Vec2 At, float Gain, EAcSoundPool Pool);
	void Later(double Seconds, FName Sound, ac::Vec2 At, float Gain, EAcSoundPool Pool = EAcSoundPool::Combat,
		TOptional<float> Pan = {});

	double Now = 0.0;
	std::vector<FAcSoundCue> Cues;
	TMap<FName, TArray<double>> Recent;
	double LastVoice = -10.0;
	double LastAlert = -100.0;
	double LastEngine = -10.0;
	/// When each tower (by where it stands) last fired.
	std::map<std::pair<double, double>, double> TowerShots;
	struct FDelayed { double At; FName Sound; ac::Vec2 Where; float Gain; EAcSoundPool Pool; TOptional<float> Pan; };
	std::vector<FDelayed> Delayed;

	struct FTool { FName Sound; int32 Variant = 0; float Gain = 0.f; };
	TMap<int64, FTool> Tools;
	TMap<int64, FTool> Miniguns;
	TSet<int64> Hums;
	float HealFade = 0.f;
	bool bHealing = false;
	double RainRate = 0.0, Storminess = 0.0;
	/// The moments: the ringshadow at the ears and its fades, the falls
	/// heard (streaks started, booms due) and their hot craters.
	FShadowHear Shadow;
	bool bUnderBand = false;
	float DroneFade = 0.f, BedFade = 0.f, ShimmerFade = 0.f;
	ac::Vec2 ShimmerAt;
	TSet<uint32> Streaked, Boomed;
	std::vector<FStarHear> Craters;
	TSet<int64> Jumping;
	/// The flight's cruise loop and its fade; the last dry click's sim time.
	bool bCruising = false;
	float CruiseFade = 0.f;
	double DryHeard = -1e9;
	TSet<int64> Anchoring;
	TSet<int64> Burrowing, Locking;
	bool bWatched = false;
	std::mt19937 Random{0x5eed};
};

/// The ears while driving (`Ears.swift`): at the pilot camera, UE cm.
struct FAcEars
{
	FVector At = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector Up = FVector::UpVector;
};

/// A sound's variants, for the bank (a UPROPERTY so the waves stay loaded).
USTRUCT()
struct FAcSoundVariants
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWave>> Waves;
};

UCLASS()
class AUTOCRAFT_API UAcAudioDirector : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAcAudioDirector* Get(const UObject* WorldContext);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/// The decisions (the pilot chunk calls its `HitMark`, `Step`...).
	FAcAudioRules& Rules() { return Brain; }

	/// Silence everything (the menu's Sound toggle); everything keeps running.
	void SetMuted(bool bInMuted);
	bool IsMuted() const { return bMuted; }
	/// A menu sound, in the ear, at once: the home screen and the pause menu
	/// hold the game, and so the cues (H1). Counted in the -AcAudioLog lines.
	void PlayMenu(FName Name, float Gain);
	/// -AcAudioRecord: asked for, started, written (AcShot's -AcShotRecord
	/// waits on these to record frames and sound together).
	bool IsRecordingRequested() const { return Recording.IsSet(); }
	bool HasRecordingStarted() const { return Recording.IsSet() && Recording->bStarted; }
	bool IsRecordingDone() const { return Recording.IsSet() && Recording->bDone; }
	/// Start the recording now (it still waits for the bank to load).
	void RecordFromNow() { if (Recording.IsSet() && !Recording->bStarted) Recording->At = Brain.Clock(); }
	/// Hear the whole map (`Listener.local` off) or what the view shows.
	void SetHearWholeMap(bool bOn);
	bool HearsWholeMap() const;

	/// While driving: place world sounds in 3D round these ears; unset: the
	/// top-down hearing. `Blocked` (optional) says when a cliff or a
	/// building stands between the ears and a sound (E7: Swift's
	/// `obstruction = -14` as measured, −1.75 dB and a 3 kHz low-pass on the
	/// direct path; `UAcPilotAudio::Blocked` is the port of `soundBlocked`).
	void SetEars(const TOptional<FAcEars>& InEars) { Ears = InEars; }
	std::function<bool(const FVector& Ears, const FVector& Sound)> Blocked;

	/// How much of a sound at `P` reaches the ears (`AudioDirector.reach`).
	/// While driving: as far as the driven unit hears, times `Carry`,
	/// whatever the listener.
	float Reach(ac::Vec2 P, float Carry = 1.f) const;
	/// `AcSound::RtsWorldGain` or, while driving, `DriveWorldGain`.
	float WorldGain() const { return Ears ? AcSound::DriveWorldGain : AcSound::RtsWorldGain; }
	/// Its pan −1…1 in the top-down view (`AudioDirector.pan`).
	float Pan(ac::Vec2 P) const;

	/// The sounds started so far, by name (for the log and tests).
	const TMap<FName, int32>& Started() const { return Totals; }

	/// A sound of the bank (`Variant` < 0: a random one), and whether placed
	/// sounds play unplaced now (offline, `ac.AudioFlat`): for E7's probe.
	USoundWave* Sound(FName Name, int32 Variant = -1) { return Pick(Name, Variant); }
	bool Flat() const { return PlaceFlat(); }

private:
	void OnFrame(const FAcFrame& Frame);
	void OnGameStarted(class UAcSimSubsystem& Sim);
	/// The MMORPG's moments at the ears (J1): the ringshadow from the world's
	/// clock, the starfalls from the client's sky.
	void HearMoments(const class UAcSimSubsystem& Sim);
	void LoadBank();
	void MakeSettings();
	USoundWave* Pick(FName Name, int32 Variant = -1);
	/// Play placed sounds unplaced (2D): `ac.AudioFlat 1`, and always on
	/// the non-realtime mixer.
	bool PlaceFlat() const;
	void Play(const FAcSoundCue& Cue);
	/// An audio device to play on (false with -nosound: then no voice is
	/// placed or started, `ac.AudioSkipSilent`).
	bool HasDevice() const;
	void SyncLoops(const std::vector<FAcLoopWant>& Wants);
	/// Where a sound at ground `P` plays from; while driving (and not at a
	/// fixed pan) also its 3D voice (E7's `UAcPilotAudio`: volume factor,
	/// low-pass, hall send).
	FVector Place(ac::Vec2 P, float Lift, TOptional<float> FixedPan, TOptional<struct FAcVoice3D>& OutVoice) const;
	double GroundHeight(ac::Vec2 P) const;
	void LogSecond(double GameTime);
	TOptional<int64> FirstSecond;
	void TickRecording(const FAcFrame& Frame);

	FAcAudioRules Brain;
	FDelegateHandle FrameHandle;
	FDelegateHandle StartedHandle;

	/// The bank: name → variants (Sounds.json).
	UPROPERTY(Transient)
	TMap<FName, FAcSoundVariants> Bank;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> Panned;
	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> DepositPool;
	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> CombatPool;
	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> InEarPool;

	/// One component per loop, by `FAcLoopWant::Key`.
	UPROPERTY(Transient)
	TMap<int64, TObjectPtr<UAudioComponent>> LoopVoices;
	TMap<int64, TPair<FName, int32>> LoopSounds;
	/// The loops playing as 3D voices round the pilot's ears (E7).
	TSet<int64> Loops3D;
	/// One-shots of looping waves (the weld's strike), stopped after one pass.
	TArray<TPair<TWeakObjectPtr<UAudioComponent>, double>> StopAt;

	TOptional<FAcEars> Ears;
	bool bMuted = false;
	/// The weather's game time the thunder has been heard up to.
	double ThunderHeardTo = -1.0;
	bool bLog = false;
	TMap<FName, int32> ThisSecond;
	TMap<FName, int32> Totals;
	int64 LoggedSecond = 0;
	int32 AudibleLoops = 0;
	TMap<FName, int32> LoopCount;

	struct FRecording
	{
		FString Path;
		double At = 1.0;
		double Seconds = 20.0;
		double Rendered = 0.0;
		bool bStarted = false;
		bool bDone = false;
		bool bNonRealtime = false;
	};
	TOptional<FRecording> Recording;
	double BankLoadedAt = 0;
	/// -AcAudioQuitAt=S: quit after S seconds of play (headless checks).
	TOptional<double> QuitAt;
};
