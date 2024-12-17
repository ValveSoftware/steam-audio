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

#include "reverb_effect.h"

#include "array_math.h"
#include "delay.h"
#include "profiler.h"
#include "context.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// ReverbEffect
// --------------------------------------------------------------------------------------------------------------------

ReverbEffect::ReverbEffect(const AudioSettings& audioSettings)
    : mSamplingRate(audioSettings.samplingRate)
    , mFrameSize(audioSettings.frameSize)
{
#if defined(IPL_ENABLE_FLOAT8)
    mApplyDispatch = (gSIMDLevel() >= SIMDLevel::AVX) ? &ReverbEffect::apply_float8 : &ReverbEffect::apply_float4;
    mTailDispatch = (gSIMDLevel() >= SIMDLevel::AVX) ? &ReverbEffect::tail_float8 : &ReverbEffect::tail_float4;
#else
    mApplyDispatch = &ReverbEffect::apply_float4;
    mTailDispatch = &ReverbEffect::tail_float4;
#endif

    calcDelaysForReverbTime(10.0f);
    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].resize(mDelayValues[i], audioSettings.frameSize);
    }

    mAllpass[0].resize(225, 0.5f, 0);
    mAllpass[1].resize(341, 0.5f, 0);
    mAllpass[2].resize(441, 0.5f, 0);
    mAllpass[3].resize(556, 0.5f, 0);

    mXOld.resize(kNumDelays, audioSettings.frameSize);
    mXNew.resize(kNumDelays, audioSettings.frameSize);

    reset();
}

void ReverbEffect::reset()
{
    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].reset();
    }

    for (auto i = 0; i < kNumAllpasses; ++i)
    {
        mAllpass[i].reset();
    }

    mCurrent = 0;
    mIsFirstFrame = true;

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mPrevReverb.reverbTimes[i] = 0.1f;
    }

    mNumTailFramesRemaining = 0;
}

AudioEffectState ReverbEffect::apply(const ReverbEffectParams& params,
                                     const AudioBuffer& in,
                                     AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);

    out.makeSilent();

    (this->*mApplyDispatch)(params.reverb->reverbTimes, in[0], out[0]);

    if (params.reverb)
    {
        memcpy(&mPrevReverb, params.reverb, sizeof(Reverb));
    }

    const auto* reverbTimes = params.reverb->reverbTimes;
    auto maxReverbTime = std::max({reverbTimes[0], reverbTimes[1], reverbTimes[2]});
    mNumTailFramesRemaining = 2 * static_cast<int>(ceilf((maxReverbTime * mSamplingRate) / mFrameSize)); // fixme: why 2x?

    return (mNumTailFramesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

AudioEffectState ReverbEffect::tailApply(const AudioBuffer& in, AudioBuffer& out)
{
    ReverbEffectParams prevParams{};
    prevParams.reverb = &mPrevReverb;

    return apply(prevParams, in, out);
}

AudioEffectState ReverbEffect::tail(AudioBuffer& out)
{
    out.makeSilent();

    (this->*mTailDispatch)(out[0]);

    mNumTailFramesRemaining--;
    return (mNumTailFramesRemaining > 0) ? AudioEffectState::TailRemaining : AudioEffectState::TailComplete;
}

void ReverbEffect::apply_float4(const float* reverbTimes,
                                const float* in,
                                float* out)
{
    PROFILE_FUNCTION();

    float clampedReverbTimes[Bands::kNumBands];
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        clampedReverbTimes[i] = std::max(0.1f, reverbTimes[i]);
    }

    memset(out, 0, mFrameSize * sizeof(float));

    float const lowCutoff[Bands::kNumBands] = {20.0f, 500.0f, 5000.0f};
    float const highCutoff[Bands::kNumBands] = {500.0f, 5000.0f, 22000.0f};

    for (auto i = 0; i < kNumDelays; ++i)
    {
        float absorptiveGains[Bands::kNumBands];
        calcAbsorptiveGains(clampedReverbTimes, mDelayValues[i], absorptiveGains);

        IIR iir[Bands::kNumBands];
        iir[0] = IIR::lowShelf(highCutoff[0], absorptiveGains[0], mSamplingRate);
        iir[1] = IIR::peaking(lowCutoff[1], highCutoff[1], absorptiveGains[1], mSamplingRate);
        iir[2] = IIR::highShelf(lowCutoff[2], absorptiveGains[2], mSamplingRate);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].setFilter(iir[j]);
        }
    }

    float toneCorrectionGains[Bands::kNumBands];
    calcToneCorrectionGains(clampedReverbTimes, toneCorrectionGains);

    IIR iir[Bands::kNumBands];
    iir[0] = IIR::lowShelf(highCutoff[0], toneCorrectionGains[0], mSamplingRate);
    iir[1] = IIR::peaking(lowCutoff[1], highCutoff[1], toneCorrectionGains[1], mSamplingRate);
    iir[2] = IIR::highShelf(lowCutoff[2], toneCorrectionGains[2], mSamplingRate);

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].setFilter(iir[i]);
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].get(mFrameSize, mXOld[i]);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].apply(mFrameSize, mXOld[i], mXOld[i]);
        }
    }

    float4_t xOld[kNumDelays];
    float4_t xNew[kNumDelays];
    for (auto i = 0; i < mFrameSize; i += 4)
    {
        for (auto j = 0; j < kNumDelays; ++j)
        {
            xOld[j] = float4::loadu(&mXOld[j][i]);
        }

        multiplyHadamardMatrix(xOld, xNew);

        for (auto j = 0; j < kNumDelays; ++j)
        {
            float4::storeu(&mXNew[j][i], xNew[j]);
        }
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXNew[i], in, mXNew[i]);

        mDelayLines[i].put(mFrameSize, mXNew[i]);
    }

    for (auto i = 1; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXOld[0], mXOld[i], mXOld[0]);
    }

    ArrayMath::scale(mFrameSize, mXOld[0], 1.0f / kNumDelays, mXOld[0]);

    for (auto i = 0; i < mFrameSize; i += 4)
    {
        xOld[0] = float4::loadu(&mXOld[0][i]);

        for (auto j = 0; j < kNumAllpasses; ++j)
        {
            xOld[0] = mAllpass[j].apply(xOld[0]);
        }

        float4::storeu(&out[i], xOld[0]);
    }

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].apply(mFrameSize, out, out);
    }
}

void ReverbEffect::tail_float4(float* out)
{
    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].get(mFrameSize, mXOld[i]);

        for (auto j = 0; j < Bands::kNumBands; ++j)
        {
            mAbsorptive[i][j][mCurrent].apply(mFrameSize, mXOld[i], mXOld[i]);
        }
    }

    float4_t xOld[kNumDelays];
    float4_t xNew[kNumDelays];
    for (auto i = 0; i < mFrameSize; i += 4)
    {
        for (auto j = 0; j < kNumDelays; ++j)
        {
            xOld[j] = float4::loadu(&mXOld[j][i]);
        }

        multiplyHadamardMatrix(xOld, xNew);

        for (auto j = 0; j < kNumDelays; ++j)
        {
            float4::storeu(&mXNew[j][i], xNew[j]);
        }
    }

    for (auto i = 0; i < kNumDelays; ++i)
    {
        mDelayLines[i].put(mFrameSize, mXNew[i]);
    }

    for (auto i = 1; i < kNumDelays; ++i)
    {
        ArrayMath::add(mFrameSize, mXOld[0], mXOld[i], mXOld[0]);
    }

    ArrayMath::scale(mFrameSize, mXOld[0], 1.0f / kNumDelays, mXOld[0]);

    for (auto i = 0; i < mFrameSize; i += 4)
    {
        xOld[0] = float4::loadu(&mXOld[0][i]);

        for (auto j = 0; j < kNumAllpasses; ++j)
        {
            xOld[0] = mAllpass[j].apply(xOld[0]);
        }

        float4::storeu(&out[i], xOld[0]);
    }

    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mToneCorrection[i][mCurrent].apply(mFrameSize, out, out);
    }
}

void ReverbEffect::calcDelaysForReverbTime(float reverbTime)
{
    int const primes[kNumDelays] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};

    auto delaySum = 0.15f * reverbTime * mSamplingRate;
    auto delayMin = static_cast<int>(delaySum / kNumDelays);

    for (auto i = 0; i < kNumDelays; ++i)
    {
        auto randomOffset = rand() % 101;
        mDelayValues[i] = nextPowerOfPrime(delayMin + randomOffset, primes[i]);
    }
}

void ReverbEffect::calcAbsorptiveGains(const float* reverbTimes,
                                       int delay,
                                       float* gains)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        // Note: Limit the gains to avoid instability in IIR filter.
        gains[i] = std::max(expf(-(6.91f * delay) / (reverbTimes[i] * mSamplingRate)), 1e-8f);
    }
}

void ReverbEffect::calcToneCorrectionGains(const float* reverbTimes,
                                           float* gains)
{
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        gains[i] = sqrtf(1.0f / reverbTimes[i]);
    }

    auto maxGain = std::max({gains[0], gains[1], gains[2]});
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        gains[i] /= maxGain;
    }
}

void ReverbEffect::multiplyHadamardMatrix(const float4_t* in,
                                          float4_t* out)
{
    out[0]  = in[0] + in[1] + in[2] + in[3] + in[4] + in[5] + in[6] + in[7] + in[8] + in[9] + in[10] + in[11] + in[12] + in[13] + in[14] + in[15];
    out[1]  = in[0] - in[1] + in[2] - in[3] + in[4] - in[5] + in[6] - in[7] + in[8] - in[9] + in[10] - in[11] + in[12] - in[13] + in[14] - in[15];
    out[2]  = in[0] + in[1] - in[2] - in[3] + in[4] + in[5] - in[6] - in[7] + in[8] + in[9] - in[10] - in[11] + in[12] + in[13] - in[14] - in[15];
    out[3]  = in[0] - in[1] - in[2] + in[3] + in[4] - in[5] - in[6] + in[7] + in[8] - in[9] - in[10] + in[11] + in[12] - in[13] - in[14] + in[15];
    out[4]  = in[0] + in[1] + in[2] + in[3] - in[4] - in[5] - in[6] - in[7] + in[8] + in[9] + in[10] + in[11] - in[12] - in[13] - in[14] - in[15];
    out[5]  = in[0] - in[1] + in[2] - in[3] - in[4] + in[5] - in[6] + in[7] + in[8] - in[9] + in[10] - in[11] - in[12] + in[13] - in[14] + in[15];
    out[6]  = in[0] + in[1] - in[2] - in[3] - in[4] - in[5] + in[6] + in[7] + in[8] + in[9] - in[10] - in[11] - in[12] - in[13] + in[14] + in[15];
    out[7]  = in[0] - in[1] - in[2] + in[3] - in[4] + in[5] + in[6] - in[7] + in[8] - in[9] - in[10] + in[11] - in[12] + in[13] + in[14] - in[15];
    out[8]  = in[0] + in[1] + in[2] + in[3] + in[4] + in[5] + in[6] + in[7] - in[8] - in[9] - in[10] - in[11] - in[12] - in[13] - in[14] - in[15];
    out[9]  = in[0] - in[1] + in[2] - in[3] + in[4] - in[5] + in[6] - in[7] - in[8] + in[9] - in[10] + in[11] - in[12] + in[13] - in[14] + in[15];
    out[10] = in[0] + in[1] - in[2] - in[3] + in[4] + in[5] - in[6] - in[7] - in[8] - in[9] + in[10] + in[11] - in[12] - in[13] + in[14] + in[15];
    out[11] = in[0] - in[1] - in[2] + in[3] + in[4] - in[5] - in[6] + in[7] - in[8] + in[9] + in[10] - in[11] - in[12] + in[13] + in[14] - in[15];
    out[12] = in[0] + in[1] + in[2] + in[3] - in[4] - in[5] - in[6] - in[7] - in[8] - in[9] - in[10] - in[11] + in[12] + in[13] + in[14] + in[15];
    out[13] = in[0] - in[1] + in[2] - in[3] - in[4] + in[5] - in[6] + in[7] - in[8] + in[9] - in[10] + in[11] + in[12] - in[13] + in[14] - in[15];
    out[14] = in[0] + in[1] - in[2] - in[3] - in[4] - in[5] + in[6] + in[7] - in[8] - in[9] + in[10] + in[11] + in[12] + in[13] - in[14] - in[15];
    out[15] = in[0] - in[1] - in[2] + in[3] - in[4] + in[5] + in[6] - in[7] - in[8] + in[9] + in[10] - in[11] + in[12] - in[13] - in[14] + in[15];

    for (auto i = 0; i < kNumDelays; ++i)
    {
        out[i] = float4::mul(out[i], float4::set1(0.25f));
    }
}

int ReverbEffect::nextPowerOfPrime(int x,
                                   int p)
{
    auto m = static_cast<int>(roundf(logf(static_cast<float>(x)) / logf(static_cast<float>(p))));
    return static_cast<int>(pow(p, m));
}

}
