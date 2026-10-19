#include "AcTerrainLab.h"

#include "AcLog.h"
#include "AcPilotCamera.h"
#include "AcRtsPawn.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "ShaderCompiler.h"
#include "UnrealClient.h"

#include "RegionFile.h"
#include "TerrainField.h"

#include <cmath>

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

	void SetCVar(const TCHAR* Name, const FString& Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(*Value, ECVF_SetByCode);
		else UE_LOG(LogAutocraft, Error, TEXT("terrain lab: no console variable %s"), Name);
	}

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

	/// The cockpit's eye over the ground, cells (docs/terrain-lab.md: 1.3 m).
	constexpr double EyeCells = 1.3;
	/// The sim's ground point at the Prospector's id (no real unit has it).
	constexpr int64 LabUnitId = 990001;

	/// The spots the views stand on, found on the region's own field.
	struct FSpots
	{
		const ac::TerrainField& Field;
		const ac::MapDefinition& Map;

		double H(const ac::Vec2 P) const { return Field.height(P); }
		/// In play, clear of the walls and the map's edge by `Margin` cells.
		bool Inside(const ac::Vec2 P, const double Margin) const
		{
			const ac::GroundRect& B = Map.bounds;
			if (P.x < B.minX + Margin || P.x > B.maxX - Margin || P.y < B.minZ + Margin || P.y > B.maxZ - Margin) return false;
			return !Map.edges || Map.edges->outline.empty() || Field.landOutside(P) < -Margin;
		}
		/// The most the ground rises or falls within `R` cells of `P`.
		double Rough(const ac::Vec2 P, const double R) const
		{
			const double H0 = H(P);
			double Most = 0;
			for (int32 K = 0; K < 8; K++)
			{
				const double A = K * UE_DOUBLE_PI / 4;
				for (const double D : {R * 0.5, R})
					Most = FMath::Max(Most, std::abs(H(P + ac::Vec2(std::cos(A), std::sin(A)) * D) - H0));
			}
			return Most;
		}
		/// Nothing of the map's (a rock, ore, a well) within `R` cells.
		bool Clear(const ac::Vec2 P, const double R) const
		{
			for (const ac::Doodad& D : Map.doodads)
				if (ac::length(D.position - P) < R) return false;
			for (const ac::OreSpec& O : Map.deposits)
				if (ac::length(O.position - P) < R + 1) return false;
			for (const ac::WellSpec& W : Map.wells)
				if (ac::length(W.position - P) < R + 1) return false;
			return true;
		}
		/// Where the region starts: its first pad, else the middle.
		ac::Vec2 Anchor() const
		{
			if (!Map.plates.empty()) return Map.plates[0];
			const ac::GroundRect& B = Map.bounds;
			return ac::Vec2((B.minX + B.maxX) / 2, (B.minZ + B.maxZ) / 2);
		}
	};

	double YawOf(const ac::Vec2 D) { return FMath::RadiansToDegrees(std::atan2(D.y, D.x)); }
}

bool UAcTerrainLabShots::IsLabShotRun()
{
	FString Value;
	return FParse::Value(FCommandLine::Get(), TEXT("AcTerrainLabShots="), Value) && !Value.IsEmpty();
}

bool UAcTerrainLabShots::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game && IsLabShotRun();
}

TStatId UAcTerrainLabShots::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAcTerrainLabShots, STATGROUP_Tickables);
}

void UAcTerrainLabShots::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAcSimSubsystem>();
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("AcTerrainLabShots="), Dir);
	if (FPaths::IsRelative(Dir)) Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
	FParse::Value(Cmd, TEXT("AcTerrainLabFrames="), SettleFrames);
	SettleFrames = FMath::Max(SettleFrames, 10);

	for (const FString& Id : List(TEXT("AcTerrainLabRegions="), TEXT("start")))
	{
		FRegion R;
		R.Id = Id;
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("server/regions"), Id + TEXT(".json"));
		if (!FFileHelper::LoadFileToString(R.Text, *Path))
		{
			UE_LOG(LogAutocraft, Error, TEXT("terrain lab: no region file %s"), *Path);
			Failures += 1;
			continue;
		}
		Regions.Add(MoveTemp(R));
	}
	// One look since the lab was promoted (2026-10-19): the game's.
	const TArray<FString> Looks = {TEXT("game")};
	const TArray<FString> Views = List(TEXT("AcTerrainLabViews="), TEXT("feet,cliff,across,third,top"));
	const TArray<FString> Hours = List(TEXT("AcTerrainLabHours="), TEXT("9,17,23"));
	const TArray<FString> WeatherNames = List(TEXT("AcTerrainLabWeathers="), TEXT("clear,rain"));
	static const TMap<FString, double> Weathers = {
		{TEXT("clear"), 0.0}, {TEXT("cloudy"), 1.0}, {TEXT("rain"), 2.0}, {TEXT("storm"), 3.0}};
	// Taken a region and a look at a time, the views' camera moving most
	// often: the clock and the weather change least.
	for (int32 R = 0; R < Regions.Num(); R++)
		for (const FString& L : Looks)
			for (int32 H = 0; H < Hours.Num(); H++)
				for (int32 W = 0; W < WeatherNames.Num(); W++)
					for (int32 V = 0; V < Views.Num(); V++)
					{
						FShot S;
						S.Region = R;
						S.Look = L;
						S.View = Views[V];
						S.Hour = FCString::Atod(*Hours[H]);
						S.WeatherName = WeatherNames[W];
						const double* Named = Weathers.Find(WeatherNames[W]);
						S.Weather = Named ? *Named : FCString::Atod(*WeatherNames[W]);
						S.Number = (V * Hours.Num() + H) * WeatherNames.Num() + W + 1;
						Shots.Add(S);
					}
	UE_LOG(LogAutocraft, Log, TEXT("terrain lab: %d stills of %d regions to %s, %d frames each"), Shots.Num(), Regions.Num(), *Dir,
		SettleFrames);
}

void UAcTerrainLabShots::Deinitialize()
{
	Mirror.reset();
	Super::Deinitialize();
}

FString UAcTerrainLabShots::PathOf(const FShot& Shot) const
{
	return Dir / Regions[Shot.Region].Id / Shot.Look /
	       FString::Printf(TEXT("%02d-%s-%02d-%s.png"), Shot.Number, *Shot.View, FMath::RoundToInt32(Shot.Hour), *Shot.WeatherName);
}

bool UAcTerrainLabShots::ShowRegion(const int32 RegionIndex)
{
	const FRegion& R = Regions[RegionIndex];
	const ac::region::Parsed Parsed = ac::region::parse(TCHAR_TO_UTF8(*R.Text));
	if (!Parsed.file)
	{
		UE_LOG(LogAutocraft, Error, TEXT("terrain lab: %s does not parse"), *R.Id);
		return false;
	}
	ac::net::RegionDescription D;
	D.regionFile = TCHAR_TO_UTF8(*R.Text);
	D.wild = true;
	D.fog = false;
	auto Next = std::make_unique<ac::net::Mirror>(D);
	ac::Simulation& Sim = Next->simulation();
	const ac::MapDefinition& Map = *Sim.map;
	const ac::TerrainField Field(Map);
	const FSpots Spots{Field, Map};
	const ac::Vec2 Anchor = Spots.Anchor();
	Poses.Reset();
	FString Notes = FString::Printf(TEXT("%s (%s), anchor %.1f, %.1f\n"), *R.Id, *Text(Map.name), Anchor.x, Anchor.y);
	const auto Eye = [&](const ac::Vec2 P, const ac::Vec2 Look, const double Pitch) {
		FPose Pose;
		Pose.At = AcSpace::ToWorld(P, Spots.H(P) + EyeCells);
		Pose.Rotation = FRotator(Pitch, YawOf(Look), 0);
		Pose.bFound = true;
		return Pose;
	};
	const auto Note = [&](const TCHAR* View, const FPose& P, const ac::Vec2 Ground) {
		Poses.Add(View, P);
		Notes += FString::Printf(TEXT("%s: %s at %.1f, %.1f, yaw %.0f, pitch %.0f\n"), View, P.bFound ? TEXT("found") : TEXT("NOT FOUND"),
			Ground.x, Ground.y, P.Rotation.Yaw, P.Rotation.Pitch);
	};

	// feet: the nearest flat, clear ground out past the pad's plate,
	// looking on outward.
	{
		FPose Pose;
		ac::Vec2 Where = Anchor;
		for (double Rad = ac::region::padReach + 4; Rad < 60 && !Pose.bFound; Rad += 1.5)
			for (int32 K = 0; K < 24 && !Pose.bFound; K++)
			{
				const double A = K * UE_DOUBLE_PI / 12;
				const ac::Vec2 Way(std::cos(A), std::sin(A));
				const ac::Vec2 P = Anchor + Way * Rad;
				if (!Spots.Inside(P, 6) || Spots.Rough(P, 4) > 0.06 || !Spots.Clear(P, 3)) continue;
				// The ground it looks at (2-6 cells ahead) flat too.
				if (Spots.Rough(P + Way * 4, 2) > 0.06) continue;
				Pose = Eye(P, Way, -20);
				Where = P;
			}
		Note(TEXT("feet"), Pose, Where);
	}
	// cliff: flat ground whose ground 3 cells on rises most of a level, the
	// face starting a cell or more away; the nearest to the anchor.
	{
		FPose Pose;
		ac::Vec2 Where = Anchor;
		double Best = TNumericLimits<double>::Max();
		const ac::GroundRect& B = Map.bounds;
		const double Level = Map.levelHeight > 0 ? Map.levelHeight : 1.0;
		for (double Y = B.minZ + 4; Y < B.maxZ - 4; Y += 2)
			for (double X = B.minX + 4; X < B.maxX - 4; X += 2)
			{
				const ac::Vec2 P(X, Y);
				const double Dist = ac::length(P - Anchor);
				if (Dist >= Best || !Spots.Inside(P, 4) || Spots.Rough(P, 1) > 0.1) continue;
				for (int32 K = 0; K < 8; K++)
				{
					const double A = K * UE_DOUBLE_PI / 4;
					const ac::Vec2 Way(std::cos(A), std::sin(A));
					const double H0 = Spots.H(P);
					if (Spots.H(P + Way * 1.0) - H0 > 0.3 || Spots.H(P + Way * 3.5) - H0 < 0.8 * Level) continue;
					Best = Dist;
					Where = P;
					// Back from it a cell, looking up the face.
					Pose = Eye(P - Way, Way, 18);
					break;
				}
			}
		Note(TEXT("cliff"), Pose, Where);
	}
	// across: the highest flat ground in play, looking toward the side of
	// the region furthest from it.
	{
		FPose Pose;
		ac::Vec2 Where = Anchor;
		double Top = -TNumericLimits<double>::Max();
		const ac::GroundRect& B = Map.bounds;
		for (double Y = B.minZ + 8; Y < B.maxZ - 8; Y += 2)
			for (double X = B.minX + 8; X < B.maxX - 8; X += 2)
			{
				const ac::Vec2 P(X, Y);
				const double H = Spots.H(P);
				if (H <= Top + 0.05 || !Spots.Inside(P, 8) || Spots.Rough(P, 2) > 0.15) continue;
				// Not in a rock: a spire's or a boulder's mesh stands over
				// the field's ground.
				bool bRock = false;
				for (const ac::Doodad& Dd : Map.doodads) bRock = bRock || ac::distance(Dd.position, P) < 2.5 + 1.5 * Dd.scale;
				if (bRock) continue;
				Top = H;
				Where = P;
			}
		if (Top > -TNumericLimits<double>::Max())
		{
			// The way the eye sees furthest over the ground (open land
			// since the world sculpting: a butte close by would fill the
			// still), the longer way on a tie.
			const double Eye0 = Spots.H(Where) + 1.3;
			ac::Vec2 Way(1, 0);
			double Best = -1;
			for (int32 K = 0; K < 32; K++)
			{
				const double A = K * 2 * UE_DOUBLE_PI / 32;
				const ac::Vec2 Dir2(std::cos(A), std::sin(A));
				double Seen = 0;
				for (double T = 1; T <= 90; T += 0.5)
				{
					const ac::Vec2 Q = Where + Dir2 * T;
					if (Q.x < B.minX || Q.x > B.maxX || Q.y < B.minZ || Q.y > B.maxZ) break;
					if (Spots.H(Q) - Eye0 > T * 0.035) break;
					Seen = T;
				}
				if (Seen > Best + 0.5) { Best = Seen; Way = Dir2; }
			}
			Pose = Eye(Where, Way, -7);
		}
		Note(TEXT("across"), Pose, Where);
	}
	// third: a Prospector half way up the ramp nearest the anchor, driving
	// up it, the third-person camera behind (AcPilotChase: the boom
	// (2.6 + 3.3 R) long, R about 1, at the pilot's starting pitch).
	{
		FPose Pose;
		ac::Vec2 Where = Anchor;
		// The map's ramps and a region file's (its ramp edits: low end, high end).
		TArray<TPair<ac::Vec2, ac::Vec2>> Ramps;
		for (const ac::Ramp& Ramp : Map.ramps) Ramps.Add({Ramp.low, Ramp.high});
		for (const ac::HeightEdit& E : Map.heightEdits)
			if (E.kind == ac::HeightEdit::Kind::ramp && E.points.size() >= 2) Ramps.Add({E.points[0], E.points[1]});
		const TPair<ac::Vec2, ac::Vec2>* Best = nullptr;
		for (const TPair<ac::Vec2, ac::Vec2>& Ramp : Ramps)
		{
			const ac::Vec2 Mid = (Ramp.Key + Ramp.Value) * 0.5;
			if (!Spots.Inside(Mid, 4)) continue;
			if (!Best || ac::length(Mid - Anchor) < ac::length((Best->Key + Best->Value) * 0.5 - Anchor)) Best = &Ramp;
		}
		if (Best)
		{
			Where = (Best->Key + Best->Value) * 0.5;
			const ac::Vec2 Up = ac::normalize(Best->Value - Best->Key);
			const double Heading = std::atan2(Up.y, Up.x);
			if (!Sim.state.players.empty())
			{
				Sim.state.units.push_back(Sim.freshUnit(ac::Unit::Kind::prospector, LabUnitId, 0, Where, Heading));
			}
			float Lift = 0, Length = 1;
			FAcPilotCamera::Boom(ac::UnitKind::prospector, Lift, Length);
			const double Pitch = FAcPilotCamera::StartPitch(ac::UnitKind::prospector);
			const FVector Ahead(Up.x, Up.y, 0);
			const FVector Look = Ahead * std::cos(Pitch) + FVector(0, 0, std::sin(Pitch));
			const FVector Pivot = AcSpace::ToWorld(Where, Spots.H(Where) + 1.0 + 0.25 + Lift);
			Pose.At = Pivot - Look * AcSpace::ToCm(5.9 * Length);
			Pose.Rotation = Look.Rotation();
			Pose.bFound = true;
		}
		Note(TEXT("third"), Pose, Where);
	}
	// top: the RTS camera at the game's zoom over the well nearest the
	// anchor (else the anchor).
	{
		FPose Pose;
		Pose.bRts = true;
		Pose.bFound = true;
		Pose.Over = Anchor;
		for (const ac::WellSpec& W : Map.wells)
			if (Pose.Over == Anchor || ac::length(W.position - Anchor) < ac::length(Pose.Over - Anchor)) Pose.Over = W.position;
		Note(TEXT("top"), Pose, Pose.Over);
	}

	UAcSimSubsystem* SimSys = GetWorld()->GetSubsystem<UAcSimSubsystem>();
	SimSys->ShowStill(&Sim, ac::MapChoice{Parsed.file->style, Parsed.file->size, 2});
	Mirror = std::move(Next);
	if (AAcTerrain* Terrain = AAcTerrain::Find(GetWorld()))
	{
		AAcTerrain::FGroundOverride Over;
		Over.Own = Parsed.file->ground;
		Terrain->SetGroundOverride(Over);
	}
	Shown = RegionIndex;
	IFileManager::Get().MakeDirectory(*(Dir / R.Id), true);
	FFileHelper::SaveStringToFile(Notes, *(Dir / R.Id / TEXT("views.txt")));
	UE_LOG(LogAutocraft, Log, TEXT("terrain lab: showing %s\n%s"), *R.Id, *Notes);
	return true;
}

void UAcTerrainLabShots::Stage(const FShot& Shot)
{
	const FString Path = PathOf(Shot);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	// A file left from an earlier run must not pass for this one's.
	IFileManager::Get().Delete(*Path, false, true, true);
	SetCVar(TEXT("ac.Hour"), FString::SanitizeFloat(Shot.Hour));
	SetCVar(TEXT("ac.Weather"), FString::SanitizeFloat(Shot.Weather));
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	const FPose* Pose = Poses.Find(Shot.View);
	if (!Pose || !Pose->bFound)
	{
		UE_LOG(LogAutocraft, Warning, TEXT("terrain lab: %s has no %s view; the still is skipped"), *Regions[Shot.Region].Id, *Shot.View);
		return;
	}
	if (Pose->bRts)
	{
		if (AAcRtsPawn* Rts = AAcRtsPawn::Get(World))
		{
			Rts->SetCover(AAcRtsPawn::DefaultCover);
			Rts->CenterOn(Pose->Over);
			if (PC && PC->GetViewTarget() != Rts) PC->SetViewTarget(Rts);
		}
		else UE_LOG(LogAutocraft, Error, TEXT("terrain lab: no RTS camera for the top view"));
	}
	else
	{
		if (!Camera)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Camera = World->SpawnActor<ACameraActor>(Pose->At, Pose->Rotation, Params);
			Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		}
		const FIntPoint Size = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ? GEngine->GameViewport->Viewport->GetSizeXY()
		                                                                                            : FIntPoint(1280, 720);
		Camera->GetCameraComponent()->SetFieldOfView(float(FAcPilotCamera::HorizontalFov(double(Size.X) / FMath::Max(Size.Y, 1))));
		Camera->SetActorLocationAndRotation(Pose->At, Pose->Rotation);
		if (PC && PC->GetViewTarget() != Camera) PC->SetViewTarget(Camera);
	}
	UE_LOG(LogAutocraft, Log, TEXT("terrain lab: %d/%d %s"), Index + 1, Shots.Num(), *Path);
}

void UAcTerrainLabShots::Finish(const bool bOk)
{
	bDone = true;
	UE_LOG(LogAutocraft, Display, TEXT("terrain lab: done, %d stills, %d failed"), Shots.Num(), Failures);
	if (bOk && Failures == 0) FPlatformMisc::RequestExit(false, TEXT("AcTerrainLabShots"));
	else FPlatformMisc::RequestExitWithStatus(false, 1, TEXT("AcTerrainLabShots"));
}

void UAcTerrainLabShots::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDone) return;
	UAcSimSubsystem* Sim = GetWorld()->GetSubsystem<UAcSimSubsystem>();
	if (Index < 0)
	{
		// The game's own map has come up and settled once: the regions next.
		if (++StartFrames < 60 || !Sim || !Sim->IsRunning() || !AAcTerrain::Find(GetWorld()))
		{
			if (StartFrames > 3000)
			{
				UE_LOG(LogAutocraft, Error, TEXT("terrain lab: the map never came up"));
				Finish(false);
			}
			return;
		}
		if (Shots.IsEmpty())
		{
			Finish(false);
			return;
		}
		Index = 0;
		Frames = 0;
		if (!ShowRegion(Shots[0].Region)) Failures += 1;
		Stage(Shots[0]);
		return;
	}
	// New shaders (the lab's material the first time) compile before a
	// still counts its frames.
	if (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
	{
		Frames = 0;
		return;
	}
	const FShot& Shot = Shots[Index];
	const FPose* Pose = Poses.Find(Shot.View);
	const bool bSkip = !Pose || !Pose->bFound;
	++Frames;
	if (!bSkip && Frames < SettleFrames) return;
	const FString Path = PathOf(Shot);
	if (!bSkip && Frames == SettleFrames)
	{
		FScreenshotRequest::RequestScreenshot(Path, false, /*bAddFilenameSuffix*/ false);
		return;
	}
	// Written once its size holds still (the image writer is async).
	const int64 Size = bSkip ? 0 : IFileManager::Get().FileSize(*Path);
	if (bSkip || (Size > 0 && Size == LastSize))
	{
		if (!bSkip) UE_LOG(LogAutocraft, Display, TEXT("terrain lab: saved %s"), *Path);
		LastSize = -1;
		Waited = 0;
		if (++Index >= Shots.Num())
		{
			Finish(true);
			return;
		}
		Frames = 0;
		if (Shots[Index].Region != Shown && !ShowRegion(Shots[Index].Region)) Failures += 1;
		Stage(Shots[Index]);
		return;
	}
	LastSize = Size;
	if (++Waited > 600)
	{
		UE_LOG(LogAutocraft, Error, TEXT("terrain lab: %s was never written"), *Path);
		Failures += 1;
		Finish(false);
	}
}
