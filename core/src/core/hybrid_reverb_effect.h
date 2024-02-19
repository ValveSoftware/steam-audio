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

#include "delay_effect.h"
#include "eq_effect.h"
#include "gain_effect.h"
#include "overlap_save_convolution_effect.h"
#include "reverb_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// HybridReverbEffect
// --------------------------------------------------------------------------------------------------------------------

struct HybridReverbEffectSettings
{
    int numChannels = 0;
    int irSize = 0;

    HybridReverbEffectSettings()
        : numChannels(0)
        , irSize(0)
    {}

    HybridReverbEffectSettings(int numChannels, int irSize)
        : numChannels(numChannels)
        , irSize(irSize)
    {}
};

struct HybridReverbEffectParams
{
    TripleBuffer<OverlapSaveFIR>* fftIR = nullptr;
    const Reverb* reverb = nullptr;
    const float* eqCoeffs = nullptr;
    int delay = 0;
    int numChannels = 0;
    int numSamples = 0;
};

class HybridReverbEffect
{
public:
    HybridReverbEffect(const AudioSettings& audioSettings,
                       const HybridReverbEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const HybridReverbEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const;

private:
    int mFrameSize;
    OverlapSaveConvolutionEffect mConvolutionEffect;
    ReverbEffect mParametricEffect;
    EQEffect mEQEffect;
    GainEffect mGainEffect;
    DelayEffect mDelayEffect;
    AudioBuffer mDelayTemp;
    AudioBuffer mEQTemp;
    AudioBuffer mGainTemp;
    AudioBuffer mReverbTemp;
    AudioEffectState mConvolutionEffectState;
    AudioEffectState mParametricEffectState;
    AudioEffectState mEQEffectState;
    AudioEffectState mGainEffectState;
    AudioEffectState mDelayEffectState;
};

}
