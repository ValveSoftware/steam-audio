//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
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
