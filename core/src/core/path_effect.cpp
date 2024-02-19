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

#include "path_effect.h"

#include "array_math.h"
#include "sh.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PathEffect
// --------------------------------------------------------------------------------------------------------------------

PathEffect::PathEffect(const AudioSettings& audioSettings,
    const PathEffectSettings& effectSettings)
    : mMaxOrder(effectSettings.maxOrder)
    , mSpatialize(effectSettings.spatialize)
    , mEQBuffer(1, audioSettings.frameSize)
    , mEQEffect(audioSettings)
    , mPrevBinaural(false)
{
    if (mSpatialize)
    {
        AmbisonicsRotateEffectSettings ambisonicsRotateSettings{};
        ambisonicsRotateSettings.maxOrder = effectSettings.maxOrder;

        mAmbisonicsRotateEffect = make_unique<AmbisonicsRotateEffect>(AudioSettings{audioSettings.samplingRate, 1}, ambisonicsRotateSettings);

        AmbisonicsPanningEffectSettings ambisonicsPanningSettings{};
        ambisonicsPanningSettings.speakerLayout = effectSettings.speakerLayout;
        ambisonicsPanningSettings.maxOrder = effectSettings.maxOrder;

        mAmbisonicsPanningEffect = make_unique<AmbisonicsPanningEffect>(audioSettings, ambisonicsPanningSettings);

        mGainEffects.resize(effectSettings.speakerLayout->numSpeakers);
        for (auto i = 0u; i < mGainEffects.size(0); ++i)
        {
            mGainEffects[i] = make_unique<GainEffect>(audioSettings);
        }

        OverlapAddConvolutionEffectSettings overlapAddSettings{};
        overlapAddSettings.numChannels = IHRTFMap::kNumEars;
        overlapAddSettings.irSize = effectSettings.hrtf->numSamples();

        mOverlapAddEffect = make_unique<OverlapAddConvolutionEffect>(audioSettings, overlapAddSettings);

        mAmbisonicsBuffer = make_unique<AudioBuffer>(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder), 1);
        mSpeakerBuffer = make_unique<AudioBuffer>(effectSettings.speakerLayout->numSpeakers, 1);

        mHRTF.resize(2, effectSettings.hrtf->numSpectrumSamples());
    }
    else
    {
        mGainEffects.resize(SphericalHarmonics::numCoeffsForOrder(effectSettings.maxOrder));
        for (auto i = 0u; i < mGainEffects.size(0); ++i)
        {
            mGainEffects[i] = make_unique<GainEffect>(audioSettings);
        }
    }
}

void PathEffect::reset()
{
    mEQEffect.reset();

    if (mSpatialize)
    {
        mAmbisonicsRotateEffect->reset();
        mAmbisonicsPanningEffect->reset();
    }

    for (auto i = 0u; i < mGainEffects.size(0); ++i)
    {
        mGainEffects[i]->reset();
    }

    if (mSpatialize)
    {
        mOverlapAddEffect->reset();
    }

    mPrevBinaural = false;
}

// Rendering the SH and EQ coefficients for pathing involves the following steps:
//
// 1. EQ is applied to the dry audio.
// 2. The EQ-filtered audio is scaled by each SH coefficient in turn and combined into an Ambisonics buffer.
AudioEffectState PathEffect::apply(const PathEffectParams& params,
                                   const AudioBuffer& in,
                                   AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == 1);

    out.makeSilent();

    if (mSpatialize)
    {
        // todo: assert?

        // apply eq to mono input
        EQEffectParams eqParams{};
        eqParams.gains = params.eqCoeffs;

        mEQEffect.apply(eqParams, in, mEQBuffer);

        for (auto i = 0; i < SphericalHarmonics::numCoeffsForOrder(params.order); ++i)
        {
            (*mAmbisonicsBuffer)[i][0] = params.shCoeffs[i];
        }

        // rotate the sh coeffs
        AmbisonicsRotateEffectParams ambisonicsRotateParams{};
        ambisonicsRotateParams.orientation = params.listener;
        ambisonicsRotateParams.order = params.order;

        mAmbisonicsRotateEffect->apply(ambisonicsRotateParams, *mAmbisonicsBuffer, *mAmbisonicsBuffer);

        if (params.binaural)
        {
            // blend hrtf
            memset(mHRTF.flatData(), 0, mHRTF.totalSize() * sizeof(complex_t));

            auto cosine = cosf((137.9f * Math::kDegreesToRadians) / (params.order + 1.51f));
            for (auto l = 0, i = 0; l <= params.order; ++l)
            {
                auto scalar = SphericalHarmonics::legendre(l, cosine);
                for (auto m = -l; m <= l; ++m, ++i)
                {
                    const complex_t* hrtfForChannel[2] = {nullptr, nullptr};
                    params.hrtf->ambisonicsHRTF(i, hrtfForChannel);

                    for (auto k = 0; k < IHRTFMap::kNumEars; ++k)
                    {
                        ArrayMath::scaleAccumulate(params.hrtf->numSpectrumSamples(),
                            reinterpret_cast<const float*>(hrtfForChannel[k]),
                            scalar * (*mAmbisonicsBuffer)[i][0],
                            reinterpret_cast<float*>(mHRTF[k]));
                    }
                }
            }

            // convolve with blended hrtf
            OverlapAddConvolutionEffectParams overlapAddParams{};
            overlapAddParams.fftIR = mHRTF.data();

            mPrevBinaural = true;

            return mOverlapAddEffect->apply(overlapAddParams, mEQBuffer, out);
        }
        else
        {
            // project sh coeffs to speaker layout
            AmbisonicsPanningEffectParams ambisonicsPanningParams{};
            ambisonicsPanningParams.order = params.order;

            mAmbisonicsPanningEffect->apply(ambisonicsPanningParams, *mAmbisonicsBuffer, *mSpeakerBuffer);

            // generate a panned output signal
            for (auto i = 0; i < out.numChannels(); ++i)
            {
                AudioBuffer outChannel(out, i);

                GainEffectParams gainParams{};
                gainParams.gain = (*mSpeakerBuffer)[i][0];

                mGainEffects[i]->apply(gainParams, mEQBuffer, outChannel);
            }

            mPrevBinaural = false;

            return AudioEffectState::TailComplete;
        }
    }
    else
    {
        assert(out.numChannels() == SphericalHarmonics::numCoeffsForOrder(mMaxOrder));

        EQEffectParams eqParams{};
        eqParams.gains = params.eqCoeffs;

        mEQEffect.apply(eqParams, in, mEQBuffer);

        auto numChannels = SphericalHarmonics::numCoeffsForOrder(params.order);

        for (auto i = 0; i < numChannels; ++i)
        {
            AudioBuffer outChannel(out, i);

            GainEffectParams gainParams{};
            gainParams.gain = params.shCoeffs[i];

            mGainEffects[i]->apply(gainParams, mEQBuffer, outChannel);
        }

        mPrevBinaural = false;

        return AudioEffectState::TailComplete;
    }
}

AudioEffectState PathEffect::tail(AudioBuffer& out)
{
    out.makeSilent();

    if (mSpatialize)
    {
        if (mPrevBinaural)
            return mOverlapAddEffect->tail(out);
        else
            return AudioEffectState::TailComplete;
    }
    else
    {
        return AudioEffectState::TailComplete;
    }
}

int PathEffect::numTailSamplesRemaining() const
{
    if (mSpatialize)
    {
        if (mPrevBinaural)
            return mOverlapAddEffect->numTailSamplesRemaining();
        else
            return 0;
    }
    else
    {
        return 0;
    }
}

}
