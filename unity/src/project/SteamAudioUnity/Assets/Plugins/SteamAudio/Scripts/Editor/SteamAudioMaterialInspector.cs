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
    [CustomEditor(typeof(SteamAudioMaterial))]
    [CanEditMultipleObjects]
    public class SteamAudioMaterialInspector : Editor
    {
        SerializedProperty lowFreqAbsorption;
        SerializedProperty midFreqAbsorption;
        SerializedProperty highFreqAbsorption;
        SerializedProperty scattering;
        SerializedProperty lowFreqTransmission;
        SerializedProperty midFreqTransmission;
        SerializedProperty highFreqTransmission;

        private void OnEnable()
        {
            lowFreqAbsorption = serializedObject.FindProperty("lowFreqAbsorption");
            midFreqAbsorption = serializedObject.FindProperty("midFreqAbsorption");
            highFreqAbsorption = serializedObject.FindProperty("highFreqAbsorption");
            scattering = serializedObject.FindProperty("scattering");
            lowFreqTransmission = serializedObject.FindProperty("lowFreqTransmission");
            midFreqTransmission = serializedObject.FindProperty("midFreqTransmission");
            highFreqTransmission = serializedObject.FindProperty("highFreqTransmission");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(lowFreqAbsorption);
            EditorGUILayout.PropertyField(midFreqAbsorption);
            EditorGUILayout.PropertyField(highFreqAbsorption);
            EditorGUILayout.PropertyField(scattering);
            EditorGUILayout.PropertyField(lowFreqTransmission);
            EditorGUILayout.PropertyField(midFreqTransmission);
            EditorGUILayout.PropertyField(highFreqTransmission);

            serializedObject.ApplyModifiedProperties();
        }
    }
}
