//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEditor;
using UnityEngine;
using UnityEditor.SceneManagement;
using UnityEngine.SceneManagement;

namespace SteamAudio
{

    //
    // SteamAudioAmbisonicsSourceInspector
    // Custom inspector for SteamAudioAmbisonicsSource components.
    //

    [CustomEditor(typeof(SteamAudioAmbisonicsSource))]
    [CanEditMultipleObjects]
    public class SteamAudioAmbisonicsSourceInspector : Editor
    {
        //
        // Draws the inspector.
        //
        public override void OnInspectorGUI()
        {
            var steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();
            if (steamAudioManager == null)
            {
                EditorGUILayout.HelpBox("A Steam Audio Manager does not exist in the scene. Click Window > Steam" +
                    " Audio.", MessageType.Error);
                return;
            }

            serializedObject.Update();

            var enableBinauralProp = serializedObject.FindProperty("enableBinaural");
            //var hrtfIndexProp = serializedObject.FindProperty("hrtfIndex");
            //var overrideHRTFIndexProp = serializedObject.FindProperty("overrideHRTFIndex");

            if (steamAudioManager.audioEngine == AudioEngine.UnityNative)
            {
                EditorGUILayout.Space();
                EditorGUILayout.LabelField("Ambisonics Settings", EditorStyles.boldLabel);
                EditorGUILayout.PropertyField(enableBinauralProp);
                if (enableBinauralProp.boolValue)
                {
                    //EditorGUILayout.PropertyField(overrideHRTFIndexProp, new GUIContent("Override HRTF Index"));
                    //EditorGUILayout.PropertyField(hrtfIndexProp, new GUIContent("HRTF Index"));
                }
            }

            // Save changes.
            serializedObject.ApplyModifiedProperties();
        }
    }
 }