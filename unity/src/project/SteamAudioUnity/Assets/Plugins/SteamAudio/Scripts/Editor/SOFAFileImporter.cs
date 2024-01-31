//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
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
    [ScriptedImporter(2, "sofa")]
    public class SOFAFileImporter : ScriptedImporter
    {
        [Range(-12.0f, 12.0f)]
        public float hrtfVolumeGainDB = 0.0f;
        public HRTFNormType hrtfNormalizationType = HRTFNormType.None;

        public override void OnImportAsset(AssetImportContext ctx)
        {
            var sofaFile = ScriptableObject.CreateInstance<SOFAFile>();

            sofaFile.sofaName = Path.GetFileName(ctx.assetPath);
            sofaFile.data = File.ReadAllBytes(ctx.assetPath);
            sofaFile.volume = hrtfVolumeGainDB;
            sofaFile.normType = hrtfNormalizationType;

            ctx.AddObjectToAsset("sofa file", sofaFile);
            ctx.SetMainObject(sofaFile);
        }
    }
}
