//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using UnityEngine;

namespace SteamAudio
{
    [CreateAssetMenu(menuName = "Steam Audio/Steam Audio Material")]
    public class SteamAudioMaterial : ScriptableObject
    {
        [Header("Absorption")]
        [Range(0.0f, 1.0f)]
        public float lowFreqAbsorption = 0.1f;
        [Range(0.0f, 1.0f)]
        public float midFreqAbsorption = 0.1f;
        [Range(0.0f, 1.0f)]
        public float highFreqAbsorption = 0.1f;
        [Header("Scattering")]
        [Range(0.0f, 1.0f)]
        public float scattering = 0.5f;
        [Header("Transmission")]
        [Range(0.0f, 1.0f)]
        public float lowFreqTransmission = 0.1f;
        [Range(0.0f, 1.0f)]
        public float midFreqTransmission = 0.1f;
        [Range(0.0f, 1.0f)]
        public float highFreqTransmission = 0.1f;

        public Material GetMaterial()
        {
            var material = new Material { };
            material.absorptionLow = lowFreqAbsorption;
            material.absorptionMid = midFreqAbsorption;
            material.absorptionHigh = highFreqAbsorption;
            material.scattering = scattering;
            material.transmissionLow = lowFreqTransmission;
            material.transmissionMid = midFreqTransmission;
            material.transmissionHigh = highFreqTransmission;
            return material;
        }
    }
}
