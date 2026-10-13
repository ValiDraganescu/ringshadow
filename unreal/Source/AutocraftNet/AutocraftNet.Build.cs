using UnrealBuildTool;

// The network library, engine-free C++20 (docs/networking.md): Unreal builds
// it as a module for the game, and unreal/net-tests and the server build the
// same files with plain clang++.
public class AutocraftNet : ModuleRules
{
	public AutocraftNet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		// Every file compiles on its own, as in AutocraftCore.
		bUseUnity = false;
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = false;
		bUseRTTI = false;
		PublicDependencyModuleNames.Add("Core");
		// The game module links what the public headers mark AUTOCRAFTNET_API
		// (Public/AutocraftNetApi.h); nothing else leaves the module.
	}
}
