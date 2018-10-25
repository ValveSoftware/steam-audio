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

        [Space]
        [Header("Radeon Rays Settings")]

        [Range(1, 8)]
        public int BakingBatchSize = 4;

        public SceneType RayTracerType() {
            if (rayTracerOption == SceneType.Custom) {
                return rayTracerOption;
            }

#if ((UNITY_EDITOR && UNITY_EDITOR_64) || (UNITY_STANDALONE && UNITY_64))
            if (rayTracerOption == SceneType.Embree) {
                return rayTracerOption;
            }
#endif
#if ((UNITY_EDITOR_WIN && UNITY_EDITOR_64) || (UNITY_STANDALONE_WIN && UNITY_64))
            if (rayTracerOption == SceneType.RadeonRays) {
                return rayTracerOption;
            }
#endif
            return SceneType.Phonon;
        }

        public ConvolutionOption ConvolutionType() {
#if ((UNITY_EDITOR_WIN && UNITY_EDITOR_64) || (UNITY_STANDALONE_WIN && UNITY_64))
            if (convolutionOption == ConvolutionOption.TrueAudioNext) {
                return convolutionOption;
            }
#endif
            return ConvolutionOption.Phonon;
        }
    }
}
