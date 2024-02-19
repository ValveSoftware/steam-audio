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

#include "indirect_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IndirectEffect
// --------------------------------------------------------------------------------------------------------------------

IndirectEffect::IndirectEffect(const AudioSettings& audioSettings,
                               const IndirectEffectSettings& effectSettings)
    : mType(effectSettings.type)
{
    switch (effectSettings.type)
    {
    case IndirectEffectType::Convolution:
        mConvolutionEffect = make_unique<OverlapSaveConvolutionEffect>(audioSettings, OverlapSaveConvolutionEffectSettings{effectSettings.numChannels, effectSettings.irSize});
        break;

    case IndirectEffectType::Parametric:
        mParametricEffect = make_unique<ReverbEffect>(audioSettings);
        break;;

    case IndirectEffectType::Hybrid:
        mHybridEffect = make_unique<HybridReverbEffect>(audioSettings, HybridReverbEffectSettings{effectSettings.numChannels, effectSettings.irSize});
        break;

#if defined(IPL_USES_TRUEAUDIONEXT)
    case IndirectEffectType::TrueAudioNext:
        mTANEffect = make_unique<TANConvolutionEffect>();
        break;
#endif

    default:
        break;
    }
}

void IndirectEffect::reset()
{
    switch (mType)
    {
    case IndirectEffectType::Convolution:
        mConvolutionEffect->reset();
        break;

    case IndirectEffectType::Parametric:
        mParametricEffect->reset();
        break;

    case IndirectEffectType::Hybrid:
        mHybridEffect->reset();
        break;

#if defined(IPL_USES_TRUEAUDIONEXT)
    case IndirectEffectType::TrueAudioNext:
        mTANEffect->reset();
        break;
#endif

    default:
        break;
    }
}

AudioEffectState IndirectEffect::apply(const IndirectEffectParams& params,
                                       const AudioBuffer& in,
                                       AudioBuffer& out)
{
    if (mType == IndirectEffectType::Convolution && !params.fftIR)
    {
        out.makeSilent();
        return AudioEffectState::TailComplete;
    }

    if (mType == IndirectEffectType::Convolution)
    {
        OverlapSaveConvolutionEffectParams overlapSaveParams{};
        overlapSaveParams.fftIR = params.fftIR;
        overlapSaveParams.numChannels = params.numChannels;
        overlapSaveParams.numSamples = params.numSamples;

        return mConvolutionEffect->apply(overlapSaveParams, in, out);
    }
    else if (mType == IndirectEffectType::Parametric)
    {
        ReverbEffectParams reverbParams{};
        reverbParams.reverb = params.reverb;

        return mParametricEffect->apply(reverbParams, in, out);
    }
    else if (mType == IndirectEffectType::Hybrid)
    {
        HybridReverbEffectParams hybridParams{};
        hybridParams.fftIR = params.fftIR;
        hybridParams.reverb = params.reverb;
        hybridParams.eqCoeffs = params.eqCoeffs;
        hybridParams.delay = params.delay;
        hybridParams.numChannels = params.numChannels;
        hybridParams.numSamples = params.numSamples;

        return mHybridEffect->apply(hybridParams, in, out);
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState IndirectEffect::apply(const IndirectEffectParams& params,
                           const AudioBuffer& in,
                           IndirectMixer& mixer)
{
    if (mType == IndirectEffectType::Convolution && !params.fftIR)
        return AudioEffectState::TailComplete;

    if (mType == IndirectEffectType::Convolution)
    {
        OverlapSaveConvolutionEffectParams overlapSaveParams{};
        overlapSaveParams.fftIR = params.fftIR;
        overlapSaveParams.numChannels = params.numChannels;
        overlapSaveParams.numSamples = params.numSamples;

        return mConvolutionEffect->apply(overlapSaveParams, in, mixer.convolutionMixer());
    }
#if defined(IPL_USES_TRUEAUDIONEXT)
    else if (mType == IndirectEffectType::TrueAudioNext)
    {
        TANConvolutionEffectParams tanParams{};
        tanParams.tan = params.tan;
        tanParams.slot = params.slot;

        return mTANEffect->apply(tanParams, in, mixer.tanMixer());
    }
#endif

    return AudioEffectState::TailComplete;
}

AudioEffectState IndirectEffect::tail(AudioBuffer& out)
{
    switch (mType)
    {
    case IndirectEffectType::Convolution:
        return mConvolutionEffect->tail(out);
    case IndirectEffectType::Parametric:
        return mParametricEffect->tail(out);
    case IndirectEffectType::Hybrid:
        return mHybridEffect->tail(out);
    default:
        return AudioEffectState::TailComplete;
    }
}

AudioEffectState IndirectEffect::tail(IndirectMixer& mixer)
{
    switch (mType)
    {
    case IndirectEffectType::Convolution:
        return mConvolutionEffect->tail(mixer.convolutionMixer());
#if defined(IPL_USES_TRUEAUDIONEXT)
    case IndirectEffectType::TrueAudioNext:
        return mTANEffect->tail(mixer.tanMixer());
#endif
    default:
        return AudioEffectState::TailComplete;
    }
}

int IndirectEffect::numTailSamplesRemaining() const
{
    switch (mType)
    {
    case IndirectEffectType::Convolution:
        return mConvolutionEffect->numTailSamplesRemaining();
    case IndirectEffectType::Parametric:
        return mParametricEffect->numTailSamplesRemaining();
    case IndirectEffectType::Hybrid:
        return mHybridEffect->numTailSamplesRemaining();
#if defined(IPL_USES_TRUEAUDIONEXT)
    case IndirectEffectType::TrueAudioNext:
        return mTANEffect->numTailSamplesRemaining();
#endif
    default:
        return 0;
    }
}


// --------------------------------------------------------------------------------------------------------------------
// IndirectMixer
// --------------------------------------------------------------------------------------------------------------------

IndirectMixer::IndirectMixer(const AudioSettings& audioSettings,
                             const IndirectEffectSettings& effectSettings)
    : mType(effectSettings.type)
{
    if (mType == IndirectEffectType::Convolution)
    {
        mConvolutionMixer = make_unique<OverlapSaveConvolutionMixer>(audioSettings, OverlapSaveConvolutionEffectSettings{effectSettings.numChannels, effectSettings.irSize});
    }
#if defined(IPL_USES_TRUEAUDIONEXT)
    else if (mType == IndirectEffectType::TrueAudioNext)
    {
        mTANMixer = make_unique<TANConvolutionMixer>();
    }
#endif
}

void IndirectMixer::reset()
{
    switch (mType)
    {
    case IndirectEffectType::Convolution:
        mConvolutionMixer->reset();
        break;

#if defined(IPL_USES_TRUEAUDIONEXT)
    case IndirectEffectType::TrueAudioNext:
        mTANMixer->reset();
        break;
#endif

    default:
        break;
    }
}

void IndirectMixer::apply(const IndirectMixerParams& params,
                          AudioBuffer& out)
{
    if (mType == IndirectEffectType::Convolution)
    {
        OverlapSaveConvolutionMixerParams overlapSaveParams{};
        overlapSaveParams.numChannels = params.numChannels;

        mConvolutionMixer->apply(overlapSaveParams, out);
    }
#if defined(IPL_USES_TRUEAUDIONEXT)
    else if (mType == IndirectEffectType::TrueAudioNext)
    {
        TANConvolutionMixerParams tanParams{};
        tanParams.tan = params.tan;

        mTANMixer->apply(tanParams, out);
    }
#endif
}

}
