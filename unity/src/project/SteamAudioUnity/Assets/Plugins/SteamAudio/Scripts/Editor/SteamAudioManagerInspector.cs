//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
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
