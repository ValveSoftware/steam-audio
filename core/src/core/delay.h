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

#if defined(IPL_ENABLE_FLOAT8)
#include "float8.h"
#endif

#include "array.h"
#include "float4.h"

namespace ipl {

#if defined(IPL_ENABLE_FLOAT8)
typedef __m256 float8_t;
#endif

// --------------------------------------------------------------------------------------------------------------------
// Delay
// --------------------------------------------------------------------------------------------------------------------

class Delay
{
public:
    Delay();

    Delay(int delay,
          int frameSize);

    void resize(int delay,
                int frameSize);

    void reset();

    void get(float4_t& out);

#if defined(IPL_ENABLE_FLOAT8)
    void IPL_FLOAT8_ATTR get(float8_t& out);
#endif

    void get(int numSamples,
             float* out);

    void put(float4_t in);

#if defined(IPL_ENABLE_FLOAT8)
    void IPL_FLOAT8_ATTR put(float8_t in);
#endif

    void put(int numSamples,
             const float* in);

private:
    Array<float> mRingBuffer;
    int mCursor;
    int mReadCursor;
};


// --------------------------------------------------------------------------------------------------------------------
// Allpass
// --------------------------------------------------------------------------------------------------------------------

class Allpass
{
public:
    Allpass();

    Allpass(int delay,
            float gain,
            int frameSize);

    void resize(int delay,
                float gain,
                int frameSize);

    void reset();

    float apply(float x);

    float4_t apply(float4_t x);

#if defined(IPL_ENABLE_FLOAT8)
    float8_t IPL_FLOAT8_ATTR apply(float8_t x);
#endif

private:
    Delay mDelay;
    float mB0;
    float mAm;
};


}
