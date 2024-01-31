//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioGeometry))]
    [CanEditMultipleObjects]
    public class SteamAudioGeometryInspector : Editor
    {
#if STEAMAUDIO_ENABLED
        SerializedProperty mMaterial;
        SerializedProperty mExportAllChildren;
        SerializedProperty mTerrainSimplificationLevel;

        private void OnEnable()
        {
            mMaterial = serializedObject.FindProperty("material");
            mExportAllChildren = serializedObject.FindProperty("exportAllChildren");
            mTerrainSimplificationLevel = serializedObject.FindProperty("terrainSimplificationLevel");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var tgt = target as SteamAudioGeometry;

            EditorGUILayout.PropertyField(mMaterial);

            if (tgt.transform.childCount != 0)
            {
                EditorGUILayout.PropertyField(mExportAllChildren);
            }

            if (tgt.gameObject.GetComponent<Terrain>() != null)
            {
                EditorGUILayout.PropertyField(mTerrainSimplificationLevel);
            }

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Geometry Statistics", EditorStyles.boldLabel);
            EditorGUILayout.LabelField("Vertices", tgt.GetNumVertices().ToString());
            EditorGUILayout.LabelField("Triangles", tgt.GetNumTriangles().ToString());

            serializedObject.ApplyModifiedProperties();
        }
#else
        public override void OnInspectorGUI()
        {
            EditorGUILayout.HelpBox("Steam Audio is not supported for the target platform or STEAMAUDIO_ENABLED define symbol is missing.", MessageType.Warning);
        }
#endif
    }
}
