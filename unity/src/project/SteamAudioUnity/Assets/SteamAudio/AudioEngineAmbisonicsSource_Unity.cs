//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineAmbisonicsSource : AudioEngineAmbisonicsSource
    {
        private const int k_binauralModeIndex = 0;
        private const int k_hrtfIndexIndex = 1;
        private const int k_overrideHRTFIndexIndex = 2;

        public override void Initialize(GameObject gameObject)
        {
            audioSource = gameObject.GetComponent<AudioSource>();
        }

        public override void UpdateParameters(SteamAudioAmbisonicsSource steamAudioAmbisonicsSource)
        {
            if (!audioSource)
                return;

            var enableBinauralValue = (steamAudioAmbisonicsSource.enableBinaural) ? 1.0f : 0.0f;
            var hrtfIndexValue = (float)steamAudioAmbisonicsSource.hrtfIndex;
            var overrideHRTFIndexValue = (steamAudioAmbisonicsSource.overrideHRTFIndex) ? 1.0f : 0.0f;

            audioSource.SetAmbisonicDecoderFloat(k_binauralModeIndex, enableBinauralValue);
            audioSource.SetAmbisonicDecoderFloat(k_hrtfIndexIndex, hrtfIndexValue);
            audioSource.SetAmbisonicDecoderFloat(k_overrideHRTFIndexIndex, overrideHRTFIndexValue);
        }

        AudioSource     audioSource     = null;
    }
}