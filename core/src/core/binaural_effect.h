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

#pragma once

#include "hrtf_database.h"
#include "overlap_add_convolution_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// BinauralEffect
// --------------------------------------------------------------------------------------------------------------------

struct BinauralEffectSettings
{
    const HRTFDatabase* hrtf = nullptr;
};

struct BinauralEffectParams
{
    const Vector3f* direction = nullptr;
    HRTFInterpolation interpolation = HRTFInterpolation::NearestNeighbor;
    float spatialBlend = 1.0f;
    HRTFPhaseType phaseType = HRTFPhaseType::None;
    const HRTFDatabase* hrtf = nullptr;
    float* peakDelays = nullptr;
};

// An audio effect that applies an HRTF to a mono audio buffer that corresponds to audio emitted by a specific source
// with a given relative direction.
class BinauralEffect
{
public:
    BinauralEffect(const AudioSettings& audioSettings,
                   const BinauralEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const BinauralEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return mOverlapAddEffect->numTailSamplesRemaining(); }

private:
    int mSamplingRate;
    int mFrameSize;
    int mHRIRSize;
    unique_ptr<OverlapAddConvolutionEffect> mOverlapAddEffect;
    Array<complex_t, 2> mInterpolatedHRTF;
    AudioBuffer mPartialDownmixed;
    AudioBuffer mPartialOutput;

    void init(const HRTFDatabase& hrtf);
};

}
