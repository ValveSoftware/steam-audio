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

#pragma once

#include "audio_buffer.h"
#include "hybrid_reverb_effect.h"
#include "overlap_save_convolution_effect.h"
#include "reverb_effect.h"
#include "tan_device.h"
#include "tan_convolution_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// IndirectEffect
// --------------------------------------------------------------------------------------------------------------------

enum class IndirectEffectType
{
    Convolution,
    Parametric,
    Hybrid,
    TrueAudioNext
};

struct IndirectEffectSettings
{
    IndirectEffectType type = IndirectEffectType::Convolution;
    int numChannels = 0;
    int irSize = 0;
};

struct IndirectEffectParams
{
    TripleBuffer<OverlapSaveFIR>* fftIR = nullptr;
    const Reverb* reverb = nullptr;
    const float* eqCoeffs = nullptr;
    int delay = 0;
    int numChannels = 0;
    int numSamples = 0;
    const TANDevice* tan = nullptr;
    int slot = -1;
};

struct IndirectMixerParams
{
    int numChannels = 0;
    const TANDevice* tan = nullptr;
};

class IndirectMixer;

class IndirectEffect
{
public:
    IndirectEffect(const AudioSettings& audioSettings,
                   const IndirectEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const IndirectEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState apply(const IndirectEffectParams& params,
                           const AudioBuffer& in,
                           IndirectMixer& mixer);

    AudioEffectState tail(AudioBuffer& out);

    AudioEffectState tail(IndirectMixer& mixer);

    int numTailSamplesRemaining() const;

private:
    IndirectEffectType mType;
    unique_ptr<OverlapSaveConvolutionEffect> mConvolutionEffect;
    unique_ptr<ReverbEffect> mParametricEffect;
    unique_ptr<HybridReverbEffect> mHybridEffect;
#if defined(IPL_USES_TRUEAUDIONEXT)
    unique_ptr<TANConvolutionEffect> mTANEffect;
#endif
};


// --------------------------------------------------------------------------------------------------------------------
// IndirectMixer
// --------------------------------------------------------------------------------------------------------------------

class IndirectMixer
{
public:
    IndirectMixer(const AudioSettings& audioSettings,
                  const IndirectEffectSettings& effectSettings);

    OverlapSaveConvolutionMixer& convolutionMixer()
    {
        return *mConvolutionMixer;
    }

#if defined(IPL_USES_TRUEAUDIONEXT)
    TANConvolutionMixer& tanMixer()
    {
        return *mTANMixer;
    }
#endif

    void reset();

    void apply(const IndirectMixerParams& params,
               AudioBuffer& out);

private:
    IndirectEffectType mType;
    unique_ptr<OverlapSaveConvolutionMixer> mConvolutionMixer;
#if defined(IPL_USES_TRUEAUDIONEXT)
    unique_ptr<TANConvolutionMixer> mTANMixer;
#endif
};

}
