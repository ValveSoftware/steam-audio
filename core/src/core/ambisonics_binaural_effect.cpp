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

#include "ambisonics_binaural_effect.h"

#include "array_math.h"
#include "ambisonics_panning_effect.h"
#include "profiler.h"
#include "sh.h"
#include "window_function.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

AmbisonicsBinauralEffect::AmbisonicsBinauralEffect(const AudioSettings& audioSettings,
                                                   const AmbisonicsBinauralEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mMaxOrder(effectSettings.maxOrder)
    , mHRIRSize(effectSettings.hrtf->numSamples())
    , mOverlapAddEffects(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder))
    , mOverlapAddEffectStates(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder))
    , mSpatializedChannel(2, audioSettings.frameSize)
{
    PROFILE_FUNCTION();

    init(*effectSettings.hrtf);
    reset();
}

void AmbisonicsBinauralEffect::reset()
{
    for (auto i = 0u; i < mOverlapAddEffects.size(); ++i)
    {
        mOverlapAddEffects[i]->reset();
    }

    for (auto i = 0; i < mOverlapAddEffectStates.totalSize(); ++i)
    {
        mOverlapAddEffectStates[i] = AudioEffectState::TailComplete;
    }
}

AudioEffectState AmbisonicsBinauralEffect::apply(const AmbisonicsBinauralEffectParams& params,
                                                 const AudioBuffer& in,
                                                 AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == SphericalHarmonics::numCoeffsForOrder(params.order));
    assert(out.numChannels() == 2);

    PROFILE_FUNCTION();

    if (mHRIRSize != params.hrtf->numSamples())
    {
        init(*params.hrtf);
    }

    out.makeSilent();

    auto cosine = cosf((137.9f * Math::kDegreesToRadians) / (params.order + 1.51f));

    for (auto l = 0, i = 0; l <= params.order; ++l)
    {
        auto scalar = SphericalHarmonics::legendre(l, cosine);

        for (auto m = -l; m <= l; ++m, ++i)
        {
            const complex_t* hrtfData[] = { nullptr, nullptr };
            params.hrtf->ambisonicsHRTF(i, hrtfData);

            AudioBuffer channel(in, i);

            OverlapAddConvolutionEffectParams overlapAddParams{};
            overlapAddParams.fftIR = hrtfData;

            mOverlapAddEffectStates[i] = mOverlapAddEffects[i]->apply(overlapAddParams, channel, mSpatializedChannel);

            for (auto j = 0; j < 2; ++j)
            {
                ArrayMath::scaleAccumulate(mFrameSize, mSpatializedChannel[j], scalar, out[j]);
            }
        }
    }

    for (auto i = 0; i < mOverlapAddEffectStates.totalSize(); ++i)
    {
        if (mOverlapAddEffectStates[i] == AudioEffectState::TailRemaining)
            return AudioEffectState::TailRemaining;
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState AmbisonicsBinauralEffect::tail(AudioBuffer& out)
{
    assert(out.numChannels() == 2);

    out.makeSilent();

    auto cosine = cosf((137.9f * Math::kDegreesToRadians) / (mMaxOrder + 1.51f));

    for (auto l = 0, i = 0; l <= mMaxOrder; ++l)
    {
        if (i >= mOverlapAddEffectStates.totalSize())
            break;

        auto scalar = SphericalHarmonics::legendre(l, cosine);

        for (auto m = -l; m <= l; ++m, ++i)
        {
            if (i >= mOverlapAddEffectStates.totalSize())
                break;

            mOverlapAddEffectStates[i] = mOverlapAddEffects[i]->tail(mSpatializedChannel);

            for (auto j = 0; j < 2; ++j)
            {
                ArrayMath::scaleAccumulate(mFrameSize, mSpatializedChannel[j], scalar, out[j]);
            }
        }
    }

    for (auto i = 0; i < mOverlapAddEffectStates.totalSize(); ++i)
    {
        if (mOverlapAddEffectStates[i] == AudioEffectState::TailRemaining)
            return AudioEffectState::TailRemaining;
    }

    return AudioEffectState::TailComplete;
}

int AmbisonicsBinauralEffect::numTailSamplesRemaining() const
{
    auto result = 0;

    for (auto i = 0; i < mOverlapAddEffects.size(); ++i)
    {
        result = std::max(result, mOverlapAddEffects[i]->numTailSamplesRemaining());
    }

    return result;
}

void AmbisonicsBinauralEffect::init(const HRTFDatabase& hrtf)
{
    mHRIRSize = hrtf.numSamples();

    AudioSettings audioSettings{};
    audioSettings.frameSize = mFrameSize;

    for (auto i = 0u; i < mOverlapAddEffects.size(); ++i)
    {
        OverlapAddConvolutionEffectSettings overlapAddSettings{};
        overlapAddSettings.numChannels = 2;
        overlapAddSettings.irSize = mHRIRSize;

        mOverlapAddEffects[i] = make_unique<OverlapAddConvolutionEffect>(audioSettings, overlapAddSettings);
    }
}

}
