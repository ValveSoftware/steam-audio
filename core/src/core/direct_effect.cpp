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

#include "direct_effect.h"

#include <algorithm>

#include "error.h"
#include "log.h"

namespace ipl {

DirectEffect::DirectEffect(const AudioSettings& audioSettings,
                           const DirectEffectSettings& effectSettings)
    : mNumChannels(effectSettings.numChannels)
    , mEQEffects(effectSettings.numChannels)
    , mGainEffects(effectSettings.numChannels)
{
    for (auto i = 0; i < effectSettings.numChannels; ++i)
    {
        mEQEffects[i] = make_unique<EQEffect>(audioSettings);
        mGainEffects[i] = make_unique<GainEffect>(audioSettings);
    }
}

void DirectEffect::reset()
{
    for (auto i = 0; i < mNumChannels; ++i)
    {
        mEQEffects[i]->reset();
        mGainEffects[i]->reset();
    }
}

AudioEffectState DirectEffect::apply(const DirectEffectParams& params,
                                     const AudioBuffer& in,
                                     AudioBuffer& out)
{
    float gain;
    float eqCoeffs[Bands::kNumBands];
    calculateGainAndEQ(params.directPath, params.flags, params.transmissionType, gain, eqCoeffs);

    auto applyEQ = ((params.flags & ApplyAirAbsorption) ||
        ((params.flags & ApplyTransmission) && params.transmissionType == TransmissionType::FreqDependent));

    for (auto i = 0; i < mNumChannels; ++i)
    {
        AudioBuffer inChannel(in, i);
        AudioBuffer outChannel(out, i);

        if (applyEQ)
        {
            EQEffectParams eqParams{};
            eqParams.gains = eqCoeffs;

            mEQEffects[i]->apply(eqParams, inChannel, outChannel);
        }

        GainEffectParams gainParams{};
        gainParams.gain = gain;

        mGainEffects[i]->apply(gainParams, (applyEQ) ? outChannel : inChannel, outChannel);
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState DirectEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

void DirectEffect::calculateGainAndEQ(const DirectSoundPath& directPath,
                                      DirectEffectFlags flags,
                                      TransmissionType transmissionType,
                                      float& overallGain,
                                      float* eqCoeffs)
{
    // Apply distance attenuation.
    overallGain = (flags & ApplyDistanceAttenuation) ? directPath.distanceAttenuation : 1.0f;

    // Apply air absorption.
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        eqCoeffs[i] = (flags & ApplyAirAbsorption) ? directPath.airAbsorption[i] : 1.0f;
    }

    // Apply directivity.
    if (flags & ApplyDirectivity)
    {
        overallGain *= directPath.directivity;
    }

    // Apply occlusion and transmission.
    if (flags & ApplyOcclusion)
    {
        if (flags & ApplyTransmission)
        {
            if (transmissionType == TransmissionType::FreqIndependent)
            {
                // Update attenuation factor with the average transmission coefficient and appropriately applied
                // occlusion factor.
                auto averageTransmissionFactor = .0f;
                for (auto i = 0; i < Bands::kNumBands; ++i)
                {
                    averageTransmissionFactor += directPath.transmission[i];
                }
                averageTransmissionFactor /= Bands::kNumBands;

                overallGain *= directPath.occlusion + (1 - directPath.occlusion) * averageTransmissionFactor;
            }
            else if (transmissionType == TransmissionType::FreqDependent)
            {
                // Update per frequency factors based on occlusion and transmission value.
                for (auto i = 0; i < Bands::kNumBands; ++i)
                {
                    eqCoeffs[i] *= directPath.occlusion + (1 - directPath.occlusion) * directPath.transmission[i];
                }
            }
        }
        else
        {
            // Update attenuation factor with the occlusion factor only.
            overallGain *= directPath.occlusion;
        }
    }

    if ((flags & ApplyAirAbsorption) ||
        ((flags & ApplyTransmission) && transmissionType == TransmissionType::FreqDependent))
    {
        // Maxium value in EQ filter should be normalized to 1 and common factor rolled into attenuation factor,
        // this will allow for smooth changes to frequency changes (possible exception is if maximum remains
        // and low / mid frequencies change dramatically). Minimum value should be .0625 (24 dB) for any frequency
        // band for a good EQ response.
        EQEffect::normalizeGains(eqCoeffs, overallGain);
    }
}

}
