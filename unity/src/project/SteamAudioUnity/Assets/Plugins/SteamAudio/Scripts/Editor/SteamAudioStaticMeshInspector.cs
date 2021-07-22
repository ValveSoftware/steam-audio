//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioStaticMesh))]
    public class SteamAudioStaticMeshInspector : Editor
    {
        SerializedProperty mAsset;
        SerializedProperty mSceneNameWhenExported;

        void OnEnable()
        {
            mAsset = serializedObject.FindProperty("asset");
            mSceneNameWhenExported = serializedObject.FindProperty("sceneNameWhenExported");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(mAsset);

            var scene = (target as SteamAudioStaticMesh).gameObject.scene;

            if (mAsset.objectReferenceValue == null)
            {
                EditorGUILayout.HelpBox(
                    "This scene has not been exported. Click Steam Audio > Export Active Scene to export.",
                    MessageType.Warning);
            }
            else if (mSceneNameWhenExported.stringValue != scene.name)
            {
                EditorGUILayout.HelpBox(
                    string.Format("This geometry was last exported for the scene {0}. If this is not what you " +
                        "intended, click Export As New Asset below.", mSceneNameWhenExported.stringValue),
                    MessageType.Warning);

                if (GUILayout.Button("Export As New Asset"))
                {
                    mAsset.objectReferenceValue = SerializedData.PromptForNewAsset(scene.name);
                    mSceneNameWhenExported.stringValue = scene.name;
                    serializedObject.ApplyModifiedProperties();

                    SteamAudioManager.ExportScene(scene, false);
                }
            }

            serializedObject.ApplyModifiedProperties();
        }
    }
}
