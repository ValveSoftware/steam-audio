//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEditor;

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

        static string[] Scripts =
        {
            "Assets/SteamAudio",
        };

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
            string[][] assetGroups = { Scripts, Plugins };
            string[] assets = BuildAssetList(assetGroups);

            AssetDatabase.ExportPackage(assets, "SteamAudio.unitypackage", ExportPackageOptions.Recurse);
        }
    }
}