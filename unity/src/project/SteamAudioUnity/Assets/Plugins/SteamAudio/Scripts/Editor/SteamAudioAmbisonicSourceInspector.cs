//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioAmbisonicSource))]
    [CanEditMultipleObjects]
    public class SteamAudioAmbisonicSourceInspector : Editor
    {
        SerializedProperty mApplyHRTF;

        private void OnEnable()
        {
            mApplyHRTF = serializedObject.FindProperty("applyHRTF");
        }

        public override void OnInspectorGUI()
        {
            if (SteamAudioSettings.Singleton.audioEngine != AudioEngineType.Unity)
            {
                EditorGUILayout.HelpBox(
                    "This component requires the audio engine to be set to Unity. Click" +
                    "Steam Audio > Settings to change this.", MessageType.Warning);

                return;
            }

            serializedObject.Update();

            EditorGUILayout.PropertyField(mApplyHRTF);

            serializedObject.ApplyModifiedProperties();
        }
    }
}
