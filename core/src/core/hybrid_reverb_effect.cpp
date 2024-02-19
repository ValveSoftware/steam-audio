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

#include "array_math.h"
#include "hybrid_reverb_effect.h"
#include "sh.h"
#include "profiler.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HybridReverbEffect
// --------------------------------------------------------------------------------------------------------------------

HybridReverbEffect::HybridReverbEffect(const AudioSettings& audioSettings,
                                       const HybridReverbEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mConvolutionEffect(audioSettings, OverlapSaveConvolutionEffectSettings{effectSettings.numChannels, effectSettings.irSize})
    , mParametricEffect(audioSettings)
    , mEQEffect(audioSettings)
    , mGainEffect(audioSettings)
    , mDelayEffect(audioSettings, DelayEffectSettings{2 * effectSettings.irSize})
    , mDelayTemp(1, audioSettings.frameSize)
    , mEQTemp(1, audioSettings.frameSize)
    , mGainTemp(1, audioSettings.frameSize)
    , mReverbTemp(1, audioSettings.frameSize)
{
    reset();
}

void HybridReverbEffect::reset()
{
    mConvolutionEffect.reset();
    mParametricEffect.reset();
    mEQEffect.reset();
    mGainEffect.reset();
    mDelayEffect.reset();

    mConvolutionEffectState = AudioEffectState::TailComplete;
    mParametricEffectState = AudioEffectState::TailComplete;
    mEQEffectState = AudioEffectState::TailComplete;
    mGainEffectState = AudioEffectState::TailComplete;
    mDelayEffectState = AudioEffectState::TailComplete;
}

AudioEffectState HybridReverbEffect::apply(const HybridReverbEffectParams& params,
                                           const AudioBuffer& in,
                                           AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);

    PROFILE_FUNCTION();

    if (params.fftIR)
    {
        OverlapSaveConvolutionEffectParams overlapSaveParams{};
        overlapSaveParams.fftIR = params.fftIR;
        overlapSaveParams.numChannels = params.numChannels;
        overlapSaveParams.numSamples = params.numSamples;

        mConvolutionEffectState = mConvolutionEffect.apply(overlapSaveParams, in, out);
    }
    else
    {
        out.makeSilent();
        mConvolutionEffectState = AudioEffectState::TailComplete;
    }

    float _eqCoeffs[Bands::kNumBands] = { params.eqCoeffs[0], params.eqCoeffs[1], params.eqCoeffs[2] };
    auto gain = 16.0f;
    EQEffect::normalizeGains(_eqCoeffs, gain);

    DelayEffectParams delayParams{};
    delayParams.delayInSamples = params.delay;

    mDelayEffectState = mDelayEffect.apply(delayParams, in, mDelayTemp);

    EQEffectParams eqParams{};
    eqParams.gains = _eqCoeffs;

    mEQEffectState = mEQEffect.apply(eqParams, mDelayTemp, mEQTemp);

    GainEffectParams gainParams{};
    gainParams.gain = gain;

    mGainEffectState = mGainEffect.apply(gainParams, mEQTemp, mGainTemp);

    ReverbEffectParams reverbParams{};
    reverbParams.reverb = params.reverb;

    mParametricEffectState = mParametricEffect.apply(reverbParams, mGainTemp, mReverbTemp);

    auto scalar = SphericalHarmonics::evaluate(0, 0, Vector3f{});
    ArrayMath::scale(mFrameSize, mReverbTemp[0], scalar, mReverbTemp[0]);

    ArrayMath::add(mFrameSize, out[0], mReverbTemp[0], out[0]);

    if (mConvolutionEffectState == AudioEffectState::TailRemaining ||
        mParametricEffectState == AudioEffectState::TailRemaining ||
        mEQEffectState == AudioEffectState::TailRemaining ||
        mGainEffectState == AudioEffectState::TailRemaining ||
        mDelayEffectState == AudioEffectState::TailRemaining)
    {
        return AudioEffectState::TailRemaining;
    }
    else
    {
        return AudioEffectState::TailComplete;
    }
}

AudioEffectState HybridReverbEffect::tail(AudioBuffer& out)
{
    out.makeSilent();

    if (mConvolutionEffectState == AudioEffectState::TailRemaining)
    {
        mConvolutionEffectState = mConvolutionEffect.tail(out);
    }

    if (mParametricEffectState == AudioEffectState::TailRemaining ||
        mEQEffectState == AudioEffectState::TailRemaining ||
        mGainEffectState == AudioEffectState::TailRemaining ||
        mDelayEffectState == AudioEffectState::TailRemaining)
    {
        if (mDelayEffectState == AudioEffectState::TailRemaining)
        {
            mDelayEffectState = mDelayEffect.tail(mDelayTemp);
            mEQEffectState = mEQEffect.tailApply(mDelayTemp, mEQTemp);
            mGainEffectState = mGainEffect.tailApply(mEQTemp, mGainTemp);
            mParametricEffectState = mParametricEffect.tailApply(mGainTemp, mReverbTemp);
        }
        else if (mEQEffectState == AudioEffectState::TailRemaining)
        {
            mEQEffectState = mEQEffect.tail(mEQTemp);
            mGainEffectState = mGainEffect.tailApply(mEQTemp, mGainTemp);
            mParametricEffectState = mParametricEffect.tailApply(mGainTemp, mReverbTemp);
        }
        else if (mGainEffectState == AudioEffectState::TailRemaining)
        {
            mGainEffectState = mGainEffect.tail(mGainTemp);
            mParametricEffectState = mParametricEffect.tailApply(mGainTemp, mReverbTemp);
        }
        else if (mParametricEffectState == AudioEffectState::TailRemaining)
        {
            mParametricEffectState = mParametricEffect.tail(mReverbTemp);
        }

        auto scalar = SphericalHarmonics::evaluate(0, 0, Vector3f{});
        ArrayMath::scale(mFrameSize, mReverbTemp[0], scalar, mReverbTemp[0]);
        ArrayMath::add(mFrameSize, out[0], mReverbTemp[0], out[0]);
    }

    if (mConvolutionEffectState == AudioEffectState::TailRemaining ||
        mParametricEffectState == AudioEffectState::TailRemaining ||
        mEQEffectState == AudioEffectState::TailRemaining ||
        mGainEffectState == AudioEffectState::TailRemaining ||
        mDelayEffectState == AudioEffectState::TailRemaining)
    {
        return AudioEffectState::TailRemaining;
    }
    else
    {
        return AudioEffectState::TailComplete;
    }
}

int HybridReverbEffect::numTailSamplesRemaining() const
{
    return std::max({
        mConvolutionEffect.numTailSamplesRemaining(),
        mParametricEffect.numTailSamplesRemaining(),
        mDelayEffect.numTailSamplesRemaining(),
        mEQEffect.numTailSamplesRemaining(),
        mGainEffect.numTailSamplesRemaining()
    });
}

}
