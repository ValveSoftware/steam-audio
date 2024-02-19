//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
