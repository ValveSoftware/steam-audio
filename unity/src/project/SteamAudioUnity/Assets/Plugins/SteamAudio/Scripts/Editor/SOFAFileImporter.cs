//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System.IO;
using UnityEngine;
#if UNITY_2020_2_OR_NEWER
using UnityEditor.AssetImporters;
#else
using UnityEditor.Experimental.AssetImporters;
#endif

namespace SteamAudio
{
    /*
     * Imports .sofa files as SOFAFile asset objects.
     */
    [ScriptedImporter(1, "sofa")]
    public class SOFAFileImporter : ScriptedImporter
    {
        [Range(0.0f, 2.0f)]
        public float volume = 1.0f;

        public override void OnImportAsset(AssetImportContext ctx)
        {
            var sofaFile = ScriptableObject.CreateInstance<SOFAFile>();

            sofaFile.sofaName = Path.GetFileName(ctx.assetPath);
            sofaFile.data = File.ReadAllBytes(ctx.assetPath);
            sofaFile.volume = volume;

            ctx.AddObjectToAsset("sofa file", sofaFile);
            ctx.SetMainObject(sofaFile);
        }
    }
}
