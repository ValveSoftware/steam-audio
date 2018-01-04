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

        string[] optionsOcclusion = new string[] { "Off", "On, No Transmission", "On, Frequency Independent Transmission", "On, Frequency Dependent Transmission" };

        bool showAdvancedOptions = false;
    }
}