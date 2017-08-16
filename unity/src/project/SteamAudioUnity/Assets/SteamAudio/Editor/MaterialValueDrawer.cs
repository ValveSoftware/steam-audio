//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEngine;

namespace SteamAudio
{

    //
    // MaterialValueDrawer
    // Custom property drawer for MaterialValue.
    //

    [CustomPropertyDrawer(typeof(MaterialValue))]
    public class MaterialValueDrawer : PropertyDrawer
    {
        //
        // Returns the total height of the field.
        //
        public override float GetPropertyHeight(SerializedProperty property, GUIContent label)
        {
            return 112f;
        }

        //
        // Draws the field.
        //
        public override void OnGUI(Rect position, SerializedProperty property, GUIContent label)
        {
            position.height = 16f;

            if (position.x <= 0)
            {
                position.x += 4f;
                position.width -= 8f;
            }

            EditorGUI.PropertyField(position, property.FindPropertyRelative("LowFreqAbsorption"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("MidFreqAbsorption"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("HighFreqAbsorption"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("Scattering"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("LowFreqTransmission"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("MidFreqTransmission"));
            position.y += 16f;
            EditorGUI.PropertyField(position, property.FindPropertyRelative("HighFreqTransmission"));
        }
    }
}