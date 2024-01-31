//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioListener))]
    public class SteamAudioListenerInspector : Editor
    {
#if STEAMAUDIO_ENABLED
        SerializedProperty mCurrentBakedListener;
        SerializedProperty mApplyReverb;
        SerializedProperty mReverbType;
        SerializedProperty mUseAllProbeBatches;
        SerializedProperty mProbeBatches;

        bool mStatsFoldout = false;
        bool mShouldShowProgressBar = false;

        private void OnEnable()
        {
            mCurrentBakedListener = serializedObject.FindProperty("currentBakedListener");
            mApplyReverb = serializedObject.FindProperty("applyReverb");
            mReverbType = serializedObject.FindProperty("reverbType");
            mUseAllProbeBatches = serializedObject.FindProperty("useAllProbeBatches");
            mProbeBatches = serializedObject.FindProperty("probeBatches");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            EditorGUILayout.PropertyField(mCurrentBakedListener);

            EditorGUILayout.PropertyField(mApplyReverb);
            if (mApplyReverb.boolValue)
            {
                EditorGUILayout.PropertyField(mReverbType);
            }

            var oldGUIEnabled = GUI.enabled;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;

            var tgt = target as SteamAudioListener;

            EditorGUILayout.PropertyField(mUseAllProbeBatches);
            if (!mUseAllProbeBatches.boolValue)
            {
                EditorGUILayout.PropertyField(mProbeBatches);
            }

            EditorGUILayout.Space();
            if (GUILayout.Button("Bake"))
            {
                tgt.BeginBake();
                mShouldShowProgressBar = true;
            }

            GUI.enabled = oldGUIEnabled;

            if (mShouldShowProgressBar && !Baker.IsBakeActive())
            {
                mShouldShowProgressBar = false;
            }

            if (mShouldShowProgressBar)
            {
                Baker.DrawProgressBar();
            }

            Repaint();

            EditorGUILayout.Space();
            mStatsFoldout = EditorGUILayout.Foldout(mStatsFoldout, "Baked Data Statistics");
            if (mStatsFoldout && !Baker.IsBakeActive())
            {
                for (var i = 0; i < tgt.GetProbeBatchesUsed().Length; ++i)
                {
                    EditorGUILayout.LabelField(tgt.GetProbeBatchesUsed()[i].gameObject.name, Common.HumanReadableDataSize(tgt.GetProbeDataSizes()[i]));
                }
                EditorGUILayout.LabelField("Total Size", Common.HumanReadableDataSize(tgt.GetTotalDataSize()));
            }

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
