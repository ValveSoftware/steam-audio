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
