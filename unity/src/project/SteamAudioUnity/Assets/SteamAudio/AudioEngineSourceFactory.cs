//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public static class AudioEngineSourceFactory
    {
        public static AudioEngineSource Create(AudioEngine audioEngine)
        {
            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    return new UnityAudioEngineSource();
                case AudioEngine.FMODStudio:
                    return Activator.CreateInstance(Type.GetType("SteamAudio.FMODAudioEngineSource"))
                        as AudioEngineSource;
                default:
                    return null;
            }
        }
    }
}