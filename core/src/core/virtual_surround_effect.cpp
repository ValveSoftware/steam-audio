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

#include "virtual_surround_effect.h"

#include "array_math.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// VirtualSurroundEffect
// --------------------------------------------------------------------------------------------------------------------

VirtualSurroundEffect::VirtualSurroundEffect(const AudioSettings& audioSettings,
                                             const VirtualSurroundEffectSettings& effectSettings)
    : mFrameSize(audioSettings.frameSize)
    , mSpeakerLayout(*effectSettings.speakerLayout)
    , mBinauralEffects(effectSettings.speakerLayout->numSpeakers)
    , mBinauralEffectStates(effectSettings.speakerLayout->numSpeakers)
    , mSpatializedChannel(2, audioSettings.frameSize)
{
    for (auto i = 0; i < effectSettings.speakerLayout->numSpeakers; ++i)
    {
        BinauralEffectSettings binauralSettings{};
        binauralSettings.hrtf = effectSettings.hrtf;

        mBinauralEffects[i] = make_unique<BinauralEffect>(audioSettings, binauralSettings);
    }
}

void VirtualSurroundEffect::reset()
{
    for (auto i = 0u; i < mBinauralEffects.size(0); ++i)
    {
        mBinauralEffects[i]->reset();
        mBinauralEffectStates[i] = AudioEffectState::TailComplete;
    }
}

// Takes the nondirectional @inputAudio buffer and produces a virtual
// surround effect in @outputAudio by spatializing each channel at a given location.
// For mono streams, data is passed through as-is. For stereo and
// higher, the source directions are defined by AudioFormat::speakerDirection().
AudioEffectState VirtualSurroundEffect::apply(const VirtualSurroundEffectParams& params,
                                              const AudioBuffer& in,
                                              AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(in.numChannels() == mSpeakerLayout.numSpeakers);
    assert(out.numChannels() == 2);

    if (in.numChannels() == 1)
    {
        for (auto i = 0; i < 2; ++i)
        {
            memcpy(out[i], in[0], mFrameSize * sizeof(float));
        }
    }
    else
    {
        out.makeSilent();

        for (auto i = 0; i < in.numChannels(); ++i)
        {
            AudioBuffer channel(in, i);

            auto direction = Vector3f::unitVector(mSpeakerLayout.speakers[i]);

            BinauralEffectParams binauralParams{};
            binauralParams.direction = &direction;
            binauralParams.hrtf = params.hrtf;

            mBinauralEffectStates[i] = mBinauralEffects[i]->apply(binauralParams, channel, mSpatializedChannel);

            AudioBuffer::mix(mSpatializedChannel, out);
        }
    }

    for (auto i = 0; i < mBinauralEffectStates.totalSize(); ++i)
    {
        if (mBinauralEffectStates[i] == AudioEffectState::TailRemaining)
            return AudioEffectState::TailRemaining;
    }

    return AudioEffectState::TailComplete;
}

AudioEffectState VirtualSurroundEffect::tail(AudioBuffer& out)
{
    assert(out.numChannels() == 2);

    out.makeSilent();

    for (auto i = 0; i < mBinauralEffectStates.totalSize(); ++i)
    {
        mBinauralEffectStates[i] = mBinauralEffects[i]->tail(mSpatializedChannel);
        AudioBuffer::mix(mSpatializedChannel, out);
    }

    for (auto i = 0; i < mBinauralEffectStates.totalSize(); ++i)
    {
        if (mBinauralEffectStates[i] == AudioEffectState::TailRemaining)
            return AudioEffectState::TailRemaining;
    }

    return AudioEffectState::TailComplete;
}

int VirtualSurroundEffect::numTailSamplesRemaining() const
{
    auto result = 0;

    for (auto i = 0; i < mBinauralEffects.totalSize(); ++i)
    {
        result = std::max(result, mBinauralEffects[i]->numTailSamplesRemaining());
    }

    return result;
}

}
