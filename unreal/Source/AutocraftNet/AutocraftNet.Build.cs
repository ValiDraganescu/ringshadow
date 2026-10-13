using UnrealBuildTool;

// The network library, engine-free C++20 (docs/networking.md): Unreal builds
// it as a module for the game, and unreal/net-tests and the server build the
// same files with plain clang++. Depends on AutocraftCore for the protocol.
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
		// The protocol's files (NetProtocol, NetSnapshot, NetMirror) speak the
		// core's types; the transport's files stay free of them.
		PublicDependencyModuleNames.Add("AutocraftCore");
		// The game module links what the public headers mark AUTOCRAFTNET_API
		// (Public/AutocraftNetApi.h); nothing else leaves the module.
	}
}
