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

#include "hybrid_reverb_estimator.h"

#include "array_math.h"
#include "profiler.h"
#include "propagation_medium.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HybridReverbEstimator
// --------------------------------------------------------------------------------------------------------------------

HybridReverbEstimator::HybridReverbEstimator(float maxDuration,
                                             int samplingRate,
                                             int frameSize)
    : mMaxDuration(maxDuration)
    , mSamplingRate(samplingRate)
    , mFrameSize(frameSize)
    , mGainEffect(AudioSettings{samplingRate, frameSize})
    , mEQEffect(AudioSettings{samplingRate, frameSize})
    , mReverbEffect(AudioSettings{samplingRate, frameSize})
    , mTempFrame(2, frameSize)
    , mReverbIR(static_cast<int>(ceilf(maxDuration * samplingRate)))
    , mBandIR(static_cast<int>(ceilf(maxDuration * samplingRate)))
{
    IIR filters[Bands::kNumBands];
    IIR::bandFilters(filters, samplingRate);
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mBandpassFilters[i].setFilter(filters[i]);
    }
}

void HybridReverbEstimator::estimate(const Reverb& reverb,
                                     ImpulseResponse& impulseResponse,
                                     float transitionTime,
                                     float overlapFraction,
                                     int order,
                                     float* eqCoeffs,
                                     int& delay)
{
    PROFILE_FUNCTION();

    auto numChannels = SphericalHarmonics::numCoeffsForOrder(order);

    // If the transition time is greater than the length of the IR, clamp it.
    auto transitionTimeInSamples = static_cast<int>(ceilf(transitionTime * mSamplingRate));
    if (transitionTimeInSamples >= impulseResponse.numSamples())
    {
        transitionTimeInSamples = impulseResponse.numSamples() - 1;
        transitionTime = static_cast<float>(transitionTimeInSamples) / mSamplingRate;
    }

    transitionTimeInSamples = static_cast<int>(ceilf(transitionTime * mSamplingRate));
    auto rampStart = static_cast<int>((1.0f - overlapFraction) * transitionTimeInSamples);
    auto rampEnd = transitionTimeInSamples;
    auto numTransitionSamples = rampEnd - rampStart;

    calibrateEQ(impulseResponse, rampStart, rampEnd, reverb.reverbTimes, eqCoeffs);

    calcReverbIR(numTransitionSamples, eqCoeffs, reverb.reverbTimes);

    for (auto i = rampStart; i < rampEnd; ++i)
    {
        auto alpha = static_cast<float>(rampEnd - i) / static_cast<float>(rampEnd - rampStart);
        for (auto j = 0; j < numChannels; ++j)
        {
            impulseResponse[j][i] *= sqrtf(alpha);
        }
    }

    for (auto i = rampStart; i < rampEnd; ++i)
    {
        auto alpha = static_cast<float>(rampEnd - i) / static_cast<float>(rampEnd - rampStart);
        impulseResponse[0][i] -= (1.0f - sqrtf(1.0f - alpha)) * mReverbIR[i - rampStart];
    }

    for (auto i = 0; i < numChannels; ++i)
    {
        for (auto j = rampEnd; j < impulseResponse.numSamples(); ++j)
        {
            impulseResponse[i][j] = 0.0f;
        }
    }

    delay = estimateDelay(transitionTime, overlapFraction);
}

void HybridReverbEstimator::calibrateEQ(const ImpulseResponse& impulseResponse,
                                        int rampStart,
                                        int rampEnd,
                                        const float* reverbTimes,
                                        float* eqCoeffs)
{
    // Render the parametric reverb with a flat EQ, as the hybrid effect plays it from the start of the crossfade.
    float flatEQ[Bands::kNumBands];
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        flatEQ[i] = 1.0f;
    }

    auto numTransitionSamples = rampEnd - rampStart;
    calcReverbIR(numTransitionSamples, flatEQ, reverbTimes);

    // In each band, scale the parametric reverb so that over the crossfade it has the energy that the
    // omnidirectional channel of the convolved impulse response has there. This matches the two whatever the
    // parametric reverb's own level in each band (which depends on the reverb times), and includes everything
    // the reconstructed impulse response applies (air absorption, distance attenuation correction).
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        mBandpassFilters[i].reset();
        mBandpassFilters[i].apply(rampEnd, impulseResponse[0], mBandIR.data());

        auto irEnergy = 0.0f;
        for (auto j = rampStart; j < rampEnd; ++j)
        {
            irEnergy += mBandIR[j] * mBandIR[j];
        }

        mBandpassFilters[i].reset();
        mBandpassFilters[i].apply(numTransitionSamples, mReverbIR.data(), mBandIR.data());

        auto reverbEnergy = 0.0f;
        for (auto j = 0; j < numTransitionSamples; ++j)
        {
            reverbEnergy += mBandIR[j] * mBandIR[j];
        }

        eqCoeffs[i] = (reverbEnergy > 0.0f) ? sqrtf(irEnergy / reverbEnergy) : 0.0f;
    }
}

int HybridReverbEstimator::estimateDelay(float transitionTime, float overlapFraction)
{
    return std::max(0, static_cast<int>(floorf((1.0f - overlapFraction) * transitionTime * mSamplingRate)));
}

void HybridReverbEstimator::calcReverbIR(int numSamples,
                                         const float* eqCoeffs,
                                         const float* reverbTimes)
{
    PROFILE_FUNCTION();

    mReverbIR.zero();

    mEQEffect.reset();
    mReverbEffect.reset();

    auto numFrames = numSamples / mFrameSize;
    if (numSamples % mFrameSize != 0)
    {
        ++numFrames;
    }

    float _eqCoeffs[Bands::kNumBands];
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        _eqCoeffs[i] = eqCoeffs[i];
    }

    auto overallGain = 16.0f * SphericalHarmonics::evaluate(0, 0, Vector3f{});
    EQEffect::normalizeGains(_eqCoeffs, overallGain);

    Reverb reverb;
    for (auto i = 0; i < Bands::kNumBands; ++i)
    {
        reverb.reverbTimes[i] = reverbTimes[i];
    }

    AudioBuffer temp(2, mFrameSize, mTempFrame.data());
    AudioBuffer temp0(temp, 0);
    AudioBuffer temp1(temp, 1);

    auto numSamplesLeft = numSamples;
    for (auto i = 0; i < numFrames; ++i)
    {
        memset(mTempFrame[0], 0, mFrameSize * sizeof(float));
        if (i == 0)
        {
            mTempFrame[0][0] = 1.0f;
        }

        EQEffectParams eqParams{};
        eqParams.gains = _eqCoeffs;

        mEQEffect.apply(eqParams, temp0, temp1);

        ArrayMath::scale(mFrameSize, mTempFrame[1], overallGain, mTempFrame[1]);

        ReverbEffectParams reverbParams{};
        reverbParams.reverb = &reverb;

        mReverbEffect.apply(reverbParams, temp1, temp0);

        auto numSamplesToCopy = std::min(mFrameSize, numSamplesLeft);
        numSamplesLeft -= numSamplesToCopy;

        memcpy(&mReverbIR[i * mFrameSize], mTempFrame[0], numSamplesToCopy * sizeof(float));
    }
}

}
