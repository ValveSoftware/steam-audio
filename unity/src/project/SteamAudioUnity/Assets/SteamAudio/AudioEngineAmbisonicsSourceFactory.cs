//
// Copyright 2018 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public static class AudioEngineAmbisonicsSourceFactory
    {
        public static AudioEngineAmbisonicsSource Create(AudioEngine audioEngine)
        {
            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    return new UnityAudioEngineAmbisonicsSource();
                //case AudioEngine.FMODStudio:
                //    return Activator.CreateInstance(Type.GetType("SteamAudio.FMODAudioEngineSource"))
                //        as AudioEngineSource;
                default:
                    return null;
            }
        }
    }
}