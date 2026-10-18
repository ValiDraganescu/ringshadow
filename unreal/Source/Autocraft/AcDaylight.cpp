#include "AcDaylight.h"

#include "AcGasGiant.h"
#include "AcGroundLook.h"
#include "AcSkyFx.h"
#include "AcLog.h"
#include "AcSimSubsystem.h"
#include "AcSpace.h"
#include "AcTerrain.h"
#include "AcWeather.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	TAutoConsoleVariable<float> CVarHour(
		TEXT("ac.Hour"), -1.0f,
		TEXT("Hold the day clock at this hour (0-24), for shots; negative: the game's own clock. ")
		TEXT("Also -AcHour=H on the command line (AUTOCRAFT_HOUR in the Swift game)."));
	TAutoConsoleVariable<float> CVarDayLength(
		TEXT("ac.DayLength"), 600.0f,
		TEXT("Seconds of game time in one day and night (Daylight.length)."));
	TAutoConsoleVariable<float> CVarNightReach(
		TEXT("ac.NightReach"), 26.0f,
		TEXT("How far a driven unit's eye sees at night, cells (110 by day): the ground is clear to a quarter of it and lost by it."));
	TAutoConsoleVariable<float> CVarShadowStep(
		TEXT("ac.Daylight.ShadowStepDeg"), 0.5f,
		TEXT("The shadow-casting lights (the sun, the ringlight) turn in steps of at most this many degrees, fewer near the horizon ")
		TEXT("(ac.Daylight.ShadowStepMetres), so the virtual shadow maps stay cached between steps (docs/perf.md); 0: every frame."));
	TAutoConsoleVariable<float> CVarShadowStepMetres(
		TEXT("ac.Daylight.ShadowStepMetres"), 20.0f,
		TEXT("The sun steps so little that the shadow tip of an object this tall moves under 0.1 m a step (20: a Citadel's mast); 0: the sun every frame."));
	TAutoConsoleVariable<float> CVarShadowStepMetresMoon(
		TEXT("ac.Daylight.ShadowStepMetresMoon"), 5.0f,
		TEXT("The same for the ringlight, whose night shadows are faint (5: a unit's height; its steps come about 4 times rarer); 0: every frame."));
	TAutoConsoleVariable<int32> CVarClouds(
		TEXT("ac.Daylight.Clouds"), 1,
		TEXT("Volumetric clouds and their shadows over the map (1) or none (0)."));
	TAutoConsoleVariable<float> CVarCloudCover(
		TEXT("ac.Daylight.CloudCover"), -1.0f,
		TEXT("Hold the cloud cover (0 clear - 1 overcast); negative: the weather drifts through the days."));
	TAutoConsoleVariable<float> CVarCloudShadows(
		TEXT("ac.Daylight.CloudShadows"), 1.0f,
		TEXT("Scale on how dark the clouds' shadows on the map are (0: none)."));
	TAutoConsoleVariable<float> CVarWet(
		TEXT("ac.Weather.Wet"), 1.0f,
		TEXT("Scale on the wet ground after rain (0: always dry)."));
	TAutoConsoleVariable<float> CVarRipples(
		TEXT("ac.Weather.Ripples"), 1.0f,
		TEXT("Scale on the rain rings in the wet ground and the puddles (0: none)."));
	TAutoConsoleVariable<float> CVarGloss(
		TEXT("ac.Weather.Gloss"), 1.0f,
		TEXT("How glossy the wet ground and the puddles get (0: wet ground stays as rough as dry)."));
	TAutoConsoleVariable<float> CVarCloudDensity(
		TEXT("ac.Daylight.CloudDensity"), 1.0f,
		TEXT("Scale on the clouds' density (thicker, darker undersides above 1)."));
	TAutoConsoleVariable<float> CVarCloudErosion(
		TEXT("ac.Daylight.CloudErosion"), 1.0f,
		TEXT("Scale on how much the fine detail noise eats the clouds' thin edges (0: smooth billows only)."));
	TAutoConsoleVariable<float> CVarCloudShadowSteps(
		TEXT("ac.Daylight.CloudShadowSteps"), 1.0f,
		TEXT("Scale on the steps each cloud sample marches toward the sun (10 at 1)."));
	TAutoConsoleVariable<float> CVarCloudShadowKm(
		TEXT("ac.Daylight.CloudShadowKm"), 4.0f,
		TEXT("How far (km) each cloud sample marches toward the sun for its shadow."));
	TAutoConsoleVariable<float> CVarCloudSun(
		TEXT("ac.Daylight.CloudSun"), 1.3f,
		TEXT("Scale on the high sun's light the clouds scatter (not the sky's): sunlit tops against shaded bases."));
	TAutoConsoleVariable<FString> CVarCloudReference(
		TEXT("ac.Daylight.CloudReference"), TEXT(""),
		TEXT("Another cloud setup under the same sun and sky, for side-by-side shots (docs/weather.md): empty ours; ")
		TEXT("engine (the OpenWorld template's); or a cloud material's path [;bottom km;height km]."));
	TAutoConsoleVariable<float> CVarCloudBottom(
		TEXT("ac.Daylight.CloudBottom"), 1.6f,
		TEXT("Where the cloud layer starts, km over the ground (a rain deck's base; fair-weather clouds sit 0.9 km higher)."));
	TAutoConsoleVariable<float> CVarCloudHeight(
		TEXT("ac.Daylight.CloudHeight"), 3.4f,
		TEXT("How thick the cloud layer is, km (a storm's towers reach its top)."));
	TAutoConsoleVariable<FString> CVarCloudParam(
		TEXT("ac.Daylight.CloudParam"), TEXT(""),
		TEXT("Cloud look tuning: vector parameters set on M_AcCloud and M_AcCloudShadow over the frame's own, ")
		TEXT("\"CloudE=0.04:1:0.5:0;CloudD=...\" (x:y:z:w; missing components keep the default). Empty: the material's defaults."));
	TAutoConsoleVariable<int32> CVarVolumetricFog(
		TEXT("ac.Daylight.VolumetricFog"), 1,
		TEXT("Volumetric fog in the haze (1) or plain height fog (0)."));
	TAutoConsoleVariable<float> CVarHazeGain(
		TEXT("ac.Daylight.HazeGain"), 1.0f,
		TEXT("Brightness of Swift's haze rising from the horizon on the sky dome and the pilot's veil."));
	TAutoConsoleVariable<float> CVarFog(
		TEXT("ac.Daylight.Fog"), 1.0f,
		TEXT("Scale on the height fog's density (0: no haze)."));
	TAutoConsoleVariable<float> CVarToneCurve(
		TEXT("ac.Daylight.ToneCurve"), 0.6f,
		TEXT("How much of Unreal's filmic tone curve the picture gets (0: SceneKit-like clip, 1: all)."));
	TAutoConsoleVariable<int32> CVarIsolate(
		TEXT("ac.Daylight.Isolate"), 0,
		TEXT("Calibration: light with one part only. 0 all, 1 sun and moon, 2 fill, 3 sky light."));

	// Calibration against the Swift renders (`windowshot` with AUTOCRAFT_HOUR,
	// side by side with `-AcShot` at the same hours). SceneKit's light
	// intensity 1000 is 1.0; these map it to lux at manual exposure EV 0.
	constexpr double SunLux = 2.9 / 1000.0;
	constexpr double MoonLux = 3.2 / 1000.0;
	constexpr double FillLux = 1.9 / 1000.0;
	/// The sky light's intensity for Swift's ambient 1.0 (its environment
	/// map at 0.75 by day, 0.12 at night).
	constexpr double SkyLightScale = 1.3;
	/// C6 (the A2-fix finding): shade and cliff faces read darker and the
	/// ground redder than Swift, whose studio ambient is bluer. A cvar lift
	/// on the sky light and its tint, tuned against Swift `bench --shot base`
	/// with the fog off at noon: cliffs, shade, plateau, lowland and grass
	/// within ~5% (they were 15-25% dark and short of blue).
	TAutoConsoleVariable<float> CVarAmbient(
		TEXT("ac.Daylight.Ambient"), 3.0f,
		TEXT("Scale on the sky light over the Swift ambient tone (C6 calibration)."));
	TAutoConsoleVariable<float> CVarAmbientBlue(
		TEXT("ac.Daylight.AmbientBlue"), 1.6f,
		TEXT("Blue tint of the sky light (1: neutral; green between 1 and this)."));
	/// The captured night sky is far darker than Swift's studio map at 0.12,
	/// so unlit night ground read ~60% of Swift's: the sky light is lifted by
	/// this much with the sun 18° or more below the horizon (none above -8°).
	TAutoConsoleVariable<float> CVarNightAmbient(
		TEXT("ac.Daylight.NightAmbient"), 3.5f,
		TEXT("Lift on the sky light at full dark (1: none); unlit night ground against Swift."));
	/// How much the atmosphere's low sun or moon is lifted at the horizon,
	/// fading out by 25°.
	constexpr double LowLightLift = 3.0;
	/// The atmosphere's sun and moon (they light the sky, not the ground).
	constexpr double SkySunLux = 6.0;
	constexpr double SkyMoonLux = 2.0;
	constexpr float FogDensity = 0.012f;

	const TCHAR* SkyMaterialPath = TEXT("/Game/Sky/M_AcSky.M_AcSky");
	const TCHAR* DomeMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* CloudMaterialPath = TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst");
	/// Our clouds and their shadows (`Tools/Editor/make_cloud_material.py`);
	/// the engine's clouds and no shadows if missing.
	const TCHAR* AcCloudMaterialPath = TEXT("/Game/Sky/MI_AcCloud.MI_AcCloud");
	const TCHAR* AcCloudShadowMaterialPath = TEXT("/Game/Sky/MI_AcCloudShadow.MI_AcCloudShadow");

	/// Weather map and noise tiles across (km): clumps ~2 km, bodies ~1 km,
	/// edge detail 250 m-1 km (finer is under a pixel and reads as grain).
	/// Sized with the wind for the map's sake: the shadows are the clouds'
	/// own, so a medium map gets ~5 passes of shade in 20 min, ~1 min each,
	/// ~28 % of the time (simulated over the baked noise; the 30 km weather
	/// and 1.5 km bodies left it in sun for up to 20 min).
	constexpr double WeatherKm = 12.0, ShapeKm = 4.0, DetailKm = 2.0;
	/// Extinction per metre at full density, and the edge erosion.
	constexpr double CloudDensity = 0.04, CloudErosion = 0.35;
	/// The wind's direction, toward +X a little +Y (Unreal axes); its speed
	/// and the distance moved are the weather's (`windKm`, 15 m/s clear: a
	/// cloud's shadow edge crosses the RTS view in ~1.5 s).
	const FVector2D CloudWind = FVector2D(1.0, 0.35).GetSafeNormal();
	/// How much longer the weather map's patterns are along the wind than
	/// across it: streets of cloud, not round blots.
	constexpr double CloudStretch = 2.0;
	/// The far clouds' haze: the horizon tone's light times this.
	constexpr double CloudHazeGain = 0.5;
	/// How much of the light a thick cloud takes away, and how much summed
	/// density (four samples up the light's ray) makes full shade.
	constexpr double CloudShadowStrength = 0.7, CloudShadowSoftness = 2.5;
	/// A strike's light in the cloud round it (`Bolt`: a core and a halo out
	/// to CloudFlashKm), at full flash. Only that cloud lights: lit all over
	/// the sky, 0.6 already turned the frame white.
	constexpr double CloudFlashGlow = 0.2;
	constexpr double CloudFlashKm = 2.5;
	/// The warm ground's bounce on the cloud bases, and the sky light left there.
	const FColor CloudGroundAlbedo(150, 128, 105);
	constexpr float CloudBottomOcclusion = 0.6f;

	double Smoothstep(double A, double B, double X)
	{
		const double T = FMath::Clamp((X - A) / (B - A), 0.0, 1.0);
		return T * T * (3 - 2 * T);
	}

	FLinearColor Mix(const FLinearColor& A, const FLinearColor& B, double T)
	{
		return A * float(1 - T) + B * float(T);
	}

	FLinearColor Rgb(double R, double G, double B) { return FLinearColor(float(R), float(G), float(B), 1.0f); }

	/// A Swift colour (sRGB, as `NSColor(calibratedRed:...)` takes it) in
	/// linear light, as Unreal's light colours want it.
	FLinearColor Lin(const FLinearColor& C) { return FLinearColor::FromSRGBColor(C.ToFColor(false)); }
}

/// The shadow lights' steps so far (the MMO bench reads it: FAcBenchMeter).
int64 GAcShadowSteps = 0;

namespace
{
	/// Turns a shadow-casting light toward `Forward` (where its light goes)
	/// only once that is a step away from where it points (docs/perf.md,
	/// client round 2). The virtual shadow maps cache a light's pages only
	/// while it holds still: turned a little every frame with the clock, the
	/// sun's shadows were drawn whole every frame. A step: at most
	/// `ac.Daylight.ShadowStepDeg`, and at most what moves the tip of an
	/// object `Metres` tall's shadow 0.1 m: the sun's 20 m (a Citadel's mast;
	/// a 5 m bound let its long dawn shadow tick visibly), the ringlight's 5
	/// m (its shadows are faint). That tip moves h / sin²(e) per radian at
	/// elevation e (along the shadow; across it h / sin(e), less), so the
	/// step is 0.1 / h · sin²(e) rad (the sun: 0.18° at 53°, the bench's
	/// noon; 0.034° at 20°; 0.015° at 13°) or the cap, the smaller.
	void TurnShadowLight(UDirectionalLightComponent* Light, const FVector& Forward, const double Metres)
	{
		const FVector To = Forward.GetSafeNormal();
		const double Cap = CVarShadowStep.GetValueOnGameThread();
		if (Cap > 0 && Metres > 0)
		{
			const FVector Was = Light->GetComponentRotation().Vector();
			const double SinE = To.Z;
			const double Step = FMath::Min(Cap, FMath::RadiansToDegrees(0.1 / Metres * SinE * SinE));
			const double Off = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Was, To).Size(), FVector::DotProduct(Was, To)));
			if (Off <= Step) return;
		}
		Light->SetWorldRotation(To.Rotation());
		if (Light->CastShadows) ++GAcShadowSteps;
	}
}

double AAcDaylight::DayLength() { return FMath::Max(1.0, double(CVarDayLength.GetValueOnGameThread())); }

double AAcDaylight::HeldHour() { return CVarHour.GetValueOnGameThread(); }

namespace
{
	TOptional<FAcWorldSky> GWorldSky;
}

void AAcDaylight::SetWorldSky(const TOptional<FAcWorldSky>& Sky) { GWorldSky = Sky; }

const TOptional<FAcWorldSky>& AAcDaylight::WorldSky() { return GWorldSky; }

double AAcDaylight::RingshadowSun = 1.0;

double AAcDaylight::HourAt(const double Time)
{
	// The MMORPG world's clock, the server's: its regions' sight follows it.
	if (GWorldSky)
	{
		return ac::WorldClock::hourAt(GWorldSky->Time, GWorldSky->DayLength,
			GWorldSky->HeldHour >= 0 ? std::optional<double>(GWorldSky->HeldHour) : std::nullopt);
	}
	// The core's clock: the sim's sight closes in on the same night.
	const double Held = HeldHour();
	return ac::Rules::hourAt(Time, DayLength(), Held >= 0 ? std::optional<double>(Held) : std::nullopt);
}

FVector AAcDaylight::SunSceneKit(const double H)
{
	const FVector Noon = FVector(-60, 110, -55).GetSafeNormal();
	const FVector West = FVector(-Noon.Z, 0, Noon.X).GetSafeNormal();
	const double Day = Sunset - Sunrise;
	const double A = H >= Sunrise && H <= Sunset
		? (H - Sunrise) / Day * PI - PI / 2
		: FMath::Fmod(H - Sunset + 24, 24.0) / (24 - Day) * PI + PI / 2;
	return FMath::Sin(A) * West + FMath::Cos(A) * Noon;
}

FVector AAcDaylight::SunToward(const double H)
{
	const FVector S = SunSceneKit(H);
	return AcSpace::AxesFromSceneKit(S.X, S.Y, S.Z).GetSafeNormal();
}

AAcDaylight::AAcDaylight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Root->SetMobility(EComponentMobility::Movable);

	auto MakeLight = [this](const TCHAR* Name) {
		UDirectionalLightComponent* L = CreateDefaultSubobject<UDirectionalLightComponent>(Name);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		return L;
	};
	// The sun and the moon on the ground: the shadows (virtual shadow maps).
	Sun = MakeLight(TEXT("Sun"));
	Sun->bAtmosphereSunLight = false;
	Sun->LightSourceAngle = 0.5357f;
	Sun->CastShadows = true;
	Sun->VolumetricScatteringIntensity = 0.0f;
	// The cloud shadows (a light function, set in BeginPlay) everywhere the
	// eye goes, never faded to the half-lit default.
	Sun->LightFunctionFadeDistance = 1.0e7f;
	Sun->DisabledBrightness = 1.0f;
	Moon = MakeLight(TEXT("Moon"));
	Moon->bAtmosphereSunLight = false;
	Moon->LightSourceAngle = 0.52f;
	Moon->CastShadows = false;
	Moon->VolumetricScatteringIntensity = 0.0f;
	Moon->LightFunctionFadeDistance = 1.0e7f;
	Moon->DisabledBrightness = 1.0f;
	// The same in the sky: atmosphere lights 0 and 1, lighting no geometry
	// (no lighting channel), but the sky, the fog's light shafts and the
	// clouds.
	auto MakeSkyLight = [&](const TCHAR* Name, int32 Index) {
		UDirectionalLightComponent* L = MakeLight(Name);
		L->bAtmosphereSunLight = true;
		L->AtmosphereSunLightIndex = Index;
		L->CastShadows = false;
		L->LightingChannels.bChannel0 = false;
		L->LightingChannels.bChannel1 = false;
		L->LightingChannels.bChannel2 = false;
		L->bAffectTranslucentLighting = false;
		return L;
	};
	SkySun = MakeSkyLight(TEXT("SkySun"), 0);
	SkySun->LightSourceAngle = 0.5357f;
	SkyMoon = MakeSkyLight(TEXT("SkyMoon"), 1);
	SkyMoon->LightSourceAngle = 0.52f;
	SkyMoon->AtmosphereSunDiskColorScale = FLinearColor(0.9f, 0.92f, 1.0f);
	// No shafts of moonlight through the clouds' shadow in the night's fog.
	SkyMoon->VolumetricScatteringIntensity = 0.0f;
	// The fill: from the side opposite the noon sun, no shadows, lights
	// neither the sky nor the fog.
	Fill = MakeLight(TEXT("Fill"));
	Fill->bAtmosphereSunLight = false;
	Fill->CastShadows = false;
	Fill->VolumetricScatteringIntensity = 0.0f;
	Fill->bAffectTranslucentLighting = true;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = SLS_CapturedScene;
	// What the capture cannot see below the horizon: the ground's own
	// bounce is Lumen's; keep the lower hemisphere dim, not black.
	SkyLight->bLowerHemisphereIsBlack = false;
	SkyLight->LowerHemisphereColor = FLinearColor(0.05f, 0.045f, 0.04f);

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);
	Fog->SetMobility(EComponentMobility::Movable);
	// A light ground haze: the map stays readable from the top-down camera
	// (about 50 m up), the far border and the pilot's horizon melt into it.
	Fog->FogDensity = FogDensity;
	Fog->FogHeightFalloff = 0.35f;
	Fog->StartDistance = 2500.0f;
	Fog->FogMaxOpacity = 0.85f;
	// Not on the sky dome (25 km): the pilot's veil (E4) paints the far
	// ground with the bare atmosphere, so a fogged dome showed the far
	// mountains as a paler outline against it (B9).
	Fog->FogCutoffDistance = 2.0e6f;
	Fog->bEnableVolumetricFog = true;
	Fog->VolumetricFogScatteringDistribution = 0.35f;
	Fog->VolumetricFogExtinctionScale = 0.6f;
	Fog->VolumetricFogDistance = 12000.0f;
	Fog->SkyAtmosphereAmbientContributionColorScale = FLinearColor(0.35f, 0.35f, 0.35f);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);
	// The cloud layer (km): from a rain deck's base at 1.6 km to the storm's
	// tops at 5 km; fair-weather clouds sit 0.9 km up in it (M_AcCloud's
	// `CloudFine.z`: at 1.6 the pilot's sky felt like a low ceiling).
	Clouds->LayerBottomAltitude = CVarCloudBottom.GetValueOnAnyThread();
	Clouds->LayerHeight = CVarCloudHeight.GetValueOnAnyThread();
	// The light reddens across the layer at dusk, not once for all of it.
	Clouds->bUsePerSampleAtmosphericLightTransmittance = true;
	// Self-shadow rays need not cross the whole layer.
	Clouds->ShadowTracingDistance = 4.0f;
	// The bases take the warm ground's bounce (the material's
	// GroundContribution) and less of the sky's light.
	Clouds->GroundAlbedo = CloudGroundAlbedo;
	Clouds->SkyLightCloudBottomOcclusion = CloudBottomOcclusion;

	Dome = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dome"));
	Dome->SetupAttachment(Root);
	Dome->SetMobility(EComponentMobility::Movable);
	Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Dome->SetCastShadow(false);
	Dome->bAffectDistanceFieldLighting = false;
	Dome->bAffectDynamicIndirectLighting = false;
	Dome->bVisibleInRayTracing = false;
	Dome->bNeverDistanceCull = true;
	Dome->SetGenerateOverlapEvents(false);
	// The basic sphere is 50 cm across its radius: 5 · 10^4 times that is a
	// 25 km dome, past everything but the clouds' far edge.
	Dome->SetRelativeScale3D(FVector(5.0e4));

	Exposure = CreateDefaultSubobject<UPostProcessComponent>(TEXT("Exposure"));
	Exposure->SetupAttachment(Root);
	Exposure->bUnbound = true;
	Exposure->Priority = -10.0f;
	FPostProcessSettings& P = Exposure->Settings;
	P.bOverride_AutoExposureMethod = true;
	P.AutoExposureMethod = AEM_Manual;
	P.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	P.AutoExposureApplyPhysicalCameraExposure = false;
	P.bOverride_AutoExposureBias = true;
	P.AutoExposureBias = 0.0f;
	// The Swift camera's grading (`CameraRig.makeCamera`): no exposure
	// adaptation, saturation 1.12, contrast +0.12, gentle bloom over 0.95.
	// SceneKit has no filmic curve (it clips): part of Unreal's is kept so
	// highlights roll off, but less of it, so the ground keeps its colour.
	P.bOverride_ColorSaturation = true;
	P.ColorSaturation = FVector4(1.12, 1.12, 1.12, 1.0);
	// Keep bright cores (explosions, flames) white instead of crushing their blue (C3).
	P.bOverride_ColorSaturationHighlights = true;
	P.ColorSaturationHighlights = FVector4(0.85, 0.85, 0.85, 1.0);
	P.bOverride_ColorContrast = true;
	P.ColorContrast = FVector4(1.12, 1.12, 1.12, 1.0);
	P.bOverride_ToneCurveAmount = true;
	P.ToneCurveAmount = 0.6f;
	P.bOverride_BloomIntensity = true;
	P.BloomIntensity = 0.5f;
	P.bOverride_BloomThreshold = true;
	P.BloomThreshold = 0.95f;
	P.bOverride_VignetteIntensity = true;
	P.VignetteIntensity = 0.0f;
	P.bOverride_LocalExposureHighlightContrastScale = true;
	P.LocalExposureHighlightContrastScale = 1.0f;
	P.bOverride_LocalExposureShadowContrastScale = true;
	P.LocalExposureShadowContrastScale = 1.0f;
}

FLinearColor AAcDaylight::GetHazeRise() const
{
	FLinearColor H = FLinearColor::FromSRGBColor(Tones.Haze.ToFColor(false)) * CVarHazeGain.GetValueOnGameThread();
	H.A = float(Tones.Rise);
	return H;
}

AAcDaylight* AAcDaylight::Find(const UWorld* World)
{
	if (!World) return nullptr;
	TActorIterator<AAcDaylight> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

AAcDaylight* AAcDaylight::SpawnFor(UWorld* World)
{
	if (!World) return nullptr;
	if (AAcDaylight* Existing = Find(World)) return Existing;
	float Held = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("AcHour="), Held))
	{
		CVarHour->Set(Held, ECVF_SetByCommandline);
	}
	FActorSpawnParameters Params;
	Params.Name = TEXT("AcDaylight");
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAcDaylight* Daylight = World->SpawnActor<AAcDaylight>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
	return Daylight;
}

void AAcDaylight::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Cloud = LoadObject<UMaterialInterface>(nullptr, AcCloudMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		CloudMaterial = UMaterialInstanceDynamic::Create(Cloud, this);
		Clouds->SetMaterial(CloudMaterial);
		if (UMaterialInterface* Shadow = LoadObject<UMaterialInterface>(nullptr, AcCloudShadowMaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			CloudShadowMaterial = UMaterialInstanceDynamic::Create(Shadow, this);
		}
	}
	else
	{
		UE_LOG(LogAutocraft, Warning, TEXT("daylight: engine clouds (%s missing: run Tools/Editor/make_cloud_material.py)"), AcCloudMaterialPath);
		if (UMaterialInterface* Engine = LoadObject<UMaterialInterface>(nullptr, CloudMaterialPath)) Clouds->SetMaterial(Engine);
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, DomeMeshPath);
	UMaterialInterface* SkyMaterial = LoadObject<UMaterialInterface>(nullptr, SkyMaterialPath);
	if (Sphere && SkyMaterial)
	{
		Dome->SetStaticMesh(Sphere);
		DomeMaterial = UMaterialInstanceDynamic::Create(SkyMaterial, this);
		Dome->SetMaterial(0, DomeMaterial);
	}
	else
	{
		UE_LOG(LogAutocraft, Warning, TEXT("daylight: no sky dome (%s missing: run Tools/Editor/make_sky_material.py)"), SkyMaterialPath);
		Dome->SetVisibility(false);
	}
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	Update(Sim ? Sim->GameTime() : 0.0);
	UE_LOG(LogAutocraft, Log, TEXT("daylight: hour %.2f%s, sun %.1f°, dark %.2f"), Hour,
	       HeldHour() >= 0 ? TEXT(" (held)") : TEXT(""), Elevation, Dark);
}

void AAcDaylight::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (StandInChecks < 120)
	{
		++StandInChecks;
		RemoveStandIns();
	}
	StageSkyView();
	const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
	Update(Sim ? Sim->GameTime() : 0.0);
}

// Other chunks put up a stand-in sun and sky for their renders when the
// level has none (`-AcTerrainView`, AcTerrain.cpp): this actor is the sky,
// so those go.
void AAcDaylight::RemoveStandIns()
{
	UWorld* World = GetWorld();
	if (!World) return;
	TArray<AActor*> Doomed;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It) Doomed.Add(*It);
	for (TActorIterator<ASkyLight> It(World); It; ++It) Doomed.Add(*It);
	for (TActorIterator<ASkyAtmosphere> It(World); It; ++It) Doomed.Add(*It);
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It) Doomed.Add(*It);
	for (AActor* A : Doomed)
	{
		UE_LOG(LogAutocraft, Log, TEXT("daylight: removing stand-in %s"), *A->GetName());
		A->Destroy();
	}
}

// -AcSkyView[=YAW] (with -AcShot): a camera 2 m over the middle of the map
// looking 12° up toward YAW (degrees, Unreal; default: the sun's azimuth at
// the hour, or the moon's at night), to show the sky, the clouds and the
// stars, which the top-down view never sees.
void AAcDaylight::StageSkyView()
{
	const TCHAR* Cmd = FCommandLine::Get();
	float Yaw = 0;
	const bool bYaw = FParse::Value(Cmd, TEXT("AcSkyView="), Yaw);
	if (!bYaw && !FParse::Param(Cmd, TEXT("AcSkyView"))) return;
	UWorld* World = GetWorld();
	if (!World || SkyViewFrames > 200) return;
	++SkyViewFrames;
	if (!SkyCamera)
	{
		FVector At = FVector::ZeroVector;
		if (const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this); Sim && Sim->IsRunning())
		{
			const ac::GroundRect& B = Sim->Map().bounds;
			At = AcSpace::ToWorld(ac::Vec2((B.minX + B.maxX) / 2, (B.minZ + B.maxZ) / 2), 0);
		}
		if (const AAcTerrain* Terrain = AAcTerrain::Find(World); Terrain && Terrain->IsBuilt())
		{
			At.Z = Terrain->GroundZ(At);
		}
		At.Z += 200;
		const FVector Toward = Elevation > 0 ? SunToward(Hour) : -SunToward(Hour);
		const double Look = bYaw ? Yaw : Toward.Rotation().Yaw;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Camera = World->SpawnActor<ACameraActor>(At, FRotator(12, Look, 0), Params);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		Camera->GetCameraComponent()->SetFieldOfView(80);
		SkyCamera = Camera;
		UE_LOG(LogAutocraft, Log, TEXT("daylight: sky view from %s, yaw %.0f"), *At.ToString(), Look);
	}
	if (APlayerController* PC = World->GetFirstPlayerController(); PC && PC->GetViewTarget() != SkyCamera)
	{
		PC->SetViewTarget(SkyCamera);
	}
}

void AAcDaylight::Update(const double Time)
{
	Hour = HourAt(Time);
	const FVector SunDir = SunToward(Hour);
	const double Deg = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(SunDir.Z, -1.0, 1.0)));
	Elevation = Deg;
	const double Abs = FMath::Abs(Deg);
	// 1 by day, 0 by night, through the twilight; the dusk glow peaks with
	// the sun on the horizon.
	const double Light = Smoothstep(-9, 8, Deg);
	const double Glow = Smoothstep(0, 1, 1 - FMath::Abs(Deg - 2) / 11);
	// As the core has it (`Rules::dark`): the sight closes in with it.
	Dark = ac::Rules::dark(Hour);
	auto Tone = [&](const FLinearColor& Day, const FLinearColor& Night, const FLinearColor& Dusk) {
		return Mix(Mix(Night, Day, Light), Dusk, Glow * 0.85);
	};
	auto ToneV = [&](double Day, double Night, double Dusk) {
		return Tone(Rgb(Day, Day, Day), Rgb(Night, Night, Night), Rgb(Dusk, Dusk, Dusk)).R;
	};
	// The weather (AcWeather.h, docs/weather.md): under rain and storm cloud
	// the light is dimmer and flatter, the haze greyer and thicker.
	AAcWeather* WeatherActor = AAcWeather::Find(GetWorld());
	Weather = WeatherActor ? WeatherActor->Sample(Time) : ac::WeatherSample();
	const double Gloom = Weather.gloom;
	auto Grey = [](const FLinearColor& C, double T) {
		const float L = (C.R + C.G + C.B) / 3.0f;
		return Mix(C, FLinearColor(L, L, L * 1.04f, C.A), T);
	};

	// The sun, warmer as it sinks; the moon opposite. Shadows fade as the
	// light nears the horizon (they grow very long and sweep fast there).
	const double Fade = Smoothstep(0, 12, Abs);
	TurnShadowLight(Sun, -SunDir, CVarShadowStepMetres.GetValueOnGameThread());
	// The ringlight (docs/mmorpg-m6.md V2): the night's key comes from
	// between the sun's antipode and the gas giant, whose lit face throws
	// it, stronger the fuller the giant.
	const FVector Giant = FAcGasGiant::Direction();
	const FVector RingFrom = (-SunDir + Giant).GetSafeNormal();
	const double GiantLit = FAcGasGiant::Lit(SunDir);
	TurnShadowLight(Moon, -RingFrom, CVarShadowStepMetresMoon.GetValueOnGameThread());
	SkySun->SetWorldRotation((-SunDir).Rotation());
	SkyMoon->SetWorldRotation(SunDir.Rotation());
	const int32 Isolate = CVarIsolate.GetValueOnGameThread();
	const double KeyGain = (Isolate == 0 || Isolate == 1) ? 1.0 : 0.0;
	const double FillGain = (Isolate == 0 || Isolate == 2) ? 1.0 : 0.0;
	const double SkyGain = (Isolate == 0 || Isolate == 3) ? 1.0 : 0.0;
	// On the ground: the Swift lights, colours and strengths as they are.
	// The gas giant between us and the sun (it sits where the sun rises): dim, not dark.
	// And the ringshadow's band over the camera (AAcRingshadow) dims it more.
	const double Eclipsed = (1.0 - 0.55 * FAcGasGiant::Eclipse(SunDir)) * FMath::Clamp(RingshadowSun, 0.0, 1.0);
	const double SunFull = Deg > 0 ? Eclipsed * 1900 * FMath::Pow(Smoothstep(0, 9, Abs), 0.6) : 0.0;
	const double MoonFull = Deg < 0 ? 650 * FMath::Pow(Smoothstep(0, 10, Abs), 0.6) * (0.6 + 0.8 * GiantLit) : 0.0;
	Sun->SetIntensity(float(SunFull * SunLux * KeyGain * FMath::Lerp(1.0, 0.3, Gloom)));
	Sun->SetLightColor(Lin(Mix(Rgb(1, 0.5, 0.28), Rgb(1, 0.93, 0.82), Smoothstep(0, 32, Deg > 0 ? Abs : 0))));
	Moon->SetIntensity(float(MoonFull * MoonLux * KeyGain * FMath::Lerp(1.0, 0.35, Gloom)));
	// Under a rain deck there is no sun disc to mirror: the wet ground and
	// the puddles flared white with the full highlight.
	Sun->SetSpecularScale(float(FMath::Lerp(1.0, 0.1, Gloom)));
	Moon->SetSpecularScale(float(FMath::Lerp(1.0, 0.1, Gloom)));
	// Cold blue, warmed a little by the giant's amber bands.
	Moon->SetLightColor(Lin(Mix(Rgb(0.5, 0.64, 1), Rgb(1.0, 0.8, 0.58), 0.22)));
	// In the sky: the atmosphere dims a low sun or moon a great deal, and the
	// sky the sky light captures with it; lift it near the horizon so the
	// ambient follows Swift's tone instead. The sun still lights the sky a
	// while after it has set (the twilight).
	auto Lift = [](double E) { return 1.0 + LowLightLift * (1.0 - Smoothstep(0, 25, E)); };
	// Behind a storm's cloud the sky itself is darker.
	const double SkyGloom = FMath::Lerp(1.0, 0.45, Gloom);
	SkySun->SetIntensity(float(Eclipsed * SkySunLux * Lift(FMath::Max(Deg, 0.0)) * Smoothstep(-12, 0, Deg) * SkyGloom));
	SkyMoon->SetIntensity(float(SkyMoonLux * Lift(FMath::Max(-Deg, 0.0)) * Smoothstep(-12, 0, -Deg) * SkyGloom));
	// The atmosphere's sun is dim enough for the sky's colour, too dim
	// against the sky light for a cloud's sunlit side to stand out. Under a
	// rain deck there is no sunlit side: the lift goes, and then some.
	// Only a high sun: a low one's red light (lifted for the sky, see
	// LowLightLift) turned dusk clouds neon, so the clouds take less of it;
	// and the moon's lifted made night clouds read as day's.
	// A reference's clouds take the sky's light as it is.
	const bool bReference = !CVarCloudReference.GetValueOnGameThread().IsEmpty();
	const double Lifted = FMath::Lerp(0.6, double(CVarCloudSun.GetValueOnGameThread()), Smoothstep(8, 30, Deg));
	const float CloudSun = bReference ? 1.0f : float(FMath::Lerp(Lifted, 0.4, Smoothstep(0, 0.5, Gloom)));
	const float CloudMoon = bReference ? 1.0f : float(FMath::Lerp(1.0, 0.4, Smoothstep(0, 0.5, Gloom)));
	if (!FMath::IsNearlyEqual(SkySun->CloudScatteredLuminanceScale.R, CloudSun, 0.01f) ||
		!FMath::IsNearlyEqual(SkyMoon->CloudScatteredLuminanceScale.R, CloudMoon, 0.01f))
	{
		SkySun->CloudScatteredLuminanceScale = FLinearColor(CloudSun, CloudSun, CloudSun);
		SkyMoon->CloudScatteredLuminanceScale = FLinearColor(CloudMoon, CloudMoon, CloudMoon);
		SkySun->MarkRenderStateDirty();
		SkyMoon->MarkRenderStateDirty();
	}
	Sun->SetShadowAmount(float(0.85 * Fade));
	Moon->SetShadowAmount(float(0.75 * Fade));
	const bool bDay = Deg > 0;
	if (bDay != bSunShadows)
	{
		bSunShadows = bDay;
		Sun->SetCastShadows(bDay);
		Moon->SetCastShadows(!bDay);
	}
	// The one directional light forward-shaded translucency, water and
	// volumetric fog use: the sun by day, the moon by night, never the sky
	// pair or the fill (equal priorities fall back to the brightest, with an
	// on-screen warning). The setters do nothing while the value holds.
	Sun->SetForwardShadingPriority(bDay ? 2 : 1);
	Moon->SetForwardShadingPriority(bDay ? 1 : 2);

	// The cool fill from the opposite side keeps shadowed faces readable.
	Fill->SetWorldRotation((-AcSpace::AxesFromSceneKit(50, 40, 60).GetSafeNormal()).Rotation());
	Fill->SetLightColor(Lin(Tone(Rgb(0.55, 0.65, 0.9), Rgb(0.32, 0.44, 1), Rgb(0.9, 0.55, 0.62))));
	Fill->SetIntensity(float(ToneV(450, 210, 360) * FillLux * FillGain * FMath::Lerp(1.0, 0.7, Gloom)));

	// The ambient: the captured sky, at Swift's ambient tone (relative to day).
	// Only once the sun is well down: at dusk the captured sky is still lit.
	const double NightLift = 1.0 + (CVarNightAmbient.GetValueOnGameThread() - 1.0) * (1.0 - Smoothstep(-18, -8, Deg));
	// Under cloud a little dimmer; a lightning flash lights the whole sky.
	const double WeatherAmbient = FMath::Lerp(1.0, 0.8, Gloom) * (1.0 + 0.8 * Weather.flash);
	SkyLight->SetIntensity(float(ToneV(0.75, 0.12, 0.4) / 0.75 * SkyLightScale * CVarAmbient.GetValueOnGameThread() * NightLift * SkyGain * WeatherAmbient));
	// At dawn and dusk the captured sky is red, and the sun on the ground is
	// already the Swift orange: cool the ambient so shade is not red too.
	const double Blue = CVarAmbientBlue.GetValueOnGameThread();
	SkyLight->SetLightColor(Grey(Mix(Rgb(1, (1 + Blue) / 2, Blue), Rgb(0.8, 1.0, 1.3), Glow * 0.8 * (1.0 - Gloom)), Gloom * 0.6));

	// The haze: the background, the fog, the terrain's border. Top down as in
	// Swift; the first-person haze is in the tones.
	// Night: a deep blue the land stands against in silhouette, not black.
	Haze = Tone(Rgb(0.27, 0.22, 0.19), Rgb(0.05, 0.068, 0.13), Rgb(0.3, 0.16, 0.12));
	// Rain greys the haze and darkens it a little.
	Haze = Grey(Haze, Gloom * 0.7) * float(FMath::Lerp(1.0, 0.8, Gloom));
	const FLinearColor HazeLinear = FLinearColor::FromSRGBColor(Haze.ToFColor(false));
	Fog->SetFogInscatteringColor(HazeLinear);
	Fog->SetVolumetricFogAlbedo(Tone(Rgb(0.95, 0.9, 0.85), Rgb(0.6, 0.7, 1.0), Rgb(1.0, 0.8, 0.7)).ToFColor(false));
	Fog->SetFogDensity(FogDensity * CVarFog.GetValueOnGameThread() * float(1.0 + 2.0 * Weather.rain + 0.6 * Gloom));
	Fog->SetVolumetricFog(CVarVolumetricFog.GetValueOnGameThread() != 0);

	if (const UWorld* World = GetWorld())
	{
		if (AAcTerrain* Terrain = AAcTerrain::Find(World); Terrain && Terrain->GetMaterial())
		{
			// On every ground set's material (W4: the regions round the
			// pilot's too), again whenever the sets change.
			const UAcSimSubsystem* Sim = UAcSimSubsystem::Get(this);
			const bool bSets = Terrain->MaterialsVersion() != TerrainVersion;
			TerrainVersion = Terrain->MaterialsVersion();
			const double Reach = Sim && Sim->IsRunning() ? AAcTerrain::BorderWidth(Sim->Map().bounds) : 90.0;
			const FLinearColor Want(Haze.R, Haze.G, Haze.B, float(Reach));
			const bool bHaze = bSets || !Want.Equals(TerrainHaze, 1.0f / 512);
			TerrainHaze = Want;
			// The wet ground (M_Terrain's `Wetness`): darker, glossy, puddles.
			const float Wet = float(Weather.wetness * CVarWet.GetValueOnGameThread());
			const float Drops = float(Weather.rain);
			const bool bWet = bSets || FMath::Abs(Wet - TerrainWet) > 1.0f / 256 || FMath::Abs(Drops - TerrainRain) > 1.0f / 256;
			if (bWet)
			{
				TerrainWet = Wet;
				TerrainRain = Drops;
			}
			Terrain->ForEachMaterial([&](UMaterialInstanceDynamic* M, const int64 Region) {
				if (bHaze) M->SetVectorParameterValue(TEXT("Haze"), Want);
				if (bWet)
				{
					M->SetScalarParameterValue(TEXT("Wetness"), Wet);
					M->SetScalarParameterValue(TEXT("RainRate"), Drops);
					M->SetScalarParameterValue(TEXT("Ripples"), CVarRipples.GetValueOnGameThread());
					M->SetScalarParameterValue(TEXT("Gloss"), CVarGloss.GetValueOnGameThread());
				}
				// Its region's ground palette, blended toward the neighbours' at the edges.
				if (const TOptional<AAcTerrain::FGroundOverride>& Over = Terrain->GetGroundOverride(); Over && M == Terrain->GetMaterial())
					AcGroundLook::ApplyGround(M, Over->Own, Over->Around, Hour, Dark);
				else if (Region != INDEX_NONE || M == Terrain->GetMaterial()) AcGroundLook::Apply(M, Sim, Region, Hour, Dark);
			});
		}
	}

	// The first-person sky (Swift `Daylight.Sky`), for the pilot's veil.
	Tones.Top = Tone(Rgb(0.07, 0.15, 0.34), Rgb(0.008, 0.012, 0.035), Rgb(0.08, 0.1, 0.26));
	Tones.High = Tone(Rgb(0.2, 0.34, 0.6), Rgb(0.015, 0.025, 0.06), Rgb(0.3, 0.28, 0.45));
	Tones.Horizon = Tone(Rgb(0.52, 0.6, 0.72), Rgb(0.05, 0.06, 0.11), Rgb(0.95, 0.5, 0.28));
	// Under a rain deck the haze is the murk under it, dark and grey, and it
	// rises high (below): the sky dome's horizon and the pilot's veil over the
	// far ground take it, so the far land is lost in the storm's grey. With
	// the fair sky's haze the far ridges under the deck read as a clear blue
	// strip (side by side, docs/weather.md "Clouds (2026-10-18)").
	Tones.Haze = Grey(Tone(Rgb(0.62, 0.6, 0.58), Rgb(0.07, 0.09, 0.16), Rgb(0.62, 0.4, 0.3)), Gloom * 0.85) * float(FMath::Lerp(1.0, 0.5, Gloom));
	// Full cover hides the sunset.
	Tones.Glow = Rgb(0.9, 0.38, 0.16) * float(Glow * (1.0 - 0.85 * Smoothstep(0.6, 0.97, Weather.cover)));
	Tones.Cloud = Tone(Rgb(0.96, 0.95, 0.93), Rgb(0.07, 0.08, 0.12), Rgb(1, 0.66, 0.5));
	Tones.Shade = Tone(Rgb(0.66, 0.69, 0.76), Rgb(0.035, 0.04, 0.065), Rgb(0.5, 0.36, 0.42));
	Tones.Sun = SunDir;
	Tones.Stars = 1 - Smoothstep(-14, -3, Deg);
	Tones.Rise = FMath::Lerp(24 + (12 - 24) * Light, 32.0, Gloom);
	// Night closes in round the driven unit: the ground clear to a quarter
	// of it (6.5 cells) and lost by 26 (`ac.NightReach`) in the night's blue
	// haze, so the land near the pilot reads in the ringlight (V2: at 10
	// cells into near black the night was a void).
	const double NightReach = FMath::Max(1.0, double(CVarNightReach.GetValueOnGameThread()));
	Tones.Reach = (NightReach + (110 - NightReach) * Light) * FMath::Lerp(1.0, 0.45, Weather.rain);

	UpdateClouds(Time, SunDir);

	Exposure->Settings.ToneCurveAmount = CVarToneCurve.GetValueOnGameThread();
	if (DomeMaterial)
	{
		DomeMaterial->SetVectorParameterValue(TEXT("Haze"), GetHazeRise());
		DomeMaterial->SetScalarParameterValue(TEXT("Stars"), float(Tones.Stars));
		DomeMaterial->SetScalarParameterValue(TEXT("Time"), float(FMath::Fmod(Time, 86400.0)));
		if (!GasGiant) GasGiant = MakeShared<FAcGasGiant>();
		GasGiant->Update(GetWorld(), this, DomeMaterial, SunDir, Dark);
		if (!SkyFx) SkyFx = MakeShared<FAcSkyFx>();
		SkyFx->Update(GetWorld(), this, DomeMaterial, SunDir, Dark);
	}
}

void AAcDaylight::ApplyCloudReference(const FString& Reference)
{
	AppliedCloudReference = Reference;
	UMaterialInterface* Material = CloudMaterial.Get();
	// The component's defaults: the engine's sample layer, 5 km up, 10 thick.
	float Bottom = 5, Height = 10;
	if (!Reference.IsEmpty())
	{
		TArray<FString> Parts;
		Reference.ParseIntoArray(Parts, TEXT(";"));
		const FString Path = Reference == TEXT("engine") ? FString(CloudMaterialPath) : Parts[0];
		if (Parts.Num() >= 3)
		{
			Bottom = FCString::Atof(*Parts[1]);
			Height = FCString::Atof(*Parts[2]);
		}
		if (UMaterialInterface* Other = LoadObject<UMaterialInterface>(nullptr, *Path)) Material = Other;
		else UE_LOG(LogAutocraft, Error, TEXT("daylight: no cloud material %s; our clouds"), *Path);
	}
	const bool bOurs = Material == CloudMaterial.Get();
	if (!Material) return;
	Clouds->SetMaterial(Material);
	Clouds->SetLayerBottomAltitude(bOurs ? CVarCloudBottom.GetValueOnGameThread() : Bottom);
	Clouds->SetLayerHeight(bOurs ? CVarCloudHeight.GetValueOnGameThread() : Height);
	Clouds->SetbUsePerSampleAtmosphericLightTransmittance(bOurs);
	Clouds->SetGroundAlbedo(bOurs ? CloudGroundAlbedo : FColor(170, 170, 170));
	Clouds->SetSkyLightCloudBottomOcclusion(bOurs ? CloudBottomOcclusion : 0.5f);
	Clouds->SetShadowTracingDistance(bOurs ? CVarCloudShadowKm.GetValueOnGameThread() : 15.0f);
	Clouds->SetViewSampleCountScale(1.0f);
	Clouds->SetShadowViewSampleCountScale(1.0f);
	UE_LOG(LogAutocraft, Log, TEXT("daylight: clouds %s (%s, layer %.1f + %.1f km)"), bOurs ? TEXT("ours") : *Reference,
		*Material->GetPathName(), Clouds->LayerBottomAltitude, Clouds->LayerHeight);
}

void AAcDaylight::UpdateClouds(const double Time, const FVector& SunDir)
{
	const bool bClouds = CVarClouds.GetValueOnGameThread() != 0;
	if (Clouds->IsVisible() != bClouds) Clouds->SetVisibility(bClouds);
	// A rain deck fills every ray with cloud (nothing to skip), so tracing it
	// cost 2.5 ms in the pilot's view at 5K against 0.8 for broken cloud;
	// its grey hides the grain fewer samples leave, which bright cumulus
	// edges show: half the view and shadow samples under a full deck (rain
	// and storm alike: by cover, not gloom).
	// A reference's clouds (`ac.Daylight.CloudReference`, docs/weather.md
	// "Clouds (2026-10-18)") under the same sun, sky and camera: its own
	// material and layer, no cloud shadows on the ground.
	if (const FString Reference = CVarCloudReference.GetValueOnGameThread(); Reference != AppliedCloudReference)
	{
		ApplyCloudReference(Reference);
	}
	if (!AppliedCloudReference.IsEmpty())
	{
		if (bCloudShadowsOn)
		{
			bCloudShadowsOn = false;
			Sun->SetLightFunctionMaterial(nullptr);
			Moon->SetLightFunctionMaterial(nullptr);
		}
		return;
	}
	const float Samples = float(FMath::Lerp(1.0, 0.5, Smoothstep(0.8, 0.96, Weather.cover)));
	const float ShadowSamples = Samples * CVarCloudShadowSteps.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(Clouds->ViewSampleCountScale, Samples, 0.01f) ||
		!FMath::IsNearlyEqual(Clouds->ShadowViewSampleCountScale, ShadowSamples, 0.01f))
	{
		Clouds->SetViewSampleCountScale(Samples);
		Clouds->SetShadowViewSampleCountScale(ShadowSamples);
	}
	// The layer (`ac.Daylight.CloudBottom`, `CloudHeight`).
	const float LayerBottom = CVarCloudBottom.GetValueOnGameThread(), LayerHeight = CVarCloudHeight.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(Clouds->LayerBottomAltitude, LayerBottom, 0.001f)) Clouds->SetLayerBottomAltitude(LayerBottom);
	if (!FMath::IsNearlyEqual(Clouds->LayerHeight, LayerHeight, 0.001f)) Clouds->SetLayerHeight(LayerHeight);
	const float ShadowKm = CVarCloudShadowKm.GetValueOnGameThread();
	if (!FMath::IsNearlyEqual(Clouds->ShadowTracingDistance, ShadowKm, 0.01f))
	{
		Clouds->ShadowTracingDistance = ShadowKm;
		Clouds->MarkRenderStateDirty();
	}

	// The weather: the cover drifts between broken and mostly clear over a
	// couple of days (two slow waves), unless held.
	// The weather's cover (AcWeather), unless held.
	const double Held = CVarCloudCover.GetValueOnGameThread();
	const double Cover = Held >= 0 ? FMath::Clamp(Held, 0.0, 1.0) : Weather.cover;

	// The coverage moves with the wind; the bodies a little faster, so they
	// boil as they go instead of sliding by as one picture. The shadow
	// material takes the same three vectors: it marches the same clouds.
	// The weather map runs along the wind, stretched CloudStretch times
	// (M_AcCloud's `CloudW`): its offset is the distance blown, wrapped at a
	// whole stretched tile.
	const FVector2D S = CloudWind * Weather.windKm * 1.15;
	const FLinearColor A(float(FMath::Fmod(Weather.windKm, WeatherKm * CloudStretch)), 0.0f,
		float(-FMath::Fmod(S.X, ShapeKm)), float(-FMath::Fmod(S.Y, ShapeKm)));
	const FLinearColor Wind(float(CloudWind.X), float(CloudWind.Y), float(CloudStretch), CVarCloudHeight.GetValueOnGameThread());
	// A storm's clouds: denser (darker undersides).
	const double Density = CloudDensity * CVarCloudDensity.GetValueOnGameThread() * FMath::Lerp(1.0, 1.8, Weather.storm);
	const FLinearColor B{float(Cover), float(Density), float(WeatherKm), float(ShapeKm)};
	// The ground's bounce on the cloud bases (the material's ground
	// contribution) assumes a sunlit ground; under a rain deck the ground is
	// in its shade, and the bounce lit the far deck's underside, seen edge
	// on, as a white strip along the horizon.
	const FColor GroundAlbedo = Mix(FLinearColor(CloudGroundAlbedo), FLinearColor::Black, Smoothstep(0.0, 0.6, Weather.gloom)).ToFColor(true);
	if (GroundAlbedo != Clouds->GroundAlbedo) Clouds->SetGroundAlbedo(GroundAlbedo);
	// The cloud type follows the weather: flat fair-weather cumulus when
	// clear, cumulus when cloudy (tall thin towers side-on read as curtains),
	// towers only in a storm.
	const double TypeBias = FMath::Lerp(-0.6, -0.25, Smoothstep(0.35, 0.7, Cover)) + 0.7 * Weather.storm;
	// A storm's clouds keep a third of the sky light at their base (Epic's
	// storm clouds' occlusion), not 0.6: heavy, dark undersides.
	const double StormLook = Smoothstep(0.3, 1.0, Weather.storm);
	const FLinearColor C{float(DetailKm), float(CloudErosion * CVarCloudErosion.GetValueOnGameThread()), float(TypeBias), float(StormLook)};
	if (CloudMaterial)
	{
		CloudMaterial->SetVectorParameterValue(TEXT("CloudA"), A);
		CloudMaterial->SetVectorParameterValue(TEXT("CloudB"), B);
		CloudMaterial->SetVectorParameterValue(TEXT("CloudC"), C);
		CloudMaterial->SetVectorParameterValue(TEXT("CloudW"), Wind);
		// The horizon's air over the far clouds (M_AcCloud's `Haze`, rising
		// as the sky dome's haze does): the first-person horizon tone at half
		// its light, darker under a rain deck. The dome's own haze tone was
		// brighter than the horizon reads and turned the far clouds into a
		// white wall (side by side, docs/weather.md).
		const FLinearColor CloudHaze = Lin(Tones.Horizon) * float(CloudHazeGain * FMath::Lerp(1.0, 0.4, Weather.gloom));
		CloudMaterial->SetVectorParameterValue(TEXT("Haze"), FLinearColor(CloudHaze.R, CloudHaze.G, CloudHaze.B, float(Tones.Rise)));
		// The far clouds thin out in front of the gas giant (M_AcCloud's `Giant`).
		const FVector G = FAcGasGiant::Direction();
		CloudMaterial->SetVectorParameterValue(TEXT("Giant"), FLinearColor(float(G.X), float(G.Y), float(G.Z),
			float(FMath::Cos(FMath::DegreesToRadians(FAcGasGiant::AngularRadiusDeg())))));
		// Swift's cloud tone (`Daylight.Sky.cloud`) for its hue only: the
		// atmosphere already dims and reddens the light reaching them, so
		// half the dusk warmth and none of the night's darkness.
		const FLinearColor Hue = Tones.Cloud / FMath::Max3(Tones.Cloud.R, Tones.Cloud.G, Tones.Cloud.B);
		// Rain cloud is grey; a storm's goes the dark blue-grey of Epic's
		// storm clouds (0.30, 0.34, 0.375).
		const FLinearColor Grey = Mix(Rgb(1, 1, 1), Hue, 0.5) * float(0.95 * FMath::Lerp(1.0, 0.55, Weather.gloom));
		CloudMaterial->SetVectorParameterValue(TEXT("Albedo"), Mix(Grey, Rgb(0.3, 0.3375, 0.375), StormLook));
		// Lightning lights the cloud from inside, round the strike: above the
		// ground where it hits, the lower part of the cloud.
		CloudMaterial->SetVectorParameterValue(TEXT("Glow"), FLinearColor(0.7f, 0.78f, 1.0f) * float(Weather.flash * CloudFlashGlow));
		if (Weather.flash > 0.0)
		{
			const AAcWeather* WeatherActor = AAcWeather::Find(GetWorld());
			FVector Cam = FVector::ZeroVector;
			if (const APlayerController* PC = GetWorld()->GetFirstPlayerController(); PC && PC->PlayerCameraManager)
			{
				Cam = PC->PlayerCameraManager->GetCameraLocation();
			}
			const double Az = WeatherActor ? WeatherActor->LastStrikeAzimuth() : 0.0, Km = WeatherActor ? WeatherActor->LastStrikeKm() : 5.0;
			const FVector2D At = FVector2D(Cam.X, Cam.Y) * 1.0e-5 + FVector2D(FMath::Cos(Az), FMath::Sin(Az)) * Km;
			CloudMaterial->SetVectorParameterValue(TEXT("Bolt"), FLinearColor(float(At.X), float(At.Y), LayerBottom + 0.8f, float(CloudFlashKm)));
		}
	}

	const double ShadowGain = CVarCloudShadows.GetValueOnGameThread();
	const bool bShadows = bClouds && CloudShadowMaterial && ShadowGain > 0;
	if (bShadows != bCloudShadowsOn)
	{
		bCloudShadowsOn = bShadows;
		Sun->SetLightFunctionMaterial(bShadows ? CloudShadowMaterial.Get() : nullptr);
		Moon->SetLightFunctionMaterial(bShadows ? CloudShadowMaterial.Get() : nullptr);
	}
	if (bShadows)
	{
		// Toward whichever light is up: the sun by day, the moon by night.
		const FVector Toward = SunDir.Z >= 0 ? SunDir : -SunDir;
		CloudShadowMaterial->SetVectorParameterValue(TEXT("CloudA"), A);
		CloudShadowMaterial->SetVectorParameterValue(TEXT("CloudB"), B);
		CloudShadowMaterial->SetVectorParameterValue(TEXT("CloudC"), C);
		CloudShadowMaterial->SetVectorParameterValue(TEXT("CloudW"), Wind);
		CloudShadowMaterial->SetVectorParameterValue(TEXT("ShadowL"), FLinearColor(float(Toward.X), float(Toward.Y), float(Toward.Z),
			float(FMath::Clamp(CloudShadowStrength * ShadowGain, 0.0, 1.0))));
		CloudShadowMaterial->SetVectorParameterValue(TEXT("ShadowC"), FLinearColor(LayerBottom, LayerHeight, float(CloudShadowSoftness), 0.0f));
	}
	ApplyCloudParams();
}

void AAcDaylight::ApplyCloudParams()
{
	const FString Spec = CVarCloudParam.GetValueOnGameThread();
	if (Spec.IsEmpty() && CloudParamsSet.IsEmpty()) return;
	TArray<FString> Items;
	Spec.ParseIntoArray(Items, TEXT(";"));
	TSet<FName> Applied;
	for (UMaterialInstanceDynamic* M : {CloudMaterial.Get(), CloudShadowMaterial.Get()})
	{
		if (!M || !M->Parent) continue;
		auto Default = [&](const FName Name) {
			FLinearColor V(0, 0, 0, 0);
			M->Parent->GetVectorParameterValue(FHashedMaterialParameterInfo(Name), V);
			return V;
		};
		TSet<FName> Now;
		for (const FString& Item : Items)
		{
			FString Name, Value;
			if (!Item.Split(TEXT("="), &Name, &Value)) continue;
			const FName Param(*Name.TrimStartAndEnd());
			FLinearColor V = Default(Param);
			TArray<FString> Parts;
			Value.ParseIntoArray(Parts, TEXT(":"), /*InCullEmpty*/ false);
			for (int32 I = 0; I < FMath::Min(4, Parts.Num()); ++I)
			{
				if (!Parts[I].TrimStartAndEnd().IsEmpty()) V.Component(I) = FCString::Atof(*Parts[I]);
			}
			M->SetVectorParameterValue(Param, V);
			Now.Add(Param);
		}
		// A name dropped from the list goes back to the material's default.
		for (const FName Old : CloudParamsSet)
		{
			if (!Now.Contains(Old)) M->SetVectorParameterValue(Old, Default(Old));
		}
		Applied.Append(Now);
	}
	CloudParamsSet = MoveTemp(Applied);
}
