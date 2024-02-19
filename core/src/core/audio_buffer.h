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

#include "array.h"
#include "speaker_layout.h"

namespace ipl {

enum class AudioEffectState
{
    TailRemaining,
    TailComplete,
};

// --------------------------------------------------------------------------------------------------------------------
// AmbisonicsType
// --------------------------------------------------------------------------------------------------------------------

// Ambisonics channel ordering and normalization. N3D is used internally for everything.
enum class AmbisonicsType
{
    N3D,
    SN3D,
    FuMa
};


// --------------------------------------------------------------------------------------------------------------------
// AudioBuffer
// --------------------------------------------------------------------------------------------------------------------

struct AudioSettings
{
    int samplingRate    = 0;
    int frameSize       = 0;

    AudioSettings()
        : samplingRate(0)
        , frameSize(0)
    {}

    AudioSettings(int samplingRate, int frameSize)
        : samplingRate(samplingRate)
        , frameSize(frameSize)
    {}
};

// A buffer of data used for audio processing. Audio buffers are always deinterleaved.
class AudioBuffer
{
public:
    // Creates a buffer that owns its data arrays.
    AudioBuffer(int numChannels,
                int numSamples);

    // Creates a buffer that just refers to external data arrays. Doesn't allocate anything on the heap.
    AudioBuffer(int numChannels,
                int numSamples,
                float* const* data);

    // Creates a buffer that refers to a single channel of another buffer. Doesn't allocate anything on the heap.
    AudioBuffer(const AudioBuffer& other,
                int channel);

    int numChannels() const
    {
        return mNumChannels;
    }

    int numSamples() const
    {
        return mNumSamples;
    }

    float* const* data()
    {
        return mData;
    }

    const float* const* data() const
    {
        return mData;
    }

    float* operator[](int i)
    {
        return mData[i];
    }

    const float* operator[](int i) const
    {
        return mData[i];
    }

    void makeSilent();

    // Interleave into an output buffer.
    void read(float* out) const;

    // Deinterleave from an input buffer.
    void write(const float* in);

    // Scale data by volume.
    void scale(float volume);

    // Adds in to out.
    static void mix(const AudioBuffer& in,
                    AudioBuffer& out);

    // Downmixes a multi-channel buffer to mono. Channels are summed up and divided by the number of channels.
    static void downmix(const AudioBuffer& in,
                        AudioBuffer& out);

    // Convert between Ambisonics formats. CANNOT be in-place.
    static void convertAmbisonics(AmbisonicsType inType,
                                  AmbisonicsType outType,
                                  const AudioBuffer& in,
                                  AudioBuffer& out);

private:
    int mNumChannels;
    int mNumSamples;
    Array<float, 2> mInternalData; // #channels x #samples.
    float* const* mData; // Either points to mInternalData or to external data arrays.
};

}
