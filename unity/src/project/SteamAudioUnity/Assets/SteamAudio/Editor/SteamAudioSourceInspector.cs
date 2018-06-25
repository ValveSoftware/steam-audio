//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEngine;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

namespace SteamAudio
{

    //
    // SteamAudioSourceInspector
    // Custom inspector for SteamAudioSource components.
    //

    [CustomEditor(typeof(SteamAudioSource))]
    [CanEditMultipleObjects]
    public class SteamAudioSourceInspector : Editor
    {
        //
        // Draws the inspector.
        //
        public override void OnInspectorGUI()
        {
            var steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
            if (steamAudioManager == null)
            {
                EditorGUILayout.HelpBox("A Steam Audio Manager does not exist in the scene. Click Window > Steam" +
                    " Audio.", MessageType.Error);
                return;
            }

            serializedObject.Update();

            if (steamAudioManager.audioEngine == AudioEngine.UnityNative)
            {
                // Direct Sound UX
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Direct Sound", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("directBinaural"));
                if (serializedObject.FindProperty("directBinaural").boolValue)
                {
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("interpolation"),
                        new GUIContent("HRTF Interpolation"));
                }

                serializedObject.FindProperty("occlusionMode").enumValueIndex =
                    EditorGUILayout.Popup("Direct Sound Occlusion",
                    serializedObject.FindProperty("occlusionMode").enumValueIndex, optionsOcclusion);

                if (serializedObject.FindProperty("occlusionMode").enumValueIndex
                    != (int)OcclusionMode.NoOcclusion)
                {
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("occlusionMethod"));

                    if (serializedObject.FindProperty("occlusionMethod").enumValueIndex ==
                        (int)OcclusionMethod.Partial)
                    {
                        EditorGUILayout.PropertyField(serializedObject.FindProperty("sourceRadius"),
                            new GUIContent("Source Radius (meters)"));
                    }
                }

                EditorGUILayout.PropertyField(serializedObject.FindProperty("physicsBasedAttenuation"));
                EditorGUILayout.PropertyField(serializedObject.FindProperty("airAbsorption"));
                EditorGUILayout.PropertyField(serializedObject.FindProperty("directMixLevel"));
                EditorGUILayout.Space();

                EditorGUILayout.LabelField("Directivity", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("dipoleWeight"));
                EditorGUILayout.PropertyField(serializedObject.FindProperty("dipolePower"));
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

                DrawDirectivity(serializedObject.FindProperty("dipoleWeight").floatValue, serializedObject.FindProperty("dipolePower").floatValue);
                EditorGUI.DrawPreviewTexture(rect, directivityPreview);

                // Indirect Sound UX
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Indirect Sound", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("reflections"));

                if (serializedObject.FindProperty("reflections").boolValue)
                {
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("simulationType"));
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("indirectMixLevel"));
                    EditorGUILayout.PropertyField(serializedObject.FindProperty("indirectBinaural"));

                    EditorGUILayout.HelpBox("Go to Window > Phonon > Simulation to update the global simulation " +
                        "settings.", MessageType.Info);

                    if (serializedObject.FindProperty("indirectBinaural").boolValue)
                    {
                        EditorGUILayout.HelpBox("The binaural setting is ignored if Phonon Listener component is " +
                            "attached with mixing enabled.", MessageType.Info);
                    }
                }
            }

            SteamAudioSource phononEffect = serializedObject.targetObject as SteamAudioSource;
            if (phononEffect.simulationType == SourceSimulationType.BakedStaticSource ||
                steamAudioManager.audioEngine != AudioEngine.UnityNative)
            {
                BakedSourceGUI();
                serializedObject.FindProperty("bakedStatsFoldout").boolValue =
                    EditorGUILayout.Foldout(serializedObject.FindProperty("bakedStatsFoldout").boolValue,
                    "Baked Static Source Statistics");

                if (phononEffect.bakedStatsFoldout)
                    BakedSourceStatsGUI();
            }

            EditorGUILayout.Space();
            if (showAdvancedOptions = EditorGUILayout.Foldout(showAdvancedOptions, "Advanced Options"))
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("avoidSilenceDuringInit"));
            }

            // Save changes.
            serializedObject.ApplyModifiedProperties();
        }

        public void BakedSourceGUI()
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Baked Static Source Settings", EditorStyles.boldLabel);

            SteamAudioSource bakedSource = serializedObject.targetObject as SteamAudioSource;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;

            EditorGUILayout.PropertyField(serializedObject.FindProperty("uniqueIdentifier"));
            bakedSource.uniqueIdentifier = bakedSource.uniqueIdentifier.Trim();
            EditorGUILayout.PropertyField(serializedObject.FindProperty("bakingRadius"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("useAllProbeBoxes"));

            if (!serializedObject.FindProperty("useAllProbeBoxes").boolValue)
                EditorGUILayout.PropertyField(serializedObject.FindProperty("probeBoxes"), true);

            if (bakedSource.uniqueIdentifier.Length == 0)
                EditorGUILayout.HelpBox("You must specify a unique identifier name.", MessageType.Warning);

            if (bakedSource.dipoleWeight > 0.0f)
            {
                EditorGUILayout.HelpBox("A baked static source should not rotate at run-time, " +
                    "otherwise indirect sound rendered using baked data may not be accurate.", MessageType.Warning);
            }

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            if (GUILayout.Button("Bake Effect"))
            {
                if (bakedSource.uniqueIdentifier.Length == 0)
                    Debug.LogError("You must specify a unique identifier name.");
                else
                {
                    bakedSource.BeginBake();
                }
            }
            EditorGUILayout.EndHorizontal();
            GUI.enabled = true;

            DisplayProgressBarAndCancel();
        }

        public void BakedSourceStatsGUI()
        {
            SteamAudioSource bakedSource = serializedObject.targetObject as SteamAudioSource;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            bakedSource.UpdateBakedDataStatistics();
            for (int i = 0; i < bakedSource.bakedProbeNames.Count; ++i)
                EditorGUILayout.LabelField(bakedSource.bakedProbeNames[i], (bakedSource.bakedProbeDataSizes[i] / 1000.0f).ToString("0.0") + " KB");
            EditorGUILayout.LabelField("Total Size", (bakedSource.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
            GUI.enabled = true;
        }

        void DisplayProgressBarAndCancel()
        {
            SteamAudioSource bakedSource = serializedObject.targetObject as SteamAudioSource;
            bakedSource.baker.DrawProgressBar();
            Repaint();
        }

        void DrawDirectivity(float dipoleWeight, float dipolePower)
        {
            if (directivityPreview == null)
            {
                directivityPreview = new Texture2D(65, 65);
            }

            if (directivitySamples == null)
            {
                directivitySamples = new float[360];
                directivityPositions = new Vector2[360];
            }

            for (var i = 0; i < directivitySamples.Length; ++i)
            {
                var theta = (i / 360.0f) * (2.0f * Mathf.PI);
                directivitySamples[i] = Mathf.Pow(Mathf.Abs((1.0f - dipoleWeight) + dipoleWeight * Mathf.Cos(theta)), dipolePower);

                var r = 31 * Mathf.Abs(directivitySamples[i]);
                var x = r * Mathf.Cos(theta) + 32;
                var y = r * Mathf.Sin(theta) + 32;
                directivityPositions[i] = new Vector2(-y, x);
            }

            for (var v = 0; v < directivityPreview.height; ++v)
            {
                for (var u = 0; u < directivityPreview.width; ++u)
                {
                    directivityPreview.SetPixel(u, v, Color.gray);
                }
            }

            for (var u = 0; u < directivityPreview.width; ++u)
            {
                directivityPreview.SetPixel(u, 32, Color.black);
            }

            for (var v = 0; v < directivityPreview.height; ++v)
            {
                directivityPreview.SetPixel(32, v, Color.black);
            }

            for (var i = 0; i < directivitySamples.Length; ++i)
            {
                var color = (directivitySamples[i] > 0.0f) ? Color.red : Color.blue;
                directivityPreview.SetPixel((int) directivityPositions[i].x, (int) directivityPositions[i].y, color);
            }

            directivityPreview.Apply();
        }

        string[] optionsOcclusion = new string[] { "Off", "On, No Transmission", "On, Frequency Independent Transmission", "On, Frequency Dependent Transmission" };

        bool showAdvancedOptions = false;

        Texture2D directivityPreview = null;
        float[] directivitySamples = null;
        Vector2[] directivityPositions = null;
    }
}