//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

namespace SteamAudio
{
    public sealed class FMODAudioEngineState : AudioEngineState
    {
        public override void Initialize(ComponentCache componentCache, GameEngineState gameEngineState)
        {
            PhononFmod.iplFmodSetEnvironment(gameEngineState.SimulationSettings(),
                gameEngineState.Environment().GetEnvironment(), gameEngineState.ConvolutionType());
        }

        public override void Destroy()
        {
            PhononFmod.iplFmodResetEnvironment();
            PhononFmod.iplFmodResetAudioEngine();
        }
    }
}