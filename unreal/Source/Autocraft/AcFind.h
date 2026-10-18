// `AcFindActor<T>(World)`: the world's one actor of class T (the first an
// actor iterator meets), kept from one call to the next (docs/perf.md,
// client round 4). The game's actors of one kind each (AAcTerrain, AAcFog,
// AAcWeather, the pilot pawn...) are found many times a frame, and an
// iterator walks the level's actor list every time: ~3 % of the MMO's game
// thread. The kept one (one per world) is used while it is alive and in
// that world; else (and off the game thread, where nothing is kept) the walk
// as before.
#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UObject/WeakObjectPtrTemplates.h"

/// `ac.FindKeep` (1; 0: walk every time, to compare).
AUTOCRAFT_API bool AcFindKeep();

template <class T>
T* AcFindActor(const UWorld* World)
{
	if (!World) return nullptr;
	const bool bKeep = IsInGameThread() && AcFindKeep();
	// One kept actor per world (PIE beside the editor's world: two at once).
	struct FKept
	{
		TWeakObjectPtr<const UWorld> World;
		TWeakObjectPtr<T> Actor;
	};
	static TArray<FKept, TInlineAllocator<4>> Kept;
	if (bKeep)
	{
		for (int32 I = Kept.Num() - 1; I >= 0; --I)
		{
			const UWorld* W = Kept[I].World.Get();
			if (!W)
			{
				Kept.RemoveAtSwap(I);
				continue;
			}
			if (W != World) continue;
			if (T* A = Kept[I].Actor.Get(); A && IsValid(A) && A->GetWorld() == World) return A;
			Kept.RemoveAtSwap(I);
		}
	}
	TActorIterator<T> It(const_cast<UWorld*>(World));
	T* Found = It ? *It : nullptr;
	if (bKeep && Found) Kept.Add({World, Found});
	return Found;
}
