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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DelayEffect
// --------------------------------------------------------------------------------------------------------------------

struct DelayEffectSettings
{
    int maxDelayInSamples = 0;

    DelayEffectSettings()
        : maxDelayInSamples(0)
    {}

    DelayEffectSettings(int maxDelayInSamples)
        : maxDelayInSamples(maxDelayInSamples)
    {}
};

struct DelayEffectParams
{
    int delayInSamples = 0;
};

class DelayEffect
{
public:
    DelayEffect(const AudioSettings& audioSettings,
                const DelayEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const DelayEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return mNumTailSamplesRemaining; }

private:
    int mFrameSize;
    int mMaxDelayInSamples;
    Array<float> mRingBuffer;
    int mWritePos;
    float mPrevDelayInSamples;
    int mNumTailSamplesRemaining;
    bool mFirstFrame;
};

}
