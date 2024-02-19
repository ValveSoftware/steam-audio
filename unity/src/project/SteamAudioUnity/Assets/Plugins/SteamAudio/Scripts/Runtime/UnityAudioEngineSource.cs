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

            mSteamAudioSource = gameObject.GetComponent<SteamAudioSource>();
            if (mSteamAudioSource)
            {
                mHandle = API.iplUnityAddSource(mSteamAudioSource.GetSource().Get());
            }
        }

        public override void Destroy()
        {
            var index = 28;
            mAudioSource.SetSpatializerFloat(index, -1);

            if (mSteamAudioSource)
            {
                API.iplUnityRemoveSource(mHandle);
            }
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
            index++; // Skip 2 deprecated params.
            index++;
            mAudioSource.SetSpatializerFloat(index++, (source.directBinaural) ? 1.0f : 0.0f);
            mAudioSource.SetSpatializerFloat(index++, mHandle);
            mAudioSource.SetSpatializerFloat(index++, (source.perspectiveCorrection) ? 1.0f : 0.0f);
        }
    }
}

#endif
