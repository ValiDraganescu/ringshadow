#include "AcWeather.h"

#include "AcDaylight.h"
#include "AcPilotPawn.h"
#include "AcLog.h"
#include "AcSimSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProceduralMeshComponent.h"

namespace
{
	TAutoConsoleVariable<float> CVarWeather(
		TEXT("ac.Weather"), -1.0f,
		TEXT("Hold the weather: 0 clear, 1 cloudy, 2 rain, 3 storm (fractions blend); negative: the schedule. ")
		TEXT("Also -AcWeather=clear|cloudy|rain|storm on the command line."));
	TAutoConsoleVariable<float> CVarRain(
		TEXT("ac.Weather.Rain"), 1.0f,
		TEXT("Scale on the rain streaks (0: none)."));
	TAutoConsoleVariable<int32> CVarLightning(
		TEXT("ac.Weather.Lightning"), 1,
		TEXT("Lightning flashes (1) or none (0)."));

	const TCHAR* RainMaterialPath = TEXT("/Game/Weather/M_AcRain.M_AcRain");
	const TCHAR* BoltMaterialPath = TEXT("/Game/Weather/M_AcBolt.M_AcBolt");
	const TCHAR* LensMaterialPath = TEXT("/Game/Weather/M_AcLensRain.M_AcLensRain");
	const TCHAR* WeatherMpcPath = TEXT("/Game/Materials/MPC_AcWorld.MPC_AcWorld");
	/// The cloud base the bolts come down from (cm; AcDaylight's layer).
	constexpr double BoltTopCm = 160000.0;
	/// A bolt's ribbon width per cm of distance (about 4 px at 1600 wide).
	constexpr double BoltWidthPerCm = 0.004;

	/// The rain box (cm): wide and deep enough to fill the RTS view between
	/// the camera and the ground, and the pilot's near field.
	const FVector RainBox(4000, 4000, 3000);
	/// Fall speed of a raindrop (cm/s): about 7 m/s.
	constexpr double FallCmPerS = 700;
	/// How far ahead of the camera the box's centre sits (cm).
	constexpr double RainAhead = 1400;
	/// A streak's length (cm) and width (cm per cm of distance: ~1.5 px at 1600 wide, 62°).
	constexpr float StreakCm = 45.0f, WidthPerCm = 0.0012f;
	/// The lightning's light on the ground at a close full strike (lux, EV 0).
	constexpr double FlashLux = 4.0;

	/// -AcWeatherSeed=N, if given.
	TOptional<uint64> FlagSeed()
	{
		int64 S = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("AcWeatherSeed="), S)) return uint64(S);
		return {};
	}

	/// The game's own seed: its game id (a UUID kept in the save), so a
	/// saved game goes on with the same weather when it is loaded.
	uint64 SessionSeed(const std::string& Id)
	{
		uint64 H = 1469598103934665603ull;
		for (const char C : Id) H = (H ^ uint8(C)) * 1099511628211ull;
		return H;
	}
}

AAcWeather::AAcWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	Rain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Rain"));
	Rain->SetupAttachment(Root);
	Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rain->SetCastShadow(false);
	Rain->bUseAsyncCooking = false;
	Rain->bAffectDistanceFieldLighting = false;
	Rain->bAffectDynamicIndirectLighting = false;
	Rain->bVisibleInRayTracing = false;
	Rain->bVisibleInReflectionCaptures = false;
	Rain->bVisibleInRealTimeSkyCaptures = false;
	Rain->bNeverDistanceCull = true;
	// The streaks are drawn far from where their vertices sit (round the
	// camera, anywhere on the map): bounds over the whole world.
	Rain->SetBoundsScale(100.0f);
	Rain->SetGenerateOverlapEvents(false);

	BoltMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Bolts"));
	BoltMesh->SetupAttachment(Root);
	BoltMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoltMesh->SetCastShadow(false);
	BoltMesh->bUseAsyncCooking = false;
	BoltMesh->bAffectDistanceFieldLighting = false;
	BoltMesh->bAffectDynamicIndirectLighting = false;
	BoltMesh->bVisibleInRayTracing = false;
	BoltMesh->bVisibleInReflectionCaptures = false;
	BoltMesh->bVisibleInRealTimeSkyCaptures = false;
	BoltMesh->bNeverDistanceCull = true;
	BoltMesh->SetGenerateOverlapEvents(false);

	Lens = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Lens"));
	Lens->SetupAttachment(Root);
	Lens->bUnbound = true;
	Lens->Priority = 2.0f;
	Lens->bEnabled = false;

	Flash = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Root);
	Flash->SetMobility(EComponentMobility::Movable);
	Flash->bAtmosphereSunLight = false;
	Flash->CastShadows = false;
	Flash->VolumetricScatteringIntensity = 0.0f;
	Flash->SetIntensity(0.0f);
	Flash->SetLightColor(FLinearColor(0.75f, 0.82f, 1.0f));
	Flash->SetForwardShadingPriority(0);
	Flash->bAffectTranslucentLighting = false;
}

AAcWeather* AAcWeather::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcWeather> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcWeather* AAcWeather::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcWeather* Existing = Find(World)) return Existing;
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcWeather");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AAcWeather>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}

double AAcWeather::WeatherTime(const double Time)
{
	const TCHAR* Cmd = FCommandLine::Get();
	double T = 0;
	if (FParse::Value(Cmd, TEXT("AcWeatherT="), T)) return T;
	// A time-lapse for clips: the schedule from S, X times as fast.
	double From = 0, Speed = 1;
	FParse::Value(Cmd, TEXT("AcWeatherFrom="), From);
	FParse::Value(Cmd, TEXT("AcWeatherSpeed="), Speed);
	return From + Time * Speed;
}

void AAcWeather::BeginPlay()
{
	Super::BeginPlay();
	const uint64 Seed = FlagSeed().Get(uint64(FMath::Rand()) << 32 ^ uint64(FPlatformTime::Cycles64()));
	Model = MakeUnique<ac::Weather>(Seed);

	// -AcWeather=storm (or a number) holds the weather for shots.
	FString Held;
	if (FParse::Value(FCommandLine::Get(), TEXT("AcWeather="), Held))
	{
		const TMap<FString, float> Names = {{TEXT("clear"), 0.f}, {TEXT("cloudy"), 1.f}, {TEXT("rain"), 2.f}, {TEXT("storm"), 3.f}};
		const float* Named = Names.Find(Held.ToLower());
		CVarWeather->Set(Named ? *Named : FCString::Atof(*Held), ECVF_SetByCommandline);
	}
	ApplyHold();
	BuildRain();
	if (UMaterialInterface* Bolt = LoadObject<UMaterialInterface>(nullptr, BoltMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		BoltMaterial = UMaterialInstanceDynamic::Create(Bolt, this);
	}
	WeatherMpc = LoadObject<UMaterialParameterCollection>(nullptr, WeatherMpcPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, LensMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		LensMaterial = UMaterialInstanceDynamic::Create(Glass, this);
		Lens->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, LensMaterial));
	}
	UE_LOG(LogAutocraft, Log, TEXT("weather: seed %llu, held %.2f"), Seed, CVarWeather.GetValueOnGameThread());
}

void AAcWeather::ApplyHold()
{
	const float Held = CVarWeather.GetValueOnGameThread();
	if (Model && Held != AppliedHold)
	{
		AppliedHold = Held;
		Model->hold(Held);
		SampledAt = -1;
	}
}

const ac::WeatherSample& AAcWeather::Sample(const double Time)
{
	ApplyHold();
	const double T = WeatherTime(Time);
	if (Model && T != SampledAt)
	{
		SampledAt = T;
		Current = Model->sample(T);
		if (CVarLightning.GetValueOnGameThread() == 0) Current.flash = 0;
		if (Current.flash > 0.01)
		{
			for (const ac::LightningStrike& S : Model->strikes(T - 1.0, T + 1e-6))
			{
				if (S.azimuth != FlashAzimuth || S.distanceKm != FlashKm)
				{
					UE_LOG(LogAutocraft, Verbose, TEXT("weather: strike at %.2f s, yaw %.0f, %.1f km"), S.time,
						FMath::RadiansToDegrees(S.azimuth), S.distanceKm);
				}
				FlashAzimuth = S.azimuth;
				FlashKm = S.distanceKm;
			}
		}
	}
	return Current;
}

TArray<ac::LightningStrike> AAcWeather::Thunder(const double T0, const double T1)
{
	TArray<ac::LightningStrike> Out;
	if (!Model || CVarLightning.GetValueOnGameThread() == 0) return Out;
	for (const ac::LightningStrike& S : Model->thunder(T0, T1)) Out.Add(S);
	return Out;
}

void AAcWeather::BuildRain()
{
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, RainMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Base)
	{
		UE_LOG(LogAutocraft, Warning, TEXT("weather: no rain (%s missing: run Tools/Editor/make_weather_materials.py)"), RainMaterialPath);
		return;
	}
	RainMaterial = UMaterialInstanceDynamic::Create(Base, this);

	// Each streak: four corners on one random point of the box (local cm),
	// told apart by their UVs; two triangles.
	TArray<FVector> Pos;
	TArray<int32> Index;
	TArray<FVector2D> UV;
	Pos.Reserve(RainStreaks * 4);
	UV.Reserve(RainStreaks * 4);
	Index.Reserve(RainStreaks * 6);
	FRandomStream Rng(0x5241494E);
	for (int32 K = 0; K < RainStreaks; ++K)
	{
		const FVector P(Rng.FRand() * RainBox.X, Rng.FRand() * RainBox.Y, Rng.FRand() * RainBox.Z);
		const int32 B = Pos.Num();
		for (int32 C = 0; C < 4; ++C)
		{
			Pos.Add(P);
			UV.Add(FVector2D(C & 1, C >> 1));
		}
		Index.Append({B, B + 1, B + 3, B, B + 3, B + 2});
	}
	Rain->CreateMeshSection_LinearColor(0, Pos, Index, {}, UV, {}, {}, false);
	Rain->SetMaterial(0, RainMaterial);
}

void AAcWeather::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	const double Time = Sim ? Sim->GameTime() : 0.0;
	// A new game or a loaded one: the weather of that game (its seed).
	// (`game`: the game's UUID, new with each game and kept in its save;
	// `id` is the save slot, "window" for every window game.)
	const std::string GameId = Sim ? Sim->Session().game.value_or(Sim->Session().id) : std::string();
	if (Sim && Sim->IsRunning() && !FlagSeed().IsSet() && GameId != SeededFor)
	{
		SeededFor = GameId;
		Model = MakeUnique<ac::Weather>(SessionSeed(SeededFor));
		AppliedHold = -2;
		SampledAt = -1;
		Bolts.Reset();
		ApplyHold();
		UE_LOG(LogAutocraft, Log, TEXT("weather: seeded from game %hs"), SeededFor.c_str());
	}
	Sample(Time);
	const AAcDaylight* Daylight = AAcDaylight::Find(GetWorld());
	const float Light = Daylight ? float(1.0 - Daylight->GetDark()) : 1.0f;
	UpdateRain(Time, Light);
	UpdateFlash();
	FVector Cam = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Cam = PC->PlayerCameraManager->GetCameraLocation();
	}
	UpdateBolts(WeatherTime(Time), Cam);
	UpdateLens();
	// Wet hulls (M_Hull reads MPC_AcWorld.Wetness), as wet as the ground.
	if (WeatherMpc)
	{
		const IConsoleVariable* Wet = IConsoleManager::Get().FindConsoleVariable(TEXT("ac.Weather.Wet"));
		const float Want = float(Current.wetness) * (Wet ? Wet->GetFloat() : 1.0f);
		if (FMath::Abs(Want - HullWet) > 1.0f / 256)
		{
			HullWet = Want;
			UKismetMaterialLibrary::SetScalarParameterValue(this, WeatherMpc, TEXT("Wetness"), Want);
		}
	}
}

void AAcWeather::UpdateRain(const double Time, const float Light)
{
	const float Rate = float(FMath::Clamp(Current.rain * CVarRain.GetValueOnGameThread(), 0.0, 1.0));
	const bool bShow = RainMaterial && Rate > 0.001f;
	if (Rain->IsVisible() != bShow) Rain->SetVisibility(bShow);
	if (!bShow) return;

	FVector Cam = FVector::ZeroVector;
	FVector Fwd = FVector::ForwardVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
	{
		Cam = PC->PlayerCameraManager->GetCameraLocation();
		Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	}
	// The wind blows the rain along the clouds' wind (AcDaylight's
	// CloudWind), a fraction of its speed at the ground.
	const FVector2D WindDir = FVector2D(1.0, 0.35).GetSafeNormal();
	const double Gust = Current.wind * 100.0 * 0.25;
	const FVector Vel(WindDir.X * Gust, WindDir.Y * Gust, -FallCmPerS * (1.0 + 0.3 * Current.storm));
	// The fall so far, wrapped per axis (doubles here, so the material's
	// floats never see a large number).
	const FVector Off(FMath::Fmod(Vel.X * Time, RainBox.X), FMath::Fmod(Vel.Y * Time, RainBox.Y), FMath::Fmod(Vel.Z * Time, RainBox.Z));
	const FVector C = Cam + Fwd * RainAhead;
	// Lit by the sky: grey by day, dim by night, white in a flash.
	const float Day = FMath::Lerp(0.12f, 1.0f, Light) * FMath::Lerp(1.0f, 0.7f, float(Current.gloom));
	const float Lit = Day + float(Current.flash) * 2.0f;
	RainMaterial->SetVectorParameterValue(TEXT("RainC"), FLinearColor(float(C.X), float(C.Y), float(C.Z), Rate));
	RainMaterial->SetVectorParameterValue(TEXT("RainBox"), FLinearColor(float(RainBox.X), float(RainBox.Y), float(RainBox.Z), StreakCm * (1.0f + 0.4f * float(Current.storm))));
	RainMaterial->SetVectorParameterValue(TEXT("RainOff"), FLinearColor(float(Off.X), float(Off.Y), float(Off.Z), WidthPerCm));
	RainMaterial->SetVectorParameterValue(TEXT("RainVel"), FLinearColor(float(Vel.X), float(Vel.Y), float(Vel.Z), 0.0f));
	RainMaterial->SetVectorParameterValue(TEXT("RainColor"), FLinearColor(0.55f * Lit, 0.6f * Lit, 0.68f * Lit, 0.26f));
}

void AAcWeather::UpdateFlash()
{
	const float F = float(Current.flash);
	Flash->SetIntensity(float(FlashLux) * F);
	if (F > 0.0f)
	{
		// From high above the strike's side of the sky.
		const FVector Toward(FMath::Cos(FlashAzimuth) * 0.45, FMath::Sin(FlashAzimuth) * 0.45, 0.9);
		Flash->SetWorldRotation((-Toward.GetSafeNormal()).Rotation());
	}
}

void AAcWeather::UpdateBolts(const double T, const FVector& Cam)
{
	if (!Model || !BoltMaterial) return;
	if (T < BoltsTo || T - BoltsTo > 2.0)
	{
		Bolts.Reset();
		BoltsTo = T - 1.0;
	}
	// New strikes: a jagged channel down from the cloud base and a few
	// branches, seeded from the strike (the same strike, the same bolt).
	if (CVarLightning.GetValueOnGameThread() != 0)
	{
		for (const ac::LightningStrike& S : Model->strikes(BoltsTo, T))
		{
			FRandomStream R(int32(uint32(S.time * 1000.0) * 2654435761u));
			const FVector Dir(FMath::Cos(S.azimuth), FMath::Sin(S.azimuth), 0.0);
			const FVector Side(-Dir.Y, Dir.X, 0.0);
			const FVector Ground = FVector(Cam.X, Cam.Y, 0.0) + Dir * (S.distanceKm * 1.0e5);
			const FVector Top = Ground + FVector(0, 0, BoltTopCm) + Side * R.FRandRange(-30000, 30000);
			FBolt B;
			B.Time = S.time;
			B.Power = S.power;
			auto Walk = [&R](FVector From, const FVector& To, int32 Steps, double Jag)
			{
				TArray<FVector> P;
				const FVector Step = (To - From) / Steps;
				const double Len = Step.Size();
				FVector Off = FVector::ZeroVector;
				for (int32 K = 0; K <= Steps; ++K)
				{
					// The wander tapers to nothing at the end, so the channel
					// meets its ground point without a long straight last step.
					P.Add(From + Step * K + Off * (1.0 - FMath::Pow(double(K) / Steps, 3.0)));
					Off += FVector(R.FRandRange(-1, 1), R.FRandRange(-1, 1), R.FRandRange(-0.2, 0.2)) * Len * Jag;
				}
				return P;
			};
			B.Paths.Add(Walk(Top, Ground, 44, 0.4));
			const int32 Branches = 2 + R.RandRange(0, 1);
			for (int32 K = 0; K < Branches; ++K)
			{
				const TArray<FVector>& Main = B.Paths[0];
				const FVector From = Main[R.RandRange(4, Main.Num() / 2)];
				const FVector To = From + FVector(R.FRandRange(-1, 1), R.FRandRange(-1, 1), 0).GetSafeNormal() * R.FRandRange(15000, 35000)
					- FVector(0, 0, R.FRandRange(20000, 50000));
				B.Paths.Add(Walk(From, To, 14, 0.5));
			}
			Bolts.Add(MoveTemp(B));
		}
	}
	BoltsTo = T;
	Bolts.RemoveAll([T](const FBolt& B) { return T - B.Time > 1.0; });
	if (Bolts.Num() == 0)
	{
		if (bBoltsShown)
		{
			BoltMesh->ClearAllMeshSections();
			bBoltsShown = false;
		}
		return;
	}
	// Ribbons facing the camera, a few pixels wide at any distance; the
	// branches thinner and dimmer.
	TArray<FVector> Pos;
	TArray<int32> Index;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Colour;
	for (const FBolt& B : Bolts)
	{
		const double Bright = ac::Weather::flicker(T - B.Time) * B.Power;
		for (int32 PathIndex = 0; PathIndex < B.Paths.Num(); ++PathIndex)
		{
			const TArray<FVector>& P = B.Paths[PathIndex];
			const double Scale = PathIndex == 0 ? 1.0 : 0.6;
			// Two ribbons per channel: the core, and a wide faint glow round it.
			for (const FVector2D& Layer : {FVector2D(5.0, 0.2), FVector2D(1.0, 1.0)})
			{
				const int32 Base = Pos.Num();
				for (int32 K = 0; K < P.Num(); ++K)
				{
					const FVector Tangent = (P[FMath::Min(K + 1, P.Num() - 1)] - P[FMath::Max(K - 1, 0)]).GetSafeNormal();
					const FVector ToCam = Cam - P[K];
					const FVector Across = FVector::CrossProduct(Tangent, ToCam).GetSafeNormal();
					const double W = FMath::Max(ToCam.Size() * BoltWidthPerCm * Scale, 50.0) * Layer.X;
					const float V = float(K) / float(P.Num() - 1);
					// The channel comes out of the cloud base: its top fades in.
					const double In = PathIndex == 0 ? FMath::SmoothStep(0.0, 0.15, double(V)) : 1.0;
					Pos.Add(P[K] - Across * W * 0.5);
					Pos.Add(P[K] + Across * W * 0.5);
					UV.Add(FVector2D(0, V));
					UV.Add(FVector2D(1, V));
					const FLinearColor C(1, 1, 1, float(Bright * Scale * Layer.Y * In));
					Colour.Add(C);
					Colour.Add(C);
					if (K > 0)
					{
						const int32 A = Base + (K - 1) * 2;
						Index.Append({A, A + 1, A + 3, A, A + 3, A + 2});
					}
				}
			}
		}
	}
	BoltMesh->CreateMeshSection_LinearColor(0, Pos, Index, {}, UV, Colour, {}, false);
	BoltMesh->SetMaterial(0, BoltMaterial);
	bBoltsShown = true;
}

void AAcWeather::UpdateLens()
{
	// Only in the cockpit: the glass is the driven unit's, not the RTS
	// camera's or the chase camera's.
	const AAcPilotPawn* Pilot = AAcPilotPawn::Find(this);
	const float Rate = float(FMath::Clamp(Current.rain * CVarRain.GetValueOnGameThread(), 0.0, 1.0));
	const bool bOn = LensMaterial && Rate > 0.02f && Pilot && Pilot->Driving() && !Pilot->ChaseEye().IsSet();
	if (Lens->bEnabled != bOn) Lens->bEnabled = bOn;
	if (bOn) LensMaterial->SetScalarParameterValue(TEXT("LensRain"), Rate);
}
