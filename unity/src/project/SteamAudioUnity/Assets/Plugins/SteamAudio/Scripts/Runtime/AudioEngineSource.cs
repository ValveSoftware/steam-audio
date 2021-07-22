//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

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
                return new FMODStudioAudioEngineSource();
            default:
                return null;
            }
        }
    }
}
