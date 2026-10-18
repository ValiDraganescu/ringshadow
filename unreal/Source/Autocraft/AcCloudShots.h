// `-AcCloudShots=DIR`: a set of sky pictures in one run, every pairing of
// hour, weather, view and cloud look, one PNG each, then quit
// (docs/weather.md, "Clouds (2026-10-18)"). Our clouds and a reference's
// (`ac.Daylight.CloudReference`) under the same sun, sky and camera, so a
// round of cloud work costs one hidden run:
//
//   uv run unreal/Tools/hidden_run.py --out DIR --res 5120x1378 --
//     -AcPaused -AcCloudShots={out} -AcCloudShotsLooks=ours,engine
//
// -AcCloudShotsHours=12,17.5,18.3     the hours (held, `ac.Hour`).
// -AcCloudShotsWeathers=clear,cloudy,storm   (or `ac.Weather` numbers).
// -AcCloudShotsViews=sun,away         sun: toward the sun's azimuth; away:
//                                     the sun behind; side: 90° off it; a
//                                     number: that yaw (degrees, Unreal).
// -AcCloudShotsLooks=ours,engine      looks, `[label@]REFERENCE[+NAME=VALUE...]`:
//                                     an `ac.Daylight.CloudReference` value
//                                     (`ours` is the empty one) and console
//                                     variables set for that shot only, e.g.
//                                     `thin@ours+ac.Daylight.CloudParam=CloudB=:0.03`.
// -AcCloudShotsPitch=10 -AcCloudShotsFov=75 -AcCloudShotsHeight=2 (m over
//                                     the map's middle).
// -AcCloudShotsDelay=S               wait S seconds with the world up
//                                     before the first picture (an MMO
//                                     world's sky comes with its snapshots).
// -AcCloudShotsFrames=90              frames each picture settles (the
//                                     clouds' temporal rebuild, the sky
//                                     light's capture, TSR).
// -AcCloudShotsPan=DEG                each picture is a recording instead:
//                                     the camera turns DEG a second for
//                                     -AcCloudShotsSeconds (4), a PNG a
//                                     frame (NAME_00000.png...), to see the
//                                     clouds under camera motion; run it
//                                     with -UseFixedTimeStep -FPS=30 (or 60).
// Files: DIR/<hour>-<weather>-<view>-<look>.png. Use -AcPaused: the sim holds
// still (no AIs play) and only the sky moves.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "AcCloudShots.generated.h"

class ACameraActor;

UCLASS()
class AUTOCRAFT_API UAcCloudShots : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsCloudShotRun();

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FShot
	{
		double Hour = 12;
		double Weather = 0;
		FString WeatherName, View, Look;
		FString Name() const;
	};
	/// Sets the clock, the weather and the look, and aims the camera.
	void Stage(const FShot& Shot);
	FString FramePath(int32 Frame) const;
	/// A frame's game time: the fixed step (-UseFixedTimeStep -FPS=N), else 1/30.
	static double Step();
	void Finish(bool bOk);

	FString Dir;
	TArray<FShot> Shots;
	double Pitch = 10, Fov = 75, Height = 2, Pan = 0, Seconds = 4, Delay = 0;
	/// When the map and the sky were first up (wall clock), for the delay.
	double UpSince = -1;
	int32 SettleFrames = 90;
	int32 Index = -1;
	/// Frames into the current shot, and the frames of a pan recorded.
	int32 Frames = 0, Recorded = 0, Waited = 0;
	int64 LastSize = -1;
	/// The start's wait for the map and the sky to be up.
	int32 StartFrames = 0;
	double Yaw = 0;
	bool bDone = false;
	/// The console variables a look set, with the values to put back.
	TMap<FString, FString> Touched;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
