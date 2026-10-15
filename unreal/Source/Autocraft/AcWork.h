// The game's own long CPU jobs (a region built ahead, a ground set's
// prepare, a minimap bake) run on a pool of the game's own threads, never
// on the engine's thread pool. That pool is the task graph's background
// workers, two of them here, and the render thread waits on short engine
// tasks queued there: in a packaged run a second-long region build held a
// 108 ms engine task back 860 ms, and the frame with it (K3, docs/builds.md
// "Measuring packaged"). The pool is made on first use and ends before the
// engine does.
#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"

class FQueuedThreadPool;

namespace AcWork
{
	/// Three threads below normal priority.
	FQueuedThreadPool& Pool();

	/// `Async(EAsyncExecution::ThreadPool, Fn)` on the game's pool.
	template <class FFn>
	auto Run(FFn&& Fn)
	{
		return AsyncPool(Pool(), Forward<FFn>(Fn));
	}
}
