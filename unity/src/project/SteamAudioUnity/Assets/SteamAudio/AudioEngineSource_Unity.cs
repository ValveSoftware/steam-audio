//
// Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
// https://valvesoftware.github.io/steam-audio/license.html
//

using System;
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineSource : AudioEngineSource
    {
        public override void Initialize(GameObject gameObject)
        {
            audioSource = gameObject.GetComponent<AudioSource>();
        }

        public override void UpdateParameters(SteamAudioSource steamAudioSource)
        {
            if (!audioSource || !audioSource.isPlaying)
                return;

            var directBinauralValue = (steamAudioSource.directBinaural) ? 1.0f : 0.0f;
            var hrtfInterpolationValue = (float)steamAudioSource.interpolation;
            var distanceAttenuationValue = (steamAudioSource.physicsBasedAttenuation) ? 1.0f : 0.0f;
            var airAbsorptionValue = (steamAudioSource.airAbsorption) ? 1.0f : 0.0f;
            var dipoleWeightValue = steamAudioSource.dipoleWeight;
            var dipolePowerValue = steamAudioSource.dipolePower;
            var occlusionModeValue = (float)steamAudioSource.occlusionMode;
            var occlusionMethodValue = (float)steamAudioSource.occlusionMethod;
            var sourceRadiusValue = steamAudioSource.sourceRadius;
            var directMixLevelValue = steamAudioSource.directMixLevel;
            var reflectionsValue = (steamAudioSource.reflections) ? 1.0f : 0.0f;
            var indirectBinauralValue = (steamAudioSource.indirectBinaural) ? 1.0f : 0.0f;
            var indirectMixLevelValue = steamAudioSource.indirectMixLevel;
            var simulationTypeValue = (steamAudioSource.simulationType == SourceSimulationType.Realtime) ? 0.0f : 1.0f;
            var usesStaticListenerValue = (steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener) ? 1.0f : 0.0f;
            var bypassDuringInitValue = (steamAudioSource.avoidSilenceDuringInit) ? 1.0f : 0.0f;
            var hrtfIndexValue = (float) steamAudioSource.hrtfIndex;
            var overrideHRTFIndexValue = (steamAudioSource.overrideHRTFIndex) ? 1.0f : 0.0f;

            audioSource.SetSpatializerFloat(0, directBinauralValue);
            audioSource.SetSpatializerFloat(1, hrtfInterpolationValue);
            audioSource.SetSpatializerFloat(2, distanceAttenuationValue);
            audioSource.SetSpatializerFloat(3, airAbsorptionValue);
            audioSource.SetSpatializerFloat(4, occlusionModeValue);
            audioSource.SetSpatializerFloat(5, occlusionMethodValue);
            audioSource.SetSpatializerFloat(6, sourceRadiusValue);
            audioSource.SetSpatializerFloat(7, directMixLevelValue);
            audioSource.SetSpatializerFloat(8, reflectionsValue);
            audioSource.SetSpatializerFloat(9, indirectBinauralValue);
            audioSource.SetSpatializerFloat(10, indirectMixLevelValue);
            audioSource.SetSpatializerFloat(11, simulationTypeValue);
            audioSource.SetSpatializerFloat(12, usesStaticListenerValue);
            audioSource.SetSpatializerFloat(14, bypassDuringInitValue);
            audioSource.SetSpatializerFloat(15, dipoleWeightValue);
            audioSource.SetSpatializerFloat(16, dipolePowerValue);
            audioSource.SetSpatializerFloat(17, hrtfIndexValue);
            audioSource.SetSpatializerFloat(18, overrideHRTFIndexValue);

            var index = 19;
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.distanceAttenuation);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionLow);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionMid);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.airAbsorptionHigh);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.propagationDelay);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.occlusionFactor);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorLow);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorMid);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.transmissionFactorHigh);
            audioSource.SetSpatializerFloat(index++, steamAudioSource.directPath.directivityFactor);
        }

        public override bool ShouldSendIdentifier(SteamAudioSource steamAudioSource)
        {
            return (steamAudioSource.simulationType == SourceSimulationType.BakedStaticSource ||
                steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener);
        }

        public override bool UsesBakedStaticListener(SteamAudioSource steamAudioSource)
        {
            return (steamAudioSource.simulationType == SourceSimulationType.BakedStaticListener);
        }

        public override void SendIdentifier(SteamAudioSource steamAudioSource, int identifier)
        {
            var identifierFloat = BitConverter.ToSingle(BitConverter.GetBytes(identifier), 0);
            audioSource.SetSpatializerFloat(13, identifierFloat);
        }

        AudioSource     audioSource     = null;
    }
}