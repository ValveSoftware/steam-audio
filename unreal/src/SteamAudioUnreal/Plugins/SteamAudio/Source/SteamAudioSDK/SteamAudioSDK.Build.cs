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
using UnrealBuildTool;

public class SteamAudioSDK : ModuleRules
{
    public SteamAudioSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        // This module doesn't contain any source code, it just allows other modules to link to the Steam Audio SDK.
        Type = ModuleType.External;

        PublicIncludePaths.Add("$(PluginDir)/Source/SteamAudioSDK/include");

#if !UE_5_0_OR_LATER
        if (Target.Platform == UnrealTargetPlatform.Win32)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x86/phonon.lib");

            PublicDelayLoadDLLs.Add("phonon.dll");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x86/phonon.dll");
        }
#endif
#if UE_5_2_OR_LATER
        // Native Windows on ARM64 builds use the arm64 SDK binaries.
        bool bIsWindowsArm64 = (Target.Platform == UnrealTargetPlatform.Win64 && Target.Architecture == UnrealArch.Arm64);
        // ARM64EC builds can load both ARM64EC and x64 DLLs. Use the ARM64EC SDK binaries if they are present, since
        // they run natively instead of under emulation; otherwise, fall back to the x64 SDK binaries.
        bool bIsWindowsArm64ec = (Target.Platform == UnrealTargetPlatform.Win64 &&
                                  Target.Architecture.ToString().ToLowerInvariant() == "arm64ec" &&
                                  File.Exists(Path.Combine(PluginDirectory, "Source/SteamAudioSDK/lib/windows-arm64ec/phonon.dll")));
#else
        bool bIsWindowsArm64 = false;
        bool bIsWindowsArm64ec = false;
#endif
        if (Target.Platform == UnrealTargetPlatform.Win64 && bIsWindowsArm64)
        {
            // GPU acceleration (TrueAudio Next) is not available on arm64.
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-arm64/phonon.lib");

            PublicDelayLoadDLLs.Add("phonon.dll");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-arm64/phonon.dll");
        }
        if (Target.Platform == UnrealTargetPlatform.Win64 && bIsWindowsArm64ec)
        {
            // GPU acceleration (TrueAudio Next) is not available on arm64.
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-arm64ec/phonon.lib");

            PublicDelayLoadDLLs.Add("phonon.dll");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-arm64ec/phonon.dll");
        }
        if (Target.Platform == UnrealTargetPlatform.Win64 && !bIsWindowsArm64 && !bIsWindowsArm64ec)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x64/phonon.lib");

            PublicDelayLoadDLLs.Add("phonon.dll");
            PublicDelayLoadDLLs.Add("TrueAudioNext.dll");
            PublicDelayLoadDLLs.Add("GPUUtilities.dll");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x64/phonon.dll");
            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x64/TrueAudioNext.dll");
            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/windows-x64/GPUUtilities.dll");
        }
        if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-x64/libphonon.so");

            PublicDelayLoadDLLs.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-x64/libphonon.so");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-x64/libphonon.so");
        }
#if UE_5_0_OR_LATER
        if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
#else
        if (Target.Platform == UnrealTargetPlatform.LinuxAArch64)
#endif
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-arm64/libphonon.so");

            PublicDelayLoadDLLs.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-arm64/libphonon.so");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/linux-arm64/libphonon.so");
        }
        if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/osx/libphonon.dylib");

            PublicDelayLoadDLLs.Add("$(PluginDir)/Source/SteamAudioSDK/lib/osx/libphonon.dylib");

            RuntimeDependencies.Add("$(PluginDir)/Source/SteamAudioSDK/lib/osx/libphonon.dylib");
        }
        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/android/armeabi-v7a/libphonon.so");
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/android/arm64-v8a/libphonon.so");
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/android/x86/libphonon.so");
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/android/x86_64/libphonon.so");

            string APLName = "SteamAudioSDK_APL.xml";
            string APLPathRelativeToEngine = Utils.MakePathRelativeTo(System.IO.Path.Combine(ModuleDirectory, APLName), Target.RelativeEnginePath);
            AdditionalPropertiesForReceipt.Add("AndroidPlugin", APLPathRelativeToEngine);
        }
        if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/ios/libpffft.a");
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/ios/libmysofa.a");
            PublicAdditionalLibraries.Add("$(PluginDir)/Source/SteamAudioSDK/lib/ios/libphonon.a");
        }
    }
}
