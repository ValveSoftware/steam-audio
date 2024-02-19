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
#include "matrix.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsPanningEffect
// --------------------------------------------------------------------------------------------------------------------

struct AmbisonicsPanningEffectSettings
{
    const SpeakerLayout* speakerLayout = nullptr;
    int maxOrder = 0;
};

struct AmbisonicsPanningEffectParams
{
    int order = 0;
};

class AmbisonicsPanningEffect
{
public:
    static const int kNumVirtualSpeakers = 24;
    static const Vector3f kVirtualSpeakers[kNumVirtualSpeakers];

    AmbisonicsPanningEffect(const AudioSettings& audioSettings,
                            const AmbisonicsPanningEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const AmbisonicsPanningEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return 0; }

private:
    SpeakerLayout mSpeakerLayout;
    int mMaxOrder;
    DynamicMatrixf mAmbisonicsToSpeakersMatrix;
    DynamicMatrixf mAmbisonicsVectors;
    DynamicMatrixf mSpeakersVectors;
};

}