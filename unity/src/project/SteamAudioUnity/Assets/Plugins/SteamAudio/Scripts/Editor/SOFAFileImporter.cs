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
