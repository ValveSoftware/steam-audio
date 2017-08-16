//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{

    //
    // SteamAudioMaterialInspector
    // Custom inspector for SteamAudioMaterial components.
    //

    [CustomEditor(typeof(SteamAudioMaterial))]
    [CanEditMultipleObjects]
    public class SteamAudioMaterialInspector : Editor
    {
        //
        // Draws the inspector.
        //
        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Material Preset", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(serializedObject.FindProperty("Preset"));

            if (serializedObject.FindProperty("Preset").enumValueIndex < 11)
            {
                MaterialValue actualValue = ((SteamAudioMaterial)target).Value;
                actualValue.CopyFrom(MaterialPresetList.PresetValue(serializedObject.FindProperty("Preset").enumValueIndex));
            }
            else
            {
                EditorGUILayout.LabelField("Custom Material", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(serializedObject.FindProperty("Value"));
            }

            EditorGUILayout.Space();

            // Save changes.
            serializedObject.ApplyModifiedProperties();
        }
    }
}