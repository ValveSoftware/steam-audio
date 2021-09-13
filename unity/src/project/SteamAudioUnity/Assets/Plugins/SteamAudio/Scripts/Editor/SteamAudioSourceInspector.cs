//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioSource))]
    public class SteamAudioSourceInspector : Editor
    {
        SerializedProperty mDirectBinaural;
        SerializedProperty mInterpolation;
        SerializedProperty mDistanceAttenuation;
        SerializedProperty mDistanceAttenuationInput;
        SerializedProperty mAirAbsorption;
        SerializedProperty mAirAbsorptionInput;
        SerializedProperty mAirAbsorptionLow;
        SerializedProperty mAirAbsorptionMid;
        SerializedProperty mAirAbsorptionHigh;
        SerializedProperty mDirectivity;
        SerializedProperty mDirectivityInput;
        SerializedProperty mDipoleWeight;
        SerializedProperty mDipolePower;
        SerializedProperty mDirectivityValue;
        SerializedProperty mOcclusion;
        SerializedProperty mOcclusionInput;
        SerializedProperty mOcclusionType;
        SerializedProperty mOcclusionRadius;
        SerializedProperty mOcclusionSamples;
        SerializedProperty mOcclusionValue;
        SerializedProperty mTransmission;
        SerializedProperty mTransmissionType;
        SerializedProperty mTransmissionInput;
        SerializedProperty mTransmissionLow;
        SerializedProperty mTransmissionMid;
        SerializedProperty mTransmissionHigh;
        SerializedProperty mDirectMixLevel;
        SerializedProperty mReflections;
        SerializedProperty mReflectionsType;
        SerializedProperty mUseDistanceCurveForReflections;
        SerializedProperty mCurrentBakedSource;
        SerializedProperty mApplyHRTFToReflections;
        SerializedProperty mReflectionsMixLevel;
        SerializedProperty mPathing;
        SerializedProperty mPathingProbeBatch;
        SerializedProperty mPathValidation;
        SerializedProperty mFindAlternatePaths;
        SerializedProperty mApplyHRTFToPathing;
        SerializedProperty mPathingMixLevel;

        Texture2D mDirectivityPreview = null;
        float[] mDirectivitySamples = null;
        Vector2[] mDirectivityPositions = null;

        private void OnEnable()
        {
            mDirectBinaural = serializedObject.FindProperty("directBinaural");
            mInterpolation = serializedObject.FindProperty("interpolation");
            mDistanceAttenuation = serializedObject.FindProperty("distanceAttenuation");
            mDistanceAttenuationInput = serializedObject.FindProperty("distanceAttenuationInput");
            mAirAbsorption = serializedObject.FindProperty("airAbsorption");
            mAirAbsorptionInput = serializedObject.FindProperty("airAbsorptionInput");
            mAirAbsorptionLow = serializedObject.FindProperty("airAbsorptionLow");
            mAirAbsorptionMid = serializedObject.FindProperty("airAbsorptionMid");
            mAirAbsorptionHigh = serializedObject.FindProperty("airAbsorptionHigh");
            mDirectivity = serializedObject.FindProperty("directivity");
            mDirectivityInput = serializedObject.FindProperty("directivityInput");
            mDipoleWeight = serializedObject.FindProperty("dipoleWeight");
            mDipolePower = serializedObject.FindProperty("dipolePower");
            mDirectivityValue = serializedObject.FindProperty("directivityValue");
            mOcclusion = serializedObject.FindProperty("occlusion");
            mOcclusionInput = serializedObject.FindProperty("occlusionInput");
            mOcclusionType = serializedObject.FindProperty("occlusionType");
            mOcclusionRadius = serializedObject.FindProperty("occlusionRadius");
            mOcclusionSamples = serializedObject.FindProperty("occlusionSamples");
            mOcclusionValue = serializedObject.FindProperty("occlusionValue");
            mTransmission = serializedObject.FindProperty("transmission");
            mTransmissionType = serializedObject.FindProperty("transmissionType");
            mTransmissionInput = serializedObject.FindProperty("transmissionInput");
            mTransmissionLow = serializedObject.FindProperty("transmissionLow");
            mTransmissionMid = serializedObject.FindProperty("transmissionMid");
            mTransmissionHigh = serializedObject.FindProperty("transmissionHigh");
            mDirectMixLevel = serializedObject.FindProperty("directMixLevel");
            mReflections = serializedObject.FindProperty("reflections");
            mReflectionsType = serializedObject.FindProperty("reflectionsType");
            mUseDistanceCurveForReflections = serializedObject.FindProperty("useDistanceCurveForReflections");
            mCurrentBakedSource = serializedObject.FindProperty("currentBakedSource");
            mApplyHRTFToReflections = serializedObject.FindProperty("applyHRTFToReflections");
            mReflectionsMixLevel = serializedObject.FindProperty("reflectionsMixLevel");
            mPathing = serializedObject.FindProperty("pathing");
            mPathingProbeBatch = serializedObject.FindProperty("pathingProbeBatch");
            mPathValidation = serializedObject.FindProperty("pathValidation");
            mFindAlternatePaths = serializedObject.FindProperty("findAlternatePaths");
            mApplyHRTFToPathing = serializedObject.FindProperty("applyHRTFToPathing");
            mPathingMixLevel = serializedObject.FindProperty("pathingMixLevel");
        }

        public override void OnInspectorGUI()
        {
            var audioEngineIsUnity = (SteamAudioSettings.Singleton.audioEngine == AudioEngineType.Unity);

            serializedObject.Update();

            if (audioEngineIsUnity)
            {
                EditorGUILayout.PropertyField(mDirectBinaural);
                EditorGUILayout.PropertyField(mInterpolation);
            }

            if (audioEngineIsUnity)
            {
                EditorGUILayout.PropertyField(mDistanceAttenuation);
                if (mDistanceAttenuation.boolValue)
                {
                    EditorGUILayout.PropertyField(mDistanceAttenuationInput);
                }
            }

            if (audioEngineIsUnity)
            {
                EditorGUILayout.PropertyField(mAirAbsorption);
                if (mAirAbsorption.boolValue)
                {
                    EditorGUILayout.PropertyField(mAirAbsorptionInput);
                    if ((AirAbsorptionInput)mAirAbsorptionInput.enumValueIndex == AirAbsorptionInput.UserDefined)
                    {
                        EditorGUILayout.PropertyField(mAirAbsorptionLow);
                        EditorGUILayout.PropertyField(mAirAbsorptionMid);
                        EditorGUILayout.PropertyField(mAirAbsorptionHigh);
                    }
                }
            }

            if (audioEngineIsUnity)
            {
                EditorGUILayout.PropertyField(mDirectivity);
                if (mDirectivity.boolValue)
                {
                    EditorGUILayout.PropertyField(mDirectivityInput);

                    if ((DirectivityInput) mDirectivityInput.enumValueIndex == DirectivityInput.SimulationDefined)
                    {
                        EditorGUILayout.PropertyField(mDipoleWeight);
                        EditorGUILayout.PropertyField(mDipolePower);
                        DrawDirectivity(mDipoleWeight.floatValue, mDipolePower.floatValue);
                    }
                    else if ((DirectivityInput) mDirectivityInput.enumValueIndex == DirectivityInput.UserDefined)
                    {
                        EditorGUILayout.PropertyField(mDirectivityValue);
                    }
                }
            }

            EditorGUILayout.PropertyField(mOcclusion);
            if (mOcclusion.boolValue)
            {
                if (audioEngineIsUnity)
                {
                    EditorGUILayout.PropertyField(mOcclusionInput);
                }

                if (!audioEngineIsUnity ||
                    (OcclusionInput) mOcclusionInput.enumValueIndex == OcclusionInput.SimulationDefined)
                {
                    EditorGUILayout.PropertyField(mOcclusionType);
                    if ((OcclusionType) mOcclusionType.enumValueIndex == OcclusionType.Volumetric)
                    {
                        EditorGUILayout.PropertyField(mOcclusionRadius);
                        EditorGUILayout.PropertyField(mOcclusionSamples);
                    }
                }
                else if ((OcclusionInput) mOcclusionInput.enumValueIndex == OcclusionInput.UserDefined)
                {
                    EditorGUILayout.PropertyField(mOcclusionValue);
                }

                EditorGUILayout.PropertyField(mTransmission);
                if (audioEngineIsUnity)
                {
                    if (mTransmission.boolValue)
                    {
                        EditorGUILayout.PropertyField(mTransmissionType);
                        EditorGUILayout.PropertyField(mTransmissionInput);
                        if ((TransmissionInput)mTransmissionInput.enumValueIndex == TransmissionInput.UserDefined)
                        {
                            if (mTransmissionType.enumValueIndex == (int)TransmissionType.FrequencyDependent)
                            {
                                EditorGUILayout.PropertyField(mTransmissionLow);
                                EditorGUILayout.PropertyField(mTransmissionMid);
                                EditorGUILayout.PropertyField(mTransmissionHigh);
                            }
                            else
                            {
                                EditorGUILayout.PropertyField(mTransmissionMid);
                            }
                        }
                    }
                }
            }

            if (audioEngineIsUnity)
            {
                EditorGUILayout.PropertyField(mDirectMixLevel);
            }

            EditorGUILayout.PropertyField(mReflections);
            if (mReflections.boolValue)
            {
                EditorGUILayout.PropertyField(mReflectionsType);

                if (audioEngineIsUnity &&
                    mDistanceAttenuation.boolValue &&
                    (DistanceAttenuationInput) mDistanceAttenuationInput.enumValueIndex == DistanceAttenuationInput.CurveDriven)
                {
                    EditorGUILayout.PropertyField(mUseDistanceCurveForReflections);
                }

                if ((ReflectionsType) mReflectionsType.enumValueIndex == ReflectionsType.BakedStaticSource)
                {
                    EditorGUILayout.PropertyField(mCurrentBakedSource);
                }

                if (audioEngineIsUnity)
                {
                    EditorGUILayout.PropertyField(mApplyHRTFToReflections);
                    EditorGUILayout.PropertyField(mReflectionsMixLevel);
                }
            }

            EditorGUILayout.PropertyField(mPathing);
            if (mPathing.boolValue)
            {
                EditorGUILayout.PropertyField(mPathingProbeBatch);
                EditorGUILayout.PropertyField(mPathValidation);
                EditorGUILayout.PropertyField(mFindAlternatePaths);

                if (audioEngineIsUnity)
                {
                    EditorGUILayout.PropertyField(mApplyHRTFToPathing);
                    EditorGUILayout.PropertyField(mPathingMixLevel);
                }
            }

            serializedObject.ApplyModifiedProperties();
        }

        void DrawDirectivity(float dipoleWeight, float dipolePower)
        {
            if (mDirectivityPreview == null)
            {
                mDirectivityPreview = new Texture2D(65, 65);
            }

            if (mDirectivitySamples == null)
            {
                mDirectivitySamples = new float[360];
                mDirectivityPositions = new Vector2[360];
            }

            for (var i = 0; i < mDirectivitySamples.Length; ++i)
            {
                var theta = (i / 360.0f) * (2.0f * Mathf.PI);
                mDirectivitySamples[i] = Mathf.Pow(Mathf.Abs((1.0f - dipoleWeight) + dipoleWeight * Mathf.Cos(theta)), dipolePower);

                var r = 31 * Mathf.Abs(mDirectivitySamples[i]);
                var x = r * Mathf.Cos(theta) + 32;
                var y = r * Mathf.Sin(theta) + 32;
                mDirectivityPositions[i] = new Vector2(-y, x);
            }

            for (var v = 0; v < mDirectivityPreview.height; ++v)
            {
                for (var u = 0; u < mDirectivityPreview.width; ++u)
                {
                    mDirectivityPreview.SetPixel(u, v, Color.gray);
                }
            }

            for (var u = 0; u < mDirectivityPreview.width; ++u)
            {
                mDirectivityPreview.SetPixel(u, 32, Color.black);
            }

            for (var v = 0; v < mDirectivityPreview.height; ++v)
            {
                mDirectivityPreview.SetPixel(32, v, Color.black);
            }

            for (var i = 0; i < mDirectivitySamples.Length; ++i)
            {
                var color = (mDirectivitySamples[i] > 0.0f) ? Color.red : Color.blue;
                mDirectivityPreview.SetPixel((int) mDirectivityPositions[i].x, (int) mDirectivityPositions[i].y, color);
            }

            mDirectivityPreview.Apply();

            EditorGUILayout.PrefixLabel("Preview");
            EditorGUILayout.Space();
            var rect = EditorGUI.IndentedRect(EditorGUILayout.GetControlRect());
            var center = rect.center;
            center.x += 4;
            rect.center = center;
            rect.width = 65;
            rect.height = 65;

            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.Space();

            EditorGUI.DrawPreviewTexture(rect, mDirectivityPreview);
        }
    }
}
