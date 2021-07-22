//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEditor;

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
}
