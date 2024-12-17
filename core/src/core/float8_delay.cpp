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

#if defined(IPL_ENABLE_FLOAT8)

#include "delay.h"
#include "float8.h"

namespace ipl {

// --------------------------------------------------------------------------------------------------------------------
// Delay
// --------------------------------------------------------------------------------------------------------------------

void IPL_FLOAT8_ATTR Delay::get(float8_t& out)
{
    if (mReadCursor + 7 < static_cast<int>(mRingBuffer.size(0)))
    {
        auto result = float8::loadu(&mRingBuffer[mReadCursor]);

        mReadCursor += 8;
        if (mReadCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mReadCursor -= static_cast<int>(mRingBuffer.size(0));
        }

        out = result;
    }
    else
    {
        alignas(float8_t) float values[8];

        for (auto i = 0; i < 8; ++i)
        {
            values[i] = mRingBuffer[mReadCursor];
            ++mReadCursor;
            if (mReadCursor >= static_cast<int>(mRingBuffer.size(0)))
            {
                mReadCursor = 0;
            }
        }

        out = float8::load(values);
    }
}

void IPL_FLOAT8_ATTR Delay::put(float8_t in)
{
    if (mCursor + 7 < static_cast<int>(mRingBuffer.size(0)))
    {
        float8::storeu(&mRingBuffer[mCursor], in);

        mCursor += 8;
        if (mCursor >= static_cast<int>(mRingBuffer.size(0)))
        {
            mCursor -= static_cast<int>(mRingBuffer.size(0));
        }
    }
    else
    {
        alignas(float8_t) float values[8];
        float8::store(values, in);

        for (auto i = 0; i < 8; ++i)
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


// --------------------------------------------------------------------------------------------------------------------
// Allpass
// --------------------------------------------------------------------------------------------------------------------

float8_t IPL_FLOAT8_ATTR Allpass::apply(float8_t x)
{
    auto vm = float8::zero();
    mDelay.get(vm);
    auto v = float8::sub(x, float8::mul(float8::set1(mAm), vm));
    mDelay.put(v);
    return float8::add(float8::mul(float8::set1(mB0), v), vm);
}

}

#endif
