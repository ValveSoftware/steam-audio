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

#include "audio_buffer.h"

#include "array_math.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// AudioBuffer
// --------------------------------------------------------------------------------------------------------------------

AudioBuffer::AudioBuffer(int numChannels,
                         int numSamples)
    : mNumChannels(numChannels)
    , mNumSamples(numSamples)
    , mInternalData(numChannels, numSamples)
    , mData(mInternalData.data())
{}

AudioBuffer::AudioBuffer(int numChannels,
                         int numSamples,
                         float* const* data)
    : mNumChannels(numChannels)
    , mNumSamples(numSamples)
    , mData(data)
{}

AudioBuffer::AudioBuffer(const AudioBuffer& other,
                         int channel)
    : mNumChannels(1)
    , mNumSamples(other.mNumSamples)
    , mData(&other.mData[channel])
{}

void AudioBuffer::makeSilent()
{
    for (auto i = 0; i < mNumChannels; ++i)
    {
        memset(mData[i], 0, mNumSamples * sizeof(float));
    }
}

void AudioBuffer::read(float* out) const
{
    for (auto i = 0, index = 0; i < mNumSamples; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j, ++index)
        {
            out[index] = mData[j][i];
        }
    }
}

void AudioBuffer::write(const float* in)
{
    for (auto i = 0, index = 0; i < mNumSamples; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j, ++index)
        {
            mData[j][i] = in[index];
        }
    }
}

void AudioBuffer::scale(float volume)
{
    for (auto i = 0, index = 0; i < mNumSamples; ++i)
    {
        for (auto j = 0; j < mNumChannels; ++j, ++index)
        {
            mData[j][i] *= volume;
        }
    }
}

void AudioBuffer::mix(const AudioBuffer& in,
                      AudioBuffer& out)
{
    assert(in.numChannels() == out.numChannels());
    assert(in.numSamples() == out.numSamples());

    for (auto i = 0; i < in.numChannels(); ++i)
    {
        ArrayMath::add(in.numSamples(), in[i], out[i], out[i]);
    }
}

void AudioBuffer::downmix(const AudioBuffer& in,
                          AudioBuffer& out)
{
    assert(in.numSamples() == out.numSamples());
    assert(out.numChannels() == 1);

    memcpy(out[0], in[0], in.numSamples() * sizeof(float));
    for (auto i = 1; i < in.numChannels(); ++i)
    {
        ArrayMath::add(in.numSamples(), in[i], out[0], out[0]);
    }

    ArrayMath::scale(in.numSamples(), out[0], 1.0f / in.numChannels(), out[0]);
}

// Scaling factors for SN3D to MaxN: https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats#cite_note-Malham-2
// Scaling factors for N3D to SN3D: https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats#Furse-Malham
// Ordering conversion between ACN and FuMa: https://en.wikipedia.org/wiki/Ambisonic_data_exchange_formats
void AudioBuffer::convertAmbisonics(AmbisonicsType inType,
                                    AmbisonicsType outType,
                                    const AudioBuffer& in,
                                    AudioBuffer& out)
{
    assert(in.numChannels() == out.numChannels());
    assert(in.numSamples() == out.numSamples());

    auto numChannels = in.numChannels();
    auto numSamples = in.numSamples();

    const int fumaForACNIndex[] = { 0, 2, 3, 1, 8, 6, 4, 5, 7, 15, 13, 11, 9, 10, 12, 14 };
    const int acnForFuMaIndex[] = { 0, 3, 1, 2, 6, 7, 5, 8, 4, 12, 13, 11, 14, 10, 15, 9 };

    const float fumaForSN3DFactor[] = {
        1.0f / sqrtf(2.0f),
        1.0f,
        1.0f,
        1.0f,
        1.0f,
        2.0f / sqrtf(3.0f),
        2.0f / sqrtf(3.0f),
        2.0f / sqrtf(3.0f),
        2.0f / sqrtf(3.0f),
        1.0f,
        sqrtf(45.0f) / 32.0f,
        sqrtf(45.0f) / 32.0f,
        3.0f / sqrtf(5.0f),
        3.0f / sqrtf(5.0f),
        sqrtf(8.0f) / 5.0f,
        sqrtf(8.0f) / 5.0f
    };

    for (auto inIndex = 0; inIndex < numChannels; ++inIndex)
    {
        auto outIndex = inIndex;
        if (inType != AmbisonicsType::FuMa && outType == AmbisonicsType::FuMa)
        {
            outIndex = fumaForACNIndex[inIndex];
        }
        else if (inType == AmbisonicsType::FuMa && outType != AmbisonicsType::FuMa)
        {
            outIndex = acnForFuMaIndex[inIndex];
        }

        auto acnInIndex = (inType != AmbisonicsType::FuMa) ? inIndex : acnForFuMaIndex[inIndex];
        auto l = static_cast<int>(floorf(sqrtf(static_cast<float>(acnInIndex))));

        auto factor = 1.0f;
        if (inType == AmbisonicsType::N3D && outType == AmbisonicsType::SN3D)
        {
            factor = 1.0f / sqrtf(2.0f * l + 1);
        }
        else if (inType == AmbisonicsType::N3D && outType == AmbisonicsType::FuMa)
        {
            factor = (1.0f / sqrtf(2.0f * l + 1)) * fumaForSN3DFactor[outIndex];
        }
        else if (inType == AmbisonicsType::SN3D && outType == AmbisonicsType::N3D)
        {
            factor = sqrtf(2.0f * l + 1);
        }
        else if (inType == AmbisonicsType::SN3D && outType == AmbisonicsType::FuMa)
        {
            factor = fumaForSN3DFactor[outIndex];
        }
        else if (inType == AmbisonicsType::FuMa && outType == AmbisonicsType::SN3D)
        {
            factor = 1.0f / fumaForSN3DFactor[inIndex];
        }
        else if (inType == AmbisonicsType::FuMa && outType == AmbisonicsType::N3D)
        {
            factor = sqrtf(2.0f * l + 1) * (1.0f / fumaForSN3DFactor[inIndex]);
        }

        ArrayMath::scale(numSamples, in[inIndex], factor, out[outIndex]);
    }
}

}
