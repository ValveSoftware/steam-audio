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

#include "ambisonics_binaural_effect.h"
#include "ambisonics_panning_effect.h"
#include "ambisonics_rotate_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsDecodeEffect
// --------------------------------------------------------------------------------------------------------------------

struct AmbisonicsDecodeEffectSettings
{
    const SpeakerLayout* speakerLayout = nullptr;
    int maxOrder = 0;
    const HRTFDatabase* hrtf = nullptr;
};

struct AmbisonicsDecodeEffectParams
{
    int order = 0;
    const CoordinateSpace3f* orientation = nullptr;
    bool binaural = false;
    const HRTFDatabase* hrtf = nullptr;
};

class AmbisonicsDecodeEffect
{
public:
    AmbisonicsDecodeEffect(const AudioSettings& audioSettings,
                           const AmbisonicsDecodeEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const AmbisonicsDecodeEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const;

private:
    int mFrameSize;
    SpeakerLayout mSpeakerLayout;
    int mMaxOrder;
    unique_ptr<AmbisonicsPanningEffect> mPanningEffect;
    unique_ptr<AmbisonicsBinauralEffect> mBinauralEffect;
    unique_ptr<AmbisonicsRotateEffect> mRotateEffect;
    AudioBuffer mRotated;
    bool mPrevBinaural;
};

}
