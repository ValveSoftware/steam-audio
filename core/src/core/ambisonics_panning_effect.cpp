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

#include "ambisonics_panning_effect.h"

#include "math_functions.h"
#include "panning_effect.h"
#include "profiler.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsPanningEffect
// --------------------------------------------------------------------------------------------------------------------

// Virtual speaker positions obtained from:
// http://neilsloane.com/sphdesigns/dim3/des.3.24.7.txt
const Vector3f AmbisonicsPanningEffect::kVirtualSpeakers[AmbisonicsPanningEffect::kNumVirtualSpeakers] = {
    { .8662468181078205913835980f, .4225186537611115291185464f, .2666354015167047203315344f },
    { .8662468181078205913835980f, -.4225186537611115291185464f, -.2666354015167047203315344f },
    { .8662468181078205913835980f, .2666354015167047203315344f, -.4225186537611115291185464f },
    { .8662468181078205913835980f, -.2666354015167047203315344f, .4225186537611115291185464f },
    { -.8662468181078205913835980f, .4225186537611115291185464f, -.2666354015167047203315344f },
    { -.8662468181078205913835980f, -.4225186537611115291185464f, .2666354015167047203315344f },
    { -.8662468181078205913835980f, .2666354015167047203315344f, .4225186537611115291185464f },
    { -.8662468181078205913835980f, -.2666354015167047203315344f, -.4225186537611115291185464f },
    { .2666354015167047203315344f, .8662468181078205913835980f, .4225186537611115291185464f },
    { -.2666354015167047203315344f, .8662468181078205913835980f, -.4225186537611115291185464f },
    { -.4225186537611115291185464f, .8662468181078205913835980f, .2666354015167047203315344f },
    { .4225186537611115291185464f, .8662468181078205913835980f, -.2666354015167047203315344f },
    { -.2666354015167047203315344f, -.8662468181078205913835980f, .4225186537611115291185464f },
    { .2666354015167047203315344f, -.8662468181078205913835980f, -.4225186537611115291185464f },
    { .4225186537611115291185464f, -.8662468181078205913835980f, .2666354015167047203315344f },
    { -.4225186537611115291185464f, -.8662468181078205913835980f, -.2666354015167047203315344f },
    { .4225186537611115291185464f, .2666354015167047203315344f, .8662468181078205913835980f },
    { -.4225186537611115291185464f, -.2666354015167047203315344f, .8662468181078205913835980f },
    { .2666354015167047203315344f, -.4225186537611115291185464f, .8662468181078205913835980f },
    { -.2666354015167047203315344f, .4225186537611115291185464f, .8662468181078205913835980f },
    { .4225186537611115291185464f, -.2666354015167047203315344f, -.8662468181078205913835980f },
    { -.4225186537611115291185464f, .2666354015167047203315344f, -.8662468181078205913835980f },
    { .2666354015167047203315344f, .4225186537611115291185464f, -.8662468181078205913835980f },
    { -.2666354015167047203315344f, -.4225186537611115291185464f, -.8662468181078205913835980f }
};

AmbisonicsPanningEffect::AmbisonicsPanningEffect(const AudioSettings& audioSettings,
                                                 const AmbisonicsPanningEffectSettings& effectSettings)
    : mSpeakerLayout(*effectSettings.speakerLayout)
    , mMaxOrder(effectSettings.maxOrder)
    , mAmbisonicsToSpeakersMatrix(effectSettings.speakerLayout->numSpeakers, SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder))
    , mAmbisonicsVectors(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder), audioSettings.frameSize)
    , mSpeakersVectors(effectSettings.speakerLayout->numSpeakers, audioSettings.frameSize)
{
    DynamicMatrixf ambisonicsToVirtualSpeakersMatrix(kNumVirtualSpeakers, SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder));

    for (auto l = 0, i = 0; l <= effectSettings.maxOrder; ++l)
    {
        for (auto m = -l; m <= l; ++m, ++i)
        {
            for (auto j = 0; j < kNumVirtualSpeakers; ++j)
            {
                ambisonicsToVirtualSpeakersMatrix(j, i) = SphericalHarmonics::evaluate(l, m, Vector3f::unitVector(kVirtualSpeakers[j]));
            }
        }
    }

    DynamicMatrixf virtualSpeakersToSpeakersMatrix(effectSettings.speakerLayout->numSpeakers, kNumVirtualSpeakers);

    for (auto i = 0; i < kNumVirtualSpeakers; ++i)
    {
        for (auto j = 0; j < effectSettings.speakerLayout->numSpeakers; ++j)
        {
            virtualSpeakersToSpeakersMatrix(j, i) = (4.0f * Math::kPi / kNumVirtualSpeakers) * PanningEffect::panningWeight(kVirtualSpeakers[i], *effectSettings.speakerLayout, j);
        }
    }

    multiplyMatrices(virtualSpeakersToSpeakersMatrix, ambisonicsToVirtualSpeakersMatrix, mAmbisonicsToSpeakersMatrix);
}

void AmbisonicsPanningEffect::reset()
{}

// Based on t-design Ambisonic panning, as described in:
//
//  All-Round Ambisonic Panning and Decoding
//  F. Zotter, M. Frank
//  Journal of the Audio Engineering Society 2012
AudioEffectState AmbisonicsPanningEffect::apply(const AmbisonicsPanningEffectParams& params,
                                                const AudioBuffer& in,
                                                AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == SphericalHarmonics::numCoeffsForOrder(params.order));
    assert(out.numChannels() == mSpeakerLayout.numSpeakers);
    assert(params.order <= mMaxOrder);

    PROFILE_FUNCTION();

    out.makeSilent();

    for (auto k = 0; k < in.numSamples(); ++k)
    {
        for (auto i = 0; i < in.numChannels(); ++i)
        {
            mAmbisonicsVectors(i, k) = in[i][k];
        }
    }

    multiplyMatrices(mAmbisonicsToSpeakersMatrix, mAmbisonicsVectors, mSpeakersVectors);

    for (auto k = 0; k < out.numSamples(); ++k)
    {
        for (auto i = 0; i < out.numChannels(); ++i)
        {
            out[i][k] = mSpeakersVectors(i, k);
        }
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState AmbisonicsPanningEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

}
