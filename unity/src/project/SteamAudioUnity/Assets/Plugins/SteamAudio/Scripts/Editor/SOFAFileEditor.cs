//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{
    /*
     * Custom editor GUI for SOFAFile assets.
     */
    [CustomEditor(typeof(SOFAFile))]
    public class SOFAFileEditor : SteamAudioEditor
    {
        protected override void OnSteamAudioGUI()
        {
            EditorGUILayout.PropertyField(serializedObject.FindProperty("sofaName"));
            EditorGUILayout.LabelField("Size", Common.HumanReadableDataSize(serializedObject.FindProperty("data").arraySize));
        }
    }
}
