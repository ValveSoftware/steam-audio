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
    mCursor = 0;
    mReadCursor = 0;
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


// --------------------------------------------------------------------------------------------------------------------
// Allpass
// --------------------------------------------------------------------------------------------------------------------

Allpass::Allpass()
    : mB0(0.0f)
    , mAm(0.0f)
{
    reset();
}

Allpass::Allpass(int delay,
                 float gain,
                 int frameSize)
{
    resize(delay, gain, frameSize);
    reset();
}

void Allpass::resize(int delay,
                     float gain,
                     int frameSize)
{
    mDelay.resize(delay, frameSize);
    mB0 = -gain;
    mAm = -gain;
}

void Allpass::reset()
{
    mDelay.reset();
    mB0 = 0.0f;
    mAm = 0.0f;
}

float Allpass::apply(float x)
{
    auto vm = 0.0f;
    mDelay.get(1, &vm);
    auto v = x - mAm * vm;
    mDelay.put(1, &v);
    return mB0 * v + vm;
}

float4_t Allpass::apply(float4_t x)
{
    auto vm = float4::zero();
    mDelay.get(vm);
    auto v = float4::sub(x, float4::mul(float4::set1(mAm), vm));
    mDelay.put(v);
    return float4::add(float4::mul(float4::set1(mB0), v), vm);
}

}
