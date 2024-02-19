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
    public class SteamAudioReverbGUI : IAudioEffectPluginGUI
    {
        public override string Name
        {
            get
            {
                return "Steam Audio Reverb";
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
                return "Listener-centric reverb using Steam Audio.";
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
