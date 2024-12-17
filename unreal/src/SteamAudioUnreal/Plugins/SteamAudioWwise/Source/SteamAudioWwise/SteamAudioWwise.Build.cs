//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

using System.IO;
using System;
using UnrealBuildTool;

public class SteamAudioWwise: ModuleRules
{
	public SteamAudioWwise(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
			"Projects",
			"AkAudio",
            "WwiseSoundEngine",
            "SteamAudio"
        });

        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalLibraries.Add(GetStaticLibraryPath("SteamAudioWwiseFX"));
        }
    }

    private string GetStaticLibraryPath(string LibName)
    {
        // Assumes Wwise relative path to SteamAudioWwise folder.
        string WwisePluginPath = Path.Combine(PluginDirectory, "..", "Wwise");
        WwiseSoundEngineVersion WwiseVersion = WwiseSoundEngineVersion.GetInstance(WwisePluginPath);
        var VersionNumber = WwiseVersion.Major.ToString() + "_" + WwiseVersion.Minor.ToString();

        string WwiseThirdPartyPath = Path.Combine(WwisePluginPath, "ThirdParty");
        var WwiseUEPlatformInstance = WwiseUEPlatform.GetWwiseUEPlatformInstance(Target, VersionNumber, WwiseThirdPartyPath);

        string WwiseLibraryPath = Path.Combine(WwiseThirdPartyPath, WwiseUEPlatformInstance.AkPlatformLibDir, WwiseUEPlatformInstance.WwiseConfigurationDir + "-iphoneos", "lib");
        return WwiseUEPlatformInstance.GetLibraryFullPath(LibName, WwiseLibraryPath);
    }
}
