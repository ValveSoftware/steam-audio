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
#if STEAMAUDIO_ENABLED

using System;
using UnityEngine;

namespace SteamAudio
{
    public sealed class UnityAudioEngineSource : AudioEngineSource
    {
        AudioSource mAudioSource = null;
        SteamAudioSource mSteamAudioSource = null;
        int mHandle = -1;

        public override void Initialize(GameObject gameObject)
        {
            mAudioSource = gameObject.GetComponent<AudioSource>();

            for (int i = 0; i < mCachedParameters.Length; i++)
                mCachedParameters[i] = float.NaN;

            mSteamAudioSource = gameObject.GetComponent<SteamAudioSource>();
            if (mSteamAudioSource)
            {
                mHandle = API.iplUnityAddSource(mSteamAudioSource.GetSource().Get());
            }
        }

        public override void Destroy()
        {
            var index = 28;

            if (mAudioSource != null)
            {
                mAudioSource.SetSpatializerFloat(index, -1);
            }

            if (mSteamAudioSource)
            {
                API.iplUnityRemoveSource(mHandle);
            }
        }

        const int SPATIALIZER_PARAMETER_COUNT = 34;
        readonly float[] mCachedParameters = new float[SPATIALIZER_PARAMETER_COUNT];

        void SetParameter(int index, float value)
        {
            if (mCachedParameters[index] == value)
                return;

            mCachedParameters[index] = value;
            mAudioSource.SetSpatializerFloat(index, value);
        }

        public override void UpdateParameters(SteamAudioSource source)
        {
            if (!mAudioSource)
                return;

            var index = 0;
            SetParameter(index++, (source.distanceAttenuation) ? 1.0f : 0.0f);
            SetParameter(index++, (source.airAbsorption) ? 1.0f : 0.0f);
            SetParameter(index++, (source.directivity) ? 1.0f : 0.0f);
            SetParameter(index++, (source.occlusion) ? 1.0f : 0.0f);
            SetParameter(index++, (source.transmission) ? 1.0f : 0.0f);
            SetParameter(index++, (source.reflections) ? 1.0f : 0.0f);
            SetParameter(index++, (source.pathing) ? 1.0f : 0.0f);
            SetParameter(index++, (float) source.interpolation);
            SetParameter(index++, source.distanceAttenuationValue);
            SetParameter(index++, (source.distanceAttenuationInput == DistanceAttenuationInput.CurveDriven) ? 1.0f : 0.0f);
            SetParameter(index++, source.airAbsorptionLow);
            SetParameter(index++, source.airAbsorptionMid);
            SetParameter(index++, source.airAbsorptionHigh);
            SetParameter(index++, (source.airAbsorptionInput == AirAbsorptionInput.UserDefined) ? 1.0f : 0.0f);
            SetParameter(index++, source.directivityValue);
            SetParameter(index++, source.dipoleWeight);
            SetParameter(index++, source.dipolePower);
            SetParameter(index++, (source.directivityInput == DirectivityInput.UserDefined) ? 1.0f : 0.0f);
            SetParameter(index++, source.occlusionValue);
            SetParameter(index++, (float) source.transmissionType);
            SetParameter(index++, source.transmissionLow);
            SetParameter(index++, source.transmissionMid);
            SetParameter(index++, source.transmissionHigh);
            SetParameter(index++, source.directMixLevel);
            SetParameter(index++, (source.applyHRTFToReflections) ? 1.0f : 0.0f);
            SetParameter(index++, source.reflectionsMixLevel);
            SetParameter(index++, (source.applyHRTFToPathing) ? 1.0f : 0.0f);
            SetParameter(index++, source.pathingMixLevel);
            index++; // Skip 2 deprecated params.
            index++;
            SetParameter(index++, (source.directBinaural) ? 1.0f : 0.0f);
            SetParameter(index++, mHandle);
            SetParameter(index++, (source.perspectiveCorrection) ? 1.0f : 0.0f);
            SetParameter(index++, (source.normalizePathingEQ) ? 1.0f : 0.0f);
        }
    }
}

#endif
