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
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var targetObject = serializedObject.targetObject as SteamAudioDynamicObject;
            var hasAssetFileName = (targetObject.assetFileName != null && targetObject.assetFileName.Length > 0);

            EditorGUILayout.Space();

            if (hasAssetFileName) {
                EditorGUILayout.LabelField("Asset File Name", targetObject.assetFileName);
                if (GUILayout.Button("Export Dynamic Object")) {
                    targetObject.Export(targetObject.assetFileName, false);
                }
                if (GUILayout.Button("Export Dynamic Object To New Asset")) {
                    ExportToNewAsset(serializedObject);
                }
                if (GUILayout.Button("Export Dynamic Object as OBJ")) {
                    targetObject.Export(targetObject.assetFileName, true);
                }
            } else {
                EditorGUILayout.HelpBox(
                    "This Dynamic Object has not been exported to an asset yet. Please click Export Dynamic Object " +
                    "to do so.", MessageType.Warning);
                if (GUILayout.Button("Export Dynamic Object")) {
                    ExportToNewAsset(serializedObject);
                }
            }

            EditorGUILayout.Space();

            serializedObject.ApplyModifiedProperties();
        }

        void ExportToNewAsset(SerializedObject serializedObject)
        {
            var targetObject = serializedObject.targetObject as SteamAudioDynamicObject;
            var assetFileName = EditorUtility.SaveFilePanelInProject("Export Steam Audio Dynamic Object",
                EditorSceneManager.GetActiveScene().name + "_" + targetObject.name, "instancedmesh",
                "Select a file to export this Steam Audio Dynamic Object's data to.",
                Application.streamingAssetsPath);
            if (assetFileName != null && assetFileName.Length > 0) {
                string assetsPath = Application.dataPath;
                string projectPath = Application.dataPath.Split(new string[] { "/Assets" }, System.StringSplitOptions.None)[0];
                string fullPath = projectPath + "/" + assetFileName;
                string relativePath = fullPath.Split(new string[] { Application.streamingAssetsPath + "/" }, System.StringSplitOptions.None)[1];

                assetFileName = relativePath;

                targetObject.Export(assetFileName, false);
                serializedObject.FindProperty("assetFileName").stringValue = assetFileName;
            }
        }
    }
}
