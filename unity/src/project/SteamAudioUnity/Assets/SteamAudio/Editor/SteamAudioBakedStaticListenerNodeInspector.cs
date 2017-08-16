//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEngine;

namespace SteamAudio
{
    //
    // SteamAudioBakedStaticListenerNodeInspector
    // Custom inspector for BakedStaticListener.
    //

    [CustomEditor(typeof(SteamAudioBakedStaticListenerNode))]
    public class SteamAudioBakedStaticListenerNodeInspector : Editor
    {
        //
        // Draws the inspector GUI.
        //
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Baked Static Listener Settings", EditorStyles.boldLabel);

            SteamAudioBakedStaticListenerNode bakedStaticListener = 
                serializedObject.targetObject as SteamAudioBakedStaticListenerNode;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            EditorGUILayout.PropertyField(serializedObject.FindProperty("uniqueIdentifier"));
            bakedStaticListener.uniqueIdentifier = bakedStaticListener.uniqueIdentifier.Trim();
            EditorGUILayout.PropertyField(serializedObject.FindProperty("bakingRadius"));
            EditorGUILayout.PropertyField(serializedObject.FindProperty("useAllProbeBoxes"));

            if (bakedStaticListener.uniqueIdentifier.Length == 0)
                EditorGUILayout.HelpBox("You must specify a unique identifier name.", MessageType.Warning);

            if (!serializedObject.FindProperty("useAllProbeBoxes").boolValue)
                EditorGUILayout.PropertyField(serializedObject.FindProperty("probeBoxes"), true);

            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            if (GUILayout.Button("Bake Effect"))
            {
                if (bakedStaticListener.uniqueIdentifier.Length == 0)
                    Debug.LogError("You must specify a unique identifier name.");
                else
                {
                    bakedStaticListener.BeginBake();
                }
            }
            EditorGUILayout.EndHorizontal();
            GUI.enabled = true;

            DisplayProgressBarAndCancel();

            serializedObject.FindProperty("bakedStatsFoldout").boolValue =
                EditorGUILayout.Foldout(serializedObject.FindProperty("bakedStatsFoldout").boolValue,
                "Baked Static Listener Node Statistics");
            if (bakedStaticListener.bakedStatsFoldout)
                BakedStaticListenerNodeStatsGUI();
            serializedObject.ApplyModifiedProperties();
        }

        void DisplayProgressBarAndCancel()
        {
            SteamAudioBakedStaticListenerNode bakedStaticListener = 
                serializedObject.targetObject as SteamAudioBakedStaticListenerNode;
            bakedStaticListener.phononBaker.DrawProgressBar();
            Repaint();
        }

        public void BakedStaticListenerNodeStatsGUI()
        {
            SteamAudioBakedStaticListenerNode bakedNode = 
                serializedObject.targetObject as SteamAudioBakedStaticListenerNode;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;
            bakedNode.UpdateBakedDataStatistics();
            for (int i = 0; i < bakedNode.bakedProbeNames.Count; ++i)
                EditorGUILayout.LabelField(bakedNode.bakedProbeNames[i], 
                    (bakedNode.bakedProbeDataSizes[i] / 1000.0f).ToString("0.0") + " KB");
            EditorGUILayout.LabelField("Total Size", 
                (bakedNode.bakedDataSize / 1000.0f).ToString("0.0") + " KB");
            GUI.enabled = true;
        }
    }
}