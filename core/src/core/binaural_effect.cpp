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

#include "binaural_effect.h"

#include "profiler.h"
#include "window_function.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// BinauralEffect
// --------------------------------------------------------------------------------------------------------------------

BinauralEffect::BinauralEffect(const AudioSettings& audioSettings,
                               const BinauralEffectSettings& effectSettings)
    : mSamplingRate(audioSettings.samplingRate)
    , mFrameSize(audioSettings.frameSize)
    , mPartialDownmixed(2, audioSettings.frameSize)
    , mPartialOutput(2, audioSettings.frameSize)
{
    PROFILE_FUNCTION();

    init(*effectSettings.hrtf);
    reset();
}

void BinauralEffect::reset()
{
    mOverlapAddEffect->reset();
}

AudioEffectState BinauralEffect::apply(const BinauralEffectParams& params,
                                       const AudioBuffer& in,
                                       AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 2 || in.numChannels() == 1);
    assert(out.numChannels() == 2);

    PROFILE_FUNCTION();

    if (mHRIRSize != params.hrtf->numSamples())
    {
        init(*params.hrtf);
    }

    out.makeSilent();

    const complex_t* hrtfData[] = { nullptr, nullptr };
    int peakDelayInSamples[] = { 0, 0 };
    auto enableSpatialBlend = (in.numChannels() == 2 && params.spatialBlend < 1.0f);

    auto& _hrtf = const_cast<HRTFDatabase&>(*params.hrtf);

    if (params.interpolation == HRTFInterpolation::NearestNeighbor)
    {
        _hrtf.nearestHRTF(*params.direction, hrtfData, params.spatialBlend, params.phaseType, mInterpolatedHRTF.data(), peakDelayInSamples);

        if (enableSpatialBlend)
        {
            hrtfData[0] = mInterpolatedHRTF[0];
            hrtfData[1] = mInterpolatedHRTF[1];
        }
    }
    else if (params.interpolation == HRTFInterpolation::Bilinear)
    {
        _hrtf.interpolatedHRTF(*params.direction, mInterpolatedHRTF.data(), params.spatialBlend, params.phaseType, peakDelayInSamples);

        hrtfData[0] = mInterpolatedHRTF[0];
        hrtfData[1] = mInterpolatedHRTF[1];
    }

    auto overlapAddEffectState = AudioEffectState::TailComplete;

    if (in.numChannels() == 2)
    {
        for (auto i = 0; i < mFrameSize; ++i)
        {
            mPartialDownmixed[0][i] = (1.0f - 0.5f * params.spatialBlend) * in[0][i] + (0.5f * params.spatialBlend) * in[1][i];
            mPartialDownmixed[1][i] = (1.0f - 0.5f * params.spatialBlend) * in[1][i] + (0.5f * params.spatialBlend) * in[0][i];
        }

        OverlapAddConvolutionEffectParams overlapAddParams{};
        overlapAddParams.fftIR = hrtfData;
        overlapAddParams.multipleInputs = (params.spatialBlend < 1.0f);

        overlapAddEffectState = mOverlapAddEffect->apply(overlapAddParams, mPartialDownmixed, out);
    }
    else
    {
        OverlapAddConvolutionEffectParams overlapAddParams{};
        overlapAddParams.fftIR = hrtfData;

        overlapAddEffectState = mOverlapAddEffect->apply(overlapAddParams, in, out);
    }

    if (params.peakDelays)
    {
        for (auto i = 0; i < 2; ++i)
        {
            params.peakDelays[i] = static_cast<float>(peakDelayInSamples[i]) / mSamplingRate;
        }
    }

    return overlapAddEffectState;
}

AudioEffectState BinauralEffect::tail(AudioBuffer& out)
{
    return mOverlapAddEffect->tail(out);
}

void BinauralEffect::init(const HRTFDatabase& hrtf)
{
    PROFILE_FUNCTION();

    mHRIRSize = hrtf.numSamples();

    AudioSettings audioSettings{};
    audioSettings.samplingRate = mSamplingRate;
    audioSettings.frameSize = mFrameSize;

    OverlapAddConvolutionEffectSettings overlapAddSettings{};
    overlapAddSettings.numChannels = 2;
    overlapAddSettings.irSize = mHRIRSize;

    mOverlapAddEffect = make_unique<OverlapAddConvolutionEffect>(audioSettings, overlapAddSettings);

    mInterpolatedHRTF.resize(2, hrtf.numSpectrumSamples());
}

}
