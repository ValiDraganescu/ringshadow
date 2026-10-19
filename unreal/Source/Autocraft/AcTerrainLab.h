// `-AcTerrainLabShots=DIR`: the terrain lab's sheet (docs/terrain-lab.md
// section 3). For each region file asked for, the ground is built as a
// client builds it (`ac::net::Mirror`: its terrain, rocks, ore and wells,
// no server, no fog), a Prospector is stood on a ramp, and every pairing of
// look, view, hour and weather is saved as a PNG, then the run quits:
//
//   uv run unreal/Tools/hidden_run.py --out DIR --res 1280x720 --
//     -AcMap=highlands-small -AcTerrainLabShots={out}
//
// DIR/<region>/<look>/NN-<view>-<hour>-<weather>.png, NN counting the
// sheet's stills in the order of the views, then the hours, then the
// weathers (the same NN in every run, so two runs pair up).
//
// The views (each found on the region's own ground, the same each run):
//   feet    the cockpit's eye 1.3 cells up, 20° down at flat ground near
//           the pad, looking away from it
//   cliff   the cockpit at a cliff's foot, looking up the face
//   across  the cockpit on the region's highest ground, toward its far side
//   third   the third-person camera behind a Prospector on a ramp
//   top     the RTS camera at the game's zoom over a well (else the pad)
//
// -AcTerrainLabRegions=start,brine-pans   region ids (server/regions; start)
// (one look: the game's; the lab's own was promoted into it, 2026-10-19)
// -AcTerrainLabViews=feet,cliff,across,third,top
// -AcTerrainLabHours=9,17,23   -AcTerrainLabWeathers=clear,rain
// -AcTerrainLabFrames=90       frames each still settles
//
// It also writes DIR/<region>/views.txt: where each view stands, so a
// round's stills can be told apart from a moved camera.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "NetMirror.h"
#include "Types.h"

#include <memory>

#include "AcTerrainLab.generated.h"

class ACameraActor;

UCLASS()
class AUTOCRAFT_API UAcTerrainLabShots : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsLabShotRun();

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	struct FShot
	{
		int32 Region = 0;
		FString Look, View, WeatherName;
		double Hour = 12, Weather = 0;
		/// The still's number in its sheet (1-based).
		int32 Number = 0;
	};
	/// A view's camera: where it stands and looks (Unreal, the world frame
	/// of the region shown), or the RTS camera over a ground point.
	struct FPose
	{
		FVector At = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		bool bRts = false;
		ac::Vec2 Over{0, 0};
		bool bFound = false;
	};
	struct FRegion
	{
		FString Id;
		FString Text;
	};

	/// Shows region `Index`: its mirror, its palette, the Prospector on the
	/// ramp, the views found.
	bool ShowRegion(int32 Index);
	/// Clock, weather, look and camera for a still.
	void Stage(const FShot& Shot);
	FString PathOf(const FShot& Shot) const;
	void Finish(bool bOk);

	FString Dir;
	TArray<FRegion> Regions;
	TArray<FShot> Shots;
	TMap<FString, FPose> Poses;
	int32 SettleFrames = 90;
	int32 Index = -1;
	int32 Shown = -1;
	int32 Frames = 0, Waited = 0, StartFrames = 0;
	int64 LastSize = -1;
	int32 Failures = 0;
	bool bDone = false;
	std::unique_ptr<ac::net::Mirror> Mirror;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
