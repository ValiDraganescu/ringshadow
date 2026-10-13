// A world that lasts, the game's side (AcMmo.h, docs/mmorpg-m3.md S4), in a
// scratch folder (unreal/server/build/test-mmo-save), never the player's:
//   PilotKeyFile  no key file: a new key written (its folder made); read back
//                 the same; a file that is not 32 hex digits refused, kept
//   DeleteSave    the save and its -wal and -shm files deleted by name, a
//                 neighbour left alone
//   SaveRefused   the real server on a save that is not a database: the start
//                 fails with the server's reason and its log's path, the file
//                 untouched
// The resume itself (a Citadel kept, the same pilot on the same spot) runs
// through the game: the flow step `mmoresume` (AcMmoFlowTest.cpp). Run hidden
// as AcMmoTests.cpp says (`Automation RunTests Autocraft.Mmo`).
#include "AcMmo.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags Flags =
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter;

	/// The tests' scratch folder (git ignores unreal/server/build).
	FString Scratch()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("server"), TEXT("build"), TEXT("test-mmo-save")));
	}

	void Remove(const FString& Path)
	{
		IFileManager::Get().Delete(*Path, false, true, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoPilotKeyFileTest, "Autocraft.Mmo.PilotKeyFile", Flags)
bool FAcMmoPilotKeyFileTest::RunTest(const FString&)
{
	const FString Dir = FPaths::Combine(Scratch(), TEXT("key"));
	const FString File = FPaths::Combine(Dir, TEXT("pilot.key"));
	Remove(File);
	IFileManager::Get().DeleteDirectory(*Dir, false, false);
	TestFalse(TEXT("no key file yet"), IFileManager::Get().FileExists(*File));

	const std::optional<ac::net::PilotKey> Made = FAcMmoClient::LoadPilotKey(File);
	TestTrue(TEXT("a new key"), Made.has_value());
	TestTrue(TEXT("written, its folder made"), IFileManager::Get().FileExists(*File));
	const std::optional<ac::net::PilotKey> Again = FAcMmoClient::LoadPilotKey(File);
	TestTrue(TEXT("read back the same"), Made && Again && *Made == *Again);
	FString Hex;
	FFileHelper::LoadFileToString(Hex, *File);
	TestEqual(TEXT("32 hex digits"), Hex.TrimStartAndEnd().Len(), 32);
	TestTrue(TEXT("a random key, not zeros"), Made && *Made != ac::net::PilotKey{});

	// Not a key: refused, and the file stays as it is.
	FFileHelper::SaveStringToFile(TEXT("not a key\n"), *File);
	FString Why;
	TestFalse(TEXT("a bad key file refused"), FAcMmoClient::LoadPilotKey(File, &Why).has_value());
	TestTrue(TEXT("says why"), Why.Contains(TEXT("not 32 hex digits")));
	FString Kept;
	FFileHelper::LoadFileToString(Kept, *File);
	TestEqual(TEXT("the bad file is not replaced"), Kept, FString(TEXT("not a key\n")));

	// The paths the game uses are in the MMO folder.
	TestTrue(TEXT("the key file"), FAcMmoClient::PilotKeyFile().EndsWith(TEXT("/pilot.key")));
	TestTrue(TEXT("the first world's save"), FAcMmoClient::SaveFile().EndsWith(TEXT("/first.sqlite")));
	Remove(File);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoDeleteSaveTest, "Autocraft.Mmo.DeleteSave", Flags)
bool FAcMmoDeleteSaveTest::RunTest(const FString&)
{
	const FString Dir = FPaths::Combine(Scratch(), TEXT("delete"));
	const FString Save = FPaths::Combine(Dir, TEXT("first.sqlite"));
	const FString Other = FPaths::Combine(Dir, TEXT("first.sqlite.keep"));
	for (const FString& P : {Save, Save + TEXT("-wal"), Save + TEXT("-shm"), Other}) FFileHelper::SaveStringToFile(TEXT("x"), *P);
	TestTrue(TEXT("deleted"), FAcMmoClient::DeleteSave(Save));
	for (const FString& P : {Save, Save + TEXT("-wal"), Save + TEXT("-shm")})
	{
		TestFalse(*FString::Printf(TEXT("%s is gone"), *FPaths::GetCleanFilename(P)), IFileManager::Get().FileExists(*P));
	}
	TestTrue(TEXT("a neighbour is left"), IFileManager::Get().FileExists(*Other));
	TestTrue(TEXT("nothing there: fine"), FAcMmoClient::DeleteSave(Save));
	Remove(Other);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcMmoSaveRefusedTest, "Autocraft.Mmo.SaveRefused", Flags)
bool FAcMmoSaveRefusedTest::RunTest(const FString&)
{
	const FString Save = FPaths::Combine(Scratch(), TEXT("refused"), TEXT("first.sqlite"));
	const FString Junk = TEXT("this is not a database\n");
	FAcMmoClient::DeleteSave(Save);
	FFileHelper::SaveStringToFile(Junk, *Save);
	FAcMmoClient Client;
	Client.SavePath = Save;
	Client.PilotKey = ac::net::newPilotKey();
	Client.Start(FPlatformTime::Seconds());
	const double End = FPlatformTime::Seconds() + 10;
	while (FPlatformTime::Seconds() < End && Client.Phase() != FAcMmoClient::EPhase::Failed && Client.Phase() != FAcMmoClient::EPhase::Playing)
	{
		Client.Tick(FPlatformTime::Seconds());
		FPlatformProcess::Sleep(1.0f / 120.0f);
	}
	AddInfo(FString::Printf(TEXT("the banner: %s"), *Client.Message()));
	TestTrue(TEXT("the start failed"), Client.Phase() == FAcMmoClient::EPhase::Failed);
	TestTrue(TEXT("with the server's reason"), Client.Message().Contains(TEXT("did not start: save ")) && Client.Message().Contains(TEXT("not a database")));
	TestTrue(TEXT("and its log"), Client.Message().Contains(TEXT("server.log")));
	FString Kept;
	FFileHelper::LoadFileToString(Kept, *Save);
	TestEqual(TEXT("the file untouched"), Kept, Junk);
	Client.Stop();
	FAcMmoClient::DeleteSave(Save);
	return true;
}

#endif
