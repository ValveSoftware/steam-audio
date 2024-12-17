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
#include "bands.h"
#include "iir.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// EQEffect
// --------------------------------------------------------------------------------------------------------------------

struct EQEffectParams
{
    const float* gains = nullptr;
};

class EQEffect
{
public:
    EQEffect(const AudioSettings& audioSettings);

    void reset();

    AudioEffectState apply(const EQEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    AudioEffectState tailApply(const AudioBuffer& in, AudioBuffer& out);

    int numTailSamplesRemaining() const { return 0; }

    static void normalizeGains(float* eqGains,
                               float& overallGain);

private:
    int mSamplingRate;
    int mFrameSize;
    IIRFilterer mFilters[Bands::kNumBands][2];
    Array<float> mTemp;
    float mPrevGains[Bands::kNumBands];
    int mCurrent;
    bool mFirstFrame;

    void setFilterGains(int index,
                        const float* gains);

    void applyFilterCascade(int index,
                            const float* in,
                            float* out);
};

}
