//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEngine;

namespace SteamAudio
{

    //
    // SteamAudioGeometryInspector
    // Custom inspector for the AcousticGeometry class.
    //

    [CustomEditor(typeof(SteamAudioGeometry))]
    public class SteamAudioGeometryInspector : Editor
    {
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            SteamAudioGeometry geometry = serializedObject.targetObject as SteamAudioGeometry;

            EditorGUILayout.Space();
            bool toggleValue = serializedObject.FindProperty("exportAllChildren").boolValue;
            if (geometry.transform.childCount != 0)
                serializedObject.FindProperty("exportAllChildren").boolValue = GUILayout.Toggle(toggleValue, " Export All Children");

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Geometry Statistics", EditorStyles.boldLabel);
            EditorGUILayout.LabelField("Vertices", geometry.GetNumVertices().ToString());
            EditorGUILayout.LabelField("Triangles", geometry.GetNumTriangles().ToString());

            if (geometry.gameObject.GetComponent<Terrain>() != null)
            {
                EditorGUILayout.LabelField("Terrain Export Settings", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("TerrainSimplificationLevel"));
            }

            EditorGUILayout.Space();

            serializedObject.ApplyModifiedProperties();
        }
    }
}