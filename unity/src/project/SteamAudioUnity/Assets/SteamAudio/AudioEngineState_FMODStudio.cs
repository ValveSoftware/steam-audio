//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    public sealed class FMODAudioEngineState : AudioEngineState
    {
        public override void Initialize(ComponentCache componentCache, GameEngineState gameEngineState, string[] sofaFileNames)
        {
			foreach (var sofaFileName in sofaFileNames)
			{
				PhononFmod.iplFmodAddSOFAFileName(sofaFileName);
			}
			PhononFmod.iplFmodSetCurrentSOFAFile(0);

			PhononFmod.iplFmodSetEnvironment(gameEngineState.SimulationSettings(),
                gameEngineState.Environment().GetEnvironment(), gameEngineState.ConvolutionType());
        }

        public override void Destroy()
        {
            PhononFmod.iplFmodResetEnvironment();
        }

		public override void UpdateSOFAFile(int index)
		{
			PhononFmod.iplFmodSetCurrentSOFAFile(index);
		}
	}
}