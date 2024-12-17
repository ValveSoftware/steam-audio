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
#if STEAMAUDIO_ENABLED

using System;
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public sealed class WwiseAudioEngineState : AudioEngineState
    {
        public override void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings, PerspectiveCorrection correction)
        {
            var wwiseSettings = new WwiseSettings{ metersPerUnit = 1.0f };
            WwiseAPI.iplWwiseInitialize(context, ref wwiseSettings);
            WwiseAPI.iplWwiseSetHRTF(defaultHRTF);
            WwiseAPI.iplWwiseSetSimulationSettings(simulationSettings);
        }

        public override void Destroy()
        {
            WwiseAPI.iplWwiseTerminate();
        }

        public override void SetHRTF(IntPtr hrtf)
        {
            WwiseAPI.iplWwiseSetHRTF(hrtf);
        }

        public override void SetReverbSource(Source reverbSource)
        {
            WwiseAPI.iplWwiseSetReverbSource(reverbSource.Get());
        }
    }

    public sealed class WwiseAudioEngineStateHelpers : AudioEngineStateHelpers
    {
        public override Transform GetListenerTransform()
        {
            var wwiseListener = (MonoBehaviour) GameObject.FindObjectOfType<AkAudioListener>();
            if (wwiseListener == null)
                return null;

            return wwiseListener.transform;
        }

        public override AudioSettings GetAudioSettings()
        {
            return new AudioSettings { 
                samplingRate = (int) AkWwiseInitializationSettings.Instance.UserSettings.m_SampleRate,
                frameSize = (int) AkWwiseInitializationSettings.Instance.UserSettings.m_SamplesPerFrame
            };
        }
    }
}

#endif