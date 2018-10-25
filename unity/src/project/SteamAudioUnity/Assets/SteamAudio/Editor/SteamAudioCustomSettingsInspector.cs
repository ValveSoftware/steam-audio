//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEditor;
using UnityEngine;

namespace SteamAudio
{

    //
    // SteamAudioCustomSettingsInspector
    // Custom inspector for custom phonon settings component.
    //

    [CustomEditor(typeof(SteamAudioCustomSettings))]
    public class SteamAudioCustomSettingsInspector : Editor
    {

        //
        // Draws the inspector.
        //
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var rayTracerProperty = serializedObject.FindProperty("rayTracerOption");
            var convolutionProperty = serializedObject.FindProperty("convolutionOption");
            var minCuProperty = serializedObject.FindProperty("minComputeUnitsToReserve");
            var maxCuProperty = serializedObject.FindProperty("maxComputeUnitsToReserve");
            var durationProperty = serializedObject.FindProperty("Duration");
            var ambisonicsOrderProperty = serializedObject.FindProperty("AmbisonicsOrder");
            var maxSourcesProperty = serializedObject.FindProperty("MaxSources");
            var bakingBatchSizeProperty = serializedObject.FindProperty("BakingBatchSize");

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Simulation Settings", EditorStyles.boldLabel);
            rayTracerProperty.enumValueIndex = EditorGUILayout.Popup("Ray Tracer",
                rayTracerProperty.enumValueIndex, optionsRayTracer);

            if ((SceneType) rayTracerProperty.enumValueIndex == SceneType.Embree) {
                EditorGUILayout.HelpBox(
                    "Embree is supported on Windows (64-bit), Linux (64-bit), and macOS (64-bit). On all other " +
                    "platforms, Steam Audio will revert to Phonon ray tracing.",
                    MessageType.Info);
            } else if ((SceneType) rayTracerProperty.enumValueIndex == SceneType.RadeonRays) {
                EditorGUILayout.HelpBox(
                    "Radeon Rays is supported on Windows (64-bit). On all other platforms, Steam Audio will revert " +
                    "to Phonon ray tracing.",
                    MessageType.Info);

                EditorGUILayout.PropertyField(bakingBatchSizeProperty);

                EditorGUILayout.HelpBox(
                    "Radeon Rays is currently intended to be used for baking only. Using Radeon Rays with Steam " +
                    "Audio Sources that are configured for real-time indirect sound, or with a Steam Audio " +
                    "Reverb effect configured for real-time reverb, may lead to decreased frame rates.",
                    MessageType.Warning);
            } else if ((SceneType) rayTracerProperty.enumValueIndex == SceneType.Custom) {
                EditorGUILayout.HelpBox(
                    "Unity's built-in ray tracer should only be used for occlusion and transmission. Steam Audio " +
                    "Sources with real-time or baked indirect sound, Steam Audio Mixer Return effects, or Steam " +
                    "Audio Reverb effects should not be used with Unity's built-in ray tracer.",
                    MessageType.Warning);
            }

            EditorGUILayout.Space();
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Rendering Settings", EditorStyles.boldLabel);
            convolutionProperty.enumValueIndex = EditorGUILayout.Popup("Convolution Option", 
                convolutionProperty.enumValueIndex, optionsConvolution);

            if ((ConvolutionOption) convolutionProperty.enumValueIndex == ConvolutionOption.TrueAudioNext)
            {
                EditorGUILayout.HelpBox(
                    "TrueAudio Next is supported on Windows (64-bit). On all other platforms, Steam Audio will " +
                    "revert to Phonon convolution.",
                    MessageType.Info);

                EditorGUILayout.PropertyField(minCuProperty);
                EditorGUILayout.PropertyField(maxCuProperty);

                maxCuProperty.intValue = Mathf.Max(minCuProperty.intValue, maxCuProperty.intValue);

                if (minCuProperty.intValue == 0 && maxCuProperty.intValue == 0)
                {
                    EditorGUILayout.HelpBox("Setting both the minimum and maximum number of requested CUs to zero " +
                        "disables CU reservation; the entire GPU will be used for audio processing. To enable CU " +
                        "reservation, increase the value of one of the above sliders.", MessageType.Warning);
                }

                EditorGUILayout.HelpBox("All scenes in the application that use TrueAudio Next should use the same " +
                    "settings for Min Compute Units To Reserve and Max Compute Units To Reserve.", MessageType.Info);

                EditorGUILayout.PropertyField(durationProperty);
                EditorGUILayout.PropertyField(ambisonicsOrderProperty);
                EditorGUILayout.PropertyField(maxSourcesProperty);
            }

            EditorGUILayout.Space();
            EditorGUILayout.HelpBox("The Steam Audio Custom Settings component should be placed beneath " +
            "the Steam Audio Manager component in the Inspector Window.", MessageType.Info);
            serializedObject.ApplyModifiedProperties();
        }

        string[] optionsRayTracer = new string[] { "Phonon", "Embree", "Radeon Rays", "Unity" };
        string[] optionsConvolution = new string[] { "Phonon", "TrueAudio Next" };
    }
}