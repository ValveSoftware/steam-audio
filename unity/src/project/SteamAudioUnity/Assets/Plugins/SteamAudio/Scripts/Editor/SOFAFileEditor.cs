//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{
    /*
     * Custom editor GUI for SOFAFile assets.
     */
    [CustomEditor(typeof(SOFAFile))]
    public class SOFAFileEditor : Editor
    {
        public override void OnInspectorGUI()
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("sofaName"));
            EditorGUILayout.LabelField("Size", Common.HumanReadableDataSize(serializedObject.FindProperty("data").arraySize));
        }
    }
}
