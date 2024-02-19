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

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// PanningEffect
// --------------------------------------------------------------------------------------------------------------------

struct PanningData;

struct PanningEffectSettings
{
    const SpeakerLayout* speakerLayout = nullptr;
};

struct PanningEffectParams
{
    const Vector3f* direction = nullptr;
};

// Audio effect that applies multichannel panning coefficients to an incoming mono audio buffer.
class PanningEffect
{
public:
    PanningEffect(const PanningEffectSettings& settings);

    void reset();

    AudioEffectState apply(const PanningEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return 0; }

    static float panningWeight(const Vector3f& direction,
                               const SpeakerLayout& speakerLayout,
                               int index,
                               const PanningData* panningData = nullptr);

private:
    SpeakerLayout mSpeakerLayout;
    Vector3f mPrevDirection;

    static float stereoPanningWeight(const Vector3f& direction,
                                     int index);

    static float firstOrderPanningWeight(const Vector3f& direction,
                                         const SpeakerLayout& speakerLayout,
                                         int index);

    static float secondOrderPanningWeight(const Vector3f& direction,
                                          const SpeakerLayout& speakerLayout,
                                          int index);

    // Calculates pairwise constant-power panning weights for surround speaker layouts
    // (e.g. 5.1, 7.1).
    static float pairwisePanningWeight(const Vector3f& direction,
                                       const SpeakerLayout& speakerLayout,
                                       int index,
                                       const PanningData* panningData = nullptr);

    // Precalculates some intermediate data that can be reused when evaluating panning
    // weights for different speakers given the same source direction.
    static void calcPairwisePanningData(const Vector3f& direction,
                                        const SpeakerLayout& speakerLayout,
                                        PanningData& panningData);
};

}
