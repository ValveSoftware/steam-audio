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
        public AudioEngineState    audioEngineState    = null;
        public int                 referenceCount      = 0;

        public void Initialize(GameEngineStateInitReason reason, AudioEngine audioEngine, SimulationSettingsValue simulationValue)
        {
            if (referenceCount == 0)
            {
                componentCache.Initialize();
                gameEngineState.Initialize(simulationValue, componentCache, reason);

                if (reason == GameEngineStateInitReason.Playing)
                {
                    audioEngineState = AudioEngineStateFactory.Create(audioEngine);
                    audioEngineState.Initialize(componentCache, gameEngineState);
                }
            }

            ++referenceCount;
        }

        // Destroys Phonon Manager.
        public void Destroy()
        {
            --referenceCount;

            if (referenceCount == 0)
            {
                if (audioEngineState != null)
                {
                    audioEngineState.Destroy();
                    audioEngineState = null;
                }

                gameEngineState.Destroy();
                componentCache.Destroy();
            }
        }
	}
}