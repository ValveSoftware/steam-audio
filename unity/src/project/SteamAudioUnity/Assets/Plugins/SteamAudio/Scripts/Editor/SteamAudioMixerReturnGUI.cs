//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;
using UnityEditor;

namespace SteamAudio
{
    public class SteamAudioMixerReturnGUI : IAudioEffectPluginGUI
    {
        public override string Name
        {
            get
            {
                return "Steam Audio Mixer Return";
            }
        }

        public override string Vendor
        {
            get
            {
                return "Valve Corporation";
            }
        }

        public override string Description
        {
            get
            {
                return "Enables accelerated mixing of reflections for sources spatialized using Steam Audio.";
            }
        }

        public override bool OnGUI(IAudioEffectPlugin plugin)
        {
            if (SteamAudioSettings.Singleton.audioEngine != AudioEngineType.Unity)
            {
                EditorGUILayout.HelpBox(
                    "This Audio Mixer effect requires the audio engine to be set to Unity. Click" +
                    "Steam Audio > Settings to change this.", MessageType.Warning);

                return false;
            }

            var binauralValue = 0.0f;

            plugin.GetFloatParameter("Binaural", out binauralValue);

            var binaural = (binauralValue == 1.0f);

            binaural = EditorGUILayout.Toggle("Apply HRTF", binaural);

            binauralValue = (binaural) ? 1.0f : 0.0f;

            plugin.SetFloatParameter("Binaural", binauralValue);

            return false;
        }
    }
}
