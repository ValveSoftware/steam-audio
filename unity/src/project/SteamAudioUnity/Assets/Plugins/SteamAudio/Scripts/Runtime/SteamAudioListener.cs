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

namespace SteamAudio
{
    public enum ReverbType
    {
        Realtime,
        Baked
    }

    [AddComponentMenu("Steam Audio/Steam Audio Listener")]
    public class SteamAudioListener : MonoBehaviour
    {
        [Header("Baked Static Listener Settings")]
        public SteamAudioBakedListener currentBakedListener = null;

        [Header("Reverb Settings")]
        public bool applyReverb = false;
        public ReverbType reverbType = ReverbType.Realtime;

        [Header("Baked Reverb Settings")]
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
        Simulator mSimulator = null;
        Source mSource = null;

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

        private void Awake()
        {
            Reinitialize();
        }

        public void Reinitialize()
        {
            mSimulator = SteamAudioManager.Simulator;

            var settings = SteamAudioManager.GetSimulationSettings(false);
            mSource = new Source(SteamAudioManager.Simulator, settings);

            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }

        private void OnDestroy()
        {
            if (mSource != null)
            {
                mSource.Release();
            }
        }

        private void Start()
        {
            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }

        private void OnEnable()
        {
            if (applyReverb)
            {
                mSource.AddToSimulator(mSimulator);
                SteamAudioManager.AddListener(this);
                SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
            }
        }

        private void OnDisable()
        {
            if (applyReverb)
            {
                SteamAudioManager.RemoveListener(this);
                mSource.RemoveFromSimulator(mSimulator);
                SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
            }
        }

        private void Update()
        {
            SteamAudioManager.GetAudioEngineState().SetReverbSource(mSource);
        }

        public BakedDataIdentifier GetBakedDataIdentifier()
        {
            var identifier = new BakedDataIdentifier { };
            identifier.type = BakedDataType.Reflections;
            identifier.variation = BakedDataVariation.Reverb;
            return identifier;
        }

        public void SetInputs(SimulationFlags flags)
        {
            var inputs = new SimulationInputs { };
            inputs.source.origin = Common.ConvertVector(transform.position);
            inputs.source.ahead = Common.ConvertVector(transform.forward);
            inputs.source.up = Common.ConvertVector(transform.up);
            inputs.source.right = Common.ConvertVector(transform.right);
            inputs.distanceAttenuationModel.type = DistanceAttenuationModelType.Default;
            inputs.airAbsorptionModel.type = AirAbsorptionModelType.Default;
            inputs.reverbScaleLow = 1.0f;
            inputs.reverbScaleMid = 1.0f;
            inputs.reverbScaleHigh = 1.0f;
            inputs.hybridReverbTransitionTime = SteamAudioSettings.Singleton.hybridReverbTransitionTime;
            inputs.hybridReverbOverlapPercent = SteamAudioSettings.Singleton.hybridReverbOverlapPercent / 100.0f;
            inputs.baked = (reverbType != ReverbType.Realtime) ? Bool.True : Bool.False;
            if (reverbType == ReverbType.Baked)
            {
                inputs.bakedDataIdentifier = GetBakedDataIdentifier();
            }

            inputs.flags = 0;
            if (applyReverb)
            {
                inputs.flags = inputs.flags | SimulationFlags.Reflections;
            }

            inputs.directFlags = 0;

            mSource.SetInputs(flags, inputs);
        }

        public void UpdateOutputs(SimulationFlags flags)
        {}

        private void OnDrawGizmosSelected()
        {
            var oldColor = Gizmos.color;
            var oldMatrix = Gizmos.matrix;

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
            tasks[0].name = "Reverb";
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
            mIdentifier.type = BakedDataType.Reflections;
            mIdentifier.variation = BakedDataVariation.Reverb;
        }

        void CacheProbeBatchesUsed()
        {
            mProbeBatchesUsed = (useAllProbeBatches) ? FindObjectsOfType<SteamAudioProbeBatch>() : probeBatches;
        }
#endif
    }
}