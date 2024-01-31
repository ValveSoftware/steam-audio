//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//
#if STEAMAUDIO_ENABLED

using System;
using System.Reflection;
using UnityEngine;

namespace SteamAudio
{
    public abstract class AudioEngineSource
    {
        public virtual void Initialize(GameObject gameObject)
        { }

        public virtual void Destroy()
        { }

        public virtual void UpdateParameters(SteamAudioSource source)
        { }

        public virtual void GetParameters(SteamAudioSource source)
        { }

        public static AudioEngineSource Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineSource();
            case AudioEngineType.FMODStudio:
                return CreateFMODStudioAudioEngineSource();
            default:
                return null;
            }
        }

        private static AudioEngineSource CreateFMODStudioAudioEngineSource()
        {
            var type = Type.GetType("SteamAudio.FMODStudioAudioEngineSource,SteamAudioUnity");
            return (type != null) ? (AudioEngineSource) Activator.CreateInstance(type) : null;
        }
    }
}

#endif
