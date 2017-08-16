//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    // Stores references to various Steam Audio components, so that we don't have to repeatedly call
    // FindObjectOfType<> to find them every frame.
    public class ComponentCache
    {
        public void Initialize()
        {
            AudioListener();
            SteamAudioListener();
            SteamAudioCustomSettings();
            SteamAudioCustomSpeakerLayout();
        }

        public void Destroy()
        {
            audioListener                   = null;
            steamAudioListener              = null;
            steamAudioCustomSpeakerLayout   = null;
            steamAudioCustomSettings        = null;

            isSteamAudioListenerSet             = false;
            isSteamAudioCustomSpeakerLayoutSet  = false;
            isSteamAudioCustomSettingsSet       = false;
        }

        public AudioListener AudioListener()
        {
            audioListener = GameObject.FindObjectOfType<AudioListener>();
            return audioListener;
        }

        public SteamAudioListener SteamAudioListener()
        {
            if (!isSteamAudioListenerSet && steamAudioListener == null)
            {
                steamAudioListener = GameObject.FindObjectOfType<SteamAudioListener>();
                isSteamAudioListenerSet = true;
            }

            return steamAudioListener;
        }

        public SteamAudioCustomSpeakerLayout SteamAudioCustomSpeakerLayout()
        {
            if (!isSteamAudioCustomSpeakerLayoutSet && steamAudioCustomSpeakerLayout == null)
            {
                steamAudioCustomSpeakerLayout = GameObject.FindObjectOfType<SteamAudioCustomSpeakerLayout>();
                isSteamAudioCustomSpeakerLayoutSet = true;
            }

            return steamAudioCustomSpeakerLayout;
        }

        public SteamAudioCustomSettings SteamAudioCustomSettings()
        {
            if (!isSteamAudioCustomSettingsSet && steamAudioCustomSettings == null)
            {
                steamAudioCustomSettings = GameObject.FindObjectOfType<SteamAudioCustomSettings>();
                isSteamAudioCustomSettingsSet = true;
            }

            return steamAudioCustomSettings;
        }

        AudioListener                   audioListener                       = null;
        SteamAudioListener              steamAudioListener                  = null;
        SteamAudioCustomSpeakerLayout   steamAudioCustomSpeakerLayout       = null;
        SteamAudioCustomSettings        steamAudioCustomSettings            = null;

        bool                            isSteamAudioListenerSet             = false;
        bool                            isSteamAudioCustomSpeakerLayoutSet  = false;
        bool                            isSteamAudioCustomSettingsSet       = false;
    }
}