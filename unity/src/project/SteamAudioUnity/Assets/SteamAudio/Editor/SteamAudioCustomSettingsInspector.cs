//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

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

            EditorGUILayout.LabelField("Simulation Settings", EditorStyles.boldLabel);
            serializedObject.FindProperty("rayTracerOption").enumValueIndex = EditorGUILayout.Popup("Raytracer Options", serializedObject.FindProperty("rayTracerOption").enumValueIndex, optionsRayTracer);

            EditorGUILayout.LabelField("Renderer Settings", EditorStyles.boldLabel);
            serializedObject.FindProperty("convolutionOption").enumValueIndex = EditorGUILayout.Popup("Convolution Options", serializedObject.FindProperty("convolutionOption").enumValueIndex, optionsConvolution);

            SteamAudioCustomSettings customSettings = serializedObject.targetObject as SteamAudioCustomSettings;
            if (customSettings.convolutionOption == ConvolutionOption.TrueAudioNext)
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("numComputeUnits"));
            }

            EditorGUILayout.HelpBox("This is an experimental feature. Please contact the developers to get relevant documentation to use Custom Phonon Settings feature.", MessageType.Info);
            serializedObject.ApplyModifiedProperties();
        }

        string[] optionsRayTracer = new string[] { "Phonon", "Embree", "Radeon Rays", "Custom" };
        string[] optionsConvolution = new string[] { "Phonon", "TrueAudio Next" };
    }
}