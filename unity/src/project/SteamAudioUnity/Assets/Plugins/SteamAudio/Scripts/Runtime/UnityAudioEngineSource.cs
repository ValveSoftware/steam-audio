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
        AudioSource mAudioSource = null;
        byte[] mSimulationOutputsBuffer = new byte[sizeof(Int64)];

        public override void Initialize(GameObject gameObject)
        {
            mAudioSource = gameObject.GetComponent<AudioSource>();
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            if (!mAudioSource)
                return;

            var index = 0;
            mAudioSource.SetSpatializerFloat(index++, (source.distanceAttenuation) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.airAbsorption) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.directivity) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.occlusion) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.transmission) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.reflections) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (source.pathing) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, (float) source.interpolation);
            mAudioSource.SetSpatializerFloat(index++, source.distanceAttenuationValue);
            mAudioSource.SetSpatializerFloat(index++, (source.distanceAttenuationInput == DistanceAttenuationInput.CurveDriven) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, source.airAbsorptionLow);
            mAudioSource.SetSpatializerFloat(index++, source.airAbsorptionMid);
            mAudioSource.SetSpatializerFloat(index++, source.airAbsorptionHigh);
            mAudioSource.SetSpatializerFloat(index++, (source.airAbsorptionInput == AirAbsorptionInput.UserDefined) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, source.directivityValue);
            mAudioSource.SetSpatializerFloat(index++, source.dipoleWeight);
            mAudioSource.SetSpatializerFloat(index++, source.dipolePower);
            mAudioSource.SetSpatializerFloat(index++, (source.directivityInput == DirectivityInput.UserDefined) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, source.occlusionValue);
            mAudioSource.SetSpatializerFloat(index++, (float) source.transmissionType);
            mAudioSource.SetSpatializerFloat(index++, source.transmissionLow);
            mAudioSource.SetSpatializerFloat(index++, source.transmissionMid);
            mAudioSource.SetSpatializerFloat(index++, source.transmissionHigh);
            mAudioSource.SetSpatializerFloat(index++, source.directMixLevel);
            mAudioSource.SetSpatializerFloat(index++, (source.applyHRTFToReflections) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, source.reflectionsMixLevel);
            mAudioSource.SetSpatializerFloat(index++, (source.applyHRTFToPathing) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, source.pathingMixLevel);
            SetSpatializerIntPtr(source.GetSource().Get(), ref index);
            mAudioSource.SetSpatializerFloat(index++, (source.directBinaural) ? 1.0f : 0.0f);
        }

        void SetSpatializerIntPtr(IntPtr ptr, ref int index)
        {
            if (IntPtr.Size == sizeof(Int64))
            {
                var ptrValue = ptr.ToInt64();

                mSimulationOutputsBuffer[0] = (byte) (ptrValue >> 0);
                mSimulationOutputsBuffer[1] = (byte) (ptrValue >> 8);
                mSimulationOutputsBuffer[2] = (byte) (ptrValue >> 16);
                mSimulationOutputsBuffer[3] = (byte) (ptrValue >> 24);
                mSimulationOutputsBuffer[4] = (byte) (ptrValue >> 32);
                mSimulationOutputsBuffer[5] = (byte) (ptrValue >> 40);
                mSimulationOutputsBuffer[6] = (byte) (ptrValue >> 48);
                mSimulationOutputsBuffer[7] = (byte) (ptrValue >> 56);

                var valueLow = BitConverter.ToSingle(mSimulationOutputsBuffer, 0);
                var valueHigh = BitConverter.ToSingle(mSimulationOutputsBuffer, 4);

                mAudioSource.SetSpatializerFloat(index++, valueLow);
                mAudioSource.SetSpatializerFloat(index++, valueHigh);
            }
            else
            {
                var ptrValue = ptr.ToInt32();

                mSimulationOutputsBuffer[0] = (byte) (ptrValue >> 0);
                mSimulationOutputsBuffer[1] = (byte) (ptrValue >> 8);
                mSimulationOutputsBuffer[2] = (byte) (ptrValue >> 16);
                mSimulationOutputsBuffer[3] = (byte) (ptrValue >> 24);

                var value = BitConverter.ToSingle(mSimulationOutputsBuffer, 0);

                mAudioSource.SetSpatializerFloat(index++, value);
                mAudioSource.SetSpatializerFloat(index++, 0.0f);
            }
        }
    }
}
