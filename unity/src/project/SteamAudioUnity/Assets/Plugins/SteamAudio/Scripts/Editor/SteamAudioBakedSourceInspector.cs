//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioBakedSource))]
    public class SteamAudioBakedSourceInspector : Editor
    {
#if STEAMAUDIO_ENABLED
        SerializedProperty mInfluenceRadius;
        SerializedProperty mUseAllProbeBatches;
        SerializedProperty mProbeBatches;

        bool mStatsFoldout = false;
        static bool mShouldShowProgressBar = false;

        private void OnEnable()
        {
            mInfluenceRadius = serializedObject.FindProperty("influenceRadius");
            mUseAllProbeBatches = serializedObject.FindProperty("useAllProbeBatches");
            mProbeBatches = serializedObject.FindProperty("probeBatches");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var oldGUIEnabled = GUI.enabled;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;

            var tgt = target as SteamAudioBakedSource;

            EditorGUILayout.PropertyField(mInfluenceRadius);
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

        [MenuItem("Steam Audio/Steam Audio Baked Source/Bake All Static Source Reflections In Current Scene", false, 65)]
        public static void BakeAllReflectionsInScene()
        {
            var bakedSources = FindObjectsByType<SteamAudioBakedSource>(FindObjectsSortMode.None);
            if (bakedSources.Length == 0)
            {
                EditorUtility.DisplayDialog(
                    "No Steam Audio Baked Sources Found",
                    "No Steam Audio Baked Source components were found in the currently-open scene.",
                    "OK");
                return;
            }

            SteamAudioBakedSource.BeginBake(bakedSources);
            mShouldShowProgressBar = true;
        }
#else
        public override void OnInspectorGUI()
        {
            EditorGUILayout.HelpBox("Steam Audio is not supported for the target platform or STEAMAUDIO_ENABLED define symbol is missing.", MessageType.Warning);
        }
#endif
    }
}
