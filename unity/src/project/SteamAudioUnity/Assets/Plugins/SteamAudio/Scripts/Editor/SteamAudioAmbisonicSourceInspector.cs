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

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioAmbisonicSource))]
    [CanEditMultipleObjects]
    public class SteamAudioAmbisonicSourceInspector : Editor
    {
        SerializedProperty mApplyHRTF;

        private void OnEnable()
        {
            mApplyHRTF = serializedObject.FindProperty("applyHRTF");
        }

        public override void OnInspectorGUI()
        {
            if (SteamAudioSettings.Singleton.audioEngine != AudioEngineType.Unity)
            {
                EditorGUILayout.HelpBox(
                    "This component requires the audio engine to be set to Unity. Click" +
                    "Steam Audio > Settings to change this.", MessageType.Warning);

                return;
            }

            serializedObject.Update();

            EditorGUILayout.PropertyField(mApplyHRTF);

            serializedObject.ApplyModifiedProperties();
        }
    }
}
