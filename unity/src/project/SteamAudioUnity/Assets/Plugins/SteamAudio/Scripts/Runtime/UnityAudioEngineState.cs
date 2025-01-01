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
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineState : AudioEngineState
    {
        public override void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings, PerspectiveCorrection correction)
        {
            API.iplUnityInitialize(context);
            API.iplUnitySetHRTF(defaultHRTF);
            API.iplUnitySetSimulationSettings(simulationSettings);
            API.iplUnitySetPerspectiveCorrection(correction);
        }

        public override void Destroy()
        {
            API.iplUnityTerminate();
        }

        public override void SetHRTF(IntPtr hrtf)
        {
            API.iplUnitySetHRTF(hrtf);
        }

        public override void SetPerspectiveCorrection(PerspectiveCorrection correction)
        {
            API.iplUnitySetPerspectiveCorrection(correction);
        }

        public override void SetReverbSource(Source reverbSource)
        {
            API.iplUnitySetReverbSource(reverbSource.Get());
        }

        public override void SetHRTFDisabled(bool disabled)
        {
            base.SetHRTFDisabled(disabled);

            API.iplUnitySetHRTFDisabled(disabled);
        }
    }

    public sealed class UnityAudioEngineStateHelpers : AudioEngineStateHelpers
    {
        public override Transform GetListenerTransform()
        {
#if UNITY_2023_3_OR_NEWER
            var audioListener = GameObject.FindFirstObjectByType<AudioListener>();
#else
            var audioListener = GameObject.FindObjectOfType<AudioListener>();
#endif
            return (audioListener != null) ? audioListener.transform : null;
        }

        public override AudioSettings GetAudioSettings()
        {
            var audioSettings = new AudioSettings { };

            audioSettings.samplingRate = UnityEngine.AudioSettings.outputSampleRate;

            var numBuffers = 0;
            UnityEngine.AudioSettings.GetDSPBufferSize(out audioSettings.frameSize, out numBuffers);

            return audioSettings;
        }
    }
}

#endif
