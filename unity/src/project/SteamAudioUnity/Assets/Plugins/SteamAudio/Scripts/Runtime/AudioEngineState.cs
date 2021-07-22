//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;
using SteamAudio;

namespace SteamAudio
{
    public abstract class AudioEngineState
    {
        public virtual void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings)
        { }

        public virtual void Destroy()
        { }

        public virtual void SetHRTF(IntPtr hrtf)
        { }

        public virtual void SetReverbSource(Source reverbSource)
        { }

        public static AudioEngineState Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineState();
            case AudioEngineType.FMODStudio:
                return new FMODStudioAudioEngineState();
            default:
                return null;
            }
        }
    }

    public abstract class AudioEngineStateHelpers
    {
        public abstract Transform GetListenerTransform();

        public abstract SteamAudio.AudioSettings GetAudioSettings();

        public static AudioEngineStateHelpers Create(AudioEngineType type)
        {
            switch (type)
            {
                case AudioEngineType.Unity:
                    return new UnityAudioEngineStateHelpers();
                case AudioEngineType.FMODStudio:
                    return new FMODStudioAudioEngineStateHelpers();
                default:
                    return null;
            }
        }
    }
}
