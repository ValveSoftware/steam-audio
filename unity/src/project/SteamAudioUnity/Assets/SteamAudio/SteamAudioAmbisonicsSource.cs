//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    [AddComponentMenu("Steam Audio/Steam Audio Ambisonics Source")]
    public class SteamAudioAmbisonicsSource : MonoBehaviour
    {
        void Awake()
        {
            var steamAudioManager = SteamAudioManager.GetSingleton();
            if (steamAudioManager == null)
            {
                Debug.LogError("Phonon Manager Settings object not found in the scene! Click Window > Phonon");
                return;
            }

            steamAudioManager.Initialize(GameEngineStateInitReason.Playing);
            managerData = steamAudioManager.ManagerData();

            audioEngine = steamAudioManager.audioEngine;
            audioEngineAmbisonicsSource = AudioEngineAmbisonicsSourceFactory.Create(audioEngine);
            audioEngineAmbisonicsSource.Initialize(gameObject);

            audioEngineAmbisonicsSource.UpdateParameters(this);
        }

        void Start()
        {
            audioEngineAmbisonicsSource.UpdateParameters(this);
        }

        void OnEnable()
        {
            audioEngineAmbisonicsSource.UpdateParameters(this);
        }

        void OnDestroy()
        {
            if (managerData != null)
                managerData.Destroy();

            if (audioEngineAmbisonicsSource != null)
                audioEngineAmbisonicsSource.Destroy();
        }

        void Update()
        {
            audioEngineAmbisonicsSource.UpdateParameters(this);
        }

        public bool                 enableBinaural      = true;

        public bool                 overrideHRTFIndex   = false;
        public int                  hrtfIndex           = 0;

        ManagerData                 managerData         = null;
        AudioEngine                 audioEngine         = AudioEngine.UnityNative;

        AudioEngineAmbisonicsSource audioEngineAmbisonicsSource = null;
    }
}