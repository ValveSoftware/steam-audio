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
    [CustomEditor(typeof(SteamAudioManager))]
    [CanEditMultipleObjects]
    public class SteamAudioManagerInspector : Editor
    {
#if STEAMAUDIO_ENABLED
        SerializedProperty mCurrentHRTF;

        private void OnEnable()
        {
            mCurrentHRTF = serializedObject.FindProperty("currentHRTF");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var tgt = target as SteamAudioManager;

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("HRTF Settings", EditorStyles.boldLabel);
            mCurrentHRTF.intValue = EditorGUILayout.Popup("Current HRTF", mCurrentHRTF.intValue, tgt.hrtfNames);

            EditorGUILayout.Space();
            EditorGUILayout.HelpBox(
                "This component should not be added manually to any GameObject. It is automatically created and" +
                "destroyed by Steam Audio.", MessageType.Warning);

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
