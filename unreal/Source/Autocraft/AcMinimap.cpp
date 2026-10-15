#include "AcMinimap.h"

#include "AcTerrain.h"
#include "Async/Async.h"

#include "AcMissions.h"

#include "AcMmo.h"

#include "AcCab.h"
#include "AcCabArt.h"
#include "AcConsole.h"
#include "AcFog.h"
#include "AcLog.h"
#include "AcMinimapBake.h"
#include "AcRtsPawn.h"
#include "AcSimSubsystem.h"
#include "SAcConsole.h"
#include "SAcMinimap.h"

#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#include "TerrainField.h"
#include "Types.h"

namespace
{
	/// "A,B[,C...]" → numbers (false when fewer than `N`).
	bool ParseNumbers(const FString& Text, int32 N, TArray<double>& Out)
	{
		TArray<FString> Parts;
		Text.Replace(TEXT("\""), TEXT("")).ParseIntoArray(Parts, TEXT(","));
		Out.Reset();
		for (const FString& P : Parts) Out.Add(FCString::Atod(*P.TrimStartAndEnd()));
		return Out.Num() >= N;
	}

	FAutoConsoleCommandWithWorldAndArgs GMinimapClick(TEXT("ac.MinimapClick"),
		TEXT("Press the console's minimap at a fraction of its width and height (0 0: top left)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UAcMinimapSubsystem* M = UAcMinimapSubsystem::Get(World);
			if (!M || Args.Num() < 2) return;
			M->PressAt(FVector2D(FCString::Atod(*Args[0]), FCString::Atod(*Args[1])));
		}));
}

UAcMinimapSubsystem* UAcMinimapSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAcMinimapSubsystem>() : nullptr;
}

bool UAcMinimapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UAcMinimapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim) return;
	const TCHAR* Cmd = FCommandLine::Get();
	FString Text;
	TArray<double> N;
	if (FParse::Value(Cmd, TEXT("AcMinimapClick="), Text) && ParseNumbers(Text, 2, N)) PendingPress = FVector2D(N[0], N[1]);
	if (FParse::Value(Cmd, TEXT("AcMinimapSight="), Text) && ParseNumbers(Text, 3, N))
	{
		SightNow = TPair<ac::Vec2, double>(ac::Vec2(N[0], N[1]), N[2]);
	}
	FrameHandle = Sim->AddFrameListener(EAcFrameStage::Hud, FAcFrameEvent::FDelegate::CreateUObject(this, &UAcMinimapSubsystem::OnFrame));
	StartedHandle = Sim->OnGameStarted.AddUObject(this, &UAcMinimapSubsystem::OnGameStarted);
	if (Sim->IsRunning()) Build(*Sim);
}

void UAcMinimapSubsystem::Deinitialize()
{
	if (UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this))
	{
		Sim->RemoveFrameListener(EAcFrameStage::Hud, FrameHandle);
		Sim->OnGameStarted.Remove(StartedHandle);
	}
	if (const TSharedPtr<SAcConsole> Console = MountedOn.Pin()) Console->SetMinimap(nullptr);
	Main.Reset();
	Others.Reset();
	TerrainTexture.Reset();
	Baked.Reset();
	for (TPair<FString, TFuture<FAcArtImage>>& B : Baking) B.Value.Wait();
	Baking.Reset();
	Super::Deinitialize();
}

void UAcMinimapSubsystem::OnGameStarted(UAcSimSubsystem& Sim)
{
	Build(Sim);
}

void UAcMinimapSubsystem::Build(const UAcSimSubsystem& Sim)
{
	const double T0 = FPlatformTime::Seconds();
	const ac::MapDefinition& Map = Sim.Map();
	// The window game has a free camera: the minimap covers the map's bounds.
	MapBounds = Map.bounds;
	// Baked ahead (W4), or now.
	const FString Key = AAcTerrain::KeyOf(Map);
	FAcArtImage Image;
	if (const TSharedPtr<TStrongObjectPtr<UTexture2D>>* Ahead = Baked.Find(Key); Ahead && (*Ahead)->IsValid())
	{
		TerrainTexture.Reset((*Ahead)->Get());
		Image.Width = TerrainTexture->GetSizeX();
		Image.Height = TerrainTexture->GetSizeY();
	}
	else
	{
		const ac::TerrainField Field(Map);
		Image = AcMinimapBake::Terrain(Field, MapBounds, PixelsPerCell);
		TerrainTexture.Reset(AcCabArt::ToTexture(Image, TEXT("AcMinimapTerrain")));
		if (Sim.IsMmo()) Baked.Add(Key, MakeShared<TStrongObjectPtr<UTexture2D>>(TerrainTexture.Get()));
	}
	const FVector2D Size = FAcCabLayout::MinimapSizeFor(MapBounds.width(), MapBounds.depth());
	if (!Main)
	{
		SAssignNew(Main, SAcMinimap)
			.Scale(1.0)
			.ShowFootprint(true)
			.OnGround(FAcOnMinimapGround::CreateUObject(this, &UAcMinimapSubsystem::JumpTo));
	}
	Main->SetMap(MapBounds, Size, TerrainTexture.Get());
	for (const TWeakPtr<SAcMinimap>& W : Others)
	{
		if (const TSharedPtr<SAcMinimap> M = W.Pin())
		{
			// Keep its width, take the new map's aspect (`makeCommandMinimap`).
			const FVector2D Old = M->Size();
			M->SetMap(MapBounds, FVector2D(Old.X, Old.X * MapBounds.depth() / FMath::Max(MapBounds.width(), 1e-6)),
				TerrainTexture.Get());
		}
	}
	UE_LOG(LogAutocraft, Log, TEXT("minimap: %dx%d terrain for %s in %.0f ms, %.0fx%.0f points"), Image.Width, Image.Height,
		UTF8_TO_TCHAR(Map.name.c_str()), (FPlatformTime::Seconds() - T0) * 1000, Size.X, Size.Y);
}

TSharedRef<SAcMinimap> UAcMinimapSubsystem::MakeMinimap(const FVector2D Size, const double Scale, const bool bShowFootprint)
{
	TSharedRef<SAcMinimap> M = SNew(SAcMinimap)
		.Scale(Scale)
		.ShowFootprint(bShowFootprint)
		.OnGround(FAcOnMinimapGround::CreateUObject(this, &UAcMinimapSubsystem::JumpTo));
	M->SetMap(MapBounds, Size, TerrainTexture.Get());
	M->SetSight(SightNow);
	Others.Add(M);
	return M;
}

void UAcMinimapSubsystem::SetSight(TOptional<TPair<ac::Vec2, double>> Sight)
{
	SightNow = MoveTemp(Sight);
}

void UAcMinimapSubsystem::Mount()
{
	const UAcConsoleSubsystem* Consoles = UAcConsoleSubsystem::Get(this);
	const TSharedPtr<SAcConsole> Console = Consoles ? Consoles->Console() : nullptr;
	if (!Console || !Main || MountedOn.Pin() == Console) return;
	Console->SetMinimapSize(Main->Size());
	Console->SetMinimap(Main);
	MountedOn = Console;
	UE_LOG(LogAutocraft, Log, TEXT("minimap: on the console"));
}

void UAcMinimapSubsystem::JumpTo(const ac::Vec2 Ground)
{
	// GameController.jumpFromMinimap: `free.edit { $0.center(on:) }`.
	if (AAcRtsPawn* Pawn = AAcRtsPawn::Get(this)) Pawn->CenterOn(Ground);
}

bool UAcMinimapSubsystem::PressAt(const FVector2D Fraction)
{
	if (!Main) return false;
	const FGeometry& G = Main->LastGeometry();
	const FVector2D Local = FVector2D(G.GetLocalSize()) * Fraction;
	if (G.GetLocalSize().X <= 0) return false;
	const ac::Vec2 Ground = Main->Ground(Main->ToOwn(G, Local));
	const bool bOn = Main->Press(G, Local);
	UE_LOG(LogAutocraft, Log, TEXT("minimap: press at %.2f,%.2f → ground %.1f,%.1f%s"), Fraction.X, Fraction.Y, Ground.x, Ground.y,
		bOn ? TEXT("") : TEXT(" (off the map)"));
	return bOn;
}

void UAcMinimapSubsystem::BakeAround(const UAcSimSubsystem& Sim)
{
	const FAcMmoClient* Mmo = Sim.MmoClient();
	if (!Mmo) return;
	for (auto It = Baking.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsReady()) continue;
		const FAcArtImage Image = It.Value().Get();
		Baked.Add(It.Key(), MakeShared<TStrongObjectPtr<UTexture2D>>(AcCabArt::ToTexture(Image, TEXT("AcMinimapTerrain"))));
		It.RemoveCurrent();
		// One a frame: a texture's upload is small, but not nothing.
		break;
	}
	TSet<FString> Keep;
	Keep.Add(AAcTerrain::KeyOf(Sim.Map()));
	for (const FAcRegionAround& A : Mmo->Around())
		if (A.Map) Keep.Add(AAcTerrain::KeyOf(*A.Map));
	for (const FAcRegionAround& A : Mmo->Around())
	{
		if (!A.Map) continue;
		const FString Key = AAcTerrain::KeyOf(*A.Map);
		if (Baked.Contains(Key) || Baking.Contains(Key)) continue;
		// One at a time, a while after the region changed, on one thread
		// (never crowding the frames, W4).
		if (!Baking.IsEmpty() || Mmo->SinceRegionChange() < 2.0) break;
		Baking.Add(Key, Async(EAsyncExecution::ThreadPool, [Map = A.Map, Ppc = PixelsPerCell]() {
			const ac::TerrainField Field(*Map);
			return AcMinimapBake::Terrain(Field, Map->bounds, Ppc, true);
		}));
	}
	for (auto It = Baked.CreateIterator(); It; ++It)
		if (!Keep.Contains(It.Key())) It.RemoveCurrent();
}

void UAcMinimapSubsystem::OnFrame(const FAcFrame& Frame)
{
	UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	if (!Sim || !Sim->IsRunning() || !Main) return;
	Mount();
	BakeAround(*Sim);

	TArray<TSharedPtr<SAcMinimap>> All = {Main};
	Others.RemoveAll([](const TWeakPtr<SAcMinimap>& W) { return !W.IsValid(); });
	for (const TWeakPtr<SAcMinimap>& W : Others) All.Add(W.Pin());

	const ac::GameState& Shown = Sim->Shown();
	const AAcRtsPawn* Pawn = AAcRtsPawn::Get(this);
	const ac::FreeView* View = Pawn ? Pawn->View() : nullptr;
	const std::vector<ac::Vec2> Footprint = View ? View->footprint() : std::vector<ac::Vec2>();
	AAcFog* Fog = AAcFog::Find(GetWorld());
	// The MMORPG test's regions are open land, not enemy ground: the whole
	// region on the minimap, no fog over what the pilot has not seen.
	const bool bFog = Fog && Fog->IsOn() && Fog->MinimapTexture() && !Sim->IsMmo();
	// The MMORPG test: the region's flight pads.
	TArray<ac::Vec2> Pads;
	if (const FAcMmoClient* Mmo = Sim->MmoClient())
	{
		if (const ac::net::WorldMessage::Region* Here = Mmo->Here(); Here && !Mmo->Travelling())
		{
			for (const ac::region::Place& P : ac::net::padsOnGround(*Here, Sim->Map().bounds)) Pads.Add(P.at);
		}
	}
	// C2: a survey flare burning in this region (world time against the sky's).
	TOptional<TPair<ac::Vec2, double>> Flare;
	if (const FAcMmoClient* Mmo = Sim->MmoClient())
	{
		const ac::net::ProgressMessage* P = Mmo->Progress();
		const ac::net::WorldMessage::Region* Here = Mmo->Here();
		if (P && P->flare && Here && !Mmo->Travelling() && P->flare->region == Here->id && Mmo->Sky() && Mmo->Sky()->time < P->flare->until)
		{
			Flare = TPair<ac::Vec2, double>(P->flare->at, P->flare->radius);
		}
	}
	// K2: the Citadel missions taken with a place in this region.
	TArray<SAcMinimap::FMission> Missions;
	if (const FAcMmoClient* Mmo = Sim->MmoClient())
	{
		const ac::net::WorldMessage::Region* Here = Mmo->Here();
		if (Here && !Mmo->Travelling())
		{
			for (const ac::net::CitadelMission* X : AcMissions::Marked(*Mmo))
			{
				if (X->region != Here->id || X->kind == ac::net::CitadelMission::Kind::hunt) continue;
				Missions.Add({X->at, X->radius, AcMissions::TierColor(X->tier)});
			}
		}
	}
	for (const TSharedPtr<SAcMinimap>& M : All)
	{
		M->SetPads(Pads);
		M->SetFlare(Flare);
		M->SetMissions(Missions);
		M->SetMarks(Shown, Sim->Map());
		M->SetFootprint(Footprint);
		if (bFog) M->SetFog(Fog->MinimapTexture(), Fog->GridOrigin(), Fog->GridSize());
		else M->SetFog(nullptr, ac::Vec2(), ac::Vec2());
		M->SetSight(SightNow);
	}

	// A scripted press waits until the minimap has been drawn.
	if (PendingPress && ++PressWait > 3 && PressAt(*PendingPress)) PendingPress.Reset();
}
