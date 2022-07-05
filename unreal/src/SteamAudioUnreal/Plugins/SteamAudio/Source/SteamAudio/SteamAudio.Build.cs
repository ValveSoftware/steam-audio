//
// Copyright (C) Valve Corporation. All rights reserved.
//

using UnrealBuildTool;

public class SteamAudio : ModuleRules
{
    public SteamAudio(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "Projects",
            "Landscape",
            "AudioMixer",
            "AudioExtensions"
        });

        PublicDependencyModuleNames.AddRange(new string[] {
            "SteamAudioSDK"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }
    }
}
