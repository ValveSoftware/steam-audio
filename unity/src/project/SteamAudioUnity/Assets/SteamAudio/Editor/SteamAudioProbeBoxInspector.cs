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
    // SteamAudioProbeBoxInspector
    // Custom inspector for SteamAudioProbeBox.
    //

    [CustomEditor(typeof(SteamAudioProbeBox))]
    public class SteamAudioProbeBoxInspector : Editor
    {
        //
        // Draws the inspector GUI.
        //
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            string[] placementStrategyString = { "Centroid", "Uniform Floor" };
            var placementStrategyProperty = serializedObject.FindProperty("placementStrategy");
            int enumValueIndex = (placementStrategyProperty.enumValueIndex > 0) ? 1 : 0;
            enumValueIndex = EditorGUILayout.Popup("Placement Strategy", enumValueIndex, placementStrategyString);
            placementStrategyProperty.enumValueIndex = (enumValueIndex > 0) ? 2 : 0;

            if (serializedObject.FindProperty("placementStrategy").intValue == (int) ProbePlacementStrategy.Octree)
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("maxNumTriangles"));
                EditorGUILayout.PropertyField(serializedObject.FindProperty("maxOctreeDepth"));
            }
            else if (serializedObject.FindProperty("placementStrategy").intValue == (int)ProbePlacementStrategy.UniformFloor)
            {
                EditorGUILayout.PropertyField(serializedObject.FindProperty("horizontalSpacing"));
                EditorGUILayout.PropertyField(serializedObject.FindProperty("heightAboveFloor"));
            }

            SteamAudioProbeBox probeBox = serializedObject.targetObject as SteamAudioProbeBox;
            EditorGUILayout.BeginHorizontal();
            EditorGUILayout.PrefixLabel(" ");
            if (GUILayout.Button("Generate Probes"))
            {
                probeBox.GenerateProbes();
                EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            }
            EditorGUILayout.EndHorizontal();

            if (probeBox.probeSpherePoints != null && probeBox.probeSpherePoints.Length != 0)
            {
                EditorGUILayout.LabelField("Probe Box Statistics", EditorStyles.boldLabel);
                EditorGUILayout.LabelField("Probe Points", (probeBox.probeSpherePoints.Length / 3).ToString());
                EditorGUILayout.LabelField("Probe Data Size", (probeBox.dataSize / 1000.0f).ToString("0.0") + " KB");
            }

            for (int i = 0; i < probeBox.dataLayerInfo.Count; ++i)
            {
                if (i == 0)
                    EditorGUILayout.LabelField("Detailed Statistics", EditorStyles.boldLabel);

                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(probeBox.dataLayerInfo[i].name, (probeBox.dataLayerInfo[i].size / 1000.0f).ToString("0.0") + " KB");
                if (GUILayout.Button("Clear"))
                {
                    probeBox.DeleteBakedDataByIdentifier(probeBox.dataLayerInfo[i].identifier);
                    EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
                }
                EditorGUILayout.EndHorizontal();
            }

            EditorGUILayout.Space();
            serializedObject.ApplyModifiedProperties();
        }
    }
}