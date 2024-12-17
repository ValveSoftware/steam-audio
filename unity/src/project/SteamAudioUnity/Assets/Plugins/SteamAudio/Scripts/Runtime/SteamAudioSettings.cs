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
#if UNITY_EDITOR
using UnityEditor;
#endif

namespace SteamAudio
{
    public enum AudioEngineType
    {
        Unity,
        FMODStudio,
        Wwise
    }

    [CreateAssetMenu(menuName = "Steam Audio/Steam Audio Settings")]
    public class SteamAudioSettings : ScriptableObject
    {
        [Header("Audio Engine Settings")]
        public AudioEngineType audioEngine = AudioEngineType.Unity;

        [Header("HRTF Settings")]
        public bool hrtfDisabled = false;
        public bool perspectiveCorrection = false;
        [Range(.25f, 4.0f)]
        public float perspectiveCorrectionFactor = 1.0f;
        [Range(-12.0f, 12.0f)]
        public float hrtfVolumeGainDB = 0.0f;
        public HRTFNormType hrtfNormalizationType = HRTFNormType.None;
        public SOFAFile[] SOFAFiles = null;

        [Header("Material Settings")]
        public SteamAudioMaterial defaultMaterial = null;

        [Header("Ray Tracer Settings")]
        public SceneType sceneType = SceneType.Default;
        public LayerMask layerMask = new LayerMask();

        [Header("Occlusion Settings")]
        [Range(1, 128)]
        public int maxOcclusionSamples = 16;

        [Header("Real-time Reflections Settings")]
        [Range(1024, 65536)]
        public int realTimeRays = 4096;
        [Range(1, 64)]
        public int realTimeBounces = 4;
        [Range(0.1f, 10.0f)]
        public float realTimeDuration = 1.0f;
        [Range(0, 3)]
        public int realTimeAmbisonicOrder = 1;
        [Range(1, 128)]
        public int realTimeMaxSources = 32;
        [Range(0, 100)]
        public int realTimeCPUCoresPercentage = 5;
        [Range(0.1f, 10.0f)]
        public float realTimeIrradianceMinDistance = 1.0f;

        [Header("Baked Reflections Settings")]
        public bool bakeConvolution = true;
        public bool bakeParametric = false;
        [Range(1024, 65536)]
        public int bakingRays = 16384;
        [Range(1, 64)]
        public int bakingBounces = 16;
        [Range(0.1f, 10.0f)]
        public float bakingDuration = 1.0f;
        [Range(0, 3)]
        public int bakingAmbisonicOrder = 1;
        [Range(0, 100)]
        public int bakingCPUCoresPercentage = 50;
        [Range(0.1f, 10.0f)]
        public float bakingIrradianceMinDistance = 1.0f;

        [Header("Baked Pathing Settings")]
        [Range(1, 32)]
        public int bakingVisibilitySamples = 4;
        [Range(0.0f, 2.0f)]
        public float bakingVisibilityRadius = 1.0f;
        [Range(0.0f, 1.0f)]
        public float bakingVisibilityThreshold = 0.1f;
        [Range(0.0f, 1000.0f)]
        public float bakingVisibilityRange = 1000.0f;
        [Range(0.0f, 1000.0f)]
        public float bakingPathRange = 1000.0f;
        [Range(0, 100)]
        public int bakedPathingCPUCoresPercentage = 50;

        [Header("Simulation Update Settings")]
        [Range(0.1f, 1.0f)]
        public float simulationUpdateInterval = 0.1f;

        [Header("Reflection Effect Settings")]
        public ReflectionEffectType reflectionEffectType = ReflectionEffectType.Convolution;

        [Header("Hybrid Reverb Settings")]
        [Range(0.1f, 2.0f)]
        public float hybridReverbTransitionTime = 1.0f;
        [Range(0, 100)]
        public int hybridReverbOverlapPercent = 25;

        [Header("OpenCL Settings")]
        public OpenCLDeviceType deviceType = OpenCLDeviceType.GPU;
        [Range(0, 16)]
        public int maxReservedComputeUnits = 8;
        [Range(0.0f, 1.0f)]
        public float fractionComputeUnitsForIRUpdate = 0.5f;

        [Header("Radeon Rays Settings")]
        [Range(1, 16)]
        public int bakingBatchSize = 8;

        [Header("TrueAudio Next Settings")]
        [Range(0.1f, 10.0f)]
        public float TANDuration = 1.0f;
        [Range(0, 3)]
        public int TANAmbisonicOrder = 1;
        [Range(1, 128)]
        public int TANMaxSources = 32;

        [Header("Advanced Settings")]
        public bool EnableValidation = false;

        static SteamAudioSettings sSingleton = null;

        public static SteamAudioSettings Singleton
        {
            get
            {
                if (sSingleton == null)
                {
                    sSingleton = Resources.Load<SteamAudioSettings>("SteamAudioSettings");
                    if (sSingleton == null)
                    {
                        sSingleton = CreateInstance<SteamAudioSettings>();
                        sSingleton.name = "Steam Audio Settings";

#if UNITY_EDITOR
                        sSingleton.defaultMaterial = (SteamAudioMaterial) AssetDatabase.LoadAssetAtPath("Assets/Plugins/SteamAudio/Resources/Materials/Default.asset", typeof(SteamAudioMaterial));

                        AssetDatabase.CreateAsset(sSingleton, "Assets/Plugins/SteamAudio/Resources/SteamAudioSettings.asset");
#endif
                    }
                }

                return sSingleton;
            }
        }
    }
}
