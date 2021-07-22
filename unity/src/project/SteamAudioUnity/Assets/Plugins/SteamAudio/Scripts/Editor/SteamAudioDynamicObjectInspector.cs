//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioDynamicObject))]
    public class SteamAudioDynamicObjectInspector : Editor
    {
        SerializedProperty mAsset;

        private void OnEnable()
        {
            mAsset = serializedObject.FindProperty("asset");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(mAsset);

            if (mAsset.objectReferenceValue == null)
            {
                EditorGUILayout.HelpBox(
                    "This Dynamic Object has not been exported to an asset yet. Please click Export Dynamic Object " +
                    "to do so.", MessageType.Warning);
            }

            EditorGUILayout.Space();

            if (GUILayout.Button("Export Dynamic Object"))
            {
                if (mAsset.objectReferenceValue == null)
                {
                    var name = (target as SteamAudioDynamicObject).gameObject.scene.name + "_" + target.name;
                    mAsset.objectReferenceValue = SerializedData.PromptForNewAsset(name);
                    serializedObject.ApplyModifiedProperties();
                }

                SteamAudioManager.ExportDynamicObject(target as SteamAudioDynamicObject, false);
            }

            if (GUILayout.Button("Export Dynamic Object as OBJ"))
            {
                SteamAudioManager.ExportDynamicObject(target as SteamAudioDynamicObject, true);
            }

            serializedObject.ApplyModifiedProperties();
        }
    }
}
