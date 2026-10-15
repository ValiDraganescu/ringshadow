// The game's config (U2, AcSettings.h): the Settings page's values written
// to an ini and read back into a fresh object; a first run's defaults are
// ac::PlayerSettings' (core-tests) and the flow mmosettings checks them on a
// fresh config. Run hidden:
//   uv run unreal/Tools/hidden_run.py --out unreal/server/build/u2/tests --tests Autocraft.Settings
#include "AcSettings.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcSettingsConfigTest, "Autocraft.Settings.Config", Flags)
bool FAcSettingsConfigTest::RunTest(const FString&)
{
	const FString Ini = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AcSettingsTest.ini")));
	IFileManager::Get().Delete(*Ini, false, true, true);

	UAcSettings* A = NewObject<UAcSettings>(GetTransientPackage());
	A->VoiceVolume = 0.55f;
	A->MusicVolume = 0.3f;
	A->SfxVolume = 0.65f;
	A->bMmoLag = true;
	A->SetFullscreenMode(EWindowMode::Windowed);
	A->SetScreenResolution(FIntPoint(1280, 720));
	A->SaveConfig(CPF_Config, *Ini);
	GConfig->Flush(false, Ini);

	FString Text;
	TestTrue(TEXT("the file written"), FFileHelper::LoadFileToString(Text, *Ini));
	TestTrue(TEXT("its section"), Text.Contains(TEXT("[/Script/Autocraft.AcSettings]")));
	TestTrue(TEXT("the voice"), Text.Contains(TEXT("VoiceVolume=0.55")));
	TestTrue(TEXT("the lag"), Text.Contains(TEXT("bMmoLag=True")));

	UAcSettings* B = NewObject<UAcSettings>(GetTransientPackage());
	B->LoadConfig(nullptr, *Ini);
	TestEqual(TEXT("voice read back"), B->VoiceVolume, 0.55f, 1e-6f);
	TestEqual(TEXT("music read back"), B->MusicVolume, 0.3f, 1e-6f);
	TestEqual(TEXT("effects read back"), B->SfxVolume, 0.65f, 1e-6f);
	TestTrue(TEXT("lag read back"), B->bMmoLag);
	TestEqual(TEXT("window read back"), int32(B->GetFullscreenMode()), int32(EWindowMode::Windowed));
	TestEqual(TEXT("resolution read back"), B->GetScreenResolution(), FIntPoint(1280, 720));

	GConfig->UnloadFile(Ini);
	IFileManager::Get().Delete(*Ini, false, true, true);
	return true;
}

#endif
