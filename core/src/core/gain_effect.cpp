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

#include "gain_effect.h"

#include "array_math.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// GainEffect
// --------------------------------------------------------------------------------------------------------------------

GainEffect::GainEffect(const AudioSettings& audioSettings)
    : mFrameSize(audioSettings.frameSize)
{
    reset();
}

void GainEffect::reset()
{
    mPrevGain = 0.0f;
    mFirstFrame = true;
}

AudioEffectState GainEffect::apply(const GainEffectParams& params,
                                   const AudioBuffer& in,
                                   AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == 1);

    PROFILE_FUNCTION();

    if (mFirstFrame)
    {
        ArrayMath::scale(mFrameSize, in[0], params.gain, out[0]);
        mPrevGain = params.gain;
        mFirstFrame = false;
    }
    else
    {
        auto targetGain = mPrevGain + (1.0f / kNumInterpolationFrames) * (params.gain - mPrevGain);

        auto curGain = mPrevGain;
        auto dGain = (targetGain - mPrevGain) / mFrameSize;

        for (auto i = 0; i < mFrameSize; ++i)
        {
            out[0][i] = curGain * in[0][i];
            curGain += dGain;
        }

        mPrevGain = targetGain;
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState GainEffect::tailApply(const AudioBuffer& in, AudioBuffer& out)
{
    GainEffectParams prevParams{};
    prevParams.gain = mPrevGain;

    return apply(prevParams, in, out);
}

AudioEffectState GainEffect::tail(AudioBuffer& out)
{
    out.makeSilent();
    return AudioEffectState::TailComplete;
}

}
