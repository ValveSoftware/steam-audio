//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//
#if STEAMAUDIO_ENABLED

using System;
using System.Reflection;
using UnityEngine;
using SteamAudio;

namespace SteamAudio
{
    public abstract class AudioEngineState
    {
        public virtual void Initialize(IntPtr context, IntPtr defaultHRTF, SimulationSettings simulationSettings, PerspectiveCorrection correction)
        { }

        public virtual void Destroy()
        { }

        public virtual void SetHRTF(IntPtr hrtf)
        { }

        public virtual void SetPerspectiveCorrection(PerspectiveCorrection correction)
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
                return CreateFMODStudioAudioEngineState();
            default:
                return null;
            }
        }

        private static AudioEngineState CreateFMODStudioAudioEngineState()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineState,SteamAudioUnity");
            return (type != null) ? (AudioEngineState) Activator.CreateInstance(type) : null;
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
                    return CreateFMODStudioAudioEngineStateHelpers();
                default:
                    return null;
            }
        }

        private static AudioEngineStateHelpers CreateFMODStudioAudioEngineStateHelpers()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineStateHelpers,SteamAudioUnity");
            return (type != null) ? (AudioEngineStateHelpers) Activator.CreateInstance(type) : null;
        }
    }
}

#endif
