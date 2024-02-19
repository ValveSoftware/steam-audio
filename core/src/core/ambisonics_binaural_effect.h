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

#include "audio_buffer.h"
#include "containers.h"
#include "hrtf_database.h"
#include "overlap_add_convolution_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsBinauralEffect
// --------------------------------------------------------------------------------------------------------------------

struct AmbisonicsBinauralEffectSettings
{
    int maxOrder = 0;
    const HRTFDatabase* hrtf = nullptr;
};

struct AmbisonicsBinauralEffectParams
{
    const HRTFDatabase* hrtf = nullptr;
    int order = 0;
};

// Audio effect that renders an Ambisonics buffer using binaural rendering.
class AmbisonicsBinauralEffect
{
public:
    AmbisonicsBinauralEffect(const AudioSettings& audioSettings,
                             const AmbisonicsBinauralEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const AmbisonicsBinauralEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const;

private:
    int mFrameSize;
    int mMaxOrder;
    int mHRIRSize;
    vector<unique_ptr<OverlapAddConvolutionEffect>> mOverlapAddEffects;
    Array<AudioEffectState> mOverlapAddEffectStates;
    AudioBuffer mSpatializedChannel;

    void init(const HRTFDatabase& hrtf);
};

}
