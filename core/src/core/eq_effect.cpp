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

#include "eq_effect.h"
#include "math_functions.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EQEffect
// --------------------------------------------------------------------------------------------------------------------

EQEffect::EQEffect(const AudioSettings& audioSettings)
    : mSamplingRate(audioSettings.samplingRate)
    , mFrameSize(audioSettings.frameSize)
    , mTemp(audioSettings.frameSize)
{
    reset();
}

void EQEffect::reset()
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mPrevGains[i] = 1.0f;
    }

    setFilterGains(0, mPrevGains);
    setFilterGains(1, mPrevGains);

    mCurrent = 0;

    mFirstFrame = true;
}

AudioEffectState EQEffect::apply(const EQEffectParams& params,
                                 const AudioBuffer& in,
                                 AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == 1);

    PROFILE_FUNCTION();

    if (mFirstFrame)
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            mPrevGains[i] = params.gains[i];
        }

        setFilterGains(mCurrent, params.gains);

        mFirstFrame = false;
    }

    if (mPrevGains[0] != params.gains[0] || mPrevGains[1] != params.gains[1] || mPrevGains[2] != params.gains[2])
    {
        auto previous = mCurrent;
        mCurrent = 1 - mCurrent;

        setFilterGains(mCurrent, params.gains);

        mFilters[0][mCurrent].copyState(mFilters[0][previous]);
        mFilters[1][mCurrent].copyState(mFilters[1][previous]);
        mFilters[2][mCurrent].copyState(mFilters[2][previous]);

        applyFilterCascade(previous, in[0], mTemp.data());
        applyFilterCascade(mCurrent, in[0], out[0]);

        for (auto i = 0; i < mFrameSize; ++i)
        {
            auto weight = static_cast<float>(i) / static_cast<float>(mFrameSize);
            out[0][i] = weight * out[0][i] + (1.0f - weight) * mTemp[i];
        }

        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            mPrevGains[i] = params.gains[i];
        }
    }
    else
    {
        applyFilterCascade(mCurrent, in[0], out[0]);
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState EQEffect::tailApply(const AudioBuffer& in, AudioBuffer& out)
{
    EQEffectParams prevParams{};
    prevParams.gains = mPrevGains;

    return apply(prevParams, in, out);
}

AudioEffectState EQEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

void EQEffect::setFilterGains(int index,
                              const float* gains)
{
    mFilters[0][index].setFilter(IIR::lowShelf(Bands::kHighCutoffFrequencies[0], gains[0], mSamplingRate));
    mFilters[1][index].setFilter(IIR::peaking(Bands::kLowCutoffFrequencies[1], Bands::kHighCutoffFrequencies[1], gains[1], mSamplingRate));
    mFilters[2][index].setFilter(IIR::highShelf(Bands::kLowCutoffFrequencies[2], gains[2], mSamplingRate));
}

void EQEffect::applyFilterCascade(int index,
                                  const float* in,
                                  float* out)
{
    mFilters[0][index].apply(mFrameSize, in, out);
    mFilters[1][index].apply(mFrameSize, out, out);
    mFilters[2][index].apply(mFrameSize, out, out);
}

void EQEffect::normalizeGains(float* eqGains,
                              float& overallGain)
{
    const auto kMaxEQGain = 0.0625f;

    auto maxGain = std::max({eqGains[0], eqGains[1], eqGains[2]});

    if (maxGain < std::numeric_limits<float>::min())
    {
        overallGain = 0.0f;
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            eqGains[i] = 1.0f;
        }
    }
    else
    {
        for (auto i = 0; i < Bands::kNumBands; ++i)
        {
            eqGains[i] /= maxGain;
            eqGains[i] = std::max(eqGains[i], kMaxEQGain);
        }

        overallGain *= maxGain;
    }
}

}
