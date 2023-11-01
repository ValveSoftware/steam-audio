//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SerializedData))]
    public class SerializedDataInspector : SteamAudioEditor
    {
        SerializedProperty mData;

        private void OnEnable()
        {
            mData = serializedObject.FindProperty("data");
        }

        protected override void OnSteamAudioGUI()
        {
            serializedObject.Update();

            var size = mData.arraySize;
            EditorGUILayout.LabelField("Serialized Data", Common.HumanReadableDataSize(size));

            serializedObject.ApplyModifiedProperties();
        }
    }
}
