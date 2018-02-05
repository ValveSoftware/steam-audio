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
        [Space]
        [Header("Simulation Settings")]

        public SceneType rayTracerOption = SceneType.Phonon;

        [Space]
        [Header("Rendering Settings")]

        public ConvolutionOption convolutionOption = ConvolutionOption.Phonon;

        [Space]
        [Header("Resource Reservation")]

        [Range(0, 16)]
        public int minComputeUnitsToReserve = 4;

        [Range(0, 64)]
        public int maxComputeUnitsToReserve = 8;

        [Space]
        [Header("Override Simulation Settings")]

        [Range(0.1f, 5.0f)]
        public float Duration = 1.0f;

        [Range(0, 3)]
        public int AmbisonicsOrder = 1;

        [Range(1, 128)]
        public int MaxSources = 32;
    }
}
