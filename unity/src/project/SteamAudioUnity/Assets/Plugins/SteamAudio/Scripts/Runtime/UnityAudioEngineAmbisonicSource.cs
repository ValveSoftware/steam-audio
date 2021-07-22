//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineAmbisonicSource : AudioEngineAmbisonicSource
    {
        AudioSource mAudioSource = null;

        public override void Initialize(GameObject gameObject)
        {
            mAudioSource = gameObject.GetComponent<AudioSource>();
        }

        public override void UpdateParameters(SteamAudioAmbisonicSource ambisonicSource)
        {
            if (!mAudioSource)
                return;

            var index = 0;
            mAudioSource.SetAmbisonicDecoderFloat(index++, (ambisonicSource.applyHRTF) ? 1.0f : 0.0f);
        }
    }
}
