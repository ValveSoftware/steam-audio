//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System.Collections;
using UnityEngine;

namespace SteamAudio
{
	public class ManagerData
	{
        public ComponentCache      componentCache      = new ComponentCache();
        public GameEngineState     gameEngineState     = new GameEngineState();
        public AudioEngineState    audioEngineState    = new AudioEngineState();
        public int                 referenceCount      = 0;

        public void Initialize(GameEngineStateInitReason reason, AudioEngine audioEngine, SimulationSettingsValue simulationValue)
        {
            if (referenceCount == 0)
            {
                componentCache.Initialize();
                gameEngineState.Initialize(simulationValue, componentCache, reason);

                if (reason == GameEngineStateInitReason.Playing)
                    audioEngineState.Initialize(audioEngine, componentCache, gameEngineState);
            }

            ++referenceCount;
        }

        // Destroys Phonon Manager.
        public void Destroy()
        {
            --referenceCount;

            if (referenceCount == 0)
            {
                audioEngineState.Destroy();
                gameEngineState.Destroy();
                componentCache.Destroy();
            }
        }
	}
}