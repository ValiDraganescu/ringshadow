// A2, the siege heard (docs/music.md "Siege layers"): two music beds over
// KSTR 88.7 while a canyon is fought near the pilot, the radio ducked under
// them (`ac::SiegeMix`), and the cue timeline the shots read.
//
// - The wave layer (`Resources/Sounds/siege/siege_wave.mp3`, a loop) rises
//   while a wave is out near the pilot, louder with the wave's number.
// - The Matriarch layer (`siege_matriarch.mp3`) while she comes for a
//   canyon's turrets (FAILED), or stands in the pilot's region.
// - Both fade out on HELD, a reset, or with no canyon near; the radio comes
//   back over 2 s after them.
// Silent runs (-nosound) play nothing but log every level as it moves.

#include "AcMusicPlayer.h"

#include "AcCanyon.h"
#include "AcLog.h"
#include "AcMmo.h"
#include "AcMp3Loader.h"
#include "AcSimSubsystem.h"
#include "AcAudioDirector.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

#include <cmath>

namespace
{
	/// The beds' own levels under the music's gain (Rising Pressure sits
	/// 3 dB under Firefight).
	constexpr float WaveBedGain = 0.8f;
	constexpr float QueenBedGain = 1.0f;

	FString BedPath(const TCHAR* Id)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Resources/Sounds/siege"), FString(Id) + TEXT(".mp3")));
	}
}

void UAcMusicPlayer::Cue(const FString& Kind, const FString& Name, const double Gain)
{
	// The same one-shot twice in a breath (two frames asking): once.
	if (Kind == TEXT("flight") && !CueLog.IsEmpty() && CueLog.Last().Kind == Kind && CueLog.Last().Name == Name && Clock - CueLog.Last().At < 0.3) return;
	CueLog.Add(FCue{Kind, Name, Clock, Gain});
	UE_LOG(LogAutocraft, Log, TEXT("audio: cue %s %s at %.2f s gain %.3f"), *Kind, *Name, Clock, Gain);
}

void UAcMusicPlayer::StepSiege(const float Dt)
{
	// What the pilot is near: the zone in its region, else next door.
	std::optional<ac::SiegeMix::Near> Near;
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const FAcMmoClient* Mmo = Sim && Sim->IsMmo() ? Sim->MmoClient() : nullptr;
	FString What;
	if (Mmo)
	{
		if (const ac::net::Sky::Zone* Z = AcCanyon::Near(*Mmo))
		{
			ac::SiegeMix::Near N;
			N.state = int(Z->state);
			N.wave = Z->wave;
			N.waves = Z->waves;
			N.out = Z->ends.has_value() || Z->left > 0;
			if (const ac::net::Mirror* M = Mmo->Mirror())
				for (const ac::Unit& U : M->simulation().state.units)
					N.matriarch = N.matriarch || (U.kind == ac::UnitKind::matriarch && U.hp > 0);
			Near = N;
			const FAcZonePlace* P = AcCanyon::Place(*Mmo, *Z);
			What = FString::Printf(TEXT("%s, wave %lld/%lld"), P ? *P->Name : TEXT("a canyon"), (long long)Z->wave, (long long)Z->waves);
		}
	}
	Siege.step(Dt, Near);
	if (!What.IsEmpty()) SiegeWhat = What;

	// The beds, only with a world that has sound.
	UWorld* World = PlayWorld();
	const bool bSound = World && World->GetAudioDeviceRaw() && !bSilent;
	auto Bed = [&](TObjectPtr<UAudioComponent>& Layer, TObjectPtr<USoundBase>& Wave, const TCHAR* Id, const double Level, const float BedGain) {
		const float Gain = float(Level) * BedGain * AcSound::MusicGain() * AcSound::MasterGain() * (bMuted ? 0.f : 1.f);
		if (Level <= 1e-3)
		{
			if (Layer)
			{
				Layer->Stop();
				Layer = nullptr;
			}
			return;
		}
		if (!bSound) return;
		if (!Layer)
		{
			if (!Wave)
				if (UAcMp3Wave* W = UAcMp3Loader::LoadMp3(BedPath(Id), this))
				{
					W->bLoopFile = true;
					Wave = W;
				}
			if (!Wave) return;
			Layer = UGameplayStatics::CreateSound2D(World, Wave, Gain, 1.f, 0.f, nullptr, true, false);
			if (Layer) Layer->Play();
		}
		if (Layer) Layer->SetVolumeMultiplier(FMath::Max(Gain, 0.001f));
	};
	Bed(WaveLayer, WaveBed, TEXT("siege_wave"), Siege.wave, WaveBedGain);
	Bed(QueenLayer, QueenBed, TEXT("siege_matriarch"), Siege.matriarch, QueenBedGain);

	// The timeline: a layer's or the radio's level when it has moved 0.1 (and
	// at rest), the radio's in dB.
	const double Now[3] = {Siege.wave, Siege.matriarch, Siege.radio};
	static const TCHAR* const Names[3] = {TEXT("siege_wave"), TEXT("siege_matriarch"), TEXT("radio")};
	for (int32 K = 0; K < 3; ++K)
	{
		const double Rest = K == 2 ? 1.0 : 0.0;
		const bool bMoved = std::abs(Now[K] - SiegeLogged[K]) >= 0.1 || (Now[K] == Rest && SiegeLogged[K] != Rest)
			|| (K < 2 && Now[K] > 0 && SiegeLogged[K] == 0);
		if (!bMoved) continue;
		SiegeLogged[K] = Now[K];
		Cue(K == 2 ? TEXT("duck") : TEXT("layer"), K == 2 ? FString(Names[K]) : FString::Printf(TEXT("%s (%s)"), Names[K], *SiegeWhat), Now[K]);
	}
}
