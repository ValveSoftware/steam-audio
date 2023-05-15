//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
#if UNITY_2020_2_OR_NEWER
using UnityEditor.AssetImporters;
#else
using UnityEditor.Experimental.AssetImporters;
#endif

namespace SteamAudio
{
    /*
     * Custom editor GUI for SOFAFile import settings.
     */
    [CustomEditor(typeof(SOFAFileImporter))]
    public class SOFAFileImporterEditor : ScriptedImporterEditor
    {
        public override void OnInspectorGUI()
        {
            serializedObject.Update();
            EditorGUILayout.PropertyField(serializedObject.FindProperty("volume"));
            serializedObject.ApplyModifiedProperties();
            ApplyRevertGUI();
        }
    }
}
