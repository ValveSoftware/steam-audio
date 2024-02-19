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

#include "overlap_add_convolution_effect.h"

#include "array_math.h"
#include "profiler.h"
#include "window_function.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OverlapAddConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

OverlapAddConvolutionEffect::OverlapAddConvolutionEffect(const AudioSettings& audioSettings,
                                                         const OverlapAddConvolutionEffectSettings& effectSettings)
    : mNumChannels(effectSettings.numChannels)
    , mIRSize(effectSettings.irSize)
    , mFrameSize(audioSettings.frameSize)
    , mWindow(audioSettings.frameSize + audioSettings.frameSize / 4)
    , mFFT(static_cast<int>(mWindow.size(0)) + effectSettings.irSize - 1)
    , mWindowedDry(mFFT.numRealSamples)
    , mFFTWindowedDry(mFFT.numComplexSamples)
    , mDry(effectSettings.numChannels, static_cast<int>(mWindow.size(0)))
    , mFFTWet(effectSettings.numChannels, mFFT.numComplexSamples)
    , mWet(effectSettings.numChannels, mFFT.numRealSamples)
    , mOverlap(effectSettings.numChannels, mFFT.numRealSamples - audioSettings.frameSize)
{
    WindowFunction::tukey(audioSettings.frameSize, audioSettings.frameSize / 4, mWindow.data());
    mWindowedDry.zero();
    reset();
}

void OverlapAddConvolutionEffect::reset()
{
    mDry.zero();
    mOverlap.zero();

    mNumTailSamplesRemaining = 0;
}

AudioEffectState OverlapAddConvolutionEffect::apply(const OverlapAddConvolutionEffectParams& params,
                                                    const AudioBuffer& in,
                                                    AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1 || in.numChannels() == mNumChannels);
    assert(out.numChannels() == mNumChannels);

    PROFILE_FUNCTION();

    if (in.numChannels() > 1 && params.multipleInputs)
    {
        for (auto i = 0; i < mNumChannels; ++i)
        {
            memcpy(mDry[i], &mDry[i][mFrameSize], (mFrameSize / 4) * sizeof(float));
            memcpy(&mDry[i][mFrameSize / 4], in[i], mFrameSize * sizeof(float));

            ArrayMath::multiply(static_cast<int>(mWindow.size(0)), mDry[i], mWindow.data(), mWindowedDry.data());
            mFFT.applyForward(mWindowedDry.data(), mFFTWindowedDry.data());

            ArrayMath::multiply(mFFT.numComplexSamples, mFFTWindowedDry.data(), params.fftIR[i], mFFTWet[i]);
        }
    }
    else
    {
        // Populated to avoid any discontinuity when using per channel vs. all channel apply.
        for (auto i = 0; i < mNumChannels; ++i)
        {
            memcpy(mDry[i], &mDry[i][mFrameSize], (mFrameSize / 4) * sizeof(float));
            memcpy(&mDry[i][mFrameSize / 4], in[0], mFrameSize * sizeof(float));
        }

        ArrayMath::multiply(static_cast<int>(mWindow.size(0)), mDry[0], mWindow.data(), mWindowedDry.data());
        mFFT.applyForward(mWindowedDry.data(), mFFTWindowedDry.data());

        for (auto i = 0; i < mNumChannels; ++i)
        {
            ArrayMath::multiply(mFFT.numComplexSamples, mFFTWindowedDry.data(), params.fftIR[i], mFFTWet[i]);
        }
    }

    for (auto i = 0; i < mNumChannels; ++i)
    {
        mFFT.applyInverse(mFFTWet[i], mWet[i]);

        ArrayMath::add(static_cast<int>(mOverlap.size(1)), mWet[i], mOverlap[i], mWet[i]);

        memcpy(mOverlap[i], &mWet[i][mFrameSize], mOverlap.size(1) * sizeof(float));
        memcpy(out[i], mWet[i], mFrameSize * sizeof(float));
    }

    mNumTailSamplesRemaining = static_cast<int>(mOverlap.size(1));
    return (mNumTailSamplesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

AudioEffectState OverlapAddConvolutionEffect::tail(AudioBuffer& out)
{
    assert(out.numChannels() == mNumChannels);
    assert(out.numSamples() == mFrameSize);

    out.makeSilent();

    auto startIndex = static_cast<int>(mOverlap.size(1)) - mNumTailSamplesRemaining;
    auto endIndex = std::min(startIndex + mFrameSize, static_cast<int>(mOverlap.size(1)));
    auto numSamplesToCopy = endIndex - startIndex;

    for (auto i = 0; i < mNumChannels; ++i)
    {
        memcpy(out[i], &mOverlap[i][startIndex], numSamplesToCopy * sizeof(float));
    }

    mNumTailSamplesRemaining -= numSamplesToCopy;
    return (mNumTailSamplesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

}
