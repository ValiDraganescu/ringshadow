// W4, seamless regions (docs/mmorpg-m8.md, docs/worlds.md "In the game"):
// the regions round the pilot's, made ahead off the game's thread. Their
// maps are what the game draws beyond the walls (AAcTerrain's ground sets);
// the four across the passages also get a mirror (`Client::prepare`), so
// walking into one swaps it in instead of building the region then.
#include "AcMmo.h"

#include "AcLog.h"
#include "AcModelCatalog.h"
#include "AcPose.h"
#include "AcSpace.h"
#include "AcWork.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectGlobals.h"

#include "NetMirror.h"
#include "RegionFile.h"
#include "WorldAtlas.h"

namespace
{
	FString Utf(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }
}

const TMap<FString, FString>& FAcMmoClient::RegionTexts()
{
	if (bTextsRead) return Texts;
	bTextsRead = true;
	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (WorldFile.IsEmpty() || !FFileHelper::LoadFileToString(Json, *WorldFile) ||
		!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root || !Root->TryGetArrayField(TEXT("grid"), Rows))
	{
		UE_LOG(LogAutocraft, Warning, TEXT("mmo: no world file to draw the regions round from (%s)"), *WorldFile);
		return Texts;
	}
	for (const TSharedPtr<FJsonValue>& Row : *Rows)
	{
		for (const TSharedPtr<FJsonValue>& Entry : Row->AsArray())
		{
			FString File;
			if (!Entry->TryGetString(File)) continue;
			FString Body;
			if (!FFileHelper::LoadFileToString(Body, *FPaths::Combine(FPaths::GetPath(WorldFile), File))) continue;
			const ac::region::Parsed Parsed = ac::region::parse(TCHAR_TO_UTF8(*Body));
			if (Parsed.file) Texts.Add(Utf(Parsed.file->id), Body);
		}
	}
	return Texts;
}

ac::Vec2 FAcMmoClient::FrameOrigin() const
{
	const TOptional<FAcRegionAround> Here = HereGround();
	return Here && Session ? Here->Centre - *Session : ac::Vec2(0, 0);
}

TOptional<ac::Vec2> FAcMmoClient::TakeSeamless()
{
	TOptional<ac::Vec2> Out = SeamlessShift;
	SeamlessShift.Reset();
	return Out;
}

int64 FAcMmoClient::MirrorIndex() const
{
	const ac::net::WorldMessage* W = World();
	if (!W) return INDEX_NONE;
	if (!MirrorRegion.empty())
	{
		for (size_t I = 0; I < W->regions.size(); I++)
			if (W->regions[I].id == MirrorRegion) return int64(I);
		return INDEX_NONE;
	}
	return W->here >= 0 && W->here < int64(W->regions.size()) ? W->here : INDEX_NONE;
}

TOptional<FAcRegionAround> FAcMmoClient::HereGround() const
{
	const ac::net::WorldMessage* W = World();
	const int64 HereIndex = MirrorIndex();
	if (!W || HereIndex == INDEX_NONE) return {};
	const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(*W, {});
	const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, HereIndex);
	if (!P) return {};
	const ac::net::WorldMessage::Region& R = W->regions[size_t(HereIndex)];
	FAcRegionAround Out;
	Out.Id = Utf(R.id);
	Out.Index = HereIndex;
	Out.Centre = P->centre();
	const int64 Step[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
	for (int32 E = 0; E < 4; E++)
	{
		bool bThere = false;
		for (const ac::net::WorldMessage::Region& Q : W->regions)
			bThere = bThere || (Q.column == R.column + Step[E][0] && Q.row == R.row + Step[E][1]);
		if (!bThere) Out.Rim |= uint8(1 << E);
	}
	return Out;
}

void FAcMmoClient::PrepareAround()
{
	// What finished on the worker threads.
	for (auto It = Building.CreateIterator(); It; ++It)
	{
		if (!It.Value().IsReady()) continue;
		FRegionBuilt* Done = It.Value().Get();
		It.RemoveCurrent();
		if (!Done) continue;
		if (Done->Map) BuiltMaps.Add(Done->Id, Done->Map);
		if (Done->Mirror && Net)
		{
			// What the region's units and buildings are drawn with, loaded
			// now, off the game thread (K3: else at the crossing).
			{
				TArray<FName> Models;
				const ac::GameState& G = Done->Mirror->simulation().state;
				for (const ac::Unit& U : G.units) Models.AddUnique(FName(*FString::Printf(TEXT("%s_blue"), AcPose::ModelBase(U.kind))));
				for (const ac::Structure& S : G.structures)
				{
					Models.AddUnique(FName(*FString::Printf(TEXT("%s_blue"), AcPose::ModelBase(S.kind))));
					Models.AddUnique(FName(*FString::Printf(TEXT("scaffold_%s_blue"), AcPose::ModelBase(S.kind))));
				}
				FAcModelCatalog::Preload(Models);
			}
			// Made for the region the pilot stood in then: still of this world
			// (the description says), so still good.
			Net->prepare(std::move(Done->Mirror));
			MirrorsMade.Add(Done->Id);
			UE_LOG(LogAutocraft, Log, TEXT("mmo: %s made ahead (map and mirror)"), *Done->Id);
		}
		delete Done;
	}
	// The Brood's models (shardling, gloamwing, lodeback, matriarch), loaded
	// off the game thread a while after the regions round the first were
	// made: the server sends its bugs only in snapshots, so a region made
	// ahead can't say which come, and the first one seen after a crossing
	// loaded its model on the game thread (K3: a Lodeback, 68 meshes, a
	// 32 ms frame at every W4 crossing). Late enough to leave the session's
	// first seconds alone.
	{
		static double MadeAt = -1;
		static bool bBroodAsked = false;
		if (MadeAt < 0 && !MirrorsMade.IsEmpty() && Building.IsEmpty()) MadeAt = FPlatformTime::Seconds();
		if (!bBroodAsked && MadeAt > 0 && FPlatformTime::Seconds() - MadeAt > 8.0)
		{
			bBroodAsked = true;
			TArray<FName> Brood;
			for (const ac::UnitKind K : {ac::UnitKind::shardling, ac::UnitKind::gloamwing, ac::UnitKind::lodeback, ac::UnitKind::matriarch})
				Brood.Add(FName(*FString::Printf(TEXT("%s_blue"), AcPose::ModelBase(K))));
			FAcModelCatalog::Preload(Brood);
		}
	}
	const ac::net::WorldMessage* W = World();
	const ac::net::Mirror* M = Net ? Net->mirror() : nullptr;
	// The world map's pictures (SAcWorldMap::Picture loads one as it first
	// paints it): asked for as the world arrives, off the game thread, so a
	// map opened in the session's first seconds never waits for the loader
	// behind everything else it reads then (K3: 1-2 s a picture, packaged).
	if (W)
	{
		static TSet<FString> PicturesAsked;
		for (const ac::net::WorldMessage::Region& Q : W->regions)
		{
			const FString Asset = TEXT("T_AcMap_") + Utf(Q.id).Replace(TEXT("-"), TEXT("_"));
			if (PicturesAsked.Contains(Asset)) continue;
			PicturesAsked.Add(Asset);
			LoadPackageAsync(TEXT("/Game/UI/Map/") + Asset, FLoadPackageAsyncDelegate::CreateLambda(
				[Asset](const FName&, UPackage* Package, const EAsyncLoadingResult::Type Result)
				{
					// Rooted as the map's own load roots it.
					if (UTexture2D* T = Result == EAsyncLoadingResult::Succeeded && Package ? FindObject<UTexture2D>(Package, *Asset) : nullptr)
						T->AddToRoot();
				}));
		}
	}
	// Aboard the walk-in Dropship (C4): the target region (`flyingTo`, set
	// from the lift to the touch down) is made ahead too, map and mirror,
	// wherever it lies; the ground round the pilot's region stays.
	if (!W || !M) return;
	// The Welcome's region: `here` until the first RegionChange names one.
	if (MirrorRegion.empty() && W->here >= 0 && W->here < int64(W->regions.size()) && M->snapshotId() > 0)
		MirrorRegion = W->regions[size_t(W->here)].id;
	const TOptional<FAcRegionAround> Here = HereGround();
	if (!Here) return;
	// The world frame's anchor: the session's first region.
	if (!Session) Session = Here->Centre;
	const ac::net::WorldMessage::Region& R = W->regions[size_t(Here->Index)];
	const TMap<FString, FString>& All = RegionTexts();
	const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(*W, {});
	TArray<FAcRegionAround> Now;
	int64 Target = W->flyingTo ? *W->flyingTo : INDEX_NONE;
	// Fallen (P6): the region the pilot comes back in, made ahead the same
	// way while the respawn screen counts down (a home two regions from the
	// fall was else built on the game thread at the arrival: ~0.6 s).
	if (Target == INDEX_NONE)
		if (const ac::net::ProgressMessage* P = Progress(); P && P->respawn && P->respawn->region != R.id)
			for (int64 QI = 0; QI < int64(W->regions.size()); ++QI)
				if (W->regions[size_t(QI)].id == P->respawn->region) Target = QI;
	for (int64 QI = 0; QI < int64(W->regions.size()); ++QI)
	{
		const ac::net::WorldMessage::Region& Q = W->regions[size_t(QI)];
		const int64 DC = Q.column - R.column, DR = Q.row - R.row;
		const bool bTarget = QI == Target && QI != Here->Index;
		if (!bTarget && ((DC == 0 && DR == 0) || FMath::Abs(DC) > 1 || FMath::Abs(DR) > 1)) continue;
		const FString Id = Utf(Q.id);
		const FString* Body = All.Find(Id);
		if (!Body) continue;
		// Across a side (not a corner): a mirror made ahead too, as the
		// pilot may walk in; and the Dropship's target, first (F2).
		const bool bSide = (DC == 0) != (DR == 0);
		const bool bWantMirror = (bSide || bTarget) && !MirrorsMade.Contains(Id);
		// The target before the rest.
		const bool bMayBuild = Target == INDEX_NONE || bTarget || MirrorsMade.Contains(Utf(W->regions[size_t(Target)].id));
		const std::shared_ptr<const ac::MapDefinition>* Map = BuiltMaps.Find(Id);
		// One at a time, a while after the region changed (W4: the frames
		// round a crossing keep the machine to themselves); two at a time
		// from the start at the session's start (Q2: the ground round it
		// built in its first seconds, before a Dropship's wide view draws it;
		// all eight at once starved the frames).
		const bool bNow = SessionStart() && Target == INDEX_NONE;
		if ((!Map || bWantMirror) && bMayBuild && !Building.Contains(Id)
			&& (bNow ? Building.Num() < 2 : Building.IsEmpty() && SinceRegionChange() > 2.0))
		{
			const std::string Text = TCHAR_TO_UTF8(**Body);
			const ac::net::RegionDescription Like = M->region();
			const FString IdCopy = Id;
			Building.Add(Id, AcWork::Run([Text, Like, IdCopy, bWantMirror]() -> FRegionBuilt* {
				FRegionBuilt* Out = new FRegionBuilt();
				Out->Id = IdCopy;
				if (const ac::region::Parsed P = ac::region::parse(Text); P.file)
					Out->Map = std::make_shared<const ac::MapDefinition>(ac::region::build(*P.file).map);
				if (bWantMirror) Out->Mirror = std::make_unique<ac::net::Mirror>(ac::net::Mirror::describe(Like, Text));
				return Out;
			}));
		}
		if (!Map) continue;
		FAcRegionAround A;
		A.Id = Id;
		A.Map = *Map;
		for (size_t I = 0; I < W->regions.size(); I++)
			if (W->regions[I].id == Q.id) A.Index = int64(I);
		if (const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, Q.id)) A.Centre = P->centre();
		const int64 Step[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
		for (int32 E = 0; E < 4; E++)
		{
			bool bThere = false;
			for (const ac::net::WorldMessage::Region& O : W->regions)
				bThere = bThere || (O.column == Q.column + Step[E][0] && O.row == Q.row + Step[E][1]);
			if (!bThere) A.Rim |= uint8(1 << E);
		}
		Now.Add(MoveTemp(A));
	}
	AroundNow = MoveTemp(Now);
}

// MARK: - The Dropship (F2)

TOptional<FVector> FAcMmoClient::RegionPoint(const int64 Index, const ac::Vec2 At) const
{
	const ac::net::WorldMessage* W = World();
	if (!W || Index < 0 || Index >= int64(W->regions.size()) || !Session) return {};
	const ac::net::WorldMessage::Region& R = W->regions[size_t(Index)];
	// Its map: the mirror's, or one built ahead.
	const ac::MapDefinition* Map = nullptr;
	const ac::net::Mirror* M = Net ? Net->mirror() : nullptr;
	if (M && MirrorIndex() == Index && M->simulation().map) Map = &*M->simulation().map;
	else if (const std::shared_ptr<const ac::MapDefinition>* Built = BuiltMaps.Find(Utf(R.id))) Map = Built->get();
	if (!Map) return {};
	const std::vector<ac::net::atlas::Place> Places = ac::net::atlas::layout(*W, {});
	const ac::net::atlas::Place* P = ac::net::atlas::placeOf(Places, Index);
	if (!P) return {};
	// The cells on its ground (`padsOnGround`'s way), then where that ground
	// lies in the session's frame (its centre less the session's).
	ac::net::WorldMessage::Region One = R;
	One.pads = {ac::net::WorldMessage::Pad{"", At}};
	const ac::Vec2 Ground = ac::net::padsOnGround(One, Map->bounds)[0].at;
	const ac::Vec2 Frame = P->centre() - *Session;
	return FVector((Ground.x + Frame.x) * AcSpace::CmPerCell, (Ground.y + Frame.y) * AcSpace::CmPerCell, 0.0);
}

void FAcMmoClient::FollowFerry(const double Now)
{
	const ac::net::WorldMessage* W = World();
	if (!W || !W->ferry || !W->flyingTo)
	{
		if (FerryNow) UE_LOG(LogAutocraft, Log, TEXT("mmo: the Dropship's flight is over"));
		FerryNow.Reset();
		return;
	}
	const ac::net::WorldMessage::Ferry& F = *W->ferry;
	const FString ToRegion = Utf(W->regions[size_t(*W->flyingTo)].id);
	const FString Pad = Utf(F.pad);
	const double Start = Now - F.elapsed;
	// The same World message each frame: its `elapsed` read once, when new.
	const bool bNewMessage = F.elapsed != FerryElapsedSeen;
	FerryElapsedSeen = F.elapsed;
	if (!FerryNow || FerryNow->ToRegion != ToRegion || FerryNow->Pad != Pad)
	{
		FFerry New;
		New.StartWall = Start;
		New.Plan = F.plan;
		New.ToIndex = *W->flyingTo;
		New.ToRegion = ToRegion;
		New.Pad = Pad;
		// The boarding pad: on the mirror's ground while it is the region left.
		New.From = RegionPoint(F.from, F.boarded).Get(FVector::ZeroVector);
		FerryNow = New;
		UE_LOG(LogAutocraft, Log, TEXT("mmo: aboard the Dropship to %s %s, %.1f s in of %.1f (switch at %.1f), from %s"), *ToRegion, *Pad,
			F.elapsed, F.plan.total(), F.plan.approachAt(), *New.From.ToCompactString());
	}
	// A later World message (the switch): the server's clock again, when it
	// drifted.
	else if (bNewMessage && FMath::Abs(FerryNow->StartWall - Start) > 0.25)
	{
		FerryNow->StartWall = Start;
	}
	if (!FerryNow->To)
	{
		FerryNow->To = RegionPoint(*W->flyingTo, F.at);
		if (FerryNow->To) UE_LOG(LogAutocraft, Log, TEXT("mmo: the Dropship's target pad at %s"), *FerryNow->To->ToCompactString());
	}
}
