//
// Copyright 2017-2023 Valve Corporation.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
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
