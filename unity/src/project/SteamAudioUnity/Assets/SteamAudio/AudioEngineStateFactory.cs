//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;

namespace SteamAudio
{
    public static class AudioEngineStateFactory
    {
        public static AudioEngineState Create(AudioEngine audioEngine)
        {
            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    return new UnityAudioEngineState();
                case AudioEngine.FMODStudio:
                    return Activator.CreateInstance(Type.GetType("SteamAudio.FMODAudioEngineState")) 
                        as AudioEngineState;
                default:
                    return null;
            }
        }
    }
}