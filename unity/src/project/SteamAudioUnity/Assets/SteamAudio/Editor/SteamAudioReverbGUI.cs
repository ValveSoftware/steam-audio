//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    public class SteamAudioReverbGUI : IAudioEffectPluginGUI
    {
        public override string Name
        {
            get { return "Steam Audio Reverb"; }
        }

        public override string Description
        {
            get { return "Listener-centric reverb using Steam Audio."; }
        }

        public override string Vendor
        {
            get { return "Valve Corporation"; }
        }

        public override bool OnGUI(IAudioEffectPlugin plugin)
        {
            if (steamAudioManager == null)
                steamAudioManager = GameObject.FindObjectOfType<SteamAudioManager>();

            if (steamAudioManager == null)
            {
                EditorGUILayout.HelpBox("A Steam Audio Manager does not exist in the scene. Click Window > Steam" +
                    " Audio.", MessageType.Error);
                return false;
            }

            if (steamAudioManager.audioEngine != AudioEngine.UnityNative)
            {
                EditorGUILayout.HelpBox("This Audio Mixer effect requires the audio engine to be set to Unity Native." +
                    " Click Window > Steam Audio to change this.", MessageType.Error);
                return false;
            }

            var binauralValue = 0.0f;
            var typeValue = 0.0f;
            var bypassDuringInitValue = 0.0f;

            plugin.GetFloatParameter("Binaural", out binauralValue);
            plugin.GetFloatParameter("Type", out typeValue);
            plugin.GetFloatParameter("BypassAtInit", out bypassDuringInitValue);

            var binaural = (binauralValue == 1.0f);
            var type = (SimulationType) typeValue;
            var bypassDuringInit = (bypassDuringInitValue == 1.0f);

            binaural = EditorGUILayout.Toggle("Binaural", binaural);
            type = (SimulationType) EditorGUILayout.EnumPopup("Simulation Type", type);

            EditorGUILayout.Space();
            if (showAdvancedOptions = EditorGUILayout.Foldout(showAdvancedOptions, "Advanced Options"))
            {
                bypassDuringInit = EditorGUILayout.Toggle("Avoid Silence During Init", bypassDuringInit);
            }

            binauralValue = (binaural) ? 1.0f : 0.0f;
            typeValue = (float) type;
            bypassDuringInitValue = (bypassDuringInit) ? 1.0f : 0.0f;

            plugin.SetFloatParameter("Binaural", binauralValue);
            plugin.SetFloatParameter("Type", typeValue);
            plugin.SetFloatParameter("BypassAtInit", bypassDuringInitValue);
            return false;
        }

        SteamAudioManager   steamAudioManager   = null;
        bool                showAdvancedOptions = false;
    }
}