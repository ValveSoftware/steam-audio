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

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioStaticMesh))]
    public class SteamAudioStaticMeshInspector : Editor
    {
#if STEAMAUDIO_ENABLED
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
#else
        public override void OnInspectorGUI()
        {
            EditorGUILayout.HelpBox("Steam Audio is not supported for the target platform or STEAMAUDIO_ENABLED define symbol is missing.", MessageType.Warning);
        }
#endif
    }
}
