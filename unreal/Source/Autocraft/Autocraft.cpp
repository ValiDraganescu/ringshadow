#include "AcAppIcon.h"
#include "AcLog.h"
#include "AcQualityTier.h"
#include "Modules/ModuleManager.h"

/// The game module: starts the Autocraft log file (AcLog.h), in a game run
/// puts up the Dock icon (AcAppIcon.h), and sets this Mac's quality tier
/// (AcQualityTier.h).
class FAutocraftModule final : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		AcLog::Start();
		AcAppIcon::Apply();
		AcQualityTier::Apply();
	}
	virtual void ShutdownModule() override { AcLog::Stop(); }
};

IMPLEMENT_PRIMARY_GAME_MODULE(FAutocraftModule, Autocraft, "Autocraft");
