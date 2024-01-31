//
// Copyright 2017-2023 Valve Corporation. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    public abstract class AudioEngineAmbisonicSource
    {
        public virtual void Initialize(GameObject gameObject)
        { }

        public virtual void Destroy()
        { }

        public virtual void UpdateParameters(SteamAudioAmbisonicSource ambisonicSource)
        { }

        public virtual void GetParameters(SteamAudioAmbisonicSource ambisonicSource)
        { }

        public static AudioEngineAmbisonicSource Create(AudioEngineType type)
        {
            switch (type)
            {
            case AudioEngineType.Unity:
                return new UnityAudioEngineAmbisonicSource();
            default:
                return null;
            }
        }
    }
}
