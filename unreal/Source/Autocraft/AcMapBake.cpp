#include "AcMapBake.h"

#include "AcDaylight.h"
#include "AcLog.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Dom/JsonObject.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"

#include "RegionFile.h"
#include "TerrainField.h"

namespace
{
	/// Frames a region shows before its picture: the meshes, the ground's
	/// textures and the light settle.
	constexpr int32 SettleFrames = 90;
	/// The camera's height over the ground, cm (the ground is a few metres high).
	constexpr double CameraHeight = 200000.0;

	FString Text(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

bool UAcMapBakeSubsystem::IsBakeRun()
{
	FString Dir;
	return FParse::Value(FCommandLine::Get(), TEXT("AcMapBake="), Dir) && !Dir.IsEmpty();
}

bool UAcMapBakeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game && IsBakeRun();
}

TStatId UAcMapBakeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAcMapBakeSubsystem, STATGROUP_Tickables);
}

void UAcMapBakeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAcSimSubsystem>();
	const TCHAR* Cmd = FCommandLine::Get();
	FParse::Value(Cmd, TEXT("AcMapBake="), Dir);
	Dir = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Dir);
	FParse::Value(Cmd, TEXT("AcMapBakePx="), PixelsPerCell);
	PixelsPerCell = FMath::Clamp(PixelsPerCell, 1, 32);
	FParse::Value(Cmd, TEXT("AcMapBakeHour="), Hour);
	FString WorldPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("server/worlds/first.json"));
	if (FString Given; FParse::Value(Cmd, TEXT("AcMapBakeWorld="), Given)) WorldPath = FPaths::ConvertRelativePathToFull(FPaths::LaunchDir(), Given);
	TArray<FString> Only;
	if (FString List; FParse::Value(Cmd, TEXT("AcMapBakeOnly="), List)) List.ParseIntoArray(Only, TEXT(","));

	// The world's grid: every region file in it.
	FString Json;
	TSharedPtr<FJsonObject> World;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *WorldPath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), World) || !World
		|| !World->TryGetArrayField(TEXT("grid"), Rows))
	{
		UE_LOG(LogAutocraft, Error, TEXT("mapbake: no world at %s"), *WorldPath);
		Failures += 1;
		return;
	}
	int32 RowAt = 0;
	for (const TSharedPtr<FJsonValue>& Row : *Rows)
	{
		int32 ColumnAt = -1;
		for (const TSharedPtr<FJsonValue>& Entry : Row->AsArray())
		{
			ColumnAt += 1;
			FString File;
			if (!Entry->TryGetString(File)) continue;
			FRegion R;
			if (!FFileHelper::LoadFileToString(R.Text, *FPaths::Combine(FPaths::GetPath(WorldPath), File))) continue;
			const ac::region::Parsed Parsed = ac::region::parse(TCHAR_TO_UTF8(*R.Text));
			if (!Parsed.file)
			{
				UE_LOG(LogAutocraft, Error, TEXT("mapbake: %s does not parse"), *File);
				Failures += 1;
				continue;
			}
			R.Id = Text(Parsed.file->id);
			R.Choice = ac::MapChoice{Parsed.file->style, Parsed.file->size, 2};
			R.Ground = Parsed.file->ground;
			R.Column = ColumnAt;
			R.Row = RowAt;
			All.Add(R);
			if (Only.Num() > 0 && !Only.Contains(R.Id)) continue;
			Regions.Add(MoveTemp(R));
		}
		RowAt += 1;
	}
	UE_LOG(LogAutocraft, Log, TEXT("mapbake: %d regions of %s to %s at %d px a cell, hour %.1f"), Regions.Num(), *WorldPath, *Dir,
		PixelsPerCell, Hour);
}

void UAcMapBakeSubsystem::Deinitialize()
{
	AAcDaylight::SetWorldSky(NullOpt);
	Mirror.reset();
	Super::Deinitialize();
}

void UAcMapBakeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bDone) return;
	UAcSimSubsystem* Sim = GetWorld()->GetSubsystem<UAcSimSubsystem>();
	if (!Sim || !Sim->IsRunning()) return;
	// The world's day, held, in clear weather.
	FAcWorldSky Sky;
	Sky.HeldHour = Hour;
	Sky.WeatherHold = 0;
	AAcDaylight::SetWorldSky(Sky);
	Frames += 1;
	if (Current == INDEX_NONE)
	{
		// The game's own map has settled once: the first region next.
		if (Frames < SettleFrames) return;
		Show(0);
		return;
	}
	if (Current >= Regions.Num())
	{
		bDone = true;
		UE_LOG(LogAutocraft, Display, TEXT("mapbake: done, %d regions, %d failed"), Regions.Num(), Failures);
		FPlatformMisc::RequestExit(false, TEXT("AcMapBake"));
		return;
	}
	if (Frames < SettleFrames) return;
	if (!Capture(Current)) Failures += 1;
	Show(Current + 1);
}

void UAcMapBakeSubsystem::Show(const int32 Index)
{
	Current = Index;
	Frames = 0;
	if (!Regions.IsValidIndex(Index)) return;
	UAcSimSubsystem* Sim = GetWorld()->GetSubsystem<UAcSimSubsystem>();
	// The region as a client builds it: its ground, its ore and wells, no
	// units, no fog.
	ac::net::RegionDescription D;
	D.regionFile = TCHAR_TO_UTF8(*Regions[Index].Text);
	D.wild = true;
	D.fog = false;
	auto Next = std::make_unique<ac::net::Mirror>(D);
	Sim->ShowStill(&Next->simulation(), Regions[Index].Choice);
	Mirror = std::move(Next);
	// Its own palette, and its neighbours' toward the edges.
	if (AAcTerrain* Terrain = AAcTerrain::Find(GetWorld()))
	{
		AAcTerrain::FGroundOverride Over;
		Over.Own = Regions[Index].Ground;
		const int32 Step[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
		for (int32 E = 0; E < 4; E++)
			for (const FRegion& O : All)
				if (O.Column == Regions[Index].Column + Step[E][0] && O.Row == Regions[Index].Row + Step[E][1]) Over.Around[E] = O.Ground;
		Terrain->SetGroundOverride(Over);
	}
	UE_LOG(LogAutocraft, Log, TEXT("mapbake: showing %s"), *Regions[Index].Id);
}

bool UAcMapBakeSubsystem::Capture(const int32 Index)
{
	const FRegion& R = Regions[Index];
	const ac::MapDefinition& Map = *Mirror->simulation().map;
	const ac::GroundRect& B = Map.bounds;
	const int32 W = FMath::RoundToInt(B.width() * PixelsPerCell), H = FMath::RoundToInt(B.depth() * PixelsPerCell);
	UWorld* World = GetWorld();
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity, Params);
		USceneCaptureComponent2D* C = Camera->GetCaptureComponent2D();
		C->ProjectionType = ECameraProjectionMode::Orthographic;
		C->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		C->bCaptureEveryFrame = false;
		C->bCaptureOnMovement = false;
		C->bAlwaysPersistRenderingState = true;
		// The ground only: no fog over it, no motion blur, no lens.
		C->ShowFlags.SetFog(false);
		C->ShowFlags.SetVolumetricFog(false);
		C->ShowFlags.SetMotionBlur(false);
		C->ShowFlags.SetBloom(false);
		C->ShowFlags.SetLensFlares(false);
		C->ShowFlags.SetEyeAdaptation(false);
		C->ShowFlags.SetAtmosphere(false);
		C->ShowFlags.SetCloud(false);
		Target = NewObject<UTextureRenderTarget2D>(this);
		Target->RenderTargetFormat = RTF_RGBA8_SRGB;
		Target->ClearColor = FLinearColor::Black;
	}
	if (Target->SizeX != W || Target->SizeY != H) Target->InitAutoFormat(W, H);
	Target->UpdateResourceImmediate(true);
	USceneCaptureComponent2D* C = Camera->GetCaptureComponent2D();
	C->TextureTarget = Target;
	C->OrthoWidth = float(AcSpace::ToCm(B.width()));
	// Straight down over the region's middle, north (-y) up the picture,
	// east (+x) to its right.
	const ac::Vec2 Mid((B.minX + B.maxX) / 2, (B.minZ + B.maxZ) / 2);
	Camera->SetActorLocationAndRotation(AcSpace::ToWorld(Mid, AcSpace::ToCells(CameraHeight)), FRotator(-90.0, -90.0, 0.0));
	C->CaptureScene();
	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != W * H)
	{
		UE_LOG(LogAutocraft, Error, TEXT("mapbake: %s: no picture read back"), *R.Id);
		return false;
	}
	// Ground out of play: transparent (whatever outline the region has).
	const ac::TerrainField Field(Map);
	const auto Height = [&Field](ac::Vec2 P) { return Field.height(P); };
	int32 Out = 0;
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const ac::Vec2 P(B.minX + (X + 0.5) / PixelsPerCell, B.minZ + (Y + 0.5) / PixelsPerCell);
			FColor& Px = Pixels[Y * W + X];
			const bool bIn = Map.inPlay(P, 0, Height);
			Px.A = bIn ? 255 : 0;
			Out += bIn ? 0 : 1;
		}
	}
	const FString Path = FPaths::Combine(Dir, R.Id + TEXT(".png"));
	const FImageView Image(Pixels.GetData(), W, H, EGammaSpace::sRGB);
	if (!FImageUtils::SaveImageByExtension(*Path, Image))
	{
		UE_LOG(LogAutocraft, Error, TEXT("mapbake: %s: could not write %s"), *R.Id, *Path);
		return false;
	}
	UE_LOG(LogAutocraft, Display, TEXT("mapbake: %s: %d x %d to %s (%.1f%% out of play)"), *R.Id, W, H, *Path, 100.0 * Out / FMath::Max(1, W * H));
	return true;
}
