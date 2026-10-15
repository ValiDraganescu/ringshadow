// W4, seamless regions (docs/mmorpg-m8.md, docs/worlds.md "In the game"):
// the regions round the pilot's, made ahead off the game's thread. Their
// maps are what the game draws beyond the walls (AAcTerrain's ground sets);
// the four across the passages also get a mirror (`Client::prepare`), so
// walking into one swaps it in instead of building the region then.
#include "AcMmo.h"

#include "AcLog.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
			// Made for the region the pilot stood in then: still of this world
			// (the description says), so still good.
			Net->prepare(std::move(Done->Mirror));
			MirrorsMade.Add(Done->Id);
			UE_LOG(LogAutocraft, Log, TEXT("mmo: %s made ahead (map and mirror)"), *Done->Id);
		}
		delete Done;
	}
	const ac::net::WorldMessage* W = World();
	const ac::net::Mirror* M = Net ? Net->mirror() : nullptr;
	if (!W || !M || W->flyingTo) return;
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
	for (const ac::net::WorldMessage::Region& Q : W->regions)
	{
		const int64 DC = Q.column - R.column, DR = Q.row - R.row;
		if ((DC == 0 && DR == 0) || FMath::Abs(DC) > 1 || FMath::Abs(DR) > 1) continue;
		const FString Id = Utf(Q.id);
		const FString* Body = All.Find(Id);
		if (!Body) continue;
		// Across a side (not a corner): a mirror made ahead too, as the
		// pilot may walk in.
		const bool bSide = (DC == 0) != (DR == 0);
		const bool bWantMirror = bSide && !MirrorsMade.Contains(Id);
		const std::shared_ptr<const ac::MapDefinition>* Map = BuiltMaps.Find(Id);
		// One at a time, a while after the region changed (W4: the frames
		// round a crossing keep the machine to themselves).
		if ((!Map || bWantMirror) && Building.IsEmpty() && SinceRegionChange() > 2.0)
		{
			const std::string Text = TCHAR_TO_UTF8(**Body);
			const ac::net::RegionDescription Like = M->region();
			const FString IdCopy = Id;
			Building.Add(Id, Async(EAsyncExecution::ThreadPool, [Text, Like, IdCopy, bWantMirror]() -> FRegionBuilt* {
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
