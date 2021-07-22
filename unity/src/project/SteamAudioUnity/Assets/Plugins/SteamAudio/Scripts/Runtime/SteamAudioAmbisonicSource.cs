//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Ambisonic Source")]
    [RequireComponent(typeof(AudioSource))]
    public class SteamAudioAmbisonicSource : MonoBehaviour
    {
        [Header("HRTF Settings")]
        public bool applyHRTF = true;

        AudioEngineAmbisonicSource mAudioEngineAmbisonicSource = null;

        private void Awake()
        {
            mAudioEngineAmbisonicSource = AudioEngineAmbisonicSource.Create(SteamAudioSettings.Singleton.audioEngine);
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.Initialize(gameObject);
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void Start()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void OnDestroy()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.Destroy();
            }
        }

        private void OnEnable()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

        private void Update()
        {
            if (mAudioEngineAmbisonicSource != null)
            {
                mAudioEngineAmbisonicSource.UpdateParameters(this);
            }
        }

    }
}
