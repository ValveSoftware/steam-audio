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

        public virtual void UpdateParameters(SteamAudioSource steamAudioSource)
        { }

        public virtual bool ShouldSendIdentifier(SteamAudioSource steamAudioSource)
        {
            return true;
        }

        public abstract bool UsesBakedStaticListener(SteamAudioSource steamAudioSource);

        public abstract void SendIdentifier(SteamAudioSource steamAudioSource, int identifier);
    }
}