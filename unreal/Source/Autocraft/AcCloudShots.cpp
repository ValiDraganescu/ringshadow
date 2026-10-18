#include "AcCloudShots.h"

#include "AcDaylight.h"
#include "AcLog.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
	TArray<FString> List(const TCHAR* Flag, const TCHAR* Default)
	{
		FString Value = Default;
		FParse::Value(FCommandLine::Get(), Flag, Value, /*bShouldStopOnSeparator*/ false);
		TArray<FString> Out;
		Value.ParseIntoArray(Out, TEXT(","));
		return Out;
	}

	double Number(const TCHAR* Flag, double Default)
	{
		float V = 0;
		return FParse::Value(FCommandLine::Get(), Flag, V) ? double(V) : Default;
	}

	void SetCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(*Value, ECVF_SetByCode);
		else UE_LOG(LogAutocraft, Error, TEXT("cloud shots: no console variable %s"), Name);
	}
}

double UAcCloudShots::Step()
{
	return FApp::UseFixedTimeStep() && FApp::GetFixedDeltaTime() > 0 ? FApp::GetFixedDeltaTime() : 1.0 / 30;
}

bool UAcCloudShots::IsCloudShotRun()
{
	FString Value;
	return FParse::Value(FCommandLine::Get(), TEXT("AcCloudShots="), Value);
}

bool UAcCloudShots::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game && IsCloudShotRun();
}

FString UAcCloudShots::FShot::Name() const
{
	FString HourName = FString::Printf(TEXT("%05.2f"), Hour).Replace(TEXT("."), TEXT("h"));
	// `label@look+...` names itself; a plain reference by its asset's name.
	FString Label, Rest = Look;
	if (!Look.Split(TEXT("@"), &Label, &Rest))
	{
		Rest.Split(TEXT("+"), &Label, nullptr);
		if (Label.IsEmpty()) Label = Rest;
		Label = Label.IsEmpty() || Label == TEXT("ours") ? FString(TEXT("ours")) : FPaths::GetBaseFilename(Label);
	}
	return FString::Printf(TEXT("%s-%s-%s-%s"), *HourName, *WeatherName, *View, *Label);
}

void UAcCloudShots::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FParse::Value(FCommandLine::Get(), TEXT("AcCloudShots="), Dir);
	if (FPaths::IsRelative(Dir)) Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
	IFileManager::Get().MakeDirectory(*Dir, true);
	Pitch = Number(TEXT("AcCloudShotsPitch="), Pitch);
	Fov = Number(TEXT("AcCloudShotsFov="), Fov);
	Height = Number(TEXT("AcCloudShotsHeight="), Height);
	Pan = Number(TEXT("AcCloudShotsPan="), Pan);
	Seconds = Number(TEXT("AcCloudShotsSeconds="), Seconds);
	SettleFrames = int32(Number(TEXT("AcCloudShotsFrames="), SettleFrames));
	Delay = Number(TEXT("AcCloudShotsDelay="), Delay);
	static const TMap<FString, double> Weathers = {
		{TEXT("clear"), 0.0}, {TEXT("cloudy"), 1.0}, {TEXT("rain"), 2.0}, {TEXT("storm"), 3.0}};
	for (const FString& H : List(TEXT("AcCloudShotsHours="), TEXT("12,17.5,18.3")))
		for (const FString& W : List(TEXT("AcCloudShotsWeathers="), TEXT("clear,cloudy,storm")))
			for (const FString& V : List(TEXT("AcCloudShotsViews="), TEXT("sun,away")))
				for (const FString& L : List(TEXT("AcCloudShotsLooks="), TEXT("ours,engine")))
				{
					FShot S;
					S.Hour = FCString::Atod(*H);
					const double* Named = Weathers.Find(W);
					S.Weather = Named ? *Named : FCString::Atod(*W);
					S.WeatherName = W;
					S.View = V;
					S.Look = L;
					Shots.Add(S);
				}
	UE_LOG(LogAutocraft, Log, TEXT("cloud shots: %d to %s, %d frames each%s"), Shots.Num(), *Dir, SettleFrames,
		Pan != 0 ? *FString::Printf(TEXT(", panning %.1f°/s for %.1f s"), Pan, Seconds) : TEXT(""));
}

TStatId UAcCloudShots::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAcCloudShots, STATGROUP_Tickables);
}

FString UAcCloudShots::FramePath(const int32 Frame) const
{
	const FString Base = Dir / Shots[Index].Name();
	return Frame < 0 ? Base + TEXT(".png") : FString::Printf(TEXT("%s_%05d.png"), *Base, Frame);
}

void UAcCloudShots::Stage(const FShot& Shot)
{
	// A file left from an earlier run must not pass for this one's.
	IFileManager::Get().Delete(*FramePath(-1), false, true, true);
	for (int32 I = 0; Pan != 0 && I < FMath::RoundToInt32(Seconds / Step()); ++I) IFileManager::Get().Delete(*FramePath(I), false, true, true);
	SetCVar(TEXT("ac.Hour"), FString::SanitizeFloat(Shot.Hour));
	SetCVar(TEXT("ac.Weather"), FString::SanitizeFloat(Shot.Weather));
	// The look: a reference (`ours`: none) and console variables to set,
	// `[label@]REFERENCE[+NAME=VALUE...]`; the last shot's go back first.
	for (const TPair<FString, FString>& Old : Touched) SetCVar(*Old.Key, Old.Value);
	Touched.Reset();
	FString Look = Shot.Look;
	Look.Split(TEXT("@"), nullptr, &Look);
	TArray<FString> Parts;
	Look.ParseIntoArray(Parts, TEXT("+"));
	const FString Reference = Parts.IsEmpty() || Parts[0] == TEXT("ours") ? FString() : Parts[0];
	SetCVar(TEXT("ac.Daylight.CloudReference"), Reference);
	for (int32 I = 1; I < Parts.Num(); ++I)
	{
		FString Name, Value;
		if (!Parts[I].Split(TEXT("="), &Name, &Value)) continue;
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(*Name)) Touched.Add(Name, V->GetString());
		SetCVar(*Name, Value);
	}
	UWorld* World = GetWorld();
	FVector At = FVector::ZeroVector;
	if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(World); Sim && Sim->IsRunning())
	{
		const ac::GroundRect& B = Sim->Map().bounds;
		At = AcSpace::ToWorld(ac::Vec2((B.minX + B.maxX) / 2, (B.minZ + B.maxZ) / 2), 0);
	}
	if (const AAcTerrain* Terrain = AAcTerrain::Find(World); Terrain && Terrain->IsBuilt()) At.Z = Terrain->GroundZ(At);
	At.Z += Height * 100;
	const double SunYaw = AAcDaylight::SunToward(Shot.Hour).Rotation().Yaw;
	if (Shot.View == TEXT("sun")) Yaw = SunYaw;
	else if (Shot.View == TEXT("away")) Yaw = SunYaw + 180;
	else if (Shot.View == TEXT("side")) Yaw = SunYaw + 90;
	else Yaw = FCString::Atod(*Shot.View);
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ACameraActor>(At, FRotator::ZeroRotator, Params);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	}
	Camera->GetCameraComponent()->SetFieldOfView(float(Fov));
	Camera->SetActorLocationAndRotation(At, FRotator(Pitch, Yaw, 0));
	if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->GetViewTarget() != Camera)
	{
		PC->SetViewTarget(Camera);
	}
	UE_LOG(LogAutocraft, Log, TEXT("cloud shots: %d/%d %s (yaw %.0f, the sun's %.0f)"), Index + 1, Shots.Num(), *Shot.Name(), Yaw, SunYaw);
}

void UAcCloudShots::Finish(const bool bOk)
{
	bDone = true;
	SetCVar(TEXT("ac.Daylight.CloudReference"), FString());
	if (bOk) FPlatformMisc::RequestExit(false, TEXT("AcCloudShots"));
	else FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("AcCloudShots"));
}

void UAcCloudShots::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDone) return;
	if (Index < 0)
	{
		// The map, the terrain and the sky are up.
		const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(GetWorld());
		const AAcTerrain* Terrain = AAcTerrain::Find(GetWorld());
		if (++StartFrames < 60 || !Sim || !Sim->IsRunning() || !Terrain || !Terrain->IsBuilt() || !AAcDaylight::Find(GetWorld()))
		{
			UpSince = -1;
			if (StartFrames > 3000)
			{
				UE_LOG(LogAutocraft, Error, TEXT("cloud shots: the map never came up"));
				Finish(false);
			}
			return;
		}
		// -AcCloudShotsDelay=S: the world up S seconds first (an MMO world's
		// sky, its weather and the wet ground settle after it joins).
		if (UpSince < 0) UpSince = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - UpSince < Delay) return;
		if (Shots.IsEmpty())
		{
			Finish(false);
			return;
		}
		Index = 0;
		Frames = 0;
		Stage(Shots[0]);
		return;
	}
	++Frames;
	const int32 RecordFrames = Pan != 0 ? FMath::Max(1, FMath::RoundToInt32(Seconds / Step())) : 0;
	if (Frames < SettleFrames) return;
	if (RecordFrames > 0 && Recorded < RecordFrames)
	{
		// The pan turns from the shot's yaw, a frame at a time.
		Camera->SetActorRotation(FRotator(Pitch, Yaw + Pan * Recorded * Step(), 0));
		FScreenshotRequest::RequestScreenshot(FramePath(Recorded), false, /*bAddFilenameSuffix*/ false);
		++Recorded;
		return;
	}
	if (Frames == SettleFrames && RecordFrames == 0)
	{
		FScreenshotRequest::RequestScreenshot(FramePath(-1), false, /*bAddFilenameSuffix*/ false);
		return;
	}
	// Written once its size holds still (the image writer is async).
	const FString Last = FramePath(RecordFrames > 0 ? RecordFrames - 1 : -1);
	const int64 Size = IFileManager::Get().FileSize(*Last);
	if (Size > 0 && Size == LastSize)
	{
		UE_LOG(LogAutocraft, Display, TEXT("cloud shots: saved %s"), *Last);
		LastSize = -1;
		Waited = Recorded = 0;
		if (++Index >= Shots.Num())
		{
			Finish(true);
			return;
		}
		Frames = 0;
		Stage(Shots[Index]);
		return;
	}
	LastSize = Size;
	if (++Waited > 600)
	{
		UE_LOG(LogAutocraft, Error, TEXT("cloud shots: %s was never written"), *Last);
		Finish(false);
	}
}
