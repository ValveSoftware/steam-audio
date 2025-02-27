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

#include "delay_effect.h"

#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DelayEffect
// --------------------------------------------------------------------------------------------------------------------

DelayEffect::DelayEffect(const AudioSettings& audioSettings,
                         const DelayEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mMaxDelayInSamples(effectSettings.maxDelayInSamples)
    , mRingBuffer(effectSettings.maxDelayInSamples)
{
    reset();
}

void DelayEffect::reset()
{
    mRingBuffer.zero();
    mWritePos = 0;
    mPrevDelayInSamples = 0.0f;
    mNumTailSamplesRemaining = 0;
    mFirstFrame = true;
}

AudioEffectState DelayEffect::apply(const DelayEffectParams& params,
                                    const AudioBuffer& in,
                                    AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == 1);

    PROFILE_FUNCTION();

    if (params.delayInSamples >= static_cast<int>(mRingBuffer.size(0)))
    {
        out.makeSilent();
        return AudioEffectState::TailComplete;
    }

    auto curDelayInSamples = mFirstFrame ? params.delayInSamples : mPrevDelayInSamples;
    auto dDelayInSamples = mFirstFrame ? 0.0f : (params.delayInSamples - mPrevDelayInSamples) / mFrameSize;

    for (auto i = 0; i < mFrameSize; ++i)
    {
        out[0][i] = 0.0f;

        mRingBuffer[mWritePos] = in[0][i];

        int delayedSampleIndex[2];
        delayedSampleIndex[0] = static_cast<int>(floorf(mWritePos - curDelayInSamples));
        delayedSampleIndex[1] = delayedSampleIndex[0] + 1;

        for (auto j = 0; j < 2; ++j)
        {
            if (delayedSampleIndex[j] < 0)
            {
                delayedSampleIndex[j] += static_cast<int>(mRingBuffer.size(0));
            }
            else if (delayedSampleIndex[j] >= static_cast<int>(mRingBuffer.size(0)))
            {
                delayedSampleIndex[j] -= static_cast<int>(mRingBuffer.size(0));
            }
        }

        float weights[2];
        weights[1] = ceilf(curDelayInSamples) - curDelayInSamples;
        weights[0] = 1.0f - weights[1];

        out[0][i] = weights[0] * mRingBuffer[delayedSampleIndex[0]] + weights[1] * mRingBuffer[delayedSampleIndex[1]];

        mWritePos = (mWritePos + 1) % mRingBuffer.size(0);

        curDelayInSamples += dDelayInSamples;
    }

    mPrevDelayInSamples = static_cast<float>(params.delayInSamples);

    mNumTailSamplesRemaining = std::max(params.delayInSamples - mFrameSize, 0);
    return (mNumTailSamplesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

AudioEffectState DelayEffect::tail(AudioBuffer& out)
{
    assert(out.numChannels() == 1);
    assert(out.numSamples() == mFrameSize);

    if (mNumTailSamplesRemaining >= static_cast<int>(mRingBuffer.size(0)))
    {
        out.makeSilent();
        return AudioEffectState::TailComplete;
    }

    for (auto i = 0; i < mFrameSize; ++i)
    {
        int delayedSampleIndex = mWritePos - mNumTailSamplesRemaining;

        if (delayedSampleIndex < 0)
        {
            delayedSampleIndex += static_cast<int>(mRingBuffer.size(0));
        }
        else if (delayedSampleIndex >= static_cast<int>(mRingBuffer.size(0)))
        {
            delayedSampleIndex -= static_cast<int>(mRingBuffer.size(0));
        }

        out[0][i] = mRingBuffer[delayedSampleIndex];

        mNumTailSamplesRemaining = std::max(mNumTailSamplesRemaining - 1, 0);
    }

    return (mNumTailSamplesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

}
