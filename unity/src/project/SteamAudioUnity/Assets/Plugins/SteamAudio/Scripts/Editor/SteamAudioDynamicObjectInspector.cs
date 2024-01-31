//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
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
 #if STEAMAUDIO_ENABLED
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
#else
        public override void OnInspectorGUI()
        {
            EditorGUILayout.HelpBox("Steam Audio is not supported for the target platform or STEAMAUDIO_ENABLED define symbol is missing.", MessageType.Warning);
        }
#endif
    }
}
