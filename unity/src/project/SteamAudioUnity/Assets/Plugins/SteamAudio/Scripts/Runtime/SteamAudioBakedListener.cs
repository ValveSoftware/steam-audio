//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Baked Listener")]
    public class SteamAudioBakedListener : MonoBehaviour
    {
        [Header("Baked Static Listener Settings")]
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
            identifier.variation = BakedDataVariation.StaticListener;
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
            tasks[0].probeBatches = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
            tasks[0].probeBatchNames = new string[tasks[0].probeBatches.Length];
            tasks[0].probeBatchAssets = new SerializedData[tasks[0].probeBatches.Length];
            for (var i = 0; i < tasks[0].probeBatchNames.Length; ++i)
            {
                tasks[0].probeBatchNames[i] = tasks[0].probeBatches[i].gameObject.name;
                tasks[0].probeBatchAssets[i] = tasks[0].probeBatches[i].GetAsset();
            }

            Baker.BeginBake(tasks);
        }

        void CacheIdentifier()
        {
            mIdentifier = GetBakedDataIdentifier();
        }

        void CacheProbeBatchesUsed()
        {
            mProbeBatchesUsed = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
        }
    }
}