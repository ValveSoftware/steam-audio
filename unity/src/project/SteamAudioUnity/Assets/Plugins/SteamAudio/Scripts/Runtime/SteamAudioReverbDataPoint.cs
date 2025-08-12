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

using UnityEngine;
using System;
using System.Runtime.InteropServices;
using System.IO;
using System.Collections.Generic;

#if UNITY_EDITOR
using UnityEditor;
#endif

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Reverb Data Point")]
    [ExecuteAlways]
    public class SteamAudioReverbDataPoint : MonoBehaviour
    {
        public int sampleRate = 44100;

        [Range(0, 3)]
        public int ambisonicOrder = 0;

        [Range(0.1f, 10.0f)]
        public float reverbDuration = 1.0f;

        public bool storeEnergyField = false;
        public bool storeImpulseResponse = true;

        [Header("Baked Data")]
        public SteamAudioReverbData reverbData;

        static List<SteamAudioReverbData> sAssetsToFlush = null;

#if STEAMAUDIO_ENABLED
        void CreateFolderRecursively(string path)
        {
#if UNITY_EDITOR
            string[] parts = path.Split('/');
            string currentPath = "";

            for (int i = 1; i < parts.Length; i++)
            {
                currentPath += "/" + parts[i];
                string fullPath = "Assets" + currentPath;

                if (!AssetDatabase.IsValidFolder(fullPath))
                {
                    string parentPath = string.Join("/", parts, 0, i);
                    AssetDatabase.CreateFolder(parentPath, parts[i]);
                    AssetDatabase.Refresh();
                }
            }
#endif
        }

        public static string GetAssetFolderPath()
        {
            // Must start with Assets/
            return "Assets/Resources/ReverbData";
        }

        static BakedDataIdentifier GetBakedDataIdentifier()
        {
            var identifier = new BakedDataIdentifier { };
            identifier.type = BakedDataType.Reflections;
            identifier.variation = BakedDataVariation.Reverb;
            return identifier;
        }

        public void EnsureValidData()
        {
#if UNITY_EDITOR
            if (reverbData != null)
                return;

            var folderPath = GetAssetFolderPath();

            if (!AssetDatabase.IsValidFolder(folderPath))
            {
                CreateFolderRecursively(folderPath);
            }

            string assetName = $"{gameObject.name}_{Guid.NewGuid()}.asset";
            string path = Path.Combine(folderPath, assetName);

            reverbData = ScriptableObject.CreateInstance<SteamAudioReverbData>();
            reverbData.Initialize();

            AssetDatabase.CreateAsset(reverbData, path);
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            EditorUtility.SetDirty(this);
#endif
        }

        public static void BeginBake(SteamAudioReverbDataPoint[] probes)
        {
#if UNITY_EDITOR
            AssetDatabase.StartAssetEditing();
#endif

            var tasks = new BakedDataTask[probes.Length];
            for (var i = 0; i < probes.Length; i++)
            {
                tasks[i].gameObject = probes[i].gameObject;
                tasks[i].component = probes[i];
                tasks[i].name = probes[i].gameObject.name;
                tasks[i].identifier = GetBakedDataIdentifier();
                tasks[i].probeBatches = null;
                tasks[i].probeBatchNames = null;
                tasks[i].probeBatchAssets = null;
                tasks[i].probe = probes[i];
                tasks[i].probePosition = probes[i].gameObject.transform.position;

                tasks[i].probe.EnsureValidData();
            }

#if UNITY_EDITOR
            AssetDatabase.StopAssetEditing();
#endif

            Baker.BeginBake(tasks, true);
        }

        public void UpdateEnergyField(IntPtr energyField)
        {
            if (!storeEnergyField)
            {
                reverbData.reverbEnergyFieldNumChannels = 0;
                reverbData.reverbEnergyFieldNumBands = 0;
                reverbData.reverbEnergyFieldNumBins = 0;
                reverbData.reverbEnergyField = null;
                return;
            }

            if (energyField == IntPtr.Zero)
                return;

            reverbData.reverbEnergyFieldNumChannels = API.iplEnergyFieldGetNumChannels(energyField);
            reverbData.reverbEnergyFieldNumBands = 3;
            reverbData.reverbEnergyFieldNumBins = API.iplEnergyFieldGetNumBins(energyField);
            reverbData.reverbEnergyField = new float[reverbData.reverbEnergyFieldNumChannels * reverbData.reverbEnergyFieldNumBands * reverbData.reverbEnergyFieldNumBins];

            IntPtr reverbEnergyFieldData = API.iplEnergyFieldGetData(energyField);
            if (reverbEnergyFieldData == IntPtr.Zero)
                return;

            Marshal.Copy(reverbEnergyFieldData, reverbData.reverbEnergyField, 0, reverbData.reverbEnergyField.Length);
        }

        public void UpdateImpulseResponse(IntPtr ir)
        {
            if (!storeImpulseResponse)
            {
                reverbData.reverbIRNumChannels = 0;
                reverbData.reverbIRNumSamples = 0;
                reverbData.reverbIR = null;
                return;
            }

            if (ir == IntPtr.Zero)
                return;

            reverbData.reverbIRNumChannels = API.iplImpulseResponseGetNumChannels(ir);
            reverbData.reverbIRNumSamples = API.iplImpulseResponseGetNumSamples(ir);
            reverbData.reverbIR = new float[reverbData.reverbIRNumChannels * reverbData.reverbIRNumSamples];

            IntPtr reverbIRData = API.iplImpulseResponseGetData(ir);
            if (reverbIRData == IntPtr.Zero)
                return;
            
            Marshal.Copy(reverbIRData, reverbData.reverbIR, 0, reverbData.reverbIR.Length);
        }

        public void WriteReverbDataToFile(bool flush = true)
        {
            if (flush)
            {
                FlushWrite(reverbData);
            }
            else
            {
                if (sAssetsToFlush == null)
                {
                    sAssetsToFlush = new List<SteamAudioReverbData>();
                }

                sAssetsToFlush.Add(reverbData);
            }
        }

        public static void FlushWrite(SteamAudioReverbData dataAsset)
        {
#if UNITY_EDITOR
            var assetPaths = new string[1];
            assetPaths[0] = AssetDatabase.GetAssetPath(dataAsset);

            // TODO: Deprecate older versions of Unity.
#if UNITY_2017_3_OR_NEWER
            AssetDatabase.ForceReserializeAssets(assetPaths);
#endif
#endif
        }

        public static void FlushAllWrites()
        {
#if UNITY_EDITOR
            AssetDatabase.StartAssetEditing();
            if (sAssetsToFlush != null)
            {
                for (int i = 0; i < sAssetsToFlush.Count; ++i)
                {
                    EditorUtility.DisplayProgressBar("Saving Assets", $"{sAssetsToFlush[i]}", ((float)(i + 1)) / sAssetsToFlush.Count);
                    FlushWrite(sAssetsToFlush[i]);
                }

                sAssetsToFlush.Clear();
            }
            AssetDatabase.StopAssetEditing();
#endif
        }
#endif
    }
}