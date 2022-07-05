//
// Copyright (C) Valve Corporation. All rights reserved.
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
	}
}
