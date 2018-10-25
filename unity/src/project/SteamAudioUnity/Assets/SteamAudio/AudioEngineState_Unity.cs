//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineState : AudioEngineState
    {
        public override void Initialize(ComponentCache componentCache, GameEngineState gameEngineState, string[] sofaFileNames)
        {
            foreach (var sofaFileName in sofaFileNames) {
                PhononUnityNative.iplUnityAddSOFAFileName(sofaFileName);
            }
            PhononUnityNative.iplUnitySetCurrentSOFAFile(0);

            PhononUnityNative.iplUnitySetEnvironment(gameEngineState.SimulationSettings(),
                gameEngineState.Environment().GetEnvironment(), gameEngineState.ConvolutionType());
        }

        public override void Destroy()
        {
            PhononUnityNative.iplUnityResetEnvironment();
        }

        public override void UpdateListener(Vector3 position, Vector3 ahead, Vector3 up)
        {
            PhononUnityNative.iplUnitySetListener(position, ahead, up);
        }

        public override void UpdateSOFAFile(int index) 
        {
            PhononUnityNative.iplUnitySetCurrentSOFAFile(index);
        }
    }
}