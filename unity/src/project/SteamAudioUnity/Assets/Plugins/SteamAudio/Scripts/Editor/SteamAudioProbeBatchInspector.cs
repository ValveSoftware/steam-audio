//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEditor;
using UnityEditor.SceneManagement;

namespace SteamAudio
{
    [CustomEditor(typeof(SteamAudioProbeBatch))]
    public class SteamAudioProbeBatchInspector : Editor
    {
#if STEAMAUDIO_ENABLED
        SerializedProperty mPlacementStrategy;
        SerializedProperty mHorizontalSpacing;
        SerializedProperty mHeightAboveFloor;
        SerializedProperty mAsset;

        bool mShouldShowProgressBar = false;

        private void OnEnable()
        {
            mPlacementStrategy = serializedObject.FindProperty("placementStrategy");
            mHorizontalSpacing = serializedObject.FindProperty("horizontalSpacing");
            mHeightAboveFloor = serializedObject.FindProperty("heightAboveFloor");
            mAsset = serializedObject.FindProperty("asset");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            var oldGUIEnabled = GUI.enabled;
            GUI.enabled = !Baker.IsBakeActive() && !EditorApplication.isPlayingOrWillChangePlaymode;

            var tgt = target as SteamAudioProbeBatch;

            EditorGUILayout.PropertyField(mAsset);

            EditorGUILayout.PropertyField(mPlacementStrategy);
            if ((ProbeGenerationType) mPlacementStrategy.enumValueIndex == ProbeGenerationType.UniformFloor)
            {
                EditorGUILayout.PropertyField(mHorizontalSpacing);
                EditorGUILayout.PropertyField(mHeightAboveFloor);
            }

            EditorGUILayout.Space();
            if (GUILayout.Button("Generate Probes"))
            {
                tgt.GenerateProbes();
                EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
            }

            if (tgt.GetNumProbes() > 0)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Baked Pathing Settings", EditorStyles.boldLabel);
                if (GUILayout.Button("Bake"))
                {
                    tgt.BeginBake();
                    mShouldShowProgressBar = true;
                }
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

            if (tgt.GetNumProbes() > 0)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Probe Statistics", EditorStyles.boldLabel);
                EditorGUILayout.LabelField("Probes", tgt.GetNumProbes().ToString());
                EditorGUILayout.LabelField("Data Size", Common.HumanReadableDataSize(tgt.probeDataSize));

                if (tgt.GetNumLayers() > 0)
                {
                    EditorGUILayout.Space();
                    EditorGUILayout.LabelField("Detailed Statistics", EditorStyles.boldLabel);
                    for (var i = 0; i < tgt.GetNumLayers(); ++i)
                    {
                        var layerInfo = tgt.GetInfoForLayer(i);

                        var name = "";
                        if (layerInfo.identifier.type == BakedDataType.Pathing)
                        {
                            name = "Pathing";
                        }
                        else if (layerInfo.identifier.variation == BakedDataVariation.Reverb)
                        {
                            name = "Reverb";
                        }
                        else
                        {
                            name = layerInfo.gameObject.name;
                        }

                        EditorGUILayout.BeginHorizontal();
                        EditorGUILayout.LabelField(name, Common.HumanReadableDataSize(layerInfo.dataSize));
                        if (GUILayout.Button("Clear"))
                        {
                            tgt.DeleteBakedDataForIdentifier(layerInfo.identifier);
                            EditorSceneManager.MarkSceneDirty(SceneManager.GetActiveScene());
                        }
                        EditorGUILayout.EndHorizontal();
                    }
                }
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
