//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine; // deleteme?
using UnityEditor;
#if UNITY_2021_2_OR_NEWER
using UnityEditor.Build;
#endif
using UnityEditor.Callbacks;
#if UNITY_IOS
using UnityEditor.iOS.Xcode;
#endif

namespace SteamAudio
{
    public static class Build
    {
        public static void BuildSteamAudio()
        {
            var args = Environment.GetCommandLineArgs();
            var lastArg = args[args.Length - 1];

            var fileName = "SteamAudio.unitypackage";
            if (lastArg != "SteamAudio.Build.BuildSteamAudio")
            {
                fileName = lastArg + "/" + fileName;
            }

            var assets = new string[] { "Assets/Plugins" };

            AssetDatabase.ExportPackage(assets, fileName, ExportPackageOptions.Recurse);
        }
    }

    [InitializeOnLoad]
    public static class Defines
    {
        // Define the constant STEAMAUDIO_ENABLED for all platforms that are supported by
        // Steam Audio. User scripts should check if this constant is defined
        // (using #if STEAMAUDIO_ENABLED) before using any of the Steam Audio C# classes.
        static Defines()
        {
#if UNITY_2021_2_OR_NEWER
            NamedBuildTarget[] supportedPlatforms = {
                NamedBuildTarget.Standalone,
                NamedBuildTarget.Android,
                NamedBuildTarget.iOS,
            };

            foreach (var supportedPlatform in supportedPlatforms)
            {
                var defines = PlayerSettings.GetScriptingDefineSymbols(supportedPlatform);
                if (!defines.Contains("STEAMAUDIO_ENABLED"))
                {
                    if (defines.Length > 0)
                    {
                        defines += ";";
                    }

                    defines += "STEAMAUDIO_ENABLED";

                    PlayerSettings.SetScriptingDefineSymbols(supportedPlatform, defines);
                }
            }
#endif
        }
    }

    public static class BuildProcessor
    {
        [PostProcessBuild]
        public static void OnPostProcessBuild(BuildTarget buildTarget, string buildPath)
        {
            if (buildTarget == BuildTarget.iOS)
            {
#if UNITY_IOS
                var projectPath = PBXProject.GetPBXProjectPath(buildPath);

                var project = new PBXProject();
                project.ReadFromFile(projectPath);

                var file = project.AddFile("usr/lib/libz.tbd", "Frameworks/libz.tbd", PBXSourceTree.Sdk);
                var target = project.TargetGuidByName("UnityFramework");
                project.AddFileToBuild(target, file);

                project.WriteToFile(projectPath);
#endif
            }
        }
    }
}
