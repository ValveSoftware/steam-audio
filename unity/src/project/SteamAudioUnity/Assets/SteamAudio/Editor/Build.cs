//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEditor;
using UnityEditor.Callbacks;

namespace SteamAudio
{

    //
    // Build
    // Command-line build system for Steam Audio.
    //

    public static class Build
    {
        //
        // Steam Audio scripts
        //

        static string Scripts = "Assets/SteamAudio";

        //
        // Steam Audio plugins
        //

        static string[] Plugins =
        {
            "Assets/Plugins/x86/phonon.dll",
            "Assets/Plugins/x86/audioplugin_phonon.dll",
            "Assets/Plugins/x86_64/phonon.dll",
            "Assets/Plugins/x86_64/audioplugin_phonon.dll",
            "Assets/Plugins/phonon.bundle",
            "Assets/Plugins/audioplugin_phonon.bundle",
            "Assets/Plugins/x86/libphonon.so",
            "Assets/Plugins/x86/libaudioplugin_phonon.so",
            "Assets/Plugins/x86_64/libphonon.so",
            "Assets/Plugins/x86_64/libaudioplugin_phonon.so",
            "Assets/Plugins/android/libphonon.so",
            "Assets/Plugins/android/libaudioplugin_phonon.so"
        };

        static string[] FMODStudioPlugins =
        {
            // "Assets/Plugins/x86/phonon_fmod.dll",
            "Assets/Plugins/x86_64/phonon_fmod.dll" //,
            // "Assets/Plugins/x86/libphonon_fmod.so",
            // "Assets/Plugins/x86_64/libphonon_fmod.so",
            // "Assets/Plugins/phonon_fmod.bundle",
            // "Assets/Plugins/android/libphonon_fmod.so"
        };

        static string[] TrueAudioNextPlugins =
        {
            "Assets/Plugins/x86_64/tanrt64.dll",
            "Assets/Plugins/x86_64/GPUUtilities.dll"
        };

        static string[] EmbreePlugins =
        {
            "Assets/Plugins/x86_64/embree.dll",
            "Assets/Plugins/x86_64/tbb.dll",
            "Assets/Plugins/x86_64/tbbmalloc.dll",
            "Assets/Plugins/x86_64/libembree.so",
            "Assets/Plugins/x86_64/libtbb.so.2",
            "Assets/Plugins/x86_64/libtbbmalloc.so.2",
            "Assets/Plugins/libembree.dylib",
            "Assets/Plugins/libtbb.dylib",
            "Assets/Plugins/libtbbmalloc.dylib"
        };

        static string FMODStudioAudioEngineSuffix = "_FMODStudio";

        public static string[] FilteredAssets(string directory, string[] excludeSuffixes, string includeOnlySuffix)
        {
            var files = Directory.GetFiles(Directory.GetCurrentDirectory() + "/" + directory, "*", 
                SearchOption.AllDirectories);

            var assets = new List<string>();
            foreach (var file in files)
            {
                if (file.Contains(".meta"))
                    continue;

                if (excludeSuffixes != null)
                {
                    var fileExcluded = false;
                    foreach (var suffix in excludeSuffixes)
                    {
                        if (file.Contains(suffix))
                        {
                            fileExcluded = true;
                            break;
                        }
                    }
                    if (fileExcluded)
                        continue;
                }

                if (includeOnlySuffix != null && !file.Contains(includeOnlySuffix))
                    continue;

                var relativeName = file.Replace(Directory.GetCurrentDirectory() + "/", "");
                relativeName = relativeName.Replace('\\', '/');

                assets.Add(relativeName);
            }

            return assets.ToArray();
        }

        //
        // BuildAssetList
        // Builds an asset list given an array of asset groups.
        //
        static string[] BuildAssetList(string[][] assetGroups)
        {
            int numAssets = 0;
            foreach (string[] assetGroup in assetGroups)
                numAssets += assetGroup.Length;

            string[] assets = new string[numAssets];

            int offset = 0;
            foreach (string[] assetGroup in assetGroups)
            {
                Array.Copy(assetGroup, 0, assets, offset, assetGroup.Length);
                offset += assetGroup.Length;
            }

            return assets;
        }

        //
        // BuildSteamAudio
        // Builds a Unity package for Steam Audio.
        //
        public static void BuildSteamAudio()
        {
            var unityScripts = FilteredAssets(Scripts, new string[] { FMODStudioAudioEngineSuffix }, null);

            string[][] assetGroups = { unityScripts, Plugins };
            string[] assets = BuildAssetList(assetGroups);

            AssetDatabase.ExportPackage(assets, "SteamAudio.unitypackage", ExportPackageOptions.Recurse);
        }

        public static void BuildSteamAudioFMODStudio()
        {
            var fmodScripts = FilteredAssets(Scripts, null, FMODStudioAudioEngineSuffix);

            var assetGroups = new string[][] { fmodScripts, FMODStudioPlugins };
            var assets = BuildAssetList(assetGroups);

            AssetDatabase.ExportPackage(assets, "SteamAudio_FMODStudio.unitypackage", ExportPackageOptions.Recurse);
        }

        public static void BuildSteamAudioTrueAudioNext()
        {
            var assetGroups = new string[][] { TrueAudioNextPlugins };
            var assets = BuildAssetList(assetGroups);

            AssetDatabase.ExportPackage(assets, "SteamAudio_TrueAudioNext.unitypackage", ExportPackageOptions.Recurse);
        }

        public static void BuildSteamAudioEmbree()
        {
            var assetGroups = new string[][] { EmbreePlugins };
            var assets = BuildAssetList(assetGroups);

            AssetDatabase.ExportPackage(assets, "SteamAudio_Embree.unitypackage", ExportPackageOptions.Recurse);
        }
    }

    public static class PlayerBuildPostprocessor
    {
        [PostProcessBuild]
        public static void OnBuildPlayer(BuildTarget target, string playerPath)
        {
            var playerDirectory = Path.GetDirectoryName(playerPath);
            var playerName = Path.GetFileNameWithoutExtension(playerPath);

            var embreeAssets = Directory.GetFiles(Directory.GetCurrentDirectory() + "/Assets/Plugins/x86_64", "embree.dll");

            if (embreeAssets != null && embreeAssets.Length > 0)
            {
                switch (target)
                {
                    case BuildTarget.StandaloneWindows64:
                        // Embree .dll files are correctly copied on Windows, so nothing to do here.
                        break;

                    case BuildTarget.StandaloneLinux64:
                    case BuildTarget.StandaloneLinuxUniversal:
                        FileUtil.CopyFileOrDirectory("Assets/Plugins/x86_64/libtbb.so.2",
                            playerDirectory + "/" + playerName + "_Data/Plugins/x86_64/libtbb.so.2");
                        FileUtil.CopyFileOrDirectory("Assets/Plugins/x86_64/libtbbmalloc.so.2",
                            playerDirectory + "/" + playerName + "_Data/Plugins/x86_64/libtbbmalloc.so.2");
                        break;

#if UNITY_2017_3_OR_NEWER
                    case BuildTarget.StandaloneOSX:
#else
                case BuildTarget.StandaloneOSXUniversal:
#endif
                        FileUtil.CopyFileOrDirectory("Assets/Plugins/libembree.dylib",
                            playerDirectory + "/" + playerName + ".app/Contents/Plugins/libembree.dylib");
                        FileUtil.CopyFileOrDirectory("Assets/Plugins/libtbb.dylib",
                            playerDirectory + "/" + playerName + ".app/Contents/Plugins/libtbb.dylib");
                        FileUtil.CopyFileOrDirectory("Assets/Plugins/libtbbmalloc.dylib",
                            playerDirectory + "/" + playerName + ".app/Contents/Plugins/libtbbmalloc.dylib");
                        break;

                    default:
                        break;
                }
            }
        }
    }
}