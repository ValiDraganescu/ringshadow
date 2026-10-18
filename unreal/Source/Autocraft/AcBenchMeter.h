// `FAcBenchMeter`: the bench's measures (AcBench.h) over a run of frames of
// any staged moment, for the MMO bench (`-AcMmoShotsBench`, AcMmoShots.cpp;
// docs/perf.md): the wall frame, Unreal's game, render and RHI threads, the
// GPU's frame, the driver's GPU busy over the run, the game's tick and its
// parts (AcPerf.h), hitches, memory, and after the run the RHI's counts and,
// with `stat gpu` on, each pass's GPU time. `Json` is the `bench-result`
// line bench.py reads, in the shape of AcBench's.
#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UWorld;

struct AUTOCRAFT_API FAcBenchMeter
{
	/// Timing from now (the driver's GPU count read after a short pause).
	void Begin();
	/// One timed frame: call once a frame between Begin and End.
	void Frame();
	/// Timing ends: the driver's count and the memory.
	void End();
	int32 Num() const { return Wall.Num(); }

	/// `stat rhi` and `stat scenerendering` (and `stat gpu`) shown, or every stat hidden.
	static void ShowStats(UWorld* World, bool bOn, bool bGpu);
	/// While the stats show: the draws and, with `stat gpu`, the passes.
	void ReadStats(bool bGpu);

	/// The result, named, as AcBench's `bench-result` JSON.
	FString Json(const FString& Name, const UObject* Context, const TFunction<void(FJsonObject&)>& More = nullptr) const;

	/// A counter of a shown stat group (`stat rhi`), its average over the
	/// stats frame; -1 if the group is not shown.
	static double StatCounter(const TCHAR* Name);
	/// `stat gpu`'s passes on the first graphics queue: busy ms a frame (the
	/// stats' average), largest first.
	static TArray<TPair<FString, double>> GpuStatPasses();

private:
	double LastWall = 0;
	double GpuStartNs = -1, GpuEndNs = -1;
	double FootprintMB = 0, GraphicsMB = 0, PeakMB = 0;
	TArray<double> Wall, Game, Render, Rhi, Gpu, Ticks;
	/// The shadow lights' steps (AcDaylight.cpp) before each timed frame.
	TArray<int64> Steps;
	double Parts[8] = {};
	double PartsMax[8] = {};
	int32 Draws = -1, Primitives = -1, MeshDraws = -1;
	TArray<TPair<FString, double>> Passes;
};
