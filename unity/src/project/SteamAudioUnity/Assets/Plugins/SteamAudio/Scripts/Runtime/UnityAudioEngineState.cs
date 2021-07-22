//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineState : AudioEngineState
    {
        public override void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings)
        {
            API.iplUnityInitialize(context);
            API.iplUnitySetHRTF(defaultHRTF);
            API.iplUnitySetSimulationSettings(simulationSettings);
        }

        public override void Destroy()
        {
            API.iplUnityTerminate();
        }

        public override void SetHRTF(IntPtr hrtf)
        {
            API.iplUnitySetHRTF(hrtf);
        }

        public override void SetReverbSource(Source reverbSource)
        {
            API.iplUnitySetReverbSource(reverbSource.Get());
        }
    }

    public sealed class UnityAudioEngineStateHelpers : AudioEngineStateHelpers
    {
        public override Transform GetListenerTransform()
        {
            return GameObject.FindObjectOfType<AudioListener>().transform;
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
