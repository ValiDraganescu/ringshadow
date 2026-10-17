#include "AcPoseVehiclesSheet.h"

#include "AcKindTable.h"
#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcModelRow.h"
#include "AcPose.h"
#include "AcShot.h"
#include "AcSpace.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#endif

#include "TerrainField.h"
#include "Types.h"
#include "WindowMaps.h"

namespace
{
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* Key, const double Default)
	{
		double V = Default;
		return O->TryGetNumberField(Key, V) ? V : Default;
	}

	bool Flag(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		bool B = false;
		return O->TryGetBoolField(Key, B) && B;
	}

	TOptional<double> Maybe(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		double V = 0;
		if (O->TryGetNumberField(Key, V)) return V;
		return {};
	}
}

/// One copy of the sheet: its unit, the game around it and the renderer
/// memory, and the meshes that show its pose.
struct FAcPoseSheetCopy
{
	explicit FAcPoseSheetCopy(ac::Unit InUnit) : U(MoveTemp(InUnit)) {}
	ac::Unit U;
	ac::GameState State;
	FAcUnitMemory Memory;
	const FAcModelInfo* Model = nullptr;
	FAcPoseFn Fn = nullptr;
	FString Kind, Label;
	int32 Index = 0;
	double Time0 = 10.0, Stride0 = 0.0, Walk = 0.0, Speed = 0.0;
	int32 Warm = 0;
	bool bFirefly = false;
	ac::Vec2 At, Dir;
	/// Each mesh component of the copy and the part it shows.
	TArray<TPair<int32, TObjectPtr<UStaticMeshComponent>>> Parts;
	TArray<FName> Materials;
};

AAcPoseSheet::AAcPoseSheet()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}


void AAcPoseSheet::Place(FAcPoseSheetCopy& Copy, const double Elapsed)
{
	static const ac::MapDefinition Map = ac::WindowMaps::playground();
	static const ac::TerrainField Field(Map);
	ac::Unit& U = Copy.U;
	const double Time = Copy.Time0 + Elapsed;
	Copy.State.time = Time;
	if (Elapsed > 0.0) U.stride = Copy.Stride0 + Copy.Walk * Elapsed;
	FAcPose Pose;
	auto PoseAt = [&](const int32 Step)
	{
		// Warm-up steps drive up to the copy's place.
		if (Copy.bFirefly)
		{
			U.stride = 5.0 + double(Step) * Copy.Speed;
			U.position = Copy.At - Copy.Dir * (Copy.Speed * double(Copy.Warm + 1 - Step));
		}
		FAcPoseContext C{Copy.State, &U, nullptr, *Copy.Model, Field, Copy.Memory, Time, 0.0, false, 0};
		AcPose::RestUnit(C, Pose);
		if (Copy.Fn) Copy.Fn(C, Pose);
	};
	const int32 Steps = Copy.bFirefly && Elapsed == 0.0 ? Copy.Warm + 1 : 1;
	for (int32 Step = 1; Step <= Steps; ++Step) PoseAt(Step);

	// On flat ground: the placement less the playground's height here.
	FTransform Placement = Pose.Placement;
	Placement.AddToTranslation(FVector(0, 0, -AcSpace::ToCm(Field.height(U.position))));
	const FAcModelInfo& Model = *Copy.Model;
	TArray<FTransform> World;
	TArray<bool> Visible = Pose.Visible;
	World.SetNum(Model.Parts.Num());
	for (int32 P = 0; P < Model.Parts.Num(); ++P)
	{
		const int32 Parent = Model.Parts[P].Parent;
		World[P] = Pose.Local[P] * (Parent != INDEX_NONE ? World[Parent] : Placement);
		if (Parent != INDEX_NONE) Visible[P] = Visible[P] && Visible[Parent];
	}
	auto EmissionOf = [&](const int32 P, const FName Material)
	{
		float Emission = Pose.Emission[P];
		for (const FAcPose::FMeshEmission& Over : Pose.MeshEmission)
		{
			if (Over.Part == P && Over.Material == Material) Emission = Over.Value;
		}
		return Emission;
	};
	if (Copy.Parts.IsEmpty())
	{
		for (int32 P = 0; P < Model.Parts.Num(); ++P)
		{
			for (const FAcModelMesh& Mesh : Model.Parts[P].Meshes)
			{
				UStaticMesh* SM = Mesh.LoadMesh();
				if (!SM) continue;
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
				C->SetupAttachment(RootComponent);
				C->SetStaticMesh(SM);
				if (UMaterialInterface* Mat = Mesh.LoadMaterial()) C->SetMaterial(0, Mat);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetCustomPrimitiveDataFloat(AcModelData::TeamIndex, 0.f);
				C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, EmissionOf(P, Mesh.Material));
				C->SetWorldTransform(World[P]);
				C->SetVisibility(Visible[P]);
				C->RegisterComponent();
				Meshes.Add(C);
				Copy.Parts.Emplace(P, C);
				Copy.Materials.Add(Mesh.Material);
			}
		}
		UE_LOG(LogAutocraft, Log, TEXT("pose sheet: %s frame %d '%s' placed at %s"), *Copy.Kind, Copy.Index, *Copy.Label,
			*Placement.GetTranslation().ToString());
		return;
	}
	for (int32 I = 0; I < Copy.Parts.Num(); ++I)
	{
		const int32 P = Copy.Parts[I].Key;
		UStaticMeshComponent* C = Copy.Parts[I].Value;
		C->SetWorldTransform(World[P]);
		C->SetVisibility(Visible[P]);
		C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, EmissionOf(P, Copy.Materials[I]));
	}
}

void AAcPoseSheet::BeginPlay()
{
	Super::BeginPlay();
	FString Kind, Spec;
	if (FParse::Value(FCommandLine::Get(), TEXT("AcPoseSheet="), Kind)
		&& FParse::Value(FCommandLine::Get(), TEXT("AcPoseSheetSpec="), Spec))
	{
		Build(Kind, Spec);
	}
}

void AAcPoseSheet::Build(const FString& KindName, const FString& SpecFile)
{
	const TOptional<ac::UnitKind> Kind = AcKinds::Named(KindName);
	const FAcModelInfo* Model = FAcModelCatalog::Get().Find(FName(KindName + TEXT("_blue")));
	const FAcPoseFn Fn = Kind ? AcPose::Find(*Kind) : nullptr;
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!Kind || !Model || !FFileHelper::LoadFileToString(Text, *SpecFile)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		UE_LOG(LogAutocraft, Error, TEXT("pose sheet: no model %s or no spec %s"), *KindName, *SpecFile);
		return;
	}
	TSharedPtr<FJsonObject> Sheet;
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("sheets")))
	{
		if (V->AsObject()->GetStringField(TEXT("kind")) == KindName) Sheet = V->AsObject();
	}
	if (!Sheet)
	{
		UE_LOG(LogAutocraft, Error, TEXT("pose sheet: no sheet for %s in %s"), *KindName, *SpecFile);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>& Frames = Sheet->GetArrayField(TEXT("frames"));
	const int32 N = Frames.Num();
	const double Gap = Num(Sheet, TEXT("gap"), 1.5);
	// `heading` in the sheet turns the copies (radians; 0 faces along the row, a profile).
	const double Heading = Num(Sheet, TEXT("heading"), UE_DOUBLE_HALF_PI + 0.6);
	const ac::Vec2 Dir(std::cos(Heading), std::sin(Heading));

	bLive = Flag(Sheet, TEXT("live"));
	for (int32 K = 0; K < N; ++K)
	{
		const TSharedPtr<FJsonObject> F = Frames[K]->AsObject();
		TSharedPtr<FAcPoseSheetCopy> Copy = MakeShared<FAcPoseSheetCopy>(ac::Unit(7, *Kind, 0, ac::Vec2(0, 0), Heading, ac::Unit::Task::idle));
		Copy->Model = Model;
		Copy->Fn = Fn;
		Copy->Kind = KindName;
		Copy->Index = K;
		Copy->Time0 = Num(F, TEXT("time"), 10.0);
		Copy->At = ac::Vec2((double(K) - double(N - 1) / 2.0) * Gap, 0.0);
		Copy->Dir = Dir;
		const TOptional<double> Since = Maybe(F, TEXT("since"));
		Copy->Warm = int32(Num(F, TEXT("warm"), 0.0));
		Copy->Speed = Num(F, TEXT("speed"), 0.0);
		Copy->Walk = Num(F, TEXT("walk"), 0.0);
		Copy->bFirefly = *Kind == ac::UnitKind::firefly;
		const double Time = Copy->Time0;
		const ac::Vec2 At = Copy->At;
		const bool bFirefly = Copy->bFirefly;

		ac::Unit& U = Copy->U;
		U.position = At;
		U.stride = Num(F, TEXT("stride"), 0.0);
		Copy->Stride0 = U.stride;
		U.moving = bFirefly ? Flag(F, TEXT("moving")) : Flag(F, TEXT("walking"));
		if (Flag(F, TEXT("aiming")))
		{
			U.task = ac::Unit::Task::attacking;
			U.moving = false;
		}
		if (Flag(F, TEXT("mining")))
		{
			U.task = ac::Unit::Task::mining;
			U.timer = Num(F, TEXT("timer"), 5.0);
			U.moving = false;
		}
		if (const TOptional<double> J = Maybe(F, TEXT("jump")))
		{
			// A 2-cell leap along the heading, `jump` of the way over.
			U.jumpFrom = At - Dir * (*J * 2.0);
			U.jumpTo = *U.jumpFrom + Dir * 2.0;
		}
		if (bFirefly || *Kind == ac::UnitKind::atlas) U.aim = Heading + Num(F, TEXT("aim"), 0.0);
		// The new kinds: a Scorpion `anchor`ed (0-1 buried, `anchored` says
		// which way it goes), its `cooldown` running, its `lock` held that many
		// seconds on a target `lockAt` cells ahead; an Atlas `stomp`ed that many
		// seconds ago.
		if (const TOptional<double> A = Maybe(F, TEXT("anchor")))
		{
			U.anchor = *A;
			U.anchored = F->HasField(TEXT("anchored")) ? Flag(F, TEXT("anchored")) : true;
		}
		if (const TOptional<double> Cd = Maybe(F, TEXT("cooldown"))) U.cooldown = *Cd;
		if (const TOptional<double> St = Maybe(F, TEXT("stomp"))) U.stompedAt = Time - *St;

		ac::GameState& State = Copy->State;
		State.time = Time;
		if (const TOptional<double> Lock = Maybe(F, TEXT("lock")))
		{
			// A Ranger of the other side that far ahead, locked onto.
			const double Away = Num(F, TEXT("lockAt"), 3.0);
			State.units.push_back(ac::Unit(99, ac::UnitKind::ranger, 1, At + Dir * Away, Heading + UE_DOUBLE_PI, ac::Unit::Task::idle));
			U.lockTarget = 99;
			U.lockFrom = Time - *Lock;
			U.lockAt = Time;
			U.task = ac::Unit::Task::attacking;
			U.target = 99;
		}
		FAcUnitMemory& Memory = Copy->Memory;
		if (Since) Memory.LastShot = Time - *Since;
		if (*Kind == ac::UnitKind::juggernaut)
		{
			const double Twist = Num(F, TEXT("twist"), 0.0);
			if (Twist != 0.0 || Flag(F, TEXT("aiming")))
			{
				Memory.LastShotAt = At + ac::Vec2(std::cos(Heading + Twist), std::sin(Heading + Twist)) * 5.0;
			}
		}
		Memory.FlameLength = Num(F, TEXT("flameLength"), 3.0);
		// The turn rate held (no time passes between the sheet's poses).
		Memory.TurnRate = 4.0 * Num(F, TEXT("turn"), 0.0);
		Memory.LastHeading = Heading;
		Copy->Label = F->GetStringField(TEXT("label"));

		// A prop at rest ahead of the copy (an ore deposit under the drill).
		FString PropName;
		if (F->TryGetStringField(TEXT("prop"), PropName))
		{
			if (const FAcModelInfo* Prop = FAcModelCatalog::Get().Find(FName(PropName)))
			{
				const ac::Vec2 PropAt = At + Dir * Num(F, TEXT("propAt"), 1.5);
				FTransform Placement(FQuat::Identity, AcSpace::FromSceneKit(PropAt.x, 0, PropAt.y));
				Placement.SetRotation(FQuat(FVector::UpVector, Num(F, TEXT("propTurn"), 0.0)));
				TArray<FTransform> World;
				World.SetNum(Prop->Parts.Num());
				for (int32 P = 0; P < Prop->Parts.Num(); ++P)
				{
					const int32 Parent = Prop->Parts[P].Parent;
					World[P] = Prop->Parts[P].Rest * (Parent != INDEX_NONE ? World[Parent] : Placement);
					for (const FAcModelMesh& Mesh : Prop->Parts[P].Meshes)
					{
						UStaticMesh* SM = Mesh.LoadMesh();
						if (!SM) continue;
						UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
						C->SetupAttachment(RootComponent);
						C->SetStaticMesh(SM);
						if (UMaterialInterface* Mat = Mesh.LoadMaterial()) C->SetMaterial(0, Mat);
						C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						C->SetCustomPrimitiveDataFloat(AcModelData::TeamIndex, 0.f);
						C->SetCustomPrimitiveDataFloat(AcModelData::EmissionScale, 0.f);
						C->SetWorldTransform(World[P]);
						C->RegisterComponent();
						Meshes.Add(C);
					}
				}
			}
			else
			{
				UE_LOG(LogAutocraft, Warning, TEXT("pose sheet: no prop model %s"), *PropName);
			}
		}

		Place(*Copy, 0.0);
		Copies.Add(MoveTemp(Copy));
	}

	// Nothing else in the level: the model row and its floor hidden.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AAcModelRow>() || It->ActorHasTag(TEXT("AcFloor"))) It->SetActorHiddenInGame(true);
	}
	const double Cy = Num(Sheet, TEXT("cy"), 0.7), D = Num(Sheet, TEXT("D"), 30.0);
	VerticalFov = Num(Sheet, TEXT("fov"), 6.0);
	Aspect = Num(Sheet, TEXT("width"), 16.0) / Num(Sheet, TEXT("height"), 9.0);
	const FVector Target = AcSpace::FromSceneKit(0, Cy, 0);
	const FVector CamPos = AcSpace::FromSceneKit(0, Cy + 0.55 * D, D);
	const FVector SunPos = AcSpace::FromSceneKit(-4, 8, 6);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Camera = GetWorld()->SpawnActor<ACameraActor>(CamPos, (Target - CamPos).Rotation(), Params);
	Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	// The project keeps the vertical FOV (it derives it from this aspect).
	Camera->GetCameraComponent()->SetAspectRatio(float(Aspect));
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		It->GetLightComponent()->SetMobility(EComponentMobility::Movable);
		It->SetActorRotation((Target - SunPos).Rotation());
	}
	if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this))
	{
		Shot->Hold();
		bHolding = true;
	}
}

void AAcPoseSheet::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Camera) return;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->GetViewTarget() != Camera) PC->SetViewTarget(Camera);
		if (SettleFrames == 30 && PC->PlayerCameraManager)
		{
			UE_LOG(LogAutocraft, Log, TEXT("pose sheet: view %s fov %.2f at %s (camera fov %.2f)"),
				*GetNameSafe(PC->GetViewTarget()), PC->PlayerCameraManager->GetFOVAngle(),
				*PC->PlayerCameraManager->GetCameraLocation().ToString(), Camera->GetCameraComponent()->FieldOfView);
		}
	}
	// SceneKit's FOV is vertical; the sheet's own aspect (the viewport's
	// reported size can differ from the shot's under -RenderOffscreen).
	{
		const double Half = FMath::DegreesToRadians(VerticalFov / 2.0);
		Camera->GetCameraComponent()->SetFieldOfView(
			float(FMath::RadiansToDegrees(2.0 * FMath::Atan(FMath::Tan(Half) * Aspect))));
	}
	if (!bHolding)
	{
		if (bLive)
		{
			LiveTime += DeltaSeconds;
			for (const TSharedPtr<FAcPoseSheetCopy>& Copy : Copies) Place(*Copy, LiveTime);
		}
		return;
	}
	bool bBusy = false;
#if WITH_EDITOR
	bBusy = (GShaderCompilingManager && GShaderCompilingManager->IsCompiling())
		|| FAssetCompilingManager::Get().GetNumRemainingAssets() > 0;
#endif
	IStreamingManager::Get().StreamAllResources(0.0f);
	SettleFrames = bBusy ? 0 : SettleFrames + 1;
	if (SettleFrames > 90)
	{
		if (UAcShotSubsystem* Shot = UAcShotSubsystem::Get(this)) Shot->Release();
		bHolding = false;
	}
}

bool UAcPoseSheetSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FCString::Strifind(FCommandLine::Get(), TEXT("-AcPoseSheet=")) != nullptr;
}

void UAcPoseSheetSubsystem::OnWorldBeginPlay(UWorld& World)
{
	Super::OnWorldBeginPlay(World);
	if (!World.IsGameWorld()) return;
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	World.SpawnActor<AAcPoseSheet>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
}
