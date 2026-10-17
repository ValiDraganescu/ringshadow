#include "AcBenchMeter.h"

#include "AcPerf.h"
#include "AcPerfSystem.h"
#include "AcSimSubsystem.h"
#include "AcWorldRenderer.h"

#include "Components/LocalLightComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformProcess.h"
#include "RenderCore.h"
#include "RHI.h"
#include "RHIStats.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Stats/StatsData.h"
#include "UObject/UObjectIterator.h"

#include "Simulation.h"
#include "Types.h"

void FAcBenchMeter::Begin()
{
	*this = FAcBenchMeter();
	// The driver's count comes a little late: read after a pause, as AcBench's.
	FPlatformProcess::Sleep(0.1f);
	GpuStartNs = AcPerfSystem::AppGpuNanoseconds();
	LastWall = FPlatformTime::Seconds();
}

void FAcBenchMeter::Frame()
{
	const double Now = FPlatformTime::Seconds();
	Wall.Add((Now - LastWall) * 1000.0);
	LastWall = Now;
	Game.Add(FPlatformTime::ToMilliseconds(GGameThreadTime));
	Render.Add(FPlatformTime::ToMilliseconds(GRenderThreadTime));
	Rhi.Add(FPlatformTime::ToMilliseconds(GRHIThreadTime));
	Gpu.Add(FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0)));
	const FAcPerf& Perf = FAcPerf::Get();
	Ticks.Add(Perf.LastTickMs());
	for (int32 I = 0; I < (int32)EAcTickPart::Count; ++I)
	{
		Parts[I] += Perf.LastTickParts()[I];
		PartsMax[I] = FMath::Max(PartsMax[I], Perf.LastTickParts()[I]);
	}
}

void FAcBenchMeter::End()
{
	FPlatformProcess::Sleep(0.1f);
	GpuEndNs = AcPerfSystem::AppGpuNanoseconds();
	AcPerfSystem::Memory(FootprintMB, GraphicsMB, PeakMB);
}

void FAcBenchMeter::ShowStats(UWorld* World, const bool bOn, const bool bGpu)
{
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC) return;
	if (!bOn)
	{
		PC->ConsoleCommand(TEXT("stat none"));
		return;
	}
	PC->ConsoleCommand(TEXT("stat rhi"));
	PC->ConsoleCommand(TEXT("stat scenerendering"));
	if (bGpu) PC->ConsoleCommand(TEXT("stat gpu"));
}

void FAcBenchMeter::ReadStats(const bool bGpu)
{
	Draws = FMath::Max3(Draws, GNumDrawCallsRHI[0], (int32)StatCounter(TEXT("STAT_RHIDraws")));
	Primitives = FMath::Max3(Primitives, GNumPrimitivesDrawnRHI[0], (int32)StatCounter(TEXT("STAT_RHIPrimitives")));
	MeshDraws = FMath::Max(MeshDraws, (int32)StatCounter(TEXT("STAT_MeshDrawCalls")));
	if (bGpu) Passes = GpuStatPasses();
}

double FAcBenchMeter::StatCounter(const TCHAR* Name)
{
#if STATS
	const FGameThreadStatsData* D = FLatestGameThreadStatsData::Get().Latest;
	if (!D) return -1;
	const FName Short(Name);
	for (const FActiveStatGroupInfo& G : D->ActiveStatGroups)
	{
		for (const FComplexStatMessage& M : G.CountersAggregate)
		{
			if (M.NameAndInfo.GetShortName() != Short) continue;
			return M.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double
				? M.GetValue_double(EComplexStatField::IncAve)
				: double(M.GetValue_int64(EComplexStatField::IncAve));
		}
	}
#endif
	return -1;
}

TArray<TPair<FString, double>> FAcBenchMeter::GpuStatPasses()
{
	TArray<TPair<FString, double>> Out;
#if STATS
	const FGameThreadStatsData* D = FLatestGameThreadStatsData::Get().Latest;
	if (!D) return Out;
	for (int32 G = 0; G < D->ActiveStatGroups.Num(); ++G)
	{
		if (!D->GroupNames.IsValidIndex(G) || D->GroupNames[G].ToString() != TEXT("STATGROUP_GPU0_Graphics0")) continue;
		for (const FComplexStatMessage& M : D->ActiveStatGroups[G].GpuStatsAggregate)
		{
			// The type is the short name's number: 0 busy, 1 wait, 2 idle (StatsRender2.cpp).
			if (M.GetShortName().GetNumber() != 0) continue;
			if (M.NameAndInfo.GetField<EStatDataType>() != EStatDataType::ST_double) continue;
			Out.Emplace(M.GetDescription(), M.GetValue_double(EComplexStatField::IncAve));
		}
	}
	Out.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Value > B.Value; });
#endif
	return Out;
}

FString FAcBenchMeter::Json(const FString& Name, const UObject* Context, const TFunction<void(FJsonObject&)>& More) const
{
	auto P = [](TArray<double> V, const double Q)
	{
		V.Sort();
		return FAcPerf::Percentile(V, Q);
	};
	auto Max = [](const TArray<double>& V)
	{
		double M = 0;
		for (const double X : V) M = FMath::Max(M, X);
		return M;
	};
	auto Mean = [](const TArray<double>& V)
	{
		double S = 0;
		for (const double X : V) S += X;
		return V.Num() ? S / V.Num() : 0.0;
	};
	const int32 Frames = FMath::Max(1, Wall.Num());
	const double GpuBusy = GpuStartNs >= 0 && GpuEndNs >= GpuStartNs ? (GpuEndNs - GpuStartNs) / 1e6 / Frames : P(Gpu, 0.5);
	const UWorld* World = Context ? Context->GetWorld() : nullptr;
	int32 Lights = 0, Lit = 0;
	for (TObjectIterator<ULocalLightComponent> It; It; ++It)
	{
		const ULocalLightComponent* L = *It;
		if (L->GetWorld() != World || !L->IsRegistered() || !L->IsVisible()) continue;
		++Lights;
		if (L->Intensity > 0) ++Lit;
	}
	// Hitches: frames over 33 and 50 ms.
	int32 Over33 = 0, Over50 = 0;
	for (const double W : Wall)
	{
		Over33 += W > 33.3 ? 1 : 0;
		Over50 += W > 50.0 ? 1 : 0;
	}
	const UAcWorldRenderer* Renderer = Context ? UAcWorldRenderer::Get(Context) : nullptr;
	UAcSimSubsystem* Sim = Context ? UAcSimSubsystem::Get(Context) : nullptr;
	FVector2D Pixels(0, 0);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Pixels);

	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("shot"), Name);
	J->SetNumberField(TEXT("frames"), Wall.Num());
	J->SetNumberField(TEXT("width"), Pixels.X);
	J->SetNumberField(TEXT("height"), Pixels.Y);
	J->SetNumberField(TEXT("gpuBusy"), GpuBusy);
	J->SetNumberField(TEXT("gpuWallP50"), P(Gpu, 0.5));
	J->SetNumberField(TEXT("gpuWallP95"), P(Gpu, 0.95));
	J->SetNumberField(TEXT("gpuWallMax"), Max(Gpu));
	J->SetNumberField(TEXT("frameP50"), P(Wall, 0.5));
	J->SetNumberField(TEXT("frameP95"), P(Wall, 0.95));
	J->SetNumberField(TEXT("frameP99"), P(Wall, 0.99));
	J->SetNumberField(TEXT("frameMax"), Max(Wall));
	J->SetNumberField(TEXT("frameMean"), Mean(Wall));
	J->SetNumberField(TEXT("over33"), Over33);
	J->SetNumberField(TEXT("over50"), Over50);
	J->SetNumberField(TEXT("fps"), 1000.0 / FMath::Max(Mean(Wall), 1e-3));
	J->SetNumberField(TEXT("gameP50"), P(Game, 0.5));
	J->SetNumberField(TEXT("gameP95"), P(Game, 0.95));
	J->SetNumberField(TEXT("renderP50"), P(Render, 0.5));
	J->SetNumberField(TEXT("renderP95"), P(Render, 0.95));
	J->SetNumberField(TEXT("rhiP50"), P(Rhi, 0.5));
	J->SetNumberField(TEXT("rhiP95"), P(Rhi, 0.95));
	J->SetNumberField(TEXT("tickP50"), P(Ticks, 0.5));
	J->SetNumberField(TEXT("tickP95"), P(Ticks, 0.95));
	J->SetNumberField(TEXT("tickMax"), Max(Ticks));
	J->SetNumberField(TEXT("footprintMB"), FootprintMB);
	J->SetNumberField(TEXT("graphicsMB"), GraphicsMB);
	J->SetNumberField(TEXT("peakMB"), PeakMB);
	J->SetNumberField(TEXT("draws"), Draws);
	J->SetNumberField(TEXT("triangles"), Primitives);
	J->SetNumberField(TEXT("meshDraws"), MeshDraws);
	J->SetNumberField(TEXT("objects"), Renderer ? Renderer->LastStats().Objects : 0);
	J->SetNumberField(TEXT("instances"), Renderer ? Renderer->LastStats().Instances : 0);
	const bool bRunning = Sim && Sim->IsRunning();
	J->SetNumberField(TEXT("units"), bRunning ? (double)Sim->State().units.size() : 0);
	J->SetNumberField(TEXT("structures"), bRunning ? (double)Sim->State().structures.size() : 0);
	J->SetNumberField(TEXT("players"), bRunning ? (double)Sim->State().players.size() : 0);
	J->SetNumberField(TEXT("lights"), Lights);
	J->SetNumberField(TEXT("litLights"), Lit);
	J->SetNumberField(TEXT("load"), AcPerfSystem::LoadAverage());
	TArray<TSharedPtr<FJsonValue>> PartRows;
	for (int32 I = 0; I < (int32)EAcTickPart::Count; ++I)
	{
		TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("name"), FAcPerf::PartName((EAcTickPart)I));
		Row->SetNumberField(TEXT("ms"), Parts[I] / Frames);
		Row->SetNumberField(TEXT("max"), PartsMax[I]);
		PartRows.Add(MakeShared<FJsonValueObject>(Row));
	}
	J->SetArrayField(TEXT("parts"), PartRows);
	if (!Passes.IsEmpty())
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		for (const TPair<FString, double>& Pass : Passes) Out->SetNumberField(Pass.Key, Pass.Value);
		J->SetObjectField(TEXT("gpuPasses"), Out);
	}
	if (More) More(*J);
	FString Text;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
	FJsonSerializer::Serialize(J, Writer);
	return Text;
}
