// Where palettes meet: the terrain material's port of the blend field
// (Tools/Editor/make_terrain_material.py, `Lab::Gb*`) against the C++ one
// (GroundBlend.h), which the scenery plans with. M_GroundBlendProbe is the
// field alone, from the same HLSL text M_Terrain is made with: drawn into a
// float render target over rectangles of the First Reach where palettes
// meet (an edge, a corner, a corner's far segments), read back, compared
// a pixel at a time.
//
// It needs the GPU, so it runs in the game, not under -nullrhi (there it
// says so and passes):
//   uv run unreal/Tools/hidden_run.py --out DIR --res 640x400 -- -AcMap=highlands-small \
//     '-ExecCmds=Automation RunTests Autocraft.Ground.Blend; Quit' '-TestExit=Automation Test Queue Empty'
#include "GroundBlend.h"

#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter;

	/// The First Reach (worlds/first.json): 3 × 3 medium regions.
	const char* const Ids[3][3] = {{"ashen-shelf", "cold-fork", "glass-flats"},
								   {"rust-hollow", "start", "tern-ridge"},
								   {"dust-terraces", "brine-pans", "slag-pockets"}};
	constexpr float W = 144, D = 96;

	ac::groundblend::Frame FrameAt(const int32 C, const int32 R)
	{
		static const int32 Dc[9] = {0, 0, 1, 0, -1, 1, 1, -1, -1}, Dr[9] = {0, -1, 0, 1, 0, -1, 1, 1, -1};
		std::array<std::string_view, 9> V;
		for (int32 K = 0; K < 9; K++)
		{
			const int32 Cc = C + Dc[K], Rr = R + Dr[K];
			V[size_t(K)] = Cc < 0 || Cc > 2 || Rr < 0 || Rr > 2 ? std::string_view() : std::string_view(Ids[Rr][Cc]);
		}
		return ac::groundblend::frame(C * W, R * D, C * W + W, R * D + D, V);
	}

	FLinearColor Seeds(const std::array<uint32_t, 4>& A) { return FLinearColor(float(A[0]), float(A[1]), float(A[2]), float(A[3])); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcGroundBlendShaderTest, "Autocraft.Ground.Blend.Shader", Flags)
bool FAcGroundBlendShaderTest::RunTest(const FString&)
{
	if (!FApp::CanEverRender())
	{
		AddInfo(TEXT("no rendering here (-nullrhi): run it in the game (AcGroundBlendTests.cpp)"));
		return true;
	}
	UMaterialInterface* Probe = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Terrain/M_GroundBlendProbe.M_GroundBlendProbe"));
	if (!TestNotNull(TEXT("M_GroundBlendProbe (Tools/Editor/make_terrain_material.py)"), Probe)) return false;
	UWorld* World = nullptr;
	for (const FWorldContext& C : GEngine->GetWorldContexts())
		if (C.World() && (C.WorldType == EWorldType::Game || C.WorldType == EWorldType::PIE)) World = C.World();
	if (!TestNotNull(TEXT("a game world to draw in"), World)) return false;

	constexpr int32 N = 96;
	UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>();
	Target->RenderTargetFormat = RTF_RGBA32f;
	Target->ClearColor = FLinearColor::Black;
	Target->bAutoGenerateMips = false;
	Target->InitCustomFormat(N, N, PF_A32B32G32R32F, /*bInForceLinearGamma*/ true);
	Target->UpdateResourceImmediate(true);
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Probe, World);

	// Region (column, row) and a rectangle round where its palettes meet.
	struct FCase
	{
		const TCHAR* What;
		int32 C, R;
		float X0, Y0, Size;
	};
	const FCase Cases[] = {
		{TEXT("Prospect Landing's north-east corner (plain, frost, glass, plain)"), 1, 1, 2 * W - 40, D - 40, 80},
		{TEXT("Rust Hollow's north edge (rust | ash), the corner's far segment too"), 0, 1, 104, D - 30, 60},
		{TEXT("Brine Pans' east edge (salt | slag)"), 1, 2, 2 * W - 30, 2 * D + 20, 60},
		{TEXT("Prospect Landing's south-west corner, past its rectangle"), 1, 1, W - 30, 2 * D - 30, 60},
	};
	double Worst = 0;
	int32 Mixed = 0;
	for (const FCase& K : Cases)
	{
		const ac::groundblend::Frame F = FrameAt(K.C, K.R);
		M->SetVectorParameterValue(TEXT("ProbeRect"), FLinearColor(K.X0, K.Y0, K.Size, K.Size));
		M->SetVectorParameterValue(TEXT("Bounds"), FLinearColor(F.minX, F.minY, F.maxX - F.minX, F.maxY - F.minY));
		M->SetVectorParameterValue(TEXT("Neighbours"), FLinearColor(F.has[0], F.has[1], F.has[2], F.has[3]));
		M->SetVectorParameterValue(TEXT("BleedEdge"), Seeds(F.edge));
		M->SetVectorParameterValue(TEXT("BleedV"), Seeds(F.beyondV));
		M->SetVectorParameterValue(TEXT("BleedH"), Seeds(F.beyondH));
		UKismetRenderingLibrary::ClearRenderTarget2D(World, Target, FLinearColor(-1, -1, -1, 0));
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, Target, M);
		FlushRenderingCommands();
		TArray<FLinearColor> Pixels;
		FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
		if (!TestTrue(FString::Printf(TEXT("%s: read back"), K.What), Resource && Resource->ReadLinearColorPixels(Pixels) && Pixels.Num() == N * N))
			continue;
		double CaseWorst = 0;
		int32 CaseMixed = 0;
		for (int32 Y = 0; Y < N; Y++)
			for (int32 X = 0; X < N; X++)
			{
				const float Px = K.X0 + (float(X) + 0.5f) / N * K.Size, Py = K.Y0 + (float(Y) + 0.5f) / N * K.Size;
				const ac::groundblend::Field Want = ac::groundblend::field(F, Px, Py);
				const FLinearColor Got = Pixels[Y * N + X];
				CaseWorst = FMath::Max(CaseWorst, double(FMath::Max(FMath::Abs(Got.R - Want.a), FMath::Abs(Got.G - Want.b))));
				CaseMixed += (Want.a > 0.02f && Want.a < 0.98f) || (Want.b > 0.02f && Want.b < 0.98f);
			}
		AddInfo(FString::Printf(TEXT("%s: worst %.2g over %d pixels, %d mixed"), K.What, CaseWorst, N * N, CaseMixed));
		Worst = FMath::Max(Worst, CaseWorst);
		Mixed += CaseMixed;
	}
	// A pixel's sample falls a few ulps apart on the GPU (fused multiply-adds,
	// its divides); the field is smooth, so its answer by much less than 1%.
	TestTrue(FString::Printf(TEXT("the material's field is the C++ one (worst %.2g)"), Worst), Worst < 2e-3);
	TestTrue(FString::Printf(TEXT("the rectangles hold palettes meeting (%d mixed pixels)"), Mixed), Mixed > 2000);
	return true;
}

#endif
