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

#include "overlap_save_convolution_effect.h"

#include "array_math.h"
#include "float4.h"
#include "math_functions.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveFIR
// --------------------------------------------------------------------------------------------------------------------

OverlapSaveFIR::OverlapSaveFIR(int numChannels,
                               int irSize,
                               int frameSize)
{
    auto numBlocks = OverlapSaveConvolutionEffect::numBlocks(frameSize, irSize);
    auto numSpectrumSamples = Math::nextpow2(2 * frameSize) / 2 + 1;

    mData.resize(numChannels, numBlocks, numSpectrumSamples);

    reset();
}

void OverlapSaveFIR::reset()
{
    memset(mData.flatData(), 0, mData.totalSize() * sizeof(complex_t));
}


// --------------------------------------------------------------------------------------------------------------------
// OverlapSavePartitioner
// --------------------------------------------------------------------------------------------------------------------

OverlapSavePartitioner::OverlapSavePartitioner(int frameSize)
    : mFrameSize(frameSize)
    , mFFT(2 * frameSize)
    , mTempIRBlock(mFFT.numRealSamples)
{
    mTempIRBlock.zero();
}

void OverlapSavePartitioner::partition(const ImpulseResponse& ir,
                                       int numChannels,
                                       int numSamples,
                                       OverlapSaveFIR& fftIR)
{
    PROFILE_FUNCTION();

    for (auto i = 0; i < numChannels; ++i)
    {
        auto numSamplesLeft = std::min(numSamples, ir.numSamples());
        for (auto j = 0; j < fftIR.numBlocks(); ++j)
        {
            auto numSamplesToCopy = std::min(mFrameSize, numSamplesLeft);
            numSamplesLeft -= numSamplesToCopy;

            if (numSamplesToCopy <= 0)
                break;

            memcpy(mTempIRBlock.data(), &ir[i][j * mFrameSize], numSamplesToCopy * sizeof(float));
            mFFT.applyForward(mTempIRBlock.data(), fftIR[i][j]);
        }
    }
}


// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionEffect
// --------------------------------------------------------------------------------------------------------------------

OverlapSaveConvolutionEffect::OverlapSaveConvolutionEffect(const AudioSettings& audioSettings,
                                                           const OverlapSaveConvolutionEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mIRSize(effectSettings.irSize)
    , mNumChannels(effectSettings.numChannels)
    , mFFT(2 * audioSettings.frameSize)
    , mDryBlock(mFFT.numRealSamples)
    , mFFTDryBlocks(OverlapSaveConvolutionEffect::numBlocks(audioSettings.frameSize, effectSettings.irSize), mFFT.numComplexSamples)
    , mFFTWet(effectSettings.numChannels, mFFT.numComplexSamples)
    , mPrevFFTWet(effectSettings.numChannels, mFFT.numComplexSamples)
    , mWet(effectSettings.numChannels, mFFT.numRealSamples)
    , mPrevWet(effectSettings.numChannels, mFFT.numRealSamples)
{
    mPrevFFTIR = ipl::make_unique<OverlapSaveFIR>(mNumChannels, mIRSize, mFrameSize);

    reset();
}

void OverlapSaveConvolutionEffect::reset()
{
    mDryBlock.zero();
    mFFTDryBlocks.zero();
    mDryBlockIndex = 0;
    mNumTailBlocksRemaining = 0;
    mPrevFFTIR->reset();
}

AudioEffectState OverlapSaveConvolutionEffect::apply(const OverlapSaveConvolutionEffectParams& params,
                                                     const AudioBuffer& in,
                                                     AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);
    assert(out.numChannels() == mNumChannels);

    PROFILE_FUNCTION();

    auto crossfade = apply(params, in);

    for (auto i = 0; i < params.numChannels; ++i)
    {
        mFFT.applyInverse(mFFTWet[i], mWet[i]);
    }

    if (crossfade)
    {
        for (auto i = 0; i < params.numChannels; ++i)
        {
            mFFT.applyInverse(mPrevFFTWet[i], mPrevWet[i]);

            for (auto j = 0; j < mFrameSize; ++j)
            {
                auto weight = static_cast<float>(j) / static_cast<float>(mFrameSize);
                mWet[i][j + mFrameSize] = (1.0f - weight) * mPrevWet[i][j + mFrameSize] + weight * mWet[i][j + mFrameSize];
            }
        }
    }

    out.makeSilent();

    for (auto i = 0; i < params.numChannels; ++i)
    {
        memcpy(out[i], &mWet[i][mFrameSize], mFrameSize * sizeof(float));
    }

    return (mNumTailBlocksRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

AudioEffectState OverlapSaveConvolutionEffect::apply(const OverlapSaveConvolutionEffectParams& params,
                                                     const AudioBuffer& in,
                                                     OverlapSaveConvolutionMixer& mixer)
{
    assert(in.numChannels() == 1);

    PROFILE_FUNCTION();

    auto crossfade = apply(params, in);

    mixer.mix(mFFTWet.data(), (crossfade) ? mPrevFFTWet.data() : nullptr);

    return (mNumTailBlocksRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

bool OverlapSaveConvolutionEffect::apply(const OverlapSaveConvolutionEffectParams& params,
                                         const AudioBuffer& in)
{
    memcpy(&mDryBlock[0], &mDryBlock[mFrameSize], mFrameSize * sizeof(float));
    memcpy(&mDryBlock[mFrameSize], in[0], mFrameSize * sizeof(float));

    --mDryBlockIndex;
    if (mDryBlockIndex < 0)
    {
        mDryBlockIndex = static_cast<int>(mFFTDryBlocks.size(0)) - 1;
    }

    mFFT.applyForward(mDryBlock.data(), mFFTDryBlocks[mDryBlockIndex]);

    auto numBlocks = OverlapSaveConvolutionEffect::numBlocks(mFrameSize, mIRSize);

    auto crossfade = params.fftIR->updateReadBuffer();
    if (crossfade)
    {
        mFFTWet.zero();
        for (auto i = 0; i < params.numChannels; ++i)
        {
            for (auto j = 0; j < numBlocks; ++j)
            {
                auto index = static_cast<int>((mDryBlockIndex + j) % mFFTDryBlocks.size(0));
                ArrayMath::multiplyAccumulate(static_cast<int>(mFFTDryBlocks.size(1)), mFFTDryBlocks[index], (*params.fftIR->readBuffer)[i][j], mFFTWet[i]);
            }
        }

        mPrevFFTWet.zero();
        for (auto i = 0; i < params.numChannels; ++i)
        {
            for (auto j = 0; j < numBlocks; ++j)
            {
                auto index = static_cast<int>((mDryBlockIndex + j) % mFFTDryBlocks.size(0));
                ArrayMath::multiplyAccumulate(static_cast<int>(mFFTDryBlocks.size(1)), mFFTDryBlocks[index], (*mPrevFFTIR)[i][j], mPrevFFTWet[i]);
            }
        }

        mPrevFFTIR.swap(params.fftIR->readBuffer);
    }
    else
    {
        mFFTWet.zero();
        for (auto i = 0; i < params.numChannels; ++i)
        {
            for (auto j = 0; j < numBlocks; ++j)
            {
                auto index = static_cast<int>((mDryBlockIndex + j) % mFFTDryBlocks.size(0));
                ArrayMath::multiplyAccumulate(static_cast<int>(mFFTDryBlocks.size(1)), mFFTDryBlocks[index], (*mPrevFFTIR)[i][j], mFFTWet[i]);
            }
        }
    }

    mNumTailBlocksRemaining = numBlocks - 1;

    return crossfade;
}

AudioEffectState OverlapSaveConvolutionEffect::tail(AudioBuffer& out)
{
    assert(out.numChannels() <= mNumChannels);
    assert(out.numSamples() == mFrameSize);

    out.makeSilent();

    tail();

    auto numChannels = std::min(out.numChannels(), mNumChannels);
    for (auto i = 0; i < numChannels; ++i)
    {
        mFFT.applyInverse(mFFTWet[i], mWet[i]);
        memcpy(out[i], &mWet[i][mFrameSize], mFrameSize * sizeof(float));
    }

    return (mNumTailBlocksRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

AudioEffectState OverlapSaveConvolutionEffect::tail(OverlapSaveConvolutionMixer& mixer)
{
    tail();
    mixer.mix(mFFTWet.data(), nullptr);
    return (mNumTailBlocksRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

void OverlapSaveConvolutionEffect::tail()
{
    mFFTWet.zero();

    auto numBlocks = OverlapSaveConvolutionEffect::numBlocks(mFrameSize, mIRSize);
    auto offset = numBlocks - mNumTailBlocksRemaining;

    for (auto i = 0; i < mNumChannels; ++i)
    {
        for (auto j = 0; j < mNumTailBlocksRemaining; ++j)
        {
            auto index = static_cast<int>((mDryBlockIndex + j) % mFFTDryBlocks.size(0));
            ArrayMath::multiplyAccumulate(static_cast<int>(mFFTDryBlocks.size(1)), mFFTDryBlocks[index], (*mPrevFFTIR)[i][j + offset], mFFTWet[i]);
        }
    }

    mNumTailBlocksRemaining--;
}

int OverlapSaveConvolutionEffect::numBlocks(int frameSize,
                                            int irSize)
{
    return static_cast<int>(ceilf(static_cast<float>(irSize) / static_cast<float>(frameSize)));
}


// --------------------------------------------------------------------------------------------------------------------
// OverlapSaveConvolutionMixer
// --------------------------------------------------------------------------------------------------------------------

OverlapSaveConvolutionMixer::OverlapSaveConvolutionMixer(const AudioSettings& audioSettings,
                                                         const OverlapSaveConvolutionEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mNumChannels(effectSettings.numChannels)
    , mFFT(2 * audioSettings.frameSize)
    , mFFTWet(effectSettings.numChannels, mFFT.numComplexSamples)
    , mPrevFFTWet(effectSettings.numChannels, mFFT.numComplexSamples)
    , mWet(effectSettings.numChannels, mFFT.numRealSamples)
    , mPrevWet(effectSettings.numChannels, mFFT.numRealSamples)
{
    reset();
}

void OverlapSaveConvolutionMixer::reset()
{
    mFFTWet.zero();
    mPrevFFTWet.zero();

    mWet.zero();
    mPrevWet.zero();
}

void OverlapSaveConvolutionMixer::mix(const complex_t* const* fftWet,
                                      const complex_t* const* fftWetPrev)
{
    for (auto i = 0; i < mNumChannels; ++i)
    {
        ArrayMath::add(mFFT.numComplexSamples, fftWet[i], mFFTWet[i], mFFTWet[i]);
        ArrayMath::add(mFFT.numComplexSamples, (fftWetPrev) ? fftWetPrev[i] : fftWet[i], mPrevFFTWet[i], mPrevFFTWet[i]);
    }
}

void OverlapSaveConvolutionMixer::apply(const OverlapSaveConvolutionMixerParams& params,
                                        AudioBuffer& out)
{
    out.makeSilent();

    for (auto i = 0; i < params.numChannels; ++i)
    {
        mFFT.applyInverse(mFFTWet[i], mWet[i]);
        mFFT.applyInverse(mPrevFFTWet[i], mPrevWet[i]);

        for (auto j = 0; j < mFrameSize; ++j)
        {
            auto weight = static_cast<float>(j) / static_cast<float>(mFrameSize);
            mWet[i][j + mFrameSize] = (1.0f - weight) * mPrevWet[i][j + mFrameSize] + weight * mWet[i][j + mFrameSize];
        }

        memcpy(out[i], &mWet[i][mFrameSize], mFrameSize * sizeof(float));
    }

    reset();
}

}
