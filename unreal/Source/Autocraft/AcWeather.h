// `AAcWeather`: the weather (docs/weather.md) in the world. Owns the model
// (`ac::Weather`, a pure function of game time and a seed), the rain and the
// lightning's light; `AAcDaylight` reads `Sample` for the clouds, the light
// and the haze, the audio director for the rain bed and the thunder.
//
// - The rain: one procedural mesh of `RainStreaks` quads, written once.
//   Each quad's four corners sit on one random point of a box; the vertex
//   shader (`M_AcRain`, Tools/Editor/make_weather_materials.py) moves the
//   point with the wind and the fall, wraps it into a box ahead of the
//   camera, and opens it into a thin streak facing the camera (a pixel or
//   two wide at any distance). How many show follows the rain rate. One
//   draw call; the CPU only sets a few parameters a frame.
// - The lightning: a directional light without shadows, from above the
//   strike's side, flashing with `FAcWeatherSample::flash`; and each
//   strike's bolt, seen from the cockpit and the sky view (the RTS camera
//   never sees the horizon): a jagged channel from the cloud base to the
//   ground at the strike's distance and side, with two or three branches,
//   seeded from the strike; a camera-facing ribbon (`M_AcBolt`) rebuilt
//   each frame of the second it flickers (`ac::Weather::flicker`).
// - Rain on the glass while driving a unit in first person in the rain: a
//   post-process (`M_AcLensRain`) of drops that refract the view.
// - Wet hulls: `MPC_AcWorld.Wetness`, which M_Hull reads (darker,
//   glossier, top faces most), as wet as the ground.
// - No rain indoors (AcShelter.h): the Citadels' shelter boxes in
//   MPC_AcWorld, where the streaks, the hulls and the ground leave them
//   dry; the lens dries while the driven unit is sheltered.
//
// Console: `ac.Weather` (-1 the schedule; 0 clear, 1 cloudy, 2 rain,
// 3 storm; fractions blend), `ac.Weather.Rain`, `ac.Weather.Lightning`.
// Flags: `-AcWeather=clear|cloudy|rain|storm` (or a number),
// `-AcWeatherSeed=N` (else the seed is the game's id, so a saved
// game keeps its weather), `-AcWeatherT=S` (sample at S seconds of game time
// instead of the game's, for shots of a moment of the schedule),
// `-AcWeatherFrom=S -AcWeatherSpeed=X` (a time-lapse for clips: the schedule
// from S seconds on, X times as fast; the clouds race with it).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "AcShelter.h"

#include "Weather.h"

#include "AcWeather.generated.h"

class UDirectionalLightComponent;
class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UProceduralMeshComponent;

UCLASS()
class AUTOCRAFT_API AAcWeather : public AActor
{
	GENERATED_BODY()

public:
	/// Streaks in the rain box at the heaviest rain.
	static constexpr int32 RainStreaks = 20000;

	AAcWeather();

	static AAcWeather* SpawnFor(UWorld* World);
	static AAcWeather* Find(const UWorld* World);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/// The weather at `Time` seconds of game time (the same time gives the
	/// same sample; asked again for the same time, the last one).
	const ac::WeatherSample& Sample(double Time);
	/// The last sample (clear before the first).
	const ac::WeatherSample& Last() const { return Current; }
	/// Lightning strikes whose thunder arrives in [T0, T1).
	TArray<ac::LightningStrike> Thunder(double T0, double T1);
	/// The time the weather is sampled at for the game time `Time`.
	static double WeatherTime(double Time);
	/// The latest strike's direction (radians, world) and distance (km)
	/// from the camera: where the clouds light up.
	double LastStrikeAzimuth() const { return FlashAzimuth; }
	double LastStrikeKm() const { return FlashKm; }
	/// The lens drops' rate now (eased: 0 once the driven unit is sheltered).
	float LensRate() const { return LensNow; }
	/// The shelter boxes last written to MPC_AcWorld, nearest the camera first.
	const TArray<AcShelter::FBox>& Shelters() const { return ShelterBoxes; }

private:
	void BuildRain();
	void UpdateRain(double Time, float Light);
	void UpdateFlash();
	void UpdateBolts(double T, const FVector& Cam);
	void UpdateLens();
	void UpdateShelters(const FVector& Cam);
	void ApplyHold();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> Rain;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDirectionalLightComponent> Flash;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RainMaterial;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProceduralMeshComponent> BoltMesh;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BoltMaterial;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPostProcessComponent> Lens;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LensMaterial;
	/// MPC_AcWorld (its `Wetness`, read by M_Hull) and what it was last set to.
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialParameterCollection> WeatherMpc;
	float HullWet = -1.0f;
	/// The shelter boxes written to the MPC (none yet: `bSheltersWritten`).
	TArray<AcShelter::FBox> ShelterBoxes;
	bool bSheltersWritten = false;
	float LensNow = 0.0f;

	/// A strike's drawn bolt: its channel and branches (world cm), how bright.
	struct FBolt
	{
		double Time = 0, Power = 0;
		TArray<TArray<FVector>> Paths;
	};
	TArray<FBolt> Bolts;
	/// Strikes drawn up to this weather time.
	double BoltsTo = -1;
	bool bBoltsShown = false;

	TUniquePtr<ac::Weather> Model;
	/// The game the model is seeded from (its id; empty: not yet).
	std::string SeededFor;
	/// Seeded by the MMORPG world's sky (`AAcDaylight::WorldSky`).
	bool bWorldSeeded = false;
	ac::WeatherSample Current;
	double SampledAt = -1;
	float AppliedHold = -2;
	/// Where the latest strike was (for the flash's direction).
	double FlashAzimuth = 0;
	double FlashKm = 5;
};
