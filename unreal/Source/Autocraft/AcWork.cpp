#include "AcWork.h"

#include "Misc/CoreDelegates.h"
#include "Misc/QueuedThreadPool.h"

FQueuedThreadPool& AcWork::Pool()
{
	static FQueuedThreadPool* Made = []
	{
		FQueuedThreadPool* P = FQueuedThreadPool::Allocate();
		// 4 MB stacks: the region builds and the mirrors recurse a little.
		verify(P->Create(3, 4 * 1024 * 1024, TPri_BelowNormal, TEXT("AcWorkPool")));
		// Its jobs finish (or never start) before the engine goes.
		FCoreDelegates::OnPreExit.AddLambda([P] { P->Destroy(); });
		return P;
	}();
	return *Made;
}
