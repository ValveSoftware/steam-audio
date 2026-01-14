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

using System;
using UnityEngine;
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Baked Source")]
    public class SteamAudioBakedSource : MonoBehaviour
    {
        [Header("Baked Static Source Settings")]
        [Range(0.0f, 1000.0f)]
        public float influenceRadius = 1000.0f;
        public bool useAllProbeBatches = false;
        public SteamAudioProbeBatch[] probeBatches = null;

        [SerializeField]
        int mTotalDataSize = 0;
        [SerializeField]
        int[] mProbeDataSizes = null;
        [SerializeField]
        BakedDataIdentifier mIdentifier = new BakedDataIdentifier { };
        [SerializeField]
        SteamAudioProbeBatch[] mProbeBatchesUsed = null;

#if STEAMAUDIO_ENABLED
        public int GetTotalDataSize()
        {
            return mTotalDataSize;
        }

        public int[] GetProbeDataSizes()
        {
            return mProbeDataSizes;
        }

        public int GetSizeForProbeBatch(int index)
        {
            return mProbeDataSizes[index];
        }

        public SteamAudioProbeBatch[] GetProbeBatchesUsed()
        {
            if (mProbeBatchesUsed == null)
            {
                CacheProbeBatchesUsed();
            }

            return mProbeBatchesUsed;
        }

        public BakedDataIdentifier GetBakedDataIdentifier()
        {
            var identifier = new BakedDataIdentifier { };
            identifier.type = BakedDataType.Reflections;
            identifier.variation = BakedDataVariation.StaticSource;
            identifier.endpointInfluence.center = Common.ConvertVector(transform.position);
            identifier.endpointInfluence.radius = influenceRadius;
            return identifier;
        }

        private void OnDrawGizmosSelected()
        {
            var oldColor = Gizmos.color;
            var oldMatrix = Gizmos.matrix;

            Gizmos.color = Color.yellow;
            Gizmos.DrawWireSphere(transform.position, influenceRadius);

            Gizmos.color = Color.magenta;

            if (mProbeBatchesUsed != null)
            {
                foreach (var probeBatch in mProbeBatchesUsed)
                {
                    if (probeBatch == null)
                        continue;

                    Gizmos.matrix = probeBatch.transform.localToWorldMatrix;
                    Gizmos.DrawWireCube(new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(1, 1, 1));
                }
            }

            Gizmos.matrix = oldMatrix;
            Gizmos.color = oldColor;
        }

        public void UpdateBakedDataStatistics()
        {
            if (mProbeBatchesUsed == null)
                return;

            mProbeDataSizes = new int[mProbeBatchesUsed.Length];
            mTotalDataSize = 0;

            for (var i = 0; i < mProbeBatchesUsed.Length; ++i)
            {
                mProbeDataSizes[i] = mProbeBatchesUsed[i].GetSizeForLayer(mIdentifier);
                mTotalDataSize += mProbeDataSizes[i];
            }
        }

        public void BeginBake()
        {
            CacheIdentifier();
            CacheProbeBatchesUsed();

            var tasks = new BakedDataTask[1];
            tasks[0].gameObject = gameObject;
            tasks[0].component = this;
            tasks[0].name = gameObject.name;
            tasks[0].identifier = mIdentifier;
#if UNITY_2020_3_OR_NEWER
            tasks[0].probeBatches = (useAllProbeBatches) ? FindObjectsByType<SteamAudioProbeBatch>(FindObjectsSortMode.None) : probeBatches;
#else
            tasks[0].probeBatches = (useAllProbeBatches) ?  FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
#endif
            tasks[0].probeBatchNames = new string[tasks[0].probeBatches.Length];
            tasks[0].probeBatchAssets = new SerializedData[tasks[0].probeBatches.Length];
            for (var i = 0; i < tasks[0].probeBatchNames.Length; ++i)
            {
                tasks[0].probeBatchNames[i] = tasks[0].probeBatches[i].gameObject.name;
                tasks[0].probeBatchAssets[i] = tasks[0].probeBatches[i].GetAsset();
            }

            Baker.BeginBake(tasks);
        }

        public static void BeginBake(SteamAudioBakedSource[] bakedSources)
        {
#if UNITY_EDITOR
            AssetDatabase.StartAssetEditing();
#endif

            var tasks = new BakedDataTask[bakedSources.Length];

            for (var i = 0; i < bakedSources.Length; i++)
            {
                tasks[i].gameObject = bakedSources[i].gameObject;
                tasks[i].component = bakedSources[i];
                tasks[i].name = bakedSources[i].gameObject.name;
                tasks[i].identifier = bakedSources[i].GetBakedDataIdentifier();
#if UNITY_2020_3_OR_NEWER
                tasks[i].probeBatches = (bakedSources[i].useAllProbeBatches) ? FindObjectsByType<SteamAudioProbeBatch>(FindObjectsSortMode.None) : bakedSources[i].probeBatches;
#else
                tasks[i].probeBatches = (bakedSources[i].useAllProbeBatches) ?  FindObjectsOfType<SteamAudioProbeBatch>() : bakedSources[i].probeBatches;
#endif
                tasks[i].probeBatchNames = new string[tasks[i].probeBatches.Length];
                tasks[i].probeBatchAssets = new SerializedData[tasks[i].probeBatches.Length];
                for (var j = 0; j < tasks[i].probeBatchNames.Length; ++j)
                {
                    tasks[i].probeBatchNames[j] = tasks[i].probeBatches[j].gameObject.name;
                    tasks[i].probeBatchAssets[j] = tasks[i].probeBatches[j].GetAsset();
                }
            }

#if UNITY_EDITOR
            AssetDatabase.StopAssetEditing();
#endif

            Baker.BeginBake(tasks);
        }

        void CacheIdentifier()
        {
            mIdentifier = GetBakedDataIdentifier();
        }

        void CacheProbeBatchesUsed()
        {
#if UNITY_2020_3_OR_NEWER
            mProbeBatchesUsed = (useAllProbeBatches) ? FindObjectsByType<SteamAudioProbeBatch>(FindObjectsSortMode.None) : probeBatches;
#else
            mProbeBatchesUsed = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
#endif
        }
#endif
    }
}