//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    //
    // CustomPhononSettings
    // Custom Phonon Settings.
    //

    [AddComponentMenu("Steam Audio/Steam Audio Custom Settings")]
    public class SteamAudioCustomSettings : MonoBehaviour
    {
        // Simulation settings.
        public SceneType rayTracerOption = SceneType.Phonon;

        //Renderer settings.
        public ConvolutionOption convolutionOption;

        //OpenCL settings.
        [Range(0, 8)]
        public int numComputeUnits = 4;
    }
}
