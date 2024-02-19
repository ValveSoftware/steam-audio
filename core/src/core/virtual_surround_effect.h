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

#include "binaural_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// VirtualSurroundEffect
// --------------------------------------------------------------------------------------------------------------------

struct VirtualSurroundEffectSettings
{
    const SpeakerLayout* speakerLayout = nullptr;
    const HRTFDatabase* hrtf = nullptr;
};

struct VirtualSurroundEffectParams
{
    const HRTFDatabase* hrtf = nullptr;
};

// The virtual surround effect takes a non-directional input signal and treats each channel as a positional speaker by
// applying a corresponding HRTF filter. Mono and stereo sources are passed through unprocessed.
class VirtualSurroundEffect
{
public:
    VirtualSurroundEffect(const AudioSettings& audioSettings,
                          const VirtualSurroundEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const VirtualSurroundEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const;

private:
    int mFrameSize;
    SpeakerLayout mSpeakerLayout;
    Array<unique_ptr<BinauralEffect>> mBinauralEffects;
    Array<AudioEffectState> mBinauralEffectStates;
    AudioBuffer mSpatializedChannel;
};

}
