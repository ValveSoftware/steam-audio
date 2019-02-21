//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    public abstract class AudioEngineAmbisonicsSource
    {
        public virtual void Initialize(GameObject gameObject)
        { }

        public virtual void Destroy()
        { }

        public virtual void UpdateParameters(SteamAudioAmbisonicsSource steamAudioAmbisonicsSource)
        { }

        public virtual void GetParameters(SteamAudioAmbisonicsSource steamAudioAmbisonicsSource)
        { }
    }
}