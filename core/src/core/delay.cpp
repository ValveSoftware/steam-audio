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

#include "delay.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Delay
// --------------------------------------------------------------------------------------------------------------------

Delay::Delay()
    : mCursor(0)
    , mReadCursor(0)
{}

Delay::Delay(int delay,
             int frameSize)
    : Delay()
{
    resize(delay, frameSize);
}

void Delay::resize(int delay,
                   int frameSize)
{
    mRingBuffer.resize(delay + frameSize);
    reset();
}

void Delay::reset()
{
    mRingBuffer.zero();
}

void Delay::get(float4_t& out)
{
    if (mReadCursor + 3 < static_cast<int>(mRingBuffer.size(0)))
    {
        auto result = float4::loadu(&mRingBuffer[mReadCursor]);

        mReadCursor += 4;
        if (mReadCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mReadCursor -= static_cast<int>(mRingBuffer.size(0));
        }

        out = result;
    }
    else
    {
        alignas(float4_t) float values[4];

        for (auto i = 0; i < 4; ++i)
        {
            values[i] = mRingBuffer[mReadCursor];
            ++mReadCursor;
            if (mReadCursor >= static_cast<int>(mRingBuffer.size(0)))
            {
                mReadCursor = 0;
            }
        }

        out = float4::load(values);
    }
}

void Delay::get(int numSamples,
                float* out)
{
    if (mReadCursor + (numSamples - 1) < static_cast<int>(mRingBuffer.size(0)))
    {
        memcpy(out, &mRingBuffer[mReadCursor], numSamples * sizeof(float));
        mReadCursor += numSamples;
        if (mReadCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mReadCursor -= static_cast<int>(mRingBuffer.size(0));
        }
    }
    else
    {
        auto size1 = static_cast<int>(mRingBuffer.size(0)) - mReadCursor;
        auto size2 = numSamples - size1;
        memcpy(out, &mRingBuffer[mReadCursor], size1 * sizeof(float));
        memcpy(&out[size1], &mRingBuffer[0], size2 * sizeof(float));
        mReadCursor = size2;
    }
}

void Delay::put(float4_t in)
{
    if (mCursor + 3 < static_cast<int>(mRingBuffer.size(0)))
    {
        float4::storeu(&mRingBuffer[mCursor], in);

        mCursor += 4;
        if (mCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mCursor -= static_cast<int>(mRingBuffer.size(0));
        }
    }
    else
    {
        alignas(float4_t) float values[4];
        float4::store(values, in);

        for (auto i = 0; i < 4; ++i)
        {
            mRingBuffer[mCursor] = values[i];
            ++mCursor;
            if (mCursor >= static_cast<int>(mRingBuffer.size(0)))
            {
                mCursor = 0;
            }
        }
    }
}

void Delay::put(int numSamples,
                const float* in)
{
    if (mCursor + (numSamples - 1) < static_cast<int>(mRingBuffer.size(0)))
    {
        memcpy(&mRingBuffer[mCursor], in, numSamples * sizeof(float));
        mCursor += numSamples;
        if (mCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mCursor -= static_cast<int>(mRingBuffer.size(0));
        }
    }
    else
    {
        auto size1 = static_cast<int>(mRingBuffer.size(0)) - mCursor;
        auto size2 = numSamples - size1;
        memcpy(&mRingBuffer[mCursor], in, size1 * sizeof(float));
        memcpy(&mRingBuffer[0], &in[size1], size2 * sizeof(float));
        mCursor = size2;
    }
}

}
