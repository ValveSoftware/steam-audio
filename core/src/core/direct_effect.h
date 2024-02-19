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
#include "delay_effect.h"
#include "direct_simulator.h"
#include "eq_effect.h"
#include "gain_effect.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// DirectEffect
// --------------------------------------------------------------------------------------------------------------------

enum DirectEffectFlags
{
    ApplyDistanceAttenuation = 1 << 0,
    ApplyAirAbsorption = 1 << 1,
    ApplyDirectivity = 1 << 2,
    ApplyOcclusion = 1 << 3,
    ApplyTransmission = 1 << 4
};

enum class TransmissionType
{
    FreqIndependent,
    FreqDependent
};

struct DirectEffectSettings
{
    int numChannels = 1;
};

struct DirectEffectParams
{
    DirectSoundPath directPath;
    DirectEffectFlags flags = static_cast<DirectEffectFlags>(0);
    TransmissionType transmissionType = TransmissionType::FreqDependent;
};

// Audio effect that applies direct sound path parameters to an incoming multichannel audio buffer.
class DirectEffect
{
public:
    DirectEffect(const AudioSettings& audioSettings,
                 const DirectEffectSettings& effectSettings);

    void reset();

    AudioEffectState apply(const DirectEffectParams& params,
                           const AudioBuffer& in,
                           AudioBuffer& out);

    AudioEffectState tail(AudioBuffer& out);

    int numTailSamplesRemaining() const { return 0; }

private:
    int mNumChannels;
    Array<unique_ptr<EQEffect>> mEQEffects; // One filter object per channel to apply effect.
    Array<unique_ptr<GainEffect>> mGainEffects; // Attenuation interpolation.

    static void calculateGainAndEQ(const DirectSoundPath& directPath,
                                   DirectEffectFlags flags,
                                   TransmissionType transmissionType,
                                   float& overallGain,
                                   float* eqCoeffs);
};

}
