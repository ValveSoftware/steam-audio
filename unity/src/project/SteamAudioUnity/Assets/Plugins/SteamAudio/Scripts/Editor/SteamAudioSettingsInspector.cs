//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioSettings))]
    [CanEditMultipleObjects]
    public class SteamAudioSettingsInspector : Editor
    {
        SerializedProperty mAudioEngine;
        SerializedProperty mPerspectiveCorrection;
        SerializedProperty mPerspectiveCorrectionFactor;
        SerializedProperty mHRTFVolumeNormalizationType;
        SerializedProperty mHRTFVolumeGainDB;
        SerializedProperty mSOFAFiles;
        SerializedProperty mDefaultMaterial;
        SerializedProperty mSceneType;
        SerializedProperty mLayerMask;
        SerializedProperty mMaxOcclusionSamples;
        SerializedProperty mRealTimeRays;
        SerializedProperty mRealTimeBounces;
        SerializedProperty mRealTimeDuration;
        SerializedProperty mRealTimeAmbisonicOrder;
        SerializedProperty mRealTimeMaxSources;
        SerializedProperty mRealTimeCPUCoresPercentage;
        SerializedProperty mRealTimeIrradianceMinDistance;
        SerializedProperty mBakeConvolution;
        SerializedProperty mBakeParametric;
        SerializedProperty mBakingRays;
        SerializedProperty mBakingBounces;
        SerializedProperty mBakingDuration;
        SerializedProperty mBakingAmbisonicOrder;
        SerializedProperty mBakingCPUCoresPercentage;
        SerializedProperty mBakingIrradianceMinDistance;
        SerializedProperty mBakingVisibilitySamples;
        SerializedProperty mBakingVisibilityRadius;
        SerializedProperty mBakingVisibilityThreshold;
        SerializedProperty mBakingVisibilityRange;
        SerializedProperty mBakingPathRange;
        SerializedProperty mBakedPathingCPUCoresPercentage;
        SerializedProperty mSimulationUpdateInterval;
        SerializedProperty mReflectionEffectType;
        SerializedProperty mHybridReverbTransitionTime;
        SerializedProperty mHybridReverbOverlapPercent;
        SerializedProperty mDeviceType;
        SerializedProperty mMaxReservedCUs;
        SerializedProperty mFractionCUsForIRUpdate;
        SerializedProperty mBakingBatchSize;
        SerializedProperty mTANDuration;
        SerializedProperty mTANAmbisonicOrder;
        SerializedProperty mTANMaxSources;

#if !UNITY_2019_2_OR_NEWER
        static string[] sSceneTypes = new string[] { "Phonon", "Embree", "Radeon Rays", "Unity" };
#endif

#if !UNITY_2019_2_OR_NEWER
        static string[] sReflectionEffectTypes = new string[] { "Convolution", "Parametric", "Hybrid", "TrueAudio Next" };
#endif

        private void OnEnable()
        {
            mAudioEngine = serializedObject.FindProperty("audioEngine");
            mPerspectiveCorrection = serializedObject.FindProperty("perspectiveCorrection");
            mPerspectiveCorrectionFactor = serializedObject.FindProperty("perspectiveCorrectionFactor");
            mHRTFVolumeGainDB = serializedObject.FindProperty("hrtfVolumeGainDB");
            mHRTFVolumeNormalizationType = serializedObject.FindProperty("hrtfNormalizationType");
            mSOFAFiles = serializedObject.FindProperty("SOFAFiles");
            mDefaultMaterial = serializedObject.FindProperty("defaultMaterial");
            mSceneType = serializedObject.FindProperty("sceneType");
            mLayerMask = serializedObject.FindProperty("layerMask");
            mMaxOcclusionSamples = serializedObject.FindProperty("maxOcclusionSamples");
            mRealTimeRays = serializedObject.FindProperty("realTimeRays");
            mRealTimeBounces = serializedObject.FindProperty("realTimeBounces");
            mRealTimeDuration = serializedObject.FindProperty("realTimeDuration");
            mRealTimeAmbisonicOrder = serializedObject.FindProperty("realTimeAmbisonicOrder");
            mRealTimeMaxSources = serializedObject.FindProperty("realTimeMaxSources");
            mRealTimeCPUCoresPercentage = serializedObject.FindProperty("realTimeCPUCoresPercentage");
            mRealTimeIrradianceMinDistance = serializedObject.FindProperty("realTimeIrradianceMinDistance");
            mBakeConvolution = serializedObject.FindProperty("bakeConvolution");
            mBakeParametric = serializedObject.FindProperty("bakeParametric");
            mBakingRays = serializedObject.FindProperty("bakingRays");
            mBakingBounces = serializedObject.FindProperty("bakingBounces");
            mBakingDuration = serializedObject.FindProperty("bakingDuration");
            mBakingAmbisonicOrder = serializedObject.FindProperty("bakingAmbisonicOrder");
            mBakingCPUCoresPercentage = serializedObject.FindProperty("bakingCPUCoresPercentage");
            mBakingIrradianceMinDistance = serializedObject.FindProperty("bakingIrradianceMinDistance");
            mBakingVisibilitySamples = serializedObject.FindProperty("bakingVisibilitySamples");
            mBakingVisibilityRadius = serializedObject.FindProperty("bakingVisibilityRadius");
            mBakingVisibilityThreshold = serializedObject.FindProperty("bakingVisibilityThreshold");
            mBakingVisibilityRange = serializedObject.FindProperty("bakingVisibilityRange");
            mBakingPathRange = serializedObject.FindProperty("bakingPathRange");
            mBakedPathingCPUCoresPercentage = serializedObject.FindProperty("bakedPathingCPUCoresPercentage");
            mSimulationUpdateInterval = serializedObject.FindProperty("simulationUpdateInterval");
            mReflectionEffectType = serializedObject.FindProperty("reflectionEffectType");
            mHybridReverbTransitionTime = serializedObject.FindProperty("hybridReverbTransitionTime");
            mHybridReverbOverlapPercent = serializedObject.FindProperty("hybridReverbOverlapPercent");
            mDeviceType = serializedObject.FindProperty("deviceType");
            mMaxReservedCUs = serializedObject.FindProperty("maxReservedComputeUnits");
            mFractionCUsForIRUpdate = serializedObject.FindProperty("fractionComputeUnitsForIRUpdate");
            mBakingBatchSize = serializedObject.FindProperty("bakingBatchSize");
            mTANDuration = serializedObject.FindProperty("TANDuration");
            mTANAmbisonicOrder = serializedObject.FindProperty("TANAmbisonicOrder");
            mTANMaxSources = serializedObject.FindProperty("TANMaxSources");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(mAudioEngine);
            EditorGUILayout.PropertyField(mPerspectiveCorrection, new UnityEngine.GUIContent("Enable Perspective Correction"));

            if (mPerspectiveCorrection.boolValue)
                EditorGUILayout.PropertyField(mPerspectiveCorrectionFactor);

            EditorGUILayout.PropertyField(mHRTFVolumeGainDB, new UnityEngine.GUIContent("HRTF Volume Gain (dB)"));
            EditorGUILayout.PropertyField(mHRTFVolumeNormalizationType, new UnityEngine.GUIContent("HRTF Normalization Type"));

            EditorGUILayout.PropertyField(mSOFAFiles, true);
            EditorGUILayout.PropertyField(mDefaultMaterial);
#if UNITY_2019_2_OR_NEWER
            EditorGUILayout.PropertyField(mSceneType);
#else
            SceneTypeField();
#endif

            if (((SceneType) mSceneType.enumValueIndex) == SceneType.Custom)
            {
                EditorGUILayout.PropertyField(mLayerMask);
            }

            EditorGUILayout.PropertyField(mMaxOcclusionSamples);

            EditorGUILayout.PropertyField(mRealTimeRays);
            EditorGUILayout.PropertyField(mRealTimeBounces);
            EditorGUILayout.PropertyField(mRealTimeDuration);
            EditorGUILayout.PropertyField(mRealTimeAmbisonicOrder);
            EditorGUILayout.PropertyField(mRealTimeMaxSources);
            EditorGUILayout.PropertyField(mRealTimeCPUCoresPercentage);
            EditorGUILayout.PropertyField(mRealTimeIrradianceMinDistance);

            EditorGUILayout.PropertyField(mBakeConvolution);
            EditorGUILayout.PropertyField(mBakeParametric);
            EditorGUILayout.PropertyField(mBakingRays);
            EditorGUILayout.PropertyField(mBakingBounces);
            EditorGUILayout.PropertyField(mBakingDuration);
            EditorGUILayout.PropertyField(mBakingAmbisonicOrder);
            EditorGUILayout.PropertyField(mBakingCPUCoresPercentage);
            EditorGUILayout.PropertyField(mBakingIrradianceMinDistance);

            EditorGUILayout.PropertyField(mBakingVisibilitySamples);
            EditorGUILayout.PropertyField(mBakingVisibilityRadius);
            EditorGUILayout.PropertyField(mBakingVisibilityThreshold);
            EditorGUILayout.PropertyField(mBakingVisibilityRange);
            EditorGUILayout.PropertyField(mBakingPathRange);
            EditorGUILayout.PropertyField(mBakedPathingCPUCoresPercentage);

            EditorGUILayout.PropertyField(mSimulationUpdateInterval);

#if UNITY_2019_2_OR_NEWER
            EditorGUILayout.PropertyField(mReflectionEffectType);
#else
            ReflectionEffectTypeField();
#endif

            if (((ReflectionEffectType) mReflectionEffectType.enumValueIndex) == ReflectionEffectType.Hybrid)
            {
                EditorGUILayout.PropertyField(mHybridReverbTransitionTime);
                EditorGUILayout.PropertyField(mHybridReverbOverlapPercent);
            }

            if (((SceneType) mSceneType.enumValueIndex) == SceneType.RadeonRays ||
                ((ReflectionEffectType) mReflectionEffectType.enumValueIndex) == ReflectionEffectType.TrueAudioNext)
            {
                EditorGUILayout.PropertyField(mDeviceType);
                EditorGUILayout.PropertyField(mMaxReservedCUs);
                EditorGUILayout.PropertyField(mFractionCUsForIRUpdate);

                if (((SceneType) mSceneType.enumValueIndex) == SceneType.RadeonRays)
                { 
                    EditorGUILayout.PropertyField(mBakingBatchSize);
                }

                if (((ReflectionEffectType) mReflectionEffectType.enumValueIndex) == ReflectionEffectType.TrueAudioNext)
                {
                    EditorGUILayout.PropertyField(mTANDuration);
                    EditorGUILayout.PropertyField(mTANAmbisonicOrder);
                    EditorGUILayout.PropertyField(mTANMaxSources);
                }
            }

            serializedObject.ApplyModifiedProperties();
        }

#if !UNITY_2019_2_OR_NEWER
        void SceneTypeField()
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Ray Tracer Settings", EditorStyles.boldLabel);
            mSceneType.enumValueIndex = EditorGUILayout.Popup(mSceneType.displayName, mSceneType.enumValueIndex, sSceneTypes);
        }
#endif

#if !UNITY_2019_2_OR_NEWER
        void ReflectionEffectTypeField()
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Reflection Effect Settings", EditorStyles.boldLabel);
            mReflectionEffectType.enumValueIndex = EditorGUILayout.Popup(mReflectionEffectType.displayName, mReflectionEffectType.enumValueIndex, sReflectionEffectTypes);
        }
#endif
    }
}
