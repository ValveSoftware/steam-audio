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
    // PhononMixerInspector
    // Custom inspector for the PhononMixer component.
    //
    [CustomEditor(typeof(SteamAudioListener))]
    public class SteamAudioListenerInspector : Editor
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

            var staticListenerNodes = GameObject.FindObjectsOfType<SteamAudioBakedStaticListenerNode>();
            var staticListenerNodesExist = (staticListenerNodes != null && staticListenerNodes.Length > 0);
            if (staticListenerNodesExist)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Static Listener Settings", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("currentStaticListenerNode"));
            }

            BakedReverbGUI();
            serializedObject.FindProperty("bakedStatsFoldout").boolValue =
                EditorGUILayout.Foldout(serializedObject.FindProperty("bakedStatsFoldout").boolValue,
                "Baked Reverb Statistics");
            SteamAudioListener phononListener = serializedObject.targetObject as SteamAudioListener;
            if (phononListener.bakedStatsFoldout)
                BakedReverbStatsGUI();

            serializedObject.ApplyModifiedProperties();
        }

        //
        // GUI for BakedReverb
        //
        public void BakedReverbGUI()
        {
            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Baked Reverb Settings", EditorStyles.boldLabel);
            SteamAudioListener bakedReverb = serializedObject.targetObject as SteamAudioListener;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            EditorGUILayout.PropertyField(serializedObject.FindProperty("useAllProbeBoxes"));
            if (!serializedObject.FindProperty("useAllProbeBoxes").boolValue)
                EditorGUILayout.PropertyField(serializedObject.FindProperty("probeBoxes"), true);

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            if (GUILayout.Button("Bake Reverb"))
            {
                bakedReverb.BeginBake();
            }
            EditorGUILayout.EndHorizontal();
            GUI.enabled = true;

            DisplayProgressBarAndCancel();
        }
        public void BakedReverbStatsGUI()
        {
            SteamAudioListener bakedReverb = serializedObject.targetObject as SteamAudioListener;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            bakedReverb.UpdateBakedDataStatistics();
            for (int i = 0; i < bakedReverb.bakedProbeNames.Count; ++i)
                EditorGUILayout.LabelField(bakedReverb.bakedProbeNames[i], 
                    (bakedReverb.bakedProbeDataSizes[i] / 1000.0f).ToString("0.0") + " KB");
            EditorGUILayout.LabelField("Total Size", 
                (bakedReverb.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
            GUI.enabled = true;
        }

        void DisplayProgressBarAndCancel()
        {
            SteamAudioListener bakedReverb = serializedObject.targetObject as SteamAudioListener;
            bakedReverb.phononBaker.DrawProgressBar();
            Repaint();
        }
    }
}