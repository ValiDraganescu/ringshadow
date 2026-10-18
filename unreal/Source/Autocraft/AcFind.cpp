#include "AcFind.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<int32> CVarFindKeep(TEXT("ac.FindKeep"), 1,
		TEXT("The game's one-of-a-kind actors are kept once found (1), or found by walking the level's actors each time (0, to compare)."));
}

bool AcFindKeep()
{
	return CVarFindKeep.GetValueOnGameThread() != 0;
}
