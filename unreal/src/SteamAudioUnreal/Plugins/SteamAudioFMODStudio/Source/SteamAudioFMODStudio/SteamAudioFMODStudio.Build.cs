//
// Copyright 2017-2023 Valve Corporation.
//

using UnrealBuildTool;

public class SteamAudioFMODStudio : ModuleRules
{
	public SteamAudioFMODStudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
			"Projects",
			"FMODStudio",
			"SteamAudio"
        });

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicAdditionalLibraries.Add("$(PluginDir)/../FMODStudio/Binaries/IOS/libphonon_fmod.a");
		}
	}
}
