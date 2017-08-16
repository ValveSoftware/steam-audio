//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    public class AudioEngineState
    {
        public void Initialize(AudioEngine engine, ComponentCache componentCache, GameEngineState gameEngineState)
        {
            audioEngine = engine;

            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    PhononUnityNative.iplUnitySetEnvironment(gameEngineState.SimulationSettings(),
                        gameEngineState.Environment().GetEnvironment());
                    break;
                default:
                    Debug.LogError("Unsupported audio engine: " + audioEngine.ToString());
                    break;
            }
        }

        public void Destroy()
        {
            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    PhononUnityNative.iplUnityResetEnvironment();
                    PhononUnityNative.iplUnityResetAudioEngine();
                    break;
                default:
                    break;
            }
        }

        public void UpdateListener(Vector3 position, Vector3 ahead, Vector3 up)
        {
            switch (audioEngine)
            {
                case AudioEngine.UnityNative:
                    PhononUnityNative.iplUnitySetListener(position, ahead, up);
                    break;
                default:
                    break;
            }
        }

        AudioEngine             audioEngine;
    }
}