//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

namespace SteamAudio
{
    public sealed class UnityAudioEngineState : AudioEngineState
    {
        public override void Initialize(ComponentCache componentCache, GameEngineState gameEngineState)
        {
            PhononUnityNative.iplUnitySetEnvironment(gameEngineState.SimulationSettings(),
                gameEngineState.Environment().GetEnvironment(), gameEngineState.ConvolutionType());
        }

        public override void Destroy()
        {
            PhononUnityNative.iplUnityResetEnvironment();
            PhononUnityNative.iplUnityResetAudioEngine();
        }

        public override void UpdateListener(Vector3 position, Vector3 ahead, Vector3 up)
        {
            PhononUnityNative.iplUnitySetListener(position, ahead, up);
        }
    }
}